#pragma once
#ifndef STYIO_RUNTIME_READY_QUEUE_HPP_
#define STYIO_RUNTIME_READY_QUEUE_HPP_

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>

class StyioTask;

namespace styio::runtime {

/// Abstract ready-queue for the task scheduler.
/// Enables swapping implementations behind a common interface.
class IReadyQueue {
public:
  virtual ~IReadyQueue() = default;

  /// Push a task onto the queue (thread-safe).
  virtual void push(StyioTask* task) = 0;

  /// Try to pop a task. Returns nullptr if queue is empty.
  virtual StyioTask* try_pop() = 0;

  /// Non-blocking snapshot of queue size (approximate for MPMC queues).
  virtual std::size_t approximate_size() const = 0;

  /// True if queue is currently empty (may be stale for MPMC queues).
  virtual bool empty() const = 0;

  /// Notify waiting consumers that a task is available.
  virtual void notify_one() = 0;

  /// Notify all waiting consumers (used at shutdown).
  virtual void notify_all() = 0;

  /// Wait with timeout. Returns true if notified, false on timeout.
  virtual bool wait_for(std::unique_lock<std::mutex>& lock,
                        std::condition_variable& cv) = 0;
};

// ---------------------------------------------------------------------------
// MutexDequeReadyQueue — current production implementation.
// Wraps std::mutex + std::deque + std::condition_variable.
// ---------------------------------------------------------------------------
class MutexDequeReadyQueue : public IReadyQueue {
public:
  void push(StyioTask* task) override {
    std::lock_guard<std::mutex> lock(mu_);
    queue_.push_back(task);
  }

  StyioTask* try_pop() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) return nullptr;
    StyioTask* t = queue_.front();
    queue_.pop_front();
    return t;
  }

  std::size_t approximate_size() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.size();
  }

  bool empty() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return queue_.empty();
  }

  void notify_one() override { cv_.notify_one(); }
  void notify_all() override { cv_.notify_all(); }

  bool wait_for(std::unique_lock<std::mutex>& lock,
                std::condition_variable& /*cv*/) override {
    cv_.wait(lock);
    return true;
  }

  /// Direct access to the internal mutex (for condition_variable usage).
  std::mutex& mutex() { return mu_; }

private:
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<StyioTask*> queue_;
};

// ---------------------------------------------------------------------------
// BoundedMPMCReadyQueue — lock-free bounded ring buffer.
// Controlled by STYIO_USE_MPMC_QUEUE environment variable.
// ---------------------------------------------------------------------------
class BoundedMPMCReadyQueue : public IReadyQueue {
  static constexpr std::size_t kDefaultCapacity = 4096;

public:
  explicit BoundedMPMCReadyQueue(std::size_t capacity = kDefaultCapacity)
    : capacity_(capacity), mask_(capacity - 1) {
    // capacity must be power of two
    ring_.reset(new std::atomic<StyioTask*>[capacity]);
    for (std::size_t i = 0; i < capacity; ++i) {
      ring_[i].store(nullptr, std::memory_order_relaxed);
    }
  }

  void push(StyioTask* task) override {
    std::size_t head = head_.load(std::memory_order_relaxed);
    while (true) {
      std::size_t tail = tail_.load(std::memory_order_acquire);
      if (head - tail >= capacity_) {
        // Queue full — busy-wait (bounded by caller)
        continue;
      }
      std::size_t idx = head & mask_;
      StyioTask* expected = nullptr;
      if (ring_[idx].compare_exchange_weak(expected, task,
            std::memory_order_release, std::memory_order_relaxed)) {
        head_.store(head + 1, std::memory_order_release);
        return;
      }
      head = head_.load(std::memory_order_relaxed);
    }
  }

  StyioTask* try_pop() override {
    std::size_t tail = tail_.load(std::memory_order_relaxed);
    while (true) {
      std::size_t head = head_.load(std::memory_order_acquire);
      if (tail >= head) return nullptr; // empty
      std::size_t idx = tail & mask_;
      StyioTask* task = ring_[idx].load(std::memory_order_acquire);
      if (task == nullptr) return nullptr;
      if (ring_[idx].compare_exchange_weak(task, nullptr,
            std::memory_order_release, std::memory_order_relaxed)) {
        tail_.store(tail + 1, std::memory_order_release);
        return task;
      }
      tail = tail_.load(std::memory_order_relaxed);
    }
  }

  std::size_t approximate_size() const override {
    std::size_t h = head_.load(std::memory_order_acquire);
    std::size_t t = tail_.load(std::memory_order_acquire);
    return h > t ? h - t : 0;
  }

  bool empty() const override {
    return head_.load(std::memory_order_acquire) <=
           tail_.load(std::memory_order_acquire);
  }

  void notify_one() override { cv_.notify_one(); }
  void notify_all() override { cv_.notify_all(); }

  bool wait_for(std::unique_lock<std::mutex>& lock,
                std::condition_variable& /*cv*/) override {
    cv_.wait(lock);
    return true;
  }

  std::condition_variable& cv() { return cv_; }

private:
  std::size_t capacity_;
  std::size_t mask_;
  std::unique_ptr<std::atomic<StyioTask*>[]> ring_;
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
  std::condition_variable cv_;
};

/// Factory: create the appropriate queue based on environment.
inline std::unique_ptr<IReadyQueue> make_ready_queue() {
  if (const char* env = std::getenv("STYIO_USE_MPMC_QUEUE")) {
    if (env[0] == '1' || env[0] == 'y' || env[0] == 'Y') {
      return std::make_unique<BoundedMPMCReadyQueue>();
    }
  }
  return std::make_unique<MutexDequeReadyQueue>();
}

} // namespace styio::runtime

#endif // STYIO_RUNTIME_READY_QUEUE_HPP_
