# cc_vrwrapper :: sc-trace

**Signal watching, callback triggers, and enhanced tracing for SystemC.**

A lightweight, header-only C++17/C++20 library that wraps SystemC's tracing facilities with a clean, type-safe API. It provides signal-change callbacks, VCD tracing with regex filtering, and JSON waveform output with delta-cycle visibility — all with zero runtime overhead on the simulation hot path.

---

## Table of Contents

- [cc\_vrwrapper :: sc-trace](#cc_vrwrapper--sc-trace)
  - [Table of Contents](#table-of-contents)
  - [Features](#features)
  - [Requirements](#requirements)
  - [Quick Start](#quick-start)
  - [File Structure](#file-structure)
  - [Usage Guide](#usage-guide)
  - [HTML Waveform Viewer](#html-waveform-viewer-1)
  - [API Reference](#api-reference)
  - [Limitations](#limitations)

---

## Features

### Watch API (Callbacks)
- `watch(sig, cb)` — fire on any value change
- `watch_rising(sig, cb)` — fire on rising edge (0→1)
- `watch_falling(sig, cb)` — fire on falling edge (1→0)
- `watch_value(sig, val, cb)` — fire when signal equals a specific value
- `watch_transition(sig, from, to, cb)` — fire on a specific value transition
- Callbacks may take either `void(const T&)` or `void()` — both supported
- Edge detection supported for: `bool`, `sc_logic`, integral types, `sc_int<W>`, `sc_uint<W>`, `sc_bigint<W>`, `sc_biguint<W>`
- Bus types (`sc_lv<W>`, `sc_bv<W>`) are correctly rejected at compile time for edge operations

### TraceManager (VCD)
- Wraps SystemC's `sc_trace_file` with RAII
- Regex-based include/exclude filters for signal names
- Hierarchical naming via `set_top_scope()` (dot-separated scopes)
- Signal grouping via `trace_with_group()`
- Auto-trace all signals of a module (`trace_all`, recursive option)
- Auto-trace top-level signals declared in `sc_main` (`trace_top_level_signals`)
- Optional gzip compression of the output file (zero simulation overhead)

### JsonTrace
- JSON waveform output for browser-based viewing
- Three delta-cycle modes:
  - `OFF` — minimal overhead, no delta info
  - `GLOBAL` — records the global `sc_delta_count()` for each change
  - `TRIGGERED` — records cycle + local delta relative to a trigger signal (e.g., `clk`)
- `snapshot()` to capture writes that happen between `sc_start()` calls
- Outputs `triggers[]` array mapping cycle numbers to absolute times
- Post-processing in `write()` — zero overhead during simulation

### HTML Waveform Viewer
- Standalone HTML file (no server required)
- Drag-and-drop a `wave.json` to render waveforms
- Zoom, pan, cursor, trigger markers, delta annotations
- Dark theme, GTKWave-like experience

---

## Requirements

- **C++17 or C++20** (both supported in the same files)
- **SystemC 3.0+** (tested with Accellera SystemC 3.0.1)
- **zlib** (only required if `compress_output(true)` is used)
- **POSIX threads** (`pthread`)

---

## Quick Start

```cpp
#include <systemc>
#include "sc_trace.hpp"

namespace tr = cc_vrwrapper::trace;
using namespace sc_core;
using namespace sc_dt;

SC_MODULE(Counter) {
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_out<sc_uint<8>> count;

    SC_CTOR(Counter) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        sc_uint<8> c = 0;
        count.write(c);
        while (true) {
            wait();
            if (reset.read()) c = 0;
            else              c = c + 1;
            count.write(c);
        }
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 5, SC_NS);
    sc_signal<bool> reset("reset");
    sc_signal<sc_uint<8>> count("count");

    Counter dut("dut");
    dut.clk(clk);
    dut.reset(reset);
    dut.count(count);

    // Register common signal types for trace_all()
    CC_VRWRAPPER_REGISTER_COMMON_TYPES();

    // VCD tracing
    auto tf = tr::create_trace_file("wave");
    tf->set_time_unit(10.0, SC_PS);
    tf->set_top_scope("tb");
    tf->trace(clk, "clk");
    tf->trace(reset, "reset");
    tf->trace(count, "count");

    // JSON tracing with TRIGGERED delta mode
    auto jt = tr::create_json_trace("wave.json");
    jt->set_delta_trigger(clk);
    jt->trace(clk, "clk");
    jt->trace(reset, "reset");
    jt->trace(count, "count");
    jt->start();

    // Watch a specific value
    tr::watch_value(count, sc_uint<8>(10), []() {
        std::cout << "count reached 10 @ " << sc_time_stamp() << "\n";
    });

    // Stimulus
    reset.write(true);
    jt->snapshot();
    sc_start(5, SC_NS);

    reset.write(false);
    jt->snapshot();
    sc_start(100, SC_NS);

    tf->close();
    jt->write();
    return 0;
}
```

Compile:

```bash
g++ -std=c++20 -I$SYSTEMC_HOME/include \
    -L$SYSTEMC_HOME/lib -Wl,-rpath,$SYSTEMC_HOME/lib \
    main.cpp -lsystemc -lz -lm -lpthread -o sim
```

---

## File Structure

```
sc-trace/
├── sc_trace.hpp       # Umbrella header (includes all four below)
├── utils.hpp          # Type traits, value conversion, NameFilter,
│                      #   type-erased signal tracing registry
├── watch.hpp          # SignalWatcher module + watch_* free functions
├── trace.hpp          # TraceManager (VCD) — RAII, filters, grouping,
│                      #   trace_all, trace_top_level_signals, gzip
├── json_trace.hpp     # JsonTrace — JSON output, delta-cycle modes,
│                      #   snapshot(), trigger support
└── viewer.html        # Standalone waveform viewer for wave.json
```

---

## Usage Guide

### 1. Watch API

The Watch API installs callbacks that fire when a signal changes. Internally, each watcher is an `SC_METHOD` process sensitive to the signal's `value_changed_event()`.

> **IMPORTANT:** All `watch_*()` calls **must** be made:
> 1. **After** the port is bound to a signal (calling on an unbound `sc_in`/`sc_out` raises SystemC error `E112`).
> 2. **Before** `sc_start()` is called (SystemC does not allow registering processes after simulation starts).
>
> Recommended patterns:
> - Call from `sc_main()` after all `dut.port(signal)` bindings are done.
> - Override `start_of_simulation()` in your module and call from there.

#### `watch(sig, cb)` — Any value change

```cpp
// 1-arg form: receives the new value
tr::watch(count, [](const sc_uint<8>& v) {
    std::cout << "count = " << v << "\n";
});

// 0-arg form: just a notification
tr::watch(enable, []() {
    std::cout << "enable changed\n";
});
```

#### `watch_rising(sig, cb)` — Rising edge

```cpp
tr::watch_rising(clk, []() {
    std::cout << "clk rising @ " << sc_time_stamp() << "\n";
});
```

Edge semantics:
- `bool` and `sc_logic`: `0 → 1`
- Integral types (`sc_int`, `sc_uint`, `sc_bigint`, `sc_biguint`): `0 → non-zero`

Bus types (`sc_lv`, `sc_bv`) do **not** support edges — a `static_assert` will reject them at compile time. Use `watch_value` or `watch_transition` for bus signals.

#### `watch_falling(sig, cb)` — Falling edge

```cpp
tr::watch_falling(clk, [](const bool&) {
    std::cout << "clk falling\n";
});
```

#### `watch_value(sig, val, cb)` — Value match

```cpp
// 0-arg form
tr::watch_value(count, sc_uint<8>(42), []() {
    std::cout << "count reached 42!\n";
});

// 1-arg form: receives the matched value
tr::watch_value(state, sc_lv<4>("1010"), [](const sc_lv<4>& v) {
    std::cout << "state is 0xA\n";
});
```

#### `watch_transition(sig, from, to, cb)` — Specific transition

```cpp
tr::watch_transition(state,
    sc_lv<4>("0010"),    // from = MEDIUM
    sc_lv<4>("0100"),    // to   = HIGH
    []() {
        std::cout << "MEDIUM -> HIGH\n";
    });
```

#### `clear_watchers()` — Cleanup

```cpp
// Only safe BEFORE sc_start() or AFTER sc_start() returns.
// Watchers are normally cleaned up automatically at program exit.
tr::clear_watchers();
```

---

### 2. TraceManager (VCD)

`TraceManager` is an RAII wrapper around SystemC's `sc_trace_file`. It supports regex filters, hierarchical naming, auto-tracing, and optional gzip compression.

#### Basic usage

```cpp
auto tf = tr::create_trace_file("wave");
tf->set_time_unit(10.0, SC_PS);  // required for sub-ns clocks
tf->trace(clk, "clk");
tf->trace(data, "data");
// ...
tf->close();  // optional — also called by destructor
```

#### Time unit

Always set the time unit **before** calling `trace()`. If the simulation has sub-nanosecond events (e.g., a 5 ns clock with 50% duty cycle produces edges at 2.5 ns), the default 1 ns VCD resolution will silently drop falling edges and SystemC will emit `W713` warnings.

```cpp
tf->set_time_unit(10.0, SC_PS);   // 10 ps resolution
```

#### Regex filters

Filters apply to the **final** signal name (after `top_scope` and group prefixes are applied).

```cpp
// Only trace signals whose name contains "clk" or "data"
tf->set_include_filter("clk|data");

// Exclude all signals whose name contains "internal"
tf->set_exclude_filter("internal");
```

#### Hierarchical naming (`set_top_scope`)

SystemC always wraps everything under a `SystemC` top scope in VCD — this cannot be removed. However, `set_top_scope()` lets you organize signals cleanly below that:

```cpp
tf->set_top_scope("tb");
tf->trace(clk, "clk");           // → tb.clk
tf->trace(data, "data");         // → tb.data
```

In GTKWave/Surfer, this appears as:

```
SystemC
└── tb
    ├── clk
    └── data
```

#### Signal grouping (`trace_with_group`)

```cpp
tf->trace_with_group(count, "count", "datapath");
// → tb.datapath.count

tf->trace_with_group(state, "state", "control");
// → tb.control.state
```

---

### 3. JsonTrace

`JsonTrace` records every signal change during simulation and outputs a JSON file that can be loaded by the included `viewer.html` or analyzed with `jq` / Python.

#### Basic usage

```cpp
auto jt = tr::create_json_trace("wave.json");
jt->trace(clk, "clk");
jt->trace(data, "data");
jt->start();

sc_start(100, SC_NS);

jt->write();
```

#### `snapshot()` — Capture inter-`sc_start` writes

SystemC's `value_changed_event()` does **not** fire for writes that happen outside of `sc_start()` (i.e., between `sc_start()` calls or before the first `sc_start()`). This is a SystemC limitation, not a library bug.

`snapshot()` re-reads all registered signals and records any changes:

```cpp
jt->trace(reset, "reset");

reset.write(true);        // ← write before sc_start
jt->snapshot();           // ← captures the change
sc_start(5, SC_NS);

reset.write(false);       // ← write between sc_start calls
jt->snapshot();           // ← captures the change
sc_start(100, SC_NS);
```

**Rule of thumb:** call `snapshot()` before every `sc_start()` if you wrote to any signal since the last `sc_start()`.

---

### 4. Delta Cycle Modes

SystemC can perform multiple "delta cycles" at the same simulation time. This is where RTL-style designs often hide subtle bugs (zero-time glitches, combinatorial settling, race conditions). `JsonTrace` provides three modes for delta-cycle visibility.

#### `OFF` (default)

No delta info. Minimal overhead.

```json
{"time": 5.0, "value": "1"}
```

#### `GLOBAL`

Records the global `sc_delta_count()` for each change.

```cpp
jt->set_record_delta_cycles(true);   // or:
jt->set_delta_mode(tr::JsonTrace::DeltaMode::GLOBAL);
```

```json
{"time": 5.0, "delta": 6, "value": "1"}
```

The `delta` field here is the absolute delta counter at the moment of the change. Multiple changes at the same `time` but with different `delta` values indicate delta-cycle activity.

#### `TRIGGERED` (recommended)

Records a **cycle number** and a **local delta** relative to a trigger signal's rising edge (typically a clock). This is the most useful mode for debugging clocked designs.

```cpp
jt->set_delta_trigger(clk);
// Auto-enables TRIGGERED mode if delta_mode was OFF.
```

Output format:

```json
{
  "triggers": [
    {"cycle": 1, "time": 0,   "delta": 1},
    {"cycle": 2, "time": 5,   "delta": 6},
    {"cycle": 3, "time": 10,  "delta": 11}
  ],
  "signals": [
    {
      "name": "count", "width": 8,
      "changes": [
        {"time": 0,  "cycle": 0, "delta": 0, "value": "0"},
        {"time": 5,  "cycle": 2, "delta": 1, "value": "1"},
        {"time": 10, "cycle": 3, "delta": 1, "value": "2"}
      ]
    }
  ]
}
```

**How to interpret:**
- `cycle` — which clock period this change belongs to. `0` means "before the first trigger" (initialization phase).
- `delta` — local delta count since the start of that cycle. `0` means the change happened in the same delta as the trigger itself.

Example reading: `{"time": 5, "cycle": 2, "delta": 1, "value": "1"}` means: "in clock cycle 2, one delta cycle after the rising edge of `clk`, the value became `1`."

This is exactly the visibility needed to answer questions like:
- "Did this signal change in the same delta as the clock edge, or one delta later?"
- "Why does this combinational path glitch at `t=10ns`?"
- "Is my `SC_THREAD` waking up in delta 0 or delta 1?"

---

### 5. Auto-Tracing

#### `trace_top_level_signals()`

Traces all `sc_signal` and `sc_clock` objects declared directly in `sc_main()`:

```cpp
size_t n = tf->trace_top_level_signals();
std::cout << "Traced " << n << " top-level signals\n";
```

#### `trace_all(mod, recursive)`

Traces all `sc_signal` children of a module. Optionally recurses into submodules:

```cpp
// Trace only direct children of dut
tf->trace_all(dut, false);

// Trace dut and all submodules recursively
tf->trace_all(dut, true);
```

**Note:** `trace_all()` traces `sc_signal`s **inside** modules, not ports. Ports are typically bound to external signals (which `trace_top_level_signals()` already covers).

---

### 6. Signal Type Registration

Because C++ templates produce distinct types for each width (`sc_lv<8>` ≠ `sc_lv<16>`), runtime dispatch in `trace_all()` must know which types to try. The library uses a registration model:

#### Pre-registered types

`bool` and `sc_logic` are pre-registered and always available.

#### Registering additional types

```cpp
tr::register_signal_type<sc_dt::sc_lv<8>>();
tr::register_signal_type<sc_dt::sc_lv<16>>();
tr::register_signal_type<sc_dt::sc_int<32>>();
tr::register_signal_type<sc_dt::sc_biguint<128>>();
```

Must be called **before** `trace_all()` or `trace_top_level_signals()`. Duplicate registrations are silently ignored.

#### Convenience macro

Registers all common SystemC signal types at once (widths 8/16/32/64/128 for `sc_int`, `sc_uint`, `sc_lv`, `sc_bv`, `sc_bigint`, `sc_biguint`):

```cpp
CC_VRWRAPPER_REGISTER_COMMON_TYPES();
```

For non-standard widths, add them explicitly:

```cpp
CC_VRWRAPPER_REGISTER_COMMON_TYPES();
tr::register_signal_type<sc_dt::sc_lv<12>>();
tr::register_signal_type<sc_dt::sc_int<5>>();
```

---

## HTML Waveform Viewer

The library ships with a standalone HTML viewer (`viewer.html`) for visualizing `wave.json` files. No server required — just open the file in a browser.

### Features

- **Drag-and-drop** a `wave.json` file onto the window
- **Mouse wheel zoom** (centered on cursor)
- **Click-and-drag pan**
- **Click** to place a cursor; status bar shows time, cycle, delta, signal, value at the cursor
- **Trigger markers** as green dashed vertical lines with cycle labels
- **Delta annotations** — when multiple changes occur at the same `time` but different `delta`, a small `d0→d1` label is drawn
- 1-bit signals rendered as square waves
- Multi-bit signals rendered as hex value boxes
- `X` and `Z` values rendered with hatching (yellow for X, pink for Z)
- Dark theme matching modern editors

### Usage

1. Generate a `wave.json` by running a simulation with `JsonTrace`.
2. Open `viewer.html` in any modern browser.
3. Click "Open wave.json" or simply drag the file onto the window.

---

## API Reference

### Free functions (`watch.hpp`)

| Function | Description |
|----------|-------------|
| `watch(sig, cb)` | Call `cb` on any value change |
| `watch_rising(sig, cb)` | Call `cb` on rising edge (0→1) |
| `watch_falling(sig, cb)` | Call `cb` on falling edge (1→0) |
| `watch_value(sig, val, cb)` | Call `cb` when `sig == val` |
| `watch_transition(sig, from, to, cb)` | Call `cb` when `sig` goes `from → to` |
| `clear_watchers()` | Destroy all watchers (see warning in source) |
| `register_signal_type<T>()` | Register type `T` for runtime dispatch |
| `CC_VRWRAPPER_REGISTER_COMMON_TYPES()` | Macro: register all common types |
| `create_trace_file(name)` | Returns `shared_ptr<TraceManager>` |
| `create_json_trace(name)` | Returns `shared_ptr<JsonTrace>` |

### `TraceManager` (`trace.hpp`)

| Method | Description |
|--------|-------------|
| `TraceManager(filename)` | Constructor — creates the VCD file |
| `set_time_unit(v, unit)` | Set VCD time resolution (call before `trace`) |
| `set_top_scope(scope)` | Set prefix applied to all subsequent traces |
| `set_include_filter(re)` | Regex include filter |
| `set_exclude_filter(re)` | Regex exclude filter |
| `clear_filters()` | Remove all filters |
| `trace(sig, name)` | Add a signal |
| `trace_with_group(sig, name, group)` | Add a signal under a group prefix |
| `trace_all(mod, recursive)` | Auto-trace signals in a module |
| `trace_top_level_signals()` | Auto-trace signals in `sc_main` |
| `compress_output(true)` | Enable gzip on close |
| `close()` | Close the file (also called by destructor) |
| `raw()` | Get underlying `sc_trace_file*` |

### `JsonTrace` (`json_trace.hpp`)

| Method | Description |
|--------|-------------|
| `JsonTrace(filename)` | Constructor |
| `set_include_filter(re)` | Regex include filter |
| `set_exclude_filter(re)` | Regex exclude filter |
| `set_record_delta_cycles(bool)` | Enable/disable GLOBAL delta mode |
| `set_delta_mode(mode)` | Set delta mode (`OFF`/`GLOBAL`/`TRIGGERED`) |
| `set_delta_trigger(sig)` | Set trigger signal (auto-enables TRIGGERED) |
| `trace(sig, name)` | Register a signal for JSON tracing |
| `snapshot()` | Re-read all signals and record changes |
| `start()` | Mark simulation start (optional) |
| `write()` | Write the JSON file (also called by destructor) |
| `signal_count()` | Number of registered signals |
| `trigger_count()` | Number of recorded trigger events |

---

## Limitations

1. **No `sc_in`/`sc_out` tracing in `trace_all()`** — Only `sc_signal` and `sc_clock` children of a module are auto-traced. Ports should be traced via the bound external signal (covered by `trace_top_level_signals()`).

2. **SystemC `SystemC` top scope** — SystemC always wraps VCD content under a `SystemC` scope; this cannot be removed without writing a custom VCD writer. `set_top_scope()` organizes signals below that level.

3. **Delta mode must be set before `trace()`** — The delta-recording decision is captured into the watcher closure at `trace()` time for stable, branch-predictable behavior.

4. **`clear_watchers()` is unsafe during `sc_start()`** — SystemC does not support destroying modules with registered processes while the kernel is running. Only call before `sc_start()` or after `sc_start()` returns.

5. **`snapshot()` is required for inter-`sc_start` writes** — SystemC's `value_changed_event()` does not fire for writes outside of `sc_start()`. This is a SystemC limitation, not a library bug.

6. **Gzip requires `-lz`** — Link with `-lz` if `compress_output(true)` is used. The library degrades gracefully if compression fails (warns and leaves the file uncompressed).

---
