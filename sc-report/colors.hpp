#ifndef CC_VRWRAPPER_REPORT_COLORS_HPP
#define CC_VRWRAPPER_REPORT_COLORS_HPP

#include <string>
#include <atomic>
#include <systemc>

namespace cc_vrwrapper::report::colors
{

// ------------------------------------------------------------------------
// ANSI escape codes
// ------------------------------------------------------------------------
inline constexpr const char* RESET   = "\033[0m";
inline constexpr const char* BOLD    = "\033[1m";
inline constexpr const char* DIM     = "\033[2m";
inline constexpr const char* RED     = "\033[31m";
inline constexpr const char* GREEN   = "\033[32m";
inline constexpr const char* YELLOW  = "\033[33m";
inline constexpr const char* BLUE    = "\033[34m";
inline constexpr const char* MAGENTA = "\033[35m";
inline constexpr const char* CYAN    = "\033[36m";
inline constexpr const char* WHITE   = "\033[37m";
inline constexpr const char* RED_BG  = "\033[41m";

// ------------------------------------------------------------------------
// Theme: maps severity → (foreground, background, bold?)
// ------------------------------------------------------------------------
enum class Theme { Dark, Light };

// CHANGED: made atomic — set_theme() and scheme_for() may be called from
// different threads (e.g. config thread vs. report handler thread).
inline std::atomic<Theme> current_theme{Theme::Dark};

inline void set_theme(Theme t) { current_theme.store(t); }

struct ColorScheme {
    const char* color;
    const char* bg;
    bool        bold;
};

inline ColorScheme scheme_for(sc_core::sc_severity s)
{
    // CHANGED: use .load() for explicit atomic read
    if (current_theme.load() == Theme::Dark) {
        switch (s) {
            case sc_core::SC_INFO:    return { GREEN,   nullptr, false };
            case sc_core::SC_WARNING: return { YELLOW,  nullptr, true  };
            case sc_core::SC_ERROR:   return { RED,     nullptr, true  };
            case sc_core::SC_FATAL:   return { WHITE,   RED_BG,  true  };
            default:                  return { nullptr, nullptr, false };
        }
    } else {
        // Light theme: darker colors for readability on white bg
        switch (s) {
            case sc_core::SC_INFO:    return { BLUE,    nullptr, false };
            case sc_core::SC_WARNING: return { MAGENTA, nullptr, true  };
            case sc_core::SC_ERROR:   return { RED,     nullptr, true  };
            case sc_core::SC_FATAL:   return { WHITE,   RED_BG,  true  };
            default:                  return { nullptr, nullptr, false };
        }
    }
}

// Build the full ANSI prefix for a severity.
inline std::string ansi_prefix(sc_core::sc_severity s)
{
    ColorScheme cs = scheme_for(s);
    std::string out;
    if (cs.bold) out += BOLD;
    if (cs.color) out += cs.color;
    if (cs.bg)    out += cs.bg;
    return out;
}

} // namespace cc_vrwrapper::report::colors

#endif