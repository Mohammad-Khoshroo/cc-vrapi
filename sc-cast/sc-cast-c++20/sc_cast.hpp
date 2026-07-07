#ifndef CC_VRWRAPPER_CAST_HPP
#define CC_VRWRAPPER_CAST_HPP

// ============================================================================
// cc_vrwrapper :: master header  (C++20)
// Includes all conversion modules.
// Compile with: -std=c++20  (or /std:c++20 on MSVC)
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
//    sc_int<8> i = sc_cast<sc_int<8>>(-5);
//    sc_bv<16> b = sc_cast<sc_bv<16>>("0xCAFE");
//
//  Supported types (concepts):
//    - ScLv, ScBv, ScLogic
//    - ScInt, ScUint, ScBigInt, ScBigUint
//    - bool, integral types, float, double, std::string
//
//  C++20 features used:
//    - Concepts (replaces enable_if)
//    - std::bit_cast (replaces union/memcpy type punning)
//    - std::ranges::any_of / std::erase_if
//    - std::optional (clean parse success/failure)
//    - std::format (type-safe message construction)
//
// ----------------------------------------------------------------------------

#endif // CC_VRWRAPPER_CAST_HPP