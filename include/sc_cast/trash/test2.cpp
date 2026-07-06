#include "header.hpp"
#include "sc_cast.hpp"
#include "sc_report_peronalized.hpp"

using namespace sc_dt;
using namespace sc_core;

// Helper: print sc_lv nicely with hex and binary info
template <int W>
void print_lv_info(const sc_lv<W> &lv)
{
    std::cout << lv.to_string() << " (hex: 0x" << std::hex << lv.to_uint() << std::dec << ")\n";
}

// Random signed int generator
template <typename T>
T rand_signed(T min_val, T max_val)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> dist(min_val, max_val);
    return static_cast<T>(dist(gen));
}

// Random unsigned int generator
template <typename T>
T rand_unsigned(T min_val, T max_val)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(min_val, max_val);
    return static_cast<T>(dist(gen));
}

template <int WIDTH>
void test_bool()
{
    std::cout << " bool tests:\n";
    for (bool b : {false, true})
    {
        sc_lv<WIDTH> lv = sc_cast::sc_lv_cast<WIDTH>(b);
        std::cout << "  bool " << b << " -> ";
        print_lv_info(lv);
        sc_assert(lv[0] == (b ? SC_LOGIC_1 : SC_LOGIC_0));
        for (int i = 1; i < WIDTH; ++i)
            sc_assert(lv[i] == SC_LOGIC_0);
    }
}

template <typename T, int WIDTH>
void test_signed_integral()
{
    std::vector<std::string> modes = {"lsb", "msb", "invalid"};

    std::cout << " signed " << typeid(T).name() << " tests:\n";
    for (std::string mode : modes)
    {
        std::cout << "  Mode: " << mode << "\n";

        for (int i = 0; i < 5; ++i)
        {
            T val = rand_signed<T>(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());

            try
            {
                sc_lv<WIDTH> lv = sc_cast::sc_lv_cast<T, WIDTH>(val, mode);
                std::cout << "   val = " << static_cast<int64_t>(val) << " -> ";
                print_lv_info(lv);

            }
            catch (...)
            {
                std::cout << "   Exception thrown for val = " << val << " mode = " << mode << "\n";
            }
        }
    }
}

template <typename T, int WIDTH>
void test_unsigned_integral()
{
    std::vector<std::string> modes = {"lsb", "msb", "invalid"};

    std::cout << " unsigned " << typeid(T).name() << " tests:\n";
    for (const std::string mode : modes)
    {
        std::cout << "  Mode: " << mode << "\n";

        for (int i = 0; i < 5; ++i)
        {
            T val = rand_unsigned<T>(0, std::numeric_limits<T>::max());

            try
            {
                sc_lv<WIDTH> lv = sc_cast::sc_lv_cast<T, WIDTH>(val, mode);
                std::cout << "   val = " << static_cast<uint64_t>(val) << " -> ";
                print_lv_info(lv);
            }
            catch (...)
            {
                std::cout << "   Exception thrown for val = " << val << " mode = " << mode << "\n";
            }
        }
    }
}

// template <int WIDTH>
// void test_edge_cases()
// {
//     std::cout << " Edge cases:\n";

//     int64_t min_signed = -(1LL << (WIDTH - 1));
//     int64_t max_signed = (1LL << (WIDTH - 1)) - 1;
//     uint64_t max_unsigned = (1ULL << WIDTH) - 1;

//     auto print_edge_case = [](int64_t val)
//     {
//         sc_lv<WIDTH> lv = sc_cast::sc_lv_cast<int64_t, WIDTH>(val, "lsb");
//         std::cout << "  val = " << val << " -> ";
//         print_lv_info(lv);
//     };

//     print_edge_case(0);
//     print_edge_case(-1);
//     print_edge_case(min_signed);
//     print_edge_case(max_signed);
//     print_edge_case(max_signed + 1); // overflow
//     print_edge_case(min_signed - 1); // overflow
// }

template <int WIDTH>
void run_tests_for_width()
{
    std::cout << "==============================\n";
    std::cout << "Testing WIDTH = " << WIDTH << " bits\n";
    test_bool<WIDTH>();
    std::cout << "==============================\n";
    test_signed_integral<int8_t, WIDTH>();
    std::cout << "==============================\n";
    test_unsigned_integral<uint8_t, WIDTH>();
    std::cout << "==============================\n";

    test_signed_integral<int16_t, WIDTH>();
    std::cout << "==============================\n";
    test_unsigned_integral<uint16_t, WIDTH>();
    std::cout << "==============================\n";

    test_signed_integral<int32_t, WIDTH>();
    std::cout << "==============================\n";
    test_unsigned_integral<uint32_t, WIDTH>();
    std::cout << "==============================\n";

    test_signed_integral<int64_t, WIDTH>();
    std::cout << "==============================\n";
    test_unsigned_integral<uint64_t, WIDTH>();
    std::cout << "==============================\n";
    // test_edge_cases<WIDTH>();

    std::cout << "\n";
}

SC_MODULE(Testbench){
    void run(){
        constexpr int widths[] = {4, 8, 16, 32, 64};

for (int w : widths)
{
    switch (w)
    {
    case 4:
        run_tests_for_width<4>();
        break;
    case 8:
        run_tests_for_width<8>();
        break;
    case 16:
        run_tests_for_width<16>();
        break;
    case 32:
        run_tests_for_width<32>();
        break;
    case 64:
        run_tests_for_width<64>();
        break;
    default:
        std::cout << "Unsupported width " << w << "\n";
        break;
    }
}
sc_stop();
}

SC_CTOR(Testbench)
{
    SC_THREAD(run);
}
}
;

int sc_main(int argc, char *argv[])
{
    sc_report_personalized::init_report_handler("include/systemc_report_2.log");
    Testbench tb("tb");
    sc_start();
    sc_report_personalized::close_report_log();
    return 0;
}
