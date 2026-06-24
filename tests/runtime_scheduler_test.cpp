#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>

#include "StyioRuntime/ReadyQueue.hpp"

// Dummy task type for testing (avoids linking full StyioTask).
struct DummyTask {
  int id;
  static std::atomic<int> next_id;
  DummyTask() : id(next_id.fetch_add(1)) {}
};

std::atomic<int> DummyTask::next_id{0};

using styio::runtime::IReadyQueue;
using styio::runtime::MutexDequeReadyQueue;
using styio::runtime::BoundedMPMCReadyQueue;
using styio::runtime::make_ready_queue;

// Note: the queue stores StyioTask*, but for testing we cast DummyTask*.
// This is safe as long as we only test queue operations, not task execution.

template <typename QueueFactory>
void basic_push_pop(QueueFactory& factory) {
  auto q = factory();
  DummyTask t1, t2, t3;
  q->push(reinterpret_cast<StyioTask*>(&t1));
  q->push(reinterpret_cast<StyioTask*>(&t2));
  q->push(reinterpret_cast<StyioTask*>(&t3));

  auto* r1 = q->try_pop();
  auto* r2 = q->try_pop();
  auto* r3 = q->try_pop();
  auto* r4 = q->try_pop();

  EXPECT_NE(r1, nullptr);
  EXPECT_NE(r2, nullptr);
  EXPECT_NE(r3, nullptr);
  EXPECT_EQ(r4, nullptr);
  EXPECT_TRUE(q->empty());
}

TEST(MutexDequeReadyQueue, BasicPushPop) {
  auto factory = []() { return std::make_unique<MutexDequeReadyQueue>(); };
  basic_push_pop(factory);
}

TEST(MutexDequeReadyQueue, ApproximateSize) {
  MutexDequeReadyQueue q;
  EXPECT_EQ(q.approximate_size(), 0u);
  DummyTask t;
  q.push(reinterpret_cast<StyioTask*>(&t));
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
  DummyTask tasks[8];
  for (int i = 0; i < 8; ++i) {
    q.push(reinterpret_cast<StyioTask*>(&tasks[i]));
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
      q.push(reinterpret_cast<StyioTask*>(t));
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

TEST(MakeReadyQueue, DefaultIsMutexDeque) {
  // Without env var, returns MutexDequeReadyQueue.
  auto q = make_ready_queue();
  EXPECT_NE(q, nullptr);
  EXPECT_TRUE(q->empty());
}
