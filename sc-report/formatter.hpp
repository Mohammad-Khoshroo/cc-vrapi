#ifndef CC_VRWRAPPER_REPORT_FORMATTER_HPP
#define CC_VRWRAPPER_REPORT_FORMATTER_HPP

#include <string>
#include <sstream>
#include <systemc>
#include "utils.hpp"

namespace cc_vrwrapper::report::formatter
{

struct ReportInfo {
    sc_core::sc_severity severity;
    std::string          msg_type;
    std::string          message;
    std::string          timestamp;
    std::string          file;
    int                  line;
    std::string          process;
};

// ------------------------------------------------------------------------
// Plain text format (no colors, no ANSI codes)
// ------------------------------------------------------------------------
inline std::string format_text(const ReportInfo& r)
{
    std::ostringstream ss;
    const char* sev = "UNKNOWN";
    switch (r.severity) {
        case sc_core::SC_INFO:    sev = "INFO";    break;
        case sc_core::SC_WARNING: sev = "WARNING"; break;
        case sc_core::SC_ERROR:   sev = "ERROR";   break;
        case sc_core::SC_FATAL:   sev = "FATAL";   break;
        default: break;
    }
    ss << "[" << sev << "] "
       << r.msg_type << ": " << r.message
       << " @ " << r.timestamp
       << " (" << r.file << ":" << r.line << ")"
       << " in process: " << r.process;
    return ss.str();
}

// ------------------------------------------------------------------------
// JSON format (escaped, valid JSON)
// ------------------------------------------------------------------------
inline std::string format_json(const ReportInfo& r)
{
    const char* sev = "UNKNOWN";
    switch (r.severity) {
        case sc_core::SC_INFO:    sev = "INFO";    break;
        case sc_core::SC_WARNING: sev = "WARNING"; break;
        case sc_core::SC_ERROR:   sev = "ERROR";   break;
        case sc_core::SC_FATAL:   sev = "FATAL";   break;
        default: break;
    }
    std::ostringstream ss;
    ss << "{"
       << "\"severity\":\""   << sev << "\","
       << "\"msg_type\":\""   << utils::json_escape(r.msg_type)  << "\","
       << "\"message\":\""    << utils::json_escape(r.message)   << "\","
       << "\"time\":\""       << utils::json_escape(r.timestamp) << "\","
       << "\"file\":\""       << utils::json_escape(r.file)      << "\","
       << "\"line\":"         << r.line << ","
       << "\"process\":\""    << utils::json_escape(r.process)   << "\""
       << "}";
    return ss.str();
}

// ------------------------------------------------------------------------
// Logfmt format: key=value pairs
// ------------------------------------------------------------------------
inline std::string format_logfmt(const ReportInfo& r)
{
    const char* sev = "unknown";
    switch (r.severity) {
        case sc_core::SC_INFO:    sev = "info";    break;
        case sc_core::SC_WARNING: sev = "warning"; break;
        case sc_core::SC_ERROR:   sev = "error";   break;
        case sc_core::SC_FATAL:   sev = "fatal";   break;
        default: break;
    }
    std::ostringstream ss;
    ss << "severity=" << sev
       << " msg_type=" << utils::logfmt_escape(r.msg_type)
       << " message="  << utils::logfmt_escape(r.message)
       << " time="     << utils::logfmt_escape(r.timestamp)
       << " file="     << utils::logfmt_escape(r.file)
       << " line="     << r.line
       << " process="  << utils::logfmt_escape(r.process);
    return ss.str();
}

// Dispatcher
inline std::string format(const ReportInfo& r, bool as_json, bool as_logfmt)
{
    if (as_json)   return format_json(r);
    if (as_logfmt) return format_logfmt(r);
    return format_text(r);
}

} // namespace cc_vrwrapper::report::formatter

#endif