#ifndef SC_CAST_INT_HPP
#define SC_CAST_INT_HPP

// ============================================================================
// sc_cast :: sc_int.hpp
// Conversions to/from sc_int<W>, sc_uint<W>, sc_bigint<W>, sc_biguint<W>
// ============================================================================

#include "utils.hpp"

namespace sc_cast
{
    // ========================================================================
    // TO sc_int family
    // ========================================================================

    // Identity
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(const INT& val) { return val; }

    // bool → sc_int family
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(bool value)
    {
        return INT(value ? 1 : 0);
    }

    // integral (not bool) → sc_int family
    template <typename INT, typename T>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value
                     && is_sc_intlike<INT>::value, INT>
    sc_lv_cast(T value, std::string_view mode_view = "data", bool MSB = false)
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

        if constexpr (is_sc_signed_int<INT>::value) {
            if (is_address && value < 0)
                SC_REPORT_FATAL("sc_lv_cast", "Address mode can't be negative");
        }

        if (!MSB) {
            // LSB mode: keep LSBs
            return INT(static_cast<int64_t>(value));
        } else {
            // MSB mode: keep MSBs
            int inputTypeBitSize = sizeof(T) * 8;
            if (WIDTH >= inputTypeBitSize)
                return INT(static_cast<int64_t>(value));
            int shift = inputTypeBitSize - WIDTH;
            SC_REPORT_INFO("sc_lv_cast", "MSB mode: keeping upper bits of input value");
            return INT(static_cast<int64_t>(
                std::is_signed<T>::value ? (static_cast<int64_t>(value) >> shift)
                                         : (static_cast<uint64_t>(value) >> shift)));
        }
    }

    // float → sc_int family (truncate to integer)
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(float value)
    {
        SC_REPORT_WARNING("sc_lv_cast",
            "Float-to-sc_int conversion truncates fractional part.");
        return INT(static_cast<int64_t>(value));
    }

    // double → sc_int family (truncate to integer)
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(double value)
    {
        SC_REPORT_WARNING("sc_lv_cast",
            "Double-to-sc_int conversion truncates fractional part.");
        return INT(static_cast<int64_t>(value));
    }

    // string → sc_int family
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(std::string_view input_str,
               std::string_view mode_view = "data",
               int base = 2)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return detail::make_unknown<INT>();
        }

        std::string str(input_str);
        if (std::any_of(str.begin(), str.end(), detail::safe_isspace)) {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            str.erase(std::remove_if(str.begin(), str.end(), detail::safe_isspace), str.end());
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
        bool is_signed_target = is_sc_signed_int<INT>::value;

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

        uint64_t uValue = 0;
        if (!detail::parse_u64(num_str, detected_base, uValue)) {
            SC_REPORT_ERROR("sc_lv_cast", "Failed to parse numeric string for sc_int.");
            return detail::make_unknown<INT>();
        }

        // Build the result
        if constexpr (WIDTH <= 64) {
            int64_t v = (sign == '-') ? -static_cast<int64_t>(uValue)
                                      : static_cast<int64_t>(uValue);
            return INT(v);
        } else {
            // For sc_bigint/sc_biguint > 64 bits, construct from string directly
            std::string signed_str = (sign == '-') ? ("-" + num_str) : num_str;
            // Use the original base for construction
            sc_numrep sn = SC_DEC;
            if (detected_base == 2)  sn = SC_BIN;
            else if (detected_base == 8)  sn = SC_OCT;
            else if (detected_base == 16) sn = SC_HEX;
            return INT(signed_str.c_str(), sn);
        }
    }

    // sc_lv → sc_int family
    template <typename INT, int W2>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(const sc_lv<W2>& lv)
    {
        if (!lv.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "sc_lv has X/Z. Returning 0.");
            return INT(0);
        }
        if constexpr (is_sc_signed_int<INT>::value)
            return INT(lv.to_int64());
        else
            return INT(lv.to_uint64());
    }

    // sc_bv → sc_int family
    template <typename INT, int W2>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(const sc_bv<W2>& bv)
    {
        if constexpr (is_sc_signed_int<INT>::value)
            return INT(bv.to_int64());
        else
            return INT(bv.to_uint64());
    }

    // sc_logic → sc_int family
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, INT>
    sc_lv_cast(const sc_logic& lg)
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
    template <typename T = bool, typename INT>
    std::enable_if_t<std::is_same<T, bool>::value && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        return (val.to_uint64() & 1ULL) != 0;
    }

    // sc_int family → integral (not bool)
    template <typename T, typename INT>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value
                     && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        if constexpr (std::is_signed<T>::value) {
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
    template <typename T, typename INT>
    std::enable_if_t<std::is_floating_point<T>::value && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        if constexpr (is_sc_signed_int<INT>::value)
            return static_cast<T>(static_cast<int64_t>(val.to_int64()));
        else
            return static_cast<T>(val.to_uint64());
    }

    // sc_int family → string
    template <typename T = std::string, typename INT>
    std::enable_if_t<std::is_same<T, std::string>::value && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        return val.to_string(SC_DEC);
    }

    // sc_int family → sc_lv (cross conversion)
    template <typename T, typename INT>
    std::enable_if_t<is_sc_lv<T>::value && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        constexpr int T_W = sc_lv_traits<T>::width;
        constexpr int INT_W = sc_lv_traits<INT>::width;
        if constexpr (INT_W <= 64) {
            return T(val.to_uint64());
        } else {
            std::string binstr = val.to_string(SC_BIN);
            return sc_lv_cast<T>(binstr, "data", 2);
        }
    }

    // sc_int family → sc_bv (cross conversion)
    template <typename T, typename INT>
    std::enable_if_t<is_sc_bv<T>::value && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        constexpr int T_W = sc_lv_traits<T>::width;
        constexpr int INT_W = sc_lv_traits<INT>::width;
        if constexpr (INT_W <= 64) {
            return T(val.to_uint64());
        } else {
            std::string binstr = val.to_string(SC_BIN);
            return sc_lv_cast<T>(binstr, "data", 2);
        }
    }

    // sc_int family → sc_logic (LSB)
    template <typename T = sc_logic, typename INT>
    std::enable_if_t<std::is_same<T, sc_logic>::value && is_sc_intlike<INT>::value, T>
    sc_lv_cast(const INT& val)
    {
        return (val.to_uint64() & 1ULL) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // ========================================================================
    // string_cast specialization for sc_int family
    // ========================================================================
    template <typename T>
    std::enable_if_t<is_sc_intlike<T>::value, T>
    string_cast(std::string_view str,
                std::string_view mode = "data",
                int base = 2)
    {
        return sc_lv_cast<T>(str, mode, base);
    }

} // namespace sc_cast

#endif // SC_CAST_INT_HPP