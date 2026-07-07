#ifndef CC_VRWRAPPER_SC_CONCAT_HPP
#define CC_VRWRAPPER_SC_CONCAT_HPP

// ============================================================================
// cc_vrwrapper :: sc-concat
// Bit-manipulation utilities for SystemC types:
//   - cat(a, b, c, ...)       — variadic concatenation
//   - cat<Result>(a, b, ...)  — concatenation with explicit return type
//   - slice<HI, LO>(v)        — bit slicing (extract bits [HI:LO])
//   - msb<W>(v), lsb<W>(v)    — top/bottom W bits
//   - repeat<N>(v)            — pattern repetition
//   - reverse(v)              — bit order reversal
//   - zero_extend<W>(v)       — zero-extension to width W
//   - sign_extend<W>(v)       — sign-extension to width W
//
// Supported input types: sc_lv, sc_bv, sc_logic, sc_int, sc_uint,
//                        sc_bigint, sc_biguint
//
// Default return type: sc_lv<W> (preserves X/Z bits).
// Use cat<Result>(...) to convert to other types.
//
// Supports BOTH C++17 and C++20 in the same files.
//   - C++20: uses concepts for documentation (when available)
//   - C++17: uses static_assert for type checking
//
// Namespace: cc_vrwrapper::concat
// Usage:
//   using namespace cc_vrwrapper::concat;
//   auto r = cat(a, b, c);
// ============================================================================

#include "utils.hpp"
#include "cat.hpp"
#include "slice.hpp"
#include "repeat.hpp"
#include "reverse.hpp"
#include "extend.hpp"

#endif // CC_VRWRAPPER_SC_CONCAT_HPP