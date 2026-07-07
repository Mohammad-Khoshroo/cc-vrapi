// ============================================================================
// pretest.cpp — Comprehensive test for cc_vrwrapper::trace library
//
// Tests covered:
//   1. watch()         — any value change (1-arg callback)
//   2. watch_rising()  — rising edge (0-arg callback)
//   3. watch_falling() — falling edge (1-arg callback)
//   4. watch_value()   — value match (0-arg and 1-arg)
//   5. watch_transition() — specific transition (0-arg)
//   6. Signal types: bool, sc_logic, sc_lv, sc_int, sc_uint, sc_bigint
//   7. TraceManager (VCD) with time unit
//   8. JsonTrace with TRIGGERED delta mode
//   9. Edge detection on bool, sc_logic, sc_uint
// ============================================================================

#include <systemc>
#include "sc_trace.hpp"
#include <iostream>

namespace tr = cc_vrwrapper::trace;
using namespace sc_core;
using namespace sc_dt;

// ============================================================================
// DUT 1: Counter — exercises bool, sc_uint<8>, sc_lv<4>
// ============================================================================
SC_MODULE(Counter) {
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> enable;
    sc_out<sc_uint<8>> count;
    sc_out<sc_lv<4>> state;
    sc_out<bool> overflow;

    SC_CTOR(Counter) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        sc_uint<8> count_reg = 0;
        sc_lv<4> state_reg = "0000";

        count.write(count_reg);
        state.write(state_reg);
        overflow.write(false);

        while (true) {
            wait();

            if (reset.read()) {
                count_reg = 0;
                state_reg = "0000";
            } else if (enable.read()) {
                count_reg = count_reg + 1;
                if (count_reg < 10) {
                    state_reg = "0001";       // LOW
                } else if (count_reg < 100) {
                    state_reg = "0010";       // MEDIUM
                } else if (count_reg < 200) {
                    state_reg = "0100";       // HIGH
                } else {
                    state_reg = "1000";       // VERY_HIGH
                }
            }

            count.write(count_reg);
            state.write(state_reg);
            overflow.write(count_reg == 255);
        }
    }
};

// ============================================================================
// DUT 2: Stimulus — exercises sc_int, sc_bigint, sc_logic
// ============================================================================
SC_MODULE(Stimulus) {
    sc_in<bool> clk;
    sc_out<sc_int<8>> data_int;
    sc_out<sc_bigint<16>> data_bigint;
    sc_out<sc_logic> data_logic;

    SC_CTOR(Stimulus) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        data_int.write(0);
        data_bigint.write(0);
        data_logic.write(SC_LOGIC_0);

        int i = 0;
        while (true) {
            wait();
            data_int.write(i);
            data_bigint.write(static_cast<long long>(i) * 100);
            data_logic.write((i % 2 == 0) ? SC_LOGIC_1 : SC_LOGIC_0);
            i++;
        }
    }
};

// ============================================================================
// sc_main
// ============================================================================
int sc_main(int argc, char* argv[]) {
    // ---- Signals ----
    sc_clock clk("clk", 5, SC_NS);
    sc_signal<bool> reset("reset");
    sc_signal<bool> enable("enable");
    sc_signal<sc_uint<8>> count("count");
    sc_signal<sc_lv<4>> state("state");
    sc_signal<bool> overflow("overflow");
    sc_signal<sc_int<8>> data_int("data_int");
    sc_signal<sc_bigint<16>> data_bigint("data_bigint");
    sc_signal<sc_logic> data_logic("data_logic");

    // ---- DUTs ----
    Counter counter("counter");
    counter.clk(clk);
    counter.reset(reset);
    counter.enable(enable);
    counter.count(count);
    counter.state(state);
    counter.overflow(overflow);

    Stimulus stim("stim");
    stim.clk(clk);
    stim.data_int(data_int);
    stim.data_bigint(data_bigint);
    stim.data_logic(data_logic);

    // ================================================================
    // WATCH API TESTS (must be AFTER port binding, BEFORE sc_start)
    // ================================================================

    int rising_count = 0;
    int falling_count = 0;
    int any_count = 0;
    int value_42_count = 0;
    int very_high_count = 0;
    int trans_count = 0;
    int logic_rising = 0;
    int bigint_any = 0;
    int int_any = 0;

    // Test 1: watch_rising — 0-arg lambda on sc_in<bool>
    tr::watch_rising(counter.clk, [&]() {
        rising_count++;
    });

    // Test 2: watch_falling — 1-arg lambda on sc_in<bool>
    tr::watch_falling(counter.clk, [&falling_count](const bool&) {
        falling_count++;
    });

    // Test 3: watch — 1-arg lambda on sc_signal<sc_uint<8>>
    tr::watch(count, [&any_count](const sc_uint<8>&) {
        any_count++;
    });

    // Test 4: watch_value — 0-arg lambda, fires when count == 42
    tr::watch_value(count, sc_uint<8>(42), [&]() {
        value_42_count++;
        std::cout << "  [event] count reached 42 @ "
                  << sc_time_stamp() << "\n";
    });

    // Test 5: watch_value — 1-arg lambda on sc_lv<4>
    tr::watch_value(state, sc_lv<4>("1000"), [&](const sc_lv<4>& v) {
        very_high_count++;
        std::cout << "  [event] state = VERY_HIGH ("
                  << v << ") @ " << sc_time_stamp() << "\n";
    });

    // Test 6: watch_transition — 0-arg, MEDIUM → HIGH
    tr::watch_transition(state,
        sc_lv<4>("0010"),    // from = MEDIUM
        sc_lv<4>("0100"),    // to   = HIGH
        [&]() {
            trans_count++;
            std::cout << "  [event] state: MEDIUM → HIGH @ "
                      << sc_time_stamp() << "\n";
        });

    // Test 7: watch_rising on sc_signal<sc_logic>
    tr::watch_rising(data_logic, [&]() {
        logic_rising++;
    });

    // Test 8: watch on sc_signal<sc_bigint<16>>
    tr::watch(data_bigint, [&bigint_any](const sc_bigint<16>&) {
        bigint_any++;
    });

    // Test 9: watch on sc_signal<sc_int<8>>
    tr::watch(data_int, [&int_any](const sc_int<8>&) {
        int_any++;
    });

    // ================================================================
    // VCD TRACE (TraceManager)
    // ================================================================
    auto tf = tr::create_trace_file("wave");
    tf->set_time_unit(10.0, SC_PS);
    tf->trace(clk,           "clk");
    tf->trace(reset,         "reset");
    tf->trace(enable,        "enable");
    tf->trace(count,         "count");
    tf->trace(state,         "state");
    tf->trace(overflow,      "overflow");
    tf->trace(data_int,      "data_int");
    tf->trace(data_bigint,   "data_bigint");
    tf->trace(data_logic,    "data_logic");

    // Demonstrate trace_with_group (adds grouped signal to same VCD)
    tf->trace_with_group(count, "count", "datapath");
    tf->trace_with_group(state, "state", "control");

    // ================================================================
    // JSON TRACE — TRIGGERED delta mode (clk rising edge = cycle start)
    // ================================================================
    auto jt = tr::create_json_trace("wave.json");
    jt->set_delta_trigger(clk);    // clk↑ marks new cycle
    jt->trace(clk,           "clk");
    jt->trace(reset,         "reset");
    jt->trace(enable,        "enable");
    jt->trace(count,         "count");
    jt->trace(state,         "state");
    jt->trace(overflow,      "overflow");
    jt->trace(data_int,      "data_int");
    jt->trace(data_bigint,   "data_bigint");
    jt->trace(data_logic,    "data_logic");
    jt->start();

    // ================================================================
    // TEST SCENARIO
    // ================================================================

    std::cout << "=== Phase 1: Reset (5 ns) ===\n";
    reset.write(true);
    enable.write(false);
    sc_start(5, SC_NS);

    std::cout << "=== Phase 2: Counting (300 ns = 60 cycles) ===\n";
    reset.write(false);
    enable.write(true);
    sc_start(300, SC_NS);

    std::cout << "=== Phase 3: Paused (15 ns = 3 cycles) ===\n";
    enable.write(false);
    sc_start(15, SC_NS);

    // ================================================================
    // SUMMARY
    // ================================================================
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "          TEST SUMMARY\n";
    std::cout << "========================================\n";
    std::cout << "clk rising edges      : " << rising_count << "\n";
    std::cout << "clk falling edges     : " << falling_count << "\n";
    std::cout << "count value changes   : " << any_count << "\n";
    std::cout << "count == 42 events    : " << value_42_count
              << " (expect 1)\n";
    std::cout << "state == VERY_HIGH    : " << very_high_count << "\n";
    std::cout << "MEDIUM → HIGH trans   : " << trans_count
              << " (expect 1)\n";
    std::cout << "data_logic rising     : " << logic_rising << "\n";
    std::cout << "data_bigint changes   : " << bigint_any << "\n";
    std::cout << "data_int changes      : " << int_any << "\n";
    std::cout << "JSON trigger events   : " << jt->trigger_count() << "\n";
    std::cout << "JSON signal count     : " << jt->signal_count() << "\n";
    std::cout << "========================================\n";

    tf->close();
    jt->write();

    return 0;
}