#ifndef CC_VRWRAPPER_REPORT_HANDLER_HPP
#define CC_VRWRAPPER_REPORT_HANDLER_HPP

#include <systemc>
#include <stdexcept>
#include <mutex>
#include <memory>
#include <cstdlib>
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
                                        const sc_core::sc_actions& /*actions*/)
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

    // Filter: suppress severities LESS severe than the configured threshold.
    // (min_severity acts as a "minimum severity to display" — typical for logging)
    // SC_INFO=0, SC_WARNING=1, SC_ERROR=2, SC_FATAL=3
    // Example: if min_severity = SC_WARNING, then SC_INFO (0 < 1) is suppressed,
    //          but SC_WARNING (1), SC_ERROR (2), SC_FATAL (3) are shown.
    //
    // CHANGED: was c.max_severity (renamed in config.hpp for clarity)
    if (sev < c.min_severity) return;
    if (!filters::should_report(sev, info.msg_type, info.message)) return;

    // Stats
    stats::bump(sev);

    // Dispatch to all sinks (our custom logging)
    sinks::sink_manager().write_all(info);

    // Throw if configured (user wants exception)
    //
    // WARNING: throwing from within the report handler can be dangerous if
    // SystemC invokes the handler from a noexcept context or from C code.
    // If the exception propagates out of such a context, std::terminate
    // will be called. Use with caution; for production code prefer checking
    // stats::get() after sc_start() returns.
    if (c.throw_on_error && sev >= c.throw_threshold) {
        throw std::runtime_error("[" + info.msg_type + "] " + info.message);
    }

    // For SC_FATAL, abort the simulation directly.
    // We do NOT call default_handler because in SystemC 3.0 it would
    // re-display the message, causing duplicate output.
    if (sev == sc_core::SC_FATAL) {
        sinks::sink_manager().flush_all();
        std::abort();
    }

    // For all other severities: do nothing more.
    // We already logged via our sinks. SystemC's actions are set to
    // SC_DO_NOTHING (see init_report_handler), so SystemC won't
    // try to execute SC_DISPLAY or SC_LOG after we return.
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

    // ========================================================================
    // CRITICAL: Suppress SystemC's built-in default actions.
    //
    // In SystemC 3.0, the default actions for each severity include
    // SC_DISPLAY (print to stderr) and SC_LOG (write to systemc log).
    // Even if our custom handler doesn't call default_handler, SystemC 3.0
    // may still execute these actions independently AFTER our handler
    // returns, causing DUPLICATE output.
    //
    // By setting actions to SC_DO_NOTHING, we tell SystemC to do nothing
    // except call our handler. Our handler takes full responsibility
    // for all logging.
    //
    // For SC_FATAL, we keep SC_ABORT as a safety net. Our handler calls
    // std::abort() directly (see above), but if for any reason that path
    // is not reached (e.g. a future code change disables it), SystemC's
    // SC_ABORT ensures the simulation still terminates rather than
    // continuing in an inconsistent state.
    // ========================================================================
    sc_core::sc_report_handler::set_actions(sc_core::SC_INFO,
        sc_core::SC_DO_NOTHING);
    sc_core::sc_report_handler::set_actions(sc_core::SC_WARNING,
        sc_core::SC_DO_NOTHING);
    sc_core::sc_report_handler::set_actions(sc_core::SC_ERROR,
        sc_core::SC_DO_NOTHING);
    sc_core::sc_report_handler::set_actions(sc_core::SC_FATAL,
        sc_core::SC_ABORT);

    // Install our custom handler
    sc_core::sc_report_handler::set_handler(personalized_report_handler);
}

inline void close_report_log()
{
    sinks::sink_manager().flush_all();
}

// Convenience: register a user callback
inline void add_callback(sinks::CallbackFn fn)
{
    auto& mgr = sinks::sink_manager();
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