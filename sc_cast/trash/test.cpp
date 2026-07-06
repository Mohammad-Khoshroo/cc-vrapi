#include "header.hpp"  // All necessary includes (SystemC, etc.)
#include "sc_cast.hpp" // Your custom casting library
#include "sc_report_peronalized.hpp" // Custom report handler

SC_MODULE(Testbench)
{
    std::ofstream logfile;

    void run()
    {
        constexpr int WIDTH = 8;

        // Open log file
        logfile.open("include/sc_lv_cast_log.txt");
        if (!logfile.is_open())
        {
            SC_REPORT_ERROR("Testbench", "Failed to open log file.");
            sc_stop();
            return;
        }

        std::vector<std::string> test_inputs = {
            
            "0Bx10z10x1z0", // valid mixed base
            "0b 34546yg343", // invalid binary
            "0b10101010",    // binary
            "0x1F",          // hex
            "075",           // octal
            "+42",           // signed decimal
            "-10",           // signed decimal
            "123",           // unsigned decimal
            "0XFF",          // hex upper
            "0B1100",        // binary upper
            "0",             // single-digit
            "-0",            // signed zero
            "0xZZ",          // invalid hex (should warn or error)
            "0b102",         // invalid binary
            "abc",           // completely invalid
            "-300",          // negative overflow for WIDTH=8
            "300"            // positive overflow for WIDTH=8
        };

        std::string mode;

        for (const std::string &str : test_inputs)
        {
            logfile << "==== Input: \"" << str << "\" ====" << std::endl;

            sc_lv<WIDTH> val;
            // test as address (unsigned)
            mode = "address";
            try
            {
                val = sc_cast::sc_lv_cast<WIDTH>(str, mode, 2);
                logfile << "[Address] sc_lv<" << WIDTH << "> = " << val.to_string() << std::endl;
            }
            catch (const std::exception &e)
            {
                logfile << "[Address] Exception: " << e.what() << std::endl;
                logfile << "[Address] sc_lv<" << WIDTH << "> = " << val.to_string() << std::endl;
            }

            // test as data (signed)
            mode = "data";
            try
            {
                val = sc_cast::sc_lv_cast<WIDTH>(str, mode, 2);
                logfile << "[Data   ] sc_lv<" << WIDTH << "> = " << val.to_string() << std::endl;
            }
            catch (const std::exception &e)
            {
                logfile << "[Data   ] Exception: " << e.what() << std::endl;
                
                logfile << "[Data   ] sc_lv<" << WIDTH << "> = " << val.to_string() << std::endl;
            }

            logfile << std::endl;
        }

        logfile << "=== End of Simulation ===" << std::endl;
        logfile.close();

        sc_stop(); // finish simulation
    }

    SC_CTOR(Testbench)
    {
        SC_THREAD(run);
    }
};

int sc_main(int argc, char *argv[])
{
    sc_report_personalized::init_report_handler("include/systemc_report.log");
    Testbench tb("tb");
    sc_start();
    sc_report_personalized::close_report_log();
    return 0;
}
