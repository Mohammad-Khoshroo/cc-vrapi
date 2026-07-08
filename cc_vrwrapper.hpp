#ifndef CC_VRWRAPPER_HPP
#define CC_VRWRAPPER_HPP

// ============================================================================
// cc_vrwrapper.hpp — Umbrella header for the cc_vrwrapper library
//
// A collection of utilities for SystemC-based hardware verification:
//
//   csv-parser   — CSV parsing (third-party, namespace ::csv)
//   sc-cast      — Safe casts between SystemC data types
//                  (namespace cc_vrwrapper)
//   sc-concat    — Concat / slice / repeat / reverse / extend
//                  (namespace cc_vrwrapper::concat)
//   sc-report    — Enhanced SC_REPORT with colors, filters, diffing
//                  (namespace cc_vrwrapper::report)
//   sc-trace     — Signal watching, VCD/JSON tracing, delta-cycle
//                  (namespace cc_vrwrapper::trace)
//
// Usage:
//   #include "cc_vrwrapper.hpp"
//   namespace ccv = cc_vrwrapper;
//
//   ccv::trace::watch_rising(clk, [](){ ... });
//   ccv::cast::to_uint64(val);            // cast is directly in cc_vrwrapper
//   ccv::concat::cat<8>(a, b);
//   ccv::report::info("message");
//   csv::CSVReader reader("file.csv");    // third-party, keeps its own ns
//
// Configuration macros (define BEFORE including this header):
//   CCVR_NO_CSV      — exclude csv-parser
//   CCVR_NO_CAST     — exclude sc-cast
//   CCVR_NO_CONCAT   — exclude sc-concat
//   CCVR_NO_REPORT   — exclude sc-report
//   CCVR_NO_TRACE    — exclude sc-trace
//
// Requires:
//   - C++17 or C++20 (both supported)
//   - SystemC 3.0+  (not needed for csv-parser alone)
//   - zlib (only if sc-trace compression is used)
// ============================================================================

// ----------------------------------------------------------------------------
// Version info
// ----------------------------------------------------------------------------
#define CCVR_VERSION_MAJOR 1
#define CCVR_VERSION_MINOR 0
#define CCVR_VERSION_PATCH 0
#define CCVR_VERSION "1.0.0"

// ----------------------------------------------------------------------------
// C++ standard detection
// ----------------------------------------------------------------------------
#if __cplusplus >= 202002L
    #define CCVR_CPP20 1
    #define CCVR_CPP_VERSION 20
#elif __cplusplus >= 201703L
    #define CCVR_CPP20 0
    #define CCVR_CPP_VERSION 17
#else
    #error "cc_vrwrapper requires C++17 or later (__cplusplus >= 201703L)"
#endif

// ----------------------------------------------------------------------------
// SystemC dependency — required by all sub-libraries except csv-parser
// ----------------------------------------------------------------------------
#if !defined(CCVR_NO_CAST)   || \
    !defined(CCVR_NO_CONCAT) || \
    !defined(CCVR_NO_REPORT) || \
    !defined(CCVR_NO_TRACE)
    #ifndef SC_INCLUDE_DYNAMIC_PROCESSES
        #define SC_INCLUDE_DYNAMIC_PROCESSES
    #endif
    #include <systemc>
#endif

// ============================================================================
// 1. csv-parser  (third-party, namespace ::csv)
// ============================================================================
#ifndef CCVR_NO_CSV
    #include "csv-parser/csv.hpp"
    // csv-parser keeps its own `csv` namespace — intentionally NOT aliased
    // into cc_vrwrapper, so upstream updates stay drop-in compatible.
    #define CCVR_HAS_CSV 1
#else
    #define CCVR_HAS_CSV 0
#endif

// ============================================================================
// 2. sc-cast  (namespace cc_vrwrapper — directly, no sub-namespace)
// ============================================================================
#ifndef CCVR_NO_CAST
    #if CCVR_CPP20
        #include "sc-cast/sc-cast-c++20/sc_cast.hpp"
    #else
        #include "sc-cast/sc-cast-c++17/sc_cast.hpp"
    #endif
    // sc-cast declares its API directly in cc_vrwrapper — nothing to do.
    #define CCVR_HAS_CAST 1
#else
    #define CCVR_HAS_CAST 0
#endif

// ============================================================================
// 3. sc-concat  (namespace cc_vrwrapper::concat)
// ============================================================================
#ifndef CCVR_NO_CONCAT
    #include "sc-concat/sc_concat.hpp"
    // sc-concat declares its API in cc_vrwrapper::concat — nothing to do.
    #define CCVR_HAS_CONCAT 1
#else
    #define CCVR_HAS_CONCAT 0
#endif

// ============================================================================
// 4. sc-report  (namespace cc_vrwrapper::report)
// ============================================================================
#ifndef CCVR_NO_REPORT
    #include "sc-report/sc-report.hpp"
    // sc-report declares its API in cc_vrwrapper::report — nothing to do.
    #define CCVR_HAS_REPORT 1
#else
    #define CCVR_HAS_REPORT 0
#endif

// ============================================================================
// 5. sc-trace  (namespace cc_vrwrapper::trace)
// ============================================================================
#ifndef CCVR_NO_TRACE
    #include "sc-trace/sc_trace.hpp"
    // sc-trace declares its API in cc_vrwrapper::trace — nothing to do.
    #define CCVR_HAS_TRACE 1
#else
    #define CCVR_HAS_TRACE 0
#endif

// ============================================================================
// Namespace alias for convenience
// ============================================================================
namespace ccvr = cc_vrwrapper;

// ============================================================================
// Compile-time build info (only emitted when CC_VRWRAPPER_DEBUG is defined)
// ============================================================================
#ifdef CC_VRWRAPPER_DEBUG
    #include <iostream>
    namespace cc_vrwrapper {
    inline void print_build_info() {
        std::cout << "cc_vrwrapper " << CCVR_VERSION
                  << " (C++" << CC_VRWRAPPER_CPP_VERSION << ")\n"
                  << "  csv-parser : " << (CCVR_HAS_CSV    ? "yes" : "no") << "\n"
                  << "  sc-cast    : " << (CCVR_HAS_CAST   ? "yes" : "no") << "\n"
                  << "  sc-concat  : " << (CCVR_HAS_CONCAT ? "yes" : "no") << "\n"
                  << "  sc-report  : " << (CCVR_HAS_REPORT ? "yes" : "no") << "\n"
                  << "  sc-trace   : " << (CCVR_HAS_TRACE  ? "yes" : "no") << "\n";
    }
    } // namespace cc_vrwrapper
#endif

#endif // CC_VRWRAPPER_HPP