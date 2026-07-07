#ifndef CC_VRWRAPPER_CONCAT_REPEAT_HPP
#define CC_VRWRAPPER_CONCAT_REPEAT_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat :: repeat.hpp
// Pattern repetition: repeat<N>(pattern) → sc_lv<N * pattern_width>
// ============================================================================

#include "utils.hpp"

namespace cc_vrwrapper {
namespace concat {

// --------------------------------------------------------------------
// repeat<N>(v) — repeat v's bit pattern N times
//
// Example:
//   sc_lv<4> p = "1010";
//   auto r = repeat<4>(p);  // sc_lv<16> = "1010101010101010"
//
//   sc_logic one(SC_LOGIC_1);
//   auto r2 = repeat<8>(one);  // sc_lv<8> = "11111111"
//
//   sc_int<4> neg(-1);  // 1111
//   auto r3 = repeat<2>(neg);  // sc_lv<8> = "11111111"
// --------------------------------------------------------------------

template <int N, typename T>
auto repeat(const T& v) -> sc_lv<N * sc_width_v<T>>
{
    static_assert(is_sc_concatable_v<T>,
        "repeat(): T must be a SystemC logic-like or int-like type");
    static_assert(N > 0, "repeat<N>(): N must be > 0");

    constexpr int W = sc_width_v<T>;
    constexpr int RESULT_W = N * W;

    std::string pattern = detail::to_bitstring(v);
    std::string bits;
    bits.reserve(RESULT_W);
    for (int i = 0; i < N; ++i) {
        bits += pattern;
    }
    return sc_lv<RESULT_W>(bits.c_str());
}

} // namespace concat
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_CONCAT_REPEAT_HPP