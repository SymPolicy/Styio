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
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

class ThreadPool {
public:
  explicit ThreadPool(int worker_count) {
    for (int i = 0; i < worker_count; ++i) {
      workers_.emplace_back([this]() { worker_loop(); });
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      stopping_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
      worker.join();
    }
  }

  std::future<int64_t> submit(std::function<int64_t()> fn) {
    auto task = std::make_shared<std::packaged_task<int64_t()>>(std::move(fn));
    auto fut = task_>get_future();
    {
      std::lock_guard<std::mutex> lock(mu_);
      queue_.push([task]() { (*task)(); });
    }
    cv_.notify_one();
    return fut;
  }

private:
  std::mutex mu_;
  std::condition_variable cv_;
  std::queue<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  bool stopping_ = false;

  void worker_loop() {
    for (;;) {
      std::function<void()> job;
      {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this]() { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty()) {
          return;
        }
        job = std::move(queue_.front());
        queue_.pop();
      }
      job();
    }
  }
};

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

  ThreadPool pool(workers);
  std::vector<std::future<int64_t>> sleep_futures;
  sleep_futures.reserve(tasks);
  auto par_start = Clock::now();
  for (int i = 0; i < tasks; ++i) {
    sleep_futures.push_back(pool.submit([sleep_ms]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
      return int64_t{1};
    }));
  }
  int64_t sleep_sum = 0;
  for (auto& future : sleep_futures) {
    sleep_sum += future.get();
  }
  auto par_ms = elapsed_ms(par_start, Clock::now());

  std::vector<std::future<int64_t>> noop_futures;
  noop_futures.reserve(noop_tasks);
  auto noop_start = Clock::now();
  for (int i = 0; i < noop_tasks; ++i) {
    noop_futures.push_back(pool.submit([]() { return int64_t{1}; }));
  }
  int64_t noop_sum = 0;
  for (auto& future : noop_futures) {
    noop_sum += future.get();
  }
  auto noop_us = elapsed_us(noop_start, Clock::now());

  std::cout << "{"
            << "\"language\":\"cpp\","
            << "\"runtime\":\"cpp_thread_pool\","
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
  var mu sync.Mutex
  sleepSum := int64(0)
  wg.Add(tasks)
  for i := 0; i < tasks; i++ {
    go func() {
      defer wg.Done()
      time.Sleep(time.Duration(sleepMs) * time.Millisecond)
      mu.Lock()
      sleepSum++
      mu.Unlock()
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
      mu.Lock()
      noopSum++
      mu.Unlock()
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
use std::sync::{Arc, Condvar, Mutex};
use std::sync::mpsc::{self, Sender};
use std::thread;
use std::time::{Duration, Instant};

enum Message {
    Sleep(u64, Arc<(Mutex<i64>, Condvar)>),
    Noop(Arc<(Mutex<i64>, Condvar)>),
    Stop,
}

struct Pool {
    tx: Sender<Message>,
    workers: Vec<thread::JoinHandle<()>>,
}

impl Pool {
    fn new(worker_count: usize) -> Self {
        let (tx, rx) = mpsc::channel::<Message>();
        let rx = Arc::new(Mutex::new(rx));
        let mut workers = Vec::with_capacity(worker_count);
        for _ in 0..worker_count {
            let rx = Arc::clone(&rx);
            workers.push(thread::spawn(move || loop {
                let msg = {
                    let guard = rx.lock().unwrap();
                    guard.recv().unwrap()
                };
                match msg {
                    Message::Sleep(ms, done) => {
                        thread::sleep(Duration::from_millis(ms));
                        let (lock, cv) = &*done;
                        let mut count = lock.lock().unwrap();
                        *count += 1;
                        cv.notify_one();
                    }
                    Message::Noop(done) => {
                        let (lock, cv) = &*done;
                        let mut count = lock.lock().unwrap();
                        *count += 1;
                        cv.notify_one();
                    }
                    Message::Stop => break,
                }
            }));
        }
        Self { tx, workers }
    }

    fn submit(&self, msg: Message) {
        self.tx.send(msg).unwrap();
    }
}

impl Drop for Pool {
    fn drop(&mut self) {
        for _ in 0..self.workers.len() {
            let _ = self.tx.send(Message::Stop);
        }
        while let Some(worker) = self.workers.pop() {
            let _ = worker.join();
        }
    }
}

fn wait_count(done: &Arc<(Mutex<i64>, Condvar)>, target: i64) -> i64 {
    let (lock, cv) = &**done;
    let mut count = lock.lock().unwrap();
    while *count < target {
        count = cv.wait(count).unwrap();
    }
    *count
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let tasks: usize = args[1].parse().unwrap();
    let sleep_ms: u64 = args[2].parse().unwrap();
    let noop_tasks: usize = args[3].parse().unwrap();
    let workers: usize = args[4].parse().unwrap();

    let seq_start = Instant::now();
    for _ in 0..tasks {
        thread::sleep(Duration::from_millis(sleep_ms));
    }
    let seq_ms = seq_start.elapsed().as_millis();

    let pool = Pool::new(workers);
    let sleep_done = Arc::new((Mutex::new(0_i64), Condvar::new()));
    let par_start = Instant::now();
    for _ in 0..tasks {
        pool.submit(Message::Sleep(sleep_ms, Arc::clone(&sleep_done)));
    }
    let sleep_sum = wait_count(&sleep_done, tasks as i64);
    let par_ms = par_start.elapsed().as_millis();

    let noop_done = Arc::new((Mutex::new(0_i64), Condvar::new()));
    let noop_start = Instant::now();
    for _ in 0..noop_tasks {
        pool.submit(Message::Noop(Arc::clone(&noop_done)));
    }
    let noop_sum = wait_count(&noop_done, noop_tasks as i64);
    let noop_us = noop_start.elapsed().as_micros();

    let speedup = if par_ms > 0 { seq_ms as f64 / par_ms as f64 } else { 0.0 };
    let per_task = if noop_tasks > 0 { noop_us as f64 / noop_tasks as f64 } else { 0.0 };
    println!(
        "{{\"language\":\"rust\",\"runtime\":\"rust_std_worker_pool\",\"status\":\"ok\",\"workers\":{},\"sleep\":{{\"tasks\":{},\"sleep_ms\":{},\"sequential_ms\":{},\"parallel_ms\":{},\"speedup\":{},\"sum\":{}}},\"noop\":{{\"tasks\":{},\"total_us\":{},\"per_task_us\":{},\"sum\":{}}}}}",
        workers, tasks, sleep_ms, seq_ms, par_ms, speedup, sleep_sum, noop_tasks, noop_us, per_task, noop_sum
    );
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
        return unavailable("cpp", "cpp_thread_pool", "C++ compiler not found")
    src = work_dir / "async_bench.cpp"
    exe = work_dir / "async_bench_cpp"
    src.write_text(CPP_SOURCE, encoding="utf-8")
    proc = run([compiler, "-O3", "-std=c++20", "-pthread", str(src), "-o", str(exe)], cwd=ROOT, timeout=120)
    if proc.returncode != 0:
        return unavailable("cpp", "cpp_thread_pool", proc.stderr.strip())
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
    rustc = None
    if args.bootstrap_toolchains:
        local = bootstrap_rustc(toolchain_dir)
        rustc = str(local) if local else None
    rustc = rustc or shutil.which("rustc")
    if not rustc:
        return unavailable("rust", "rust_std_worker_pool", "rustc toolchain not found")
    src = work_dir / "async_bench.rs"
    exe = work_dir / "async_bench_rust"
    src.write_text(RUST_SOURCE, encoding="utf-8")
    proc = run([rustc, "-O", str(src), "-o", str(exe)], cwd=ROOT, timeout=180)
    if proc.returncode != 0:
        return unavailable("rust", "rust_std_worker_pool", proc.stderr.strip())
    proc = run([str(exe), str(args.tasks), str(args.sleep_ms), str(args.noop_tasks), str(args.workers)], cwd=ROOT, timeout=120)
    result = json_from_stdout(proc)
    result["toolchain"] = command_version(rustc)
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
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
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
            "- C++ uses a fixed worker pool, Go uses goroutines with `GOMAXPROCS`, Rust uses a standard-library worker pool, and Styio uses the repository task scheduler target.",
            "- `--bootstrap-toolchains` installs missing Go/Rust toolchains under the build directory; this keeps comparison runs reproducible on machines without system Go or Rust.",
            "",
        ]
    )
    (out_dir / "summary.md").write_text("\n".join(lines), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run Styio async runtime benchmarks against C++, Go, and Rust baselines.")
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
