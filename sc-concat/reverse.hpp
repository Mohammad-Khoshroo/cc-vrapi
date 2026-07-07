#ifndef CC_VRWRAPPER_CONCAT_REVERSE_HPP
#define CC_VRWRAPPER_CONCAT_REVERSE_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat :: reverse.hpp
// Bit order reversal: reverse(v) → sc_lv<W> with bits in reverse order
// ============================================================================

#include "utils.hpp"
#include <algorithm>

namespace cc_vrwrapper {
namespace concat {

// --------------------------------------------------------------------
// reverse(v) — reverse bit order of v
//
// Bit 0 (LSB) becomes bit W-1 (MSB) and vice versa.
//
// Example:
//   sc_lv<8> v = "10110100";
//   auto r = reverse(v);  // sc_lv<8> = "00101101"
//
// Note: this reverses the BIT ORDER, not the byte order.
// --------------------------------------------------------------------

template <typename T>
auto reverse(const T& v) -> sc_lv<sc_width_v<T>>
{
    static_assert(is_sc_concatable_v<T>,
        "reverse(): T must be a SystemC logic-like or int-like type");

    constexpr int W = sc_width_v<T>;
    std::string bits = detail::to_bitstring(v);
    std::reverse(bits.begin(), bits.end());
    return sc_lv<W>(bits.c_str());
}

} // namespace concat
} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_CONCAT_REVERSE_HPP