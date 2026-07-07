#ifndef SC_CAST_LV_HPP
#define SC_CAST_LV_HPP

// ============================================================================
// sc_cast :: sc_lv.hpp  (C++20)
// Conversions to/from sc_lv<W>
// ============================================================================

#include "utils.hpp"

namespace sc_cast
{
    // ========================================================================
    // TO sc_lv<W>
    // ========================================================================

    // Identity
    template <ScLv LV>
    LV sc_lv_cast(const LV& lv) { return lv; }

    // bool → sc_lv
    template <ScLv LV>
    LV sc_lv_cast(bool value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if (WIDTH <= 0) {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return detail::make_unknown<LV>();
        }
        LV lv(0);
        lv[0] = value ? SC_LOGIC_1 : SC_LOGIC_0;
        return lv;
    }

    // integral (not bool) → sc_lv
    template <ScLv LV, std::integral T>
        requires (!std::same_as<T, bool>)
    LV sc_lv_cast(T value, std::string_view mode_view = "data", bool MSB = false)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<LV>();
        }

        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if (WIDTH <= 0) {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return detail::make_unknown<LV>();
        }

        int64_t  Value64  = static_cast<int64_t>(value);
        uint64_t uValue64 = static_cast<uint64_t>(value);
        bool     is_signed        = std::is_signed_v<T>;
        int      inputTypeBitSize = sizeof(T) * 8;
        uint64_t umaxRange        = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        int64_t  maxRange         = (WIDTH >= 64) ? INT64_MAX  : (1LL << (WIDTH - 1)) - 1;
        int64_t  minRange         = (WIDTH >= 64) ? INT64_MIN  : -(1LL << (WIDTH - 1));

        if (!MSB) {
            if (is_signed) {
                if (is_address && value < 0)
                    SC_REPORT_FATAL("sc_lv_cast", "Address mode can't be negative");
                if (WIDTH < inputTypeBitSize && (value > maxRange || value < minRange))
                    SC_REPORT_WARNING("sc_lv_cast", "Input type is larger than WIDTH, may cause overflow");
                return LV(Value64);
            } else {
                if (WIDTH < inputTypeBitSize && uValue64 > umaxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Input type is larger than WIDTH, may cause unsigned overflow");
                return LV(uValue64);
            }
        } else {
            if (WIDTH >= inputTypeBitSize)
                return is_signed ? LV(Value64) : LV(uValue64);
            int shift = inputTypeBitSize - WIDTH;
            SC_REPORT_INFO("sc_lv_cast", "MSB mode: keeping upper bits of input value");
            return is_signed ? LV(static_cast<int64_t>(Value64 >> shift))
                             : LV(uValue64 >> shift);
        }
    }

    // float → sc_lv (C++20: std::bit_cast)
    template <ScLv LV>
    LV sc_lv_cast(float value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if (WIDTH < 32)
            SC_REPORT_FATAL("sc_lv_cast", "Width must be at least 32 bits for float to sc_lv conversion");
        uint32_t raw = std::bit_cast<uint32_t>(value);
        return LV(raw);
    }

    // double → sc_lv (C++20: std::bit_cast)
    template <ScLv LV>
    LV sc_lv_cast(double value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if (WIDTH < 64)
            SC_REPORT_FATAL("sc_lv_cast", "Width must be at least 64 bits for double to sc_lv conversion");
        uint64_t raw = std::bit_cast<uint64_t>(value);
        return LV(raw);
    }

    // string → sc_lv
    template <ScLv LV>
    LV sc_lv_cast(std::string_view input_str,
                  std::string_view mode_view = "data",
                  int base = 2)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<LV>();
        }

        std::string str(input_str);
        if (WIDTH <= 0)
            SC_REPORT_FATAL("sc_lv_cast", "Width must be positive");

        uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        int64_t  maxRange  = (WIDTH >= 64) ? INT64_MAX  : (1LL << (WIDTH - 1)) - 1;

        // C++20: std::ranges::any_of + std::erase_if
        if (std::ranges::any_of(str, detail::safe_isspace)) {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            std::erase_if(str, detail::safe_isspace);
        }

        if (str.empty()) {
            SC_REPORT_ERROR("sc_lv_cast", "Empty string input. X signal issue.");
            return detail::make_unknown<LV>();
        }

        size_t size = str.size();
        char c0 = str[0];
        char c1 = (size > 1) ? str[1] : 0;
        std::string substr2 = (size > 2) ? std::string(str.substr(2)) : "";
        std::string substr1 = (size > 1) ? std::string(str.substr(1)) : "";

        // Binary prefix "0b..."
        if (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B')) {
            if (!std::regex_match(substr2, std::regex("^[01XZxz]+$"))) {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in binary string. X signal issue.");
                return detail::make_unknown<LV>();
            }
            substr2 = detail::adjust_bitstr<LV>(std::move(substr2), WIDTH, is_data, "Binary");
            return detail::from_bitstr<LV>(substr2);
        }

        // Pure binary (no prefix, base==2)
        if ((str.find_first_not_of("01xXzZ") == std::string::npos) && (base == 2)) {
            str = detail::adjust_bitstr<LV>(std::move(str), WIDTH, is_data, "Binary");
            return detail::from_bitstr<LV>(str);
        }

        // Hex prefix "0x..."
        if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X')) {
            std::string hex_str = substr2;
            if (!std::regex_match(hex_str, std::regex("^[0-9a-fA-FxzXZ]+$"))) {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in hex string. X signal issue.");
                return detail::make_unknown<LV>();
            }
            int bitsNum = static_cast<int>(hex_str.size()) * 4;
            if (bitsNum > WIDTH) {
                SC_REPORT_WARNING("sc_lv_cast", "Hex string longer than WIDTH — keeping LSBs.");
                int drop_digits = (bitsNum - WIDTH + 3) / 4;
                hex_str = hex_str.substr(drop_digits);
            } else if (bitsNum < WIDTH) {
                int padDigits = static_cast<int>(std::ceil((WIDTH - bitsNum) / 4.0));
                char pad = hex_str.empty() ? '0' : hex_str[0];
                hex_str = is_data ? (std::string(padDigits, pad) + hex_str)
                                  : (std::string(padDigits, '0') + hex_str);
            }
            std::string bitstr;
            for (char ch : hex_str) {
                if (ch == 'x' || ch == 'X') bitstr += "XXXX";
                else if (ch == 'z' || ch == 'Z') bitstr += "ZZZZ";
                else {
                    int val = detail::safe_isdigit(ch) ? ch - '0'
                                                       : std::toupper(ch) - 'A' + 10;
                    bitstr += std::bitset<4>(val).to_string();
                }
            }
            return detail::from_bitstr<LV>(bitstr);
        }

        // Octal prefix "0..."
        if (size >= 2 && c0 == '0' && detail::safe_isdigit(c1)) {
            std::string oct_str = substr1;
            if (!std::regex_match(oct_str, std::regex("^[0-7xzXZ]+$"))) {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in octal string. X signal issue.");
                return detail::make_unknown<LV>();
            }
            int bitsNum = static_cast<int>(oct_str.size()) * 3;
            if (bitsNum > WIDTH) {
                SC_REPORT_WARNING("sc_lv_cast", "Octal string longer than WIDTH — keeping LSBs.");
                int drop_digits = (bitsNum - WIDTH + 2) / 3;
                oct_str = oct_str.substr(drop_digits);
            } else if (bitsNum < WIDTH) {
                int padDigits = static_cast<int>(std::ceil((WIDTH - bitsNum) / 3.0));
                char pad = oct_str.empty() ? '0' : oct_str[0];
                oct_str = is_data ? (std::string(padDigits, pad) + oct_str)
                                  : (std::string(padDigits, '0') + oct_str);
            }
            std::string bitstr;
            for (char ch : oct_str) {
                if (ch == 'x' || ch == 'X') bitstr += "XXX";
                else if (ch == 'z' || ch == 'Z') bitstr += "ZZZ";
                else bitstr += std::bitset<3>(ch - '0').to_string();
            }
            return detail::from_bitstr<LV>(bitstr);
        }

        // Decimal
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$"))) {
            if (is_address) {
                if (str[0] == '+' || str[0] == '-') {
                    SC_REPORT_ERROR("sc_lv_cast", "Address mode does not support signed decimal strings.");
                    return detail::make_unknown<LV>();
                }
                auto parsed = detail::parse_u64(str, 10);
                if (!parsed) {
                    SC_REPORT_ERROR("sc_lv_cast", "Failed to parse decimal string.");
                    return detail::make_unknown<LV>();
                }
                if (*parsed > umaxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value overflows max for WIDTH");
                return sc_lv_cast<LV>(*parsed);
            } else {
                char sign = str[0];
                std::string dec_str = (sign == '+' || sign == '-') ? substr1 : str;
                auto parsed = detail::parse_u64(dec_str, 10);
                if (!parsed) {
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

    // sc_logic → sc_lv
    template <ScLv LV>
    LV sc_lv_cast(const sc_logic& lg)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        char c = lg.to_char();
        return LV(std::string(WIDTH, c).c_str());
    }

    // sc_bv → sc_lv
    template <ScLv LV, int W2>
    LV sc_lv_cast(const sc_bv<W2>& bv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        std::string bitstr = bv.to_string();
        if (static_cast<int>(bitstr.size()) < WIDTH)
            bitstr = std::string(WIDTH - bitstr.size(), '0') + bitstr;
        else if (static_cast<int>(bitstr.size()) > WIDTH)
            bitstr = bitstr.substr(bitstr.size() - WIDTH);
        return LV(bitstr.c_str());
    }

    // sc_int/sc_uint/sc_bigint/sc_biguint → sc_lv
    template <ScLv LV, ScIntLike INT>
    LV sc_lv_cast(const INT& val)
    {
        constexpr int INT_W = sc_lv_traits<INT>::width;
        if constexpr (INT_W <= 64) {
            return LV(val.to_uint64());
        } else {
            std::string binstr = val.to_string(SC_BIN);
            return sc_lv_cast<LV>(binstr, "data", 2);
        }
    }

    // ========================================================================
    // FROM sc_lv<W>
    // ========================================================================

    // sc_lv → bool
    template <typename T = bool, ScLv LV>
        requires std::same_as<T, bool>
    T sc_lv_cast(const LV& lv)
    {
        if (!lv.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "Input contains unknown (X/Z) bits. Returning false.");
            return false;
        }
        return lv.get_bit(0);
    }

    // sc_lv → integral (not bool)
    template <std::integral T, ScLv LV>
        requires (!std::same_as<T, bool>)
    T sc_lv_cast(const LV& lv)
    {
        if (!lv.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "Input contains unknown (X/Z) bits. Returning 0.");
            return 0;
        }
        if constexpr (std::is_signed_v<T>) {
            int64_t val = lv.to_int64();
            if (val < std::numeric_limits<T>::min() || val > std::numeric_limits<T>::max())
                SC_REPORT_WARNING("sc_lv_cast", "Signed overflow in conversion from sc_lv to target type");
            return static_cast<T>(val);
        } else {
            uint64_t val = lv.to_uint64();
            if (val > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                SC_REPORT_WARNING("sc_lv_cast", "Unsigned overflow in conversion from sc_lv to target type");
            return static_cast<T>(val);
        }
    }

    // sc_lv → float (C++20: std::bit_cast)
    template <typename T = float, ScLv LV>
        requires std::same_as<T, float>
    T sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if (WIDTH < 32)
            SC_REPORT_FATAL("sc_lv_cast", "Float needs at least 32 bits for sc_lv->float");
        uint32_t raw = static_cast<uint32_t>(lv.to_uint64() & 0xFFFFFFFFu);
        return std::bit_cast<float>(raw);
    }

    // sc_lv → double (C++20: std::bit_cast)
    template <typename T = double, ScLv LV>
        requires std::same_as<T, double>
    T sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        if (WIDTH < 64)
            SC_REPORT_FATAL("sc_lv_cast", "Double needs at least 64 bits for sc_lv->double");
        uint64_t raw = lv.to_uint64();
        return std::bit_cast<double>(raw);
    }

    // sc_lv → string
    template <typename T = std::string, ScLv LV>
        requires std::same_as<T, std::string>
    T sc_lv_cast(const LV& lv) { return lv.to_string(); }

    // sc_lv → sc_bv
    template <ScBv T, ScLv LV>
    T sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<T>::width;
        if (!lv.is_01())
            SC_REPORT_WARNING("sc_lv_cast", "sc_lv contains X/Z; converting to 0 in sc_bv.");
        std::string bitstr = lv.to_string();
        for (char& c : bitstr)
            if (c != '0' && c != '1') c = '0';
        if (static_cast<int>(bitstr.size()) < WIDTH)
            bitstr = std::string(WIDTH - bitstr.size(), '0') + bitstr;
        else if (static_cast<int>(bitstr.size()) > WIDTH)
            bitstr = bitstr.substr(bitstr.size() - WIDTH);
        return T(bitstr.c_str());
    }

    // ========================================================================
    // string_cast specialization for sc_lv
    // ========================================================================
    template <ScLv T>
    T string_cast(std::string_view str,
                  std::string_view mode = "data",
                  int base = 2)
    {
        return sc_lv_cast<T>(str, mode, base);
    }

} // namespace sc_cast

#endif // SC_CAST_LV_HPP