#include <systemc>
#include "sc-report.hpp"
// #include "sc_cast.hpp"

int sc_main(int argc, char* argv[])
{
    using namespace cc_vrwrapper;

    // ---- Configure the report handler ----
    namespace r = cc_vrwrapper::report;

    r::config::set_log_file_path("log/sim_run.log");
    // r::config::set_log_as_json(true);
    r::config::set_min_severity(sc_core::SC_INFO);   // suppress INFO
    r::config::set_force_no_color(false);                // auto-detect TTY
    r::config::set_log_to_terminal(true);
    r::config::set_log_to_file(true);
    r::config::set_max_file_size(5 * 1024 * 1024);       // 5 MB rotation
    r::config::set_compress_rotated(true);
    r::config::set_throw_on_error(false);
    r::config::set_throw_threshold(sc_core::SC_FATAL);

    // Themes
    r::colors::set_theme(r::colors::Theme::Dark);

    // Filters
    r::filters::suppress_msg_type("noisy_module");
    r::filters::add_message_regex_deny("ignore_this_pattern.*");

    // ---- Initialize ----
    r::handler::init_report_handler();

    // Optional: add a user callback
    r::handler::add_callback([](const r::formatter::ReportInfo& info) {
        if (info.severity == sc_core::SC_ERROR) {
            std::cout << "[USER HOOK] Got an error: " << info.message << "\n";
        }
        });

    // Optional: add a network sink (POSIX only)
    // r::handler::add_network_sink("127.0.0.1", 9000);

    // ---- Use logging via macros ----
    VR_LOG_INFO("sim", "Starting simulation");
    VR_LOG_WARNING("sim", "Something suspicious happened");

    // ---- Run ----
    sc_core::sc_start();

    // ---- Print statistics ----
    r::stats::print();

    // ---- Regression diff (compare against baseline) ----
    // r::diff::compare("logs/baseline.log", "logs/sim_run.log");

    r::handler::close_report_log();
    return 0;
}