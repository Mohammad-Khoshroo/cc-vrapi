#ifndef CC_VRWRAPPER_TRACE_JSON_TRACE_HPP
#define CC_VRWRAPPER_TRACE_JSON_TRACE_HPP

// ============================================================================
// cc_vrwrapper :: sc-trace :: json_trace.hpp
// JSON waveform output for browser-based waveform viewing.
//
// Records all signal value changes during simulation and outputs them
// as a JSON file. This format can be loaded by custom web-based waveform
// viewers or analyzed with tools like jq, Python, etc.
//
// Delta cycle recording modes (set via set_delta_mode / set_record_delta_cycles
// / set_delta_trigger — MUST be configured BEFORE trace()):
//
//   OFF (default)
//       No delta info. Minimal overhead.
//       JSON: {"time": <ns>, "value": "<str>"}
//
//   GLOBAL
//       Record sc_delta_count() as-is (the global counter).
//       JSON: {"time": <ns>, "delta": <n>, "value": "<str>"}
//
//   TRIGGERED
//       Record cycle + local delta relative to a trigger signal's
//       edge (e.g., clk rising edge). Useful for debugging
//       within-clock-cycle behavior — multiple changes at the same
//       simulation time but different delta cycles become visible
//       as a per-cycle delta sequence.
//
//       Each rising edge of the trigger signal marks the start of a
//       new "cycle". Changes before the first trigger are in cycle 0.
//
//       JSON output:
//         {
//           "triggers": [
//             {"cycle": 1, "time": <ns>, "delta": <global_delta>},
//             {"cycle": 2, "time": <ns>, "delta": <global_delta>},
//             ...
//           ],
//           "signals": [
//             {
//               "name": "...", "width": N,
//               "changes": [
//                 {"time": <ns>, "cycle": <n>, "delta": <local>, "value": "<str>"}
//               ]
//             }
//           ]
//         }
//
// IMPORTANT: trace() calls MUST be made BEFORE sc_start().
//            Delta mode MUST be set BEFORE trace().
//            write() should be called AFTER sc_start() completes.
// ============================================================================

#include "utils.hpp"
#include "watch.hpp"
#include <systemc>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <iomanip>
#include <cstdio>
#include <utility>

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// JsonTrace — records signal changes and outputs as JSON
// ========================================================================

class JsonTrace {
public:
    // ----------------------------------------------------------------
    // DeltaMode — controls how delta cycle information is recorded
    // ----------------------------------------------------------------
    enum class DeltaMode {
        OFF,         // no delta info (minimal overhead)
        GLOBAL,      // record sc_delta_count() as-is
        TRIGGERED    // record cycle + local delta relative to a trigger
    };

    struct SignalChange {
        double        time;     // in nanoseconds
        std::string   value;
        unsigned long delta;    // global sc_delta_count() at change;
                                // only meaningful when delta_mode_ != OFF
    };

    struct SignalInfo {
        std::string name;
        int width;
        std::vector<SignalChange> changes;
    };

    struct TriggerEvent {
        double        time;     // in nanoseconds
        unsigned long delta;    // global sc_delta_count() at trigger
    };

    // Non-copyable, non-movable — owns watcher modules and recorded data
    JsonTrace(const JsonTrace&) = delete;
    JsonTrace& operator=(const JsonTrace&) = delete;
    JsonTrace(JsonTrace&&) = delete;
    JsonTrace& operator=(JsonTrace&&) = delete;

    explicit JsonTrace(const std::string& filename)
        : filename_(filename)
        , started_(false)
        , written_(false)
        , delta_mode_(DeltaMode::OFF)
        , instance_id_(next_instance_id())
    {}

    ~JsonTrace()
    {
        // Always write if not already written, so the user does not
        // lose data even if they forget to call write() explicitly.
        if (!written_) {
            write();
        }
    }

    // ----------------------------------------------------------------
    // Filter methods — call BEFORE trace()
    // ----------------------------------------------------------------

    void set_include_filter(const std::string& pattern)
    {
        filter_.add_include(pattern);
    }

    void set_exclude_filter(const std::string& pattern)
    {
        filter_.add_exclude(pattern);
    }

    // ----------------------------------------------------------------
    // Delta cycle configuration — MUST be called BEFORE trace()
    // ----------------------------------------------------------------

    // Backward-compatible: enable=true → GLOBAL, enable=false → OFF
    void set_record_delta_cycles(bool enable)
    {
        delta_mode_ = enable ? DeltaMode::GLOBAL : DeltaMode::OFF;
    }

    // Explicit mode setting
    void set_delta_mode(DeltaMode mode)
    {
        delta_mode_ = mode;
    }

    DeltaMode delta_mode() const { return delta_mode_; }

    // ----------------------------------------------------------------
    // set_delta_trigger() — set the reference signal for TRIGGERED mode
    //
    // Each rising edge of sig marks the start of a new "cycle".
    // In TRIGGERED mode, each recorded change is tagged with:
    //   - cycle: which trigger interval it belongs to
    //            (0 = before first trigger, 1 = after first trigger, ...)
    //   - delta: local delta count relative to the start of the cycle
    //
    // MUST be called BEFORE trace(). Auto-enables TRIGGERED mode
    // if delta_mode_ is currently OFF.
    //
    // The trigger signal itself may also be traced via trace() — the
    // two watchers are independent and do not interfere.
    //
    // Example:
    //   JsonTrace jt("wave.json");
    //   jt.set_delta_trigger(clk);   // clk rising edge = cycle start
    //   jt.trace(clk, "clk");
    //   jt.trace(data, "data");
    // ----------------------------------------------------------------
    template <typename Signal>
    void set_delta_trigger(Signal& sig)
    {
        static_assert(is_sc_signal_or_port_v<Signal>,
            "set_delta_trigger(): Signal must be sc_signal<T>, "
            "sc_in<T>, or sc_out<T>");

        using value_type = signal_value_type_t<Signal>;
        static_assert(supports_edge_detection_v<value_type>,
            "set_delta_trigger(): value type must support edge detection "
            "(bool, sc_logic, integral, sc_int, sc_uint, sc_bigint, "
            "sc_biguint). For bus types, use GLOBAL mode instead.");

        std::string name = "vrjson_trig_"
            + std::to_string(instance_id_);
        auto watcher = std::make_shared<SignalWatcher<Signal>>(
            name.c_str(),
            sig,
            [this](const value_type&) {
                TriggerEvent ev;
                ev.time  = sc_core::sc_time_stamp().to_seconds() * 1e9;
                ev.delta = sc_core::sc_delta_count();
                triggers_.push_back(ev);
            },
            WatchMode::RISING_EDGE
        );
        watchers_.push_back(watcher);

        // Auto-enable TRIGGERED mode if currently OFF, so the user
        // doesn't need to call set_delta_mode() separately.
        if (delta_mode_ == DeltaMode::OFF) {
            delta_mode_ = DeltaMode::TRIGGERED;
        }
    }

    // ----------------------------------------------------------------
    // trace() — register a signal for JSON tracing
    //
    // MUST be called BEFORE sc_start().
    // Records the initial value at time 0, then watches for changes.
    //
    // Note: For sc_in<T>/sc_out<T>, the port MUST be bound to a
    // signal before calling trace(), otherwise sig.read() will fail.
    // ----------------------------------------------------------------
    template <typename Signal>
    void trace(Signal& sig, const std::string& name)
    {
        static_assert(is_sc_signal_or_port_v<Signal>,
            "JsonTrace::trace(): Signal must be sc_signal<T>, "
            "sc_in<T>, or sc_out<T>");

        // Check filter
        if (!filter_.matches(name)) return;

        using value_type = signal_value_type_t<Signal>;

        int idx = static_cast<int>(signals_.size());
        int w = signal_width_v<value_type>;
        signals_.push_back({name, w, {}});

        // Pre-reserve to reduce reallocations during simulation.
        signals_[idx].changes.reserve(64);

        // Capture the delta-recording decision at trace() time so
        // the watcher closure uses a stable branch.
        const bool record_delta = (delta_mode_ != DeltaMode::OFF);

        // Record initial value at time 0 (delta = 0).
        signals_[idx].changes.push_back({
            0.0,
            value_to_string<value_type>(sig.read()),
            0UL
        });

        // Watch for changes — the watcher records each change.
        // SystemC is single-threaded — no mutex needed.
        std::string watcher_name = "vrjson_"
            + std::to_string(instance_id_) + "_"
            + std::to_string(idx);
        auto watcher = std::make_shared<SignalWatcher<Signal>>(
            watcher_name.c_str(),
            sig,
            [this, idx, record_delta](const value_type& val) {
                double t = sc_core::sc_time_stamp().to_seconds() * 1e9;
                unsigned long d = record_delta
                    ? sc_core::sc_delta_count()
                    : 0UL;
                signals_[idx].changes.push_back({
                    t,
                    value_to_string<value_type>(val),
                    d
                });
            },
            WatchMode::ANY_CHANGE
        );
        watchers_.push_back(watcher);
    }

    // ----------------------------------------------------------------
    // start() — mark that simulation is about to begin
    //
    // Optional: kept for backward compatibility. The destructor now
    // always calls write() if it has not been called yet.
    // ----------------------------------------------------------------
    void start()
    {
        started_ = true;
    }

    // ----------------------------------------------------------------
    // write() — write the JSON file
    //
    // Call AFTER sc_start() completes.
    // In TRIGGERED mode, post-processing computes per-change
    // (cycle, local_delta) from the recorded global delta counts.
    // ----------------------------------------------------------------
    void write()
    {
        if (written_) return;

        std::ofstream f(filename_);
        if (!f.is_open()) {
            std::cerr << "[JsonTrace] Cannot open " << filename_
                      << " for writing\n";
            return;
        }

        // Use high precision for time values.
        f << std::setprecision(15);

        f << "{\n";

        // In TRIGGERED mode, output the triggers array first so
        // consumers can map cycle numbers to absolute times.
        if (delta_mode_ == DeltaMode::TRIGGERED && !triggers_.empty()) {
            f << "  \"triggers\": [\n";
            for (size_t i = 0; i < triggers_.size(); ++i) {
                const auto& tr = triggers_[i];
                f << "    {\"cycle\": " << (i + 1)
                  << ", \"time\": "  << tr.time
                  << ", \"delta\": " << tr.delta << "}";
                if (i + 1 < triggers_.size()) f << ",";
                f << "\n";
            }
            f << "  ],\n";
        }

        f << "  \"signals\": [\n";

        for (size_t i = 0; i < signals_.size(); ++i) {
            const auto& sig = signals_[i];
            f << "    {\n";
            f << "      \"name\": \"" << json_escape(sig.name) << "\",\n";
            f << "      \"width\": " << sig.width << ",\n";
            f << "      \"changes\": [\n";

            for (size_t j = 0; j < sig.changes.size(); ++j) {
                const auto& ch = sig.changes[j];
                f << "        {\"time\": " << ch.time;

                if (delta_mode_ == DeltaMode::GLOBAL) {
                    f << ", \"delta\": " << ch.delta;
                } else if (delta_mode_ == DeltaMode::TRIGGERED) {
                    // Post-process: compute (cycle, local_delta) from
                    // the recorded global delta count.
                    auto cd = compute_cycle_delta(ch.delta);
                    f << ", \"cycle\": " << cd.first
                      << ", \"delta\": " << cd.second;
                }

                f << ", \"value\": \"" << json_escape(ch.value) << "\"}";
                if (j + 1 < sig.changes.size()) f << ",";
                f << "\n";
            }

            f << "      ]\n";
            f << "    }";
            if (i + 1 < signals_.size()) f << ",";
            f << "\n";
        }

        f << "  ]\n";
        f << "}\n";
        f.close();

        written_ = true;
    }

    // ----------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------
    const std::string& filename() const { return filename_; }
    size_t signal_count() const { return signals_.size(); }
    size_t trigger_count() const { return triggers_.size(); }

private:
    // Generate a unique ID for each JsonTrace instance so that
    // watcher module names never collide across instances.
    static int next_instance_id()
    {
        static std::atomic<int> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }

    // ----------------------------------------------------------------
    // compute_cycle_delta() — post-process a change's global delta
    // into (cycle, local_delta) using the recorded trigger events.
    //
    // Returns:
    //   - cycle = 0 if no trigger has fired yet (initialization phase)
    //   - cycle = i+1 if triggers_[i] is the latest trigger whose
    //     global delta is <= change_delta
    //   - local_delta = change_delta - trigger_delta (>= 0)
    //
    // Linear scan from the end — triggers_ is typically small
    // (one entry per clock period), so this is effectively O(1)
    // for the common case of changes near the latest trigger.
    // ----------------------------------------------------------------
    std::pair<int, unsigned long>
    compute_cycle_delta(unsigned long change_delta) const
    {
        for (size_t i = triggers_.size(); i > 0; --i) {
            if (triggers_[i - 1].delta <= change_delta) {
                return { static_cast<int>(i),
                         change_delta - triggers_[i - 1].delta };
            }
        }
        // No trigger has fired yet — initialization phase.
        return { 0, change_delta };
    }

    // Minimal JSON string escaping for names and values.
    static std::string json_escape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 2);
        for (char c : s) {
            switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                            static_cast<unsigned int>(
                                static_cast<unsigned char>(c)));
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    }

    std::string filename_;
    std::vector<SignalInfo>  signals_;
    std::vector<TriggerEvent> triggers_;
    std::vector<std::shared_ptr<sc_core::sc_module>> watchers_;
    NameFilter filter_;
    bool started_;
    bool written_;
    DeltaMode delta_mode_;
    int  instance_id_;
};

// ========================================================================
// Convenience function
// ========================================================================

inline std::shared_ptr<JsonTrace> create_json_trace(const std::string& filename)
{
    return std::make_shared<JsonTrace>(filename);
}

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_JSON_TRACE_HPP