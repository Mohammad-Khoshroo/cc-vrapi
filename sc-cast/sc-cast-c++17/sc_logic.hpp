#ifndef CC_VRWRAPPER_LOGIC_HPP
#define CC_VRWRAPPER_LOGIC_HPP

// ============================================================================
// cc_vrwrapper :: sc_logic.hpp  (C++17)
// Conversions TO sc_logic  (destination = sc_logic)
// + FROM sc_logic to primitive types (bool/int/string)
// ============================================================================

#include "utils.hpp"

namespace cc_vrwrapper
{
    // ===== TO sc_logic =====
    // All "TO sc_logic" functions are templated on <typename T> with
    // enable_if<is_sc_logic<T>> so that sc_cast<sc_logic>(...) works.

    // Identity
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(const T& lg) { return lg; }

    // bool → sc_logic
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(bool value)
    {
        return T(value ? SC_LOGIC_1 : SC_LOGIC_0);
    }

    // integral (not bool) → sc_logic (keep LSB)
    template <typename T, typename I>
    std::enable_if_t<is_sc_logic<T>::value
                     && std::is_integral<I>::value
                     && !std::is_same<I, bool>::value, T>
    sc_cast(I value, std::string_view mode_view = "data", bool /*MSB*/ = false)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_cast", "Invalid mode. Use \"data\" or \"address\".");
            return T(SC_LOGIC_X);
        }
        if (is_address && value < 0)
            SC_REPORT_FATAL("sc_cast", "Address mode can't be negative");
        return T((value & 1) ? SC_LOGIC_1 : SC_LOGIC_0);
    }

    // string → sc_logic
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(std::string_view input_str,
            std::string_view mode_view = "data",
            int /*base*/ = 2)
    {
        bool is_address, is_data;
        if (!detail::check_mode(mode_view, is_address, is_data)) {
            SC_REPORT_ERROR("sc_cast", "Invalid mode. Use \"data\" or \"address\".");
            return T(SC_LOGIC_X);
        }

        std::string str(input_str);
        if (std::any_of(str.begin(), str.end(), detail::safe_isspace)) {
            SC_REPORT_WARNING("sc_cast", "Whitespace found in input string — it was removed.");
            str.erase(std::remove_if(str.begin(), str.end(), detail::safe_isspace), str.end());
        }
        if (str.empty()) {
            SC_REPORT_ERROR("sc_cast", "Empty string input. Returning X.");
            return T(SC_LOGIC_X);
        }
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$"))) {
            std::string num_part = (str[0]=='+'||str[0]=='-') ? str.substr(1) : str;
            uint64_t v = 0;
            if (!detail::parse_u64(num_part, 10, v)) {
                SC_REPORT_ERROR("sc_cast", "Failed to parse decimal string for sc_logic.");
                return T(SC_LOGIC_X);
            }
            return T((v & 1) ? SC_LOGIC_1 : SC_LOGIC_0);
        }
        char c = str.back();
        if (c == '1')              return T(SC_LOGIC_1);
        if (c == '0')              return T(SC_LOGIC_0);
        if (c == 'Z' || c == 'z')  return T(SC_LOGIC_Z);
        if (c == 'X' || c == 'x')  return T(SC_LOGIC_X);
        SC_REPORT_ERROR("sc_cast", "Invalid character for sc_logic.");
        return T(SC_LOGIC_X);
    }

    // sc_lv → sc_logic (LSB)
    template <typename T, int W>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(const sc_lv<W>& lv)
    {
        return T(lv.get_bit(0) ? SC_LOGIC_1 : SC_LOGIC_0);
    }

    // sc_bv → sc_logic (LSB)
    template <typename T, int W>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(const sc_bv<W>& bv)
    {
        return T(bv.get_bit(0) ? SC_LOGIC_1 : SC_LOGIC_0);
    }

    // sc_int family → sc_logic (LSB)
    template <typename T, typename INT>
    std::enable_if_t<is_sc_logic<T>::value && is_sc_intlike<INT>::value, T>
    sc_cast(const INT& val)
    {
        return T((val.to_uint64() & 1ULL) ? SC_LOGIC_1 : SC_LOGIC_0);
    }

    // float/double → sc_logic: not meaningful (error)
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(float)
    {
        SC_REPORT_ERROR("sc_cast", "Cannot convert float to sc_logic (needs 32 bits).");
        return T(SC_LOGIC_X);
    }
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    sc_cast(double)
    {
        SC_REPORT_ERROR("sc_cast", "Cannot convert double to sc_logic (needs 64 bits).");
        return T(SC_LOGIC_X);
    }

    // ===== FROM sc_logic (to primitive types only) =====

    // sc_logic → bool
    template <typename T = bool>
    std::enable_if_t<std::is_same<T, bool>::value, T>
    sc_cast(const sc_logic& lg)
    {
        if (!lg.is_01()) {
            SC_REPORT_WARNING("sc_cast", "sc_logic is X/Z. Returning false.");
            return false;
        }
        return lg.to_bool();
    }

    // sc_logic → integral (not bool)
    template <typename T>
    std::enable_if_t<std::is_integral<T>::value && !std::is_same<T, bool>::value, T>
    sc_cast(const sc_logic& lg)
    {
        if (!lg.is_01()) {
            SC_REPORT_WARNING("sc_cast", "sc_logic is X/Z. Returning 0.");
            return 0;
        }
        return lg.to_bool() ? static_cast<T>(1) : static_cast<T>(0);
    }

    // sc_logic → string (single char)
    template <typename T = std::string>
    std::enable_if_t<std::is_same<T, std::string>::value, T>
    sc_cast(const sc_logic& lg)
    {
        return std::string(1, lg.to_char());
    }

    // ===== string_cast specialization =====
    template <typename T>
    std::enable_if_t<is_sc_logic<T>::value, T>
    string_cast(std::string_view str,
                std::string_view mode = "data",
                int base = 2)
    {
        return sc_cast<T>(str, mode, base);
    }

} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_LOGIC_HPP