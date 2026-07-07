#ifndef SC_CAST_HPP
#define SC_CAST_HPP

// ============================================================================
// sc_cast :: master header  (C++20)
// Includes all conversion modules.
// Compile with: -std=c++20  (or /std:c++20 on MSVC)
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
//  string_cast<DestType>(str, mode, base)
//
//  Supported target types (concepts):
//    - ScLv        : sc_lv<W>         (logic vector: 0/1/X/Z)
//    - ScBv        : sc_bv<W>         (bit vector: 0/1 only)
//    - ScLogic     : sc_logic         (single bit: 0/1/X/Z)
//    - ScInt       : sc_int<W>        (signed fixed-width integer)
//    - ScUint      : sc_uint<W>       (unsigned fixed-width integer)
//    - ScBigInt    : sc_bigint<W>     (signed arbitrary-precision integer)
//    - ScBigUint   : sc_biguint<W>    (unsigned arbitrary-precision integer)
//    - bool / char / short / int / long / long long (and unsigned variants)
//    - float / double
//    - std::string
//
//  Source types:
//    - Any of the above SystemC types
//    - bool and all C++ integral types
//    - float / double
//    - std::string / std::string_view  (modes: "data" or "address")
//
//  C++20 features used:
//    - Concepts (replaces enable_if)
//    - std::bit_cast (replaces union/memcpy type punning)
//    - std::ranges::any_of / std::erase_if
//    - std::optional (clean parse success/failure)
//    - std::format (type-safe message construction)
//
// ----------------------------------------------------------------------------

#endif // SC_CAST_HPP