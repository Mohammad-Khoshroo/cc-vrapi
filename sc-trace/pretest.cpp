#include <systemc>
#include "sc_trace.hpp"

namespace tr = cc_vrwrapper::trace;
using namespace sc_core;
using namespace sc_dt;

SC_MODULE(DUT)
{
    sc_in<bool> clk;
    sc_in<sc_lv<8>> data;

    SC_CTOR(DUT) {
        SC_THREAD(run);
        sensitive << clk.pos();
    }

    void run() {
        // ...
    }
};

int sc_main(int argc, char* argv[]) {
    sc_clock clk("clk", 5, SC_NS);
    sc_signal<sc_lv<8>> data;

    DUT dut("dut");
    dut.clk(clk);
    dut.data(data);

    tr::watch_rising(dut.clk, []() {
        std::cout << "clk↑ @ " << sc_time_stamp() << "\n";
    });
    tr::watch_value(dut.data, sc_lv<8>(0xFF), [](const sc_lv<8>&) {
        std::cout << "data = 0xFF!\n";
    });

    auto tf = tr::create_trace_file("wave");
    tf->set_time_unit(10.0, SC_PS);
    tf->trace(clk, "clk");
    tf->trace(data, "data");

    auto jt = tr::create_json_trace("wave.json");
    jt->trace(clk, "clk");
    jt->trace(data, "data");
    jt->start();

    sc_start(50, SC_NS);

    tf->close();
    jt->write();
    return 0;
}