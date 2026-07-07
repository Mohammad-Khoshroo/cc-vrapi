#ifndef CC_VRWRAPPER_TRACE_UTILS_HPP
#define CC_VRWRAPPER_TRACE_UTILS_HPP

// ============================================================================
// cc_vrwrapper :: sc-trace :: utils.hpp
// Common type traits, value conversion, and filter helpers.
// Supports BOTH C++17 and C++20.
// ============================================================================

#include <systemc>
#include <string>
#include <sstream>
#include <type_traits>
#include <regex>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include <cstdint>

namespace cc_vrwrapper {
namespace trace {

using namespace sc_core;
using namespace sc_dt;

// ========================================================================
// TYPE TRAITS — extract value type from signal/port
// ========================================================================

template <typename T> struct signal_value_type { using type = void; };
template <typename T> struct signal_value_type<sc_signal<T>> { using type = T; };
template <typename T> struct signal_value_type<sc_in<T>>     { using type = T; };
template <typename T> struct signal_value_type<sc_out<T>>    { using type = T; };

template <typename T>
using signal_value_type_t = typename signal_value_type<T>::type;

// ----------------------------------------------------------------
// Check if T is a signal or port
// ----------------------------------------------------------------
template <typename T> struct is_sc_signal     : std::false_type {};
template <typename T> struct is_sc_signal<sc_signal<T>> : std::true_type {};

template <typename T> struct is_sc_in          : std::false_type {};
template <typename T> struct is_sc_in<sc_in<T>>     : std::true_type {};

template <typename T> struct is_sc_out         : std::false_type {};
template <typename T> struct is_sc_out<sc_out<T>>    : std::true_type {};

template <typename T>
struct is_sc_signal_or_port : std::integral_constant<bool,
    is_sc_signal<T>::value || is_sc_in<T>::value || is_sc_out<T>::value> {};

template <typename T>
inline constexpr bool is_sc_signal_or_port_v = is_sc_signal_or_port<T>::value;

// ----------------------------------------------------------------
// Width traits for common value types
// ----------------------------------------------------------------
template <typename T> struct signal_width { static constexpr int value = 0; };
template <> struct signal_width<bool>        { static constexpr int value = 1; };
template <> struct signal_width<sc_logic>    { static constexpr int value = 1; };
template <int W> struct signal_width<sc_lv<W>>      { static constexpr int value = W; };
template <int W> struct signal_width<sc_bv<W>>      { static constexpr int value = W; };
template <int W> struct signal_width<sc_int<W>>     { static constexpr int value = W; };
template <int W> struct signal_width<sc_uint<W>>    { static constexpr int value = W; };
template <int W> struct signal_width<sc_bigint<W>>  { static constexpr int value = W; };
template <int W> struct signal_width<sc_biguint<W>> { static constexpr int value = W; };

template <typename T>
inline constexpr int signal_width_v = signal_width<T>::value;

// ========================================================================
// VALUE TO STRING — convert any signal value to a string
// ========================================================================

template <typename T>
std::string value_to_string(const T& val)
{
    if constexpr (std::is_same_v<T, bool>) {
        return val ? "1" : "0";
    } else if constexpr (std::is_same_v<T, sc_logic>) {
        return std::string(1, val.to_char());
    } else if constexpr (std::is_integral_v<T>) {
        return std::to_string(val);
    } else {
        // For sc_lv, sc_bv, sc_int, sc_uint, etc.
        // Use operator<< which all SystemC data types support
        std::ostringstream ss;
        ss << val;
        return ss.str();
    }
}

// ========================================================================
// TIME HELPERS
// ========================================================================

inline std::string time_to_string(sc_time t)
{
    return t.to_string();
}

inline double time_to_ns(sc_time t)
{
    return t.to_seconds() * 1e9;
}

inline double time_to_ps(sc_time t)
{
    return t.to_seconds() * 1e12;
}

// ========================================================================
// NAME FILTER — regex-based include/exclude for signal names
// ========================================================================

struct NameFilter {
    std::vector<std::regex> include_patterns;
    std::vector<std::regex> exclude_patterns;

    bool matches(const std::string& name) const
    {
        // If include patterns exist, name must match at least one
        if (!include_patterns.empty()) {
            bool found = false;
            for (const auto& re : include_patterns) {
                if (std::regex_search(name, re)) {
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }

        // If exclude patterns exist, name must not match any
        for (const auto& re : exclude_patterns) {
            if (std::regex_search(name, re)) {
                return false;
            }
        }

        return true;
    }

    void add_include(const std::string& pattern)
    {
        try {
            include_patterns.emplace_back(pattern);
        } catch (const std::regex_error& e) {
            std::cerr << "[sc-trace] Invalid include regex: \""
                      << pattern << "\" — " << e.what() << "\n";
        }
    }

    void add_exclude(const std::string& pattern)
    {
        try {
            exclude_patterns.emplace_back(pattern);
        } catch (const std::regex_error& e) {
            std::cerr << "[sc-trace] Invalid exclude regex: \""
                      << pattern << "\" — " << e.what() << "\n";
        }
    }

    void clear()
    {
        include_patterns.clear();
        exclude_patterns.clear();
    }
};

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_UTILS_HPP