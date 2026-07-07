#ifndef CC_VRWRAPPER_REPORT_STATS_HPP
#define CC_VRWRAPPER_REPORT_STATS_HPP

#include <atomic>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <systemc>

namespace cc_vrwrapper::report::stats
{

struct ReportStats {
    std::atomic<std::uint64_t> info    {0};
    std::atomic<std::uint64_t> warning {0};
    std::atomic<std::uint64_t> error   {0};
    std::atomic<std::uint64_t> fatal   {0};
    std::atomic<std::uint64_t> total   {0};
};

inline ReportStats g_stats;
inline std::mutex   stats_print_mutex;

inline void bump(sc_core::sc_severity s)
{
    g_stats.total++;
    switch (s) {
        case sc_core::SC_INFO:    g_stats.info++;    break;
        case sc_core::SC_WARNING: g_stats.warning++; break;
        case sc_core::SC_ERROR:   g_stats.error++;   break;
        case sc_core::SC_FATAL:   g_stats.fatal++;   break;
        default: break;
    }
}

inline void reset()
{
    g_stats.info.store(0);
    g_stats.warning.store(0);
    g_stats.error.store(0);
    g_stats.fatal.store(0);
    g_stats.total.store(0);
}

inline const ReportStats& get() { return g_stats; }

inline void print(std::ostream& os = std::cerr)
{
    std::lock_guard<std::mutex> g(stats_print_mutex);
    os << "========================================\n"
       << "Report Statistics\n"
       << "========================================\n"
       << "  INFO    : " << g_stats.info    << "\n"
       << "  WARNING : " << g_stats.warning << "\n"
       << "  ERROR   : " << g_stats.error   << "\n"
       << "  FATAL   : " << g_stats.fatal   << "\n"
       << "  TOTAL   : " << g_stats.total   << "\n"
       << "========================================\n";
}

} // namespace cc_vrwrapper::report::stats

#endif