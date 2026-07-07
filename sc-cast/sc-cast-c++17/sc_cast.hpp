#ifndef SC_CAST_HPP
#define SC_CAST_HPP

// ============================================================================
// sc_cast :: master header
// Includes all conversion modules.
// Include this single file in your project to get all sc_cast utilities.
// ============================================================================

#include "utils.hpp"
#include "sc_lv.hpp"
#include "sc_bv.hpp"
#include "sc_logic.hpp"
#include "sc_int.hpp"

// ----------------------------------------------------------------------------
// Summary of available conversions
// ----------------------------------------------------------------------------
//
//  sc_lv_cast<DestType>(source, ...)
//
//  Supported target types:
//    - sc_lv<W>         (logic vector: 0/1/X/Z)
//    - sc_bv<W>         (bit vector: 0/1 only)
//    - sc_logic         (single bit: 0/1/X/Z)
//    - sc_int<W>        (signed fixed-width integer)
//    - sc_uint<W>       (unsigned fixed-width integer)
//    - sc_bigint<W>     (signed arbitrary-precision integer)
//    - sc_biguint<W>    (unsigned arbitrary-precision integer)
//    - bool, char, short, int, long, long long
//    - unsigned char, unsigned short, unsigned int, ...
//    - float, double
//    - std::string
//
//  Source types:
//    - Any of the above SystemC types
//    - bool and all C++ integral types
//    - float / double
//    - std::string / std::string_view  (modes: "data" or "address")
//
//  Generic helper:
//    string_cast<T>(str, mode, base)   — string → any T
//
// ----------------------------------------------------------------------------

#endif // SC_CAST_HPP