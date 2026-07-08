#ifndef CC_VRWRAPPER_TRACE_TRACE_HPP
#define CC_VRWRAPPER_TRACE_TRACE_HPP

// ============================================================================
// cc_vrwrapper :: sc-trace :: trace.hpp
// Enhanced VCD tracing with:
//   - Regex include/exclude filters for signal names
//   - Signal grouping / hierarchical naming (dot-separated scopes)
//   - trace_all() — auto-trace all signals of a module (optionally recursive)
//   - trace_top_level_signals() — auto-trace signals declared in sc_main
//   - gzip compression of output file (optional)
//   - Automatic cleanup via RAII
// ============================================================================

#include "utils.hpp"
#include <systemc>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <zlib.h>

namespace cc_vrwrapper {
namespace trace {

// ========================================================================
// Internal helper — gzip a file in-place (original is removed)
// ========================================================================

namespace detail {

inline bool gzip_file(const std::string& filename)
{
    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) return false;

    std::string gz_name = filename + ".gz";
    gzFile out = gzopen(gz_name.c_str(), "wb");
    if (!out) {
        in.close();
        return false;
    }

    gzbuffer(out, 1 << 20);  // 1 MB buffer

    char buf[8192];
    while (in) {
        in.read(buf, sizeof(buf));
        std::streamsize n = in.gcount();
        if (n > 0) {
            gzwrite(out, buf, static_cast<unsigned>(n));
        }
    }

    in.close();
    gzclose(out);

    std::remove(filename.c_str());
    return true;
}

} // namespace detail

// ========================================================================
// TraceManager — enhanced VCD tracing
// ========================================================================

class TraceManager {
public:
    TraceManager(const TraceManager&) = delete;
    TraceManager& operator=(const TraceManager&) = delete;
    TraceManager(TraceManager&&) = delete;
    TraceManager& operator=(TraceManager&&) = delete;

    explicit TraceManager(const std::string& filename)
        : tf_(sc_core::sc_create_vcd_trace_file(filename.c_str()))
        , filename_(filename)
        , closed_(false)
        , compress_(false)
        , top_scope_()
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
    void set_time_unit(double v, sc_core::sc_time_unit unit)
    {
        if (tf_) {
            tf_->set_time_unit(v, unit);
        }
    }

    // ----------------------------------------------------------------
    // set_top_scope() — set a prefix applied to all subsequent trace()
    // and trace_with_group() calls.
    //
    // VCD viewers (GTKWave, Surfer) interpret dots in signal names as
    // scope separators. By setting a top scope, all signals appear
    // under a clean hierarchy instead of mixing at the top level.
    //
    // Example:
    //   tm.set_top_scope("dut");
    //   tm.trace(clk, "clk");              // → dut.clk
    //   tm.trace_with_group(count, "count", "datapath");
    //                                      // → dut.datapath.count
    // ----------------------------------------------------------------
    void set_top_scope(const std::string& scope)
    {
        top_scope_ = scope;
    }

    // ----------------------------------------------------------------
    // compress_output() — enable gzip compression of the VCD file.
    //
    // When enabled, close() compresses the file to <filename>.gz and
    // removes the original. Zero overhead during simulation.
    // ----------------------------------------------------------------
    void compress_output(bool enable = true)
    {
        compress_ = enable;
    }

    // ----------------------------------------------------------------
    // trace() — add a signal to the VCD file.
    // ----------------------------------------------------------------
    template <typename Signal>
    void trace(Signal& sig, const std::string& name)
    {
        static_assert(is_sc_signal_or_port_v<Signal>,
            "TraceManager::trace(): Signal must be sc_signal<T>, "
            "sc_in<T>, or sc_out<T>");

        std::string full_name = make_full_name(name);
        if (!filter_.matches(full_name)) return;
        if (!tf_) return;

        sc_core::sc_trace(tf_, sig, full_name);
    }

    // ----------------------------------------------------------------
    // trace_with_group() — add signal with a group prefix.
    // ----------------------------------------------------------------
    template <typename Signal>
    void trace_with_group(Signal& sig, const std::string& name,
                          const std::string& group)
    {
        static_assert(is_sc_signal_or_port_v<Signal>,
            "TraceManager::trace_with_group(): Signal must be sc_signal<T>, "
            "sc_in<T>, or sc_out<T>");

        std::string full_name = make_full_name(group + "." + name);
        if (!filter_.matches(full_name)) return;
        if (!tf_) return;

        sc_core::sc_trace(tf_, sig, full_name);
    }

    // ----------------------------------------------------------------
    // trace_all() — auto-trace all sc_signal children of a module.
    //
    // Iterates over mod.get_child_objects() and traces every sc_signal
    // whose template type is in the registry (see register_signal_type).
    //
    // Parameters:
    //   mod        — the module to trace signals from
    //   recursive  — if true, also trace signals of all submodules
    //
    // Returns the number of signals successfully traced.
    // ----------------------------------------------------------------
    size_t trace_all(sc_core::sc_module& mod, bool recursive = false)
    {
        return trace_all_impl(mod, "", recursive);
    }

    // ----------------------------------------------------------------
    // trace_top_level_signals() — trace all sc_signal objects that are
    // direct children of the simulation root (i.e., declared in sc_main).
    //
    // Returns the number of signals traced.
    // ----------------------------------------------------------------
    size_t trace_top_level_signals()
    {
        size_t count = 0;
        for (sc_core::sc_object* obj : sc_core::sc_get_top_level_objects()) {
            std::string kind = obj->kind();
            if (kind != "sc_signal" && kind != "sc_clock") continue;

            std::string full_name = top_scope_.empty()
                ? obj->name()
                : top_scope_ + "." + obj->name();
            if (!filter_.matches(full_name)) continue;
            if (!tf_) continue;

            for (auto& tracer : detail::tracer_registry()) {
                if (tracer(tf_, obj, full_name)) {
                    count++;
                    break;
                }
            }
        }
        return count;
    }

    // ----------------------------------------------------------------
    // close() — close the VCD file (called automatically by destructor).
    // If compress_output() was enabled, the file is gzipped at this point.
    // ----------------------------------------------------------------
    void close()
    {
        if (tf_ && !closed_) {
            sc_core::sc_close_vcd_trace_file(tf_);
            closed_ = true;
            tf_ = nullptr;

            if (compress_) {
                std::string vcd_path = filename_ + ".vcd";
                if (!detail::gzip_file(vcd_path)) {
                    std::cerr << "[TraceManager] Warning: gzip compression "
                              << "failed for " << vcd_path
                              << " (file left uncompressed)\n";
                }
            }
        }
    }

    // ----------------------------------------------------------------
    sc_core::sc_trace_file* raw() const { return tf_; }
    const std::string& filename() const { return filename_; }
    operator sc_core::sc_trace_file*() const { return tf_; }

private:
    std::string make_full_name(const std::string& name) const
    {
        if (top_scope_.empty()) {
            return name;
        }
        return top_scope_ + "." + name;
    }

    size_t trace_all_impl(sc_core::sc_module& mod,
                          const std::string& parent_prefix,
                          bool recursive)
    {
        size_t count = 0;

        std::string mod_prefix;
        if (parent_prefix.empty()) {
            mod_prefix = mod.name();
        } else {
            mod_prefix = parent_prefix + "." + std::string(mod.name());
        }

        std::string base_prefix = mod_prefix;
        if (parent_prefix.empty() && !top_scope_.empty()) {
            base_prefix = top_scope_ + "." + mod_prefix;
        }

        for (sc_core::sc_object* obj : mod.get_child_objects()) {
            const std::string kind = obj->kind();

            if (kind == "sc_signal" || kind == "sc_clock") {
                std::string full_name = base_prefix + "." + obj->name();
                if (!filter_.matches(full_name)) continue;
                if (!tf_) continue;

                for (auto& tracer : detail::tracer_registry()) {
                    if (tracer(tf_, obj, full_name)) {
                        count++;
                        break;
                    }
                }
            } else if (kind == "sc_module" && recursive) {
                auto submod = dynamic_cast<sc_core::sc_module*>(obj);
                if (submod) {
                    count += trace_all_impl(*submod, base_prefix, recursive);
                }
            }
        }

        return count;
    }

    sc_core::sc_trace_file* tf_;
    NameFilter filter_;
    std::string filename_;
    bool closed_;
    bool compress_;
    std::string top_scope_;
};

// ========================================================================
inline std::shared_ptr<TraceManager> create_trace_file(const std::string& filename)
{
    return std::make_shared<TraceManager>(filename);
}

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_TRACE_HPP