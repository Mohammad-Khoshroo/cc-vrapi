// ============================================================================
// test_full.cpp — Comprehensive test for cc_vrwrapper::trace library
//
// Covers:
//   [W1] watch()         — 1-arg callback (any change)
//   [W2] watch()         — 0-arg callback (any change)
//   [W3] watch_rising()  — 0-arg callback (bool)
//   [W4] watch_falling() — 1-arg callback (bool)
//   [W5] watch_value()   — 0-arg callback (sc_uint)
//   [W6] watch_value()   — 1-arg callback (sc_lv)
//   [W7] watch_transition() — 0-arg callback (sc_lv)
//   [W8] watch_rising()  — on sc_logic
//   [W9] watch_rising()  — on sc_int
//   [W10] watch_rising() — on sc_bigint
//
//   [T1] TraceManager basic trace()
//   [T2] TraceManager trace_with_group()
//   [T3] TraceManager set_top_scope()
//   [T4] TraceManager set_include_filter() / set_exclude_filter()
//   [T5] TraceManager trace_all(recursive=true)
//   [T6] TraceManager trace_top_level_signals()
//   [T7] TraceManager compress_output() [optional, off by default]
//   [T8] TraceManager set_time_unit()
//
//   [J1] JsonTrace basic trace()
//   [J2] JsonTrace set_record_delta_cycles(true) — GLOBAL mode
//   [J3] JsonTrace set_delta_trigger() — TRIGGERED mode
//   [J4] JsonTrace snapshot()
//   [J5] JsonTrace start() / write()
//
//   [S1] sc_clock support
//   [S2] register_signal_type<T>() / CC_VRWRAPPER_REGISTER_COMMON_TYPES()
//   [S3] clear_watchers()
// ============================================================================

#include <systemc>
#include "sc_trace.hpp"
#include <iostream>
#include <iomanip>

namespace tr = cc_vrwrapper::trace;
using namespace sc_core;
using namespace sc_dt;

// ============================================================================
// Counter DUT — bool, sc_uint, sc_lv
// ============================================================================
SC_MODULE(Counter) {
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> enable;
    sc_out<sc_uint<8>> count;
    sc_out<sc_lv<4>> state;

    SC_CTOR(Counter) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        sc_uint<8> c = 0;
        sc_lv<4> s = "0000";
        count.write(c);
        state.write(s);
        while (true) {
            wait();
            if (reset.read()) {
                c = 0; s = "0000";
            } else if (enable.read()) {
                c = c + 1;
                if      (c < 5)  s = "0001";  // LOW
                else if (c < 10) s = "0010";  // MEDIUM
                else             s = "0100";  // HIGH
            }
            count.write(c);
            state.write(s);
        }
    }
};

// ============================================================================
// Stimulus DUT — sc_int, sc_bigint, sc_logic, sc_bv
// ============================================================================
SC_MODULE(Stimulus) {
    sc_in<bool> clk;
    sc_out<sc_int<8>>  data_int;
    sc_out<sc_bigint<16>> data_bigint;
    sc_out<sc_logic>   data_logic;
    sc_out<sc_bv<8>>   data_bv;

    SC_CTOR(Stimulus) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        data_int.write(0);
        data_bigint.write(0);
        data_logic.write(SC_LOGIC_0);
        data_bv.write("00000000");

        int i = 0;
        while (true) {
            wait();
            data_int.write(i);
            data_bigint.write(static_cast<long long>(i) * 100);
            data_logic.write((i % 2 == 0) ? SC_LOGIC_1 : SC_LOGIC_0);
            sc_bv<8> bv;
            for (int b = 0; b < 8; b++) bv[b] = (i >> b) & 1;
            data_bv.write(bv);
            i++;
        }
    }
};

// ============================================================================
// Submodule inside Counter for recursive trace_all test
// ============================================================================
SC_MODULE(SubMod) {
    sc_in<bool> clk;
    sc_signal<sc_uint<4>> internal_sig;

    SC_CTOR(SubMod) : internal_sig("internal_sig") {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        internal_sig.write(0);
        sc_uint<4> v = 0;
        while (true) {
            wait();
            internal_sig.write(++v);
        }
    }
};

// ============================================================================
// Global counters for callback verification
// ============================================================================
struct Counters {
    int w1_any_1arg = 0;        // watch() 1-arg
    int w2_any_0arg = 0;        // watch() 0-arg
    int w3_rise_bool = 0;       // watch_rising() bool
    int w4_fall_bool = 0;       // watch_falling() bool
    int w5_val_uint = 0;        // watch_value() sc_uint
    int w6_val_lv = 0;          // watch_value() sc_lv
    int w7_trans_lv = 0;        // watch_transition() sc_lv
    int w8_rise_logic = 0;      // watch_rising() sc_logic
    int w9_rise_int = 0;        // watch_rising() sc_int
    int w10_rise_bigint = 0;    // watch_rising() sc_bigint
};
static Counters cnt;

// ============================================================================
// sc_main
// ============================================================================
int sc_main(int argc, char* argv[]) {
    // ---- Signals (top-level) ----
    sc_clock clk("clk", 5, SC_NS);
    sc_signal<bool> reset("reset");
    sc_signal<bool> enable("enable");
    sc_signal<sc_uint<8>> count("count");
    sc_signal<sc_lv<4>> state("state");
    sc_signal<sc_int<8>> data_int("data_int");
    sc_signal<sc_bigint<16>> data_bigint("data_bigint");
    sc_signal<sc_logic> data_logic("data_logic");
    sc_signal<sc_bv<8>> data_bv("data_bv");

    // ---- DUTs ----
    Counter counter("counter");
    counter.clk(clk);
    counter.reset(reset);
    counter.enable(enable);
    counter.count(count);
    counter.state(state);

    Stimulus stim("stim");
    stim.clk(clk);
    stim.data_int(data_int);
    stim.data_bigint(data_bigint);
    stim.data_logic(data_logic);
    stim.data_bv(data_bv);

    SubMod submod("submod");
    submod.clk(clk);

    // ================================================================
    // [S2] Register signal types
    // ================================================================
    CC_VRWRAPPER_REGISTER_COMMON_TYPES();
    // Also register a non-standard width to show extensibility
    tr::register_signal_type<sc_lv<4>>();
    tr::register_signal_type<sc_int<8>>();
    tr::register_signal_type<sc_bv<8>>();
    tr::register_signal_type<sc_bigint<16>>();

    // ================================================================
    // WATCH API TESTS — must be AFTER port binding, BEFORE sc_start
    // ================================================================

    // [W1] watch() — 1-arg callback
    tr::watch(count, [](const sc_uint<8>&) {
        cnt.w1_any_1arg++;
    });

    // [W2] watch() — 0-arg callback
    tr::watch(enable, []() {
        cnt.w2_any_0arg++;
    });

    // [W3] watch_rising() — 0-arg on bool
    tr::watch_rising(counter.clk, []() {
        cnt.w3_rise_bool++;
    });

    // [W4] watch_falling() — 1-arg on bool
    tr::watch_falling(counter.clk, [](const bool&) {
        cnt.w4_fall_bool++;
    });

    // [W5] watch_value() — 0-arg on sc_uint, fires when count == 7
    tr::watch_value(count, sc_uint<8>(7), []() {
        cnt.w5_val_uint++;
        std::cout << "  [event] count == 7 @ " << sc_time_stamp() << "\n";
    });

    // [W6] watch_value() — 1-arg on sc_lv, fires when state == HIGH
    tr::watch_value(state, sc_lv<4>("0100"), [](const sc_lv<4>& v) {
        cnt.w6_val_lv++;
        std::cout << "  [event] state == HIGH @ " << sc_time_stamp() << "\n";
    });

    // [W7] watch_transition() — 0-arg, MEDIUM → HIGH
    tr::watch_transition(state,
        sc_lv<4>("0010"),    // from = MEDIUM
        sc_lv<4>("0100"),    // to   = HIGH
        []() {
            cnt.w7_trans_lv++;
            std::cout << "  [event] MEDIUM→HIGH @ " << sc_time_stamp() << "\n";
        });

    // [W8] watch_rising() — on sc_logic
    tr::watch_rising(data_logic, []() {
        cnt.w8_rise_logic++;
    });

    // [W9] watch_rising() — on sc_int<8>
    tr::watch_rising(data_int, []() {
        cnt.w9_rise_int++;
    });

    // [W10] watch_rising() — on sc_bigint<16>
    tr::watch_rising(data_bigint, []() {
        cnt.w10_rise_bigint++;
    });

    // ================================================================
    // VCD TRACE (TraceManager)
    // ================================================================
    auto tf = tr::create_trace_file("wave");
    tf->set_time_unit(10.0, SC_PS);              // [T8]
    tf->set_top_scope("tb");                     // [T3]

    // [T4] Filter test — exclude data_bv, include only "tb.*"
    tf->set_exclude_filter("data_bv");

    // [T5][T6] Auto-trace
    size_t n_top = tf->trace_top_level_signals();
    size_t n_mod = tf->trace_all(counter, true) + tf->trace_all(stim, true);
    std::cout << "Auto-traced: " << n_top << " top-level + "
              << n_mod << " module signals\n";

    // [T1] Explicit trace() for clk (sc_clock)
    tf->trace(clk, "clk");

    // [T2] trace_with_group()
    tf->trace_with_group(count, "count", "datapath");
    tf->trace_with_group(state, "state", "control");

    // ================================================================
    // JSON TRACE — TRIGGERED delta mode
    // ================================================================
    auto jt = tr::create_json_trace("wave.json");
    jt->set_delta_trigger(clk);                  // [J3]
    jt->trace(clk, "clk");
    jt->trace(reset, "reset");
    jt->trace(enable, "enable");
    jt->trace(count, "count");
    jt->trace(state, "state");
    jt->trace(data_int, "data_int");
    jt->trace(data_bigint, "data_bigint");
    jt->trace(data_logic, "data_logic");
    jt->trace(data_bv, "data_bv");
    jt->start();                                 // [J5]

    // ================================================================
    // SEPARATE JSON TRACE — GLOBAL delta mode (separate file)
    // ================================================================
    auto jt_global = tr::create_json_trace("wave_global.json");
    jt_global->set_record_delta_cycles(true);    // [J2] GLOBAL mode
    jt_global->trace(clk, "clk");
    jt_global->trace(count, "count");
    jt_global->start();

    // ================================================================
    // SIMULATION
    // ================================================================
    std::cout << "\n=== Phase 1: Reset (5 ns) ===\n";
    reset.write(true);
    enable.write(false);
    jt->snapshot();         // [J4]
    jt_global->snapshot();
    sc_start(5, SC_NS);

    std::cout << "=== Phase 2: Counting (100 ns = 20 cycles) ===\n";
    reset.write(false);
    enable.write(true);
    jt->snapshot();
    jt_global->snapshot();
    sc_start(100, SC_NS);

    std::cout << "=== Phase 3: Paused (10 ns = 2 cycles) ===\n";
    enable.write(false);
    jt->snapshot();
    jt_global->snapshot();
    sc_start(10, SC_NS);

    // ================================================================
    // [S3] clear_watchers() — safe after sc_start returns
    // ================================================================
    tr::clear_watchers();

    // ================================================================
    // SUMMARY
    // ================================================================
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "       WATCH CALLBACK SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "[W1]  watch() 1-arg          : " << cnt.w1_any_1arg << "\n";
    std::cout << "[W2]  watch() 0-arg          : " << cnt.w2_any_0arg << "\n";
    std::cout << "[W3]  watch_rising(bool)     : " << cnt.w3_rise_bool << "\n";
    std::cout << "[W4]  watch_falling(bool)    : " << cnt.w4_fall_bool << "\n";
    std::cout << "[W5]  watch_value(uint==7)   : " << cnt.w5_val_uint
              << " (expect 1)\n";
    std::cout << "[W6]  watch_value(lv==HIGH)  : " << cnt.w6_val_lv << "\n";
    std::cout << "[W7]  watch_transition(MED→HIGH): " << cnt.w7_trans_lv
              << " (expect 1)\n";
    std::cout << "[W8]  watch_rising(logic)    : " << cnt.w8_rise_logic << "\n";
    std::cout << "[W9]  watch_rising(int)      : " << cnt.w9_rise_int << "\n";
    std::cout << "[W10] watch_rising(bigint)   : " << cnt.w10_rise_bigint << "\n";
    std::cout << "========================================\n";
    std::cout << "JSON triggers (TRIGGERED)    : " << jt->trigger_count() << "\n";
    std::cout << "JSON signals (TRIGGERED)     : " << jt->signal_count() << "\n";
    std::cout << "JSON signals (GLOBAL)        : " << jt_global->signal_count() << "\n";
    std::cout << "========================================\n";

    tf->close();
    jt->write();
    jt_global->write();

    return 0;
}