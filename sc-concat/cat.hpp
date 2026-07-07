#ifndef CC_VRWRAPPER_CONCAT_CAT_HPP
#define CC_VRWRAPPER_CONCAT_CAT_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat :: cat.hpp
// Variadic concatenation: cat(a, b, c, ...) → sc_lv<TotalWidth>
//                       cat<Result>(a, b, c, ...) → Result
//
// Concatenation order: left-to-right = MSB-to-LSB.
// The FIRST argument becomes the MSB side of the result.
// ============================================================================

#include "utils.hpp"

namespace cc_vrwrapper {
namespace concat {

// --------------------------------------------------------------------
// cat(a, b, c, ...) — variadic; returns sc_lv<TotalWidth>
//
// Examples:
//   sc_lv<4> a = "1010";
//   sc_lv<4> b = "1100";
//   auto r = cat(a, b);  // r is sc_lv<8> = "10101100"
//
//   sc_int<8> x = 0xAB;       // 10101011
//   sc_lv<4> y = "0b1111";
//   auto r2 = cat(x, y);      // r2 is sc_lv<12> = "101010111111"
// --------------------------------------------------------------------

template <typename... Args>
auto cat(const Args&... args) -> sc_lv<detail::total_width_v<Args...>>
{
    static_assert(sizeof...(Args) > 0,
        "cat(): requires at least one argument");
    static_assert((is_sc_concatable_v<Args> && ...),
        "cat(): all arguments must be SystemC logic-like or int-like types "
        "(sc_lv, sc_bv, sc_logic, sc_int, sc_uint, sc_bigint, sc_biguint)");

    constexpr int W = detail::total_width_v<Args...>;
    static_assert(W > 0, "cat(): total width must be positive");

    std::string bits;
    bits.reserve(W);
    ((bits += detail::to_bitstring(args)), ...);
    return sc_lv<W>(bits.c_str());
}

// --------------------------------------------------------------------
// cat<Result>(a, b, c, ...) — variadic; returns user-specified Result
//
// The total width of all arguments must equal Result's width.
//
// Examples:
//   sc_bv<8> r1 = cat<sc_bv<8>>(a, b);        // X/Z bits → '0' in sc_bv
//   sc_int<16> r2 = cat<sc_int<16>>(a, b);    // result as signed int
//   sc_biguint<32> r3 = cat<sc_biguint<32>>(a, b, c, d);
// --------------------------------------------------------------------

template <typename Result, typename... Args>
Result cat(const Args&... args)
{
    static_assert(is_sc_concatable_v<Result>,
        "cat<Result>(): Result must be a SystemC logic-like or int-like type");
    static_assert(sizeof...(Args) > 0,
        "cat<Result>(): requires at least one argument");
    static_assert((is_sc_concatable_v<Args> && ...),
        "cat<Result>(): all arguments must be SystemC logic-like or int-like types");
    static_assert(detail::total_width_v<Args...> == sc_width_v<Result>,
        "cat<Result>(): total width of args must match Result's width");

    std::string bits;
    bits.reserve(sc_width_v<Result>);
    ((bits += detail::to_bitstring(args)), ...);
    return detail::make_from_bitstring<Result>(bits);
}

} // namespace concat
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_CONCAT_CAT_HPP