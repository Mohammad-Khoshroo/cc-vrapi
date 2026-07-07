#ifndef CC_VRWRAPPER_REPORT_UTILS_HPP
#define CC_VRWRAPPER_REPORT_UTILS_HPP

#include <string>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>
#include <systemc>
#include <unistd.h>

#if __cplusplus >= 202002L
#  include <version>
#  ifdef __cpp_lib_source_location
#    include <source_location>
#    define CC_VR_HAS_STD_SOURCE_LOCATION 1
#  endif
#endif

namespace cc_vrwrapper::report::utils
{

// ------------------------------------------------------------------------
// TTY check — only emit colors if stderr is a real terminal
// ------------------------------------------------------------------------
inline bool stderr_is_tty()
{
    return ::isatty(fileno(stderr)) != 0;
}

// ------------------------------------------------------------------------
// Create parent directories for a file path (mkdir -p)
// ------------------------------------------------------------------------
inline bool ensure_parent_dirs(const std::string& path)
{
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::error_code ec;
            std::filesystem::create_directories(p.parent_path(), ec);
            return !ec;
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ------------------------------------------------------------------------
// JSON string escaping — produces valid JSON output
// ------------------------------------------------------------------------
inline std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\c': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// ------------------------------------------------------------------------
// Logfmt escaping — escapes spaces and = in values
// ------------------------------------------------------------------------
inline std::string logfmt_escape(const std::string& s)
{
    bool need_quote = false;
    for (char c : s) {
        if (c == ' ' || c == '=' || c == '"' || c == '\n' || c == '\t') {
            need_quote = true;
            break;
        }
    }
    if (!need_quote) return s;

    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    out += "\"";
    return out;
}

// ------------------------------------------------------------------------
// Source location — uses <source_location> in C++20, macros in C++17
// ------------------------------------------------------------------------
struct source_loc {
    const char* file = nullptr;
    int         line = 0;
    const char* func = nullptr;

#ifdef CC_VR_HAS_STD_SOURCE_LOCATION
    static constexpr source_loc current(
        std::source_location sl = std::source_location::current()) noexcept
    {
        return source_loc{ sl.file_name(), static_cast<int>(sl.line()),
                           sl.function_name() };
    }
#endif
};

#ifdef CC_VR_HAS_STD_SOURCE_LOCATION
#  define VR_CURRENT_LOC \
       ::cc_vrwrapper::report::utils::source_loc::current()
#else
#  define VR_CURRENT_LOC \
       ::cc_vrwrapper::report::utils::source_loc{ __FILE__, __LINE__, __func__ }
#endif

// ------------------------------------------------------------------------
// Module hierarchy — walks up the SystemC hierarchy
// ------------------------------------------------------------------------
// CHANGED: improved to detect process type and fall back to thread ID
// when called from outside a SystemC process (e.g. from std::thread).
//
// Output format:
//   - Inside SC_CTHREAD : "module.proc [CTHREAD]"
//   - Inside SC_THREAD  : "module.proc [THREAD]"
//   - Inside SC_METHOD  : "module.proc [METHOD]"
//   - From std::thread  : "thread:0x7f... [extern]"
//   - From sc_main      : "sc_main [extern]"
//   - Unknown           : "? [unknown]"
// ------------------------------------------------------------------------
inline std::string get_module_hierarchy()
{
    try {
        // First, check if we're inside a SystemC process
        auto h = sc_core::sc_get_current_process_handle();

        if (h.valid()) {
            const char* n = h.name();
            std::string name = (n && *n) ? std::string(n) : std::string("?");

            // Determine process kind
            // sc_core::sc_process_b has proc_kind() method
            const char* kind_str = "[unknown]";
            try {
                // sc_process_b::proc_kind() returns sc_core::sc_curr_proc_kind
                // which is one of SC_NO_PROCESS_, SC_METHOD_, SC_THREAD_, SC_CTHREAD_
                auto* pb = sc_core::sc_get_current_process_b();
                if (pb) {
                    auto kind = pb->proc_kind();
                    switch (kind) {
                        case sc_core::SC_METHOD_PROC_:   kind_str = "[METHOD]";  break;
                        case sc_core::SC_THREAD_PROC_:   kind_str = "[THREAD]";  break;
                        case sc_core::SC_CTHREAD_PROC_:  kind_str = "[CTHREAD]"; break;
                        default:                    kind_str = "[PROC]";    break;
                    }
                }
            } catch (...) {
                // keep "[unknown]"
            }

            return name + " " + kind_str;
        }

        // Not inside a SystemC process — check if simulation is running
        if (!sc_core::sc_is_running()) {
            return "sc_main [extern]";
        }

        // Simulation running but no current process.
        // This typically happens when called from a std::thread or
        // from native C++ code outside SystemC's kernel.
        // Fall back to thread ID for identification.
        std::ostringstream ss;
        ss << "thread:0x" << std::hex
           << std::hash<std::thread::id>{}(std::this_thread::get_id())
           << " [extern]";
        return ss.str();

    } catch (...) {
        return "? [unknown]";
    }
}

} // namespace cc_vrwrapper::report::utils

#endif // CC_VRWRAPPER_REPORT_UTILS_HPP