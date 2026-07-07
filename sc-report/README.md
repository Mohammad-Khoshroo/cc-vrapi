# cc_vrwrapper :: sc-report

> Custom SystemC report handler with pluggable sinks, multiple formats,
> filtering, statistics, rotation, and color themes.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![SystemC](https://img.shields.io/badge/SystemC-3.0+-red.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

---

## Table of Contents

- [cc\_vrwrapper :: sc-report](#cc_vrwrapper--sc-report)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Features](#features)
  - [Installation](#installation)
  - [Quick Start](#quick-start)
  - [File Structure](#file-structure)
  - [API Reference](#api-reference)
  - [Usage Guide](#usage-guide)
  - [Examples](#examples)
  - [C++17 / C++20 Support](#c17--c20-support)
  - [Thread Safety](#thread-safety)
  - [Limitations](#limitations-1)
  - [Troubleshooting](#troubleshooting)
  - [Changelog](#changelog)

---

## Introduction

`sc-report` is a lightweight, extensible library for customizing the SystemC reporting system. Instead of the default SystemC handler, it provides a complete reporting system with modern features:

- **Terminal logging** with ANSI colors (TTY only)
- **File logging** without colors, with automatic rotation and gzip compression
- **JSON / Logfmt / Text** output formats
- **Filtering** by severity, message type, and regex
- **Atomic statistics** for counting reports
- **Pluggable sinks** — terminal, file, callback, network
- **Hierarchy capture** — process type detection (CTHREAD/THREAD/METHOD)
- **Log diff** for regression testing

---

## Features

| Feature | Description |
|---------|-------------|
| ✅ Multi-format | Text / JSON / Logfmt |
| ✅ Multi-sink | Terminal / File / Callback / Network |
| ✅ Color themes | Dark / Light (with TTY detection) |
| ✅ Severity filter | `min_severity` + `enabled_severities` |
| ✅ Message type filter | suppress / allow-only |
| ✅ Regex filter | allow / deny patterns |
| ✅ Statistics | atomic counters per severity |
| ✅ File rotation | by size + gzip compression |
| ✅ Process detection | CTHREAD / THREAD / METHOD / extern |
| ✅ Thread safety | mutex-protected config / filters / sinks |
| ✅ C++17 & C++20 | dual support in same files |
| ✅ Auto mkdir | creates parent directories for log path |
| ✅ Log diff | normalize timestamps + line-by-line compare |

---

## Installation

### Prerequisites

- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+ (with C++17 or C++20)
- **SystemC**: version 3.0 or later
- **POSIX** (for NetworkSink and gzip rotation; Windows is partially supported)

### Compilation

```bash
# C++20
g++ -std=c++20 -I./sc-report your_code.cpp -lsystemc -lpthread -o your_app

# C++17
g++ -std=c++17 -I./sc-report your_code.cpp -lsystemc -lpthread -o your_app
```

### CMake

```cmake
target_include_directories(your_app PRIVATE sc-report/)
target_link_libraries(your_app systemc pthread)
```

---

## Quick Start

```cpp
#include <systemc>
#include "sc-report.hpp"

using namespace cc_vrwrapper::report;

int sc_main(int argc, char* argv[])
{
    handler::init_report_handler("logs/my_app.log");

    SC_REPORT_INFO("MAIN", "Simulation started");
    SC_REPORT_WARNING("MAIN", "Something is suspicious");
    SC_REPORT_ERROR("MAIN", "Something went wrong");

    sc_start();
    stats::print();
    handler::close_report_log();
    return 0;
}
```

**Output:**
[INFO] MAIN: Simulation started @ 0 s (main.cpp:8) in process: sc_main [extern]
[WARNING] MAIN: Something is suspicious @ 0 s (main.cpp:9) in process: sc_main [extern]
[ERROR] MAIN: Something went wrong @ 0 s (main.cpp:10) in process: sc_main [extern]


---

## File Structure

```txt
sc-report/
├── sc-report.hpp      # Umbrella header (include this)
├── utils.hpp          # TTY check, mkdir, JSON/logfmt escaping, 
├── colors.hpp         # ANSI codes + theme management (atomic)
├── config.hpp         # Configuration struct + thread-safe setters
├── filters.hpp        # Severity / msg_type / regex filters
├── stats.hpp          # Atomic statistics counters
├── formatter.hpp      # Text / JSON / Logfmt formatters
├── sinks.hpp          # Sink interface + Terminal/File/Callback/Network sinks
├── handler.hpp        # Custom report handler + public API
├── diff.hpp           # Log diff for regression testing
└── macros.hpp         # VR_LOG_* convenience macros
```

---

## API Reference

### Initialization

#### `handler::init_report_handler(path = "")`

Installs the custom handler. This function:
- Sets the log file path (if `path` is non-empty)
- Adds TerminalSink and FileSink based on the config
- Sets SystemC actions to `SC_DO_NOTHING` (prevents duplicate output)
- Installs the custom handler

```cpp
handler::init_report_handler();                      // with defaults
handler::init_report_handler("output/run.log");      // with custom path
```

#### `handler::close_report_log()`

Flushes all sinks. Always call it at the end of `sc_main`.

---

### Configuration

All setters are **thread-safe** (mutex-protected).

| Setter | Type | Default | Description |
|--------|------|---------|-------------|
| `set_log_file_path(p)` | `std::string` | `"logs/systemc_report.log"` | Log file path |
| `set_log_as_json(f)` | `bool` | `false` | JSON output (disables logfmt) |
| `set_log_as_logfmt(f)` | `bool` | `false` | Logfmt output (disables JSON) |
| `set_throw_on_error(f)` | `bool` | `false` | Throw an exception for ERROR/FATAL |
| `set_throw_threshold(s)` | `sc_severity` | `SC_ERROR` | Minimum severity for throwing |
| `set_force_no_color(f)` | `bool` | `false` | Disable colors even on a TTY |
| `set_log_to_terminal(f)` | `bool` | `true` | Enable/disable the terminal sink |
| `set_log_to_file(f)` | `bool` | `true` | Enable/disable the file sink |
| `set_min_severity(s)` | `sc_severity` | `SC_INFO` | Minimum severity to display |
| `set_max_file_size(b)` | `size_t` | `10 MB` | Max file size before rotation |
| `set_compress_rotated(f)` | `bool` | `true` | Gzip-compress the rotated file |
| `set_capture_hierarchy(f)` | `bool` | `true` | Record the process name in the log |

#### Reading the config

```cpp
auto c = config::get_config();  // thread-safe copy
std::cout << "Log file: " << c.log_file_path << "\n";
```

---

### Filters

Filters live in the `filters` namespace and are **thread-safe**.

#### Severity filter

```cpp
config::set_min_severity(SC_WARNING);  // WARNING and above only

// or, more precisely:
filters::set_enabled_severities({SC_INFO, SC_ERROR});  // INFO and ERROR only
```

#### Message type filter

```cpp
filters::suppress_msg_type("VERBOSE");
filters::allow_only_msg_type("MAIN");
filters::unsuppress_msg_type("VERBOSE");

// full cleanup
filters::suppressed_msg_types.clear();
filters::allowed_msg_types.clear();
```

#### Regex filter

```cpp
filters::add_message_regex_deny("password=\\w+");
filters::add_message_regex_allow("^(READ|WRITE) ");
// If the regex is invalid, an error message is printed to stderr
```

#### Filter application order

1. `config::min_severity`
2. `filters::enabled_severities`
3. `filters::suppressed_msg_types`
4. `filters::allowed_msg_types`
5. `filters::message_regex_deny`
6. `filters::message_regex_allow`

---

### Sinks

#### TerminalSink

Output: stderr with colors (if TTY). Always uses the text format.

#### FileSink

Output: file in text/json/logfmt format (no colors), with automatic rotation and gzip.

#### CallbackSink

```cpp
handler::add_callback([](const formatter::ReportInfo& info) {
    std::cout << "[CB] " << info.message << "\n";
});
```

#### NetworkSink

```cpp
handler::add_network_sink("127.0.0.1", 9090);
// JSON format, with reconnect logic
```

#### Sink Manager

```cpp
sinks::sink_manager().add(custom_sink);
sinks::sink_manager().remove(custom_sink);
sinks::sink_manager().clear();
```

#### Writing a custom Sink

```cpp
class DatabaseSink : public sinks::Sink {
public:
    void write(const formatter::ReportInfo& info) override { /* ... */ }
    void flush() override { /* ... */ }
};
```

---

### Formatters

Three output formats are supported:

#### Text (default)

```txt
[INFO] TYPE: msg @ 0 s (file.cpp:10) in process: sc_main [extern]
```

#### JSON

```json
{"severity":"INFO","msg_type":"TYPE","message":"msg","time":"0 s","file":"file.cpp","line":10,"process":"sc_main [extern]"}
```

#### Logfmt

severity=info msg_type=TYPE message=msg time="0 s" file=file.cpp line=10 process="sc_main [extern]"


#### The ReportInfo struct

```cpp
struct ReportInfo {
    sc_core::sc_severity severity;
    std::string          msg_type;
    std::string          message;
    std::string          timestamp;
    std::string          file;
    int                  line;
    std::string          process;
};
```

---

### Themes

```cpp
colors::set_theme(colors::Theme::Dark);   // default
colors::set_theme(colors::Theme::Light);
```

| Severity | Dark | Light |
|----------|------|-------|
| INFO | 🟢 Green | 🔵 Blue |
| WARNING | 🟡 Yellow (bold) | 🟣 Magenta (bold) |
| ERROR | 🔴 Red (bold) | 🔴 Red (bold) |
| FATAL | ⚪ Red BG + White (bold) | ⚪ Red BG + White (bold) |

> Colors are only enabled on a TTY, and can be disabled entirely by setting `force_no_color = true`.

---

### Statistics

Atomic per-severity report counters:

```cpp
auto& s = stats::get();
std::cout << "INFO: "    << s.info.load()    << "\n";
std::cout << "WARNING: " << s.warning.load() << "\n";
std::cout << "ERROR: "   << s.error.load()   << "\n";
std::cout << "FATAL: "   << s.fatal.load()   << "\n";
std::cout << "TOTAL: "   << s.total.load()   << "\n";

stats::print();           // to stderr
stats::print(std::cout);  // to any stream
stats::reset();           // zero the counters
```

---

### Macros

```cpp
VR_LOG_INFO("TYPE", "message");
VR_LOG_WARNING("TYPE", "message");
VR_LOG_ERROR("TYPE", "message");
VR_LOG_FATAL("TYPE", "message");

// with file:line embedded in the message:
VR_LOG_INFO_LOC("TYPE", "message");
VR_LOG_WARNING_LOC("TYPE", "message");
VR_LOG_ERROR_LOC("TYPE", "message");
```

---

### Log Diff

For regression testing:

```cpp
auto result = diff::compare("logs/baseline.log", "logs/current.log");
if (result.identical) {
    std::cout << "✅ No regression\n";
} else {
    std::cout << "❌ " << result.added << " added, "
              << result.removed << " removed\n";
}
```

Lines are normalized before comparison: the ` @ <time>` segment is stripped.

---

## Usage Guide

This section covers all usage scenarios of the library with practical examples.

### 1. Initial Setup

#### Simple setup (with defaults)

```cpp
handler::init_report_handler();
// - Terminal: stderr with colors
// - File: logs/systemc_report.log
// - Format: text
// - Min severity: SC_INFO
```

#### Full setup with all settings

```cpp
// 1. Config before init
config::set_log_file_path("output/run.log");
config::set_log_as_json(true);
config::set_min_severity(SC_WARNING);
config::set_max_file_size(50 * 1024 * 1024);
config::set_compress_rotated(true);
config::set_capture_hierarchy(true);
config::set_force_no_color(false);

// 2. Initialize
handler::init_report_handler();

// 3. Run
SC_REPORT_INFO("APP", "Started");
sc_start();

// 4. Cleanup
stats::print();
handler::close_report_log();
```

#### Correct initialization order
```txt
1. config::set_*()      ← before init
2. filters::*           ← before or after init (optional)
3. handler::init()      ← install the handler
4. SC_REPORT_*          ← use
5. handler::close()     ← cleanup
```

> ⚠️ To apply a completely new configuration, call `init_report_handler()` again (it clears the previous sinks).

---

### 2. Working with Severity Levels

| Severity | Numeric value | Use case |
|----------|---------------|----------|
| `SC_INFO` | 0 | General information |
| `SC_WARNING` | 1 | Non-critical warnings |
| `SC_ERROR` | 2 | Recoverable errors |
| `SC_FATAL` | 3 | Unrecoverable errors → `abort()` |

#### Reporting

```cpp
SC_REPORT_INFO("TYPE", "message");
SC_REPORT_WARNING("TYPE", "message");
SC_REPORT_ERROR("TYPE", "message");
SC_REPORT_FATAL("TYPE", "message");  // ← triggers abort()

// or with the VR_LOG macros:
VR_LOG_INFO("TYPE", "message");
VR_LOG_WARNING_LOC("TYPE", "message");  // with file:line in the message
```

#### Setting the minimum severity

```cpp
config::set_min_severity(SC_WARNING);  // WARNING + ERROR + FATAL only
config::set_min_severity(SC_ERROR);    // ERROR + FATAL only
config::set_min_severity(SC_INFO);     // everything (default)
```

#### Selecting exact severities

```cpp
filters::set_enabled_severities({SC_INFO, SC_ERROR});  // INFO and ERROR only
```

#### Custom severity threshold for throwing

```cpp
config::set_throw_on_error(true);
config::set_throw_threshold(SC_ERROR);
```

> ⚠️ Throwing from inside the handler is dangerous. Use statistics for production.

---

### 3. Output Formats

#### Text (default)

```cpp
config::set_log_as_json(false);
config::set_log_as_logfmt(false);
// Output: [INFO] TYPE: msg @ 0 s (file.cpp:10) in process: sc_main [extern]
```

#### JSON

```cpp
config::set_log_as_json(true);
// Output: {"severity":"INFO","msg_type":"TYPE",...}
```

#### Logfmt

```cpp
config::set_log_as_logfmt(true);
// Output: severity=info msg_type=TYPE message=msg ...
```

#### Comparison table

| Format | Readability | Parsable | Tools |
|--------|-------------|----------|-------|
| Text | ✅ Best | ❌ | tail, grep |
| JSON | 🟡 Medium | ✅ | jq, python |
| Logfmt | ✅ Good | ✅ | Loki, Grafana |

> JSON and Logfmt are mutually exclusive.

---

### 4. Filtering Reports

The library has three filter layers:

#### Layer 1: Severity filter

```cpp
config::set_min_severity(SC_WARNING);
// or, more precisely:
filters::set_enabled_severities({SC_INFO, SC_ERROR});
```

#### Layer 2: Message type filter

```cpp
filters::suppress_msg_type("VERBOSE");
filters::suppress_msg_type("TRACE");
filters::allow_only_msg_type("MAIN");
filters::unsuppress_msg_type("VERBOSE");

// full cleanup
filters::suppressed_msg_types.clear();
filters::allowed_msg_types.clear();
```

#### Layer 3: Regex filter

```cpp
filters::add_message_regex_deny("password=\\w+");
filters::add_message_regex_deny("secret_key=\\S+");
filters::add_message_regex_allow("^(READ|WRITE) ");
```

#### Combining filters

```cpp
config::set_min_severity(SC_WARNING);
filters::suppress_msg_type("VERBOSE");
filters::suppress_msg_type("TRACE");
filters::add_message_regex_deny("password=\\w+");

// Filtered out:
SC_REPORT_WARNING("VERBOSE", "...");                  // msg_type=VERBOSE
SC_REPORT_WARNING("TRACE", "...");                    // msg_type=TRACE
SC_REPORT_WARNING("MAIN", "User password=abc123");    // regex deny

// Logged:
SC_REPORT_WARNING("MAIN", "Memory write timeout");
SC_REPORT_ERROR("ALU", "Division by zero");
SC_REPORT_FATAL("MEM", "ECC error");
```

---

### 5. Sink Management

#### Adding a custom sink

```cpp
auto cb_sink = std::make_shared<sinks::CallbackSink>();
cb_sink->add([](const formatter::ReportInfo& info) {
    if (info.severity >= SC_ERROR) {
        send_alert_email(info.message);
    }
});
sinks::sink_manager().add(cb_sink);

handler::add_network_sink("192.168.1.100", 9090);
```

#### Removing a sink

```cpp
sinks::sink_manager().remove(cb_sink);
```

#### Removing all sinks

```cpp
sinks::sink_manager().clear();
```

#### Fully rebuilding the sinks

```cpp
sinks::sink_manager().clear();
sinks::sink_manager().add(std::make_shared<sinks::TerminalSink>());
```

#### Creating a custom Sink

```cpp
class DatabaseSink : public sinks::Sink {
    sql::Connection* conn;
public:
    DatabaseSink(sql::Connection* c) : conn(c) {}

    void write(const formatter::ReportInfo& info) override {
        std::string sql = "INSERT INTO logs (severity, msg) VALUES ('" +
            std::string(severity_to_str(info.severity)) + "', '" +
            info.message + "')";
        conn->execute(sql);
    }

    void flush() override { conn->commit(); }
};

auto db_sink = std::make_shared<DatabaseSink>(db_conn);
sinks::sink_manager().add(db_sink);
```

---

### 6. Colors and Themes

#### Automatic TTY detection

```bash
./my_sim                       # colors enabled (TTY)
./my_sim 2>stderr.log          # colors disabled (redirect)
./my_sim 2>&1 | grep ERROR     # colors disabled (pipe)
```

#### Choosing a theme

```cpp
colors::set_theme(colors::Theme::Dark);   // for dark backgrounds
colors::set_theme(colors::Theme::Light);  // for light backgrounds
```

#### Forcing colors on/off

```cpp
config::set_force_no_color(false);  // allow colors (TTY detection still applies)
config::set_force_no_color(true);   // force-disable colors
```

---

### 7. Statistics and Monitoring

#### Reading the statistics

```cpp
auto& s = stats::get();
std::cout << "INFO    : " << s.info.load()    << "\n";
std::cout << "WARNING : " << s.warning.load() << "\n";
std::cout << "ERROR   : " << s.error.load()   << "\n";
std::cout << "FATAL   : " << s.fatal.load()   << "\n";
std::cout << "TOTAL   : " << s.total.load()   << "\n";
```

#### Formatted printing

```cpp
stats::print();           // to stderr
stats::print(std::cout);  // to any stream
```

#### Resetting the statistics

```cpp
stats::reset();
```

#### Pattern: Checkpoint at the end of each phase

```cpp
SC_MODULE(Testbench)
{
    void run_phases() {
        stats::reset();
        SC_REPORT_INFO("PHASE", "=== RESET PHASE ===");
        reset_phase();
        stats::print();

        stats::reset();
        SC_REPORT_INFO("PHASE", "=== CONFIG PHASE ===");
        config_phase();
        stats::print();

        stats::reset();
        SC_REPORT_INFO("PHASE", "=== MAIN PHASE ===");
        main_phase();
        stats::print();
    }
};
```

#### Pattern: Alert threshold

```cpp
handler::add_callback([](const formatter::ReportInfo& info) {
    if (stats::get().error.load() >= 10) {
        SC_REPORT_FATAL("MONITOR", "Too many errors, aborting");
    }
});
```

---

### 8. Hierarchy and Process Info

#### Process info output format

```txt
in process: <name> [<type>]
```

#### Process types

| Type | Description | Example |
|------|-------------|---------|
| `[CTHREAD]` | SC_CTHREAD | `top.dut.cpu.proc [CTHREAD]` |
| `[THREAD]` | SC_THREAD | `top.dut.monitor [THREAD]` |
| `[METHOD]` | SC_METHOD | `top.dut.timer [METHOD]` |
| `[extern]` | Outside SystemC | `sc_main [extern]` or `thread:0x7f... [extern]` |
| `[unknown]` | Detection failed | `? [unknown]` |

#### Sample outputs

```txt
# inside SC_CTHREAD:
[INFO] MOD: msg @ 1 ns (...) in process: top.dut.proc [CTHREAD]

# inside SC_METHOD:
[INFO] MOD: msg @ 1 ns (...) in process: top.dut.handler [METHOD]

# inside sc_main:
[INFO] MOD: msg @ 0 s (...) in process: sc_main [extern]

# from std::thread:
[INFO] MOD: msg @ 3 ns (...) in process: thread:0x7f4a2b3c [extern]
```

#### Enabling/disabling hierarchy capture

```cpp
config::set_capture_hierarchy(true);   // output: in process: top.dut.proc [CTHREAD]
config::set_capture_hierarchy(false);  // output: in process: -
```

---

### 9. File Rotation

#### Enabling rotation

```cpp
config::set_max_file_size(10 * 1024 * 1024);  // 10 MB (default)
config::set_max_file_size(1 * 1024 * 1024);   // 1 MB
config::set_max_file_size(0);                 // disabled
```

#### Compressing rotated files

```cpp
config::set_compress_rotated(true);   // the .1 file is gzip-compressed → .1.gz
config::set_compress_rotated(false);  // the .1 file stays as plain text
```

#### How rotation works

When the file reaches max_file_size:

test.log   (full)  →  test.log.1   (rename)
                       test.log     (new file)


#### Rotation scenarios

```txt
# Default (compress=true):
test.log           # current file
test.log.1.gz      # compressed backup

# Without compression:
test.log           # current file
test.log.1         # uncompressed backup

# Rotation disabled (size=0):
test.log           # unbounded file
```

#### Limitation: only one backup

Only one `.1` file is kept. Older versions get overwritten. If you need multiple backups (e.g. `.1`, `.2`, `.3`), extend `maybe_rotate` in `sinks.hpp`.

---

### 10. Multi-threading

#### SystemC threads (safe)

```cpp
SC_MODULE(MultiThread)
{
    void proc1() {
        while (true) {
            SC_REPORT_INFO("P1", "from proc1");
            wait(1, SC_NS);
        }
    }

    void proc2() {
        while (true) {
            SC_REPORT_INFO("P2", "from proc2");
            wait(1, SC_NS);
        }
    }

    SC_CTOR(MultiThread) {
        SC_THREAD(proc1);
        SC_THREAD(proc2);
    }
};
```

#### std::thread (use with caution)

```cpp
auto worker = [](int id) {
    for (int i = 0; i < 100; ++i) {
        SC_REPORT_INFO("THREAD", ("Worker " + std::to_string(id)).c_str());
    }
};

std::vector<std::thread> threads;
for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
for (auto& t : threads) t.join();
```

> ⚠️ Calling SystemC APIs from std::threads may be unpredictable. Prefer SC_THREAD.

#### Thread safety table

| Component | Thread-safe? | Mechanism |
|-----------|--------------|-----------|
| `config::set_*` | ✅ | `cfg_mutex` |
| `filters::*` | ✅ | `filter_mutex` |
| `stats::bump` | ✅ | `std::atomic` |
| `sinks::write_all` | ✅ | `sinks_mutex` |
| `FileSink::write` | ✅ | `file_mutex` |
| `NetworkSink::write` | ✅ | `net_mutex` |
| `colors::set_theme` | ✅ | `std::atomic` |
| SystemC kernel API | ❌ | — |

---

### 11. Callbacks

#### Simple callback

```cpp
int error_count = 0;
handler::add_callback([&error_count](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR || info.severity == SC_FATAL) {
        error_count++;
    }
});
```

#### Callback for alerting

```cpp
handler::add_callback([](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR) {
        send_email("dev-team@company.com",
                   "Simulation Error",
                   info.message);
    }
});
```

#### Callback for metric collection

```cpp
std::map<std::string, int> error_types;
handler::add_callback([&error_types](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR) {
        error_types[info.msg_type]++;
    }
});

for (const auto& [type, count] : error_types) {
    std::cout << type << ": " << count << " errors\n";
}
```

#### Callback for rate limiting

```cpp
auto last_reset = std::chrono::steady_clock::now();
int error_count_in_window = 0;
const int MAX_PER_WINDOW = 100;

handler::add_callback([&](const formatter::ReportInfo& info) {
    if (info.severity != SC_ERROR) return;

    auto now = std::chrono::steady_clock::now();
    if (now - last_reset > std::chrono::minutes(1)) {
        last_reset = now;
        error_count_in_window = 0;
    }

    if (++error_count_in_window > MAX_PER_WINDOW) {
        SC_REPORT_WARNING("RATE", "Too many errors, dropping further");
    }
});
```

---

### 12. Network Logging

#### Setup

```cpp
handler::init_report_handler("logs/local.log");
handler::add_network_sink("127.0.0.1", 9090);
```

#### Network format

NetworkSink only sends JSON. Each report is one JSON line terminated by a newline:

```txt
{"severity":"INFO","msg_type":"MAIN","message":"...","time":"0 s","file":"...","line":10,"process":"..."}\n
```

#### Reconnect logic

NetworkSink fails silently if:
  - connection refused
  - connection dropped
  - timeout

On the next write, it attempts to reconnect.


#### Simple listener (Python)

```python
import socket, json

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind(("127.0.0.1", 9090))
sock.listen(1)
print("Listening on 127.0.0.1:9090")
conn, addr = sock.accept()
with conn:
    while True:
        data = conn.recv(4096)
        if not data: break
        for line in data.split(b"\n"):
            if line:
                msg = json.loads(line)
                print(f"[{msg['severity']}] {msg['msg_type']}: {msg['message']}")
```

```bash
python3 listener.py &
./my_sim
```

---

### 13. Regression Testing

#### Producing baseline and current logs

```cpp
// run_baseline.cpp
handler::init_report_handler("logs/baseline.log");
// ... run simulation ...
handler::close_report_log();

// run_current.cpp
handler::init_report_handler("logs/current.log");
// ... run simulation (with code changes) ...
handler::close_report_log();
```

#### Comparing

```cpp
auto result = diff::compare("logs/baseline.log", "logs/current.log");
if (result.identical) {
    std::cout << "✅ No regression\n";
} else {
    std::cout << "❌ Differences found:\n"
              << "  Added:   " << result.added   << "\n"
              << "  Removed: " << result.removed << "\n";
}
```

#### Normalization

```txt
Baseline:  [INFO] MAIN: msg @ 10 ns (file.cpp:1) in process: top
Current:   [INFO] MAIN: msg @ 100 ns (file.cpp:1) in process: top

→ After normalization (timestamp removed):
[INFO] MAIN: msg (file.cpp:1) in process: top
[INFO] MAIN: msg (file.cpp:1) in process: top

→ identical!
```


#### CI/CD integration

```bash
#!/bin/bash
./baseline_build/sim > /dev/null 2>&1
./current_build/sim > /dev/null 2>&1
./diff_tool logs/baseline.log logs/current.log
if [ $? -eq 0 ]; then
    echo "✅ PASS"; exit 0
else
    echo "❌ FAIL"; exit 1
fi
```

#### Limitations

- Line-by-line positional diff only
- If a line is inserted/removed in the middle, all subsequent lines are reported as changed
- For a smarter diff (LCS/Myers), replace the implementation

---

### 14. Configuration Recipes

#### Recipe 1: Production Logging

```cpp
config::set_log_file_path("/var/log/my_sim/run.log");
config::set_log_as_json(true);
config::set_min_severity(SC_WARNING);
config::set_max_file_size(100 * 1024 * 1024);  // 100 MB
config::set_compress_rotated(true);
config::set_force_no_color(true);
handler::init_report_handler();
```

#### Recipe 2: Debug Logging

```cpp
config::set_log_file_path("logs/debug.log");
config::set_min_severity(SC_INFO);
config::set_max_file_size(0);
config::set_force_no_color(false);
config::set_capture_hierarchy(true);
handler::init_report_handler();

handler::add_callback([](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR) {
        std::cout << "[DEBUG] Hit ERROR, attach debugger now\n";
        std::cin.get();
    }
});
```

#### Recipe 3: Testbench Logging

```cpp
config::set_log_file_path("logs/tb.log");
config::set_min_severity(SC_INFO);
config::set_capture_hierarchy(true);
filters::suppress_msg_type("VERBOSE");
filters::suppress_msg_type("TRACE");
filters::add_message_regex_deny("^clock_monitor:");
handler::init_report_handler();
```

#### Recipe 4: CI/CD Logging

```cpp
config::set_log_file_path("logs/ci.log");
config::set_log_as_json(true);
config::set_min_severity(SC_WARNING);
config::set_force_no_color(true);
handler::init_report_handler();
handler::add_network_sink("ci-monitor.internal", 9090);
```

#### Recipe 5: Silent Mode

```cpp
config::set_log_to_terminal(false);
config::set_log_to_file(true);
config::set_log_file_path("logs/silent.log");
handler::init_report_handler();
```

#### Recipe 6: Error-Only Alerting

```cpp
config::set_min_severity(SC_ERROR);
config::set_log_file_path("logs/errors.log");
handler::init_report_handler();

handler::add_callback([](const formatter::ReportInfo& info) {
    if (info.severity == SC_FATAL) {
        send_telegram("🚨 FATAL: " + info.message);
    }
});
```

#### Recipe 7: Environment-Driven Config

```cpp
if (const char* p = std::getenv("SC_REPORT_FILE")) {
    config::set_log_file_path(p);
}
if (const char* s = std::getenv("SC_REPORT_MIN_SEVERITY")) {
    std::string v(s);
    if (v == "INFO")          config::set_min_severity(SC_INFO);
    else if (v == "WARNING")  config::set_min_severity(SC_WARNING);
    else if (v == "ERROR")    config::set_min_severity(SC_ERROR);
    else if (v == "FATAL")    config::set_min_severity(SC_FATAL);
}
if (std::getenv("SC_REPORT_JSON"))      config::set_log_as_json(true);
if (std::getenv("SC_REPORT_NO_COLOR"))  config::set_force_no_color(true);

handler::init_report_handler();
```

```bash
SC_REPORT_FILE=logs/run.log \
SC_REPORT_MIN_SEVERITY=WARNING \
SC_REPORT_JSON=1 \
./my_sim
```

---

### 15. Best Practices

#### ✅ Check statistics at the end

```cpp
handler::init_report_handler();
// ... simulation ...
handler::close_report_log();

auto& s = stats::get();
if (s.error.load() > 0 || s.fatal.load() > 0) {
    std::cerr << "FAILED with " << s.error.load() << " errors\n";
    return 1;
}
return 0;
```

#### ❌ Don't use throw_on_error in production

```cpp
// ❌ BAD
config::set_throw_on_error(true);

// ✅ GOOD
config::set_throw_on_error(false);
if (stats::get().error.load() > 0) {
    throw std::runtime_error("simulation had errors");
}
```

#### ✅ Don't forget close_report_log()

```cpp
handler::init_report_handler();
// ...
handler::close_report_log();  // flush file
```

#### ✅ Try JSON for production

```cpp
config::set_log_as_json(true);
// jq 'select(.severity == "ERROR")' logs/run.log
```

#### ✅ Enable capture_hierarchy for testbenches

```cpp
config::set_capture_hierarchy(true);
// [ERROR] MEM: ... in process: top.dut.mem_ctrl [CTHREAD]
```

#### ✅ Use SC_THREAD in multi-threaded designs

```cpp
// ✅ GOOD: SystemC-managed
SC_THREAD(worker);

// 🟡 Risky: std::thread
std::thread t([]() { SC_REPORT_INFO("X", "msg"); });
```

#### ❌ Don't throw from inside a callback

```cpp
// ❌ BAD
handler::add_callback([](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR) {
        throw std::runtime_error("error");  // → terminate
    }
});

// ✅ GOOD
handler::add_callback([](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR) {
        error_flag = true;
    }
});
```

#### ✅ Set force_no_color = true in CI

```cpp
config::set_force_no_color(true);
```

---

### 16. Integration Patterns

#### Pattern 1: Wrapper Around SC_REPORT

```cpp
namespace my_proj {
    using namespace cc_vrwrapper::report;

    inline void log_info(const std::string& msg)  { VR_LOG_INFO("APP", msg.c_str()); }
    inline void log_warn(const std::string& msg)  { VR_LOG_WARNING("APP", msg.c_str()); }
    inline void log_error(const std::string& msg) { VR_LOG_ERROR("APP", msg.c_str()); }

    inline void init() {
        config::set_log_file_path("logs/app.log");
        config::set_min_severity(SC_INFO);
        handler::init_report_handler();
    }

    inline void cleanup() { handler::close_report_log(); }
}

// Usage:
my_proj::init();
my_proj::log_info("Started");
my_proj::cleanup();
```

#### Pattern 2: Module-level Logging

```cpp
SC_MODULE(ALU)
{
    void proc() {
        VR_LOG_INFO("ALU", "Compute started");
        if (div_by_zero) {
            VR_LOG_ERROR("ALU", "Division by zero");
        }
    }
    SC_CTOR(ALU) { SC_THREAD(proc); }
};
```

**Output:**
```txt
[INFO] ALU: Compute started @ 0 s (...) in process: top.alu.proc [THREAD]
[ERROR] ALU: Division by zero @ 10 ns (...) in process: top.alu.proc [THREAD]
```

#### Pattern 3: Phased Simulation

```cpp
void run_phases() {
    stats::reset();
    SC_REPORT_INFO("PHASE", "=== RESET PHASE ===");
    reset_phase();
    stats::print();

    stats::reset();
    SC_REPORT_INFO("PHASE", "=== CONFIG PHASE ===");
    config_phase();
    stats::print();

    stats::reset();
    SC_REPORT_INFO("PHASE", "=== MAIN PHASE ===");
    main_phase();
    stats::print();
}
```

#### Pattern 4: UVM-like Config DB (via callbacks)

```cpp
class ConfigDB {
    static inline std::map<std::string, std::string> db;
public:
    static void set(const std::string& k, const std::string& v) { db[k] = v; }
    static std::optional<std::string> get(const std::string& k) {
        auto it = db.find(k);
        return it != db.end() ? std::optional(it->second) : std::nullopt;
    }
};

handler::add_callback([](const formatter::ReportInfo& info) {
    if (info.msg_type == "CONFIG" && info.message.rfind("SET ", 0) == 0) {
        std::string rest = info.message.substr(4);
        size_t sp = rest.find(' ');
        if (sp != std::string::npos) {
            ConfigDB::set(rest.substr(0, sp), rest.substr(sp + 1));
        }
    }
});

SC_REPORT_INFO("CONFIG", "SET top.cpu.freq 1000");
```

#### Pattern 5: Coverage Collection (via callbacks)

```cpp
SC_MODULE(CoverageMonitor)
{
    std::set<std::string> covered_ops;

    void monitor() {
        handler::add_callback([this](const formatter::ReportInfo& info) {
            if (info.msg_type == "COV" && info.severity == SC_INFO) {
                covered_ops.insert(info.message);
            }
        });
    }

    void report() {
        std::cout << "Coverage: " << covered_ops.size() << " unique ops\n";
        for (const auto& op : covered_ops) {
            std::cout << "  - " << op << "\n";
        }
    }
};
```

#### Pattern 6: Multi-module Hierarchical Logging

```cpp
SC_MODULE(CPU) { SC_CTOR(CPU) { SC_THREAD(run); } void run() { VR_LOG_INFO("CPU", "running"); } };
SC_MODULE(Mem) { SC_CTOR(Mem) { SC_THREAD(run); } void run() { VR_LOG_INFO("MEM", "running"); } };
SC_MODULE(Bus) { SC_CTOR(Bus) { SC_THREAD(run); } void run() { VR_LOG_INFO("BUS", "running"); } };

SC_MODULE(Top) {
    CPU cpu; Mem mem; Bus bus;
    SC_CTOR(Top) : cpu("cpu"), mem("mem"), bus("bus") {}
};

int sc_main(int argc, char* argv[]) {
    handler::init_report_handler("logs/hier.log");
    Top top("top");
    sc_start(1, SC_NS);
    handler::close_report_log();
}
```

**Output:**
```txt
[INFO] CPU: running @ 0 s (...) in process: top.cpu.run [THREAD]
[INFO] MEM: running @ 0 s (...) in process: top.mem.run [THREAD]
[INFO] BUS: running @ 0 s (...) in process: top.bus.run [THREAD]
```

---

## Examples

### Example 1: Basic setup

```cpp
#include <systemc>
#include "sc-report.hpp"

int sc_main(int argc, char* argv[]) {
    using namespace cc_vrwrapper::report;

    config::set_log_file_path("logs/basic.log");
    handler::init_report_handler();

    SC_REPORT_INFO("APP", "Starting");
    SC_REPORT_WARNING("APP", "Low memory");
    SC_REPORT_ERROR("APP", "Failed to open file");

    stats::print();
    handler::close_report_log();
    return 0;
}
```

### Example 2: JSON logging with filtering

```cpp
config::set_log_as_json(true);
config::set_log_file_path("logs/structured.json");
config::set_min_severity(SC_WARNING);
filters::add_message_regex_deny("password=\\w+");
handler::init_report_handler();

SC_REPORT_INFO("DEBUG", "This will be filtered (INFO < WARNING)");
SC_REPORT_WARNING("AUTH", "Login attempt with password=secret123");  // filtered by regex
SC_REPORT_ERROR("NET", "Connection timeout");  // appears in log
```

### Example 3: Callback sink

```cpp
handler::init_report_handler();

int error_count = 0;
handler::add_callback([&error_count](const formatter::ReportInfo& info) {
    if (info.severity == SC_ERROR || info.severity == SC_FATAL) {
        error_count++;
    }
});

SC_REPORT_ERROR("X", "err 1");
SC_REPORT_ERROR("X", "err 2");
SC_REPORT_INFO("X", "info");

handler::close_report_log();
std::cout << "Total errors: " << error_count << "\n";  // 2
```

### Example 4: Hierarchy capture

```cpp
SC_MODULE(MyModule) {
    sc_in<bool> clk;
    SC_CTOR(MyModule) { SC_CTHREAD(proc, clk.pos()); }
    void proc() {
        while (true) {
            SC_REPORT_INFO("MOD", "Hello from inside module");
            wait();
        }
    }
};

int sc_main(int argc, char* argv[]) {
    handler::init_report_handler("logs/hier.log");
    sc_clock clk("clk", 1, SC_NS);
    MyModule mod("my_mod");
    mod.clk(clk);
    sc_start(3, SC_NS);
    handler::close_report_log();
}
```


**Output:**
```txt
[INFO] MOD: Hello from inside module @ 0 s (...) in process: my_mod.proc [CTHREAD]
[INFO] MOD: Hello from inside module @ 1 ns (...) in process: my_mod.proc [CTHREAD]
[INFO] MOD: Hello from inside module @ 2 ns (...) in process: my_mod.proc [CTHREAD]

```

### Example 5: File rotation

```cpp
config::set_max_file_size(500);
config::set_compress_rotated(false);
config::set_log_file_path("logs/rotating.log");
handler::init_report_handler();

for (int i = 0; i < 20; ++i) {
    SC_REPORT_INFO("ROT", "This message is about 80 bytes long, will trigger rotation");
}

handler::close_report_log();
// Files:
//   logs/rotating.log     ← current file
//   logs/rotating.log.1   ← rotated backup
```

### Example 6: Multi-thread

```cpp
handler::init_report_handler("logs/threads.log");

auto worker = [](int id) {
    for (int i = 0; i < 50; ++i) {
        SC_REPORT_INFO("THREAD", ("Worker " + std::to_string(id) +
                                   " msg " + std::to_string(i)).c_str());
    }
};

std::vector<std::thread> threads;
for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
for (auto& t : threads) t.join();

handler::close_report_log();
// Stats: 200 INFO reports (4 threads × 50)
```

---

## C++17 / C++20 Support

The library supports both C++17 and C++20 in the same files.

| Feature | C++17 | C++20 |
|---------|-------|-------|
| Fold expressions | ✅ | ✅ |
| `if constexpr` | ✅ | ✅ |
| `<filesystem>` | ✅ (may require `-lstdc++fs` on GCC < 9) | ✅ |
| Concepts | ❌ | ✅ |
| `<source_location>` | ❌ (falls back to `__FILE__`/`__LINE__`) | ✅ |

---

## Thread Safety

The library is designed for use in multi-threaded environments:

| Component | Mechanism |
|-----------|-----------|
| `config::cfg` | `std::mutex` via `cfg_mutex` |
| `filters::*` | `std::mutex` via `filter_mutex` |
| `sinks::SinkManager` | `std::mutex` via `sinks_mutex` |
| `FileSink::file` | `std::mutex` via `file_mutex` |
| `CallbackSink::callbacks` | `std::mutex` via `cb_mutex` |
| `NetworkSink` | `std::mutex` via `net_mutex` |
| `stats::g_stats` | `std::atomic<uint64_t>` per counter |
| `colors::current_theme` | `std::atomic<Theme>` |

> ⚠️ Warning: `throw_on_error` can be dangerous. If SystemC calls the
> handler from a `noexcept` context, the exception will cause
> `std::terminate`. For production, it is recommended to store a flag
> instead of throwing directly, and check it after `sc_start`.

---

## Limitations

1. **File rotation** keeps only one backup (`.1`). To keep multiple versions, extend `maybe_rotate` in `sinks.hpp`.

2. **Log diff** is a simple line-by-line algorithm. For a smarter diff (LCS/Myers), replace `diff::compare`.

3. **NetworkSink** has no buffering. If the network is slow, writes will block. For production, add an async queue.

4. **throw_on_error** from inside the handler is dangerous.

5. **`sc_get_current_process_b()`** is an unofficial SystemC API. If it is unavailable in your SystemC version, the process type is shown as `[PROC]` instead of `[CTHREAD]/[THREAD]/[METHOD]`.

---

## Troubleshooting

### Problem: No output in the terminal

**Likely cause:** `min_severity` is set too high.

```cpp
config::set_min_severity(SC_INFO);  // make sure this is SC_INFO
```

### Problem: Duplicate output in the terminal

**Cause:** SystemC default actions are still active.

```cpp
sc_report_handler::set_actions(SC_INFO,    SC_DO_NOTHING);
sc_report_handler::set_actions(SC_WARNING, SC_DO_NOTHING);
sc_report_handler::set_actions(SC_ERROR,   SC_DO_NOTHING);
```

### Problem: Log file is not created

**Likely causes:**
- Parent directory does not exist → `utils::ensure_parent_dirs` handles this automatically
- No write permission
- Wrong path

### Problem: Colors are not visible in the terminal

**Likely causes:**
- `force_no_color` is on: `config::set_force_no_color(false);`
- stderr is not a TTY (e.g. redirected to a file)
- Wrong theme: `colors::set_theme(colors::Theme::Dark);`

### Problem: `std::terminate` with `throw_on_error`

This behavior is expected if the exception is thrown from a `noexcept` context. Solution:

```cpp
config::set_throw_on_error(false);
auto& s = stats::get();
if (s.error.load() > 0 || s.fatal.load() > 0) {
    // handle error
}
```

### Problem: Compile error for `sc_get_current_process_b`

If your SystemC lacks this API:

```cpp
// in utils.hpp:
const char* kind_str = "[PROC]";
try {
    auto h = sc_core::sc_get_current_process_handle();
    if (h.valid()) {
        kind_str = "[PROC]";
    }
} catch (...) {}
```

### Problem: Link error for `<filesystem>` on GCC < 9

```bash
g++ -std=c++17 -I./sc-report your_code.cpp -lsystemc -lpthread -lstdc++fs -o your_app
```

---

## Changelog

### v1.0.0

- Initial release
- C++17 and C++20 support
- Sinks: Terminal, File (with rotation), Callback, Network
- Formats: Text, JSON, Logfmt
- Filters: severity, msg_type, regex
- Themes: Dark, Light
- Process type detection (CTHREAD/THREAD/METHOD)
- Atomic statistics
- Log diff
- Pluggable sink architecture
- Auto directory creation
- Gzip compression for rotated files
- Reconnect logic for NetworkSink

---
