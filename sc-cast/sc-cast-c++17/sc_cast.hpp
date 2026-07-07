#ifndef CC_VRWRAPPER_CAST_HPP
#define CC_VRWRAPPER_CAST_HPP

// ============================================================================
// cc_vrwrapper :: master header  (C++17)
// Includes all conversion modules.
// Compile with: -std=c++17
// ============================================================================

#include "utils.hpp"
#include "sc_lv.hpp"
#include "sc_bv.hpp"
#include "sc_logic.hpp"
#include "sc_int.hpp"

// ----------------------------------------------------------------------------
// Summary
// ----------------------------------------------------------------------------
//
//  cc_vrwrapper::sc_cast<DestType>(source, ...)
//  cc_vrwrapper::string_cast<DestType>(str, mode, base)
//
//  Usage:
//    using namespace cc_vrwrapper;
//    sc_lv<8> lv = sc_cast<sc_lv<8>>(42);
//    int v       = sc_cast<int>(lv);
//
//  Supported types:
//    - sc_lv<W>, sc_bv<W>, sc_logic
//    - sc_int<W>, sc_uint<W>, sc_bigint<W>, sc_biguint<W>
//    - bool, integral types, float, double, std::string
//
// ----------------------------------------------------------------------------

#endif // CC_VRWRAPPER_CAST_HPP