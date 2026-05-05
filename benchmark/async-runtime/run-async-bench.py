#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import shutil
import stat
import subprocess
import tarfile
import time
import urllib.request
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_TASKS = 4
DEFAULT_SLEEP_MS = 160
DEFAULT_NOOP_TASKS = 10000
DEFAULT_WORKERS = 4


CPP_SOURCE = r'''
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using Clock = std::chrono::steady_clock;

class CoroutineScheduler {
public:
  explicit CoroutineScheduler(int worker_count) {
    for (int i = 0; i < worker_count; ++i) {
      workers_.emplace_back([this]() { worker_loop(); });
    }
    timer_ = std::thread([this]() { timer_loop(); });
  }

  ~CoroutineScheduler() {
    stopping_.store(true);
    {
      std::lock_guard<std::mutex> lock(ready_mu_);
    }
    ready_cv_.notify_all();
    {
      std::lock_guard<std::mutex> lock(timer_mu_);
    }
    timer_cv_.notify_all();
    for (auto& worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    if (timer_.joinable()) {
      timer_.join();
    }
  }

  void schedule(std::coroutine_handle<> handle) {
    if (handle.done()) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(ready_mu_);
      ready_.push(handle);
    }
    ready_cv_.notify_one();
  }

  void schedule_after(std::coroutine_handle<> handle, std::chrono::milliseconds delay) {
    const auto due = Clock::now() + delay;
    {
      std::lock_guard<std::mutex> lock(timer_mu_);
      timers_.push(TimerItem{due, handle, next_timer_sequence_++});
    }
    timer_cv_.notify_one();
  }

private:
  struct TimerItem {
    Clock::time_point due;
    std::coroutine_handle<> handle;
    uint64_t sequence;
  };

  struct TimerCompare {
    bool operator()(const TimerItem& lhs, const TimerItem& rhs) const {
      if (lhs.due == rhs.due) {
        return lhs.sequence > rhs.sequence;
      }
      return lhs.due > rhs.due;
    }
  };

  std::mutex ready_mu_;
  std::condition_variable ready_cv_;
  std::queue<std::coroutine_handle<>> ready_;
  std::mutex timer_mu_;
  std::condition_variable timer_cv_;
  std::priority_queue<TimerItem, std::vector<TimerItem>, TimerCompare> timers_;
  std::vector<std::thread> workers_;
  std::thread timer_;
  std::atomic<bool> stopping_{false};
  uint64_t next_timer_sequence_ = 0;

  void worker_loop() {
    for (;;) {
      std::coroutine_handle<> handle;
      {
        std::unique_lock<std::mutex> lock(ready_mu_);
        ready_cv_.wait(lock, [this]() { return stopping_.load() || !ready_.empty(); });
        if (ready_.empty()) {
          if (stopping_.load()) {
            return;
          }
          continue;
        }
        handle = ready_.front();
        ready_.pop();
      }
      if (!handle.done()) {
        handle.resume();
      }
    }
  }

  void timer_loop() {
    std::unique_lock<std::mutex> lock(timer_mu_);
    for (;;) {
      if (stopping_.load()) {
        return;
      }
      if (timers_.empty()) {
        timer_cv_.wait(lock, [this]() { return stopping_.load() || !timers_.empty(); });
        continue;
      }
      const auto due = timers_.top().due;
      timer_cv_.wait_until(lock, due, [this, due]() {
        return stopping_.load() || timers_.empty() || timers_.top().due < due;
      });
      if (stopping_.load()) {
        return;
      }
      const auto now = Clock::now();
      while (!timers_.empty() && timers_.top().due <= now) {
        const auto item = timers_.top();
        timers_.pop();
        lock.unlock();
        schedule(item.handle);
        lock.lock();
        if (stopping_.load()) {
          return;
        }
      }
    }
  }
};

struct SleepAwaitable {
  CoroutineScheduler& scheduler;
  std::chrono::milliseconds delay;

  bool await_ready() const noexcept {
    return delay.count() <= 0;
  }

  void await_suspend(std::coroutine_handle<> handle) const {
    scheduler.schedule_after(handle, delay);
  }

  void await_resume() const noexcept {}
};

class Task {
public:
  struct State {
    std::mutex mu;
    std::condition_variable cv;
    bool done = false;
    int64_t value = 0;
    std::exception_ptr exception;
  };

  struct promise_type {
    std::shared_ptr<State> state = std::make_shared<State>();

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this), state};
    }

    std::suspend_always initial_suspend() noexcept {
      return {};
    }

    struct FinalAwaiter {
      bool await_ready() noexcept {
        return false;
      }

      void await_suspend(std::coroutine_handle<promise_type> handle) noexcept {
        auto state = handle.promise().state;
        {
          std::lock_guard<std::mutex> lock(state->mu);
          state->done = true;
        }
        state->cv.notify_one();
      }

      void await_resume() noexcept {}
    };

    FinalAwaiter final_suspend() noexcept {
      return {};
    }

    void return_value(int64_t value) noexcept {
      state->value = value;
    }

    void unhandled_exception() noexcept {
      state->exception = std::current_exception();
    }
  };

  Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})), state_(std::move(other.state_)) {}

  Task& operator=(Task&& other) noexcept {
    if (this != &other) {
      destroy();
      handle_ = std::exchange(other.handle_, {});
      state_ = std::move(other.state_);
    }
    return *this;
  }

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  ~Task() {
    destroy();
  }

  void start(CoroutineScheduler& scheduler) {
    if (!handle_) {
      throw std::logic_error("coroutine task already consumed");
    }
    scheduler.schedule(handle_);
  }

  int64_t get() {
    std::unique_lock<std::mutex> lock(state_->mu);
    state_->cv.wait(lock, [this]() { return state_->done; });
    const auto value = state_->value;
    const auto exception = state_->exception;
    lock.unlock();
    destroy();
    if (exception) {
      std::rethrow_exception(exception);
    }
    return value;
  }

private:
  Task(std::coroutine_handle<promise_type> handle, std::shared_ptr<State> state)
      : handle_(handle), state_(std::move(state)) {}

  void destroy() {
    if (handle_) {
      handle_.destroy();
      handle_ = {};
    }
  }

  std::coroutine_handle<promise_type> handle_;
  std::shared_ptr<State> state_;
};

Task sleep_task(CoroutineScheduler& scheduler, int sleep_ms) {
  co_await SleepAwaitable{scheduler, std::chrono::milliseconds(sleep_ms)};
  co_return 1;
}

Task noop_task() {
  co_return 1;
}

long long elapsed_ms(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

long long elapsed_us(Clock::time_point start, Clock::time_point end) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

int main(int argc, char** argv) {
  int tasks = std::atoi(argv[1]);
  int sleep_ms = std::atoi(argv[2]);
  int noop_tasks = std::atoi(argv[3]);
  int workers = std::atoi(argv[4]);

  auto seq_start = Clock::now();
  for (int i = 0; i < tasks; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  auto seq_ms = elapsed_ms(seq_start, Clock::now());

  CoroutineScheduler scheduler(workers);
  std::vector<Task> sleep_tasks;
  sleep_tasks.reserve(tasks);
  auto par_start = Clock::now();
  for (int i = 0; i < tasks; ++i) {
    sleep_tasks.push_back(sleep_task(scheduler, sleep_ms));
    sleep_tasks.back().start(scheduler);
  }
  int64_t sleep_sum = 0;
  for (auto& task : sleep_tasks) {
    sleep_sum += task.get();
  }
  auto par_ms = elapsed_ms(par_start, Clock::now());

  std::vector<Task> noop_tasks_vec;
  noop_tasks_vec.reserve(noop_tasks);
  auto noop_start = Clock::now();
  for (int i = 0; i < noop_tasks; ++i) {
    noop_tasks_vec.push_back(noop_task());
    noop_tasks_vec.back().start(scheduler);
  }
  int64_t noop_sum = 0;
  for (auto& task : noop_tasks_vec) {
    noop_sum += task.get();
  }
  auto noop_us = elapsed_us(noop_start, Clock::now());

  std::cout << "{"
            << "\"language\":\"cpp\","
            << "\"runtime\":\"cpp_stackless_coroutine\","
            << "\"status\":\"ok\","
            << "\"workers\":" << workers << ","
            << "\"sleep\":{\"tasks\":" << tasks
            << ",\"sleep_ms\":" << sleep_ms
            << ",\"sequential_ms\":" << seq_ms
            << ",\"parallel_ms\":" << par_ms
            << ",\"speedup\":" << (par_ms > 0 ? double(seq_ms) / double(par_ms) : 0.0)
            << ",\"sum\":" << sleep_sum << "},"
            << "\"noop\":{\"tasks\":" << noop_tasks
            << ",\"total_us\":" << noop_us
            << ",\"per_task_us\":" << (noop_tasks > 0 ? double(noop_us) / double(noop_tasks) : 0.0)
            << ",\"sum\":" << noop_sum << "}"
            << "}\n";
  return 0;
}
'''


GO_SOURCE = r'''
package main

import (
  "encoding/json"
  "os"
  "runtime"
  "strconv"
  "sync"
  "sync/atomic"
  "time"
)

func atoi(v string) int {
  n, err := strconv.Atoi(v)
  if err != nil {
    panic(err)
  }
  return n
}

func main() {
  tasks := atoi(os.Args[1])
  sleepMs := atoi(os.Args[2])
  noopTasks := atoi(os.Args[3])
  workers := atoi(os.Args[4])
  runtime.GOMAXPROCS(workers)

  seqStart := time.Now()
  for i := 0; i < tasks; i++ {
    time.Sleep(time.Duration(sleepMs) * time.Millisecond)
  }
  seqMs := time.Since(seqStart).Milliseconds()

  parStart := time.Now()
  var wg sync.WaitGroup
  sleepSum := int64(0)
  wg.Add(tasks)
  for i := 0; i < tasks; i++ {
    go func() {
      defer wg.Done()
      time.Sleep(time.Duration(sleepMs) * time.Millisecond)
      atomic.AddInt64(&sleepSum, 1)
    }()
  }
  wg.Wait()
  parMs := time.Since(parStart).Milliseconds()

  noopStart := time.Now()
  noopSum := int64(0)
  wg.Add(noopTasks)
  for i := 0; i < noopTasks; i++ {
    go func() {
      defer wg.Done()
      atomic.AddInt64(&noopSum, 1)
    }()
  }
  wg.Wait()
  noopUs := time.Since(noopStart).Microseconds()

  speedup := 0.0
  if parMs > 0 {
    speedup = float64(seqMs) / float64(parMs)
  }
  perTask := 0.0
  if noopTasks > 0 {
    perTask = float64(noopUs) / float64(noopTasks)
  }

  result := map[string]any{
    "language": "go",
    "runtime": "goroutine",
    "status": "ok",
    "workers": workers,
    "sleep": map[string]any{
      "tasks": tasks,
      "sleep_ms": sleepMs,
      "sequential_ms": seqMs,
      "parallel_ms": parMs,
      "speedup": speedup,
      "sum": sleepSum,
    },
    "noop": map[string]any{
      "tasks": noopTasks,
      "total_us": noopUs,
      "per_task_us": perTask,
      "sum": noopSum,
    },
  }
  enc := json.NewEncoder(os.Stdout)
  if err := enc.Encode(result); err != nil {
    panic(err)
  }
}
'''


RUST_SOURCE = r'''
use std::env;
use std::time::{Duration, Instant};
use tokio::runtime::Builder;

async fn run_benchmark(tasks: usize, sleep_ms: u64, noop_tasks: usize, workers: usize) {
    let seq_start = Instant::now();
    for _ in 0..tasks {
        tokio::time::sleep(Duration::from_millis(sleep_ms)).await;
    }
    let seq_ms = seq_start.elapsed().as_millis();

    let mut sleep_handles = Vec::with_capacity(tasks);
    let par_start = Instant::now();
    for _ in 0..tasks {
        sleep_handles.push(tokio::spawn(async move {
            tokio::time::sleep(Duration::from_millis(sleep_ms)).await;
            1_i64
        }));
    }
    let mut sleep_sum = 0_i64;
    for handle in sleep_handles {
        sleep_sum += handle.await.unwrap();
    }
    let par_ms = par_start.elapsed().as_millis();

    let mut noop_handles = Vec::with_capacity(noop_tasks);
    let noop_start = Instant::now();
    for _ in 0..noop_tasks {
        noop_handles.push(tokio::spawn(async { 1_i64 }));
    }
    let mut noop_sum = 0_i64;
    for handle in noop_handles {
        noop_sum += handle.await.unwrap();
    }
    let noop_us = noop_start.elapsed().as_micros();

    let speedup = if par_ms > 0 { seq_ms as f64 / par_ms as f64 } else { 0.0 };
    let per_task = if noop_tasks > 0 { noop_us as f64 / noop_tasks as f64 } else { 0.0 };
    println!(
        "{{\"language\":\"rust\",\"runtime\":\"tokio_multi_thread\",\"status\":\"ok\",\"workers\":{},\"sleep\":{{\"tasks\":{},\"sleep_ms\":{},\"sequential_ms\":{},\"parallel_ms\":{},\"speedup\":{},\"sum\":{}}},\"noop\":{{\"tasks\":{},\"total_us\":{},\"per_task_us\":{},\"sum\":{}}}}}",
        workers, tasks, sleep_ms, seq_ms, par_ms, speedup, sleep_sum, noop_tasks, noop_us, per_task, noop_sum
    );
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let tasks: usize = args[1].parse().unwrap();
    let sleep_ms: u64 = args[2].parse().unwrap();
    let noop_tasks: usize = args[3].parse().unwrap();
    let workers: usize = args[4].parse().unwrap();
    let runtime = Builder::new_multi_thread()
        .worker_threads(workers)
        .enable_time()
        .build()
        .unwrap();
    runtime.block_on(run_benchmark(tasks, sleep_ms, noop_tasks, workers));
}
'''


def run(cmd: list[str], *, cwd: Path, env: dict[str, str] | None = None, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=cwd,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )


def command_version(cmd: str) -> str:
    try:
        proc = run([cmd, "--version"], cwd=ROOT, timeout=20)
    except Exception:
        return ""
    return (proc.stdout or proc.stderr).splitlines()[0] if (proc.stdout or proc.stderr) else ""


def json_from_stdout(proc: subprocess.CompletedProcess[str]) -> dict[str, Any]:
    if proc.returncode != 0:
        raise RuntimeError(proc.stderr.strip() or proc.stdout.strip() or f"exit {proc.returncode}")
    for line in reversed(proc.stdout.splitlines()):
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            return json.loads(line)
    raise RuntimeError("benchmark did not emit a JSON object")


def host_tag() -> str:
    return f"{platform.system().lower()}-{platform.machine().lower()}"


def local_go(toolchain_dir: Path) -> Path | None:
    go = toolchain_dir / "go" / "bin" / "go"
    return go if go.exists() else None


def local_rustc(toolchain_dir: Path) -> Path | None:
    rustup_home = toolchain_dir / "rustup" / "toolchains"
    if rustup_home.exists():
        candidates = sorted(rustup_home.glob("stable-*/bin/rustc"))
        if candidates:
            return candidates[0]
    rustc = toolchain_dir / "cargo" / "bin" / "rustc"
    if rustc.exists():
        return rustc
    return None


def local_cargo(toolchain_dir: Path) -> Path | None:
    cargo = toolchain_dir / "cargo" / "bin" / "cargo"
    return cargo if cargo.exists() else None


def rust_env(toolchain_dir: Path) -> dict[str, str]:
    env = os.environ.copy()
    env["CARGO_HOME"] = str(toolchain_dir / "cargo")
    env["RUSTUP_HOME"] = str(toolchain_dir / "rustup")
    cargo_bin = toolchain_dir / "cargo" / "bin"
    env["PATH"] = f"{cargo_bin}{os.pathsep}{env.get('PATH', '')}"
    return env


def command_version_with_env(cmd: str, env: dict[str, str] | None = None) -> str:
    try:
        proc = run([cmd, "--version"], cwd=ROOT, env=env, timeout=20)
    except Exception:
        return ""
    return (proc.stdout or proc.stderr).splitlines()[0] if (proc.stdout or proc.stderr) else ""


def cargo_lock_version(lock_path: Path, package: str) -> str:
    if not lock_path.exists():
        return ""
    current: dict[str, str] = {}
    for raw_line in lock_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line == "[[package]]":
            if current.get("name") == package:
                return current.get("version", "")
            current = {}
            continue
        if "=" in line:
            key, value = line.split("=", 1)
            current[key.strip()] = value.strip().strip('"')
    if current.get("name") == package:
        return current.get("version", "")
    return ""


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(url, timeout=120) as response:
        with dest.open("wb") as handle:
            shutil.copyfileobj(response, handle)


def bootstrap_go(toolchain_dir: Path) -> Path | None:
    existing = local_go(toolchain_dir)
    if existing:
        return existing
    machine = platform.machine().lower()
    go_arch = {
        "x86_64": "amd64",
        "amd64": "amd64",
        "aarch64": "arm64",
        "arm64": "arm64",
    }.get(machine)
    if platform.system().lower() != "linux" or go_arch is None:
        return None
    with urllib.request.urlopen("https://go.dev/dl/?mode=json", timeout=30) as response:
        versions = json.loads(response.read().decode("utf-8"))
    archive = None
    for version in versions:
        if not version.get("stable", False):
            continue
        for file_info in version.get("files", []):
            if file_info.get("os") == "linux" and file_info.get("arch") == go_arch and file_info.get("kind") == "archive":
                archive = file_info["filename"]
                break
        if archive:
            break
    if not archive:
        return None
    tar_path = toolchain_dir / "downloads" / archive
    if not tar_path.exists():
        download(f"https://go.dev/dl/{archive}", tar_path)
    if (toolchain_dir / "go").exists():
        shutil.rmtree(toolchain_dir / "go")
    with tarfile.open(tar_path, "r:gz") as tar:
        tar.extractall(toolchain_dir)
    return local_go(toolchain_dir)


def bootstrap_rustc(toolchain_dir: Path) -> Path | None:
    existing = local_rustc(toolchain_dir)
    if existing:
        return existing
    machine = platform.machine().lower()
    rust_target = {
        "x86_64": "x86_64-unknown-linux-gnu",
        "amd64": "x86_64-unknown-linux-gnu",
        "aarch64": "aarch64-unknown-linux-gnu",
        "arm64": "aarch64-unknown-linux-gnu",
    }.get(machine)
    if platform.system().lower() != "linux" or rust_target is None:
        return None
    rustup = toolchain_dir / "downloads" / "rustup-init"
    if not rustup.exists():
        download(f"https://static.rust-lang.org/rustup/dist/{rust_target}/rustup-init", rustup)
    rustup.chmod(rustup.stat().st_mode | stat.S_IXUSR)
    env = os.environ.copy()
    env["CARGO_HOME"] = str(toolchain_dir / "cargo")
    env["RUSTUP_HOME"] = str(toolchain_dir / "rustup")
    proc = run(
        [str(rustup), "-y", "--profile", "minimal", "--default-toolchain", "stable", "--no-modify-path"],
        cwd=ROOT,
        env=env,
        timeout=300,
    )
    if proc.returncode != 0:
        return None
    return local_rustc(toolchain_dir)


def unavailable(language: str, runtime: str, reason: str) -> dict[str, Any]:
    return {"language": language, "runtime": runtime, "status": "unavailable", "reason": reason}


def run_cpp(work_dir: Path, args: argparse.Namespace) -> dict[str, Any]:
    compiler = shutil.which(os.environ.get("CXX", "")) if os.environ.get("CXX") else None
    compiler = compiler or shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        return unavailable("cpp", "cpp_stackless_coroutine", "C++ compiler not found")
    src = work_dir / "async_bench.cpp"
    exe = work_dir / "async_bench_cpp"
    src.write_text(CPP_SOURCE, encoding="utf-8")
    proc = run([compiler, "-O3", "-std=c++20", "-pthread", str(src), "-o", str(exe)], cwd=ROOT, timeout=120)
    if proc.returncode != 0:
        return unavailable("cpp", "cpp_stackless_coroutine", proc.stderr.strip())
    proc = run([str(exe), str(args.tasks), str(args.sleep_ms), str(args.noop_tasks), str(args.workers)], cwd=ROOT, timeout=120)
    result = json_from_stdout(proc)
    result["toolchain"] = command_version(compiler)
    return result


def run_go(work_dir: Path, toolchain_dir: Path, args: argparse.Namespace) -> dict[str, Any]:
    go = None
    if args.bootstrap_toolchains:
        local = bootstrap_go(toolchain_dir)
        go = str(local) if local else None
    go = go or shutil.which("go")
    if not go:
        return unavailable("go", "goroutine", "go toolchain not found")
    src = work_dir / "async_bench.go"
    exe = work_dir / "async_bench_go"
    src.write_text(GO_SOURCE, encoding="utf-8")
    proc = run([go, "build", "-o", str(exe), str(src)], cwd=ROOT, timeout=180)
    if proc.returncode != 0:
        return unavailable("go", "goroutine", proc.stderr.strip())
    proc = run([str(exe), str(args.tasks), str(args.sleep_ms), str(args.noop_tasks), str(args.workers)], cwd=ROOT, timeout=120)
    result = json_from_stdout(proc)
    version = run([go, "version"], cwd=ROOT, timeout=20)
    result["toolchain"] = version.stdout.strip()
    return result


def run_rust(work_dir: Path, toolchain_dir: Path, args: argparse.Namespace) -> dict[str, Any]:
    env = None
    cargo = None
    rustc = None
    if args.bootstrap_toolchains:
        bootstrap_rustc(toolchain_dir)
        local = local_cargo(toolchain_dir)
        cargo = str(local) if local else None
        rust_local = local_rustc(toolchain_dir)
        rustc = str(rust_local) if rust_local else None
        env = rust_env(toolchain_dir) if cargo else None
    cargo = cargo or shutil.which("cargo")
    rustc = rustc or shutil.which("rustc")
    if not cargo:
        return unavailable("rust", "tokio_multi_thread", "cargo toolchain not found")
    crate_dir = work_dir / "rust_tokio"
    src_dir = crate_dir / "src"
    src_dir.mkdir(parents=True, exist_ok=True)
    (crate_dir / "Cargo.toml").write_text(
        "\n".join(
            [
                "[package]",
                'name = "async_bench_tokio"',
                'version = "0.1.0"',
                'edition = "2021"',
                "",
                "[dependencies]",
                'tokio = { version = "1", features = ["rt-multi-thread", "time"] }',
                "",
            ]
        ),
        encoding="utf-8",
    )
    (src_dir / "main.rs").write_text(RUST_SOURCE, encoding="utf-8")
    proc = run([cargo, "build", "--release", "--manifest-path", str(crate_dir / "Cargo.toml")], cwd=ROOT, env=env, timeout=600)
    if proc.returncode != 0:
        return unavailable("rust", "tokio_multi_thread", proc.stderr.strip())
    exe = crate_dir / "target" / "release" / "async_bench_tokio"
    proc = run([str(exe), str(args.tasks), str(args.sleep_ms), str(args.noop_tasks), str(args.workers)], cwd=ROOT, timeout=120)
    result = json_from_stdout(proc)
    rust_version = command_version_with_env(rustc, env) if rustc else ""
    cargo_version = command_version_with_env(cargo, env)
    tokio_version = cargo_lock_version(crate_dir / "Cargo.lock", "tokio")
    result["toolchain"] = "; ".join(part for part in [rust_version, cargo_version, f"tokio {tokio_version}" if tokio_version else "tokio 1"] if part)
    return result


def parse_styio_perf(stdout: str, args: argparse.Namespace) -> dict[str, Any]:
    result: dict[str, Any] = {
        "language": "styio",
        "runtime": "styio_task_scheduler",
        "status": "ok",
        "workers": args.workers,
    }
    for line in stdout.splitlines():
        if line.startswith("styio_task_scheduler_perf "):
            data = dict(token.split("=", 1) for token in line.split()[1:] if "=" in token)
            result["sleep"] = {
                "tasks": args.tasks,
                "sleep_ms": args.sleep_ms,
                "sequential_ms": int(data["sequential_ms"]),
                "parallel_ms": int(data["parallel_ms"]),
                "speedup": float(data["speedup"]),
            }
            result["workers"] = int(data.get("workers", args.workers))
        if line.startswith("styio_task_scheduler_noop "):
            data = dict(token.split("=", 1) for token in line.split()[1:] if "=" in token)
            result["noop"] = {
                "tasks": int(data["tasks"]),
                "total_us": int(data["total_us"]),
                "per_task_us": float(data["per_task_us"]),
            }
            result["workers"] = int(data.get("workers", args.workers))
    if "sleep" not in result or "noop" not in result:
        raise RuntimeError("styio perf target did not emit expected benchmark lines")
    return result


def run_styio(build_dir: Path, args: argparse.Namespace) -> dict[str, Any]:
    proc = run(["cmake", "--build", str(build_dir), "--target", "styio_task_scheduler_perf_test", "-j2"], cwd=ROOT, timeout=300)
    if proc.returncode != 0:
        return unavailable("styio", "styio_task_scheduler", proc.stderr.strip())
    exe = build_dir / "bin" / "styio_task_scheduler_perf_test"
    if not exe.exists():
        return unavailable("styio", "styio_task_scheduler", f"{exe} not found")
    env = os.environ.copy()
    env["STYIO_TASK_THREADS"] = str(args.workers)
    env["STYIO_TASK_NOOP_COUNT"] = str(args.noop_tasks)
    proc = run([str(exe), "--gtest_filter=StyioTaskSchedulerPerf.*"], cwd=ROOT, env=env, timeout=180)
    if proc.returncode != 0:
        return unavailable("styio", "styio_task_scheduler", proc.stderr.strip() or proc.stdout.strip())
    result = parse_styio_perf(proc.stdout, args)
    result["toolchain"] = "repo runtime target"
    return result


def flatten_rows(results: list[dict[str, Any]]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    for result in results:
        base = {
            "language": result.get("language", ""),
            "runtime": result.get("runtime", ""),
            "status": result.get("status", ""),
        }
        if result.get("status") != "ok":
            rows.append({**base, "workload": "", "metric": "reason", "value": result.get("reason", "")})
            continue
        for workload in ("sleep", "noop"):
            payload = result.get(workload, {})
            for metric, value in payload.items():
                rows.append({**base, "workload": workload, "metric": metric, "value": value})
    return rows


def fmt(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, float):
        return f"{value:.3f}"
    return str(value)


def write_report(out_dir: Path, metadata: dict[str, Any], results: list[dict[str, Any]]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "metadata.json").write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    (out_dir / "results.json").write_text(json.dumps({"metadata": metadata, "results": results}, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    with (out_dir / "benchmarks.csv").open("w", encoding="utf-8", newline="") as handle:
        fieldnames = ["language", "runtime", "status", "workload", "metric", "value"]
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(flatten_rows(results))

    lines = [
        "# Async Runtime Benchmark Report",
        "",
        f"- Run ID: `{metadata['run_id']}`",
        f"- Host: `{metadata['host']}`",
        f"- Tasks: `{metadata['tasks']}` sleep tasks x `{metadata['sleep_ms']}ms`, `{metadata['noop_tasks']}` no-op tasks, `{metadata['workers']}` workers/procs",
        "",
        "| Language | Runtime | Status | Sleep seq ms | Sleep parallel ms | Speedup | Noop total us | Noop us/task | Toolchain / reason |",
        "|---|---|---|---:|---:|---:|---:|---:|---|",
    ]
    for result in results:
        sleep = result.get("sleep", {})
        noop = result.get("noop", {})
        reason = result.get("toolchain") if result.get("status") == "ok" else result.get("reason", "")
        lines.append(
            "| "
            + " | ".join(
                [
                    str(result.get("language", "")),
                    str(result.get("runtime", "")),
                    str(result.get("status", "")),
                    fmt(sleep.get("sequential_ms")),
                    fmt(sleep.get("parallel_ms")),
                    fmt(sleep.get("speedup")),
                    fmt(noop.get("total_us")),
                    fmt(noop.get("per_task_us")),
                    str(reason).replace("|", "\\|"),
                ]
            )
            + " |"
        )
    lines.extend(
        [
            "",
            "## Interpretation",
            "",
            "- `sleep` measures whether the runtime actually overlaps blocked tasks; speedup near the worker count indicates real scheduling instead of eager evaluation.",
            "- `noop` measures submit/wait/release overhead for a large fanout of trivial tasks.",
            "- C++ uses C++20 stackless coroutine frames with `co_await` and a small scheduler, Go uses goroutines with `GOMAXPROCS`, Rust uses Tokio's multi-thread runtime, and Styio uses the repository task scheduler target.",
            "- `--bootstrap-toolchains` installs missing Go/Rust toolchains under the build directory; this keeps comparison runs reproducible on machines without system Go or Rust.",
            "",
        ]
    )
    (out_dir / "summary.md").write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run Styio async runtime benchmarks against C++ stackless coroutine, Go goroutine, and Rust Tokio baselines.")
    parser.add_argument("--build-dir", default="build", help="CMake build directory for Styio runtime target.")
    parser.add_argument("--out-dir", default="", help="Output directory. Defaults to benchmark/async-runtime/reports/<run-id>.")
    parser.add_argument("--tasks", type=int, default=DEFAULT_TASKS)
    parser.add_argument("--sleep-ms", type=int, default=DEFAULT_SLEEP_MS)
    parser.add_argument("--noop-tasks", type=int, default=DEFAULT_NOOP_TASKS)
    parser.add_argument("--workers", type=int, default=DEFAULT_WORKERS)
    parser.add_argument("--bootstrap-toolchains", action="store_true", help="Install missing Go/Rust toolchains under the build directory.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    run_id = time.strftime("%Y%m%dT%H%M%SZ", time.gmtime()) + "-async-runtime"
    out_dir = Path(args.out_dir) if args.out_dir else ROOT / "benchmark" / "async-runtime" / "reports" / run_id
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    work_dir = ROOT / args.build_dir / "async-runtime-work" / run_id
    work_dir.mkdir(parents=True, exist_ok=True)
    toolchain_dir = ROOT / args.build_dir / "async-runtime-toolchains"

    metadata = {
        "run_id": run_id,
        "host": host_tag(),
        "tasks": args.tasks,
        "sleep_ms": args.sleep_ms,
        "noop_tasks": args.noop_tasks,
        "workers": args.workers,
        "bootstrap_toolchains": args.bootstrap_toolchains,
    }
    results = [
        run_styio(ROOT / args.build_dir, args),
        run_cpp(work_dir, args),
        run_go(work_dir, toolchain_dir, args),
        run_rust(work_dir, toolchain_dir, args),
    ]
    write_report(out_dir, metadata, results)
    print(out_dir)
    for result in results:
        print(f"{result.get('language')} {result.get('runtime')} {result.get('status')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
