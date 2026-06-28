#include <gtest/gtest.h>
#include <atomic>
#include <cstdlib>
#include <optional>
#include <thread>
#include <string>
#include <vector>

#include "EnvTestUtil.hpp"
#include "StyioRuntime/ReadyQueue.hpp"

// Dummy task type for testing (avoids linking full StyioTask).
struct DummyTask {
  int id;
  static std::atomic<int> next_id;
  DummyTask() : id(next_id.fetch_add(1)) {}
};

std::atomic<int> DummyTask::next_id{0};

struct TaggedTask {
  explicit TaggedTask(int task_id) : id(task_id) {}
  int id;
};

using styio::runtime::IReadyQueue;
using styio::runtime::MutexDequeReadyQueue;
using styio::runtime::BoundedMPMCReadyQueue;
using styio::runtime::make_ready_queue;

class EnvVarGuard {
public:
  explicit EnvVarGuard(std::string name)
    : name_(std::move(name)) {
    if (const char* value = std::getenv(name_.c_str())) {
      original_ = std::string(value);
    }
  }

  ~EnvVarGuard() {
    if (original_.has_value()) {
      styio_test_setenv(name_.c_str(), original_->c_str(), 1);
    }
    else {
      styio_test_unsetenv(name_.c_str());
    }
  }

  void set(const char* value) {
    styio_test_setenv(name_.c_str(), value != nullptr ? value : "", 1);
  }

private:
  std::string name_;
  std::optional<std::string> original_;
};

// Note: the queue stores opaque task pointers, but for testing we cast DummyTask*.
// This is safe as long as we only test queue operations, not task execution.

template <typename QueueFactory>
void basic_push_pop(QueueFactory& factory) {
  auto q = factory();
  DummyTask t1, t2, t3;
  ASSERT_NE(q, nullptr);
  q->push(static_cast<void*>(&t1));
  q->push(static_cast<void*>(&t2));
  q->push(static_cast<void*>(&t3));

  auto* r1 = q->try_pop();
  auto* r2 = q->try_pop();
  auto* r3 = q->try_pop();
  auto* r4 = q->try_pop();

  EXPECT_EQ(r1, static_cast<void*>(&t1));
  EXPECT_EQ(r2, static_cast<void*>(&t2));
  EXPECT_EQ(r3, static_cast<void*>(&t3));
  EXPECT_EQ(r4, nullptr);
  EXPECT_TRUE(q->empty());
  EXPECT_EQ(q->approximate_size(), 0u);
}

TEST(MutexDequeReadyQueue, BasicPushPop) {
  auto factory = []() { return std::make_unique<MutexDequeReadyQueue>(); };
  basic_push_pop(factory);
}

TEST(MutexDequeReadyQueue, ApproximateSize) {
  MutexDequeReadyQueue q;
  EXPECT_EQ(q.kind(), styio::runtime::ReadyQueueKind::MutexDeque);
  EXPECT_EQ(q.approximate_size(), 0u);
  DummyTask t;
  q.push(static_cast<void*>(&t));
  EXPECT_EQ(q.approximate_size(), 1u);
  q.try_pop();
  EXPECT_EQ(q.approximate_size(), 0u);
}

TEST(BoundedMPMCReadyQueue, BasicPushPop) {
  auto factory = []() { return std::make_unique<BoundedMPMCReadyQueue>(64); };
  basic_push_pop(factory);
}

TEST(BoundedMPMCReadyQueue, Capacity) {
  BoundedMPMCReadyQueue q(8);
  EXPECT_EQ(q.kind(), styio::runtime::ReadyQueueKind::BoundedMPMC);
  DummyTask tasks[8];
  for (int i = 0; i < 8; ++i) {
    q.push(static_cast<void*>(&tasks[i]));
  }
  EXPECT_FALSE(q.empty());
  for (int i = 0; i < 8; ++i) {
    EXPECT_NE(q.try_pop(), nullptr);
  }
  EXPECT_TRUE(q.empty());
}

TEST(BoundedMPMCReadyQueue, MultiThreadedPushPop) {
  BoundedMPMCReadyQueue q(64);
  constexpr int kPerThread = 1000;
  std::atomic<int> produced{0};
  std::atomic<int> consumed{0};

  // Producer
  std::thread producer([&]() {
    for (int i = 0; i < kPerThread; ++i) {
      DummyTask* t = new DummyTask();
      q.push(static_cast<void*>(t));
      produced.fetch_add(1);
    }
  });

  // Consumer
  std::thread consumer([&]() {
    int got = 0;
    while (got < kPerThread) {
      auto* t = q.try_pop();
      if (t) {
        delete reinterpret_cast<DummyTask*>(t);
        got++;
        consumed.fetch_add(1);
      }
    }
  });

  producer.join();
  consumer.join();

  EXPECT_EQ(produced.load(), kPerThread);
  EXPECT_EQ(consumed.load(), kPerThread);
}

TEST(BoundedMPMCReadyQueue, MultiProducerMultiConsumerNoLossNoDupes) {
  BoundedMPMCReadyQueue q(32);
  constexpr int kProducerCount = 4;
  constexpr int kConsumerCount = 4;
  constexpr int kPerProducer = 256;
  constexpr int kTotal = kProducerCount * kPerProducer;

  std::vector<std::atomic<int>> seen(kTotal);
  std::atomic<int> ready_threads{0};
  std::atomic<bool> start{false};
  std::atomic<int> consumed{0};
  std::atomic<int> duplicate_hits{0};
  std::atomic<int> bad_id_hits{0};
  std::vector<std::thread> workers;
  workers.reserve(kProducerCount + kConsumerCount);

  auto wait_for_start = [&]() {
    ready_threads.fetch_add(1);
    while (!start.load()) {
      std::this_thread::yield();
    }
  };

  for (int producer_id = 0; producer_id < kProducerCount; ++producer_id) {
    workers.emplace_back([&, producer_id]() {
      wait_for_start();
      const int base = producer_id * kPerProducer;
      for (int i = 0; i < kPerProducer; ++i) {
        q.push(static_cast<void*>(new TaggedTask(base + i)));
        if ((i & 15) == 0) {
          std::this_thread::yield();
        }
      }
    });
  }

  for (int consumer_index = 0; consumer_index < kConsumerCount; ++consumer_index) {
    (void)consumer_index;
    workers.emplace_back([&]() {
      wait_for_start();
      while (consumed.load() < kTotal) {
        auto* raw = q.try_pop();
        if (raw == nullptr) {
          std::this_thread::yield();
          continue;
        }

        auto* task = static_cast<TaggedTask*>(raw);
        if ((*task).id < 0 || (*task).id >= kTotal) {
          bad_id_hits.fetch_add(1);
        }
        else {
          int previous = seen[(*task).id].fetch_add(1);
          if (previous != 0) {
            duplicate_hits.fetch_add(1);
          }
        }

        delete task;
        consumed.fetch_add(1);
      }
    });
  }

  while (ready_threads.load() < kProducerCount + kConsumerCount) {
    std::this_thread::yield();
  }
  start.store(true);

  for (auto& worker : workers) {
    worker.join();
  }

  EXPECT_EQ(consumed.load(), kTotal);
  EXPECT_EQ(bad_id_hits.load(), 0);
  EXPECT_EQ(duplicate_hits.load(), 0);
  EXPECT_TRUE(q.empty());
  EXPECT_EQ(q.approximate_size(), 0u);

  for (int i = 0; i < kTotal; ++i) {
    EXPECT_EQ(seen[i].load(), 1) << "task id " << i << " was not observed exactly once";
  }
}

TEST(MakeReadyQueue, DefaultIsMutexDeque) {
  // Without env var, returns MutexDequeReadyQueue.
  auto q = make_ready_queue();
  EXPECT_NE(q, nullptr);
  EXPECT_EQ(q->kind(), styio::runtime::ReadyQueueKind::MutexDeque);
  EXPECT_NE(dynamic_cast<MutexDequeReadyQueue*>(q.get()), nullptr);
  EXPECT_TRUE(q->empty());
  EXPECT_EQ(q->approximate_size(), 0u);

  DummyTask t1;
  DummyTask t2;
  q->push(static_cast<void*>(&t1));
  q->push(static_cast<void*>(&t2));
  EXPECT_EQ(q->approximate_size(), 2u);
  EXPECT_EQ(q->try_pop(), static_cast<void*>(&t1));
  EXPECT_EQ(q->try_pop(), static_cast<void*>(&t2));
  EXPECT_TRUE(q->empty());
}

TEST(MakeReadyQueue, EnvVarSelectsBoundedMpmcQueue) {
  EnvVarGuard guard("STYIO_USE_MPMC_QUEUE");
  guard.set("1");

  auto q = make_ready_queue();
  EXPECT_NE(q, nullptr);
  EXPECT_EQ(q->kind(), styio::runtime::ReadyQueueKind::BoundedMPMC);
  EXPECT_NE(dynamic_cast<BoundedMPMCReadyQueue*>(q.get()), nullptr);

  DummyTask t;
  EXPECT_EQ(q->approximate_size(), 0u);
  q->push(static_cast<void*>(&t));
  EXPECT_EQ(q->approximate_size(), 1u);
  EXPECT_EQ(q->try_pop(), static_cast<void*>(&t));
  EXPECT_TRUE(q->empty());
}
