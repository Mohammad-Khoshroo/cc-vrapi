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
// JSON format:
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
// Time is in nanoseconds (double precision).
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
#include <mutex>
#include <memory>

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// JsonTrace — records signal changes and outputs as JSON
// ========================================================================

class JsonTrace {
public:
    struct SignalChange {
        double time;        // in nanoseconds
        std::string value;
    };

    struct SignalInfo {
        std::string name;
        int width;
        std::vector<SignalChange> changes;
    };

    explicit JsonTrace(const std::string& filename)
        : filename_(filename)
        , started_(false)
        , written_(false)
    {}

    ~JsonTrace()
    {
        if (started_ && !written_) {
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
    // trace() — register a signal for JSON tracing
    //
    // MUST be called BEFORE sc_start().
    // Records the initial value at time 0, then watches for changes.
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

        // Record initial value at time 0
        {
            std::lock_guard<std::mutex> g(mutex_);
            signals_[idx].changes.push_back({
                0.0,
                value_to_string<value_type>(sig.read())
            });
        }

        // Watch for changes — the watcher records each change
        auto watcher = std::make_shared<SignalWatcher<Signal>>(
            ("vrjson_" + std::to_string(idx)).c_str(),
            sig,
            [this, idx](const value_type& val) {
                std::lock_guard<std::mutex> g(mutex_);
                double t = sc_core::sc_time_stamp().to_seconds() * 1e9;
                signals_[idx].changes.push_back({
                    t,
                    value_to_string<value_type>(val)
                });
            },
            WatchMode::ANY_CHANGE
        );
        watchers_.push_back(watcher);
    }

    // ----------------------------------------------------------------
    // start() — mark that simulation is about to begin
    //
    // Optional: if not called, write() will be called by destructor.
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

        f << "{\n";
        f << "  \"signals\": [\n";

        for (size_t i = 0; i < signals_.size(); ++i) {
            const auto& sig = signals_[i];
            f << "    {\n";
            f << "      \"name\": \"" << sig.name << "\",\n";
            f << "      \"width\": " << sig.width << ",\n";
            f << "      \"changes\": [\n";

            for (size_t j = 0; j < sig.changes.size(); ++j) {
                const auto& ch = sig.changes[j];
                f << "        {\"time\": " << ch.time
                  << ", \"value\": \"" << ch.value << "\"}";
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
    std::string filename_;
    std::vector<SignalInfo> signals_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<sc_core::sc_module>> watchers_;
    NameFilter filter_;
    bool started_;
    bool written_;
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