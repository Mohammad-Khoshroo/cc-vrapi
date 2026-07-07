// ============================================================================
// test_sc_concat.cpp — Comprehensive test for sc-concat library
//
// Compile:
//   g++ -std=c++20 -I./sc-concat test_sc_concat.cpp -lsystemc -lpthread -o test
//   g++ -std=c++17 -I./sc-concat test_sc_concat.cpp -lsystemc -lpthread -o test
//
// Usage:
//   ./test
// ============================================================================

#include <systemc>
#include "sc_concat.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace sc_core;
using namespace sc_dt;
namespace cat = cc_vrwrapper::concat;

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
    auto _a = (a); auto _b = (b); \
    if (_a == _b) { std::cout << "  [PASS] " << #a << " == " << #b \
                              << " (" << _b << ")\n"; g_pass++; } \
    else { std::cout << "  [FAIL] " << #a << " == " << #b \
                     << " | got: " << _a << " | exp: " << _b \
                     << " | line " << __LINE__ << "\n"; g_fail++; } \
} while(0)

#define INFO_MSG(msg) std::cout << "  [INFO] " << msg << "\n"

// ============================================================================
// Test 1: Basic cat() — two arguments
// ============================================================================
void test_cat_basic_two()
{
    std::cout << "\n=== Test 1: cat() — two arguments ===\n";

    sc_lv<4> a("0b1010");
    sc_lv<4> b("0b1100");

    auto r = cat::cat(a, b);
    CHECK_EQ(r.to_string(), std::string("10101100"));
    INFO_MSG("cat(1010, 1100) = " + r.to_string());
}

// ============================================================================
// Test 2: cat() — three arguments
// ============================================================================
void test_cat_basic_three()
{
    std::cout << "\n=== Test 2: cat() — three arguments ===\n";

    sc_lv<4> a("0b1010");
    sc_lv<4> b("0b1100");
    sc_lv<4> c("0b0011");

    auto r = cat::cat(a, b, c);
    CHECK_EQ(r.to_string(), std::string("101011000011"));
    INFO_MSG("cat(1010, 1100, 0011) = " + r.to_string());
}

// ============================================================================
// Test 3: cat() — four arguments
// ============================================================================
void test_cat_basic_four()
{
    std::cout << "\n=== Test 3: cat() — four arguments ===\n";

    sc_lv<2> a("0b11");
    sc_lv<2> b("0b10");
    sc_lv<2> c("0b01");
    sc_lv<2> d("0b00");

    auto r = cat::cat(a, b, c, d);
    CHECK_EQ(r.to_string(), std::string("11100100"));
    INFO_MSG("cat(11, 10, 01, 00) = " + r.to_string());
}

// ============================================================================
// Test 4: cat() with mixed types (sc_lv + sc_bv + sc_logic)
// ============================================================================
void test_cat_mixed_types()
{
    std::cout << "\n=== Test 4: cat() — mixed types ===\n";

    sc_lv<4>  a("0b1010");
    sc_bv<4>  b("0b1100");
    sc_logic  c(SC_LOGIC_1);

    auto r = cat::cat(a, b, c);
    CHECK_EQ(r.to_string(), std::string("101011001"));
    INFO_MSG("cat(sc_lv<4>1010, sc_bv<4>1100, sc_logic_1) = " + r.to_string());
}

// ============================================================================
// Test 5: cat() with sc_int and sc_uint
// ============================================================================
void test_cat_with_int_types()
{
    std::cout << "\n=== Test 5: cat() — with sc_int/sc_uint ===\n";

    sc_int<8>  a(-1);   // 11111111
    sc_uint<4> b(0xA);  // 1010

    auto r = cat::cat(a, b);
    CHECK_EQ(r.to_string(), std::string("111111111010"));
    INFO_MSG("cat(sc_int<8>(-1), sc_uint<4>(0xA)) = " + r.to_string());
}

// ============================================================================
// Test 6: cat<sc_bv>() — explicit type, X/Z dropped to 0
// ============================================================================
void test_cat_explicit_bv()
{
    std::cout << "\n=== Test 6: cat<sc_bv<>>() — explicit type ===\n";

    sc_lv<4> a("0b1010");
    sc_lv<4> b("0b1100");

    sc_bv<8> r = cat::cat<sc_bv<8>>(a, b);
    CHECK_EQ(r.to_string(), std::string("10101100"));
    INFO_MSG("cat<sc_bv<8>>(1010, 1100) = " + r.to_string());
}

// ============================================================================
// Test 7: cat<sc_int>() — explicit signed int
// ============================================================================
void test_cat_explicit_int()
{
    std::cout << "\n=== Test 7: cat<sc_int<>>() — explicit signed int ===\n";

    sc_lv<4> a("0b0001");
    sc_lv<4> b("0b0010");

    sc_int<8> r = cat::cat<sc_int<8>>(a, b);
    CHECK_EQ(r.to_int(), 0b00010010);  // 18
    INFO_MSG("cat<sc_int<8>>(0001, 0010) = " + std::to_string(r.to_int()));
}

// ============================================================================
// Test 8: cat<sc_uint>() — explicit unsigned int
// ============================================================================
void test_cat_explicit_uint()
{
    std::cout << "\n=== Test 8: cat<sc_uint<>>() — explicit unsigned int ===\n";

    sc_lv<4> a("0b1111");
    sc_lv<4> b("0b0000");

    sc_uint<8> r = cat::cat<sc_uint<8>>(a, b);
    CHECK_EQ(r.to_uint(), 0b11110000u);  // 240
    INFO_MSG("cat<sc_uint<8>>(1111, 0000) = " + std::to_string(r.to_uint()));
}

// ============================================================================
// Test 9: cat() preserves X/Z bits in sc_lv
// ============================================================================
void test_cat_preserves_xz()
{
    std::cout << "\n=== Test 9: cat() — preserves X/Z ===\n";

    sc_lv<4> a("0b10XZ");
    sc_lv<4> b("0b1100");

    auto r = cat::cat(a, b);
    CHECK_EQ(r.to_string(), std::string("10XZ1100"));
    INFO_MSG("cat(10XZ, 1100) = " + r.to_string());
}

// ============================================================================
// Test 10: cat() with single argument (identity-like)
// ============================================================================
void test_cat_single()
{
    std::cout << "\n=== Test 10: cat() — single argument ===\n";

    sc_lv<4> a("0b1010");

    auto r = cat::cat(a);
    CHECK_EQ(r.to_string(), std::string("1010"));
    INFO_MSG("cat(1010) = " + r.to_string());
}

// ============================================================================
// Test 11: slice<HI, LO> — middle bits
// ============================================================================
void test_slice_middle()
{
    std::cout << "\n=== Test 11: slice<HI, LO>() — middle bits ===\n";

    sc_lv<8> v("0b10110100");
    // bit 7 6 5 4 3 2 1 0
    //     1 0 1 1 0 1 0 0

    auto mid = cat::slice<5, 2>(v);  // bits 5,4,3,2 = "1101"
    CHECK_EQ(mid.to_string(), std::string("1101"));
    INFO_MSG("slice<5,2>(10110100) = " + mid.to_string());
}

// ============================================================================
// Test 12: slice<HI, LO> — single bit
// ============================================================================
void test_slice_single_bit()
{
    std::cout << "\n=== Test 12: slice<HI, LO>() — single bit ===\n";

    sc_lv<8> v("0b10110100");

    auto bit7 = cat::slice<7, 7>(v);  // MSB = '1'
    auto bit0 = cat::slice<0, 0>(v);  // LSB = '0'
    auto bit5 = cat::slice<5, 5>(v);  // = '1'

    CHECK_EQ(bit7.to_string(), std::string("1"));
    CHECK_EQ(bit0.to_string(), std::string("0"));
    CHECK_EQ(bit5.to_string(), std::string("1"));
}

// ============================================================================
// Test 13: slice<HI, LO> — full range (identity)
// ============================================================================
void test_slice_full()
{
    std::cout << "\n=== Test 13: slice<HI, LO>() — full range ===\n";

    sc_lv<8> v("0b10110100");

    auto full = cat::slice<7, 0>(v);
    CHECK_EQ(full.to_string(), std::string("10110100"));
}

// ============================================================================
// Test 14: slice<HI, LO> — preserves X/Z
// ============================================================================
void test_slice_preserves_xz()
{
    std::cout << "\n=== Test 14: slice<HI, LO>() — preserves X/Z ===\n";

    sc_lv<8> v("0b10XZ1100");
    // bit 7 6 5 4 3 2 1 0
    //     1 0 X Z 1 1 0 0

    auto xz_part = cat::slice<5, 4>(v);  // = "XZ"
    CHECK_EQ(xz_part.to_string(), std::string("XZ"));
    INFO_MSG("slice<5,4>(10XZ1100) = " + xz_part.to_string());
}

// ============================================================================
// Test 15: msb<W>() — top W bits
// ============================================================================
void test_msb()
{
    std::cout << "\n=== Test 15: msb<W>() ===\n";

    sc_lv<8> v("0b10110100");

    auto top4 = cat::msb<4>(v);  // bits 7,6,5,4 = "1011"
    auto top2 = cat::msb<2>(v);  // bits 7,6 = "10"
    auto top1 = cat::msb<1>(v);  // bit 7 = "1"

    CHECK_EQ(top4.to_string(), std::string("1011"));
    CHECK_EQ(top2.to_string(), std::string("10"));
    CHECK_EQ(top1.to_string(), std::string("1"));
}

// ============================================================================
// Test 16: lsb<W>() — bottom W bits
// ============================================================================
void test_lsb()
{
    std::cout << "\n=== Test 16: lsb<W>() ===\n";

    sc_lv<8> v("0b10110100");

    auto bot4 = cat::lsb<4>(v);  // bits 3,2,1,0 = "0100"
    auto bot2 = cat::lsb<2>(v);  // bits 1,0 = "00"
    auto bot1 = cat::lsb<1>(v);  // bit 0 = "0"

    CHECK_EQ(bot4.to_string(), std::string("0100"));
    CHECK_EQ(bot2.to_string(), std::string("00"));
    CHECK_EQ(bot1.to_string(), std::string("0"));
}

// ============================================================================
// Test 17: slice/msb/lsb with sc_int input
// ============================================================================
void test_slice_with_int()
{
    std::cout << "\n=== Test 17: slice/msb/lsb with sc_int ===\n";

    sc_int<8> v(0xAB);  // 10101011

    auto top = cat::msb<4>(v);  // "1010"
    auto bot = cat::lsb<4>(v);  // "1011"
    auto mid = cat::slice<5, 2>(v);  // bits 5,4,3,2 = "1010"

    CHECK_EQ(top.to_string(), std::string("1010"));
    CHECK_EQ(bot.to_string(), std::string("1011"));
    CHECK_EQ(mid.to_string(), std::string("1010"));
}

// ============================================================================
// Test 18: repeat<N>() — pattern repetition
// ============================================================================
void test_repeat_pattern()
{
    std::cout << "\n=== Test 18: repeat<N>() — pattern ===\n";

    sc_lv<4> p("0b1010");

    auto r2 = cat::repeat<2>(p);  // "10101010"
    auto r4 = cat::repeat<4>(p);  // "1010101010101010"

    CHECK_EQ(r2.to_string(), std::string("10101010"));
    CHECK_EQ(r4.to_string(), std::string("1010101010101010"));
    INFO_MSG("repeat<4>(1010) = " + r4.to_string());
}

// ============================================================================
// Test 19: repeat<N>() — with sc_logic
// ============================================================================
void test_repeat_logic()
{
    std::cout << "\n=== Test 19: repeat<N>() — with sc_logic ===\n";

    sc_logic one(SC_LOGIC_1);
    sc_logic zero(SC_LOGIC_0);

    auto r1 = cat::repeat<8>(one);   // "11111111"
    auto r2 = cat::repeat<4>(zero);  // "0000"

    CHECK_EQ(r1.to_string(), std::string("11111111"));
    CHECK_EQ(r2.to_string(), std::string("0000"));
}

// ============================================================================
// Test 20: repeat<N>() — with sc_int (preserves sign)
// ============================================================================
void test_repeat_int()
{
    std::cout << "\n=== Test 20: repeat<N>() — with sc_int ===\n";

    sc_int<4> neg(-1);  // "1111"
    sc_int<4> pos(5);   // "0101"

    auto r1 = cat::repeat<2>(neg);  // "11111111"
    auto r2 = cat::repeat<2>(pos);  // "01010101"

    CHECK_EQ(r1.to_string(), std::string("11111111"));
    CHECK_EQ(r2.to_string(), std::string("01010101"));
}

// ============================================================================
// Test 21: repeat<N>() — preserves X/Z
// ============================================================================
void test_repeat_xz()
{
    std::cout << "\n=== Test 21: repeat<N>() — preserves X/Z ===\n";

    sc_lv<2> p("0b1X");

    auto r = cat::repeat<3>(p);  // "1X1X1X"
    CHECK_EQ(r.to_string(), std::string("1X1X1X"));
    INFO_MSG("repeat<3>(1X) = " + r.to_string());
}

// ============================================================================
// Test 22: reverse() — bit order reversal
// ============================================================================
void test_reverse()
{
    std::cout << "\n=== Test 22: reverse() ===\n";

    sc_lv<8> v("0b10110100");

    auto r = cat::reverse(v);  // "00101101"
    CHECK_EQ(r.to_string(), std::string("00101101"));
    INFO_MSG("reverse(10110100) = " + r.to_string());
}

// ============================================================================
// Test 23: reverse() — with sc_int
// ============================================================================
void test_reverse_int()
{
    std::cout << "\n=== Test 23: reverse() — with sc_int ===\n";

    sc_int<8> v(0b00010000);  // "00010000"

    auto r = cat::reverse(v);  // "00001000"
    CHECK_EQ(r.to_string(), std::string("00001000"));
}

// ============================================================================
// Test 24: reverse() — preserves X/Z
// ============================================================================
void test_reverse_xz()
{
    std::cout << "\n=== Test 24: reverse() — preserves X/Z ===\n";

    sc_lv<4> v("0b1X0Z");

    auto r = cat::reverse(v);  // "Z0X1"
    CHECK_EQ(r.to_string(), std::string("Z0X1"));
    INFO_MSG("reverse(1X0Z) = " + r.to_string());
}

// ============================================================================
// Test 25: zero_extend<W>() — pad with zeros
// ============================================================================
void test_zero_extend()
{
    std::cout << "\n=== Test 25: zero_extend<W>() ===\n";

    sc_lv<4> v("0b1010");

    auto r8  = cat::zero_extend<8>(v);   // "00001010"
    auto r16 = cat::zero_extend<16>(v);  // "0000000000001010"

    CHECK_EQ(r8.to_string(), std::string("00001010"));
    CHECK_EQ(r16.to_string(), std::string("0000000000001010"));
    INFO_MSG("zero_extend<8>(1010) = " + r8.to_string());
}

// ============================================================================
// Test 26: zero_extend<W>() — with sc_int (treats as unsigned)
// ============================================================================
void test_zero_extend_int()
{
    std::cout << "\n=== Test 26: zero_extend<W>() — with sc_int ===\n";

    sc_int<4> v(-1);  // "1111" (as 4-bit pattern)

    auto r = cat::zero_extend<8>(v);  // "00001111"
    CHECK_EQ(r.to_string(), std::string("00001111"));
    INFO_MSG("zero_extend<8>(sc_int<4>(-1)) = " + r.to_string());
}

// ============================================================================
// Test 27: sign_extend<W>() — positive (MSB=0)
// ============================================================================
void test_sign_extend_positive()
{
    std::cout << "\n=== Test 27: sign_extend<W>() — positive ===\n";

    sc_lv<4> v("0b0101");  // MSB=0 → positive

    auto r8 = cat::sign_extend<8>(v);  // "00000101"
    CHECK_EQ(r8.to_string(), std::string("00000101"));
    INFO_MSG("sign_extend<8>(0101) = " + r8.to_string());
}

// ============================================================================
// Test 28: sign_extend<W>() — negative (MSB=1)
// ============================================================================
void test_sign_extend_negative()
{
    std::cout << "\n=== Test 28: sign_extend<W>() — negative ===\n";

    sc_lv<4> v("0b1010");  // MSB=1 → negative

    auto r8 = cat::sign_extend<8>(v);  // "11111010"
    CHECK_EQ(r8.to_string(), std::string("11111010"));
    INFO_MSG("sign_extend<8>(1010) = " + r8.to_string());
}

// ============================================================================
// Test 29: sign_extend<W>() — with sc_int (actual signed value)
// ============================================================================
void test_sign_extend_int()
{
    std::cout << "\n=== Test 29: sign_extend<W>() — with sc_int ===\n";

    sc_int<4> neg(-6);  // "1010" (4-bit two's complement)
    sc_int<4> pos(5);   // "0101"

    auto r_neg = cat::sign_extend<8>(neg);  // "11111010"
    auto r_pos = cat::sign_extend<8>(pos);  // "00000101"

    CHECK_EQ(r_neg.to_string(), std::string("11111010"));
    CHECK_EQ(r_pos.to_string(), std::string("00000101"));
}

// ============================================================================
// Test 30: sign_extend<W>() — preserves X/Z in MSB
// ============================================================================
void test_sign_extend_xz()
{
    std::cout << "\n=== Test 30: sign_extend<W>() — preserves X/Z MSB ===\n";

    sc_lv<4> v("0bX010");  // MSB=X

    auto r = cat::sign_extend<8>(v);  // "XXXXX010"
    CHECK_EQ(r.to_string(), std::string("XXXXX010"));
    INFO_MSG("sign_extend<8>(X010) = " + r.to_string());
}

// ============================================================================
// Test 31: Combined operations — build a packet
// ============================================================================
void test_combined_packet()
{
    std::cout << "\n=== Test 31: Combined — build a packet ===\n";

    // Build a 32-bit packet: [opcode(4)] [addr(12)] [data(16)]
    sc_lv<4>  opcode("0b1010");
    sc_lv<12> addr("0b000111000111");
    sc_lv<16> data("0b1010101010101010");

    auto packet = cat::cat(opcode, addr, data);
    CHECK_EQ(packet.to_string(), std::string("10100001110001111010101010101010"));

    // Extract fields back
    auto op_extracted  = cat::slice<31, 28>(packet);
    auto addr_extracted = cat::slice<27, 16>(packet);
    auto data_extracted = cat::lsb<16>(packet);

    CHECK_EQ(op_extracted.to_string(),   std::string("1010"));
    CHECK_EQ(addr_extracted.to_string(), std::string("000111000111"));
    CHECK_EQ(data_extracted.to_string(), std::string("1010101010101010"));
    INFO_MSG("Packet built and fields extracted correctly");
}

// ============================================================================
// Test 32: Combined — register field manipulation
// ============================================================================
void test_combined_register()
{
    std::cout << "\n=== Test 32: Combined — register fields ===\n";

    // 16-bit register: [reserved(4)] [enable(1)] [mode(3)] [count(8)]
    sc_lv<16> reg("0b0000000000000000");

    // Set enable=1, mode=5, count=42
    sc_lv<1> enable("0b1");
    sc_lv<3> mode("0b101");
    sc_lv<8> count("0b00101010");  // 42

    // Build new register value
    sc_lv<4> reserved("0b0000");
    reg = cat::cat(reserved, enable, mode, count);

    CHECK_EQ(reg.to_string(), std::string("0000110100101010"));

    // Read back individual fields
    auto en_read  = cat::slice<11, 11>(reg);
    auto mode_read = cat::slice<10, 8>(reg);
    auto count_read = cat::lsb<8>(reg);

    CHECK_EQ(en_read.to_string(),   std::string("1"));
    CHECK_EQ(mode_read.to_string(), std::string("101"));
    CHECK_EQ(count_read.to_string(), std::string("00101010"));
    INFO_MSG("Register fields set and read correctly");
}

// ============================================================================
// Test 33: Combined — sign extension + concat
// ============================================================================
void test_combined_sign_extend_cat()
{
    std::cout << "\n=== Test 33: Combined — sign extend + cat ===\n";

    // Sign-extend a small signed value and concat with a tag
    sc_int<4> small_val(-2);  // "1110"
    sc_lv<4>  tag("0b0001");

    auto extended = cat::sign_extend<8>(small_val);  // "11111110"
    auto result   = cat::cat(tag, extended);          // "000111111110"

    CHECK_EQ(extended.to_string(), std::string("11111110"));
    CHECK_EQ(result.to_string(),   std::string("000111111110"));
    INFO_MSG("Sign-extended small value and concatenated with tag");
}

// ============================================================================
// Test 34: cat<sc_bigint>() — wide type
// ============================================================================
void test_cat_bigint()
{
    std::cout << "\n=== Test 34: cat<sc_bigint<>>() — wide type ===\n";

    sc_lv<32> a("0b00000000000000000000000000000001");
    sc_lv<32> b("0b00000000000000000000000000000010");

    sc_bigint<64> r = cat::cat<sc_bigint<64>>(a, b);
    CHECK_EQ(r.to_string(SC_DEC), std::string("4294967298"));  // (1 << 32) + 2
    INFO_MSG("cat<sc_bigint<64>>(1, 2) = " + r.to_string(SC_DEC));
}

// ============================================================================
// Test 35: cat<sc_biguint>() — wide unsigned
// ============================================================================
void test_cat_biguint()
{
    std::cout << "\n=== Test 35: cat<sc_biguint<>>() — wide unsigned ===\n";

    sc_lv<32> a("0b11111111111111111111111111111111");  // 0xFFFFFFFF
    sc_lv<32> b("0b00000000000000000000000000000001");  // 0x00000001

    sc_biguint<64> r = cat::cat<sc_biguint<64>>(a, b);
    // 0xFFFFFFFF00000001 = 18446744069414584321
    CHECK_EQ(r.to_string(SC_DEC), std::string("18446744069414584321"));
    INFO_MSG("cat<sc_biguint<64>>(0xFFFFFFFF, 0x1) = " + r.to_string(SC_DEC));
}

// ============================================================================
// Test 36: reverse() + repeat() combined
// ============================================================================
void test_combined_reverse_repeat()
{
    std::cout << "\n=== Test 36: Combined — reverse + repeat ===\n";

    sc_lv<4> p("0b1000");

    auto rev = cat::reverse(p);  // "0001"
    auto rep = cat::repeat<4>(rev);  // "0001000100010001"

    CHECK_EQ(rev.to_string(), std::string("0001"));
    CHECK_EQ(rep.to_string(), std::string("0001000100010001"));
    INFO_MSG("repeat<4>(reverse(1000)) = " + rep.to_string());
}

// ============================================================================
// Test 37: Nested cat() calls
// ============================================================================
void test_nested_cat()
{
    std::cout << "\n=== Test 37: Nested cat() ===\n";

    sc_lv<2> a("0b11");
    sc_lv<2> b("0b10");
    sc_lv<2> c("0b01");
    sc_lv<2> d("0b00");

    auto left  = cat::cat(a, b);   // "1110"
    auto right = cat::cat(c, d);   // "0100"
    auto result = cat::cat(left, right);  // "11100100"

    CHECK_EQ(left.to_string(),   std::string("1110"));
    CHECK_EQ(right.to_string(),  std::string("0100"));
    CHECK_EQ(result.to_string(), std::string("11100100"));
}

// ============================================================================
// Test 38: cat() with sc_logic in various positions
// ============================================================================
void test_cat_with_logic()
{
    std::cout << "\n=== Test 38: cat() — with sc_logic in positions ===\n";

    sc_logic hi(SC_LOGIC_1);
    sc_logic lo(SC_LOGIC_0);
    sc_lv<4> mid("0b1010");

    auto r1 = cat::cat(hi, mid, lo);  // "110100"
    auto r2 = cat::cat(lo, mid, hi);  // "010101"

    CHECK_EQ(r1.to_string(), std::string("110100"));
    CHECK_EQ(r2.to_string(), std::string("010101"));
}

// ============================================================================
// Test 39: slice() from sc_uint
// ============================================================================
void test_slice_uint()
{
    std::cout << "\n=== Test 39: slice() — from sc_uint ===\n";

    sc_uint<8> v(0xAB);  // 10101011

    auto hi_nibble = cat::msb<4>(v);  // "1010"
    auto lo_nibble = cat::lsb<4>(v);  // "1011"

    CHECK_EQ(hi_nibble.to_string(), std::string("1010"));
    CHECK_EQ(lo_nibble.to_string(), std::string("1011"));
}

// ============================================================================
// Test 40: Edge case — extend same width
// ============================================================================
void test_extend_same_width()
{
    std::cout << "\n=== Test 40: Edge — extend same width ===\n";

    sc_lv<8> v("0b10110100");

    auto ze = cat::zero_extend<8>(v);
    auto se = cat::sign_extend<8>(v);

    CHECK_EQ(ze.to_string(), std::string("10110100"));
    CHECK_EQ(se.to_string(), std::string("10110100"));
    INFO_MSG("Extending to same width is identity");
}

// ============================================================================
// Main
// ============================================================================
int sc_main(int argc, char* argv[])
{
    std::cout << "================================\n";
    std::cout << "sc-concat Test Suite\n";
    std::cout << "================================\n";

    // Basic cat
    test_cat_basic_two();
    test_cat_basic_three();
    test_cat_basic_four();
    test_cat_mixed_types();
    test_cat_with_int_types();

    // Explicit type cat
    test_cat_explicit_bv();
    test_cat_explicit_int();
    test_cat_explicit_uint();

    // cat properties
    test_cat_preserves_xz();
    test_cat_single();

    // slice
    test_slice_middle();
    test_slice_single_bit();
    test_slice_full();
    test_slice_preserves_xz();

    // msb / lsb
    test_msb();
    test_lsb();
    test_slice_with_int();

    // repeat
    test_repeat_pattern();
    test_repeat_logic();
    test_repeat_int();
    test_repeat_xz();

    // reverse
    test_reverse();
    test_reverse_int();
    test_reverse_xz();

    // zero_extend
    test_zero_extend();
    test_zero_extend_int();

    // sign_extend
    test_sign_extend_positive();
    test_sign_extend_negative();
    test_sign_extend_int();
    test_sign_extend_xz();

    // Combined scenarios
    test_combined_packet();
    test_combined_register();
    test_combined_sign_extend_cat();
    test_combined_reverse_repeat();
    test_nested_cat();
    test_cat_with_logic();
    test_slice_uint();
    test_extend_same_width();

    // Wide types
    test_cat_bigint();
    test_cat_biguint();

    std::cout << "\n================================\n";
    std::cout << "SUMMARY\n";
    std::cout << "================================\n";
    std::cout << "Passed : " << g_pass << "\n";
    std::cout << "Failed : " << g_fail << "\n";

    if (g_fail == 0) {
        std::cout << "\n*** ALL TESTS PASSED ***\n";
        return 0;
    } else {
        std::cout << "\n*** " << g_fail << " TEST(S) FAILED ***\n";
        return 1;
    }
}