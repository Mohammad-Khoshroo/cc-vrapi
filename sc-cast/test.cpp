// ============================================================================
// test_sc_cast.cpp — Comprehensive test suite for cc_vrwrapper library
// Compatible with both C++17 and C++20 versions.
//
// Compile (C++20):
//   g++ -std=c++20 -I/path/to/sc-cast-c++20 test_sc_cast.cpp -lsystemc -o test
//
// Compile (C++17):
//   g++ -std=c++17 -I/path/to/sc-cast-c++17 test_sc_cast.cpp -lsystemc -o test
// ============================================================================

#include <systemc>
#include "sc-cast-c++20/sc_cast.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>

using namespace sc_core;
using namespace sc_dt;
using namespace cc_vrwrapper;
using namespace std;

// ----------------------------------------------------------------------------
// Test framework
// ----------------------------------------------------------------------------
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_section_count = 0;

#define CHECK_EQ(actual, expected) do { \
    if (!((actual) == (expected))) { \
        std::cout << "  [FAIL] " << #actual << " == " << #expected \
                  << " | got: " << (actual) << " | expected: " << (expected) \
                  << " | line " << __LINE__ << "\n"; \
        g_tests_failed++; \
    } else { \
        std::cout << "  [PASS] " << #actual << " == " << (actual) << "\n"; \
        g_tests_passed++; \
    } \
} while(0)

#define CHECK_TRUE(x) do { \
    if (!(x)) { \
        std::cout << "  [FAIL] " << #x << " | line " << __LINE__ << "\n"; \
        g_tests_failed++; \
    } else { \
        std::cout << "  [PASS] " << #x << "\n"; \
        g_tests_passed++; \
    } \
} while(0)

#define CHECK_FALSE(x) do { \
    if ((x)) { \
        std::cout << "  [FAIL] (NOT) " << #x << " | line " << __LINE__ << "\n"; \
        g_tests_failed++; \
    } else { \
        std::cout << "  [PASS] (NOT) " << #x << "\n"; \
        g_tests_passed++; \
    } \
} while(0)

#define CHECK_NEAR(actual, expected, eps) do { \
    if (std::fabs((double)(actual) - (double)(expected)) > (double)(eps)) { \
        std::cout << "  [FAIL] |" << (actual) << " - " << (expected) \
                  << "| > " << (eps) << " | line " << __LINE__ << "\n"; \
        g_tests_failed++; \
    } else { \
        std::cout << "  [PASS] approx " << (actual) << " ≈ " << (expected) << "\n"; \
        g_tests_passed++; \
    } \
} while(0)

#define SECTION(name) do { \
    g_section_count++; \
    std::cout << "\n=== Section " << g_section_count << ": " << name << " ===\n"; \
} while(0)

// ============================================================================
// TESTS
// ============================================================================

void test_bool_to_lv() {
    SECTION("bool -> sc_lv");

    sc_lv<8> t = sc_cast<sc_lv<8>>(true);
    sc_lv<8> f = sc_cast<sc_lv<8>>(false);

    CHECK_EQ(t.to_string(), "00000001");
    CHECK_EQ(f.to_string(), "00000000");
    CHECK_EQ(sc_cast<bool>(t), true);
    CHECK_EQ(sc_cast<bool>(f), false);
}

void test_int_to_lv() {
    SECTION("integral -> sc_lv (basic)");

    CHECK_EQ(sc_cast<sc_lv<8>>(0).to_string(),    "00000000");
    CHECK_EQ(sc_cast<sc_lv<8>>(1).to_string(),    "00000001");
    CHECK_EQ(sc_cast<sc_lv<8>>(255).to_string(),  "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>(-1).to_string(),   "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>(-128).to_string(), "10000000");
    CHECK_EQ(sc_cast<sc_lv<16>>(0xCAFE).to_string(), "1100101011111110");
}

void test_int_to_lv_overflow() {
    SECTION("integral -> sc_lv (overflow / truncation)");

    // 300 = 0x12C -> lower 8 bits = 0x2C = 44
    CHECK_EQ(sc_cast<sc_lv<8>>(300).to_uint64(), 44u);
    // 0xDEADBEEF -> lower 8 bits = 0xEF = 239
    CHECK_EQ(sc_cast<sc_lv<8>>(0xDEADBEEFu).to_uint64(), 0xEFu);
}

void test_int_to_lv_msb_mode() {
    SECTION("integral -> sc_lv (MSB mode)");

    uint32_t v = 0xDEADBEEF;
    // LSB: keep lower 8 bits -> 0xEF
    CHECK_EQ(sc_cast<sc_lv<8>>(v, "data", false).to_uint64(), 0xEFu);
    // MSB: keep upper 8 bits -> 0xDE
    CHECK_EQ(sc_cast<sc_lv<8>>(v, "data", true).to_uint64(), 0xDEu);

    int64_t sv = -1;   // all ones
    CHECK_EQ(sc_cast<sc_lv<8>>(sv, "data", false).to_string(), "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>(sv, "data", true).to_string(),  "11111111");
}

void test_float_to_lv() {
    SECTION("float -> sc_lv (bit-reinterpret)");

    float f = 3.14f;
    sc_lv<32> lv = sc_cast<sc_lv<32>>(f);

    // Reinterpret back
    float back = sc_cast<float>(lv);
    CHECK_NEAR(back, 3.14f, 1e-6);

    // Known value: 1.0f = 0x3F800000
    sc_lv<32> one = sc_cast<sc_lv<32>>(1.0f);
    CHECK_EQ(one.to_uint64(), 0x3F800000u);

    // 0.0f = 0x00000000
    CHECK_EQ(sc_cast<sc_lv<32>>(0.0f).to_uint64(), 0x0u);

    // -2.0f = 0xC0000000
    CHECK_EQ(sc_cast<sc_lv<32>>(-2.0f).to_uint64(), 0xC0000000u);
}

void test_double_to_lv() {
    SECTION("double -> sc_lv (bit-reinterpret)");

    double d = 3.141592653589793;
    sc_lv<64> lv = sc_cast<sc_lv<64>>(d);
    double back = sc_cast<double>(lv);
    CHECK_NEAR(back, d, 1e-12);

    // 1.0 = 0x3FF0000000000000
    CHECK_EQ(sc_cast<sc_lv<64>>(1.0).to_uint64(), 0x3FF0000000000000ULL);

    // 0.0 = 0x0000000000000000
    CHECK_EQ(sc_cast<sc_lv<64>>(0.0).to_uint64(), 0x0ULL);
}

void test_string_to_lv_binary() {
    SECTION("string -> sc_lv (binary)");

    CHECK_EQ(sc_cast<sc_lv<8>>("0b1010").to_string(),    "00001010");
    CHECK_EQ(sc_cast<sc_lv<8>>("0b11111111").to_string(),"11111111");
    CHECK_EQ(sc_cast<sc_lv<4>>("0b1010").to_string(),    "1010");

    // Pure binary (no prefix, base==2)
    CHECK_EQ(sc_cast<sc_lv<8>>("1010", "data", 2).to_string(), "00001010");

    // With X
    CHECK_EQ(sc_cast<sc_lv<4>>("0b1X01").to_string(), "1X01");
    CHECK_EQ(sc_cast<sc_lv<4>>("0b1Z01").to_string(), "1Z01");
}

void test_string_to_lv_hex() {
    SECTION("string -> sc_lv (hex)");

    CHECK_EQ(sc_cast<sc_lv<8>>("0xFF").to_string(),  "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>("0xCA").to_string(),  "11001010");
    CHECK_EQ(sc_cast<sc_lv<16>>("0xCAFE").to_string(),"1100101011111110");

    // With X
    CHECK_EQ(sc_cast<sc_lv<8>>("0xX0").to_string(), "XXXX0000");
    CHECK_EQ(sc_cast<sc_lv<8>>("0xZ0").to_string(), "ZZZZ0000");
}

void test_string_to_lv_octal() {
    SECTION("string -> sc_lv (octal)");

    // 0777 = 0x1FF = 0b111111111 (9 bits)
    CHECK_EQ(sc_cast<sc_lv<9>>("0777").to_string(),  "111111111");
    // 0o5 = 0b101
    CHECK_EQ(sc_cast<sc_lv<4>>("05").to_string(),    "0101");

    // With X
    CHECK_EQ(sc_cast<sc_lv<6>>("0X5").to_string(), "XXX101");
}

void test_string_to_lv_decimal() {
    SECTION("string -> sc_lv (decimal)");

    CHECK_EQ(sc_cast<sc_lv<8>>("0").to_string(),   "00000000");
    CHECK_EQ(sc_cast<sc_lv<8>>("255").to_string(), "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>("-1").to_string(),  "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>("-128").to_string(),"10000000");

    // Unsigned address mode
    CHECK_EQ(sc_cast<sc_lv<16>>("65535", "address").to_string(), "1111111111111111");
}

void test_string_to_lv_whitespace() {
    SECTION("string -> sc_lv (whitespace handling)");

    // Whitespace is removed
    CHECK_EQ(sc_cast<sc_lv<8>>("0b 1010").to_string(), "00001010");
    CHECK_EQ(sc_cast<sc_lv<8>>(" 0xFF ").to_string(),  "11111111");
    CHECK_EQ(sc_cast<sc_lv<8>>("0b1_0101").to_string(),"00010101");
}

void test_lv_from_lv() {
    SECTION("sc_lv -> other types");

    sc_lv<8> lv("01010101");
    CHECK_EQ(sc_cast<bool>(lv), true);          // LSB = 1
    CHECK_EQ(sc_cast<uint8_t>(lv), 0x55u);
    CHECK_EQ(sc_cast<int8_t>(lv), 0x55);
    CHECK_EQ(sc_cast<uint64_t>(lv), 0x55ull);
    CHECK_EQ(sc_cast<std::string>(lv), "01010101");
}

void test_lv_with_xz_to_int() {
    SECTION("sc_lv with X/Z -> integral (warning + return 0)");

    sc_lv<8> lv("XXXX0001");
    // Contains X -> returns 0
    CHECK_EQ(sc_cast<int>(lv), 0);
    // bool also returns false
    CHECK_FALSE(sc_cast<bool>(lv));
}

void test_lv_to_float_double() {
    SECTION("sc_lv -> float/double (bit-reinterpret)");

    sc_lv<32> fbits = sc_cast<sc_lv<32>>(3.14f);
    float f = sc_cast<float>(fbits);
    CHECK_NEAR(f, 3.14f, 1e-6);

    sc_lv<64> dbits = sc_cast<sc_lv<64>>(3.141592653589793);
    double d = sc_cast<double>(dbits);
    CHECK_NEAR(d, 3.141592653589793, 1e-12);

    // 0x3F800000 -> 1.0f
    sc_lv<32> one_bits("00111111100000000000000000000000");
    CHECK_NEAR(sc_cast<float>(one_bits), 1.0f, 1e-6);
}

void test_bv_basic() {
    SECTION("sc_bv basic conversions");

    CHECK_EQ(sc_cast<sc_bv<8>>(42).to_string(),    "00101010");
    CHECK_EQ(sc_cast<sc_bv<8>>(255u).to_string(),  "11111111");
    CHECK_EQ(sc_cast<sc_bv<8>>("0b1010").to_string(), "00001010");
    CHECK_EQ(sc_cast<sc_bv<8>>("0xFF").to_string(),   "11111111");

    // bool from bv
    sc_bv<8> bv("01010101");
    CHECK_EQ(sc_cast<bool>(bv), true);
    CHECK_EQ(sc_cast<uint8_t>(bv), 0x55u);
}

void test_bv_xz_handling() {
    SECTION("sc_bv with X/Z input (warning + X/Z -> 0)");

    // X/Z in string -> converted to 0
    CHECK_EQ(sc_cast<sc_bv<4>>("0b1X0Z").to_string(), "1000");
    CHECK_EQ(sc_cast<sc_bv<8>>("0xX0").to_string(),   "00000000");

    // sc_lv with X -> sc_bv: X -> 0
    sc_lv<4> lv("1X0Z");
    CHECK_EQ(sc_cast<sc_bv<4>>(lv).to_string(), "1000");
}

void test_logic_basic() {
    SECTION("sc_logic basic");

    CHECK_EQ(sc_cast<sc_logic>(true).to_char(),  '1');
    CHECK_EQ(sc_cast<sc_logic>(false).to_char(), '0');
    CHECK_EQ(sc_cast<sc_logic>("0").to_char(),   '0');
    CHECK_EQ(sc_cast<sc_logic>("1").to_char(),   '1');
    CHECK_EQ(sc_cast<sc_logic>("X").to_char(),   'X');
    CHECK_EQ(sc_cast<sc_logic>("Z").to_char(),   'Z');
    CHECK_EQ(sc_cast<sc_logic>("0b1").to_char(), '1');  // takes last char
}

void test_logic_from_int() {
    SECTION("sc_logic from integers (LSB)");

    CHECK_EQ(sc_cast<sc_logic>(0).to_char(),  '0');
    CHECK_EQ(sc_cast<sc_logic>(1).to_char(),  '1');
    CHECK_EQ(sc_cast<sc_logic>(2).to_char(),  '0');   // LSB of 2
    CHECK_EQ(sc_cast<sc_logic>(3).to_char(),  '1');
    CHECK_EQ(sc_cast<sc_logic>(-1).to_char(), '1');   // LSB of -1
    CHECK_EQ(sc_cast<sc_logic>(-2).to_char(), '0');
}

void test_logic_to_int() {
    SECTION("sc_logic -> integers");

    CHECK_EQ(sc_cast<int>(sc_logic('1')), 1);
    CHECK_EQ(sc_cast<int>(sc_logic('0')), 0);
    // X/Z -> 0 with warning
    CHECK_EQ(sc_cast<int>(sc_logic('X')), 0);
    CHECK_EQ(sc_cast<int>(sc_logic('Z')), 0);

    CHECK_EQ(sc_cast<bool>(sc_logic('1')), true);
    CHECK_EQ(sc_cast<bool>(sc_logic('0')), false);
    CHECK_EQ(sc_cast<bool>(sc_logic('X')), false);  // warning
}

void test_cross_lv_bv() {
    SECTION("sc_lv <-> sc_bv cross conversion");

    sc_lv<8> lv("10101010");
    sc_bv<8> bv = sc_cast<sc_bv<8>>(lv);
    CHECK_EQ(bv.to_string(), "10101010");

    sc_bv<8> bv2("11001100");
    sc_lv<8> lv2 = sc_cast<sc_lv<8>>(bv2);
    CHECK_EQ(lv2.to_string(), "11001100");

    // sc_lv with X -> sc_bv: X -> 0
    sc_lv<4> lvx("1X0Z");
    sc_bv<4> bvx = sc_cast<sc_bv<4>>(lvx);
    CHECK_EQ(bvx.to_string(), "1000");
}

void test_cross_lv_logic() {
    SECTION("sc_lv <-> sc_logic cross conversion");

    sc_lv<8> lv("01010101");
    sc_logic l = sc_cast<sc_logic>(lv);
    CHECK_EQ(l.to_char(), '1');  // LSB

    sc_logic l2(SC_LOGIC_1);
    sc_lv<8> lv2 = sc_cast<sc_lv<8>>(l2);
    CHECK_EQ(lv2.to_string(), "11111111");

    sc_logic l3(SC_LOGIC_0);
    sc_lv<4> lv3 = sc_cast<sc_lv<4>>(l3);
    CHECK_EQ(lv3.to_string(), "0000");

    // X and Z preserved
    sc_logic l4(SC_LOGIC_X);
    CHECK_EQ(sc_cast<sc_lv<4>>(l4).to_string(), "XXXX");

    sc_logic l5(SC_LOGIC_Z);
    CHECK_EQ(sc_cast<sc_lv<4>>(l5).to_string(), "ZZZZ");
}

void test_cross_bv_logic() {
    SECTION("sc_bv <-> sc_logic cross conversion");

    sc_bv<8> bv("01010101");
    sc_logic l = sc_cast<sc_logic>(bv);
    CHECK_EQ(l.to_char(), '1');  // LSB

    sc_logic l1(SC_LOGIC_1);
    sc_bv<4> bv1 = sc_cast<sc_bv<4>>(l1);
    CHECK_EQ(bv1.to_string(), "1111");

    // sc_logic X -> sc_bv: converted to 0
    sc_logic lx(SC_LOGIC_X);
    sc_bv<4> bvx = sc_cast<sc_bv<4>>(lx);
    CHECK_EQ(bvx.to_string(), "0000");
}

void test_sc_int_basic() {
    SECTION("sc_int / sc_uint basic");

    CHECK_EQ(sc_cast<sc_int<8>>(42).to_int64(), 42);
    CHECK_EQ(sc_cast<sc_int<8>>(-1).to_int64(), -1);
    CHECK_EQ(sc_cast<sc_int<8>>(-128).to_int64(), -128);
    CHECK_EQ(sc_cast<sc_int<8>>(127).to_int64(), 127);

    CHECK_EQ(sc_cast<sc_uint<8>>(42).to_uint64(), 42u);
    CHECK_EQ(sc_cast<sc_uint<8>>(255u).to_uint64(), 255u);
    CHECK_EQ(sc_cast<sc_uint<8>>("0xFF").to_uint64(), 255u);
    CHECK_EQ(sc_cast<sc_uint<8>>("0b1010").to_uint64(), 10u);
}

void test_sc_int_from_lv() {
    SECTION("sc_lv -> sc_int family");

    sc_lv<8> lv("01010101");
    CHECK_EQ(sc_cast<sc_uint<8>>(lv).to_uint64(), 0x55u);
    CHECK_EQ(sc_cast<sc_int<8>>(lv).to_int64(), 0x55);

    sc_lv<8> lvneg("11111111");
    CHECK_EQ(sc_cast<sc_int<8>>(lvneg).to_int64(), -1);
    CHECK_EQ(sc_cast<sc_uint<8>>(lvneg).to_uint64(), 255u);
}

void test_sc_int_to_lv() {
    SECTION("sc_int family -> sc_lv");

    sc_int<8> si = -1;
    CHECK_EQ(sc_cast<sc_lv<8>>(si).to_string(), "11111111");

    sc_int<8> si2 = -128;
    CHECK_EQ(sc_cast<sc_lv<8>>(si2).to_string(), "10000000");

    sc_uint<8> ui = 255u;
    CHECK_EQ(sc_cast<sc_lv<8>>(ui).to_string(), "11111111");

    sc_uint<8> ui2 = 0xCAu;
    CHECK_EQ(sc_cast<sc_lv<8>>(ui2).to_string(), "11001010");
}

void test_sc_int_strings() {
    SECTION("sc_int family string conversions");

    CHECK_EQ(sc_cast<sc_int<8>>("42").to_int64(), 42);
    CHECK_EQ(sc_cast<sc_int<8>>("-42").to_int64(), -42);
    CHECK_EQ(sc_cast<sc_int<16>>("0xCAFE").to_int64(), static_cast<int16_t>(0xCAFE));
    CHECK_EQ(sc_cast<sc_uint<16>>("0xCAFE").to_uint64(), 0xCAFEu);
    CHECK_EQ(sc_cast<sc_int<8>>("0b1010").to_int64(), 10);

    // sc_int -> string
    sc_int<8> si = 42;
    CHECK_EQ(sc_cast<std::string>(si), "42");
}

void test_sc_int_overflow() {
    SECTION("sc_int family overflow / truncation");

    // 300 in 8-bit signed -> wraps to 300 - 256 = 44
    CHECK_EQ(sc_cast<sc_int<8>>(300).to_int64(), 44);
    // 300 in 8-bit unsigned -> 300 - 256 = 44
    CHECK_EQ(sc_cast<sc_uint<8>>(300u).to_uint64(), 44u);

    // -200 in 8-bit signed -> -200 + 256 = 56
    CHECK_EQ(sc_cast<sc_int<8>>(-200).to_int64(), 56);
}

void test_sc_int_msb_mode() {
    SECTION("sc_int family MSB mode");

    uint32_t v = 0xDEADBEEF;
    // LSB -> 0xEF
    CHECK_EQ(sc_cast<sc_uint<8>>(v, "data", false).to_uint64(), 0xEFu);
    // MSB -> 0xDE
    CHECK_EQ(sc_cast<sc_uint<8>>(v, "data", true).to_uint64(), 0xDEu);
}

void test_sc_bigint() {
    SECTION("sc_bigint / sc_biguint (>64 bits)");

    // A 128-bit value as string
    std::string big_hex = "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";
    sc_biguint<128> bu = sc_cast<sc_biguint<128>>(big_hex);
    std::string bu_str = sc_cast<std::string>(bu);
    CHECK_TRUE(bu_str.size() > 0);
    std::cout << "    [INFO] sc_biguint<128> of 0xFFF...F = " << bu_str << "\n";

    // Round-trip through sc_lv<128>
    sc_lv<128> lv = sc_cast<sc_lv<128>>(bu);
    std::string lv_str = lv.to_string();
    CHECK_EQ(lv_str.size(), 128u);
    std::cout << "    [INFO] sc_lv<128> = " << lv_str.substr(0, 32) << "...\n";

    // Negative big int — with our new sign-handling fix, "-1" should round-trip to "-1"
    sc_bigint<128> bi = sc_cast<sc_bigint<128>>("-1");
    std::string bi_str = sc_cast<std::string>(bi);
    CHECK_EQ(bi_str, std::string("-1"));
    std::cout << "    [INFO] sc_bigint<128>(-1) = " << bi_str << "\n";
}

void test_string_cast_generic() {
    SECTION("string_cast generic");

    CHECK_EQ(string_cast<int>("42"), 42);
    CHECK_EQ(string_cast<int>("-42"), -42);
    CHECK_EQ(string_cast<unsigned>("0xFF", "data", 16), 0xFFu);
    CHECK_EQ(string_cast<unsigned>("0b1010", "data", 2), 10u);
    CHECK_EQ(string_cast<unsigned>("0777", "data", 8), 0777u);
    CHECK_EQ(string_cast<double>("3.14"), 3.14);
    CHECK_EQ(string_cast<float>("2.5"), 2.5f);

    // Empty / X-only string
    CHECK_EQ(string_cast<int>(""), 0);
    CHECK_EQ(string_cast<int>("XX"), 0);
}

void test_round_trip() {
    SECTION("Round-trip conversions");

    int round_trip_fails = 0;

    // int -> lv -> int
    for (int i = -128; i <= 127; ++i) {
        sc_lv<8> lv = sc_cast<sc_lv<8>>(i);
        int back = sc_cast<int8_t>(lv);
        if (back != i) round_trip_fails++;
    }
    if (round_trip_fails == 0) {
        std::cout << "  [PASS] round-trip all int8 values (-128..127)\n";
        g_tests_passed++;
    } else {
        std::cout << "  [FAIL] round-trip int8: " << round_trip_fails
                  << " failures | line " << __LINE__ << "\n";
        g_tests_failed++;
    }
    round_trip_fails = 0;

    // float -> lv -> float
    for (float f : {0.0f, 1.0f, -1.0f, 3.14f, -2.5f, 1e10f, 1e-10f}) {
        sc_lv<32> lv = sc_cast<sc_lv<32>>(f);
        float back = sc_cast<float>(lv);
        if (std::fabs(back - f) > 1e-6f) round_trip_fails++;
    }
    if (round_trip_fails == 0) {
        std::cout << "  [PASS] round-trip select float values\n";
        g_tests_passed++;
    } else {
        std::cout << "  [FAIL] round-trip float: " << round_trip_fails
                  << " failures | line " << __LINE__ << "\n";
        g_tests_failed++;
    }
    round_trip_fails = 0;

    // double -> lv -> double
    for (double d : {0.0, 1.0, -1.0, 3.141592653589793, -2.718281828459045, 1e100, 1e-100}) {
        sc_lv<64> lv = sc_cast<sc_lv<64>>(d);
        double back = sc_cast<double>(lv);
        if (std::fabs(back - d) > 1e-12) round_trip_fails++;
    }
    if (round_trip_fails == 0) {
        std::cout << "  [PASS] round-trip select double values\n";
        g_tests_passed++;
    } else {
        std::cout << "  [FAIL] round-trip double: " << round_trip_fails
                  << " failures | line " << __LINE__ << "\n";
        g_tests_failed++;
    }
    round_trip_fails = 0;

    // sc_int -> sc_lv -> sc_int
    for (int i = -100; i <= 100; ++i) {
        sc_int<10> si = i;
        sc_lv<10> lv = sc_cast<sc_lv<10>>(si);
        sc_int<10> back = sc_cast<sc_int<10>>(lv);
        if (back.to_int64() != i) round_trip_fails++;
    }
    if (round_trip_fails == 0) {
        std::cout << "  [PASS] round-trip sc_int<10> (-100..100)\n";
        g_tests_passed++;
    } else {
        std::cout << "  [FAIL] round-trip sc_int<10>: " << round_trip_fails
                  << " failures | line " << __LINE__ << "\n";
        g_tests_failed++;
    }
}

// ============================================================================
// Main
// ============================================================================
int sc_main(int argc, char* argv[]) {
    std::cout << "================================\n";
    std::cout << "cc_vrwrapper Library Test Suite\n";
    std::cout << "================================\n";

    test_bool_to_lv();
    test_int_to_lv();
    test_int_to_lv_overflow();
    test_int_to_lv_msb_mode();
    test_float_to_lv();
    test_double_to_lv();
    test_string_to_lv_binary();
    test_string_to_lv_hex();
    test_string_to_lv_octal();
    test_string_to_lv_decimal();
    test_string_to_lv_whitespace();
    test_lv_from_lv();
    test_lv_with_xz_to_int();
    test_lv_to_float_double();
    test_bv_basic();
    test_bv_xz_handling();
    test_logic_basic();
    test_logic_from_int();
    test_logic_to_int();
    test_cross_lv_bv();
    test_cross_lv_logic();
    test_cross_bv_logic();
    test_sc_int_basic();
    test_sc_int_from_lv();
    test_sc_int_to_lv();
    test_sc_int_strings();
    test_sc_int_overflow();
    test_sc_int_msb_mode();
    test_sc_bigint();
    test_string_cast_generic();
    test_round_trip();

    std::cout << "\n================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "================================\n";
    std::cout << "Sections : " << g_section_count << "\n";
    std::cout << "Passed   : " << g_tests_passed << "\n";
    std::cout << "Failed   : " << g_tests_failed << "\n";
    std::cout << "Total    : " << (g_tests_passed + g_tests_failed) << "\n";
    if (g_tests_failed == 0) {
        std::cout << "\n*** ALL TESTS PASSED ***\n";
        return 0;
    } else {
        std::cout << "\n*** " << g_tests_failed << " TEST(S) FAILED ***\n";
        return 1;
    }
}