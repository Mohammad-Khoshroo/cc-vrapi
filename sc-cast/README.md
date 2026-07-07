# cc_vrwrapper — SystemC Type Cast Library

A lightweight, portable, header-only library for safe and unified conversions between SystemC data types (`sc_lv`, `sc_bv`, `sc_logic`, `sc_int`, `sc_uint`, `sc_bigint`, `sc_biguint`) and native C++ types (`bool`, `int`, `float`, `double`, `std::string`).

The library is provided in two parallel versions:

| Version | Standard | Key Features |
|---------|----------|--------------|
| `sc-cast-c++17` | C++17 | `enable_if`, `if constexpr`, type traits |
| `sc-cast-c++20` | C++20 | Concepts, `std::bit_cast`, `std::ranges`, `std::format`, `std::optional` |

The library follows the C++ cast naming convention (`static_cast`, `dynamic_cast`, `std::bit_cast`, …) by exposing a single template function named `sc_cast<DestType>(source, ...)`.

---

## Table of Contents

- [cc\_vrwrapper — SystemC Type Cast Library](#cc_vrwrapper--systemc-type-cast-library)
  - [Table of Contents](#table-of-contents)
  - [Directory Structure](#directory-structure)
  - [Requirements](#requirements)
  - [Installation](#installation)
  - [Quick Start](#quick-start)
  - [Supported Types](#supported-types)
  - [API Reference](#api-reference)
  - [Usage Guide](#usage-guide)
  - [The `mode` Parameter](#the-mode-parameter)
  - [The `MSB` Parameter](#the-msb-parameter)
  - [String Parsing Rules](#string-parsing-rules)
  - [Error Handling \& Reporting](#error-handling--reporting)
  - [Differences Between C++17 and C++20](#differences-between-c17-and-c20)
  - [Notes \& Limitations](#notes--limitations)
  - [Full Examples](#full-examples)
  - [Testing](#testing)
  - [License](#license)
  - [Contributing](#contributing)

---

## Directory Structure

```text
sc-cast/
├── sc-cast-c++17/
│   ├── utils.hpp         // Traits, helpers, base string_cast
│   ├── sc_lv.hpp         // sc_lv<W> conversions
│   ├── sc_bv.hpp         // sc_bv<W> conversions
│   ├── sc_logic.hpp      // sc_logic conversions
│   ├── sc_int.hpp        // sc_int/sc_uint/sc_bigint/sc_biguint conversions
│   └── sc_cast.hpp       // Master header — includes everything
│
└── sc-cast-c++20/
    ├── utils.hpp         // Concepts, helpers, base string_cast
    ├── sc_lv.hpp         // sc_lv<W> conversions
    ├── sc_bv.hpp         // sc_bv<W> conversions
    ├── sc_logic.hpp      // sc_logic conversions
    ├── sc_int.hpp        // sc_int/sc_uint/sc_bigint/sc_biguint conversions
    └── sc_cast.hpp       // Master header — includes everything
```

**Organization rule:** Each conversion lives in the file of its **destination** type. For example, `sc_int → sc_lv` is implemented in `sc_lv.hpp`, while `sc_lv → sc_int` is in `sc_int.hpp`. This prevents duplicate definitions across files.

---

## Requirements

| Requirement | Version |
|-------------|---------|
| C++ Compiler | GCC ≥ 9, Clang ≥ 10, MSVC ≥ 19.20 (VS 2019 16.0) |
| Standard | `-std=c++17` for v17, `-std=c++20` for v20 |
| SystemC | ≥ 2.3.1 (IEEE 1666-2011), tested with 3.0.1 |
| Standard Library | Full support for `<optional>` and `<string_view>` |

> **ABI note:** `utils.hpp` automatically includes `<cxxabi.h>` only on GCC/Clang. On MSVC, the `type_name` helper falls back to the raw `typeid().name()`.

---

## Installation

This is a header-only library — no compilation or linking is required.

1. Add one of the two folders (`sc-cast-c++17` or `sc-cast-c++20`) to your project's include path.
2. Include only the master header:

```cpp
#include <sc_cast.hpp>
// or
#include "sc_cast.hpp"
```

3. Example compile flags:

```bash
# GCC / Clang
g++ -std=c++20 -I/path/to/sc-cast-c++20 your_file.cpp -lsystemc -lpthread -o your_app

# MSVC (Windows)
cl /std:c++20 /EHsc /I"path\to\sc-cast-c++20" your_file.cpp /link systemc.lib
```

---

## Quick Start

```cpp
#include <sc_cast.hpp>
#include <iostream>

int sc_main(int argc, char* argv[]) {
    using namespace cc_vrwrapper;

    // int → sc_lv<8>
    sc_lv<8> lv = sc_cast<sc_lv<8>>(42);

    // sc_lv → int
    int v = sc_cast<int>(lv);                       // v == 42

    // string → sc_lv (hex)
    sc_lv<16> addr = sc_cast<sc_lv<16>>("0xFF", "address");

    // float → sc_lv<32> (bit-reinterpret)
    sc_lv<32> fbits = sc_cast<sc_lv<32>>(3.14f);

    // sc_lv<32> → float
    float f = sc_cast<float>(fbits);                // f ≈ 3.14f

    // sc_bv<8> → sc_logic (LSB)
    sc_bv<8> bv = sc_cast<sc_bv<8>>(0b00000101);
    sc_logic lsb = sc_cast<sc_logic>(bv);           // SC_LOGIC_1

    // sc_int → sc_uint (direct conversion)
    sc_int<8>  si = -5;
    sc_uint<8> ui = sc_cast<sc_uint<8>>(si);

    // string → sc_bigint (beyond 64 bits)
    sc_bigint<128> big =
        sc_cast<sc_bigint<128>>("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

    return 0;
}
```

---

## Supported Types

### Destination Types

| Type | Concept (C++20) | Description |
|------|-----------------|-------------|
| `sc_lv<W>` | `ScLv` | Logic vector — 0/1/X/Z |
| `sc_bv<W>` | `ScBv` | Bit vector — 0/1 only |
| `sc_logic` | `ScLogic` | Single bit — 0/1/X/Z |
| `sc_int<W>` | `ScInt` | Signed fixed-width integer |
| `sc_uint<W>` | `ScUint` | Unsigned fixed-width integer |
| `sc_bigint<W>` | `ScBigInt` | Signed arbitrary-precision integer |
| `sc_biguint<W>` | `ScBigUint` | Unsigned arbitrary-precision integer |
| `bool`, `int`, `long`, … | `std::integral` | Native C++ integral types |
| `float`, `double` | `std::floating_point` | C++ floating-point types |
| `std::string` | — | Textual representation |

### Source Types

All of the above types are supported as sources, plus:
- `std::string_view`, `std::string`, `const char*` (with `mode` and `base` parameters)

---

## API Reference

### `sc_cast<Dest>(...)`

The main conversion function. The destination type is always specified explicitly as a template argument.

```cpp
// 1. Identity: T → T
template <ScLogicLike T>        T sc_cast(const T& x);

// 2. bool → T
template <ScLogicLike T>        T sc_cast(bool value);
template <ScIntLike T>          T sc_cast(bool value);

// 3. integral → T (with mode and MSB)
template <ScLogicLike T, std::integral I>
    requires (!std::same_as<I, bool>)
T sc_cast(I value, std::string_view mode = "data", bool MSB = false);

template <ScIntLike T, std::integral I>
    requires (!std::same_as<I, bool>)
T sc_cast(I value, std::string_view mode = "data", bool MSB = false);

// 4. float/double → T
template <ScLogicLike T>        T sc_cast(float value);
template <ScLogicLike T>        T sc_cast(double value);
template <ScIntLike T>          T sc_cast(float value);   // truncates
template <ScIntLike T>          T sc_cast(double value);  // truncates

// 5. string → T  (StringLike constraint prevents implicit bool/int conversions)
template <ScLogicLike T, StringLike S>
T sc_cast(S&& str, std::string_view mode = "data", int base = 2);

template <ScIntLike T, StringLike S>
T sc_cast(S&& str, std::string_view mode = "data", int base = 2);

// 6. cross SystemC conversions (sc_lv ↔ sc_bv ↔ sc_logic ↔ sc_int family)
//    Each overload lives in the file of the destination type.
template <ScLv Dest, ...>        Dest sc_cast(const Src& src);
template <ScBv Dest, ...>        Dest sc_cast(const Src& src);
template <ScLogic Dest, ...>     Dest sc_cast(const Src& src);
template <ScIntLike Dest, ...>   Dest sc_cast(const Src& src);
```

### `string_cast<T>(...)`

A generic helper for converting strings to any type. Internally dispatches to `sc_cast<T>`.

```cpp
template <typename T>
T string_cast(std::string_view str,
              std::string_view mode = "data",
              int base = 2);
```

### `type_name<T>(...)`

A debugging utility that returns the demangled type name.

```cpp
template <typename T>
std::string type_name(const T& var);
```

---

## Usage Guide

This section demonstrates **every supported conversion** in the library. Each subsection groups conversions by source type.

### 1. From `bool`

```cpp
using namespace cc_vrwrapper;

sc_lv<8>    lv = sc_cast<sc_lv<8>>(true);       // "00000001"
sc_bv<8>    bv = sc_cast<sc_bv<8>>(false);      // "00000000"
sc_logic    lg = sc_cast<sc_logic>(true);        // SC_LOGIC_1
sc_int<8>   si = sc_cast<sc_int<8>>(true);       // 1
sc_uint<8>  ui = sc_cast<sc_uint<8>>(true);      // 1
sc_bigint<128>  bi = sc_cast<sc_bigint<128>>(true);   // 1
sc_biguint<128> bu = sc_cast<sc_biguint<128>>(true);  // 1
```

### 2. From integral types (`int`, `uint8_t`, `int64_t`, …)

```cpp
using namespace cc_vrwrapper;

// Basic
sc_lv<8>    lv  = sc_cast<sc_lv<8>>(42);
sc_bv<8>    bv  = sc_cast<sc_bv<8>>(255u);
sc_logic    lg  = sc_cast<sc_logic>(1);           // LSB of 1 → SC_LOGIC_1
sc_int<8>   si  = sc_cast<sc_int<8>>(-5);
sc_uint<8>  ui  = sc_cast<sc_uint<8>>(200);
sc_bigint<128>  bi = sc_cast<sc_bigint<128>>(-1);
sc_biguint<128> bu = sc_cast<sc_biguint<128>>(1000);

// With mode (sign vs zero extension when source is narrower than WIDTH)
sc_lv<8> a = sc_cast<sc_lv<8>>(-1, "data");        // OK → "11111111"
sc_lv<8> b = sc_cast<sc_lv<8>>(-1, "address");     // FATAL — addresses can't be negative

// With MSB (keep upper vs lower bits when source is wider than WIDTH)
uint32_t val = 0xDEADBEEF;
sc_lv<8> lsb = sc_cast<sc_lv<8>>(val, "data", false);  // 0xEF
sc_lv<8> msb = sc_cast<sc_lv<8>>(val, "data", true);   // 0xDE
```

### 3. From `float` / `double`

```cpp
using namespace cc_vrwrapper;

// Bit-reinterpretation (into sc_lv / sc_bv) — preserves IEEE-754 bits
sc_lv<32>  fbits = sc_cast<sc_lv<32>>(3.14f);     // 0x4048F5C3
sc_lv<64>  dbits = sc_cast<sc_lv<64>>(3.14159);   // 0x400921FB54442D18
sc_bv<32>  fbits2 = sc_cast<sc_bv<32>>(1.0f);     // 0x3F800000

// Numeric conversion (into sc_int family) — truncates fractional part
sc_int<32> si = sc_cast<sc_int<32>>(3.14f);       // 3 (WARNING: truncation)
sc_int<64> di = sc_cast<sc_int<64>>(3.14159);     // 3 (WARNING: truncation)

// Note: float/double → sc_logic is NOT supported (32/64 bits won't fit in 1 bit)
```

### 4. From string (`std::string`, `const char*`, `std::string_view`)

```cpp
using namespace cc_vrwrapper;

// ---- Binary (0b prefix, or pure binary with base=2) ----
sc_lv<8>  lv_bin  = sc_cast<sc_lv<8>>("0b1010");              // "11111010" (sign-extended)
sc_lv<8>  lv_pure = sc_cast<sc_lv<8>>("1010", "data", 2);     // "11111010"
sc_lv<8>  lv_addr = sc_cast<sc_lv<8>>("1010", "address", 2);  // "00001010" (zero-extended)
sc_lv<8>  lv_x    = sc_cast<sc_lv<8>>("0bX101");              // "XXXXX101"
sc_lv<8>  lv_z    = sc_cast<sc_lv<8>>("0bZ101");              // "ZZZZZ101"
sc_bv<4>  bv_x    = sc_cast<sc_bv<4>>("0b1X0Z");              // "1000" (X/Z → 0)
sc_int<8> si_bin  = sc_cast<sc_int<8>>("0b1010");             // 10

// ---- Hexadecimal (0x prefix) ----
sc_lv<16> lv_hex  = sc_cast<sc_lv<16>>("0xCAFE");             // "1100101011111110"
sc_lv<12> lv_hexX = sc_cast<sc_lv<12>>("0xX0", "data");       // "XXXXXXXX0000"
sc_lv<12> lv_hexA = sc_cast<sc_lv<12>>("0xX0", "address");    // "0000XXXX0000"
sc_bv<8>  bv_hex  = sc_cast<sc_bv<8>>("0xFF");                // "11111111"
sc_uint<16> ui_hex = sc_cast<sc_uint<16>>("0xCAFE");          // 51966

// ---- Octal (0 prefix followed by digit) ----
sc_lv<9>  lv_oct  = sc_cast<sc_lv<9>>("0777");                // "111111111"
sc_lv<4>  lv_oct2 = sc_cast<sc_lv<4>>("05", "data");          // "1101" (sign-extended)
sc_lv<4>  lv_oct3 = sc_cast<sc_lv<4>>("05", "address");       // "0101" (zero-extended)
sc_uint<16> ui_oct = sc_cast<sc_uint<16>>("0777");            // 511

// ---- Decimal ----
sc_lv<8>  lv_dec  = sc_cast<sc_lv<8>>("255", "address");      // "11111111"
sc_lv<8>  lv_neg  = sc_cast<sc_lv<8>>("-1");                  // "11111111"
sc_lv<8>  lv_neg2 = sc_cast<sc_lv<8>>("-128");                // "10000000"
sc_int<8> si_dec  = sc_cast<sc_int<8>>("42");                 // 42
sc_int<8> si_neg  = sc_cast<sc_int<8>>("-42");                // -42
sc_bigint<128> bi_dec = sc_cast<sc_bigint<128>>("-1");        // -1

// ---- Whitespace handling (auto-stripped with warning) ----
sc_lv<8> lv_ws1 = sc_cast<sc_lv<8>>("0b 1010");               // "11111010"
sc_lv<8> lv_ws2 = sc_cast<sc_lv<8>>(" 0xFF ");                // "11111111"
sc_lv<8> lv_ws3 = sc_cast<sc_lv<8>>("0b101 010");             // "11101010"

// ---- Big numbers > 64 bits ----
sc_biguint<128> bu_big =
    sc_cast<sc_biguint<128>>("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
sc_bigint<128> bi_big =
    sc_cast<sc_bigint<128>>("170141183460469231731687303715884105727");
```

### 5. From `sc_lv<W>`

```cpp
using namespace cc_vrwrapper;

sc_lv<8> lv("01010101");

// To primitives
bool        b  = sc_cast<bool>(lv);               // true (LSB)
uint8_t     u8 = sc_cast<uint8_t>(lv);            // 0x55
int8_t      i8 = sc_cast<int8_t>(lv);             // 0x55
uint64_t    u64 = sc_cast<uint64_t>(lv);          // 0x55
std::string s  = sc_cast<std::string>(lv);        // "01010101"

// With X/Z bits present → integral returns 0 (with warning)
sc_lv<8> lvx("XXXX0001");
int v = sc_cast<int>(lvx);                        // 0

// To float/double (bit-reinterpret)
sc_lv<32>  fbits = sc_cast<sc_lv<32>>(3.14f);
float      f  = sc_cast<float>(fbits);            // 3.14f

sc_lv<64>  dbits = sc_cast<sc_lv<64>>(3.14159);
double     d  = sc_cast<double>(dbits);           // 3.14159

// To other SystemC types
sc_bv<8>    bv  = sc_cast<sc_bv<8>>(lv);          // "01010101"
sc_logic    lg  = sc_cast<sc_logic>(lv);          // SC_LOGIC_1 (LSB)
sc_int<8>   si  = sc_cast<sc_int<8>>(lv);         // 0x55
sc_uint<8>  ui  = sc_cast<sc_uint<8>>(lv);        // 0x55
sc_bigint<128>  bi = sc_cast<sc_bigint<128>>(lv);
sc_biguint<128> bu = sc_cast<sc_biguint<128>>(lv);

// sc_lv with negative value (in signed interpretation)
sc_lv<8> lvneg("11111111");
sc_int<8>  si_neg = sc_cast<sc_int<8>>(lvneg);    // -1
sc_uint<8> ui_neg = sc_cast<sc_uint<8>>(lvneg);   // 255

// sc_lv with X → sc_bv: X → 0
sc_lv<4> lvxz("1X0Z");
sc_bv<4> bvxz = sc_cast<sc_bv<4>>(lvxz);          // "1000"
```

### 6. From `sc_bv<W>`

```cpp
using namespace cc_vrwrapper;

sc_bv<8> bv("01010101");

// To primitives
bool        b  = sc_cast<bool>(bv);               // true (LSB)
uint8_t     u8 = sc_cast<uint8_t>(bv);            // 0x55
int8_t      i8 = sc_cast<int8_t>(bv);             // 0x55
uint64_t    u64 = sc_cast<uint64_t>(bv);          // 0x55
std::string s  = sc_cast<std::string>(bv);        // "01010101"

// To float/double (bit-reinterpret)
sc_bv<32>  fbits = sc_cast<sc_bv<32>>(1.0f);
float      f  = sc_cast<float>(fbits);            // 1.0f

// To other SystemC types
sc_lv<8>    lv  = sc_cast<sc_lv<8>>(bv);          // "01010101"
sc_logic    lg  = sc_cast<sc_logic>(bv);          // SC_LOGIC_1 (LSB)
sc_int<8>   si  = sc_cast<sc_int<8>>(bv);         // 0x55
sc_uint<8>  ui  = sc_cast<sc_uint<8>>(bv);        // 0x55
sc_bigint<128>  bi = sc_cast<sc_bigint<128>>(bv);
sc_biguint<128> bu = sc_cast<sc_biguint<128>>(bv);
```

### 7. From `sc_logic`

```cpp
using namespace cc_vrwrapper;

sc_logic l1(SC_LOGIC_1);
sc_logic l0(SC_LOGIC_0);
sc_logic lx(SC_LOGIC_X);
sc_logic lz(SC_LOGIC_Z);

// To primitives
bool        b1 = sc_cast<bool>(l1);               // true
bool        b0 = sc_cast<bool>(l0);               // false
bool        bx = sc_cast<bool>(lx);               // false (WARNING: X/Z → false)
int         i1 = sc_cast<int>(l1);                // 1
int         i0 = sc_cast<int>(l0);                // 0
int         ix = sc_cast<int>(lx);                // 0 (WARNING: X/Z → 0)
std::string s1 = sc_cast<std::string>(l1);        // "1"
std::string sx = sc_cast<std::string>(lx);        // "X"

// To sc_lv (replicate the value across all bits)
sc_lv<8> lv1 = sc_cast<sc_lv<8>>(l1);             // "11111111"
sc_lv<8> lv0 = sc_cast<sc_lv<8>>(l0);             // "00000000"
sc_lv<8> lvx = sc_cast<sc_lv<8>>(lx);             // "XXXXXXXX"
sc_lv<8> lvz = sc_cast<sc_lv<8>>(lz);             // "ZZZZZZZZ"

// To sc_bv (X/Z → 0 with warning)
sc_bv<8> bv1 = sc_cast<sc_bv<8>>(l1);             // "11111111"
sc_bv<8> bvx = sc_cast<sc_bv<8>>(lx);             // "00000000" (WARNING)

// To sc_int family
sc_int<8>  si = sc_cast<sc_int<8>>(l1);           // 1
sc_uint<8> ui = sc_cast<sc_uint<8>>(l1);          // 1
sc_bigint<128>  bi = sc_cast<sc_bigint<128>>(l1); // 1
sc_biguint<128> bu = sc_cast<sc_biguint<128>>(l1);// 1
```

### 8. From `sc_int<W>` / `sc_uint<W>` / `sc_bigint<W>` / `sc_biguint<W>`

```cpp
using namespace cc_vrwrapper;

sc_int<8>    si  = -5;
sc_uint<8>   ui  = 200;
sc_bigint<128>  bi = -1;
sc_biguint<128> bu = 1000;

// To primitives
bool        b  = sc_cast<bool>(si);               // true (LSB)
int         i  = sc_cast<int>(si);                // -5
int64_t     i64 = sc_cast<int64_t>(bi);           // -1
uint64_t    u64 = sc_cast<uint64_t>(bu);          // 1000
float       f  = sc_cast<float>(si);              // -5.0f (numeric, not bit-reinterpret)
double      d  = sc_cast<double>(ui);             // 200.0
std::string s  = sc_cast<std::string>(si);        // "-5"

// To sc_lv (preserves bit pattern)
sc_lv<8>  lv_si = sc_cast<sc_lv<8>>(si);          // "11111011"
sc_lv<8>  lv_ui = sc_cast<sc_lv<8>>(ui);          // "11001000"
sc_lv<128> lv_bi = sc_cast<sc_lv<128>>(bi);

// To sc_bv
sc_bv<8>  bv_si = sc_cast<sc_bv<8>>(si);          // "11111011"

// To sc_logic (LSB)
sc_logic l = sc_cast<sc_logic>(si);               // SC_LOGIC_1 (LSB of -5)

// Cross conversion within sc_int family
sc_uint<8>  ui_from_si = sc_cast<sc_uint<8>>(si);     // 251 (wrap)
sc_bigint<128>  bi_from_si = sc_cast<sc_bigint<128>>(si);
sc_biguint<128> bu_from_ui = sc_cast<sc_biguint<128>>(ui);

// With mode (sign vs zero extension)
sc_lv<8> a = sc_cast<sc_lv<8>>(si, "data");        // "11111011" (sign-extended)
sc_lv<8> b = sc_cast<sc_lv<8>>(ui, "address");     // "11001000"

// With MSB (keep upper vs lower bits)
uint32_t val = 0xDEADBEEF;
sc_uint<8> lsb = sc_cast<sc_uint<8>>(val, "data", false);  // 0xEF
sc_uint<8> msb = sc_cast<sc_uint<8>>(val, "data", true);   // 0xDE
```

### 9. The `string_cast<T>(...)` helper

A convenience wrapper — behaves identically to `sc_cast<T>` for string sources, but with a cleaner name when the source is obviously a string.

```cpp
using namespace cc_vrwrapper;

// To SystemC types
sc_lv<8>     lv  = string_cast<sc_lv<8>>("0xFF");
sc_int<8>    si  = string_cast<sc_int<8>>("-42");
sc_bigint<128> bi = string_cast<sc_bigint<128>>("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

// To C++ types
int          i   = string_cast<int>("42");                    // 42
unsigned     u   = string_cast<unsigned>("0xFF", "data", 16); // 255
double       d   = string_cast<double>("3.14");               // 3.14
float        f   = string_cast<float>("2.5");                 // 2.5f

// Edge cases
int empty = string_cast<int>("");    // 0 (WARNING: empty string)
int xx    = string_cast<int>("XX");  // 0 (all X → 0)
```

### 10. The `type_name<T>(...)` helper

A debugging utility — returns the demangled type name (GCC/Clang) or raw mangled name (MSVC).

```cpp
using namespace cc_vrwrapper;

sc_lv<8> lv;
sc_int<16> si;
std::cout << type_name(lv) << "\n";   // "sc_dt::sc_lv<8>"
std::cout << type_name(si) << "\n";   // "sc_dt::sc_int<16>"
```

### 11. Conversion Matrix (Quick Reference)

| Source ↓ \ Dest → | `sc_lv` | `sc_bv` | `sc_logic` | `sc_int` family | `bool`/integral | `float`/`double` | `std::string` |
|-------------------|:------:|:------:|:----------:|:---------------:|:---------------:|:----------------:|:--------------:|
| `bool`            | ✅ | ✅ | ✅ | ✅ | — | — | — |
| `int`/integral    | ✅ | ✅ | ✅ | ✅ | — | — | — |
| `float`/`double`  | ✅¹ | ✅¹ | ❌ | ✅² | — | — | — |
| string            | ✅ | ✅ | ✅ | ✅ | — | — | — |
| `sc_lv`           | ✅ | ✅ | ✅ | ✅ | ✅ | ✅¹ | ✅ |
| `sc_bv`           | ✅ | ✅ | ✅ | ✅ | ✅ | ✅¹ | ✅ |
| `sc_logic`        | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ |
| `sc_int` family   | ✅ | ✅ | ✅ | ✅ | ✅ | ✅² | ✅ |

> ¹ = bit-reinterpretation (IEEE-754 bits preserved)
> 
> ² = numeric conversion (fractional part truncated with warning)
> 
> ❌ = not supported (returns error/unknown)

---

## The `mode` Parameter

Only relevant for conversions **from strings** or **from integers** when the source value is narrower than the destination WIDTH:

| Value | Padding behavior | Sign behavior |
|-------|------------------|---------------|
| `"data"` (default) | **Sign extension** — pads with the MSB of the input. The MSB may be `0`, `1`, `X`, or `Z` (X/Z are preserved). | Allows negative values |
| `"address"` | **Zero extension** — always pads with `'0'`. | Rejects negative values (`SC_REPORT_FATAL`) |

### Examples

```cpp
// "0b1010" has MSB='1' → sign-extend with '1'
sc_lv<8> a = sc_cast<sc_lv<8>>("0b1010", "data");     // "11111010"
sc_lv<8> b = sc_cast<sc_lv<8>>("0b1010", "address");  // "00001010"

// "0bX101" has MSB='X' → sign-extend with 'X'
sc_lv<8> c = sc_cast<sc_lv<8>>("0bX101", "data");     // "XXXXX101"

// "0xX0" → binary "XXXX0000", in sc_lv<12>: sign-extend with 'X'
sc_lv<12> d = sc_cast<sc_lv<12>>("0xX0", "data");     // "XXXXXXXX0000"
sc_lv<12> e = sc_cast<sc_lv<12>>("0xX0", "address");  // "0000XXXX0000"

// Negative integer values
sc_lv<8> f = sc_cast<sc_lv<8>>(-1, "data");           // OK → "11111111"
sc_lv<8> g = sc_cast<sc_lv<8>>(-1, "address");        // FATAL ERROR
```

> **Note on signedness:** `sc_lv` and `sc_bv` are plain bit containers and do not know whether they hold signed or unsigned data. The `mode` parameter is how the *user* tells the library how to interpret and extend the value. There is no automatic signedness inference.

---

## The `MSB` Parameter

Only relevant for conversions **from integer types** to `sc_lv` / `sc_bv` / `sc_int` family when the source type is wider than the destination WIDTH:

| Value | Behavior |
|-------|---------|
| `false` (default) | Keeps the **LSBs** (conventional behavior) |
| `true` | Keeps the **MSBs** (useful for MSB-first addressing conventions) |

### Example

```cpp
uint32_t val = 0xDEADBEEF;
sc_lv<8> lsb = sc_cast<sc_lv<8>>(val, "data", false);  // 0xEF
sc_lv<8> msb = sc_cast<sc_lv<8>>(val, "data", true);   // 0xDE
```

---

## String Parsing Rules

The string parser detects the numeric base from a prefix, much like C/C++ integer literals:

| Prefix | Base | Example | Note |
|--------|------|---------|------|
| `0b` / `0B` | 2 | `"0b1010"` | Binary |
| `0x` / `0X` | 16 | `"0xCAFE"` | Hexadecimal |
| `0` followed by digit | 8 | `"0777"` | Octal |
| (none) | 10 (or `base` arg) | `"42"` | Decimal |
| (none), `base=2` | 2 | `"1010"` | Pure binary (only when `base==2`) |

### Special characters

- In `sc_lv`: `X` and `Z` are preserved as logic values.
- In `sc_bv`: `X` and `Z` are not supported; they are silently replaced with `'0'` and a warning is emitted.
- Whitespace (including `_`) is automatically stripped with a warning. This makes the library convenient for parsing values like `"0b1010_1010"`.

### Whitespace example

```cpp
sc_lv<8> a = sc_cast<sc_lv<8>>("0b 1010");          // "11111010" (sign-extended)
sc_lv<8> b = sc_cast<sc_lv<8>>(" 0xFF ");           // "11111111"
sc_lv<8> c = sc_cast<sc_lv<8>>("0b101 010");        // "11101010"
sc_lv<8> d = sc_cast<sc_lv<8>>("0b 1010", "address"); // "00001010"
```

---

## Error Handling & Reporting

The library uses SystemC's standard reporting mechanism (`SC_REPORT_*`):

| Severity | Usage |
|----------|-------|
| `SC_REPORT_INFO` | Non-critical information (e.g., MSB mode activated) |
| `SC_REPORT_WARNING` | Operation continues but the result may be unexpected (overflow, whitespace removal, X/Z in input to `sc_bv`, etc.) |
| `SC_REPORT_ERROR` | Conversion failed; an invalid value is returned (typically "unknown" or 0) |
| `SC_REPORT_FATAL` | Simulation halts (negative width, address mode with negative value, etc.) |

### Default return values on error

| Target type | Default on error |
|-------------|------------------|
| `sc_lv<W>` | All-X string of width W |
| `sc_bv<W>` | All-0 string of width W |
| `sc_logic` | `SC_LOGIC_X` |
| `sc_int` / `sc_uint` / `sc_bigint` / `sc_biguint` | `0` |
| `bool` / integral / floating-point | `0` / `false` / `0.0` |

### Example warnings

```cpp
sc_lv<4> a = sc_cast<sc_lv<4>>(300);
// WARNING: "Input type is larger than WIDTH, may cause unsigned overflow"

sc_bv<8> b = sc_cast<sc_bv<8>>("0b1X0Z");
// WARNING: X/Z bits treated as 0 → result "1000"

sc_lv<8> c = sc_cast<sc_lv<8>>("0b1 0101");
// WARNING: "Whitespace found in input string — it was removed."
```

---

## Differences Between C++17 and C++20

| Feature | C++17 | C++20 |
|---------|-------|-------|
| Type constraint | Verbose `std::enable_if` | Concepts (`ScLv`, `ScIntLike`, `StringLike`, …) — clearer errors |
| Type punning (float ↔ bits) | `std::memcpy` | `std::bit_cast` (compile-time safe) |
| Whitespace removal | `erase-remove` idiom | `std::erase_if` + `std::ranges::any_of` |
| Numeric parsing | `bool` with out-parameter | `std::optional<uint64_t>` |
| Message construction | `std::string + ...` | `std::format` |
| Compiler error messages | Long and obscure | Clear and concise |

**Runtime behavior is identical** — both versions produce the same output for the same input.

---

## Notes & Limitations

1. **`sc_bv` does not support X/Z.** If the input contains X/Z, a warning is issued and the value is converted to `'0'`.
2. **`float` / `double` ↔ `sc_int`** is **numeric** (not bit-reinterpretation). The fractional part is truncated with a warning. For bit-reinterpretation, use `sc_lv`/`sc_bv` instead.
3. **`sc_bigint` / `sc_biguint` > 64 bits**: SystemC's string constructor is used, with the proper prefix (`0x`, `0b`, `0NNN`) re-attached. Negative values are constructed positively and then negated.
4. **Cross-type conversions** are exhaustive: every pair of supported types can be converted both ways. For example, `sc_int → sc_bv → sc_logic → int` is fully valid.
5. **`sc_logic` and floating-point**: Converting `float`/`double` → `sc_logic` results in an error (32/64 bits cannot fit into 1 bit).
6. **Empty strings** trigger `SC_REPORT_ERROR` and return the unknown value.
7. **Unicode**: Only ASCII is supported.
8. **Thread safety**: The functions are stateless and thread-safe as long as the SystemC runtime is.
9. **`StringLike` constraint**: To prevent string literals from being silently converted to `bool` (a common C++ pitfall), the string overloads are constrained to actual string-like types. `sc_cast<sc_logic>("0")` correctly invokes the string parser, not the `bool` overload.

---

## Full Examples

### Example 1: Conversions with Different Bases

```cpp
using namespace cc_vrwrapper;

sc_lv<16> bin = sc_cast<sc_lv<16>>("0b10101010");
sc_lv<16> oct = sc_cast<sc_lv<16>>("0777");
sc_lv<16> hex = sc_cast<sc_lv<16>>("0xCAFE");
sc_lv<16> dec = sc_cast<sc_lv<16>>("43981");        // = 0xABCD
```

### Example 2: Working with sc_bigint

```cpp
using namespace cc_vrwrapper;

// A number larger than 64 bits
sc_bigint<128> big =
    sc_cast<sc_bigint<128>>("170141183460469231731687303715884105727");

// Convert to decimal string
std::string binstr = sc_cast<std::string>(big);

// Convert to sc_lv<128>
sc_lv<128> lv = sc_cast<sc_lv<128>>(big);
```

### Example 3: Parsing a Configuration File

```cpp
using namespace cc_vrwrapper;

// Suppose you read these from a file:
std::string addr_str = "0x1FFC";
std::string data_str = "0b1010_1010_1010";   // with underscores!

// address mode: must be positive, zero-extended
sc_lv<16> addr = string_cast<sc_lv<16>>(addr_str, "address");
// data mode: may be signed, sign-extended
sc_lv<16> data = string_cast<sc_lv<16>>(data_str, "data", 2);
// (underscores are removed as whitespace)
```

### Example 4: Extracting a Field from an Integer

```cpp
using namespace cc_vrwrapper;

uint32_t packet = 0xAABBCCDD;

// LSB byte
sc_lv<8> lsb_byte = sc_cast<sc_lv<8>>(packet, "data", false);  // 0xDD

// MSB byte
sc_lv<8> msb_byte = sc_cast<sc_lv<8>>(packet, "data", true);   // 0xAA

// Convert to integer
uint8_t lsb_val = sc_cast<uint8_t>(lsb_byte);                   // 0xDD
```

### Example 5: sc_bv Behavior with X/Z Input

```cpp
using namespace cc_vrwrapper;

// Input with X — converted to 0
sc_bv<4> bv = sc_cast<sc_bv<4>>("0b1X0Z");
// WARNING: "Target is sc_bv (no X/Z support); X/Z bits will be treated as 0."
// Result: "1000" → 0x8

// sc_lv preserves X/Z
sc_lv<4> lv = sc_cast<sc_lv<4>>("0b1X0Z");
// Result: "1X0Z"
```

### Example 6: Bit-level Float Manipulation

```cpp
using namespace cc_vrwrapper;

// Inspect the bits of a float
sc_lv<32> fbits = sc_cast<sc_lv<32>>(3.14f);
std::cout << fbits.to_string() << "\n";   // 01000000010001111010111000010100

// Round-trip back
float back = sc_cast<float>(fbits);       // 3.14f

// Extract sign bit
sc_logic sign = sc_cast<sc_logic>(fbits); // 0 (positive)
```

### Example 7: Round-trip Conversions

```cpp
using namespace cc_vrwrapper;

// int8 → sc_lv<8> → int8 (all 256 values round-trip correctly)
for (int i = -128; i <= 127; ++i) {
    sc_lv<8> lv = sc_cast<sc_lv<8>>(i);
    int back = sc_cast<int8_t>(lv);
    assert(back == i);
}

// float → sc_lv<32> → float (bit-exact)
for (float f : {0.0f, 1.0f, -1.0f, 3.14f, -2.5f, 1e10f, 1e-10f}) {
    sc_lv<32> lv = sc_cast<sc_lv<32>>(f);
    float back = sc_cast<float>(lv);
    assert(back == f);
}
```

### Example 8: Chained Cross-type Conversions

```cpp
using namespace cc_vrwrapper;

// Start with a string, chain through multiple types
sc_lv<8> lv = sc_cast<sc_lv<8>>("0xFF");          // "11111111"
sc_bv<8> bv = sc_cast<sc_bv<8>>(lv);              // "11111111"
sc_logic l  = sc_cast<sc_logic>(bv);              // SC_LOGIC_1 (LSB)
int     i   = sc_cast<int>(l);                    // 1
double  d   = sc_cast<double>(i);                 // 1.0

// Or: integer → sc_int → sc_lv → string
sc_int<8>  si = sc_cast<sc_int<8>>(-5);
sc_lv<8>   lv2 = sc_cast<sc_lv<8>>(si);           // "11111011"
std::string s  = sc_cast<std::string>(lv2);       // "11111011"
```

---

## Testing

The library ships with a comprehensive test suite (`test_sc_cast.cpp`) covering 30+ sections and 150+ assertions, including:

- bool / integral / float / double → `sc_lv` / `sc_bv` / `sc_logic` / `sc_int` family
- string parsing for binary / hex / octal / decimal (with X/Z, whitespace, sign extension)
- cross-type conversions (`sc_lv ↔ sc_bv ↔ sc_logic ↔ sc_int` family)
- overflow / truncation / MSB mode
- `sc_bigint` / `sc_biguint` beyond 64 bits
- full round-trip verification

### Build and run

```bash
# C++20
g++ -std=c++20 -I./sc-cast-c++20 test_sc_cast.cpp -lsystemc -lpthread -o test_sc_cast
./test_sc_cast

# C++17
g++ -std=c++17 -I./sc-cast-c++17 test_sc_cast.cpp -lsystemc -lpthread -o test_sc_cast
./test_sc_cast
```

Expected output (tail):

```text
================================
SUMMARY
================================
Sections : 30
Passed   : 152
Failed   : 0
Total    : 152

*** ALL TESTS PASSED ***
```

> Some `SC_REPORT_WARNING` messages may appear during the run — these are intentional (the tests deliberately exercise overflow, X/Z-in-`sc_bv`, and whitespace-stripping paths to verify the warnings fire correctly).

---

## License

This library is freely available for your use. Attribution is appreciated (but not required) when used in commercial or academic projects.

## Contributing

To report a bug or suggest a new feature:
1. Describe the issue with expected input/output.
2. Mention your C++ version and compiler.
3. Provide a minimal reproducer if possible.

---

**Library Version:** 2.0.0  
**Date:** 2026-07-07  
**Target:** SystemC IEEE 1666 (tested with 3.0.1-Accellera)
