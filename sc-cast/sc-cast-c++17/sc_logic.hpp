#ifndef SC_CAST_LOGIC_HPP
#define SC_CAST_LOGIC_HPP

// ============================================================================
// sc_cast :: sc_logic.hpp
// Conversions to/from sc_logic (single-bit, supports 0/1/X/Z)
// ============================================================================

#include "utils.hpp"

namespace sc_cast
{
    // ========================================================================
    // TO sc_logic
    // ========================================================================

    // Identity: sc_logic → sc_logic
    inline sc_logic sc_lv_cast(const sc_logic& lg) { return lg; }

    // bool → sc_logic
    inline sc_logic sc_lv_cast(bool value)
    {
        return value ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // integral (not bool) → sc_logic (keep LSB)
    template <typename T>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, sc_logic>
    sc_lv_cast(T value, std::string_view mode_view = "data", bool /*MSB*/ = false)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return SC_LOGIC_X;
        }
        if (is_address && value < 0)
            SC_REPORT_FATAL("sc_lv_cast", "Address mode can't be negative");
        return (value & 1) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // string → sc_logic
    // Accepts: "0","1","x","X","z","Z" (last char wins if longer)
    inline sc_logic sc_lv_cast(std::string_view input_str,
                               std::string_view mode_view = "data",
                               int base = 2)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"data\" or \"address\".");
            return SC_LOGIC_X;
        }

        std::string str(input_str);
        // Strip whitespace
        if (std::any_of(str.begin(), str.end(), detail::safe_isspace)) {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            str.erase(std::remove_if(str.begin(), str.end(), detail::safe_isspace), str.end());
        }
        if (str.empty()) {
            SC_REPORT_ERROR("sc_lv_cast", "Empty string input. Returning X.");
            return SC_LOGIC_X;
        }
        // For longer numeric strings, take LSB (last meaningful bit)
        // Try decimal first
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$"))) {
            uint64_t v = 0;
            std::string num_part = (str[0]=='+'||str[0]=='-') ? str.substr(1) : str;
            if (!detail::parse_u64(num_part, 10, v)) {
                SC_REPORT_ERROR("sc_lv_cast", "Failed to parse decimal string for sc_logic.");
                return SC_LOGIC_X;
            }
            return (v & 1) ? SC_LOGIC_1 : SC_LOGIC_0;
        }
        // Otherwise take last char
        char c = str.back();
        if (c == '1') return SC_LOGIC_1;
        if (c == '0') return SC_LOGIC_0;
        if (c == 'Z' || c == 'z') return SC_LOGIC_Z;
        if (c == 'X' || c == 'x') return SC_LOGIC_X;
        SC_REPORT_ERROR("sc_lv_cast", "Invalid character for sc_logic.");
        return SC_LOGIC_X;
    }

    // sc_lv → sc_logic (take LSB)
    template <int W>
    sc_logic sc_lv_cast(const sc_lv<W>& lv)
    {
        return lv.get_bit(0) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // sc_bv → sc_logic (take LSB)
    template <int W>
    sc_logic sc_lv_cast(const sc_bv<W>& bv)
    {
        return bv.get_bit(0) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // sc_int/sc_uint/sc_bigint/sc_biguint → sc_logic (take LSB)
    template <typename INT>
    std::enable_if_t<is_sc_intlike<INT>::value, sc_logic>
    sc_lv_cast(const INT& val)
    {
        return (val.to_uint64() & 1ULL) ? SC_LOGIC_1 : SC_LOGIC_0;
    }

    // float/double → sc_logic: not meaningful (error)
    inline sc_logic sc_lv_cast(float)
    {
        SC_REPORT_ERROR("sc_lv_cast", "Cannot convert float to sc_logic (needs 32 bits).");
        return SC_LOGIC_X;
    }
    inline sc_logic sc_lv_cast(double)
    {
        SC_REPORT_ERROR("sc_lv_cast", "Cannot convert double to sc_logic (needs 64 bits).");
        return SC_LOGIC_X;
    }

    // ========================================================================
    // FROM sc_logic
    // ========================================================================

    // sc_logic → bool (X/Z → false with warning)
    template <typename T = bool>
    std::enable_if_t<std::is_same<T, bool>::value, T>
    sc_lv_cast(const sc_logic& lg)
    {
        if (!lg.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "sc_logic is X/Z. Returning false.");
            return false;
        }
        return lg.to_bool();
    }

    // sc_logic → integral (not bool)
    template <typename T>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, T>
    sc_lv_cast(const sc_logic& lg)
    {
        if (!lg.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "sc_logic is X/Z. Returning 0.");
            return 0;
        }
        return lg.to_bool() ? static_cast<T>(1) : static_cast<T>(0);
    }

    // sc_logic → string (single char)
    template <typename T = std::string>
    std::enable_if_t<std::is_same<T, std::string>::value, T>
    sc_lv_cast(const sc_logic& lg)
    {
        return std::string(1, lg.to_char());
    }

    // sc_logic → sc_lv (cross conversion)
    template <typename T, typename LV>
    std::enable_if_t<is_sc_lv<T>::value, T>
    sc_lv_cast(const sc_logic& lg)
    {
        constexpr int WIDTH = sc_lv_traits<T>::width;
        char c = lg.to_char();
        return T(std::string(WIDTH, c).c_str());
    }

    // sc_logic → sc_bv (cross conversion)
    template <typename T, typename BV>
    std::enable_if_t<is_sc_bv<T>::value, T>
    sc_lv_cast(const sc_logic& lg)
    {
        constexpr int WIDTH = sc_lv_traits<T>::width;
        if (lg.is_01())
            return lg.to_bool() ? T(std::string(WIDTH, '1').c_str())
                                : T(std::string(WIDTH, '0').c_str());
        SC_REPORT_WARNING("sc_lv_cast", "sc_logic has X/Z; converting to 0 in sc_bv.");
        return T(std::string(WIDTH, '0').c_str());
    }

    // sc_logic → sc_int/sc_uint/sc_bigint/sc_biguint
    template <typename T, typename LG>
    std::enable_if_t<is_sc_intlike<T>::value && std::is_same<LG, sc_logic>::value, T>
    sc_lv_cast(const LG& lg)
    {
        if (!lg.is_01()) {
            SC_REPORT_WARNING("sc_lv_cast", "sc_logic is X/Z. Returning 0.");
            return T(0);
        }
        return T(lg.to_bool() ? 1 : 0);
    }

    // ========================================================================
    // string_cast specialization for sc_logic
    // ========================================================================
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    string_cast(std::string_view str,
                std::string_view mode = "data",
                int base = 2)
    {
        return sc_lv_cast<T>(str, mode, base);
    }

} // namespace sc_cast

#endif // SC_CAST_LOGIC_HPP