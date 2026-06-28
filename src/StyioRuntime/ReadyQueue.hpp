#pragma once
#ifndef STYIO_RUNTIME_READY_QUEUE_HPP_
#define STYIO_RUNTIME_READY_QUEUE_HPP_

#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace styio::runtime {

enum class ReadyQueueKind {
  MutexDeque,
  BoundedMPMC,
};

/// Abstract ready-queue for the task scheduler.
/// Enables swapping implementations behind a common interface.
class IReadyQueue {
public:
  virtual ~IReadyQueue() = default;

  /// Identify the concrete backend for assertions and diagnostics.
  virtual ReadyQueueKind kind() const = 0;

  /// Push a task onto the queue (thread-safe).
  virtual void push(void* task) = 0;

  /// Try to pop a task. Returns nullptr if queue is empty.
  virtual void* try_pop() = 0;

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
// MutexDequeReadyQueue - current default implementation.
// Wraps std::mutex + std::deque + std::condition_variable.
// ---------------------------------------------------------------------------
class MutexDequeReadyQueue : public IReadyQueue {
public:
  ReadyQueueKind kind() const override { return ReadyQueueKind::MutexDeque; }

  void push(void* task) override {
    std::lock_guard<std::mutex> lock(mu_);
    queue_.push_back(task);
  }

  void* try_pop() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) return nullptr;
    void* t = queue_.front();
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
  std::deque<void*> queue_;
};

// ---------------------------------------------------------------------------
// BoundedMPMCReadyQueue - bounded multi-producer/multi-consumer queue.
// Controlled by STYIO_USE_MPMC_QUEUE environment variable.
// ---------------------------------------------------------------------------
class BoundedMPMCReadyQueue : public IReadyQueue {
  static constexpr std::size_t kDefaultCapacity = 4096;

public:
  explicit BoundedMPMCReadyQueue(std::size_t capacity = kDefaultCapacity)
    : capacity_(capacity == 0 ? kDefaultCapacity : capacity) {
  }

  ReadyQueueKind kind() const override { return ReadyQueueKind::BoundedMPMC; }

  void push(void* task) override {
    for (;;) {
      {
        std::lock_guard<std::mutex> lock(mu_);
        if (queue_.size() < capacity_) {
          queue_.push_back(task);
          cv_.notify_one();
          return;
        }
      }
      std::this_thread::yield();
    }
  }

  void* try_pop() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (queue_.empty()) {
      return nullptr;
    }
    void* task = queue_.front();
    queue_.pop_front();
    return task;
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

  std::condition_variable& cv() { return cv_; }

private:
  std::size_t capacity_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<void*> queue_;
};

/// Factory: create the appropriate queue based on environment.
inline bool ready_queue_env_requests_mpmc() {
#if defined(_WIN32)
  char* raw_value = nullptr;
  size_t raw_length = 0;
  if (_dupenv_s(&raw_value, &raw_length, "STYIO_USE_MPMC_QUEUE") != 0 || raw_value == nullptr) {
    return false;
  }
  std::unique_ptr<char, decltype(&std::free)> value(raw_value, &std::free);
  return value.get()[0] == '1' || value.get()[0] == 'y' || value.get()[0] == 'Y';
#else
  if (const char* env = std::getenv("STYIO_USE_MPMC_QUEUE")) {
    return env[0] == '1' || env[0] == 'y' || env[0] == 'Y';
  }
  return false;
#endif
}

inline std::unique_ptr<IReadyQueue> make_ready_queue() {
  if (ready_queue_env_requests_mpmc()) {
    return std::make_unique<BoundedMPMCReadyQueue>();
  }
  return std::make_unique<MutexDequeReadyQueue>();
}

} // namespace styio::runtime

#endif // STYIO_RUNTIME_READY_QUEUE_HPP_
