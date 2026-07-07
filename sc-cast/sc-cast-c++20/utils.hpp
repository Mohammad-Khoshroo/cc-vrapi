#ifndef SC_CAST_UTILS_HPP
#define SC_CAST_UTILS_HPP

// ============================================================================
// sc_cast :: utils.hpp  (C++20)
// Common utilities: concepts, type traits, helpers, generic string_cast
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
#include <ranges>          // std::ranges::any_of                       [C++20]
#include <cctype>
#include <cmath>
#include <cstring>
#include <typeinfo>
#include <stdexcept>
#include <cstdint>
#include <concepts>        // std::integral, std::floating_point, ...    [C++20]
#include <format>          // std::format                                [C++20]
#include <optional>        // std::optional                              [C++20]

#if defined(__GNUC__) || defined(__clang__)
#  include <cxxabi.h>
#  include <cstdlib>
#  define SC_CAST_HAS_CXXABI 1
#endif

// ----------------------------------------------------------------------------
// type_name — compiler-safe demangling
// ----------------------------------------------------------------------------
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
    return typeid(var).name();
#endif
}

namespace sc_cast
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

    // Combined traits
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

    // Width extraction
    template <typename T> struct sc_lv_traits;

    template <int W> struct sc_lv_traits<sc_lv<W>>      { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_bv<W>>      { static constexpr int width = W; };
    template <>      struct sc_lv_traits<sc_logic>       { static constexpr int width = 1; };
    template <int W> struct sc_lv_traits<sc_int<W>>     { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_uint<W>>    { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_bigint<W>>  { static constexpr int width = W; };
    template <int W> struct sc_lv_traits<sc_biguint<W>> { static constexpr int width = W; };

    // ========================================================================
    // CONCEPTS  [C++20]
    // ========================================================================

    template <typename T>
    concept ScLogicLike = is_sc_lv<T>::value || is_sc_bv<T>::value || is_sc_logic<T>::value;

    template <typename T>
    concept ScVector = is_sc_lv<T>::value || is_sc_bv<T>::value;   // excludes sc_logic

    template <typename T> concept ScLv     = is_sc_lv<T>::value;
    template <typename T> concept ScBv     = is_sc_bv<T>::value;
    template <typename T> concept ScLogic  = is_sc_logic<T>::value;
    template <typename T> concept ScInt    = is_sc_int<T>::value;
    template <typename T> concept ScUint   = is_sc_uint<T>::value;
    template <typename T> concept ScBigInt = is_sc_bigint<T>::value;
    template <typename T> concept ScBigUint= is_sc_biguint<T>::value;

    template <typename T>
    concept ScIntLike = is_sc_int<T>::value || is_sc_uint<T>::value
                      || is_sc_bigint<T>::value || is_sc_biguint<T>::value;

    template <typename T>
    concept ScSignedInt = is_sc_int<T>::value || is_sc_bigint<T>::value;

    template <typename T>
    concept ScUnsignedInt = is_sc_uint<T>::value || is_sc_biguint<T>::value;

    // ========================================================================
    // DETAIL HELPERS
    // ========================================================================
    namespace detail
    {
        // Safe character checks (handle negative char values)
        inline bool safe_isspace(char c) {
            return std::isspace(static_cast<unsigned char>(c)) != 0;
        }
        inline bool safe_isdigit(char c) {
            return std::isdigit(static_cast<unsigned char>(c)) != 0;
        }

        // Parse unsigned integer string — returns std::optional  [C++20]
        inline std::optional<uint64_t> parse_u64(const std::string& s, int base)
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

        // Validate mode ("data" or "address") — returns (ok, is_address, is_data)
        inline bool check_mode(std::string_view mode_view, bool& is_address, bool& is_data)
        {
            is_address = (mode_view == "address");
            is_data    = (mode_view == "data");
            return is_address || is_data;
        }

        // Build an "unknown" value for any supported type
        template <typename T>
        T make_unknown()
        {
            if constexpr (ScBv<T>)
                return T(std::string(sc_lv_traits<T>::width, '0').c_str());
            else if constexpr (ScLv<T>)
                return T(std::string(sc_lv_traits<T>::width, 'X').c_str());
            else if constexpr (ScLogic<T>)
                return T(SC_LOGIC_X);
            else // sc_int family: no "unknown", return 0
                return T(0);
        }

        // Adjust a bit-string to WIDTH (truncate keeping LSBs, or pad).
        // Only for lv/bv/logic.
        template <ScLogicLike LV>
        std::string adjust_bitstr(std::string bitstr, int WIDTH, bool is_data,
                                  std::string_view type_label)
        {
            int sz = static_cast<int>(bitstr.size());
            if (sz > WIDTH)
            {
                if constexpr (!ScLogic<LV>)
                {
                    // C++20: std::format for type-safe message
                    std::string msg = std::format("{} string longer than WIDTH — keeping LSBs.", type_label);
                    SC_REPORT_WARNING("sc_lv_cast", msg.c_str());
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
    // GENERIC string_cast for non-SystemC types
    // SystemC-type specializations live in their respective headers.
    // ========================================================================
    template <typename T>
        requires (!ScLogicLike<T> && !ScIntLike<T>)
    T string_cast(std::string_view str_view,
                  std::string_view /*mode_view*/ = "data",
                  int base = 2)
    {
        std::string str(str_view);

        if constexpr (std::integral<T>)
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

            std::optional<uint64_t> result;

            // Binary: "0b..." or pure 0/1 with base==2
            if (((str.find_first_not_of("01") == std::string::npos) && (base == 2))
                || (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B')
                    && (str.substr(2).find_first_not_of("01") == std::string::npos)))
            {
                result = detail::parse_u64(str, 2);
            }
            else if (size > 2 && c0 == '0' && (c1 == 'X' || c1 == 'x'))
            {
                result = detail::parse_u64(str, 16);
            }
            else if (size > 1 && c0 == '0')
            {
                result = detail::parse_u64(str, 8);
            }
            else
            {
                result = detail::parse_u64(str, 10);
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

#endif // SC_CAST_UTILS_HPP