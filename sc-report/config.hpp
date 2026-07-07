#ifndef CC_VRWRAPPER_REPORT_CONFIG_HPP
#define CC_VRWRAPPER_REPORT_CONFIG_HPP

#include <string>
#include <set>
#include <mutex>
#include <systemc>

namespace cc_vrwrapper::report::config
{

    // ------------------------------------------------------------------------
    // All configuration state (protected by mutex where mutable at runtime)
    // ------------------------------------------------------------------------
    struct Config {
        std::string log_file_path = "logs/systemc_report.log";
        bool        log_as_json = false;
        bool        log_as_logfmt = false;
        bool        throw_on_error = false;
        bool        force_no_color = false;
        bool        log_to_terminal = true;
        bool        log_to_file = true;
        sc_core::sc_severity min_severity = sc_core::SC_INFO;
        sc_core::sc_severity throw_threshold = sc_core::SC_ERROR;
        std::size_t max_file_size = 10 * 1024 * 1024; // 10 MB
        bool        compress_rotated = true;
        bool        capture_hierarchy = true;
    };

    inline Config   cfg;
    inline std::mutex cfg_mutex;

    inline Config get_config() {
        std::lock_guard<std::mutex> g(cfg_mutex);
        return cfg;
    }

    // Setters
    inline void set_log_file_path(const std::string& p) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.log_file_path = p; }
    inline void set_log_as_json(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.log_as_json = f; if (f) cfg.log_as_logfmt = false; }
    inline void set_log_as_logfmt(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.log_as_logfmt = f; if (f) cfg.log_as_json = false; }
    inline void set_throw_on_error(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.throw_on_error = f; }
    inline void set_throw_threshold(sc_core::sc_severity s) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.throw_threshold = s; }
    inline void set_force_no_color(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.force_no_color = f; }
    inline void set_log_to_terminal(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.log_to_terminal = f; }
    inline void set_log_to_file(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.log_to_file = f; }
    inline void set_min_severity(sc_core::sc_severity s) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.min_severity = s; }
    inline void set_max_file_size(std::size_t b) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.max_file_size = b; }
    inline void set_compress_rotated(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.compress_rotated = f; }
    inline void set_capture_hierarchy(bool f) { std::lock_guard<std::mutex> g(cfg_mutex); cfg.capture_hierarchy = f; }

} // namespace cc_vrwrapper::report::config

#endif