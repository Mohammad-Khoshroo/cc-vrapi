#ifndef CC_VRWRAPPER_REPORT_HANDLER_HPP
#define CC_VRWRAPPER_REPORT_HANDLER_HPP

#include <systemc>
#include <stdexcept>
#include <mutex>
#include <memory>
#include "config.hpp"
#include "stats.hpp"
#include "filters.hpp"
#include "formatter.hpp"
#include "sinks.hpp"
#include "utils.hpp"

namespace cc_vrwrapper::report::handler
{

inline std::mutex handler_mutex;

inline void personalized_report_handler(const sc_core::sc_report& rep,
                                        const sc_core::sc_actions& actions)
{
    sc_core::sc_severity sev = rep.get_severity();

    // Build report info
    formatter::ReportInfo info;
    info.severity  = sev;
    info.msg_type  = rep.get_msg_type();
    info.message   = rep.get_msg();
    info.timestamp = sc_core::sc_time_stamp().to_string();
    info.file      = rep.get_file_name();
    info.line      = rep.get_line_number();

    auto c = config::get_config();
    if (c.capture_hierarchy) {
        info.process = utils::get_module_hierarchy();
    } else {
        info.process = "-";
    }

    // Filter
    auto cc = config::get_config();
    if (sev > cc.max_severity) return;
    if (!filters::should_report(sev, info.msg_type, info.message)) return;

    // Stats
    stats::bump(sev);

    // Dispatch to all sinks
    sinks::sink_manager().write_all(info);

    // Throw if configured
    if (cc.throw_on_error && sev >= cc.throw_threshold) {
        throw std::runtime_error("[" + info.msg_type + "] " + info.message);
    }

    // Pass to SystemC default (handles SC_FATAL abort, etc.)
    sc_core::sc_report_handler::default_handler(rep, actions);
}

// ------------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------------

inline void init_report_handler(const std::string& path = "")
{
    if (!path.empty()) config::set_log_file_path(path);

    auto c = config::get_config();

    // Clear existing sinks
    sinks::sink_manager().clear();

    // Add terminal sink
    if (c.log_to_terminal) {
        sinks::sink_manager().add(std::make_shared<sinks::TerminalSink>());
    }

    // Add file sink
    if (c.log_to_file) {
        sinks::sink_manager().add(std::make_shared<sinks::FileSink>(c.log_file_path));
    }

    sc_core::sc_report_handler::set_handler(personalized_report_handler);
}

inline void close_report_log()
{
    sinks::sink_manager().flush_all();
}

// Convenience: register a user callback
inline void add_callback(sinks::CallbackFn fn)
{
    // Find existing callback sink, or create one
    auto& mgr = sinks::sink_manager();
    // Simple approach: always add a new callback sink wrapping the fn
    auto cb_sink = std::make_shared<sinks::CallbackSink>();
    cb_sink->add(std::move(fn));
    mgr.add(cb_sink);
}

// Convenience: add a network sink
inline void add_network_sink(const std::string& host, int port)
{
    sinks::sink_manager().add(std::make_shared<sinks::NetworkSink>(host, port));
}

} // namespace cc_vrwrapper::report::handler

#endif