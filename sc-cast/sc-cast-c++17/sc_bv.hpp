#ifndef CC_VRWRAPPER_BV_HPP
#define CC_VRWRAPPER_BV_HPP

// ============================================================================
// cc_vrwrapper :: sc_bv.hpp  (C++17)
// Conversions TO sc_bv<W>  (destination = sc_bv)
// ============================================================================

#include "utils.hpp"

namespace cc_vrwrapper
{
    // ===== TO sc_bv<W> =====

    template <typename BV>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(const BV& bv) { return bv; }

    template <typename BV>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(bool value)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (WIDTH <= 0) {
            SC_REPORT_ERROR("sc_cast", "Width must be positive");
            return detail::make_unknown<BV>();
        }
        BV bv(0);
        bv[0] = value ? SC_LOGIC_1 : SC_LOGIC_0;
        return bv;
    }

    template <typename BV, typename T>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value
                     && is_sc_bv<BV>::value, BV>
    sc_cast(T value, std::string_view mode_view = "data", bool MSB = false)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<BV>();
        }

        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (WIDTH <= 0) {
            SC_REPORT_ERROR("sc_cast", "Width must be positive");
            return detail::make_unknown<BV>();
        }

        int64_t  Value64  = static_cast<int64_t>(value);
        uint64_t uValue64 = static_cast<uint64_t>(value);
        bool     is_signed        = std::numeric_limits<T>::is_signed;
        int      inputTypeBitSize = sizeof(T) * 8;
        uint64_t umaxRange        = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;

        if (!MSB) {
            if (is_signed) {
                if (is_address && value < 0)
                    SC_REPORT_FATAL("sc_cast", "Address mode can't be negative");
                if (WIDTH < inputTypeBitSize && (value > static_cast<int64_t>((1LL << WIDTH) - 1)
                         || value < -(1LL << (WIDTH - 1))))
                    SC_REPORT_WARNING("sc_cast", "Input type is larger than WIDTH, may cause overflow");
                return BV(Value64);
            } else {
                if (WIDTH < inputTypeBitSize && uValue64 > umaxRange)
                    SC_REPORT_WARNING("sc_cast", "Input type is larger than WIDTH, may cause unsigned overflow");
                return BV(uValue64);
            }
        } else {
            if (WIDTH >= inputTypeBitSize)
                return is_signed ? BV(Value64) : BV(uValue64);
            int shift = inputTypeBitSize - WIDTH;
            SC_REPORT_INFO("sc_cast", "MSB mode: keeping upper bits of input value");
            return is_signed ? BV(static_cast<int64_t>(Value64 >> shift))
                             : BV(uValue64 >> shift);
        }
    }

    template <typename BV>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(float value)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (WIDTH < 32)
            SC_REPORT_FATAL("sc_cast", "Width must be at least 32 bits for float to sc_bv conversion");
        uint32_t raw;
        std::memcpy(&raw, &value, sizeof(raw));
        return BV(raw);
    }

    template <typename BV>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(double value)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (WIDTH < 64)
            SC_REPORT_FATAL("sc_cast", "Width must be at least 64 bits for double to sc_bv conversion");
        uint64_t raw;
        std::memcpy(&raw, &value, sizeof(raw));
        return BV(raw);
    }

    template <typename BV>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(std::string_view input_str,
            std::string_view mode_view = "data",
            int base = 2)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<BV>();
        }

        std::string str(input_str);
        if (WIDTH <= 0)
            SC_REPORT_FATAL("sc_cast", "Width must be positive");

        if (str.find_first_of("xXzZ") != std::string::npos)
            SC_REPORT_WARNING("sc_cast",
                "Target is sc_bv (no X/Z support); X/Z bits will be treated as 0.");

        for (char& c : str)
            if (c == 'x' || c == 'X' || c == 'z' || c == 'Z') c = '0';

        if (std::any_of(str.begin(), str.end(), detail::safe_isspace)) {
            SC_REPORT_WARNING("sc_cast", "Whitespace found in input string — it was removed.");
            str.erase(std::remove_if(str.begin(), str.end(), detail::safe_isspace), str.end());
        }

        if (str.empty()) {
            SC_REPORT_ERROR("sc_cast", "Empty string input.");
            return detail::make_unknown<BV>();
        }

        size_t size = str.size();
        char c0 = str[0];
        char c1 = (size > 1) ? str[1] : 0;
        std::string substr2 = (size > 2) ? std::string(str.substr(2)) : "";
        std::string substr1 = (size > 1) ? std::string(str.substr(1)) : "";
        uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;

        if (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B')) {
            if (!std::regex_match(substr2, std::regex("^[01]+$"))) {
                SC_REPORT_ERROR("sc_cast", "Invalid characters in binary string.");
                return detail::make_unknown<BV>();
            }
            substr2 = detail::adjust_bitstr<BV>(std::move(substr2), WIDTH, is_data, "Binary");
            return detail::from_bitstr<BV>(substr2);
        }

        if ((str.find_first_not_of("01") == std::string::npos) && (base == 2)) {
            str = detail::adjust_bitstr<BV>(std::move(str), WIDTH, is_data, "Binary");
            return detail::from_bitstr<BV>(str);
        }

        if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X')) {
            std::string hex_str = substr2;
            if (!std::regex_match(hex_str, std::regex("^[0-9a-fA-F]+$"))) {
                SC_REPORT_ERROR("sc_cast", "Invalid characters in hex string.");
                return detail::make_unknown<BV>();
            }
            int bitsNum = static_cast<int>(hex_str.size()) * 4;
            if (bitsNum > WIDTH) {
                SC_REPORT_WARNING("sc_cast", "Hex string longer than WIDTH — keeping LSBs.");
                int drop_digits = (bitsNum - WIDTH + 3) / 4;
                hex_str = hex_str.substr(drop_digits);
            } else if (bitsNum < WIDTH) {
                int padDigits = static_cast<int>(std::ceil((WIDTH - bitsNum) / 4.0));
                hex_str = std::string(padDigits, '0') + hex_str;
            }
            std::string bitstr;
            for (char ch : hex_str) {
                int val = detail::safe_isdigit(ch) ? ch - '0' : std::toupper(ch) - 'A' + 10;
                bitstr += std::bitset<4>(val).to_string();
            }
            return detail::from_bitstr<BV>(bitstr);
        }

        if (size >= 2 && c0 == '0' && detail::safe_isdigit(c1)) {
            std::string oct_str = substr1;
            if (!std::regex_match(oct_str, std::regex("^[0-7]+$"))) {
                SC_REPORT_ERROR("sc_cast", "Invalid characters in octal string.");
                return detail::make_unknown<BV>();
            }
            int bitsNum = static_cast<int>(oct_str.size()) * 3;
            if (bitsNum > WIDTH) {
                SC_REPORT_WARNING("sc_cast", "Octal string longer than WIDTH — keeping LSBs.");
                int drop_digits = (bitsNum - WIDTH + 2) / 3;
                oct_str = oct_str.substr(drop_digits);
            } else if (bitsNum < WIDTH) {
                int padDigits = static_cast<int>(std::ceil((WIDTH - bitsNum) / 3.0));
                oct_str = std::string(padDigits, '0') + oct_str;
            }
            std::string bitstr;
            for (char ch : oct_str)
                bitstr += std::bitset<3>(ch - '0').to_string();
            return detail::from_bitstr<BV>(bitstr);
        }

        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$"))) {
            if (is_address) {
                if (str[0] == '+' || str[0] == '-') {
                    SC_REPORT_ERROR("sc_cast", "Address mode does not support signed decimal strings.");
                    return detail::make_unknown<BV>();
                }
                uint64_t uValue;
                if (!detail::parse_u64(str, 10, uValue)) {
                    SC_REPORT_ERROR("sc_cast", "Failed to parse decimal string.");
                    return detail::make_unknown<BV>();
                }
                if (uValue > umaxRange)
                    SC_REPORT_WARNING("sc_cast", "Decimal value overflows max for WIDTH");
                return sc_cast<BV>(uValue);
            } else {
                char sign = str[0];
                std::string dec_str = (sign == '+' || sign == '-') ? substr1 : str;
                uint64_t uValue;
                if (!detail::parse_u64(dec_str, 10, uValue)) {
                    SC_REPORT_ERROR("sc_cast", "Failed to parse decimal string.");
                    return detail::make_unknown<BV>();
                }
                int64_t Value = (sign == '-') ? -static_cast<int64_t>(uValue)
                                              : static_cast<int64_t>(uValue);
                return sc_cast<BV>(Value);
            }
        }

        SC_REPORT_ERROR("sc_cast", "Invalid string format for sc_bv conversion.");
        return detail::make_unknown<BV>();
    }

    // sc_logic → sc_bv
    template <typename BV>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(const sc_logic& lg)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (lg.is_01())
            return lg.to_bool() ? BV(std::string(WIDTH, '1').c_str())
                                : BV(std::string(WIDTH, '0').c_str());
        SC_REPORT_WARNING("sc_cast", "sc_logic has X/Z; converting to 0 in sc_bv.");
        return BV(std::string(WIDTH, '0').c_str());
    }

    // sc_lv → sc_bv
    template <typename BV, int W2>
    std::enable_if_t<is_sc_bv<BV>::value, BV>
    sc_cast(const sc_lv<W2>& lv)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (!lv.is_01())
            SC_REPORT_WARNING("sc_cast", "sc_lv contains X/Z; converting to 0 in sc_bv.");
        std::string bitstr = lv.to_string();
        for (char& c : bitstr)
            if (c != '0' && c != '1') c = '0';
        if (static_cast<int>(bitstr.size()) < WIDTH)
            bitstr = std::string(WIDTH - bitstr.size(), '0') + bitstr;
        else if (static_cast<int>(bitstr.size()) > WIDTH)
            bitstr = bitstr.substr(bitstr.size() - WIDTH);
        return BV(bitstr.c_str());
    }

    // sc_int family → sc_bv
    template <typename BV, typename INT>
    std::enable_if_t<is_sc_bv<BV>::value && is_sc_intlike<INT>::value, BV>
    sc_cast(const INT& val)
    {
        constexpr int INT_W = sc_lv_traits<INT>::width;
        if constexpr (INT_W <= 64) {
            return BV(val.to_uint64());
        } else {
            std::string binstr = val.to_string(SC_BIN);
            return sc_cast<BV>(binstr, "data", 2);
        }
    }

    // ===== FROM sc_bv<W> (to primitive types only) =====

    template <typename T = bool, typename BV>
    std::enable_if_t<std::is_same<T, bool>::value && is_sc_bv<BV>::value, T>
    sc_cast(const BV& bv) { return bv.get_bit(0); }

    template <typename T, typename BV>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value
                     && is_sc_bv<BV>::value, T>
    sc_cast(const BV& bv)
    {
        if constexpr (std::is_signed<T>::value) {
            int64_t val = bv.to_int64();
            if (val < std::numeric_limits<T>::min() || val > std::numeric_limits<T>::max())
                SC_REPORT_WARNING("sc_cast", "Signed overflow in conversion from sc_bv to target type");
            return static_cast<T>(val);
        } else {
            uint64_t val = bv.to_uint64();
            if (val > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                SC_REPORT_WARNING("sc_cast", "Unsigned overflow in conversion from sc_bv to target type");
            return static_cast<T>(val);
        }
    }

    template <typename T = float, typename BV>
    std::enable_if_t<std::is_same<T, float>::value && is_sc_bv<BV>::value, T>
    sc_cast(const BV& bv)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (WIDTH < 32)
            SC_REPORT_FATAL("sc_cast", "Float needs at least 32 bits for sc_bv->float");
        uint32_t raw = static_cast<uint32_t>(bv.to_uint64() & 0xFFFFFFFFu);
        float f;
        std::memcpy(&f, &raw, sizeof(f));
        return f;
    }

    template <typename T = double, typename BV>
    std::enable_if_t<std::is_same<T, double>::value && is_sc_bv<BV>::value, T>
    sc_cast(const BV& bv)
    {
        constexpr int WIDTH = sc_lv_traits<BV>::width;
        if (WIDTH < 64)
            SC_REPORT_FATAL("sc_cast", "Double needs at least 64 bits for sc_bv->double");
        uint64_t raw = bv.to_uint64();
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    template <typename T = std::string, typename BV>
    std::enable_if_t<std::is_same<T, std::string>::value && is_sc_bv<BV>::value, T>
    sc_cast(const BV& bv) { return bv.to_string(); }

    // ===== string_cast specialization =====
    template <typename T>
    std::enable_if_t<is_sc_bv<T>::value, T>
    string_cast(std::string_view str,
                std::string_view mode = "data",
                int base = 2)
    {
        return sc_cast<T>(str, mode, base);
    }

} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_BV_HPP