#pragma once
#ifndef STYIO_RUNTIME_READY_QUEUE_HPP_
#define STYIO_RUNTIME_READY_QUEUE_HPP_

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

namespace styio::runtime {

enum class ReadyQueueKind {
  BoundedWait = 1,
};

enum class ReadyQueuePushResult {
  Accepted,
  Closed,
};

struct ReadyQueueSnapshot {
  std::size_t capacity = 0;
  std::size_t current_depth = 0;
  std::size_t peak_depth = 0;
  std::size_t accepted_pushes = 0;
  std::size_t pops = 0;
  std::size_t pressure_events = 0;
  std::size_t producer_waits = 0;
  std::size_t consumer_waits = 0;
  std::size_t close_wake_ups = 0;
  bool closed = false;
};

/// The task scheduler's single ready-queue owner.
///
/// Storage, lifecycle, and counters are protected by one mutex. A closed queue
/// rejects new pushes but continues to return accepted items until drained.
class BoundedReadyQueue {
public:
  static constexpr std::size_t kDefaultCapacity = 4096;

  explicit BoundedReadyQueue(std::size_t capacity = kDefaultCapacity)
    : capacity_(valid_capacity(capacity) ? capacity : kDefaultCapacity) {
  }

  BoundedReadyQueue(const BoundedReadyQueue&) = delete;
  BoundedReadyQueue& operator=(const BoundedReadyQueue&) = delete;

  static constexpr bool valid_capacity(std::size_t capacity) {
    return capacity > 0 && capacity <= kDefaultCapacity;
  }

  ReadyQueueKind kind() const { return ReadyQueueKind::BoundedWait; }

  ReadyQueuePushResult push(void* task) {
    std::unique_lock<std::mutex> lock(mu_);
    if (closed_) {
      return ReadyQueuePushResult::Closed;
    }
    if (queue_.size() == capacity_) {
      ++pressure_events_;
      ++producer_waits_;
      ++waiting_producers_;
      not_full_.wait(lock, [this]() {
        return closed_ || queue_.size() < capacity_;
      });
      --waiting_producers_;
      if (closed_) {
        return ReadyQueuePushResult::Closed;
      }
    }
    queue_.push_back(task);
    ++accepted_pushes_;
    if (queue_.size() > peak_depth_) {
      peak_depth_ = queue_.size();
    }
    lock.unlock();
    not_empty_.notify_one();
    return ReadyQueuePushResult::Accepted;
  }

  /// Returns no item only once the queue is both closed and drained.
  void* wait_pop() {
    std::unique_lock<std::mutex> lock(mu_);
    if (queue_.empty() && !closed_) {
      ++consumer_waits_;
      ++waiting_consumers_;
      not_empty_.wait(lock, [this]() { return closed_ || !queue_.empty(); });
      --waiting_consumers_;
    }
    if (queue_.empty()) {
      return nullptr;
    }
    void* task = queue_.front();
    queue_.pop_front();
    ++pops_;
    lock.unlock();
    not_full_.notify_one();
    return task;
  }

  void close() {
    std::unique_lock<std::mutex> lock(mu_);
    if (closed_) {
      return;
    }
    closed_ = true;
    close_wake_ups_ += waiting_producers_ + waiting_consumers_;
    lock.unlock();
    not_empty_.notify_all();
    not_full_.notify_all();
  }

  ReadyQueueSnapshot snapshot() const {
    std::lock_guard<std::mutex> lock(mu_);
    return ReadyQueueSnapshot{
      capacity_,
      queue_.size(),
      peak_depth_,
      accepted_pushes_,
      pops_,
      pressure_events_,
      producer_waits_,
      consumer_waits_,
      close_wake_ups_,
      closed_,
    };
  }

  void reset_counters() {
    std::lock_guard<std::mutex> lock(mu_);
    peak_depth_ = queue_.size();
    accepted_pushes_ = 0;
    pops_ = 0;
    pressure_events_ = 0;
    producer_waits_ = 0;
    consumer_waits_ = 0;
    close_wake_ups_ = 0;
  }

private:
  const std::size_t capacity_;
  mutable std::mutex mu_;
  std::condition_variable not_empty_;
  std::condition_variable not_full_;
  std::deque<void*> queue_;
  bool closed_ = false;
  std::size_t peak_depth_ = 0;
  std::size_t accepted_pushes_ = 0;
  std::size_t pops_ = 0;
  std::size_t pressure_events_ = 0;
  std::size_t producer_waits_ = 0;
  std::size_t consumer_waits_ = 0;
  std::size_t close_wake_ups_ = 0;
  std::size_t waiting_producers_ = 0;
  std::size_t waiting_consumers_ = 0;
};

} // namespace styio::runtime

#endif // STYIO_RUNTIME_READY_QUEUE_HPP_
