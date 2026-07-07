#ifndef CC_VRWRAPPER_REPORT_UTILS_HPP
#define CC_VRWRAPPER_REPORT_UTILS_HPP

#include <string>
#include <cstdio>
#include <filesystem>
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
// ------------------------------------------------------------------------
// Module hierarchy — walks up the SystemC hierarchy
// ------------------------------------------------------------------------
inline std::string get_module_hierarchy()
{
    try {
        auto h = sc_core::sc_get_current_process_handle();
        // Check if handle is valid (i.e., we're inside an executing process)
        if (h.valid()) {
            const char* n = h.name();
            if (n && *n) return n;
        }
        // If simulation hasn't started yet, we're in sc_main
        if (!sc_core::sc_is_running()) {
            return "sc_main";
        }
        // Simulation running but no current process (rare edge case)
        return "<unknown>";
    } catch (...) {
        return "<unknown>";
    }
}

} // namespace cc_vrwrapper::report::utils

#endif // CC_VRWRAPPER_REPORT_UTILS_HPP
