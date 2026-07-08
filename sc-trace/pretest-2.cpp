#include <systemc>
#include "sc_trace.hpp"
#include <iostream>

namespace tr = cc_vrwrapper::trace;
using namespace sc_core;
using sc_dt::sc_lv;
using sc_dt::sc_uint;
using sc_dt::sc_int;
using sc_dt::sc_bigint;
using sc_dt::sc_logic;

SC_MODULE(Counter) {
    sc_in<bool> clk;
    sc_in<bool> reset;
    sc_in<bool> enable;
    sc_out<sc_uint<8>> count;

    SC_CTOR(Counter) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        sc_uint<8> c = 0;
        count.write(c);
        while (true) {
            wait();
            if (reset.read())      c = 0;
            else if (enable.read()) c = c + 1;
            count.write(c);
        }
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 5, SC_NS);
    sc_signal<bool> reset("reset");
    sc_signal<bool> enable("enable");
    sc_signal<sc_uint<8>> count("count");

    Counter counter("counter");
    counter.clk(clk);
    counter.reset(reset);
    counter.enable(enable);
    counter.count(count);

    // 1. ثبت نوع‌ها قبل از trace_all
    CC_VRWRAPPER_REGISTER_COMMON_TYPES();

    // 2. VCD trace
    auto tf = tr::create_trace_file("wave");
    tf->set_time_unit(10.0, SC_PS);
    tf->set_top_scope("tb");
    // tf->compress_output(true);  // ← اگر Surfer باز نمی‌کند، غیرفعال کن

    size_t n = tf->trace_top_level_signals();   // سیگنال‌های sc_main
    n += tf->trace_all(counter, true);           // سیگنال‌های داخل ماژول
    std::cout << "Auto-traced " << n << " signals\n";

    // 3. JSON trace با delta mode
    auto jt = tr::create_json_trace("wave.json");
    jt->set_delta_trigger(clk);
    jt->trace(clk, "clk");
    jt->trace(reset, "reset");
    jt->trace(enable, "enable");
    jt->trace(count, "count");
    jt->start();

    // 4. شبیه‌سازی با snapshot بین هر sc_start
    reset.write(true);
    enable.write(false);
    jt->snapshot();              // ← مهم
    sc_start(5, SC_NS);

    reset.write(false);
    enable.write(true);
    jt->snapshot();              // ← مهم
    sc_start(50, SC_NS);

    enable.write(false);
    jt->snapshot();              // ← مهم
    sc_start(15, SC_NS);

    tf->close();
    jt->write();
    return 0;
}