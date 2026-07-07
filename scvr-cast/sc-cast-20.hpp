#ifndef SC_CAST_HPP
#define SC_CAST_HPP

// ============================================================================
// sc_cast : A type-cast utility library for SystemC
// C++20 version — uses concepts, std::bit_cast, std::ranges, std::format
// Supports: sc_lv<W>, sc_bv<W>, sc_logic
// Target: C++20, SystemC (IEEE 1666)
// ============================================================================

#include <systemc>         // SystemC core and data types (sc_core, sc_dt, sc_lv, SC_REPORT_*)
#include <type_traits>     // type traits (still needed for some helpers)
#include <string>          // std::string, std::string_view
#include <string_view>     // std::string_view (explicit include for portability)
#include <sstream>         // std::stringstream
#include <limits>          // numeric limits (e.g. int max/min)
#include <bitset>          // std::bitset for binary conversions
#include <regex>           // regex for string format checks
#include <algorithm>       // std::ranges::any_of
#include <cctype>          // character checks: isspace, isdigit
#include <cmath>           // ceil
#include <cstring>         // memcpy (kept for compatibility; bit_cast replaces most uses)
#include <typeinfo>        // typeid
#include <stdexcept>       // std::invalid_argument, std::out_of_range
#include <concepts>        // std::integral, std::floating_point, std::same_as  [C++20]
#include <bit>             // std::bit_cast                                  [C++20]
#include <ranges>          // std::ranges algorithms                          [C++20]
#include <format>          // std::format                                     [C++20]
#include <optional>        // std::optional                                   [C++20]

// ----------------------------------------------------------------------------
// Compiler-specific demangling support
// <cxxabi.h> is GCC/Clang-only. On MSVC we fall back to typeid().name().
// ----------------------------------------------------------------------------
#if defined(__GNUC__) || defined(__clang__)
#  include <cxxabi.h>      // demangle type names (GCC/Clang only)
#  include <cstdlib>       // free()
#  define SC_CAST_HAS_CXXABI 1
#endif

template <typename T>
std::string type_name(const T& var)
{
#if defined(SC_CAST_HAS_CXXABI)
    int status;
    char* demangled = abi::__cxa_demangle(typeid(var).name(), 0, 0, &status);
    std::string result = (status == 0 && demangled) ? demangled : typeid(var).name();
    free(demangled);
    return result;
#else
    // MSVC / unknown compiler fallback: return raw mangled name
    return typeid(var).name();
#endif
}

namespace sc_cast
{
    using namespace sc_core;
    using namespace sc_dt;
    using namespace std;

    // ========================================================================
    // CONCEPTS — replace verbose enable_if patterns
    // ========================================================================

    template <typename T>
    struct is_sc_lv : std::false_type {};
    // Special case: if T is sc_lv<W> for any int W, then it's TRUE
    template <int W>
    struct is_sc_lv<sc_lv<W>> : std::true_type {};

    // -------------------- is_sc_bv --------------------
    // Default: everything is NOT an sc_bv
    template <typename T>
    struct is_sc_bv : std::false_type {};
    template <int W>
    struct is_sc_bv<sc_bv<W>> : std::true_type {};

    // -------------------- is_sc_logic --------------------
    // Default: everything is NOT an sc_logic
    template <typename T>
    struct is_sc_logic : std::false_type {};
    template <>
    struct is_sc_logic<sc_logic> : std::true_type {};

    // -------------------- Concepts (union of all three) --------------------
    template <typename T>
    concept ScLogicLike = is_sc_lv<T>::value || is_sc_bv<T>::value || is_sc_logic<T>::value;

    template <typename T>
    concept ScVector = is_sc_lv<T>::value || is_sc_bv<T>::value;   // excludes sc_logic

    template <typename T>
    concept ScLv     = is_sc_lv<T>::value;

    template <typename T>
    concept ScBv     = is_sc_bv<T>::value;

    template <typename T>
    concept ScLogic  = is_sc_logic<T>::value;

    // -------------------- width / type traits --------------------
    template <typename T>
    struct sc_lv_traits; // primary template left undefined

    // Traits to extract width from sc_lv<W>
    template <int W>
    struct sc_lv_traits<sc_lv<W>> {
        static constexpr int width = W;
        using type = sc_lv<W>;
    };

    // Traits to extract width from sc_bv<W>
    template <int W>
    struct sc_lv_traits<sc_bv<W>> {
        static constexpr int width = W;
        using type = sc_bv<W>;
    };

    // Traits for sc_logic (width = 1)
    template <>
    struct sc_lv_traits<sc_logic> {
        static constexpr int width = 1;
        using type = sc_logic;
    };

    // ========================================================================
    // DETAIL HELPERS
    // ========================================================================
    namespace detail
    {
        // Build an "unknown" value: X for sc_lv, 0 for sc_bv, X for sc_logic
        template <ScLogicLike LV>
        LV make_unknown()
        {
            constexpr int W = sc_lv_traits<LV>::width;
            if constexpr (ScBv<LV>)
                return LV(std::string(W, '0').c_str());
            else if constexpr (ScLv<LV>)
                return LV(std::string(W, 'X').c_str());
            else // sc_logic
                return LV(SC_LOGIC_X);
        }

        // Safe isspace (handles negative char values)
        bool safe_isspace(char c) {
            return std::isspace(static_cast<unsigned char>(c)) != 0;
        }
        bool safe_isdigit(char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        }

        // Parse unsigned integer string with exception safety
        // Returns std::optional<uint64_t> — clean success/failure signal [C++20]
        std::optional<uint64_t> parse_u64(const std::string& s, int base)
        {
            try {
                size_t idx = 0;
                uint64_t v = std::stoull(s, &idx, base);
                if (idx != s.size()) return std::nullopt;
                return v;
            } catch (...) {
                return std::nullopt;
            }
        }

        // Adjust a bit-string to WIDTH: truncate (keep LSBs) or pad.
        template <ScLogicLike LV>
        std::string adjust_bitstr(std::string bitstr,
                                  int WIDTH,
                                  bool is_data,
                                  std::string_view type_label)
        {
            int sz = static_cast<int>(bitstr.size());
            if (sz > WIDTH)
            {
                if constexpr (!ScLogic<LV>)
                {
                    // C++20: std::format for type-safe message construction
                    std::string msg = std::format("{} string longer than WIDTH — keeping LSBs.", type_label);
                    SC_REPORT_WARNING("sc_lv_cast", msg.c_str());
                    bitstr = bitstr.substr(sz - WIDTH);
                }
                // sc_logic: no truncation; .back() taken later
            }
            else if (sz < WIDTH)
            {
                char pad = bitstr.empty() ? '0' : bitstr[0];
                bitstr = is_data ? (std::string(WIDTH - sz, pad) + bitstr)
                                 : (std::string(WIDTH - sz, '0') + bitstr);
            }
            return bitstr;
        }

        // Convert a finalized bit-string to the target LV type.
        template <ScLogicLike LV>
        LV from_bitstr(const std::string& bitstr)
        {
            if constexpr (ScLogic<LV>)
            {
                char c = bitstr.empty() ? '0' : bitstr.back();
                if (c == '1')              return LV(SC_LOGIC_1);
                if (c == '0')              return LV(SC_LOGIC_0);
                if (c == 'Z' || c == 'z')  return LV(SC_LOGIC_Z);
                return LV(SC_LOGIC_X);
            }
            else
            {
                return LV(bitstr.c_str());
            }
        }
    } // namespace detail

    // ========================================================================
    // FORWARD DECLARATIONS
    // ========================================================================

    // --------------------LV TO LV --------------------
    // Identity: LV → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(const LV& lv);

    // -------------------- TO LV --------------------

    // bool → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(bool value);

    // integral (not bool) → LV
    template <ScLogicLike LV, std::integral T>
        requires (!std::same_as<T, bool>)
    LV sc_lv_cast(T value, std::string_view mode_view = "data", bool MSB = false);

    // float → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(float value);

    // double → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(double value);

    // string → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(std::string_view input_str,
                  std::string_view mode_view = "data",
                  int base = 2);

    // sc_logic → sc_lv/sc_bv (cross conversion)
    template <ScVector LV>
    LV sc_lv_cast(const sc_logic& lg);

    // -------------------- FROM LV --------------------

    // LV → bool
    template <typename T = bool, ScLogicLike LV>
        requires std::same_as<T, bool>
    T sc_lv_cast(const LV& lv);

    // LV → integral (not bool)
    template <std::integral T, ScLogicLike LV>
        requires (!std::same_as<T, bool>)
    T sc_lv_cast(const LV& lv);

    // LV → float
    template <typename T = float, ScLogicLike LV>
        requires std::same_as<T, float>
    T sc_lv_cast(const LV& lv);

    // LV → double
    template <typename T = double, ScLogicLike LV>
        requires std::same_as<T, double>
    T sc_lv_cast(const LV& lv);

    // LV → std::string (binary representation, MSB-first)
    template <typename T = std::string, ScLogicLike LV>
        requires std::same_as<T, std::string>
    T sc_lv_cast(const LV& lv);

    // LV (sc_lv/sc_bv) → sc_logic (take LSB)
    template <typename T = sc_logic, ScVector LV>
        requires std::same_as<T, sc_logic>
    T sc_lv_cast(const LV& lv);

    // -------------------- Generic String to Type --------------------

    // string → T (bool/int/float/double/sc_lv/sc_bv/sc_logic)
    template <typename T>
    T string_cast(std::string_view str_view,
                  std::string_view mode_view = "data",
                  int base = 2);

    // ========================================================================
    // IMPLEMENTATIONS
    // ========================================================================

    // ----------------------------
    // TO sc_lv<WIDTH> / sc_bv<WIDTH> / sc_logic CONVERSIONS
    // ----------------------------

    // Identity: LV → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(const LV& lv) { return lv; }

    // bool type
    // bool → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(bool value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;

        if (WIDTH <= 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return detail::make_unknown<LV>();
        }

        if constexpr (ScLogic<LV>)
        {
            return LV(value ? SC_LOGIC_1 : SC_LOGIC_0);
        }
        else
        {
            LV lv(0);
            lv[0] = value ? SC_LOGIC_1 : SC_LOGIC_0;
            return lv;
        }
    }

    // integral (not bool) → LV
    template <ScLogicLike LV, std::integral T>
        requires (!std::same_as<T, bool>)
    LV sc_lv_cast(T value, std::string_view mode_view, bool MSB)
    {
        // Direct string_view comparison — no std::string copy needed
        const bool is_address = (mode_view == "address");
        const bool is_data    = (mode_view == "data");
        if (!is_address && !is_data)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<LV>();
        }

        constexpr int WIDTH = sc_lv_traits<LV>::width;

        if (WIDTH <= 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return detail::make_unknown<LV>();
        }

        if constexpr (ScLogic<LV>)
        {
            // Single-bit target: keep only LSB
            if (is_address && value < 0)
                SC_REPORT_FATAL("sc_lv_cast", "Address mode can't be negative");
            return LV((value & 1) ? SC_LOGIC_1 : SC_LOGIC_0);
        }
        else
        {
            int64_t  Value64  = static_cast<int64_t>(value);
            uint64_t uValue64 = static_cast<uint64_t>(value);

            bool     is_signed        = std::is_signed_v<T>;
            int      inputTypeBitSize = sizeof(T) * 8;
            uint64_t umaxRange        = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
            int64_t  maxRange         = (WIDTH >= 64) ? INT64_MAX  : (1LL << (WIDTH - 1)) - 1;
            int64_t  minRange         = (WIDTH >= 64) ? INT64_MIN  : -(1LL << (WIDTH - 1));

            if (!MSB)
            {
                // LSB mode (default): keep LSBs
                if (is_signed)
                {
                    if (is_address && value < 0)
                        SC_REPORT_FATAL("sc_lv_cast", "Address mode can't be negative");

                    if (WIDTH < inputTypeBitSize && (value > maxRange || value < minRange))
                        SC_REPORT_WARNING("sc_lv_cast", "Input type is larger than WIDTH, may cause overflow");

                    return LV(Value64);
                }
                else
                {
                    if (WIDTH < inputTypeBitSize && uValue64 > umaxRange)
                        SC_REPORT_WARNING("sc_lv_cast", "Input type is larger than WIDTH, may cause unsigned overflow");
                    return LV(uValue64);
                }
            }
            else
            {
                // MSB mode: keep MSBs of the input value
                if (WIDTH >= inputTypeBitSize)
                    return is_signed ? LV(Value64) : LV(uValue64);

                int shift = inputTypeBitSize - WIDTH;
                SC_REPORT_INFO("sc_lv_cast", "MSB mode: keeping upper bits of input value");
                if (is_signed)
                    return LV(static_cast<int64_t>(Value64 >> shift));
                else
                    return LV(uValue64 >> shift);
            }
        }
    }

    // Overload for float
    // float → LV
    // C++20: std::bit_cast replaces the union-based type punning
    template <ScLogicLike LV>
    LV sc_lv_cast(float value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (WIDTH < 32)
            SC_REPORT_FATAL("sc_lv_cast", "Width must be at least 32 bits for float to sc_lv conversion");

        if constexpr (ScLogic<LV>)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Cannot fit 32-bit float into sc_logic");
            return detail::make_unknown<LV>();
        }
        else
        {
            // Clean, well-defined type punning via std::bit_cast
            uint32_t raw = std::bit_cast<uint32_t>(value);
            return LV(raw);
        }
    }

    // Overload for double
    // double → LV
    // C++20: std::bit_cast replaces the union-based type punning
    template <ScLogicLike LV>
    LV sc_lv_cast(double value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (WIDTH < 64)
            SC_REPORT_FATAL("sc_lv_cast", "Width must be at least 64 bits for double to sc_lv conversion");

        if constexpr (ScLogic<LV>)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Cannot fit 64-bit double into sc_logic");
            return detail::make_unknown<LV>();
        }
        else
        {
            uint64_t raw = std::bit_cast<uint64_t>(value);
            return LV(raw);
        }
    }

    // Overload for string
    // string → LV
    template <ScLogicLike LV>
    LV sc_lv_cast(std::string_view input_str, std::string_view mode_view, int base)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;

        const bool is_address = (mode_view == "address");
        const bool is_data    = (mode_view == "data");
        if (!is_address && !is_data)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<LV>();
        }

        // We need a mutable copy for whitespace removal / padding.
        // string_view alone is insufficient here; this copy is intentional.
        std::string str(input_str);

        if (WIDTH <= 0)
            SC_REPORT_FATAL("sc_lv_cast", "Width must be positive");

        uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        int64_t  maxRange  = (WIDTH >= 64) ? INT64_MAX  : (1LL << (WIDTH - 1)) - 1;

        // C++20: std::ranges::any_of + std::erase_if (no manual erase-remove)
        if (std::ranges::any_of(str, detail::safe_isspace))
        {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            std::erase_if(str, detail::safe_isspace);
        }

        if (str.empty())
        {
            SC_REPORT_ERROR("sc_lv_cast", "Empty string input. X signal issue.");
            return detail::make_unknown<LV>();
        }

        size_t size = str.size();

        // Warn if target is sc_bv but string contains X/Z
        if constexpr (ScBv<LV>)
        {
            if (str.find_first_of("xXzZ") != std::string::npos)
                SC_REPORT_WARNING("sc_lv_cast",
                    "Target is sc_bv (no X/Z support); X/Z bits will be treated as 0.");
        }

        char c0 = str[0];
        char c1 = (size > 1) ? str[1] : 0;
        std::string substr2 = (size > 2) ? std::string(str.substr(2)) : "";
        std::string substr1 = (size > 1) ? std::string(str.substr(1)) : "";

        // Binary prefix
        if (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B'))
        {
            if (!std::regex_match(substr2, std::regex("^[01XZxz]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in binary string. X signal issue.");
                return detail::make_unknown<LV>();
            }
            substr2 = detail::adjust_bitstr<LV>(std::move(substr2), WIDTH, is_data, "Binary");
            return detail::from_bitstr<LV>(substr2);
        }

        // Another version of binary (no prefix, only when base==2)
        if ((str.find_first_not_of("01xXzZ") == std::string::npos) && (base == 2))
        {
            str = detail::adjust_bitstr<LV>(std::move(str), WIDTH, is_data, "Binary");
            return detail::from_bitstr<LV>(str);
        }

        // Hex prefix
        if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X'))
        {
            std::string hex_str = substr2; // Use the rest of the string after '0'

            if (!std::regex_match(hex_str, std::regex("^[0-9a-fA-FxzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in hex string. X signal issue.");
                return detail::make_unknown<LV>();
            }

            int bitsNum = static_cast<int>(hex_str.size()) * 4;
            if (bitsNum > WIDTH)
            {
                if constexpr (!ScLogic<LV>)
                {
                    SC_REPORT_WARNING("sc_lv_cast", "Hex string longer than WIDTH — keeping LSBs.");
                    int drop_digits = (bitsNum - WIDTH + 3) / 4;
                    hex_str = hex_str.substr(drop_digits);
                }
            }
            else if (bitsNum < WIDTH)
            {
                int padDigits = static_cast<int>(std::ceil((WIDTH - bitsNum) / 4.0));
                char pad = hex_str.empty() ? '0' : hex_str[0];
                hex_str = is_data ? (std::string(padDigits, pad) + hex_str)
                                  : (std::string(padDigits, '0') + hex_str);
            }

            std::string bitstr;
            for (char ch : hex_str)
            {
                if (ch == 'x' || ch == 'X')
                    bitstr += "XXXX";
                else if (ch == 'z' || ch == 'Z')
                    bitstr += "ZZZZ";
                else
                {
                    int val = detail::safe_isdigit(ch) ? ch - '0'
                                                       : std::toupper(ch) - 'A' + 10;
                    bitstr += std::bitset<4>(val).to_string();
                }
            }
            return detail::from_bitstr<LV>(bitstr);
        }

        // Octal prefix
        if (size >= 2 && c0 == '0' && detail::safe_isdigit(c1))
        {
            std::string oct_str = substr1; // Use the rest of the string after '0'

            if (!std::regex_match(oct_str, std::regex("^[0-7xzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in octal string. X signal issue.");
                return detail::make_unknown<LV>();
            }

            int bitsNum = static_cast<int>(oct_str.size()) * 3;
            if (bitsNum > WIDTH)
            {
                if constexpr (!ScLogic<LV>)
                {
                    SC_REPORT_WARNING("sc_lv_cast", "Octal string longer than WIDTH — keeping LSBs.");
                    int drop_digits = (bitsNum - WIDTH + 2) / 3;
                    oct_str = oct_str.substr(drop_digits);
                }
            }
            else if (bitsNum < WIDTH)
            {
                int padDigits = static_cast<int>(std::ceil((WIDTH - bitsNum) / 3.0));
                char pad = oct_str.empty() ? '0' : oct_str[0];
                oct_str = is_data ? (std::string(padDigits, pad) + oct_str)
                                  : (std::string(padDigits, '0') + oct_str);
            }

            std::string bitstr;
            for (char ch : oct_str)
            {
                if (ch == 'x' || ch == 'X')
                    bitstr += "XXX";
                else if (ch == 'z' || ch == 'Z')
                    bitstr += "ZZZ";
                else
                    bitstr += std::bitset<3>(ch - '0').to_string();
            }
            return detail::from_bitstr<LV>(bitstr);
        }

        // Decimal (signed or unsigned)
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$")))
        {
            if (is_address)
            {
                if (str[0] == '+' || str[0] == '-')
                {
                    SC_REPORT_ERROR("sc_lv_cast", "Address mode does not support signed decimal strings.");
                    return detail::make_unknown<LV>();
                }
                auto parsed = detail::parse_u64(str, 10);
                if (!parsed)
                {
                    SC_REPORT_ERROR("sc_lv_cast", "Failed to parse decimal string.");
                    return detail::make_unknown<LV>();
                }
                if (*parsed > umaxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value overflows max for WIDTH");
                return sc_lv_cast<LV>(*parsed);
            }
            else // data
            {
                char sign = str[0];
                std::string dec_str = (sign == '+' || sign == '-') ? substr1 : str;
                auto parsed = detail::parse_u64(dec_str, 10);
                if (!parsed)
                {
                    SC_REPORT_ERROR("sc_lv_cast", "Failed to parse decimal string.");
                    return detail::make_unknown<LV>();
                }
                if (static_cast<int64_t>(*parsed) > maxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value overflows max for WIDTH");
                int64_t Value = (sign == '-') ? -static_cast<int64_t>(*parsed)
                                              : static_cast<int64_t>(*parsed);
                return sc_lv_cast<LV>(Value);
            }
        }

        SC_REPORT_ERROR("sc_lv_cast", "Invalid string format for sc_lv conversion.");
        return detail::make_unknown<LV>();
    }

    // sc_logic → sc_lv/sc_bv (cross conversion)
    template <ScVector LV>
    LV sc_lv_cast(const sc_logic& lg)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if constexpr (ScBv<LV>)
        {
            // sc_bv: X/Z → 0
            if (lg.is_01())
                return LV(lg.to_bool() ? std::string(WIDTH, '1').c_str()
                                       : std::string(WIDTH, '0').c_str());
            SC_REPORT_WARNING("sc_lv_cast", "sc_logic has X/Z; converting to 0 in sc_bv.");
            return LV(std::string(WIDTH, '0').c_str());
        }
        else
        {
            // sc_lv: preserve X/Z
            char c = lg.to_char();
            return LV(std::string(WIDTH, c).c_str());
        }
    }

    // -------------------------------
    // FROM sc_lv<WIDTH> / sc_bv<WIDTH> / sc_logic CONVERSIONS
    // -------------------------------

    // LV → bool
    template <typename T, ScLogicLike LV>
        requires std::same_as<T, bool>
    T sc_lv_cast(const LV& lv)
    {
        if constexpr (ScLogic<LV>)
        {
            return lv.to_bool();
        }
        else
        {
            if (!lv.is_01())
            {
                SC_REPORT_WARNING("sc_lv_cast", "Input contains unknown (X/Z) bits. Returning false.");
                return false;
            }
            return lv.get_bit(0); // LSB
        }
    }

    // FROM sc_lv<WIDTH> to integral types
    // LV → integral (not bool)
    template <std::integral T, ScLogicLike LV>
        requires (!std::same_as<T, bool>)
    T sc_lv_cast(const LV& lv)
    {
        if constexpr (ScLogic<LV>)
        {
            if (!lv.is_01())
            {
                SC_REPORT_WARNING("sc_lv_cast", "sc_logic is X/Z. Returning 0.");
                return 0;
            }
            return static_cast<T>(lv.to_bool() ? 1 : 0);
        }
        else
        {
            if (!lv.is_01())
            {
                SC_REPORT_WARNING("sc_lv_cast", "Input contains unknown (X/Z) bits. Returning 0.");
                return 0;
            }

            if constexpr (std::is_signed_v<T>)
            {
                int64_t val = lv.to_int64();
                if (val < std::numeric_limits<T>::min() || val > std::numeric_limits<T>::max())
                    SC_REPORT_WARNING("sc_lv_cast", "Signed overflow in conversion from sc_lv to target type");
                return static_cast<T>(val);
            }
            else
            {
                uint64_t val = lv.to_uint64();
                if (val > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                    SC_REPORT_WARNING("sc_lv_cast", "Unsigned overflow in conversion from sc_lv to target type");
                return static_cast<T>(val);
            }
        }
    }

    // FROM sc_lv<32> to float
    // LV → float
    // C++20: std::bit_cast replaces the memcpy-based type punning
    template <typename T, ScLogicLike LV>
        requires std::same_as<T, float>
    T sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if constexpr (ScLogic<LV>)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Cannot convert sc_logic to float");
            return 0.0f;
        }
        else
        {
            if (WIDTH < 32)
                SC_REPORT_FATAL("sc_lv_cast", "Float needs at least 32 bits for sc_lv->float");
            uint32_t raw = static_cast<uint32_t>(lv.to_uint64() & 0xFFFFFFFFu);
            return std::bit_cast<float>(raw);
        }
    }

    // FROM sc_lv<64> to double
    // LV → double
    // C++20: std::bit_cast replaces the memcpy-based type punning
    template <typename T, ScLogicLike LV>
        requires std::same_as<T, double>
    T sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if constexpr (ScLogic<LV>)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Cannot convert sc_logic to double");
            return 0.0;
        }
        else
        {
            if (WIDTH < 64)
                SC_REPORT_FATAL("sc_lv_cast", "Double needs at least 64 bits for sc_lv->double");
            uint64_t raw = lv.to_uint64();
            return std::bit_cast<double>(raw);
        }
    }

    // LV → std::string (binary representation, MSB-first)
    template <typename T, ScLogicLike LV>
        requires std::same_as<T, std::string>
    T sc_lv_cast(const LV& lv)
    {
        return lv.to_string();
    }

    // LV (sc_lv/sc_bv) → sc_logic (take LSB)
    template <typename T, ScVector LV>
        requires std::same_as<T, sc_logic>
    T sc_lv_cast(const LV& lv)
    {
        return lv.get_bit(0) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // -------------------------------
    // WORK WITH FILE STRINGS
    // -------------------------------

    template <typename T>
    T string_cast(std::string_view str_view, std::string_view mode_view, int base)
    {
        std::string str(str_view);
        std::string mode(mode_view);

        if constexpr (ScLogicLike<T>)
        {
            using LV = T;
            return sc_lv_cast<LV>(str, mode, base);
        }
        else if constexpr (std::integral<T>)
        {
            if (str.empty())
            {
                SC_REPORT_WARNING("string_cast", "Empty string. Returning 0.");
                return static_cast<T>(0);
            }

            if (str.find_first_not_of("xX") == std::string::npos)
                return static_cast<T>(0);

            int  size = static_cast<int>(str.size());
            char c0   = str[0];
            char c1   = (size > 1) ? str[1] : 0;

            // C++20: lambda + std::optional for clean parse-and-report
            auto try_parse = [](const std::string& s, int b) -> std::optional<uint64_t> {
                return detail::parse_u64(s, b);
            };

            std::optional<uint64_t> result;

            // Binary: "0b..." or pure 0/1 string with base==2
            if (((str.find_first_not_of("01") == std::string::npos) && (base == 2))
                || (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B')
                    && (str.substr(2).find_first_not_of("01") == std::string::npos)))
            {
                result = try_parse(str, 2);
            }
            // Hex: "0x..."
            else if (size > 2 && c0 == '0' && (c1 == 'X' || c1 == 'x'))
            {
                result = try_parse(str, 16);
            }
            // Octal: "0..."
            else if (size > 1 && c0 == '0')
            {
                result = try_parse(str, 8);
            }
            // Decimal fallback
            else
            {
                result = try_parse(str, 10);
            }

            if (!result)
            {
                SC_REPORT_WARNING("string_cast", "Integer parse error. Returning 0.");
                return static_cast<T>(0);
            }
            return static_cast<T>(*result);
        }
        else if constexpr (std::floating_point<T>)
        {
            try {
                if constexpr (std::same_as<T, double>)
                    return std::stod(str);
                else
                    return std::stof(str);
            } catch (...) {
                SC_REPORT_WARNING("string_cast", "Floating-point parse error. Returning 0.");
                return T{0};
            }
        }
        else
        {
            std::stringstream ss(str);
            T val;
            ss >> val;
            return val;
        }
    }

} // namespace sc_cast

#endif // SC_CAST_HPP