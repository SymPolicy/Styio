# Styio Resource Driver Interface Specification

**Purpose:** `@protocol{…}` **资源驱动** 的 C++ 接口、生命周期与线程约定；与语言侧 `@` 语义、拓扑目标见 `Styio-Language-Design.md`、`Styio-Resource-Topology.md`。

**Last updated:** 2026-04-08

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Target Audience:** Driver plugin developers, runtime engineers

---

## 1. Overview

In Styio, the `@` prefix opens a portal to the external world — files, databases, network feeds, hardware sensors. Every `@protocol{...}` expression is backed by a **Resource Driver**: a C++ plugin that implements a standardized interface.

The driver system is the bridge between Styio's **intent-aware compiler** and the physical reality of I/O.

---

## 2. Architecture: Three-Phase Lifecycle

Every driver instance passes through exactly three phases:

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Subscribe   │────▶│    Pump      │────▶│   Release    │
│  (Connect)   │     │  (Stream)    │     │  (Cleanup)   │
└─────────────┘     └─────────────┘     └─────────────┘
```

| Phase | Trigger | Responsibility |
|-------|---------|----------------|
| **Subscribe** | `<-` handle acquisition or `>>` pipeline start | Establish physical connection, validate schema, report capabilities |
| **Pump** | Continuous, driven by data source or consumer demand | Push new data ticks into Styio pipeline, or receive data from `<<`/`->` writes |
| **Release** | Scope exit, `^^` break, program termination | Flush buffers, close connections, release file descriptors |

---

## 3. C++ Interface Definition

### 3.1 Core Interface

```cpp
#include <string>
#include <vector>
#include <optional>
#include <functional>

struct ResourceIntent {
    std::string protocol;           // "file", "mysql", "binance", etc.
    std::string address;            // "localhost:3306", "BTCUSDT", etc.
    std::vector<std::string> fields; // Fields the user actually accesses
    bool is_source;                 // true if used with >>
    bool is_sink;                   // true if used with << or ->
};

struct TickData {
    uint64_t timestamp_ns;          // Nanosecond-precision timestamp
    const void* data;               // Pointer to raw payload
    size_t data_size;               // Size of payload in bytes
    bool is_undefined;              // true if this tick represents @
    uint32_t reason_code;           // Diagnostic code (debug mode)
};

class IStyioDriver {
public:
    virtual ~IStyioDriver() = default;

    // Phase 1: Subscribe
    // Called when the compiler/runtime initializes the resource.
    // `intent` carries the statically-analyzed field access information.
    // Returns false if the resource cannot be opened or schema validation fails.
    virtual bool on_subscribe(const ResourceIntent& intent) = 0;

    // Phase 2a: Source mode — called by runtime to poll or receive data
    // Driver calls `emit_tick` to push data into the Styio pipeline.
    // This runs on a dedicated I/O thread managed by the runtime.
    virtual void start_pump(std::function<void(const TickData&)> emit_tick) = 0;

    // Phase 2b: Sink mode — called when data flows into the resource via << or ->
    virtual void on_receive(const TickData& data) = 0;

    // Phase 3: Release
    // Flush all pending writes, close connections, release handles.
    virtual void on_release() = 0;

    // Metadata query (for intent-aware optimization)
    // Returns the list of available fields/columns in the resource.
    virtual std::vector<std::string> get_schema() = 0;

    // Health check (for runtime monitoring)
    virtual bool is_healthy() const = 0;
};
```

### 3.2 Registration Macro

```cpp
// In driver_my_sensor.cpp:
#include "styio/driver_registry.hpp"

class MySensorDriver : public IStyioDriver {
    // ... implementation ...
};

STYIO_REGISTER_DRIVER("my_sensor", MySensorDriver);
```

After registration, users can write `@my_sensor{"config"}` in Styio code.

---

## 4. Intent Pushdown Protocol

This is Styio's signature optimization. The compiler analyzes the user's code to determine **exactly which fields** are accessed, then communicates this to the driver.

### 4.1 Example Flow

**User code:**

```
@mysql{"localhost:3306/trades"} >> #(row) => {
    >_($"Price: {row["price"]}, Time: {row["timestamp"]}")
}
```

**Compiler analysis:**
- Fields accessed: `price`, `timestamp`
- Fields NOT accessed: `volume`, `order_id`, `side`, ...

**Intent passed to driver:**

```cpp
ResourceIntent {
    .protocol = "mysql",
    .address = "localhost:3306/trades",
    .fields = {"price", "timestamp"},  // Only these two!
    .is_source = true,
    .is_sink = false
}
```

**Driver optimization:**
The MySQL driver generates `SELECT price, timestamp FROM trades` instead of `SELECT *`, potentially reducing network bandwidth by 10x.

### 4.2 Schema Validation

During `on_subscribe`, the driver should:

1. Call `get_schema()` to fetch available fields from the source
2. Verify that all requested fields in `intent.fields` exist
3. If any field is missing, **return false immediately** — triggering Styio's fail-fast behavior

This catches structural errors (e.g., column renamed, table dropped) **before the first data pulse**, not at runtime.

---

## 5. Driver Categories

### 5.1 Source Drivers (emit data into `>>`)

| Protocol | Resource Syntax | Data Model |
|----------|----------------|------------|
| `file` | `@file{"path.csv"}` | Line-by-line or chunked byte stream |
| `csv` | `@csv{"data.csv"}` | Parsed rows with named columns |
| `json` | `@json{"data.json"}` | Streaming JSON parser (SAX-style) |
| `parquet` | `@parquet{"data.parquet"}` | Columnar read with predicate pushdown |
| `mysql` | `@mysql{"host:port/db"}` | Row cursor with field projection |
| `redis` | `@redis{"host:port/key"}` | Pub/Sub or key-value polling |
| `kafka` | `@kafka{"broker/topic"}` | Consumer group with offset management |
| `binance` | `@binance{"BTCUSDT"}` | WebSocket market data stream |
| `http` | `@http{"https://api.example.com"}` | HTTP GET/POST with response parsing |

### 5.2 Sink Drivers (receive data from `<<` or `->`)

| Protocol | Resource Syntax | Write Model |
|----------|----------------|-------------|
| `file` | `@file{"output.txt"}` | Buffered sequential write |
| `database` | `@database{"redis://..."}` | Key-value upsert or INSERT |
| `influxdb` | `@influxdb{"host:port/db"}` | Time-series point write |
| `alert` | `@alert{"webhook_url"}` | HTTP POST notification |
| `ui` | `@ui{"chart_name"}` | Real-time visualization feed |

### 5.3 Bidirectional Drivers

Some drivers support both `>>` and `<<`:

```
db <- @mysql{"localhost:3306/trades"}
db >> #(row) => { ... }        // read
"new_row_data" << db           // write
```

---

## 6. Threading Model

### 6.1 Source Drivers

Each source driver's `start_pump` runs on a **dedicated I/O thread**. The `emit_tick` callback is thread-safe and transfers data to the main computation thread via a lock-free ring buffer.

```
┌──────────────┐    lock-free     ┌──────────────┐
│ Driver Thread │───────────────▶│ Compute Thread │
│ (I/O bound)   │   ring buffer   │ (CPU bound)    │
└──────────────┘                 └──────────────┘
```

### 6.2 Sink Drivers

`on_receive` is called from the compute thread but must be **non-blocking**. Drivers should internally buffer writes and flush asynchronously (e.g., using a background write thread or `io_uring`/IOCP).

### 6.3 Snapshot Pulls

For `(<< @resource)` (instant pull), the runtime maintains a **shadow slot** per driver. The driver's pump thread atomically updates this slot. The compute thread reads it without locking (atomic load).

---

## 7. Resource Singleton Rule

The runtime enforces a **singleton constraint**: regardless of how many times `@binance{"BTCUSDT"}` appears in code, only **one** physical WebSocket connection is established. All references share the same driver instance.

The compiler detects duplicate resource identifiers during analysis and merges them.

---

## 8. Auto-Detection Protocol

When no protocol prefix is provided (`@{"data"}` or `@("localhost:8080")`), the runtime invokes the **probe chain**:

1. Check if the string looks like a URL → try `http`/`https` driver
2. Check if the string looks like `host:port` → try TCP connection
3. Check if the string is a file path → try `file` driver
4. Check file extension (`.csv`, `.json`, `.parquet`) → try format-specific driver
5. If all probes fail → emit `@` with diagnostic reason

In **strict mode**, auto-detection is disabled. All resources must specify their protocol explicitly.

---

## 9. Configuration: Agent-Assisted Mode

When the `agent_assist` flag is enabled in the Styio config:

```toml
[runtime]
agent_assist = true
agent_endpoint = "https://ai.styio.dev/probe"
```

The runtime can send resource metadata (file name, first 1KB of content, network headers) to a remote AI agent for classification. This allows Styio to handle novel or proprietary data formats without bundling every possible parser.

In production builds, this feature is disabled and has zero overhead.

---

## 10. Error Reporting

### 10.1 Subscribe-Time Errors

If `on_subscribe` returns false, the runtime:
1. Logs the diagnostic message
2. **Terminates the program immediately** (fail-fast)
3. Reports the exact resource expression and line number

### 10.2 Pump-Time Errors

If a driver encounters an error during streaming (network timeout, corrupt data):
1. Emit a tick with `is_undefined = true` and a `reason_code`
2. The Styio pipeline receives `@` with tainted metadata
3. The pipeline continues operating (graceful degradation via `@` propagation)

### 10.3 Reason Codes

| Code | Meaning |
|------|---------|
| 0 | No error (healthy `@` — data legitimately absent) |
| 1 | Network timeout |
| 2 | Connection refused |
| 3 | Schema mismatch (field not found) |
| 4 | Parse error (malformed data) |
| 5 | Authentication failure |
| 6 | Rate limit exceeded |
| 7 | Resource exhausted (disk full, memory limit) |
| 100+ | Driver-specific codes |

---

## Appendix: Consultant's Notes

### On Driver Discovery and Loading

For the "thick library, thin artifact" model:
- **Development:** All drivers are loaded as shared libraries (`.so`/`.dll`) from a well-known directory (`$STYIO_HOME/drivers/`)
- **Production (strict build):** Only the drivers referenced in code are statically linked into the final binary via LTO

This requires the build system to:
1. Scan the AST for all `@protocol{...}` references
2. Resolve each protocol to a driver source file
3. Include only those files in the link step

### On Columnar File Formats

For `@parquet` and similar columnar formats, the intent pushdown is particularly powerful. Apache Parquet stores column metadata (min/max statistics, bloom filters) in the file footer. A Styio-aware Parquet driver can:

1. Read only the footer (a few KB)
2. Check if the requested columns exist (fail-fast if not)
3. Use column statistics to skip entire row groups that don't match filter predicates
4. Read only the requested column chunks via `pread`/`mmap` offset seeking

This can reduce I/O from gigabytes to kilobytes for selective queries.

### On Hot Reload

The driver interface is designed to support hot-reloading of strategy logic:
1. Old driver instance receives `on_release()`
2. New driver instance receives `on_subscribe()` with the same intent
3. State ledger memory is preserved and remapped

The key challenge is ensuring the new driver's schema is compatible with the old one. The runtime should verify field-level compatibility before completing the swap.
