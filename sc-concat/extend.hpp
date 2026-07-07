#ifndef CC_VRWRAPPER_CONCAT_EXTEND_HPP
#define CC_VRWRAPPER_CONCAT_EXTEND_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat :: extend.hpp
// Width extension: zero_extend<W>(v), sign_extend<W>(v)
// ============================================================================

#include "utils.hpp"

namespace cc_vrwrapper {
namespace concat {

// --------------------------------------------------------------------
// zero_extend<W>(v) — pad v with zeros on the left to width W
//
// Example:
//   sc_lv<4> v = "1010";
//   auto r = zero_extend<8>(v);  // sc_lv<8> = "00001010"
//
// Useful for: converting small unsigned values to wider widths,
// padding addresses, etc.
// --------------------------------------------------------------------

template <int W, typename T>
auto zero_extend(const T& v) -> sc_lv<W>
{
    static_assert(is_sc_concatable_v<T>,
        "zero_extend(): T must be a SystemC logic-like or int-like type");

    constexpr int SRC_W = sc_width_v<T>;
    static_assert(W > 0, "zero_extend<W>(): W must be > 0");
    static_assert(W >= SRC_W, "zero_extend<W>(): W must be >= source width");

    std::string bits = detail::to_bitstring(v);
    bits = std::string(W - SRC_W, '0') + bits;
    return sc_lv<W>(bits.c_str());
}

// --------------------------------------------------------------------
// sign_extend<W>(v) — pad v with its MSB on the left to width W
//
// The MSB of v is used as the fill bit. For X/Z MSB, fills with that
// X/Z value (which preserves the unknown state).
//
// Example:
//   sc_lv<4> v = "1010";   // MSB is '1' (signed: negative)
//   auto r = sign_extend<8>(v);  // sc_lv<8> = "11111010"
//
//   sc_lv<4> u = "0101";   // MSB is '0' (signed: positive)
//   auto r2 = sign_extend<8>(u);  // sc_lv<8> = "00000101"
// --------------------------------------------------------------------

template <int W, typename T>
auto sign_extend(const T& v) -> sc_lv<W>
{
    static_assert(is_sc_concatable_v<T>,
        "sign_extend(): T must be a SystemC logic-like or int-like type");

    constexpr int SRC_W = sc_width_v<T>;
    static_assert(W > 0, "sign_extend<W>(): W must be > 0");
    static_assert(W >= SRC_W, "sign_extend<W>(): W must be >= source width");

    std::string bits = detail::to_bitstring(v);
    char sign = bits.empty() ? '0' : bits[0];
    bits = std::string(W - SRC_W, sign) + bits;
    return sc_lv<W>(bits.c_str());
}

} // namespace concat
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_CONCAT_EXTEND_HPP