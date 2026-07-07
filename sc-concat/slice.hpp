#ifndef CC_VRWRAPPER_CONCAT_SLICE_HPP
#define CC_VRWRAPPER_CONCAT_SLICE_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat :: slice.hpp
// Bit slicing: slice<HI, LO>(value) → sc_lv<HI-LO+1>
//
// Bit numbering: bit 0 = LSB, bit W-1 = MSB.
// slice<HI, LO> extracts bits HI down to LO (inclusive).
// ============================================================================

#include "utils.hpp"

namespace cc_vrwrapper {
namespace concat {

// --------------------------------------------------------------------
// slice<HI, LO>(v) — extract bits [HI:LO] from v
//
// Example:
//   sc_lv<8> v = "10110100";   // bit 7=1, bit 6=0, ..., bit 0=0
//   auto hi  = slice<7, 4>(v);  // sc_lv<4> = "1011"   (bits 7,6,5,4)
//   auto lo  = slice<3, 0>(v);  // sc_lv<4> = "0100"   (bits 3,2,1,0)
//   auto mid = slice<5, 2>(v);  // sc_lv<4> = "1101"   (bits 5,4,3,2)
// --------------------------------------------------------------------

template <int HI, int LO, typename T>
auto slice(const T& v) -> sc_lv<HI - LO + 1>
{
    static_assert(is_sc_concatable_v<T>,
        "slice(): T must be a SystemC logic-like or int-like type");
    static_assert(HI >= LO, "slice<HI, LO>: HI must be >= LO");
    static_assert(LO >= 0,  "slice<HI, LO>: LO must be >= 0");

    constexpr int W = sc_width_v<T>;
    static_assert(HI < W, "slice<HI, LO>: HI out of range (must be < source width)");

    constexpr int RESULT_W = HI - LO + 1;

    std::string bits = detail::to_bitstring(v);  // MSB-first, length W
    // Bit N is at string position (W-1-N).
    // Bit HI is at position (W-1-HI); bit LO is at position (W-1-LO).
    // We want substring starting at (W-1-HI) with length (HI-LO+1).
    std::string sub = bits.substr(W - 1 - HI, RESULT_W);
    return sc_lv<RESULT_W>(sub.c_str());
}

// --------------------------------------------------------------------
// msb<W>(v) — top W bits of v (most significant)
//
// Example:
//   sc_lv<8> v = "10110100";
//   auto top = msb<4>(v);  // sc_lv<4> = "1011"
// --------------------------------------------------------------------

template <int W, typename T>
auto msb(const T& v) -> sc_lv<W>
{
    static_assert(is_sc_concatable_v<T>,
        "msb(): T must be a SystemC logic-like or int-like type");

    constexpr int SRC_W = sc_width_v<T>;
    static_assert(W > 0, "msb<W>(): W must be > 0");
    static_assert(W <= SRC_W, "msb<W>(): W must be <= source width");

    return slice<SRC_W - 1, SRC_W - W>(v);
}

// --------------------------------------------------------------------
// lsb<W>(v) — bottom W bits of v (least significant)
//
// Example:
//   sc_lv<8> v = "10110100";
//   auto bot = lsb<4>(v);  // sc_lv<4> = "0100"
// --------------------------------------------------------------------

template <int W, typename T>
auto lsb(const T& v) -> sc_lv<W>
{
    static_assert(is_sc_concatable_v<T>,
        "lsb(): T must be a SystemC logic-like or int-like type");

    constexpr int SRC_W = sc_width_v<T>;
    static_assert(W > 0, "lsb<W>(): W must be > 0");
    static_assert(W <= SRC_W, "lsb<W>(): W must be <= source width");

    return slice<W - 1, 0>(v);
}

} // namespace concat
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_CONCAT_SLICE_HPP