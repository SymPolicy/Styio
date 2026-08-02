#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include "StyioRuntime/ReadyQueue.hpp"

namespace {

using styio::runtime::BoundedReadyQueue;
using styio::runtime::ReadyQueueKind;
using styio::runtime::ReadyQueuePushResult;

struct TaggedTask {
  int id;
};

bool wait_for_producer_waits(
    const BoundedReadyQueue& queue,
    std::size_t count) {
  const auto deadline = std::chrono::steady_clock::now()
    + std::chrono::seconds(2);
  while (queue.snapshot().producer_waits < count) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

} // namespace

TEST(StyioBoundedTaskScheduling, CapacityIsFixedAndInvalidValuesUseDefault) {
  BoundedReadyQueue queue(8);
  BoundedReadyQueue zero(0);
  BoundedReadyQueue excessive(BoundedReadyQueue::kDefaultCapacity + 1);

  EXPECT_EQ(queue.kind(), ReadyQueueKind::BoundedWait);
  EXPECT_EQ(queue.snapshot().capacity, 8u);
  EXPECT_EQ(zero.snapshot().capacity, BoundedReadyQueue::kDefaultCapacity);
  EXPECT_EQ(excessive.snapshot().capacity, BoundedReadyQueue::kDefaultCapacity);
}

TEST(StyioBoundedTaskScheduling, CapacityOnePopWakesBlockedProducer) {
  BoundedReadyQueue queue(1);
  TaggedTask first{1};
  TaggedTask second{2};
  ASSERT_EQ(queue.push(&first), ReadyQueuePushResult::Accepted);

  auto pushed = std::async(std::launch::async, [&]() {
    return queue.push(&second);
  });
  ASSERT_TRUE(wait_for_producer_waits(queue, 1));
  EXPECT_EQ(pushed.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);

  EXPECT_EQ(queue.wait_pop(), &first);
  ASSERT_EQ(pushed.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  EXPECT_EQ(pushed.get(), ReadyQueuePushResult::Accepted);
  queue.close();
  EXPECT_EQ(queue.wait_pop(), &second);
  EXPECT_EQ(queue.wait_pop(), nullptr);

  const auto snapshot = queue.snapshot();
  EXPECT_EQ(snapshot.capacity, 1u);
  EXPECT_EQ(snapshot.current_depth, 0u);
  EXPECT_EQ(snapshot.peak_depth, 1u);
  EXPECT_EQ(snapshot.accepted_pushes, 2u);
  EXPECT_EQ(snapshot.pops, 2u);
  EXPECT_EQ(snapshot.pressure_events, 1u);
  EXPECT_EQ(snapshot.producer_waits, 1u);
  EXPECT_TRUE(snapshot.closed);
}

TEST(StyioBoundedTaskScheduling, IdempotentCloseWakesAllAndAcceptedItemsDrain) {
  BoundedReadyQueue full(1);
  TaggedTask accepted{0};
  TaggedTask rejected_a{1};
  TaggedTask rejected_b{2};
  ASSERT_EQ(full.push(&accepted), ReadyQueuePushResult::Accepted);

  auto producer_a = std::async(std::launch::async, [&]() {
    return full.push(&rejected_a);
  });
  auto producer_b = std::async(std::launch::async, [&]() {
    return full.push(&rejected_b);
  });
  ASSERT_TRUE(wait_for_producer_waits(full, 2));
  full.close();
  full.close();
  ASSERT_EQ(producer_a.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  ASSERT_EQ(producer_b.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  EXPECT_EQ(producer_a.get(), ReadyQueuePushResult::Closed);
  EXPECT_EQ(producer_b.get(), ReadyQueuePushResult::Closed);
  EXPECT_EQ(full.wait_pop(), &accepted);
  EXPECT_EQ(full.wait_pop(), nullptr);
  EXPECT_EQ(full.push(&rejected_a), ReadyQueuePushResult::Closed);
  EXPECT_EQ(full.snapshot().close_wake_ups, 2u);

  BoundedReadyQueue empty(1);
  auto consumer_a = std::async(std::launch::async, [&]() {
    return empty.wait_pop();
  });
  auto consumer_b = std::async(std::launch::async, [&]() {
    return empty.wait_pop();
  });
  const auto deadline = std::chrono::steady_clock::now()
    + std::chrono::seconds(2);
  while (empty.snapshot().consumer_waits < 2
         && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  ASSERT_EQ(empty.snapshot().consumer_waits, 2u);
  empty.close();
  ASSERT_EQ(consumer_a.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  ASSERT_EQ(consumer_b.wait_for(std::chrono::seconds(2)), std::future_status::ready);
  EXPECT_EQ(consumer_a.get(), nullptr);
  EXPECT_EQ(consumer_b.get(), nullptr);
  EXPECT_EQ(empty.snapshot().close_wake_ups, 2u);
}

TEST(StyioBoundedTaskScheduling, MultiProducerMultiConsumerIsExactOnceAcrossClose) {
  BoundedReadyQueue queue(7);
  constexpr int kProducerCount = 4;
  constexpr int kConsumerCount = 4;
  constexpr int kPerProducer = 256;
  constexpr int kTotal = kProducerCount * kPerProducer;

  std::vector<TaggedTask> tasks(kTotal);
  std::vector<std::atomic<int>> seen(kTotal);
  std::atomic<int> ready{0};
  std::atomic<bool> start{false};
  std::atomic<int> invalid{0};
  std::vector<std::thread> producers;
  std::vector<std::thread> consumers;

  const auto await_start = [&]() {
    ready.fetch_add(1, std::memory_order_release);
    ready.notify_one();
    start.wait(false, std::memory_order_acquire);
  };

  for (int producer = 0; producer < kProducerCount; ++producer) {
    producers.emplace_back([&, producer]() {
      await_start();
      const int base = producer * kPerProducer;
      for (int offset = 0; offset < kPerProducer; ++offset) {
        tasks[base + offset].id = base + offset;
        if (queue.push(&tasks[base + offset]) != ReadyQueuePushResult::Accepted) {
          invalid.fetch_add(1);
        }
      }
    });
  }
  for (int consumer = 0; consumer < kConsumerCount; ++consumer) {
    consumers.emplace_back([&]() {
      await_start();
      while (auto* raw = static_cast<TaggedTask*>(queue.wait_pop())) {
        if (raw->id < 0 || raw->id >= kTotal) {
          invalid.fetch_add(1);
        }
        else if (seen[raw->id].fetch_add(1) != 0) {
          invalid.fetch_add(1);
        }
      }
    });
  }

  const int thread_count = kProducerCount + kConsumerCount;
  while (ready.load(std::memory_order_acquire) != thread_count) {
    const int observed = ready.load(std::memory_order_relaxed);
    ready.wait(observed, std::memory_order_relaxed);
  }
  start.store(true, std::memory_order_release);
  start.notify_all();
  for (auto& producer : producers) {
    producer.join();
  }
  queue.close();
  for (auto& consumer : consumers) {
    consumer.join();
  }

  EXPECT_EQ(invalid.load(), 0);
  for (int id = 0; id < kTotal; ++id) {
    EXPECT_EQ(seen[id].load(), 1) << "task id " << id;
  }
  const auto snapshot = queue.snapshot();
  EXPECT_EQ(snapshot.accepted_pushes, static_cast<std::size_t>(kTotal));
  EXPECT_EQ(snapshot.pops, static_cast<std::size_t>(kTotal));
  EXPECT_EQ(snapshot.current_depth, 0u);
  EXPECT_LE(snapshot.peak_depth, snapshot.capacity);
  EXPECT_GT(snapshot.pressure_events, 0u);
  EXPECT_EQ(snapshot.pressure_events, snapshot.producer_waits);
}

TEST(StyioBoundedTaskScheduling, CloseRejectsBlockedProducersAndDrainsAcceptedItems) {
  constexpr int kProducerCount = 4;
  constexpr int kConsumerCount = 3;
  constexpr int kPerProducer = 32;
  constexpr int kTotal = kProducerCount * kPerProducer;
  constexpr std::size_t kCapacity = 5;

  BoundedReadyQueue queue(kCapacity);
  std::vector<TaggedTask> tasks(kTotal);
  std::vector<std::atomic<int>> push_state(kTotal);
  std::vector<std::atomic<int>> seen(kTotal);
  for (int id = 0; id < kTotal; ++id) {
    tasks[id].id = id;
    push_state[id].store(0, std::memory_order_relaxed);
    seen[id].store(0, std::memory_order_relaxed);
  }

  std::atomic<bool> allow_consumers{false};
  std::vector<std::thread> consumers;
  for (int consumer = 0; consumer < kConsumerCount; ++consumer) {
    consumers.emplace_back([&]() {
      allow_consumers.wait(false, std::memory_order_acquire);
      while (auto* task = static_cast<TaggedTask*>(queue.wait_pop())) {
        seen[task->id].fetch_add(1, std::memory_order_relaxed);
      }
    });
  }

  std::vector<std::thread> producers;
  for (int producer = 0; producer < kProducerCount; ++producer) {
    producers.emplace_back([&, producer]() {
      const int base = producer * kPerProducer;
      for (int offset = 0; offset < kPerProducer; ++offset) {
        const int id = base + offset;
        const auto result = queue.push(&tasks[id]);
        push_state[id].store(
          result == ReadyQueuePushResult::Accepted ? 1 : 2,
          std::memory_order_relaxed);
        if (result == ReadyQueuePushResult::Closed) {
          return;
        }
      }
    });
  }

  const bool all_producers_blocked =
    wait_for_producer_waits(queue, kProducerCount);
  queue.close();
  allow_consumers.store(true, std::memory_order_release);
  allow_consumers.notify_all();

  for (auto& producer : producers) {
    producer.join();
  }
  for (auto& consumer : consumers) {
    consumer.join();
  }

  std::size_t accepted = 0;
  std::size_t rejected = 0;
  for (int id = 0; id < kTotal; ++id) {
    const int state = push_state[id].load(std::memory_order_relaxed);
    const int observations = seen[id].load(std::memory_order_relaxed);
    if (state == 1) {
      ++accepted;
      EXPECT_EQ(observations, 1) << "accepted task id " << id;
    }
    else {
      EXPECT_EQ(observations, 0) << "unaccepted task id " << id;
      if (state == 2) {
        ++rejected;
      }
    }
  }

  const auto snapshot = queue.snapshot();
  EXPECT_TRUE(all_producers_blocked);
  EXPECT_EQ(accepted, kCapacity);
  EXPECT_EQ(rejected, static_cast<std::size_t>(kProducerCount));
  EXPECT_EQ(snapshot.accepted_pushes, accepted);
  EXPECT_EQ(snapshot.pops, accepted);
  EXPECT_EQ(snapshot.current_depth, 0u);
  EXPECT_EQ(snapshot.peak_depth, kCapacity);
  EXPECT_EQ(snapshot.pressure_events, static_cast<std::size_t>(kProducerCount));
  EXPECT_EQ(snapshot.producer_waits, snapshot.pressure_events);
  EXPECT_EQ(snapshot.close_wake_ups, static_cast<std::size_t>(kProducerCount));
  EXPECT_TRUE(snapshot.closed);
}
