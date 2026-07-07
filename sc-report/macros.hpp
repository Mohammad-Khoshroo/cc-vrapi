#ifndef CC_VRWRAPPER_REPORT_MACROS_HPP
#define CC_VRWRAPPER_REPORT_MACROS_HPP

#include <systemc>
#include "utils.hpp"

// Convenience macros — capture file/line/function automatically
// Uses SC_REPORT_* internally so the custom handler picks them up.

#define VR_LOG_INFO(type, msg) \
    do { SC_REPORT_INFO(type, msg); } while(0)

#define VR_LOG_WARNING(type, msg) \
    do { SC_REPORT_WARNING(type, msg); } while(0)

#define VR_LOG_ERROR(type, msg) \
    do { SC_REPORT_ERROR(type, msg); } while(0)

#define VR_LOG_FATAL(type, msg) \
    do { SC_REPORT_FATAL(type, msg); } while(0)

// Convenience: auto file/line tagging (for users who want extra detail)
#define VR_LOG_INFO_LOC(type, msg) \
    do { \
        std::string _full = std::string(msg) + " [" + __FILE__ + ":" + \
                            std::to_string(__LINE__) + "]"; \
        SC_REPORT_INFO(type, _full.c_str()); \
    } while(0)

#define VR_LOG_WARNING_LOC(type, msg) \
    do { \
        std::string _full = std::string(msg) + " [" + __FILE__ + ":" + \
                            std::to_string(__LINE__) + "]"; \
        SC_REPORT_WARNING(type, _full.c_str()); \
    } while(0)

#define VR_LOG_ERROR_LOC(type, msg) \
    do { \
        std::string _full = std::string(msg) + " [" + __FILE__ + ":" + \
                            std::to_string(__LINE__) + "]"; \
        SC_REPORT_ERROR(type, _full.c_str()); \
    } while(0)

#endif