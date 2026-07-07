# sc_cast — SystemC Type Cast Library

A lightweight, portable library for safe and unified conversions between SystemC data types (`sc_lv`, `sc_bv`, `sc_logic`, `sc_int`, `sc_uint`, `sc_bigint`, `sc_biguint`) and native C++ types (`bool`, `int`, `float`, `double`, `std::string`).

The library is provided in two versions:

| Version | Standard | Key Features |
|---------|----------|--------------|
| `sc-cast-c++17` | C++17 | `enable_if`, `if constexpr`, `std::optional` (where possible) |
| `sc-cast-c++20` | C++20 | Concepts, `std::bit_cast`, `std::ranges`, `std::format`, `std::optional` |

---

## Table of Contents

- [sc\_cast — SystemC Type Cast Library](#sc_cast--systemc-type-cast-library)
  - [Table of Contents](#table-of-contents)
  - [Directory Structure](#directory-structure)
  - [Requirements](#requirements)
  - [Installation](#installation)
  - [Quick Start](#quick-start)
  - [Supported Types](#supported-types)
  - [API Reference](#api-reference)
  - [The `mode` Parameter](#the-mode-parameter)
  - [The `MSB` Parameter](#the-msb-parameter)
  - [Error Handling \& Reporting](#error-handling--reporting)
  - [Differences Between C++17 and C++20](#differences-between-c17-and-c20)
  - [Notes \& Limitations](#notes--limitations)
  - [Full Examples](#full-examples)
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

**Organization rule:** Each conversion lives in the file of its destination type. For example, `sc_int → sc_lv` is implemented in `sc_lv.hpp`, while `sc_lv → sc_int` is in `sc_int.hpp`.

---

## Requirements

| Requirement | Version |
|-------------|---------|
| C++ Compiler | GCC ≥ 9, Clang ≥ 10, MSVC ≥ 19.20 (VS 2019 16.0) |
| Standard | `-std=c++17` for v17, `-std=c++20` for v20 |
| SystemC | ≥ 2.3.1 (IEEE 1666-2011) |
| Standard Library | Full support for `<optional>` and `<string_view>` |

> **ABI note:** `utils.hpp` automatically includes `<cxxabi.h>` only on GCC/Clang. On MSVC, the `type_name` function falls back to the raw `typeid().name()`.

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
g++ -std=c++20 -I/path/to/sc-cast-c++20 your_file.cpp -lsystemc -o your_app

# MSVC (Windows)
cl /std:c++20 /I"path\to\sc-cast-c++20" your_file.cpp /link systemc.lib
```

---

## Quick Start

```cpp
#include <sc_cast.hpp>
#include <iostream>

int main() {
    using namespace sc_cast;

    // int → sc_lv<8>
    sc_lv<8> lv = sc_lv_cast<sc_lv<8>>(42);

    // sc_lv → int
    int v = sc_lv_cast<int>(lv);                       // v == 42

    // string → sc_lv (hex)
    sc_lv<16> addr = sc_lv_cast<sc_lv<16>>("0xFF", "address");

    // float → sc_lv<32> (bit-reinterpret)
    sc_lv<32> fbits = sc_lv_cast<sc_lv<32>>(3.14f);

    // sc_lv<32> → float
    float f = sc_lv_cast<float>(fbits);                // f ≈ 3.14f

    // sc_bv<8> → sc_logic (LSB)
    sc_bv<8> bv = sc_lv_cast<sc_bv<8>>(0b00000101);
    sc_logic lsb = sc_lv_cast<sc_logic>(bv);           // SC_LOGIC_1

    // sc_int → sc_uint (direct conversion)
    sc_int<8>  si = -5;
    sc_uint<8> ui = sc_lv_cast<sc_uint<8>>(si);

    // string → sc_bigint (beyond 64 bits)
    sc_bigint<128> big =
        sc_lv_cast<sc_bigint<128>>("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

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
| `bool`, `int`, `long`, ... | `std::integral` | Native C++ integral types |
| `float`, `double` | `std::floating_point` | C++ floating-point types |
| `std::string` | — | Textual representation |

### Source Types

All of the above types are supported as sources, plus:
- `std::string_view` (with `mode` and `base` parameters)

---

## API Reference

### `sc_lv_cast<Dest>(...)`

The main conversion function. The destination type is always specified explicitly as a template argument.

```cpp
// 1. Identity: T → T
template <typename T> T sc_lv_cast(const T& x);

// 2. bool → T
template <typename T> T sc_lv_cast(bool value);

// 3. integral → T (with mode and MSB)
template <typename T, typename I>
T sc_lv_cast(I value, std::string_view mode = "data", bool MSB = false);

// 4. float/double → T
template <typename T> T sc_lv_cast(float value);
template <typename T> T sc_lv_cast(double value);

// 5. string → T
template <typename T>
T sc_lv_cast(std::string_view str, std::string_view mode = "data", int base = 2);

// 6. cross SystemC conversions (sc_lv ↔ sc_bv ↔ sc_logic ↔ sc_int family)
template <typename Dest, typename Src>
Dest sc_lv_cast(const Src& src);
```

### `string_cast<T>(...)`

A generic helper for converting strings to any type. Internally calls `sc_lv_cast<T>` with the same parameters.

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

## The `mode` Parameter

Only relevant for conversions from strings or from integers:

| Value | Padding behavior (when input is shorter than WIDTH) | Address mode behavior |
|-------|------------------------------------------------------|----------------------|
| `"data"` | Padded with sign bit (sign extension) | Allows negative values |
| `"address"` | Padded with `'0'` | Rejects negative values (FATAL) |

**Example:**

```cpp
sc_lv<8> a = sc_lv_cast<sc_lv<8>>("0b1010", "data");     // "00001010" → 0x0A
sc_lv<8> b = sc_lv_cast<sc_lv<8>>("0b1010", "address");  // "00001010" → 0x0A
// (padding differs only when input is shorter than WIDTH)

sc_lv<8> c = sc_lv_cast<sc_lv<8>>(-1, "data");           // OK → 0xFF
sc_lv<8> d = sc_lv_cast<sc_lv<8>>(-1, "address");        // FATAL ERROR
```
---

## The `MSB` Parameter

Only relevant for conversions from integer types to `sc_lv` / `sc_bv` / `sc_int` family:

| Value | Behavior |
|-------|---------|
| `false` (default) | Keeps the LSBs (conventional behavior) |
| `true` | Keeps the MSBs (useful for MSB-first addressing conventions) |

**Example:**

```cpp
uint32_t val = 0xDEADBEEF;
sc_lv<8> lsb = sc_lv_cast<sc_lv<8>>(val, "data", false);  // 0xEF
sc_lv<8> msb = sc_lv_cast<sc_lv<8>>(val, "data", true);   // 0xDE
```

---

## Error Handling & Reporting

The library uses SystemC's standard reporting mechanism:

| Severity | Usage |
|----------|-------|
| `SC_REPORT_INFO` | Non-critical information (e.g., MSB mode activated) |
| `SC_REPORT_WARNING` | Warning — operation continues but the result may be unexpected (e.g., overflow, whitespace removal, X/Z in input to sc_bv) |
| `SC_REPORT_ERROR` | Error — conversion failed, an invalid value is returned (typically "unknown" or 0) |
| `SC_REPORT_FATAL` | Fatal error — simulation halts (e.g., negative width, address mode with negative value) |

### Behavior on Error

- On invalid input, the function returns a default value:
  - For `sc_lv`: a string of `'X'` characters with the given width
  - For `sc_bv`: a string of `'0'` characters with the given width
  - For `sc_logic`: `SC_LOGIC_X`
  - For `sc_int` family: `0`

### Example Warnings

```cpp
sc_lv<4> a = sc_lv_cast<sc_lv<4>>(300);
// WARNING: "Input type is larger than WIDTH, may cause unsigned overflow"

sc_bv<8> b = sc_lv_cast<sc_bv<8>>("0xZX");
// WARNING: "Target is sc_bv (no X/Z support); X/Z bits will be treated as 0."

sc_lv<8> c = sc_lv_cast<sc_lv<8>>("0b1 0101");
// WARNING: "Whitespace found in input string — it was removed."
```
---

## Differences Between C++17 and C++20

| Feature | C++17 | C++20 |
|---------|-------|-------|
| Type constraint | Verbose `std::enable_if` | Concepts (`ScLv`, `ScIntLike`, ...) — readable, clearer errors |
| Type punning | `memcpy` or `union` | `std::bit_cast` (compile-time safe) |
| Whitespace removal | `erase-remove idiom` | `std::erase_if` + `std::ranges::any_of` |
| Numeric parsing | bool with out-parameter | `std::optional<uint64_t>` |
| Message construction | `std::string + ...` | `std::format` |
| Compiler error messages | Long and obscure | Clear and concise |

**Runtime behavior is identical** — both versions produce the same output for the same input.

---

## Notes & Limitations

1. **`sc_bv` does not support X/Z**: If the input contains X/Z, a warning is issued and the value is converted to `'0'`.
2. **`float`/`double` ↔ `sc_int`**: The conversion is numeric, not a bit-reinterpretation. The fractional part is truncated with a warning. For bit-reinterpretation, use `sc_lv`/`sc_bv` instead.
3. **`sc_bigint` > 64 bits**: For WIDTH > 64, SystemC's direct string constructor is used to avoid `uint64_t` overflow.
4. **Cross-type conversions**: Every pair of supported types can be converted both ways. For example, `sc_int → sc_bv → sc_logic → int` is fully valid.
5. **`sc_logic` and floating-point**: Converting `float`/`double` → `sc_logic` results in an error (32/64 bits cannot fit into 1 bit).
6. **Empty strings**: Converting an empty string to logic types triggers `SC_REPORT_ERROR` and returns the unknown value.
7. **Unicode**: Only ASCII is supported.
8. **Thread safety**: The functions are stateless; they are thread-safe as long as the SystemC runtime is thread-safe.

---

## Full Examples

### Example 1: Conversions with Different Bases

```cpp
using namespace sc_cast;

sc_lv<16> bin = sc_lv_cast<sc_lv<16>>("0b10101010");
sc_lv<16> oct = sc_lv_cast<sc_lv<16>>("0777");
sc_lv<16> hex = sc_lv_cast<sc_lv<16>>("0xCAFE");
sc_lv<16> dec = sc_lv_cast<sc_lv<16>>("43981");        // = 0xABCD
```

### Example 2: Working with sc_bigint

```cpp
using namespace sc_cast;

// A number larger than 64 bits
sc_bigint<128> big =
    sc_lv_cast<sc_bigint<128>>("170141183460469231731687303715884105727");

// Convert to binary string
std::string binstr = sc_lv_cast<std::string>(big);

// Convert back to sc_lv<128>
sc_lv<128> lv = sc_lv_cast<sc_lv<128>>(big);
```

### Example 3: Parsing a Configuration File

```cpp
using namespace sc_cast;

// Suppose you read these from a file:
std::string addr_str = "0x1FFC";
std::string data_str = "0b1010_1010_1010";   // with underscores!

// address mode: must be positive
sc_lv<16> addr = string_cast<sc_lv<16>>(addr_str, "address");
// data mode: may be signed
sc_lv<16> data = string_cast<sc_lv<16>>(data_str, "data", 2);
// (underscores are removed as whitespace)
```

### Example 4: Extracting a Field from an Integer

```cpp
using namespace sc_cast;

uint32_t packet = 0xAABBCCDD;

// LSB byte
sc_lv<8> lsb_byte = sc_lv_cast<sc_lv<8>>(packet, "data", false);  // 0xDD

// MSB byte
sc_lv<8> msb_byte = sc_lv_cast<sc_lv<8>>(packet, "data", true);   // 0xAA

// Convert to integer
uint8_t lsb_val = sc_lv_cast<uint8_t>(lsb_byte);                   // 0xDD
```

### Example 5: sc_bv Behavior with X/Z Input

```cpp
using namespace sc_cast;

// Input with X — converted to 0
sc_bv<4> bv = sc_lv_cast<sc_bv<4>>("0b1X0Z");
// WARNING: "Target is sc_bv (no X/Z support); X/Z bits will be treated as 0."
// Result: "1000" → 0x8

// sc_lv preserves X/Z
sc_lv<4> lv = sc_lv_cast<sc_lv<4>>("0b1X0Z");
// Result: "1X0Z"
```

---

## License

This library is freely available for your use. Attribution is appreciated (but not required) when used in commercial or academic projects.

## Contributing

To report a bug or suggest a new feature:
1. Describe the issue with expected input/output.
2. Mention your C++ version and compiler.
3. Provide a minimal reproducer if possible.

---

**Library Version:** 1.0  
**Date:** 2026-07-07  
**Target:** SystemC IEEE 1666
