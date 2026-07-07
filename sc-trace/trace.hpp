#ifndef CC_VRWRAPPER_TRACE_TRACE_HPP
#define CC_VRWRAPPER_TRACE_TRACE_HPP

// ============================================================================
// cc_vrwrapper :: sc-trace :: trace.hpp
// Enhanced VCD tracing with regex-based signal filtering and grouping.
//
// Wraps SystemC's built-in sc_trace_file with:
//   - Regex include/exclude filters for signal names
//   - Signal grouping (prefixes signal names with group name)
//   - Automatic cleanup via RAII
// ============================================================================

#include "utils.hpp"
#include <systemc>
#include <memory>
#include <string>

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// TraceManager — enhanced VCD tracing
// ========================================================================

class TraceManager {
public:
    // Non-copyable, non-movable — owns a raw sc_trace_file* handle
    TraceManager(const TraceManager&) = delete;
    TraceManager& operator=(const TraceManager&) = delete;
    TraceManager(TraceManager&&) = delete;
    TraceManager& operator=(TraceManager&&) = delete;

    explicit TraceManager(const std::string& filename)
        : tf_(sc_core::sc_create_vcd_trace_file(filename.c_str()))
        , filename_(filename)
        , closed_(false)
    {
        if (!tf_) {
            std::cerr << "[TraceManager] Failed to create VCD file: "
                      << filename << "\n";
        }
    }

    ~TraceManager()
    {
        if (!closed_ && tf_) {
            close();
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

    void clear_filters()
    {
        filter_.clear();
    }

    // ----------------------------------------------------------------
    // trace() — add a signal to the VCD file
    //
    // The signal is only added if it passes the include/exclude filters.
    //
    // Example:
    //   tm.trace(clk, "clk");
    //   tm.trace(data, "data");
    // ----------------------------------------------------------------
    template <typename Signal>
    void trace(Signal& sig, const std::string& name)
    {
        static_assert(is_sc_signal_or_port_v<Signal>,
            "TraceManager::trace(): Signal must be sc_signal<T>, "
            "sc_in<T>, or sc_out<T>");

        if (!filter_.matches(name)) return;
        if (!tf_) return;

        sc_core::sc_trace(tf_, sig, name);
    }

    // ----------------------------------------------------------------
    // trace_with_group() — add signal with a group prefix
    //
    // The signal name in VCD will be "group.name".
    // Useful for organizing signals hierarchically in waveform viewers.
    //
    // Example:
    //   tm.trace_with_group(clk, "clk", "clock_domain");
    //   // VCD name: clock_domain.clk
    // ----------------------------------------------------------------
    template <typename Signal>
    void trace_with_group(Signal& sig, const std::string& name,
                          const std::string& group)
    {
        static_assert(is_sc_signal_or_port_v<Signal>,
            "TraceManager::trace_with_group(): Signal must be sc_signal<T>, "
            "sc_in<T>, or sc_out<T>");

        std::string full_name = group + "." + name;
        if (!filter_.matches(full_name)) return;
        if (!tf_) return;

        sc_core::sc_trace(tf_, sig, full_name);
    }

    // ----------------------------------------------------------------
    // set_time_unit() — set the VCD time resolution
    // ----------------------------------------------------------------
    void set_time_unit(double v, sc_core::sc_time_unit unit)
    {
        if (tf_) {
            tf_->set_time_unit(v, unit);
        }
    }

    // ----------------------------------------------------------------
    // close() — close the VCD file (called automatically by destructor)
    // ----------------------------------------------------------------
    void close()
    {
        if (tf_ && !closed_) {
            sc_core::sc_close_vcd_trace_file(tf_);
            closed_ = true;
            tf_ = nullptr;
        }
    }

    // ----------------------------------------------------------------
    // Accessors
    // ----------------------------------------------------------------
    sc_core::sc_trace_file* raw() const { return tf_; }
    const std::string& filename() const { return filename_; }

    // Allow implicit conversion to sc_trace_file* for SystemC APIs
    operator sc_core::sc_trace_file*() const { return tf_; }

private:
    sc_core::sc_trace_file* tf_;
    NameFilter filter_;
    std::string filename_;
    bool closed_;
};

// ========================================================================
// Convenience function
// ========================================================================

inline std::shared_ptr<TraceManager> create_trace_file(const std::string& filename)
{
    return std::make_shared<TraceManager>(filename);
}

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_TRACE_HPP