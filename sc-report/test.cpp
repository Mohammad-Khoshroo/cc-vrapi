// ============================================================================
// test_sc_report.cpp — Comprehensive test for sc_report_personalized library
//
// Compile:
//   g++ -std=c++20 -I./sc_report test_sc_report.cpp -lsystemc -lpthread -o test
//   g++ -std=c++17 -I./sc_report test_sc_report.cpp -lsystemc -lpthread -o test
//
// Usage:
//   - Default run: all "safe" tests pass, error-generating ones are commented out.
//   - To test error paths: uncomment blocks marked [ERROR-GENERATING].
// ============================================================================

#include <systemc>
#include "sc-report.hpp"
#include "sc-cast/sc-cast-c++20/sc_cast.hpp"
#include <iostream>
#include <fstream>
#include <thread>
#include <vector>
#include <chrono>
#include <filesystem>

using namespace sc_core;
using namespace cc_vrwrapper::report;
namespace rpt = cc_vrwrapper::report;

// ============================================================================
// Test framework
// ============================================================================
static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond) do { \
    if (cond) { std::cout << "  [PASS] " << #cond << "\n"; g_pass++; } \
    else { std::cout << "  [FAIL] " << #cond << " | line " << __LINE__ << "\n"; g_fail++; } \
} while(0)

#define CHECK_EQ(a, b) do { \
    if ((a) == (b)) { std::cout << "  [PASS] " << #a << " == " << (b) << "\n"; g_pass++; } \
    else { std::cout << "  [FAIL] " << #a << " == " << #b \
                     << " | got: " << (a) << " | exp: " << (b) \
                     << " | line " << __LINE__ << "\n"; g_fail++; } \
} while(0)

#define INFO_MSG(msg) std::cout << "  [INFO] " << msg << "\n"

// ============================================================================
// Helper: reset all global state between tests (prevents state leakage)
// ============================================================================
static void reset_all_state()
{
    // Config
    rpt::config::set_log_as_json(false);
    rpt::config::set_log_as_logfmt(false);
    rpt::config::set_throw_on_error(false);
    rpt::config::set_force_no_color(true);
    rpt::config::set_log_to_terminal(true);
    rpt::config::set_log_to_file(true);
    rpt::config::set_min_severity(SC_INFO);
    rpt::config::set_max_file_size(10 * 1024 * 1024);
    rpt::config::set_compress_rotated(true);
    rpt::config::set_capture_hierarchy(true);

    // Filters
    rpt::filters::enabled_severities = { SC_INFO, SC_WARNING, SC_ERROR, SC_FATAL };
    rpt::filters::suppressed_msg_types.clear();
    rpt::filters::allowed_msg_types.clear();
    rpt::filters::message_regex_allow.clear();
    rpt::filters::message_regex_deny.clear();

    // Stats
    rpt::stats::reset();
}

// // ============================================================================
// // Test 1: Basic text logging
// // ============================================================================
// void test_basic_text_logging()
// {
//     std::cout << "\n=== Test 1: Basic text logging ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_basic.log");
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("test1", "Hello from test 1");
//     SC_REPORT_WARNING("test1", "A warning message");
//     SC_REPORT_ERROR("test1", "An error message");

//     rpt::handler::close_report_log();

//     // Verify file exists and has content
//     std::ifstream f("logs/test_basic.log");
//     CHECK(f.good());
//     std::string line;
//     int line_count = 0;
//     while (std::getline(f, line)) line_count++;
//     CHECK_EQ(line_count, 3);

//     INFO_MSG("Log file created at: logs/test_basic.log");
// }

// // ============================================================================
// // Test 2: JSON logging format
// // ============================================================================
// void test_json_logging()
// {
//     std::cout << "\n=== Test 2: JSON logging ===\n";

//     reset_all_state();
//     rpt::config::set_log_as_json(true);
//     rpt::config::set_log_file_path("logs/test_json.log");
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("json_test", "Simple message");
//     SC_REPORT_WARNING("json_test", "Message with \"quotes\" and \\backslash\\");
//     SC_REPORT_ERROR("json_test", "Message with\nnewline and\ttab");

//     rpt::handler::close_report_log();
//     rpt::config::set_log_as_json(false);

//     // Verify file content is valid JSON (basic check)
//     std::ifstream f("logs/test_json.log");
//     CHECK(f.good());
//     std::string line;
//     int json_lines = 0;
//     while (std::getline(f, line)) {
//         if (!line.empty() && line.front() == '{' && line.back() == '}') json_lines++;
//     }
//     CHECK_EQ(json_lines, 3);
//     INFO_MSG("JSON log created with proper escaping");
// }

// // ============================================================================
// // Test 3: Logfmt logging format
// // ============================================================================
// void test_logfmt_logging()
// {
//     std::cout << "\n=== Test 3: Logfmt logging ===\n";

//     reset_all_state();
//     rpt::config::set_log_as_logfmt(true);
//     rpt::config::set_log_file_path("logs/test_logfmt.log");
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("logfmt_test", "simple");
//     SC_REPORT_WARNING("logfmt_test", "value with spaces and = sign");

//     rpt::handler::close_report_log();
//     rpt::config::set_log_as_logfmt(false);

//     std::ifstream f("logs/test_logfmt.log");
//     CHECK(f.good());
//     std::string line;
//     std::getline(f, line);
//     CHECK(line.find("severity=info") != std::string::npos);
//     CHECK(line.find("msg_type=logfmt_test") != std::string::npos);
// }

// // ============================================================================
// // Test 4: Severity filtering
// // ============================================================================
// void test_severity_filtering()
// {
//     std::cout << "\n=== Test 4: Severity filtering ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_severity.log");
//     rpt::config::set_min_severity(SC_WARNING);  // suppress INFO
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("filter", "This should be suppressed");
//     SC_REPORT_WARNING("filter", "This should appear");
//     SC_REPORT_ERROR("filter", "This should appear");

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 0u);     // suppressed
//     CHECK_EQ(s.warning.load(), 1u);
//     CHECK_EQ(s.error.load(), 1u);
//     // reset handled by reset_all_state() in next test
// }

// // ============================================================================
// // Test 5: Message type filtering
// // ============================================================================
// void test_msg_type_filtering()
// {
//     std::cout << "\n=== Test 5: Message type filtering ===\n";

//     reset_all_state();
//     rpt::filters::suppress_msg_type("noisy_type");
//     rpt::config::set_log_file_path("logs/test_msgtype.log");
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("noisy_type", "Should be suppressed");
//     SC_REPORT_INFO("clean_type", "Should appear");
//     SC_REPORT_WARNING("noisy_type", "Also suppressed");

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 1u);     // only "clean_type"
//     CHECK_EQ(s.warning.load(), 0u);  // suppressed
// }

// // ============================================================================
// // Test 6: Regex filtering
// // ============================================================================
// void test_regex_filtering()
// {
//     std::cout << "\n=== Test 6: Regex filtering ===\n";

//     reset_all_state();
//     rpt::filters::add_message_regex_deny("password=\\w+");
//     rpt::config::set_log_file_path("logs/test_regex.log");
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("regex", "Normal message");
//     SC_REPORT_INFO("regex", "Login with password=secret123");
//     SC_REPORT_WARNING("regex", "Another password=admin here");

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 1u);     // only the normal one
//     CHECK_EQ(s.warning.load(), 0u);  // denied
// }

// // ============================================================================
// // Test 7: Statistics
// // ============================================================================
// void test_statistics()
// {
//     std::cout << "\n=== Test 7: Statistics ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_stats.log");
//     rpt::handler::init_report_handler();

//     for (int i = 0; i < 5; ++i) SC_REPORT_INFO("stats", "info");
//     for (int i = 0; i < 3; ++i) SC_REPORT_WARNING("stats", "warn");
//     for (int i = 0; i < 2; ++i) SC_REPORT_ERROR("stats", "err");

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 5u);
//     CHECK_EQ(s.warning.load(), 3u);
//     CHECK_EQ(s.error.load(), 2u);
//     CHECK_EQ(s.fatal.load(), 0u);
//     CHECK_EQ(s.total.load(), 10u);

//     std::cout << "  --- Stats printout: ---\n";
//     rpt::stats::print();
// }

// // ============================================================================
// // Test 8: Callback sink
// // ============================================================================
// void test_callback_sink()
// {
//     std::cout << "\n=== Test 8: Callback sink ===\n";

//     static int callback_count = 0;
//     static std::string last_msg;

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_callback.log");
//     rpt::handler::init_report_handler();

//     rpt::handler::add_callback([](const rpt::formatter::ReportInfo& info) {
//         callback_count++;
//         last_msg = info.message;
//         std::cout << "  [CALLBACK FIRED] severity=" << info.severity
//                   << " msg=" << info.message << "\n";
//     });

//     SC_REPORT_INFO("cb", "First callback test");
//     SC_REPORT_WARNING("cb", "Second callback test");

//     rpt::handler::close_report_log();

//     CHECK_EQ(callback_count, 2);
//     CHECK_EQ(last_msg, std::string("Second callback test"));
// }

// // ============================================================================
// // Test 9: File rotation
// // ============================================================================
// void test_file_rotation()
// {
//     std::cout << "\n=== Test 9: File rotation ===\n";

//     reset_all_state();
//     // Set small max size to trigger rotation
//     rpt::config::set_max_file_size(500);  // 500 bytes
//     rpt::config::set_compress_rotated(false);  // keep .1 plain for easy check
//     rpt::config::set_log_file_path("logs/test_rotation.log");
//     rpt::handler::init_report_handler();

//     // Each line ~80 bytes, so ~7 lines should trigger rotation
//     for (int i = 0; i < 20; ++i) {
//         SC_REPORT_INFO("rot", "This is a rotation test message with some padding to fill bytes");
//     }

//     rpt::handler::close_report_log();
//     rpt::config::set_max_file_size(10 * 1024 * 1024);  // reset to 10MB

//     // Check that .1 backup file was created
//     bool backup_exists = std::filesystem::exists("logs/test_rotation.log.1");
//     CHECK(backup_exists);
//     INFO_MSG("Rotation backup file created: logs/test_rotation.log.1");
// }

// // ============================================================================
// // Test 10: Themes
// // ============================================================================
// void test_themes()
// {
//     std::cout << "\n=== Test 10: Color themes ===\n";

//     reset_all_state();
//     rpt::config::set_force_no_color(false);  // enable colors
//     rpt::config::set_log_to_file(false);     // only terminal for this test
//     rpt::config::set_log_file_path("logs/test_themes.log");
//     rpt::handler::init_report_handler();

//     std::cout << "  --- Dark theme: ---\n";
//     rpt::colors::set_theme(rpt::colors::Theme::Dark);
//     SC_REPORT_INFO("theme", "Dark theme INFO");
//     SC_REPORT_WARNING("theme", "Dark theme WARNING");
//     SC_REPORT_ERROR("theme", "Dark theme ERROR");

//     std::cout << "  --- Light theme: ---\n";
//     rpt::colors::set_theme(rpt::colors::Theme::Light);
//     SC_REPORT_INFO("theme", "Light theme INFO");
//     SC_REPORT_WARNING("theme", "Light theme WARNING");
//     SC_REPORT_ERROR("theme", "Light theme ERROR");

//     rpt::handler::close_report_log();
//     rpt::config::set_force_no_color(true);
//     rpt::config::set_log_to_file(true);
// }

// // ============================================================================
// // Test 11: Macros
// // ============================================================================
// void test_macros()
// {
//     std::cout << "\n=== Test 11: Convenience macros ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_macros.log");
//     rpt::handler::init_report_handler();

//     VR_LOG_INFO("macro", "Info via macro");
//     VR_LOG_WARNING("macro", "Warning via macro");
//     VR_LOG_ERROR("macro", "Error via macro");

//     VR_LOG_INFO_LOC("macro", "Info with location");
//     VR_LOG_WARNING_LOC("macro", "Warning with location");

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 2u);
//     CHECK_EQ(s.warning.load(), 2u);
//     CHECK_EQ(s.error.load(), 1u);
// }

// // ============================================================================
// // Test 12: Hierarchy + Process type detection (merged)
// // ============================================================================
// SC_MODULE(TestModuleProcs)
// {
//     sc_core::sc_in<bool> clk;

//     SC_CTOR(TestModuleProcs)
//     {
//         SC_CTHREAD(cthread_proc, clk.pos());
//         SC_THREAD(thread_proc);
//         SC_METHOD(method_proc);
//         sensitive << clk.pos();
//     }

//     void cthread_proc() {
//         while (true) {
//             SC_REPORT_INFO("ptype", "From CTHREAD");
//             wait();
//         }
//     }

//     void thread_proc() {
//         while (true) {
//             SC_REPORT_INFO("ptype", "From THREAD");
//             wait(1, SC_NS);
//         }
//     }

//     void method_proc() {
//         SC_REPORT_INFO("ptype", "From METHOD");
//     }
// };


// void test_hierarchy_and_process_type()
// {
//     std::cout << "\n=== Test 12: Hierarchy + Process type detection ===\n";

//     reset_all_state();
//     rpt::config::set_capture_hierarchy(true);
//     rpt::config::set_log_file_path("logs/test_ptype.log");
//     rpt::handler::init_report_handler();

//     sc_clock clk("clk", 1, SC_NS);
//     TestModuleProcs tm("ptype_mod");
//     tm.clk(clk);
//     sc_start(3, SC_NS);

//     rpt::handler::close_report_log();

//     // Verify file contains all three process types AND module name
//     std::ifstream f("logs/test_ptype.log");
//     std::string content((std::istreambuf_iterator<char>(f)),
//         std::istreambuf_iterator<char>());

//     bool has_module   = content.find("ptype_mod")  != std::string::npos;
//     bool has_cthread  = content.find("[CTHREAD]")  != std::string::npos;
//     bool has_thread   = content.find("[THREAD]")   != std::string::npos;
//     bool has_method   = content.find("[METHOD]")   != std::string::npos;

//     CHECK(has_module);
//     CHECK(has_cthread);
//     CHECK(has_thread);
//     CHECK(has_method);

//     // Test extern thread detection (no sc_start needed — just call the function)
//     std::string hier = rpt::utils::get_module_hierarchy();
//     std::cout << "  Current hierarchy (from sc_main): " << hier << "\n";
//     CHECK(hier.find("[extern]") != std::string::npos);

//     INFO_MSG("CTHREAD, THREAD, METHOD, and extern thread all detected");
// }

// // ============================================================================
// // Test 13: Log diff
// // ============================================================================
// void test_log_diff()
// {
//     std::cout << "\n=== Test 13: Log diff ===\n";

//     // Create baseline file
//     {
//         std::ofstream b("logs/baseline.log");
//         b << "[INFO] test: hello @ 10 ns (file.cpp:1) in process: top\n"
//           << "[WARNING] test: warn @ 20 ns (file.cpp:2) in process: top\n"
//           << "[ERROR] test: err @ 30 ns (file.cpp:3) in process: top\n";
//     }
//     // Create current file (same content, different timestamps)
//     {
//         std::ofstream c("logs/current.log");
//         c << "[INFO] test: hello @ 100 ns (file.cpp:1) in process: top\n"
//           << "[WARNING] test: warn @ 200 ns (file.cpp:2) in process: top\n"
//           << "[ERROR] test: err @ 300 ns (file.cpp:3) in process: top\n";
//     }

//     auto result = rpt::diff::compare("logs/baseline.log", "logs/current.log");
//     CHECK(result.identical);  // timestamps are normalized out
//     INFO_MSG("Diff correctly ignored timestamp differences");
// }

// // ============================================================================
// // Test 14: Thread safety
// // ============================================================================
// void test_thread_safety()
// {
//     std::cout << "\n=== Test 14: Thread safety ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_threads.log");
//     rpt::handler::init_report_handler();

//     auto worker = [](int id) {
//         for (int i = 0; i < 50; ++i) {
//             SC_REPORT_INFO("thread", ("Worker " + std::to_string(id) +
//                 " message " + std::to_string(i)).c_str());
//         }
//     };

//     std::vector<std::thread> threads;
//     for (int i = 0; i < 4; ++i) threads.emplace_back(worker, i);
//     for (auto& t : threads) t.join();

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 200u);  // 4 threads * 50 msgs

//     // Verify no corrupted lines in file
//     std::ifstream f("logs/test_threads.log");
//     int line_count = 0;
//     std::string line;
//     while (std::getline(f, line)) {
//         line_count++;
//         // Each line should start with [INFO]
//         if (line.find("[INFO]") != 0) {
//             std::cout << "  [FAIL] Corrupted line " << line_count << ": " << line << "\n";
//             g_fail++;
//         }
//     }
//     CHECK_EQ(line_count, 200);
// }

// // ============================================================================
// // Test 15: Auto directory creation
// // ============================================================================
// void test_auto_dir_creation()
// {
//     std::cout << "\n=== Test 15: Auto directory creation ===\n";

//     reset_all_state();
//     std::string deep_path = "logs/nested/deep/path/test.log";
//     std::filesystem::remove_all("logs/nested");

//     rpt::config::set_log_file_path(deep_path);
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("dirs", "This should auto-create nested dirs");

//     rpt::handler::close_report_log();

//     CHECK(std::filesystem::exists(deep_path));
//     INFO_MSG("Nested directories created automatically");
// }

// // ==========================================================================
// // === [ERROR-GENERATING TESTS] =============================================
// // ==========================================================================
// // Uncomment the tests below one-by-one to verify error-handling paths.
// // Each test intentionally triggers errors, fatal errors, or exceptions.
// // ==========================================================================

// // ---------------------------------------------------------------------------
// // [ERROR-GENERATING] Test E1: Trigger a FATAL report
// // ---------------------------------------------------------------------------
// // WARNING: This will call abort() and terminate the simulation immediately.
// // The handler will print to terminal + log file, then SystemC will abort.
// // ---------------------------------------------------------------------------

// void test_fatal_error()
// {
//     std::cout << "\n=== [ERROR] Test E1: FATAL error ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_fatal.log");
//     rpt::handler::init_report_handler();

//     SC_REPORT_FATAL("fatal_test", "This is a fatal error - simulation will abort");

//     // Code below will NOT execute
//     rpt::handler::close_report_log();
// }


// // ---------------------------------------------------------------------------
// // [ERROR-GENERATING] Test E2: Throw on ERROR
// // ---------------------------------------------------------------------------
// // This will throw std::runtime_error when an ERROR report is issued.
// // The exception will propagate up and likely terminate sc_main.
// // ---------------------------------------------------------------------------
// void test_throw_on_error()
// {
//     std::cout << "\n=== [ERROR] Test E2: Throw on error ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_throw.log");
//     rpt::config::set_throw_on_error(true);
//     rpt::config::set_throw_threshold(SC_ERROR);
//     rpt::handler::init_report_handler();

//     SC_REPORT_INFO("throw", "This is fine");
//     SC_REPORT_ERROR("throw", "This will throw std::runtime_error");

//     // Code below will NOT execute
//     rpt::config::set_throw_on_error(false);
//     rpt::handler::close_report_log();
// }


// // ---------------------------------------------------------------------------
// // [ERROR-GENERATING] Test E3: Address mode with negative value (from sc_cast)
// // ---------------------------------------------------------------------------
// // Tests integration with sc_cast: triggers SC_REPORT_FATAL.
// // ---------------------------------------------------------------------------
// void test_sc_cast_fatal()
// {
//     std::cout << "\n=== [ERROR] Test E3: sc_cast FATAL ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_cast_fatal.log");
//     rpt::handler::init_report_handler();

//     using namespace cc_vrwrapper;
//     sc_lv<8> bad = sc_cast<sc_lv<8>>(-1, "address");  // FATAL: address can't be negative

//     // Code below will NOT execute
//     rpt::handler::close_report_log();
// }

// ---------------------------------------------------------------------------
// [ERROR-GENERATING] Test E4: Invalid string format
// ---------------------------------------------------------------------------
// Tests that invalid input to sc_cast triggers SC_REPORT_ERROR.
// Does NOT abort simulation, but the return value is "unknown".
// ---------------------------------------------------------------------------

void test_invalid_string_format()
{
    std::cout << "\n=== [ERROR] Test E4: Invalid string format ===\n";

    reset_all_state();
    rpt::config::set_log_file_path("logs/test_invalid.log");
    rpt::handler::init_report_handler();

    using namespace cc_vrwrapper;
    // "0b12" has invalid '2' character - should produce error + return X-filled lv
    sc_lv<8> lv = sc_cast<sc_lv<8>>("0b12");
    std::cout << "  Result (should be all X): " << lv.to_string() << "\n";

    rpt::handler::close_report_log();

    auto& s = rpt::stats::get();
    CHECK_EQ(s.error.load(), 1u);
}

// ---------------------------------------------------------------------------
// [ERROR-GENERATING] Test E5: Out-of-range decimal
// ---------------------------------------------------------------------------
void test_overflow_warning()
{
    std::cout << "\n=== [ERROR] Test E5: Overflow warning ===\n";

    reset_all_state();
    rpt::config::set_log_file_path("logs/test_overflow.log");
    rpt::handler::init_report_handler();

    using namespace cc_vrwrapper;
    // 300 doesn't fit in sc_int<8> (range -128..127) - produces warning
    sc_int<8> si = sc_cast<sc_int<8>>(300);
    std::cout << "  300 in sc_int<8> = " << si.to_int64() << " (wrapped to 44)\n";

    rpt::handler::close_report_log();

    auto& s = rpt::stats::get();
    CHECK_EQ(s.warning.load(), 1u);
}

// ---------------------------------------------------------------------------
// [ERROR-GENERATING] Test E6: sc_bv with X/Z input
// ---------------------------------------------------------------------------
void test_bv_with_xz()
{
    std::cout << "\n=== [ERROR] Test E6: sc_bv with X/Z ===\n";

    reset_all_state();
    rpt::config::set_log_file_path("logs/test_bv_xz.log");
    rpt::handler::init_report_handler();

    using namespace cc_vrwrapper;
    // sc_bv doesn't support X/Z - should produce warning and convert to 0
    sc_bv<4> bv = sc_cast<sc_bv<4>>("0b1X0Z");
    std::cout << "  Result (should be 1000): " << bv.to_string() << "\n";

    rpt::handler::close_report_log();

    auto& s = rpt::stats::get();
    CHECK_EQ(s.warning.load(), 1u);
}

// // ---------------------------------------------------------------------------
// // [ERROR-GENERATING] Test E7: Network sink connection failure
// // ---------------------------------------------------------------------------
// // Tries to connect to a non-existent server. Should NOT crash, just fail silently.
// // ---------------------------------------------------------------------------
// void test_network_sink_failure()
// {
//     std::cout << "\n=== [ERROR] Test E7: Network sink failure ===\n";

//     reset_all_state();
//     rpt::config::set_log_file_path("logs/test_network.log");
//     rpt::handler::init_report_handler();

//     // Try connecting to a port where nothing is listening
//     rpt::handler::add_network_sink("127.0.0.1", 9999);

//     SC_REPORT_INFO("net", "This should still be logged to file/terminal");

//     rpt::handler::close_report_log();

//     auto& s = rpt::stats::get();
//     CHECK_EQ(s.info.load(), 1u);
//     INFO_MSG("Network sink failure handled gracefully");
// }

// ==========================================================================
// === [END OF ERROR-GENERATING TESTS] ======================================
// ==========================================================================

// ============================================================================
// Main
// ============================================================================
int sc_main(int argc, char* argv[])
{
    std::cout << "================================\n";
    std::cout << "sc_report_personalized Test Suite\n";
    std::cout << "================================\n";

    // Clean up previous test logs
    std::filesystem::remove_all("logs");
    std::filesystem::create_directory("logs");

    // Run all safe tests
    // test_basic_text_logging();
    // test_json_logging();
    // test_logfmt_logging();
    // test_severity_filtering();
    // test_msg_type_filtering();
    // test_regex_filtering();
    // test_statistics();
    // test_callback_sink();
    // test_file_rotation();
    // test_themes();
    // test_macros();
    // test_hierarchy_and_process_type();
    // test_log_diff();
    // test_thread_safety();
    // test_auto_dir_creation();

    // === UNCOMMENT TO TEST ERROR PATHS ===
    // test_fatal_error();           // E1: will abort()
    // test_throw_on_error();        // E2: will throw
    // test_sc_cast_fatal();         // E3: will abort()
    test_invalid_string_format(); // E4: produces error, continues
    test_overflow_warning();      // E5: produces warning, continues
    test_bv_with_xz();            // E6: produces warning, continues
    // test_network_sink_failure();  // E7: fails silently, continues

    std::cout << "\n================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "================================\n";
    std::cout << "Passed : " << g_pass << "\n";
    std::cout << "Failed : " << g_fail << "\n";

    // Print final stats
    rpt::stats::print();

    if (g_fail == 0) {
        std::cout << "\n*** ALL TESTS PASSED ***\n";
        return 0;
    } else {
        std::cout << "\n*** " << g_fail << " TEST(S) FAILED ***\n";
        return 1;
    }
}