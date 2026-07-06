#include <systemc.h>
#include <iostream>
#include <fstream>
#include <set>
#include <string>
#include <stdexcept>

namespace sc_report_personalized
{
    inline std::ofstream sc_report_log;
    inline bool log_as_json = false;
    inline bool throw_on_error = false;
    inline sc_severity max_severity = SC_FATAL;

    inline std::set<sc_severity> enabled_severities = {
        SC_INFO, SC_WARNING, SC_ERROR, SC_FATAL };

    std::string severity_to_string(sc_severity severity)
    {
        switch (severity)
        {
        case SC_INFO:
            return "INFO";
        case SC_WARNING:
            return "WARNING";
        case SC_ERROR:
            return "ERROR";
        case SC_FATAL:
            return "FATAL";
        default:
            return "UNKNOWN";
        }
    }

    std::string severity_to_color(sc_severity severity)
    {
        switch (severity)
        {
        case SC_INFO:
            return "\033[32m"; // Green
        case SC_WARNING:
            return "\033[33m"; // Yellow
        case SC_ERROR:
            return "\033[31m"; // Red
        case SC_FATAL:
            return "\033[41m"; // Red BG
        default:
            return "\033[0m";
        }
    }

    void personalized_report_handler(const sc_core::sc_report& rep, const sc_core::sc_actions& actions)
    {
        sc_severity severity = rep.get_severity();

        if (severity > max_severity || enabled_severities.find(severity) == enabled_severities.end())
            return;

        const std::string sev_str = severity_to_string(severity);
        const std::string timestamp = sc_time_stamp().to_string();
        const std::string msg_type = rep.get_msg_type();
        const std::string message = rep.get_msg();
        const std::string file = rep.get_file_name();
        int line = rep.get_line_number();
        const std::string proc = sc_core::sc_get_current_process_handle().name();

        // Write to file (text or JSON)
        if (sc_report_log.is_open())
        {
            if (log_as_json)
            {
                sc_report_log << "{"
                    << "\"severity\":\"" << sev_str << "\","
                    << "\"msg_type\":\"" << msg_type << "\","
                    << "\"message\":\"" << message << "\","
                    << "\"time\":\"" << timestamp << "\","
                    << "\"file\":\"" << file << "\","
                    << "\"line\":" << line << ","
                    << "\"process\":\"" << proc << "\""
                    << "}" << std::endl;
            }
            else
            {
                sc_report_log << "[" << sev_str << "] "
                    << msg_type << ": " << message
                    << " @ " << timestamp
                    << " (" << file << ":" << line << ")"
                    << " in process: " << proc
                    << std::endl;
            }
        }

        // Print to terminal with color
        std::cerr << severity_to_color(severity)
            << "[" << sev_str << "] "
            << msg_type << ": " << message
            << " @ " << timestamp
            << " (" << file << ":" << line << ")"
            << " in process: " << proc
            << "\033[0m";
        if (severity == SC_FATAL)
        {
            std::cerr << " - Simulation will terminate!" << std::endl;
        }
        else if (severity != SC_WARNING)
        {
            std::cerr << std::endl;
        }

        // Throw if configured and severity is high
        if (throw_on_error && severity >= SC_ERROR)
        {
            throw std::runtime_error(message);
        }

        // Still pass it to default handler
        sc_core::sc_report_handler::default_handler(rep, actions);
    }

    // Call once at beginning
    inline void init_report_handler(const std::string& path = "logs/systemc_report.log")
    {
        sc_report_log.open(path);
        if (!sc_report_log.is_open())
        {
            SC_REPORT_ERROR("init_report_handler", "Failed to open log file");
        }

        sc_core::sc_report_handler::set_handler(personalized_report_handler);
    }

    inline void close_report_log()
    {
        if (sc_report_log.is_open())
            sc_report_log.close();
    }

    inline void set_enabled_severities(std::initializer_list<sc_severity> severities)
    {
        enabled_severities = severities;
    }

    inline void set_max_severity(sc_severity severity)
    {
        max_severity = severity;
    }

    inline void set_log_as_json(bool flag)
    {
        log_as_json = flag;
    }

    inline void set_throw_on_error(bool flag)
    {
        throw_on_error = flag;
    }
}
