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
using sc_dt::sc_logic;
using sc_dt::sc_lv;
using sc_dt::sc_bv;
using sc_dt::sc_int;
using sc_dt::sc_uint;
using sc_dt::sc_bigint;
using sc_dt::sc_biguint;
using sc_dt::SC_LOGIC_0;
using sc_dt::SC_LOGIC_1;
using sc_dt::SC_LOGIC_X;
using sc_dt::SC_LOGIC_Z;

// ========================================================================
// TYPE TRAITS — extract value type from signal/port
// ========================================================================

template <typename T> struct signal_value_type { using type = void; };
template <typename T> struct signal_value_type<sc_signal<T>> { using type = T; };
template <typename T> struct signal_value_type<sc_in<T>>     { using type = T; };
template <typename T> struct signal_value_type<sc_out<T>>    { using type = T; };

// sc_clock inherits from sc_signal<bool> — treat it as a bool signal
template <> struct signal_value_type<sc_core::sc_clock> { using type = bool; };

template <typename T>
using signal_value_type_t = typename signal_value_type<T>::type;

// ----------------------------------------------------------------
// Check if T is a signal or port
// ----------------------------------------------------------------
template <typename T> struct is_sc_signal     : std::false_type {};
template <typename T> struct is_sc_signal<sc_signal<T>> : std::true_type {};
template <> struct is_sc_signal<sc_core::sc_clock> : std::true_type {};

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
        // Use operator<< which all SystemC data types support.
        // Reuse a thread_local ostringstream to avoid repeated
        // construction/destruction overhead on every value change.
        static thread_local std::ostringstream ss;
        ss.str("");
        ss.clear();
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

// ========================================================================
// TYPE-ERASED SIGNAL TRACING
//
// Because C++ templates produce distinct types for each width
// (sc_lv<8> != sc_lv<16>), runtime dispatch must know which types to
// try. We use a registration model:
//   1. Common types (bool, sc_logic) are pre-registered.
//   2. User calls register_signal_type<T>() for additional types.
//   3. trace_all() iterates the registry and tries each via dynamic_cast.
// ========================================================================

namespace detail {

using TracerFn = bool(*)(sc_core::sc_trace_file*,
                         sc_core::sc_object*,
                         const std::string&);

template <typename T>
bool trace_typed(sc_core::sc_trace_file* tf,
                 sc_core::sc_object* obj,
                 const std::string& name)
{
    auto* p = dynamic_cast<sc_core::sc_signal<T>*>(obj);
    if (!p) return false;
    sc_core::sc_trace(tf, *p, name);
    return true;
}

inline std::vector<TracerFn>& tracer_registry()
{
    static std::vector<TracerFn> v = {
        &trace_typed<bool>,
        &trace_typed<sc_dt::sc_logic>
    };
    return v;
}

} // namespace detail

// --------------------------------------------------------------------
// register_signal_type<T>() — register an additional value type for
// runtime dispatch in trace_all().
//
// Must be called BEFORE trace_all(). Safe to call multiple times for
// the same type — duplicate registration is ignored.
//
// Example:
//   tr::register_signal_type<sc_dt::sc_lv<8>>();
//   tr::register_signal_type<sc_dt::sc_int<32>>();
//   tf->trace_all(dut, true);
// --------------------------------------------------------------------
template <typename T>
void register_signal_type()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    detail::tracer_registry().push_back(&detail::trace_typed<T>);
}

// --------------------------------------------------------------------
// Convenience macro — register all common SystemC signal types at once.
// --------------------------------------------------------------------
#define CC_VRWRAPPER_REGISTER_COMMON_TYPES()                              \
    do {                                                                  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_lv<8>>();   \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_lv<16>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_lv<32>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_lv<64>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_bv<8>>();   \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_bv<16>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_bv<32>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_bv<64>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_int<8>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_int<16>>(); \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_int<32>>(); \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_int<64>>(); \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_uint<8>>(); \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_uint<16>>();\
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_uint<32>>();\
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_uint<64>>();\
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_bigint<64>>();  \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_bigint<128>>(); \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_biguint<64>>(); \
        ::cc_vrwrapper::trace::register_signal_type<sc_dt::sc_biguint<128>>();\
    } while(0)

} // namespace trace
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_TRACE_UTILS_HPP