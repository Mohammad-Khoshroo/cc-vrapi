#ifndef CC_VRWRAPPER_UTILS_HPP
#define CC_VRWRAPPER_UTILS_HPP

// ============================================================================
// cc_vrwrapper :: utils.hpp  (C++17)
// Common utilities: type traits, helpers, generic string_cast
// ============================================================================

#include <systemc>
#include <type_traits>
#include <string>
#include <string_view>
#include <sstream>
#include <limits>
#include <bitset>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <typeinfo>
#include <stdexcept>
#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#  include <cxxabi.h>
#  include <cstdlib>
#  define CC_VRWRAPPER_HAS_CXXABI 1
#endif

// ----------------------------------------------------------------------------
// type_name — compiler-safe demangling
// ----------------------------------------------------------------------------
template <typename T>
std::string type_name(const T& var)
{
#if defined(CC_VRWRAPPER_HAS_CXXABI)
    int status;
    char* demangled = abi::__cxa_demangle(typeid(var).name(), 0, 0, &status);
    std::string result = (status == 0 && demangled) ? demangled : typeid(var).name();
    free(demangled);
    return result;
#else
    return typeid(var).name();
#endif
}

namespace cc_vrwrapper
{
    using namespace sc_core;
    using namespace sc_dt;
    using namespace std;

    // ========================================================================
    // TYPE TRAITS
    // ========================================================================

    template <typename T> struct is_sc_lv      : std::false_type {};
    template <int W>      struct is_sc_lv<sc_lv<W>>      : std::true_type {};

    template <typename T> struct is_sc_bv      : std::false_type {};
    template <int W>      struct is_sc_bv<sc_bv<W>>      : std::true_type {};

    template <typename T> struct is_sc_logic   : std::false_type {};
    template <>           struct is_sc_logic<sc_logic>   : std::true_type {};

    template <typename T> struct is_sc_int     : std::false_type {};
    template <int W>      struct is_sc_int<sc_int<W>>     : std::true_type {};

    template <typename T> struct is_sc_uint    : std::false_type {};
    template <int W>      struct is_sc_uint<sc_uint<W>>    : std::true_type {};

    template <typename T> struct is_sc_bigint  : std::false_type {};
    template <int W>      struct is_sc_bigint<sc_bigint<W>>  : std::true_type {};

    template <typename T> struct is_sc_biguint : std::false_type {};
    template <int W>      struct is_sc_biguint<sc_biguint<W>> : std::true_type {};

    // String-like trait to prevent implicit conversions to bool/integral
    template <typename T> struct is_string_like : std::false_type {};
    template <>           struct is_string_like<const char*>       : std::true_type {};
    template <>           struct is_string_like<char*>             : std::true_type {};
    template <>           struct is_string_like<std::string>       : std::true_type {};
    template <>           struct is_string_like<std::string_view>  : std::true_type {};
    // Also handle const/ref-qualified versions
    template <typename T> struct is_string_like<const T> : is_string_like<T> {};
    template <typename T> struct is_string_like<T&>      : is_string_like<T> {};
    template <typename T> struct is_string_like<const T&>: is_string_like<T> {};

    template <typename T>
    struct is_sc_logiclike : std::integral_constant<bool,
        is_sc_lv<T>::value || is_sc_bv<T>::value || is_sc_logic<T>::value> {};

    template <typename T>
    struct is_sc_intlike : std::integral_constant<bool,
        is_sc_int<T>::value || is_sc_uint<T>::value ||
        is_sc_bigint<T>::value || is_sc_biguint<T>::value> {};

    template <typename T>
    struct is_sc_signed_int : std::integral_constant<bool,
        is_sc_int<T>::value || is_sc_bigint<T>::value> {};

    template <typename T>
    struct is_sc_unsigned_int : std::integral_constant<bool,
        is_sc_uint<T>::value || is_sc_biguint<T>::value> {};

    template <typename T> struct sc_lv_traits;
    template <int W> struct sc_lv_traits<sc_lv<W>>      { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_bv<W>>      { static constexpr int width = W; };
    template <>      struct sc_lv_traits<sc_logic>       { static constexpr int width = 1; };
    template <int W> struct sc_lv_traits<sc_int<W>>     { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_uint<W>>    { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_bigint<W>>  { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_biguint<W>> { static constexpr int width = W; };

    // ========================================================================
    // DETAIL HELPERS
    // ========================================================================
    namespace detail
    {
        inline bool safe_isspace(char c) {
            return std::isspace(static_cast<unsigned char>(c)) != 0;
        }
        inline bool safe_isdigit(char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        }

        inline bool parse_u64(const std::string& s, int base, uint64_t& out)
        {
            try {
                size_t idx = 0;
                out = std::stoull(s, &idx, base);
                return idx == s.size();
            } catch (...) {
                return false;
            }
        }

        inline bool check_mode(std::string_view mode_view, bool& is_address, bool& is_data)
        {
            is_address = (mode_view == "address");
            is_data    = (mode_view == "data");
            return is_address || is_data;
        }

        template <typename T>
        T make_unknown()
        {
            if constexpr (is_sc_bv<T>::value)
                return T(std::string(sc_lv_traits<T>::width, '0').c_str());
            else if constexpr (is_sc_lv<T>::value)
                return T(std::string(sc_lv_traits<T>::width, 'X').c_str());
            else if constexpr (is_sc_logic<T>::value)
                return T(SC_LOGIC_X);
            else
                return T(0);
        }

                template <typename LV>
        std::enable_if_t<is_sc_logiclike<LV>::value, std::string>
        adjust_bitstr(std::string bitstr, int WIDTH, bool is_data,
                      std::string_view type_label)
        {
            int sz = static_cast<int>(bitstr.size());
            if (sz > WIDTH)
            {
                if constexpr (!is_sc_logic<LV>::value)
                {
                    std::string msg = std::string(type_label) +
                        " string longer than WIDTH — keeping LSBs.";
                    SC_REPORT_WARNING("sc_cast", msg.c_str());
                    bitstr = bitstr.substr(sz - WIDTH);
                }
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
        // Uses set_bit() to avoid SystemC string constructor quirks with X/Z.
        template <typename LV>
        std::enable_if_t<is_sc_logiclike<LV>::value, LV>
        from_bitstr(const std::string& bitstr)
        {
            if constexpr (is_sc_logic<LV>::value)
            {
                char c = bitstr.empty() ? '0' : bitstr.back();
                if (c == '1')              return LV(SC_LOGIC_1);
                if (c == '0')              return LV(SC_LOGIC_0);
                if (c == 'Z' || c == 'z')  return LV(SC_LOGIC_Z);
                return LV(SC_LOGIC_X);
            }
            else
            {
                // Construct bit-by-bit to avoid sc_lv string constructor issues.
                // bitstr is MSB-first (leftmost = highest bit index).
                LV result;
                int sz = static_cast<int>(bitstr.size());
                constexpr int WIDTH = sc_lv_traits<LV>::width;
                for (int i = 0; i < WIDTH; ++i) {
                    int bit_idx = WIDTH - 1 - i;
                    char c = (i < sz) ? bitstr[i] : '0';
                    sc_logic_value_t val;
                    if      (c == '1')              val = Log_1;
                    else if (c == '0')              val = Log_0;
                    else if (c == 'X' || c == 'x')  val = Log_X;
                    else if (c == 'Z' || c == 'z')  val = Log_Z;
                    else                            val = Log_0;
                    result.set_bit(bit_idx, val);
                }
                return result;
            }
        }

    } // namespace detail

    // ========================================================================
    // GENERIC string_cast for non-SystemC types
    // ========================================================================
    template <typename T>
    std::enable_if_t<!is_sc_logiclike<T>::value && !is_sc_intlike<T>::value, T>
    string_cast(std::string_view str_view,
                std::string_view /*mode_view*/ = "data",
                int base = 2)
    {
        std::string str(str_view);

        if constexpr (std::is_integral<T>::value)
        {
            if (str.empty()) {
                SC_REPORT_WARNING("string_cast", "Empty string. Returning 0.");
                return static_cast<T>(0);
            }
            if (str.find_first_not_of("xX") == std::string::npos)
                return static_cast<T>(0);

            int  size = static_cast<int>(str.size());
            char c0   = str[0];
            char c1   = (size > 1) ? str[1] : 0;
            uint64_t result = 0;
            bool ok = false;

            if (((str.find_first_not_of("01") == std::string::npos) && (base == 2))
                || (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B')
                    && (str.substr(2).find_first_not_of("01") == std::string::npos)))
                ok = detail::parse_u64(str, 2, result);
            else if (size > 2 && c0 == '0' && (c1 == 'X' || c1 == 'x'))
                ok = detail::parse_u64(str, 16, result);
            else if (size > 1 && c0 == '0')
                ok = detail::parse_u64(str, 8, result);
            else
                ok = detail::parse_u64(str, 10, result);

            if (!ok) {
                SC_REPORT_WARNING("string_cast", "Integer parse error. Returning 0.");
                return static_cast<T>(0);
            }
            return static_cast<T>(result);
        }
        else if constexpr (std::is_floating_point<T>::value)
        {
            try {
                if constexpr (std::is_same<T, double>::value)
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

} // namespace cc_vrwrapper

#endif // CC_VRWRAPPER_UTILS_HPP