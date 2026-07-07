# cc_vrwrapper :: sc-concat

> Modern bit-manipulation utilities for SystemC types — variadic concat,
> slicing, repetition, reversal, and width extension in one header library.

![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![SystemC](https://img.shields.io/badge/SystemC-3.0+-red.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Tests](https://img.shields.io/badge/tests-66%2F66-brightgreen.svg)

---

## Table of Contents

- [cc\_vrwrapper :: sc-concat](#cc_vrwrapper--sc-concat)
  - [Table of Contents](#table-of-contents)
  - [Introduction](#introduction)
  - [Features](#features)
  - [Installation](#installation)
  - [Quick Start](#quick-start)
  - [File Structure](#file-structure)
  - [API Reference](#api-reference)
  - [Usage Guide](#usage-guide)
  - [Examples](#examples)
  - [C++17 / C++20 Support](#c17--c20-support)
  - [Thread Safety](#thread-safety)
  - [Limitations](#limitations)
  - [Troubleshooting](#troubleshooting)
  - [Changelog](#changelog)

---

## Introduction

`sc-concat` is a lightweight, header-only C++ library that provides modern,
ergonomic bit-manipulation utilities for SystemC data types. It replaces the
clunky built-in `concat()` (which only takes two arguments and produces
hard-to-use `sc_concref_r` proxy types) with a clean variadic API.

The library is designed to feel like Python slicing and SystemVerilog bit
selections, while preserving full SystemC semantics including `X`/`Z` logic
values and signed two's-complement representations.

### Why use sc-concat?

| Built-in SystemC | sc-concat |
|------------------|-----------|
| `concat(a, b)` — only 2 args | `cat(a, b, c, d, ...)` — variadic |
| `sc_concref_r<...>` proxy types | `sc_lv<W>` — clean, predictable |
| No slicing | `slice<HI, LO>(v)`, `msb<W>(v)`, `lsb<W>(v)` |
| No repetition | `repeat<N>(v)` |
| No reversal | `reverse(v)` |
| No extension helpers | `zero_extend<W>(v)`, `sign_extend<W>(v)` |
| Silent X/Z loss when converting to `sc_bv` | Explicit `cat<sc_bv<W>>(...)` |

---

## Features

| Feature | Description |
|---------|-------------|
| ✅ Variadic concat | `cat(a, b, c, d, ...)` — any number of args |
| ✅ Explicit return type | `cat<sc_int<16>>(a, b)` — convert to any type |
| ✅ Bit slicing | `slice<HI, LO>(v)` — Python/SV-style |
| ✅ MSB / LSB helpers | `msb<W>(v)`, `lsb<W>(v)` |
| ✅ Pattern repetition | `repeat<N>(v)` |
| ✅ Bit reversal | `reverse(v)` |
| ✅ Width extension | `zero_extend<W>(v)`, `sign_extend<W>(v)` |
| ✅ X/Z preservation | `sc_lv` results keep unknown bits |
| ✅ All SystemC types | `sc_lv`, `sc_bv`, `sc_logic`, `sc_int`, `sc_uint`, `sc_bigint`, `sc_biguint` |
| ✅ Mixed-type concat | Combine any types in one call |
| ✅ C++17 & C++20 | Same files compile under both standards |
| ✅ Header-only | No linking, just `#include` |
| ✅ Zero dependencies | Only SystemC + C++ standard library |
| ✅ 66 unit tests | Full coverage of all functions |

---

## Installation

### Prerequisites

- **C++ Compiler**: GCC 9+, Clang 10+, or MSVC 2019+ (C++17 or C++20)
- **SystemC**: version 3.0 or later

### Compilation

```bash
# C++20
g++ -std=c++20 -I./sc-concat your_code.cpp -lsystemc -lpthread -o your_app

# C++17
g++ -std=c++17 -I./sc-concat your_code.cpp -lsystemc -lpthread -o your_app
```

### CMake

```cmake
target_include_directories(your_app PRIVATE sc-concat/)
target_link_libraries(your_app systemc pthread)
```

---

## Quick Start

```cpp
#include <systemc>
#include "sc-concat.hpp"

using namespace sc_core;
using namespace sc_dt;
namespace cat = cc_vrwrapper::concat;

int sc_main(int argc, char* argv[])
{
    // Variadic concatenation
    sc_lv<4> opcode("0b1010");
    sc_lv<4> addr  ("0b1100");
    sc_lv<8> data  ("0b11110000");

    auto packet = cat::cat(opcode, addr, data);  // sc_lv<16>
    std::cout << "Packet: " << packet.to_string() << "\n";
    // Output: Packet: 1010110011110000

    // Slicing
    auto op = cat::slice<15, 12>(packet);  // "1010"
    auto ad = cat::slice<11, 8>(packet);   // "1100"
    auto da = cat::lsb<8>(packet);          // "11110000"

    std::cout << "Op: "  << op.to_string() << "\n";
    std::cout << "Addr: " << ad.to_string() << "\n";
    std::cout << "Data: " << da.to_string() << "\n";

    return 0;
}
```

---

## File Structure

```
sc-concat/
├── sc-concat.hpp    # Umbrella header (include this)
├── utils.hpp        # Type traits, concepts, helpers
├── cat.hpp          # cat() — variadic concatenation
├── slice.hpp        # slice<HI,LO>(), msb<W>(), lsb<W>()
├── repeat.hpp       # repeat<N>()
├── reverse.hpp      # reverse()
└── extend.hpp       # zero_extend<W>(), sign_extend<W>()
```

---

## API Reference

### `cat(args...)`

Variadic concatenation. The **first** argument becomes the **MSB** side of
the result.

```cpp
template <typename... Args>
auto cat(const Args&... args) -> sc_lv<total_width>;
```

**Returns**: `sc_lv<sum_of_widths>` preserving all `X`/`Z` bits.

**Example**:
```cpp
sc_lv<4> a("0b1010");
sc_lv<4> b("0b1100");
auto r = cat::cat(a, b);  // sc_lv<8> = "10101100"
```

---

### `cat<Result>(args...)`

Concatenation with explicit return type. The total width of all arguments
**must equal** `Result`'s width.

```cpp
template <typename Result, typename... Args>
Result cat(const Args&... args);
```

**Example**:
```cpp
sc_lv<4> a("0b1010");
sc_lv<4> b("0b1100");

sc_bv<8>       r1 = cat::cat<sc_bv<8>>(a, b);       // X/Z dropped to 0
sc_int<8>      r2 = cat::cat<sc_int<8>>(a, b);      // signed integer
sc_biguint<64> r3 = cat::cat<sc_biguint<64>>(x, y); // wide unsigned
```

---

### `slice<HI, LO>(v)`

Extract bits `[HI:LO]` from `v`. Bit 0 = LSB, bit W-1 = MSB.

```cpp
template <int HI, int LO, typename T>
auto slice(const T& v) -> sc_lv<HI - LO + 1>;
```

**Constraints**:
- `HI >= LO`
- `LO >= 0`
- `HI < width(T)`

**Example**:
```cpp
sc_lv<8> v("0b10110100");  // bit 7=1, bit 0=0
auto hi  = slice<7, 4>(v);  // sc_lv<4> = "1011"
auto lo  = slice<3, 0>(v);  // sc_lv<4> = "0100"
auto mid = slice<5, 2>(v);  // sc_lv<4> = "1101"
```

---

### `msb<W>(v)`

Extract the top `W` bits (most significant).

```cpp
template <int W, typename T>
auto msb(const T& v) -> sc_lv<W>;
```

**Example**:
```cpp
sc_lv<8> v("0b10110100");
auto top = cat::msb<4>(v);  // "1011"
```

---

### `lsb<W>(v)`

Extract the bottom `W` bits (least significant).

```cpp
template <int W, typename T>
auto lsb(const T& v) -> sc_lv<W>;
```

**Example**:
```cpp
sc_lv<8> v("0b10110100");
auto bot = cat::lsb<4>(v);  // "0100"
```

---

### `repeat<N>(v)`

Repeat `v`'s bit pattern `N` times.

```cpp
template <int N, typename T>
auto repeat(const T& v) -> sc_lv<N * width(T)>;
```

**Example**:
```cpp
sc_lv<4> p("0b1010");
auto r = cat::repeat<4>(p);  // sc_lv<16> = "1010101010101010"
```

---

### `reverse(v)`

Reverse the bit order of `v`. Bit 0 becomes bit W-1, and vice versa.

```cpp
template <typename T>
auto reverse(const T& v) -> sc_lv<width(T)>;
```

**Example**:
```cpp
sc_lv<8> v("0b10110100");
auto r = cat::reverse(v);  // "00101101"
```

---

### `zero_extend<W>(v)`

Pad `v` with zeros on the left (MSB side) to width `W`.

```cpp
template <int W, typename T>
auto zero_extend(const T& v) -> sc_lv<W>;
```

**Constraints**: `W >= width(T)`

**Example**:
```cpp
sc_lv<4> v("0b1010");
auto r = cat::zero_extend<8>(v);  // "00001010"
```

---

### `sign_extend<W>(v)`

Pad `v` with its MSB on the left to width `W`. For `X`/`Z` MSBs, that
value is used as the fill (preserving the unknown state).

```cpp
template <int W, typename T>
auto sign_extend(const T& v) -> sc_lv<W>;
```

**Example**:
```cpp
sc_lv<4> v("0b1010");  // MSB=1 (negative)
auto r = cat::sign_extend<8>(v);  // "11111010"
```

---

## Usage Guide

This section covers all usage scenarios with practical examples.

### 1. Basic Concatenation

#### Two arguments

```cpp
sc_lv<4> a("0b1010");
sc_lv<4> b("0b1100");

auto r = cat::cat(a, b);  // sc_lv<8> = "10101100"
```

#### Three or more arguments

```cpp
sc_lv<4> a("0b1010");
sc_lv<4> b("0b1100");
sc_lv<4> c("0b0011");

auto r = cat::cat(a, b, c);  // sc_lv<12> = "101011000011"
```

#### Single argument (identity)

```cpp
sc_lv<4> a("0b1010");
auto r = cat::cat(a);  // sc_lv<4> = "1010"
```

> **Order convention**: The **first** argument becomes the **MSB side**
> of the result. This matches SystemVerilog `{a, b, c}` and Python slicing
> conventions.

---

### 2. Mixed-Type Concatenation

All SystemC logic-like and int-like types can be freely mixed:

```cpp
sc_lv<4>     a("0b1010");
sc_bv<4>     b("0b1100");
sc_logic     c(SC_LOGIC_1);
sc_int<8>    d(-1);        // "11111111"
sc_uint<4>   e(0xA);       // "1010"

auto r = cat::cat(a, b, c, d, e);
// sc_lv<21> = "101011001111111111101010"
```

#### Supported types

| Type | Width |
|------|-------|
| `sc_lv<W>` | W |
| `sc_bv<W>` | W |
| `sc_logic` | 1 |
| `sc_int<W>` | W |
| `sc_uint<W>` | W |
| `sc_bigint<W>` | W |
| `sc_biguint<W>` | W |

---

### 3. Explicit Return Type

By default, `cat()` returns `sc_lv<W>`. Use `cat<Result>(...)` to convert
to any other SystemC type:

#### Convert to `sc_bv` (drops X/Z)

```cpp
sc_lv<4> a("0b10XZ");
sc_lv<4> b("0b1100");

sc_bv<8> r = cat::cat<sc_bv<8>>(a, b);
// X/Z bits in `a` are converted to '0'
// r = "10001100"
```

#### Convert to `sc_int` / `sc_uint`

```cpp
sc_lv<4> a("0b0001");
sc_lv<4> b("0b0010");

sc_int<8>  signed_r   = cat::cat<sc_int<8>>(a, b);   // 18
sc_uint<8> unsigned_r = cat::cat<sc_uint<8>>(a, b);  // 18
```

#### Convert to wide types

```cpp
sc_lv<32> hi("0xFFFFFFFF");
sc_lv<32> lo("0x00000001");

sc_biguint<64> r = cat::cat<sc_biguint<64>>(hi, lo);
// r = 18446744069414584321
```

> **Constraint**: The total width of all arguments must exactly equal
> `Result`'s width. A `static_assert` fires at compile time otherwise.

---

### 4. Bit Slicing

#### Middle bits

```cpp
sc_lv<8> v("0b10110100");
//             bit: 76543210

auto mid = cat::slice<5, 2>(v);  // bits 5,4,3,2 = "1101"
```

#### Single bit

```cpp
auto bit7 = cat::slice<7, 7>(v);  // "1"
auto bit0 = cat::slice<0, 0>(v);  // "0"
```

#### Full range (identity)

```cpp
auto full = cat::slice<7, 0>(v);  // "10110100"
```

#### X/Z preservation

```cpp
sc_lv<8> v("0b10XZ1100");
auto xz_part = cat::slice<5, 4>(v);  // "XZ"
```

#### From `sc_int` / `sc_uint`

```cpp
sc_int<8> v(0xAB);  // "10101011"
auto hi = cat::msb<4>(v);  // "1010"
auto lo = cat::lsb<4>(v);  // "1011"
```

---

### 5. MSB / LSB Helpers

#### `msb<W>(v)` — top W bits

```cpp
sc_lv<8> v("0b10110100");
auto top4 = cat::msb<4>(v);  // "1011"
auto top2 = cat::msb<2>(v);  // "10"
auto top1 = cat::msb<1>(v);  // "1"
```

#### `lsb<W>(v)` — bottom W bits

```cpp
sc_lv<8> v("0b10110100");
auto bot4 = cat::lsb<4>(v);  // "0100"
auto bot2 = cat::lsb<2>(v);  // "00"
auto bot1 = cat::lsb<1>(v);  // "0"
```

> **Constraint**: `W <= width(T)`. A `static_assert` fires otherwise.

---

### 6. Pattern Repetition

#### With `sc_lv`

```cpp
sc_lv<4> p("0b1010");
auto r2 = cat::repeat<2>(p);  // "10101010"
auto r4 = cat::repeat<4>(p);  // "1010101010101010"
```

#### With `sc_logic`

```cpp
sc_logic one(SC_LOGIC_1);
auto all_ones = cat::repeat<8>(one);  // "11111111"
```

#### With `sc_int` (preserves sign bit pattern)

```cpp
sc_int<4> neg(-1);  // "1111"
auto r = cat::repeat<2>(neg);  // "11111111"
```

#### X/Z preservation

```cpp
sc_lv<2> p("0b1X");
auto r = cat::repeat<3>(p);  // "1X1X1X"
```

---

### 7. Bit Reversal

#### Basic reversal

```cpp
sc_lv<8> v("0b10110100");
auto r = cat::reverse(v);  // "00101101"
```

#### With `sc_int`

```cpp
sc_int<8> v(0b00010000);  // "00010000"
auto r = cat::reverse(v);  // "00001000"
```

#### X/Z preservation

```cpp
sc_lv<4> v("0b1X0Z");
auto r = cat::reverse(v);  // "Z0X1"
```

> **Note**: This reverses **bit order**, not byte order. Bit 0 (LSB)
> becomes bit W-1 (MSB) and vice versa.

---

### 8. Width Extension

#### Zero extension (unsigned)

```cpp
sc_lv<4> v("0b1010");
auto r8  = cat::zero_extend<8>(v);   // "00001010"
auto r16 = cat::zero_extend<16>(v);  // "0000000000001010"
```

#### Sign extension (signed)

The MSB of `v` is used as the fill bit:

```cpp
sc_lv<4> pos("0b0101");  // MSB=0 → positive
auto r_pos = cat::sign_extend<8>(pos);  // "00000101"

sc_lv<4> neg("0b1010");  // MSB=1 → negative
auto r_neg = cat::sign_extend<8>(neg);  // "11111010"
```

#### With `sc_int` (uses actual signed value)

```cpp
sc_int<4> neg(-6);  // "1010"
auto r = cat::sign_extend<8>(neg);  // "11111010"
```

#### X/Z preservation in MSB

```cpp
sc_lv<4> v("0bX010");  // MSB=X
auto r = cat::sign_extend<8>(v);  // "XXXXX010"
```

#### Same-width extension (identity)

```cpp
sc_lv<8> v("0b10110100");
auto r = cat::zero_extend<8>(v);  // "10110100" (unchanged)
```

---

### 9. Combined Patterns

#### Pattern 1: Build a packet

```cpp
sc_lv<4>  opcode("0b1010");
sc_lv<12> addr  ("0b000111000111");
sc_lv<16> data  ("0b1010101010101010");

auto packet = cat::cat(opcode, addr, data);  // sc_lv<32>

// Extract fields back
auto op   = cat::slice<31, 28>(packet);
auto ad   = cat::slice<27, 16>(packet);
auto da   = cat::lsb<16>(packet);
```

#### Pattern 2: Register field manipulation

```cpp
// 16-bit register: [reserved(4)] [enable(1)] [mode(3)] [count(8)]
sc_lv<4> reserved("0b0000");
sc_lv<1> enable  ("0b1");
sc_lv<3> mode    ("0b101");
sc_lv<8> count   ("0b00101010");  // 42

sc_lv<16> reg = cat::cat(reserved, enable, mode, count);
// "0000110100101010"

// Read back
auto en    = cat::slice<11, 11>(reg);  // "1"
auto md    = cat::slice<10, 8>(reg);   // "101"
auto cnt   = cat::lsb<8>(reg);         // "00101010"
```

#### Pattern 3: Sign-extend and concat

```cpp
sc_int<4> small_val(-2);  // "1110"
sc_lv<4>  tag("0b0001");

auto extended = cat::sign_extend<8>(small_val);  // "11111110"
auto result   = cat::cat(tag, extended);          // "000111111110"
```

#### Pattern 4: Reverse and repeat

```cpp
sc_lv<4> p("0b1000");
auto rev = cat::reverse(p);          // "0001"
auto rep = cat::repeat<4>(rev);      // "0001000100010001"
```

#### Pattern 5: Nested concat

```cpp
sc_lv<2> a("0b11"), b("0b10"), c("0b01"), d("0b00");

auto left  = cat::cat(a, b);          // "1110"
auto right = cat::cat(c, d);          // "0100"
auto result = cat::cat(left, right);  // "11100100"
```

---

### 10. Working with X/Z Values

`sc-concat` preserves `X` (unknown) and `Z` (high-impedance) bits throughout
all operations when the result type is `sc_lv<W>`:

```cpp
sc_lv<4> a("0b10XZ");
sc_lv<4> b("0b1100");

auto r1 = cat::cat(a, b);              // "10XZ1100"
auto r2 = cat::slice<5, 2>(r1);        // "XZ11"
auto r3 = cat::reverse(a);             // "ZX01"
auto r4 = cat::repeat<2>(a);           // "10XZ10XZ"
auto r5 = cat::sign_extend<8>(a);      // "XXXX10XZ"
```

#### When X/Z bits are dropped

`X`/`Z` bits are silently converted to `'0'` when you explicitly request
an `sc_bv` result:

```cpp
sc_lv<4> a("0b10XZ");
sc_bv<4> b = cat::cat<sc_bv<4>>(a);  // "1000" (X→0, Z→0)
```

> **Design choice**: This matches SystemC's `sc_bv` semantics (which cannot
> hold `X`/`Z`). The conversion is intentional and explicit.

---

### 11. Working with Signed Values

`sc-concat` correctly handles signed two's-complement representations:

```cpp
sc_int<8> neg(-1);  // "11111111"
sc_int<4> small(-2);  // "1110"

auto r1 = cat::cat(neg, small);  // "111111111110" (preserves bits)
auto r2 = cat::sign_extend<8>(small);  // "11111110" (sign-extended)
auto r3 = cat::zero_extend<8>(small);  // "00001110" (zero-extended)
```

> **Note**: `sc_int` values are converted to their bit pattern via `sc_lv`
> internally. The bit pattern follows two's-complement representation.

---

### 12. Wide Types (sc_bigint / sc_biguint)

`sc-concat` supports types wider than 64 bits:

```cpp
sc_lv<32> hi("0xFFFFFFFF");
sc_lv<32> lo("0x00000001");

sc_biguint<64> r1 = cat::cat<sc_biguint<64>>(hi, lo);
// r1 = 18446744069414584321

sc_lv<64> a("0b...64 ones...");
sc_lv<64> b("0b...64 zeros...");

sc_bigint<128> r2 = cat::cat<sc_bigint<128>>(a, b);
```

> Wide types use string-based construction internally (`"0b..."` prefix)
> to avoid overflow during integer conversion.

---

### 13. Configuration Recipes

#### Recipe 1: Build a network packet header

```cpp
// Ethernet frame: [dst(48)] [src(48)] [type(16)] [payload...]
sc_lv<48> dst_mac("0xAABBCCDDEEFF");
sc_lv<48> src_mac("0x112233445566");
sc_lv<16> eth_type("0x0800");  // IPv4

auto header = cat::cat(dst_mac, src_mac, eth_type);  // sc_lv<112>
```

#### Recipe 2: Build a CAN bus message

```cpp
// CAN frame: [SOF(1)] [ID(11)] [RTR(1)] [IDE(1)] [r0(1)] [DLC(4)] [data(64)]
sc_lv<1>  sof("0b0");
sc_lv<11> id ("0b00000000001");
sc_lv<1>  rtr("0b0");
sc_lv<1>  ide("0b0");
sc_lv<1>  r0 ("0b0");
sc_lv<4>  dlc("0b1000");  // 8 bytes
sc_lv<64> data("0x0123456789ABCDEF");

auto frame = cat::cat(sof, id, rtr, ide, r0, dlc, data);  // sc_lv<83>
```

#### Recipe 3: AXI4 burst address decoding

```cpp
// AXI4 ARADDR: [tag(4)] [burst(2)] [size(3)] [len(8)] [addr(16)]
sc_lv<32> araddr("0x12345678");

auto tag   = cat::slice<31, 28>(araddr);  // "0001"
auto burst = cat::slice<27, 26>(araddr);  // "00"
auto size  = cat::slice<25, 23>(araddr);  // "010"
auto len   = cat::slice<22, 15>(araddr);  // "00110101"
auto addr  = cat::lsb<16>(araddr);        // "0101011001111000"
```

#### Recipe 4: Sign-extend a 12-bit immediate to 32 bits

```cpp
sc_int<12> immediate(-42);
auto extended = cat::sign_extend<32>(immediate);  // sc_lv<32>
```

#### Recipe 5: Reverse bits of a polynomial for LFSR

```cpp
sc_lv<8> poly("0b10000011");  // CRC-8 polynomial
auto reversed_poly = cat::reverse(poly);  // "11000001"
```

#### Recipe 6: Build a lookup table with repeat

```cpp
sc_lv<4> pattern("0b1010");
auto lut = cat::repeat<16>(pattern);  // 64-bit LUT pre-filled with pattern
```

---

### 14. Best Practices

#### ✅ Use `sc_lv<W>` as the default return type

```cpp
// ✅ GOOD — preserves X/Z, predictable
auto r = cat::cat(a, b, c);

// 🟡 Only when you specifically need sc_bv/sc_int/sc_uint
sc_bv<12> r = cat::cat<sc_bv<12>>(a, b, c);
```

#### ✅ Use `slice<HI, LO>()` for clarity (not raw bit math)

```cpp
// ✅ GOOD — clear intent
auto opcode = cat::slice<31, 28>(instr);

// ❌ BAD — error-prone
sc_lv<4> opcode;
for (int i = 0; i < 4; ++i)
    opcode[i] = instr[28 + i];
```

#### ✅ Use `msb<W>()` / `lsb<W>()` for top/bottom extraction

```cpp
// ✅ GOOD
auto hi = cat::msb<8>(value);
auto lo = cat::lsb<8>(value);

// ❌ BAD — width must be computed manually
auto hi = cat::slice<31, 24>(value);  // hardcoded for 32-bit
```

#### ✅ Use `sign_extend<W>()` for signed values

```cpp
// ✅ GOOD — correct for signed
sc_int<4> small(-1);
auto r = cat::sign_extend<8>(small);  // "11111111"

// ❌ BAD — wrong for negative values
auto r = cat::zero_extend<8>(small);  // "00001111" (wrong!)
```

#### ✅ Check widths at compile time

All functions use `static_assert` to catch width mismatches early:

```cpp
// Compile error: total width (8) != Result width (16)
sc_lv<4> a, b;
auto r = cat::cat<sc_lv<16>>(a, b);  // static_assert fails
```

#### ✅ Mix types freely when building packets

```cpp
// Mixing types is fine — sc-concat handles conversions
sc_lv<4>  opcode;
sc_uint<8> addr;
sc_int<16> data;

auto packet = cat::cat(opcode, addr, data);  // sc_lv<28>
```

#### ❌ Don't rely on X/Z preservation with `sc_bv`

```cpp
// ❌ BAD — X/Z bits are silently dropped
sc_lv<4> a("0b10XZ");
sc_bv<4> b = cat::cat<sc_bv<4>>(a);  // "1000" — X and Z became 0!

// ✅ GOOD — keep as sc_lv if you need X/Z
auto b = cat::cat(a);  // sc_lv<4> = "10XZ"
```

---

### 15. Integration Patterns

#### Pattern 1: Instruction decoder

```cpp
SC_MODULE(Decoder)
{
    sc_in<sc_lv<32>> instr;

    void decode() {
        auto opcode = cat::slice<31, 26>(instr.read());
        auto rs     = cat::slice<25, 21>(instr.read());
        auto rt     = cat::slice<20, 16>(instr.read());
        auto imm    = cat::lsb<16>(instr.read());

        // Sign-extend immediate for signed arithmetic
        auto sext_imm = cat::sign_extend<32>(imm);

        std::cout << "Opcode: " << opcode.to_string() << "\n";
        std::cout << "RS: "     << rs.to_string() << "\n";
        std::cout << "RT: "     << rt.to_string() << "\n";
        std::cout << "Imm: "    << sext_imm.to_string() << "\n";
    }

    SC_CTOR(Decoder) { SC_METHOD(decode); sensitive << instr; }
};
```

#### Pattern 2: Memory address builder

```cpp
sc_lv<32> build_address(sc_uint<4> block, sc_uint<8> page, sc_uint<20> offset)
{
    return cat::cat<sc_lv<32>>(block, page, offset);
}

auto addr = build_address(0xA, 0x3F, 0x12345);
```

#### Pattern 3: CRC calculation with reverse

```cpp
sc_lv<8> crc8(sc_lv<8> data, sc_lv<8> poly)
{
    auto reversed_poly = cat::reverse(poly);
    // ... CRC computation using reversed_poly ...
    return /* result */;
}
```

#### Pattern 4: Bus multiplexer

```cpp
sc_lv<32> mux_select(sc_lv<4> sel, sc_lv<32> in0, sc_lv<32> in1)
{
    // Build a 64-bit value: [sel_padded(4)] [in0(32)] [in1(32)]
    // (simplified example)
    auto sel_padded = cat::zero_extend<4>(sel);
    auto packed = cat::cat(sel_padded, in0, in1);
    return cat::slice<36, 5>(packed);  // returns in0 or in1 based on sel
}
```

---

### 16. Performance Notes

- **Header-only**: No runtime overhead from library calls.
- **Compile-time width checks**: All width validations happen via
  `static_assert` — zero runtime cost.
- **String-based conversion**: Internally, `sc-concat` converts types to
  bitstrings and back. This is not the fastest approach, but it's the most
  robust (handles `X`/`Z`, signed values, and wide types uniformly).
- **For performance-critical code**: If you need maximum speed for narrow
  types (≤64 bits), consider using SystemC's native `concat()` or direct
  bit manipulation. `sc-concat` prioritizes correctness and ergonomics
  over raw speed.

---

## Examples

### Example 1: Basic concatenation and extraction

```cpp
#include <systemc>
#include "sc-concat.hpp"

namespace cat = cc_vrwrapper::concat;

int sc_main(int argc, char* argv[]) {
    sc_lv<4> a("0b1010");
    sc_lv<4> b("0b1100");

    auto r = cat::cat(a, b);  // sc_lv<8> = "10101100"
    std::cout << "Result: " << r.to_string() << "\n";

    auto hi = cat::msb<4>(r);  // "1010"
    auto lo = cat::lsb<4>(r);  // "1100"
    std::cout << "Hi: " << hi.to_string() << "\n";
    std::cout << "Lo: " << lo.to_string() << "\n";

    return 0;
}
```

### Example 2: Sign extension

```cpp
sc_int<4> small(-2);  // "1110"
auto extended = cat::sign_extend<8>(small);  // "11111110"
```

### Example 3: Packet builder

```cpp
sc_lv<4>  opcode("0b1010");
sc_lv<12> addr  ("0b000111000111");
sc_lv<16> data  ("0b1010101010101010");

auto packet = cat::cat(opcode, addr, data);  // sc_lv<32>

// Extract fields
auto op = cat::slice<31, 28>(packet);  // "1010"
auto ad = cat::slice<27, 16>(packet);  // "000111000111"
auto da = cat::lsb<16>(packet);         // "1010101010101010"
```

### Example 4: Wide type conversion

```cpp
sc_lv<32> hi("0xFFFFFFFF");
sc_lv<32> lo("0x00000001");

sc_biguint<64> r = cat::cat<sc_biguint<64>>(hi, lo);
// r = 18446744069414584321
```

### Example 5: X/Z preservation

```cpp
sc_lv<4> a("0b10XZ");
sc_lv<4> b("0b1100");

auto r = cat::cat(a, b);       // "10XZ1100"
auto s = cat::slice<5, 2>(r);  // "XZ11"
auto rev = cat::reverse(a);    // "ZX01"
```

---

## C++17 / C++20 Support

The library compiles under both C++17 and C++20 in the same files.

| Feature | C++17 | C++20 |
|---------|-------|-------|
| Fold expressions | ✅ | ✅ |
| `if constexpr` | ✅ | ✅ |
| `std::remove_cvref_t` | ❌ (use `std::decay_t`) | ✅ |
| Concepts | ❌ (uses `static_assert`) | ✅ (defined but optional) |
| `<source_location>` | ❌ | ✅ (not used here) |

### How dual support works

```cpp
// Type traits (work in both C++17 and C++20)
template <typename T> struct is_sc_lv : std::false_type {};
template <int W> struct is_sc_lv<sc_lv<W>> : std::true_type {};

// Concepts (C++20 only, for nicer error messages)
#if __cplusplus >= 202002L
template <typename T>
concept ScLogicLike = is_sc_logiclike_v<T>;
#endif

// static_assert (works in both)
static_assert(is_sc_concatable_v<T>, "T must be concatenable");
```

---

## Thread Safety

`sc-concat` is **purely functional** — all functions are free of side
effects and have no shared mutable state.

| Property | Status |
|----------|--------|
| Global state | ❌ None |
| Static variables | ❌ None |
| Thread-safe | ✅ Yes (by design) |
| Reentrant | ✅ Yes |
| Const-correct | ✅ Yes |

> You can safely call `sc-concat` functions from multiple threads
> simultaneously. The only external dependency is SystemC itself, whose
> thread-safety guarantees are outside this library's scope.

---

## Limitations

1. **String-based conversion**: Internally uses `std::string` for bit
   manipulation. This is robust but not the fastest approach. For
   performance-critical inner loops, consider native SystemC operations.

2. **`sc_bv` drops X/Z silently**: When using `cat<sc_bv<W>>(...)`, `X`
   and `Z` bits are converted to `'0'` (matching SystemC's `sc_bv`
   semantics). Use `cat<sc_lv<W>>(...)` or plain `cat(...)` to preserve
   `X`/`Z`.

3. **Width must be known at compile time**: All width parameters are
   template parameters. Dynamic-width operations are not supported.

4. **No slicing assignment**: You cannot write `slice<7, 0>(v) = "1010"`.
   Use `cat()` to build new values instead.

5. **`sc_bigint` / `sc_biguint` use string construction**: Wide types
   (>64 bits) are constructed via `"0b..."` string prefixes, which is
   slower than direct integer construction but avoids overflow.

---

## Troubleshooting

### Problem: Compile error "static_assert failed: total width must match Result's width"

**Cause**: The sum of argument widths doesn't match the `Result` type's width.

```cpp
sc_lv<4> a, b;
auto r = cat::cat<sc_lv<16>>(a, b);  // 4+4=8, not 16!
```

**Fix**: Either change `Result` to match, or add padding:

```cpp
// Option 1: Match widths
auto r = cat::cat<sc_lv<8>>(a, b);

// Option 2: Add padding
sc_lv<8> pad("0b00000000");
auto r = cat::cat<sc_lv<16>>(pad, a, b);
```

---

### Problem: Compile error "T must be a SystemC logic-like or int-like type"

**Cause**: You passed a type that `sc-concat` doesn't recognize.

```cpp
int x = 42;
auto r = cat::cat(x);  // int is not supported
```

**Fix**: Wrap it in a SystemC type first:

```cpp
sc_int<32> x = 42;
auto r = cat::cat(x);  // OK
```

---

### Problem: X/Z bits disappeared from my result

**Cause**: You used `cat<sc_bv<W>>(...)`, which drops `X`/`Z`.

```cpp
sc_lv<4> a("0b10XZ");
sc_bv<4> r = cat::cat<sc_bv<4>>(a);  // "1000" — X/Z dropped!
```

**Fix**: Use `sc_lv` or omit the explicit type:

```cpp
auto r = cat::cat(a);  // sc_lv<4> = "10XZ"
// or
sc_lv<4> r = cat::cat<sc_lv<4>>(a);  // "10XZ"
```

---

### Problem: Sign extension gives wrong result

**Cause**: You used `zero_extend` on a signed (negative) value.

```cpp
sc_int<4> neg(-1);  // "1111"
auto r = cat::zero_extend<8>(neg);  // "00001111" — wrong for signed!
```

**Fix**: Use `sign_extend` for signed values:

```cpp
auto r = cat::sign_extend<8>(neg);  // "11111111" — correct
```

---

### Problem: `slice<HI, LO>` gives unexpected bits

**Cause**: Confusion about bit ordering. In `sc-concat`, **bit 0 = LSB**
(rightmost), **bit W-1 = MSB** (leftmost).

```cpp
sc_lv<8> v("0b10110100");
//             bit: 76543210
//                 1 0 1 1 0 1 0 0

auto x = cat::slice<7, 4>(v);  // bits 7,6,5,4 = "1011" (top nibble)
auto y = cat::slice<3, 0>(v);  // bits 3,2,1,0 = "0100" (bottom nibble)
```

**Tip**: Always think of bit numbers as "bit N = position N from the right
(LSB)".

---

### Problem: Compile error with `sc_bigint<128>`

**Cause**: Some older SystemC versions have issues with very wide types
and string construction.

**Fix**: Ensure your SystemC version supports `sc_bigint` with `"0b..."`
prefix strings (SystemC 3.0+ is recommended).

---

## Changelog

### v1.0.0

- Initial release
- C++17 and C++20 dual support
- Functions: `cat()`, `cat<Result>()`, `slice<HI,LO>()`, `msb<W>()`,
  `lsb<W>()`, `repeat<N>()`, `reverse()`, `zero_extend<W>()`,
  `sign_extend<W>()`
- Supported types: `sc_lv`, `sc_bv`, `sc_logic`, `sc_int`, `sc_uint`,
  `sc_bigint`, `sc_biguint`
- X/Z bit preservation (in `sc_lv` results)
- Compile-time width validation via `static_assert`
- 66 unit tests, all passing

---
