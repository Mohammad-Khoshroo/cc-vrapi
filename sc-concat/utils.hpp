#ifndef CC_VRWRAPPER_CONCAT_UTILS_HPP
#define CC_VRWRAPPER_CONCAT_UTILS_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat :: utils.hpp
// Common type traits, concepts (C++20), and helpers for sc-concat.
// Supports BOTH C++17 and C++20.
// ============================================================================

#include <systemc>
#include <string>
#include <type_traits>

namespace cc_vrwrapper {
namespace concat {

using namespace sc_core;
using namespace sc_dt;

// ========================================================================
// TYPE TRAITS — work in both C++17 and C++20
// ========================================================================

template <typename T> struct is_sc_lv      : std::false_type {};
template <int W>      struct is_sc_lv<sc_lv<W>>      : std::true_type {};

template <typename T> struct is_sc_bv      : std::false_type {};
template <int W>      struct is_sc_bv<sc_bv<W>>      : std::true_type {};

template <typename T> struct is_sc_logic   : std::false_type {};
template <>           struct is_sc_logic<sc_logic>   : std::true_type {};

template <typename T> struct is_sc_int     : std::false_type {};
template <int W>      struct is_sc_int<sc_int<W>>     : std::true_type {};

template <typename T> struct is_sc_uint    : std::false_type {};
template <int W>      struct is_sc_uint<sc_uint<W>>    : std::true_type {};

template <typename T> struct is_sc_bigint  : std::false_type {};
template <int W>      struct is_sc_bigint<sc_bigint<W>>  : std::true_type {};

template <typename T> struct is_sc_biguint : std::false_type {};
template <int W>      struct is_sc_biguint<sc_biguint<W>> : std::true_type {};

template <typename T> inline constexpr bool is_sc_lv_v      = is_sc_lv<T>::value;
template <typename T> inline constexpr bool is_sc_bv_v      = is_sc_bv<T>::value;
template <typename T> inline constexpr bool is_sc_logic_v   = is_sc_logic<T>::value;
template <typename T> inline constexpr bool is_sc_int_v     = is_sc_int<T>::value;
template <typename T> inline constexpr bool is_sc_uint_v    = is_sc_uint<T>::value;
template <typename T> inline constexpr bool is_sc_bigint_v  = is_sc_bigint<T>::value;
template <typename T> inline constexpr bool is_sc_biguint_v = is_sc_biguint<T>::value;

// Group traits
template <typename T>
struct is_sc_logiclike : std::integral_constant<bool,
    is_sc_lv_v<T> || is_sc_bv_v<T> || is_sc_logic_v<T>> {};

template <typename T>
struct is_sc_intlike : std::integral_constant<bool,
    is_sc_int_v<T> || is_sc_uint_v<T> ||
    is_sc_bigint_v<T> || is_sc_biguint_v<T>> {};

template <typename T>
inline constexpr bool is_sc_logiclike_v = is_sc_logiclike<T>::value;

template <typename T>
inline constexpr bool is_sc_intlike_v = is_sc_intlike<T>::value;

// Anything concatenable
template <typename T>
struct is_sc_concatable : std::integral_constant<bool,
    is_sc_logiclike_v<T> || is_sc_intlike_v<T>> {};

template <typename T>
inline constexpr bool is_sc_concatable_v = is_sc_concatable<T>::value;

// ========================================================================
// WIDTH TRAIT — width of a SystemC type in bits
// ========================================================================

template <typename T> struct sc_width;
template <int W> struct sc_width<sc_lv<W>>      { static constexpr int value = W; };
template <int W> struct sc_width<sc_bv<W>>      { static constexpr int value = W; };
template <>      struct sc_width<sc_logic>       { static constexpr int value = 1; };
template <int W> struct sc_width<sc_int<W>>     { static constexpr int value = W; };
template <int W> struct sc_width<sc_uint<W>>    { static constexpr int value = W; };
template <int W> struct sc_width<sc_bigint<W>>  { static constexpr int value = W; };
template <int W> struct sc_width<sc_biguint<W>> { static constexpr int value = W; };

template <typename T>
inline constexpr int sc_width_v = sc_width<T>::value;

// ========================================================================
// CONCEPTS — C++20 only (for nicer error messages)
// ========================================================================

#if __cplusplus >= 202002L
template <typename T>
concept ScLogicLike = is_sc_logiclike_v<T>;

template <typename T>
concept ScIntLike = is_sc_intlike_v<T>;

template <typename T>
concept ScConcatable = is_sc_concatable_v<T>;
#endif

// ========================================================================
// DETAIL HELPERS
// ========================================================================
namespace detail
{
    // ----------------------------------------------------------------
    // to_bitstring: convert any concatenable type to MSB-first bit
    // string of length = width. Preserves X/Z for sc_lv / sc_logic.
    // ----------------------------------------------------------------
    template <typename T>
    std::string to_bitstring(const T& v)
    {
        static_assert(is_sc_concatable_v<T>,
            "to_bitstring: T must be a SystemC logic-like or int-like type");

        if constexpr (is_sc_lv_v<T> || is_sc_bv_v<T>) {
            // sc_lv<W>::to_string() returns MSB-first string of length W
            // (e.g. sc_lv<4>("0b1010").to_string() == "1010")
            return v.to_string();
        } else if constexpr (is_sc_logic_v<T>) {
            // Single char: '0', '1', 'X', 'Z' (or lowercase)
            return std::string(1, v.to_char());
        } else if constexpr (is_sc_int_v<T> || is_sc_uint_v<T> ||
                             is_sc_bigint_v<T> || is_sc_biguint_v<T>) {
            // Convert to sc_lv first to get a clean bitstring.
            // This preserves two's-complement for signed types
            // (e.g. sc_int<8>(-1) -> "11111111").
            constexpr int W = sc_width_v<T>;
            sc_lv<W> tmp;
            tmp = v;
            return tmp.to_string();
        } else {
            // Should be unreachable due to static_assert above
            return std::string{};
        }
    }

    // ----------------------------------------------------------------
    // total_width: sum of widths of a parameter pack
    // ----------------------------------------------------------------
    template <typename... Args>
    struct total_width;

    template <>
    struct total_width<> {
        static constexpr int value = 0;
    };

    template <typename T, typename... Rest>
    struct total_width<T, Rest...> {
        static constexpr int value = sc_width_v<T> + total_width<Rest...>::value;
    };

    template <typename... Args>
    inline constexpr int total_width_v = total_width<Args...>::value;

    // ----------------------------------------------------------------
    // make_from_bitstring: construct a result type from a bitstring.
    // Used by cat<Result>(...) to return user-specified type.
    // ----------------------------------------------------------------
    template <typename Result>
    Result make_from_bitstring(const std::string& bits)
    {
        static_assert(is_sc_concatable_v<Result>,
            "make_from_bitstring: Result must be a SystemC logic-like or int-like type");

        if constexpr (is_sc_lv_v<Result> || is_sc_bv_v<Result>) {
            // sc_lv / sc_bv have a string constructor
            return Result(bits.c_str());
        } else if constexpr (is_sc_int_v<Result> || is_sc_uint_v<Result>) {
            // For narrow int types, go via sc_lv -> int64/uint64
            constexpr int W = sc_width_v<Result>;
            sc_lv<W> tmp(bits.c_str());
            if constexpr (is_sc_int_v<Result>)
                return Result(tmp.to_int64());
            else
                return Result(tmp.to_uint64());
        } else if constexpr (is_sc_bigint_v<Result> || is_sc_biguint_v<Result>) {
            // For wide int types, use string constructor with binary prefix
            std::string prefixed = "0b" + bits;
            return Result(prefixed.c_str());
        } else {
            return Result{};
        }
    }

} // namespace detail

} // namespace concat
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_CONCAT_UTILS_HPP