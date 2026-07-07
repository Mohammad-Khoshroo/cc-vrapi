#ifndef CC_VRWRAPPER_REPORT_FILTERS_HPP
#define CC_VRWRAPPER_REPORT_FILTERS_HPP

#include <set>
#include <string>
#include <mutex>
#include <regex>
#include <systemc>

namespace cc_vrwrapper::report::filters
{

inline std::set<sc_core::sc_severity> enabled_severities = {
    sc_core::SC_INFO, sc_core::SC_WARNING, sc_core::SC_ERROR, sc_core::SC_FATAL
};

inline std::set<std::string> suppressed_msg_types;
inline std::set<std::string> allowed_msg_types; // empty = allow all

inline std::vector<std::regex> message_regex_allow;
inline std::vector<std::regex> message_regex_deny;
inline std::mutex filter_mutex;

inline void set_enabled_severities(std::initializer_list<sc_core::sc_severity> s)
{
    std::lock_guard<std::mutex> g(filter_mutex);
    enabled_severities = s;
}

inline void suppress_msg_type(const std::string& t)
{
    std::lock_guard<std::mutex> g(filter_mutex);
    suppressed_msg_types.insert(t);
}

inline void unsuppress_msg_type(const std::string& t)
{
    std::lock_guard<std::mutex> g(filter_mutex);
    suppressed_msg_types.erase(t);
}

inline void allow_only_msg_type(const std::string& t)
{
    std::lock_guard<std::mutex> g(filter_mutex);
    allowed_msg_types.insert(t);
}

inline void add_message_regex_allow(const std::string& pattern)
{
    std::lock_guard<std::mutex> g(filter_mutex);
    try {
        message_regex_allow.emplace_back(pattern);
    } catch (...) {}
}

inline void add_message_regex_deny(const std::string& pattern)
{
    std::lock_guard<std::mutex> g(filter_mutex);
    try {
        message_regex_deny.emplace_back(pattern);
    } catch (...) {}
}

// Returns true if the report should be processed.
inline bool should_report(sc_core::sc_severity sev,
                          const std::string& msg_type,
                          const std::string& message)
{
    std::lock_guard<std::mutex> g(filter_mutex);

    if (enabled_severities.find(sev) == enabled_severities.end()) return false;

    if (!suppressed_msg_types.empty() &&
        suppressed_msg_types.find(msg_type) != suppressed_msg_types.end())
        return false;

    if (!allowed_msg_types.empty() &&
        allowed_msg_types.find(msg_type) == allowed_msg_types.end())
        return false;

    for (const auto& re : message_regex_deny) {
        if (std::regex_search(message, re)) return false;
    }

    if (!message_regex_allow.empty()) {
        bool matched = false;
        for (const auto& re : message_regex_allow) {
            if (std::regex_search(message, re)) { matched = true; break; }
        }
        if (!matched) return false;
    }

    return true;
}

} // namespace cc_vrwrapper::report::filters

#endif