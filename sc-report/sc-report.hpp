#ifndef CC_VRWRAPPER_REPORT_HPP
#define CC_VRWRAPPER_REPORT_HPP

// ============================================================================
// cc_vrwrapper :: sc_report
// Custom SystemC report handler with:
//   - File logging (text / JSON / logfmt, NEVER with ANSI colors)
//   - Terminal logging (with ANSI colors, only if stderr is a TTY)
//   - Thread-safe writes via mutexes
//   - JSON / logfmt string escaping (valid output)
//   - Severity, msg_type, and regex filtering
//   - Statistics (counts per severity, atomic)
//   - Hierarchy info (sc_module / process name)
//   - Auto directory creation for log path
//   - Force-disable-colors flag
//   - Throw-on-error with configurable threshold
//   - File rotation by size + gzip compression of rotated files
//   - Pluggable sink architecture (terminal / file / callback / network)
//   - User-registered callbacks
//   - Log diff for regression testing
//   - Light/dark color themes
//   - Convenience macros (VR_LOG_INFO, etc.)
//
// Supports BOTH C++17 and C++20 in the same files.
//   - C++20: uses <source_location> for caller info
//   - C++17: falls back to __FILE__ / __LINE__ macros
// ============================================================================

#include "colors.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "stats.hpp"
#include "filters.hpp"
#include "formatter.hpp"
#include "sinks.hpp"
#include "diff.hpp"
#include "handler.hpp"
#include "macros.hpp"

#endif