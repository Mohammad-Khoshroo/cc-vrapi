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
// JSON format (delta_cycles OFF — default):
//   {
//     "signals": [
//       {
//         "name": "clk",
//         "width": 1,
//         "changes": [
//           {"time": 0.0, "value": "0"},
//           {"time": 5.0, "value": "1"},
//           {"time": 10.0, "value": "0"}
//         ]
//       }
//     ]
//   }
//
// JSON format (delta_cycles ON — see set_record_delta_cycles):
//   {
//     "signals": [
//       {
//         "name": "data",
//         "width": 8,
//         "changes": [
//           {"time": 0.0, "delta": 0, "value": "0x00"},
//           {"time": 10.0, "delta": 5, "value": "0x01"},
//           {"time": 10.0, "delta": 6, "value": "0x02"}
//         ]
//       }
//     ]
//   }
//
// Time is in nanoseconds (double precision).
// delta is the result of sc_core::sc_delta_count() at the moment of change.
// Multiple changes at the same time but different delta values indicate
// zero-time glitches — useful for debugging combinatorial loops and
// settling behavior in SystemC designs.
//
// IMPORTANT: trace() calls MUST be made BEFORE sc_start().
// write() should be called AFTER sc_start() completes.
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

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// JsonTrace — records signal changes and outputs as JSON
// ========================================================================

class JsonTrace {
public:
    struct SignalChange {
        double       time;        // in nanoseconds
        std::string  value;
        unsigned long delta;      // sc_delta_count() — only meaningful
                                  // when record_delta_cycles_ == true
    };

    struct SignalInfo {
        std::string name;
        int width;
        std::vector<SignalChange> changes;
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
        , record_delta_cycles_(false)
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
    // set_record_delta_cycles() — enable delta cycle visibility
    //
    // When enabled, each recorded change also stores sc_delta_count().
    // This allows distinguishing multiple changes that occur at the
    // SAME simulation time but in DIFFERENT delta cycles — a common
    // source of subtle bugs in SystemC designs (zero-time glitches,
    // combinatorial settling, etc.).
    //
    // MUST be called BEFORE trace() — the flag is captured into the
    // watcher callback closures at trace() time.
    //
    // Overhead when enabled (accepted by the user):
    //   - One sc_delta_count() call per change (~1-2 ns)
    //   - 8 bytes extra storage per change
    //   - Slightly larger JSON output
    //
    // Example:
    //   JsonTrace jt("wave.json");
    //   jt.set_record_delta_cycles(true);
    //   jt.trace(clk, "clk");
    //   jt.trace(data, "data");
    //   sc_start(100, SC_NS);
    //   jt.write();
    // ----------------------------------------------------------------
    void set_record_delta_cycles(bool enable)
    {
        record_delta_cycles_ = enable;
    }

    bool record_delta_cycles() const { return record_delta_cycles_; }

    // ----------------------------------------------------------------
    // trace() — register a signal for JSON tracing
    //
    // MUST be called BEFORE sc_start().
    // Records the initial value at time 0, then watches for changes.
    //
    // Note: For sc_in<T>/sc_out<T>, the port MUST be bound to a
    // signal before calling trace(), otherwise sig.read() will fail.
    //
    // Example:
    //   JsonTrace jt("wave.json");
    //   jt.trace(clk, "clk");
    //   jt.trace(data, "data");
    //   jt.start();         // mark ready for simulation
    //   sc_start(100, SC_NS);
    //   jt.write();         // output JSON file
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

        // Capture the delta-cycle flag at trace() time so the
        // watcher closure uses a stable decision.
        const bool record_delta = record_delta_cycles_;

        // Record initial value at time 0 (delta = 0).
        signals_[idx].changes.push_back({
            0.0,
            value_to_string<value_type>(sig.read()),
            0UL
        });

        // Watch for changes — the watcher records each change.
        // Use a globally-unique name (instance_id + idx) to avoid
        // collisions when multiple JsonTrace instances exist.
        //
        // NOTE: SystemC simulation is single-threaded — no mutex is
        // needed. The watcher callback runs synchronously inside
        // sc_start() on the same thread that called trace().
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

        // Use high precision for time values to avoid losing
        // sub-nanosecond resolution in long simulations.
        f << std::setprecision(15);

        f << "{\n";
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
                if (record_delta_cycles_) {
                    f << ", \"delta\": " << ch.delta;
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

private:
    // Generate a unique ID for each JsonTrace instance so that
    // watcher module names never collide across instances.
    static int next_instance_id()
    {
        static std::atomic<int> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
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
    std::vector<SignalInfo> signals_;
    std::vector<std::shared_ptr<sc_core::sc_module>> watchers_;
    NameFilter filter_;
    bool started_;
    bool written_;
    bool record_delta_cycles_;
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