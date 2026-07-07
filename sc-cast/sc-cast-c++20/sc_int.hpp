#ifndef SC_CAST_INT_HPP
#define SC_CAST_INT_HPP

// ============================================================================
// sc_cast :: sc_int.hpp  (C++20)
// Conversions to/from sc_int<W>, sc_uint<W>, sc_bigint<W>, sc_biguint<W>
// ============================================================================

#include "utils.hpp"

namespace sc_cast
{
    // ========================================================================
    // TO sc_int family
    // ========================================================================

    // Identity
    template <ScIntLike INT>
    INT sc_lv_cast(const INT& val) { return val; }

    // bool → sc_int family
    template <ScIntLike INT>
    INT sc_lv_cast(bool value)
    {
        return INT(value ? 1 : 0);
    }

    // integral (not bool) → sc_int family
    template <ScIntLike INT, std::integral T>
        requires (!std::same_as<T, bool>)
    INT sc_lv_cast(T value, std::string_view mode_view = "data", bool MSB = false)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<INT>();
        }

        constexpr int WIDTH = sc_lv_traits<INT>::width;
        if (WIDTH <= 0) {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return detail::make_unknown<INT>();
        }

        if constexpr (ScSignedInt<INT>) {
            if (is_address && value < 0)
                SC_REPORT_FATAL("sc_lv_cast", "Address mode can't be negative");
        }

        if (!MSB) {
            return INT(static_cast<int64_t>(value));
        } else {
            int inputTypeBitSize = sizeof(T) * 8;
            if (WIDTH >= inputTypeBitSize)
                return INT(static_cast<int64_t>(value));
            int shift = inputTypeBitSize - WIDTH;
            SC_REPORT_INFO("sc_lv_cast", "MSB mode: keeping upper bits of input value");
            return INT(static_cast<int64_t>(
                std::is_signed_v<T> ? (static_cast<int64_t>(value) >> shift)
                                    : (static_cast<uint64_t>(value) >> shift)));
        }
    }

    // float → sc_int family (truncate fractional part)
    template <ScIntLike INT>
    INT sc_lv_cast(float value)
    {
        SC_REPORT_WARNING("sc_lv_cast",
            "Float-to-sc_int conversion truncates fractional part.");
        return INT(static_cast<int64_t>(value));
    }

    // double → sc_int family (truncate fractional part)
    template <ScIntLike INT>
    INT sc_lv_cast(double value)
    {
        SC_REPORT_WARNING("sc_lv_cast",
            "Double-to-sc_int conversion truncates fractional part.");
        return INT(static_cast<int64_t>(value));
    }

    // string → sc_int family
    template <ScIntLike INT>
    INT sc_lv_cast(std::string_view input_str,
                   std::string_view mode_view = "data",
                   int base = 2)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<INT>();
        }

        std::string str(input_str);
        // C++20: std::ranges::any_of + std::erase_if
        if (std::ranges::any_of(str, detail::safe_isspace)) {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            std::erase_if(str, detail::safe_isspace);
        }
        if (str.empty()) {
            SC_REPORT_ERROR("sc_lv_cast", "Empty string input.");
            return detail::make_unknown<INT>();
        }

        // X/Z-only string → 0
        if (str.find_first_not_of("xXzZ") == std::string::npos) {
            SC_REPORT_WARNING("sc_lv_cast", "String is all X/Z. Returning 0.");
            return INT(0);
        }

        size_t size = str.size();
        char c0 = str[0];
        char c1 = (size > 1) ? str[1] : 0;
        std::string substr2 = (size > 2) ? std::string(str.substr(2)) : "";
        std::string substr1 = (size > 1) ? std::string(str.substr(1)) : "";

        constexpr int WIDTH = sc_lv_traits<INT>::width;

        // Detect numeric base
        int detected_base = base;
        std::string num_str = str;

        if (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B')) {
            detected_base = 2;  num_str = substr2;
        } else if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X')) {
            detected_base = 16; num_str = substr2;
        } else if (size >= 2 && c0 == '0' && detail::safe_isdigit(c1)) {
            detected_base = 8;  num_str = substr1;
        } else if (std::regex_match(str, std::regex("^[+-]?[0-9]+$"))) {
            detected_base = 10;
        } else if (detected_base == 2 && str.find_first_not_of("01") == std::string::npos) {
            num_str = str;
        } else {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid string format for sc_int conversion.");
            return detail::make_unknown<INT>();
        }

        // Address mode: no sign allowed
        if (is_address && (num_str[0] == '+' || num_str[0] == '-')) {
            SC_REPORT_ERROR("sc_lv_cast", "Address mode does not support signed decimal strings.");
            return detail::make_unknown<INT>();
        }

        // Strip sign
        char sign = 0;
        if (!num_str.empty() && (num_str[0] == '+' || num_str[0] == '-')) {
            sign = num_str[0];
            num_str = num_str.substr(1);
        }

        if constexpr (WIDTH <= 64) {
            auto parsed = detail::parse_u64(num_str, detected_base);
            if (!parsed) {
                SC_REPORT_ERROR("sc_lv_cast", "Failed to parse numeric string for sc_int.");
                return detail::make_unknown<INT>();
            }
            int64_t v = (sign == '-') ? -static_cast<int64_t>(*parsed)
                                      : static_cast<int64_t>(*parsed);
            return INT(v);
        } else {
            // sc_bigint/sc_biguint > 64 bits: build from string directly
            std::string signed_str = (sign == '-') ? ("-" + num_str) : num_str;
            sc_numrep sn = SC_DEC;
            if (detected_base == 2)       sn = SC_BIN;
            else if (detected_base == 8)  sn = SC_OCT;
            else if (detected_base == 16) sn = SC_HEX;
            return INT(signed_str.c_str(), sn);
        }
    }

    // sc_lv → sc_int family
    template <ScIntLike INT, int W2>
    INT sc_lv_cast(const sc_lv<W2>& lv)
    {
        if (!lv.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "sc_lv has X/Z. Returning 0.");
            return INT(0);
        }
        if constexpr (ScSignedInt<INT>)
            return INT(lv.to_int64());
        else
            return INT(lv.to_uint64());
    }

    // sc_bv → sc_int family
    template <ScIntLike INT, int W2>
    INT sc_lv_cast(const sc_bv<W2>& bv)
    {
        if constexpr (ScSignedInt<INT>)
            return INT(bv.to_int64());
        else
            return INT(bv.to_uint64());
    }

    // sc_logic → sc_int family
    template <ScIntLike INT>
    INT sc_lv_cast(const sc_logic& lg)
    {
        if (!lg.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "sc_logic is X/Z. Returning 0.");
            return INT(0);
        }
        return INT(lg.to_bool() ? 1 : 0);
    }

    // ========================================================================
    // FROM sc_int family
    // ========================================================================

    // sc_int family → bool
    template <typename T = bool, ScIntLike INT>
        requires std::same_as<T, bool>
    T sc_lv_cast(const INT& val)
    {
        return (val.to_uint64() & 1ULL) != 0;
    }

    // sc_int family → integral (not bool)
    template <std::integral T, ScIntLike INT>
        requires (!std::same_as<T, bool>)
    T sc_lv_cast(const INT& val)
    {
        if constexpr (std::is_signed_v<T>) {
            int64_t v = val.to_int64();
            if (v < std::numeric_limits<T>::min() || v > std::numeric_limits<T>::max())
                SC_REPORT_WARNING("sc_lv_cast", "Signed overflow in conversion from sc_int to target type");
            return static_cast<T>(v);
        } else {
            uint64_t v = val.to_uint64();
            if (v > static_cast<uint64_t>(std::numeric_limits<T>::max()))
                SC_REPORT_WARNING("sc_lv_cast", "Unsigned overflow in conversion from sc_int to target type");
            return static_cast<T>(v);
        }
    }

    // sc_int family → float/double (numeric, not bit-reinterpret)
    template <std::floating_point T, ScIntLike INT>
    T sc_lv_cast(const INT& val)
    {
        if constexpr (ScSignedInt<INT>)
            return static_cast<T>(static_cast<int64_t>(val.to_int64()));
        else
            return static_cast<T>(val.to_uint64());
    }

    // sc_int family → string
    template <typename T = std::string, ScIntLike INT>
        requires std::same_as<T, std::string>
    T sc_lv_cast(const INT& val)
    {
        return val.to_string(SC_DEC);
    }

    // sc_int family → sc_lv
    template <ScLv T, ScIntLike INT>
    T sc_lv_cast(const INT& val)
    {
        constexpr int INT_W = sc_lv_traits<INT>::width;
        if constexpr (INT_W <= 64) {
            return T(val.to_uint64());
        } else {
            std::string binstr = val.to_string(SC_BIN);
            return sc_lv_cast<T>(binstr, "data", 2);
        }
    }

    // sc_int family → sc_bv
    template <ScBv T, ScIntLike INT>
    T sc_lv_cast(const INT& val)
    {
        constexpr int INT_W = sc_lv_traits<INT>::width;
        if constexpr (INT_W <= 64) {
            return T(val.to_uint64());
        } else {
            std::string binstr = val.to_string(SC_BIN);
            return sc_lv_cast<T>(binstr, "data", 2);
        }
    }

    // sc_int family → sc_logic (LSB)
    template <typename T = sc_logic, ScIntLike INT>
        requires std::same_as<T, sc_logic>
    T sc_lv_cast(const INT& val)
    {
        return (val.to_uint64() & 1ULL) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // ========================================================================
    // string_cast specialization for sc_int family
    // ========================================================================
    template <ScIntLike T>
    T string_cast(std::string_view str,
                  std::string_view mode = "data",
                  int base = 2)
    {
        return sc_lv_cast<T>(str, mode, base);
    }

} // namespace sc_cast

#endif // SC_CAST_INT_HPP