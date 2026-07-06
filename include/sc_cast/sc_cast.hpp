
#include <systemc>         // SystemC core and data types (sc_core, sc_dt, sc_lv, SC_REPORT_*)
#include <type_traits>     // type traits: enable_if, is_integral, is_same, etc.
#include <string>          // std::string, std::string_view
#include <sstream>         // std::stringstream
#include <limits>          // numeric limits (e.g. int max/min)
#include <bitset>          // std::bitset for binary conversions
#include <regex>           // regex for string format checks
#include <algorithm>       // remove_if, any_of
#include <cctype>          // character checks: isspace, isdigit
#include <cmath>           // ceil
#include <cstring>         // memcpy
#include <typeinfo>        // typeid
#include <cxxabi.h>        // demangle type names (GCC/Clang only)
#include <cstdlib>         // free()

template <typename T>
std::string type_name(const T& var)
{
    int status;
    char* demangled = abi::__cxa_demangle(typeid(var).name(), 0, 0, &status);
    std::string result = (status == 0 && demangled) ? demangled : typeid(var).name();
    free(demangled);
    return result;
}

namespace sc_cast
{

    using namespace sc_core;
    using namespace sc_dt;
    using namespace std;

    template <typename T>
    struct is_sc_lv : std::false_type
    {
    };
    // Special case: if T is sc_lv<W> for any int W, then it's TRUE
    template <int W>
    struct is_sc_lv<sc_lv<W>> : std::true_type
    {
    };
    
    template <typename T>
    struct sc_lv_traits; // primary template left undefined
    
    // Traits to extract width from sc_lv<W>
    template <int W>
    struct sc_lv_traits<sc_dt::sc_lv<W>>
    {
        static constexpr int width = W;
    };

    // Default: everything is NOT an sc_lv

    // --------------------LV TO LV --------------------
    // Identity: LV → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(LV& lv) { return lv; }

    // -------------------- TO LV --------------------

    // bool → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(bool value);

    // integral (not bool) → LV
    template <typename LV, typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value&& sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(T value, std::string_view mode_view = "data", bool MSB = false);

    // float → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(float value);

    // double → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(double value);

    // string → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(std::string_view input_str, std::string_view mode_view = "data", int base = 2);

    // -------------------- FROM LV --------------------

    // LV → bool
    template <typename T = bool, typename LV>
    typename std::enable_if<std::is_same<T, bool>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv);

    // LV → integral (not bool)
    template <typename T, typename LV>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv);

    // LV → float
    template <typename T = float, typename LV>
    typename std::enable_if<std::is_same<T, float>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv);

    // LV → double
    template <typename T = double, typename LV>
    typename std::enable_if<std::is_same<T, double>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv);

    // -------------------- Generic String to Type --------------------

    // string → T (bool/int/float/double)
    template <typename T>
    T string_cast(std::string_view str_view, std::string_view mode_view = "data", int base = 2);

    // ----------------------------
    // TO sc_lv<WIDTH> CONVERSIONS
    // ----------------------------

    // bool type
    // bool → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(bool value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;

        std::string unknown_(WIDTH, 'X');
        const char* unknown = unknown_.c_str();

        if (WIDTH <= 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return LV(unknown);
        }
        LV lv(0);
        lv[0] = value ? SC_LOGIC_1 : SC_LOGIC_0;
        return lv;
    }

    // integral (not bool) → LV
    template <typename LV, typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value&& sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(T value, std::string_view mode_view, bool MSB)
    {
        std::string mode(mode_view); // make a copy to modify

        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        std::string unknown_(WIDTH, 'X');
        const char* unknown = unknown_.c_str();

        if (WIDTH <= 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Width must be positive");
            return LV(unknown);
        }

        std::string bitstr;
        int64_t Value64 = static_cast<int64_t>(value);
        uint64_t uValue64 = static_cast<uint64_t>(value);

        bool is_signed = std::numeric_limits<T>::is_signed;
        int inputTypeBitSize = sizeof(T) * 8;
        uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        int64_t maxRange = (WIDTH >= 64) ? INT64_MAX : (1LL << (WIDTH - 1)) - 1;
        int64_t minRange = (WIDTH >= 64) ? INT64_MIN : -(1LL << (WIDTH - 1));

        if (!MSB)
        {
            if (is_signed)
            {
                if (mode == "address" && value < 0)
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
        // else if (MSB)
        // {
        //     bitstr = is_signed
        //                  ? std::bitset<64>(Value64).to_string()
        //                  : std::bitset<64>(uValue64).to_string();

        //     bitstr = bitstr.substr(0, WIDTH);
        //     return LV(bitstr.c_str());
        // }
        else
        {
            SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"lsb\" or \"msb\".");
            return LV(unknown);
        }
    }

    // Overload for float
    // float → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(float value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (WIDTH < 32)
            SC_REPORT_FATAL("sc_Lv_cast", "Width must be at least 32 bits for float to sc_lv conversion");

        union
        {
            float floatRepresentation;
            uint32_t binaryRepresentation;
        } conv;
        conv.floatRepresentation = value;
        return LV(conv.binaryRepresentation);
    }

    // Overload for double
    // float → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(double value)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (WIDTH < 64)
            SC_REPORT_FATAL("sc_Lv_cast", "Width must be 32 bits for double to sc_lv");

        union
        {
            double doubleRepresentation;
            uint64_t binaryRepresentation;
        } conv;
        conv.doubleRepresentation = value;
        return LV(conv.binaryRepresentation);
    }

    // Overload for string
    // string → LV
    template <typename LV>
    typename std::enable_if<sc_cast::is_sc_lv<LV>::value, LV>::type
        sc_lv_cast(std::string_view input_str, std::string_view mode_view, int base)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        std::string mode(mode_view); // make a copy to modify
        std::string str(input_str);  // make a copy to modify

        if (WIDTH <= 0)
            SC_REPORT_FATAL("sc_Lv_cast", "Width must be positive");

        std::string unknown_(WIDTH, 'X');
        const char* unknown = unknown_.c_str();

        uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        int64_t maxRange = (WIDTH >= 64) ? INT64_MAX : (1LL << (WIDTH - 1)) - 1;
        int64_t minRange = (WIDTH >= 64) ? INT64_MIN : -((1LL << (WIDTH - 1)));

        // Remove whitespace and warn
        if (std::any_of(str.begin(), str.end(), ::isspace))
        {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
        }

        size_t size = str.size();
        if (size == 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Empty string input. X signal issue.");
            return LV(unknown);
        }

        char c0 = str[0];
        char c1 = (size > 1) ? str[1] : 0;
        std::string substr2 = (size > 2) ? str.substr(2) : "";
        std::string substr1 = (size > 1) ? str.substr(1) : "";
        int substr2_size = static_cast<int>(substr2.size());
        int substr1_size = static_cast<int>(substr1.size());

        // Binary prefix
        if (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B'))
        {
            if (!std::regex_match(substr2, std::regex("^[01XZxz]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in binary string. X signal issue.");
                return LV(unknown);
            }
            if (substr2_size > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", "Binary string longer than WIDTH — truncating LSBs.");

            else if (substr2_size < WIDTH)
            {
                if (mode == "data")
                    substr2 = std::string(WIDTH - substr2_size, substr2[0]) + substr2;
                else if (mode == "address")
                    substr2 = std::string(WIDTH - substr2_size, '0') + substr2;
            }

            return LV(substr2.c_str());
        }
        // Another version of binary
        if ((str.find_first_not_of("01xXzZ") == string::npos) && (base == 2))
        {
            if (size > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", "Binary string longer than WIDTH — truncating LSBs.");

            else if (size < WIDTH)
            {
                if (mode == "data")
                    str = std::string(WIDTH - size, str[0]) + str;
                else if (mode == "address")
                    str = std::string(WIDTH - size, '0') + str;
            }

            return LV(str.c_str());
        }

        // Hex prefix
        if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X'))
        {
            std::string hex_str = substr2; // Use the rest of the string after '0'

            if (!std::regex_match(hex_str, std::regex("^[0-9a-fA-FxzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in hex string. X signal issue.");
                return LV(unknown);
            }

            int bitsNum = static_cast<int>(hex_str.size()) * 4;

            if (bitsNum > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", "Hex string longer than WIDTH — truncating LSBs.");

            else if (bitsNum < WIDTH)
            {
                int padDigits = ceil((WIDTH - bitsNum) / 4.0);
                if (mode == "data")
                    hex_str = std::string(padDigits, hex_str[0]) + hex_str;
                else if (mode == "address")
                    hex_str = std::string(padDigits, '0') + hex_str;
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
                    int val = std::isdigit(ch) ? ch - '0' : std::toupper(ch) - 'A' + 10;
                    bitstr += std::bitset<4>(val).to_string();
                }
            }
            return LV(bitstr.c_str());
        }

        // Octal prefix
        if (size >= 2 && c0 == '0' && std::isdigit(c1))
        {
            std::string oct_str = substr1; // Use the rest of the string after '0'

            if (!std::regex_match(oct_str, std::regex("^[0-7xzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in octal string. X signal issue.");
                return LV(unknown);
            }

            int bitsNum = static_cast<int>(oct_str.size()) * 3;

            if (bitsNum > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", "Octal string longer than WIDTH — truncating LSBs.");

            else if (bitsNum < WIDTH)
            {
                int padDigits = ceil((WIDTH - bitsNum) / 3.0);
                if (mode == "data")
                    oct_str = std::string(padDigits, oct_str[0]) + oct_str;
                else if (mode == "address")
                    oct_str = std::string(padDigits, '0') + oct_str;
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
            return LV(bitstr.c_str());
        }

        // Decimal (signed or unsigned)
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$")))
        {
            std::string dec_str;
            if (mode == "address")
            {
                if (str[0] == '+' || str[0] == '-')
                    SC_REPORT_ERROR("sc_lv_cast", "Address mode does not support signed decimal strings.");
                else
                    dec_str = str;
                uint64_t uValue = stoull(dec_str);
                if (uValue > umaxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value overflows max for WIDTH");
                return sc_lv_cast<LV>(uValue);
            }
            else if (mode == "data")
            {
                char sign = str[0];
                dec_str = (sign == '+' || sign == '-') ? substr1 : str;
                uint64_t uValue = stoull(dec_str);
                if (uValue > maxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value overflows max for WIDTH");
                int64_t Value = (sign == '-') ? -int64_t(uValue) : int64_t(uValue);
                return sc_lv_cast<LV>(Value);
            }
        }

        SC_REPORT_ERROR("sc_lv_cast", "Invalid string format for sc_lv conversion.");
        return LV(unknown);
    }

    // -------------------------------
    // FROM sc_lv<WIDTH> CONVERSIONS
    // -------------------------------

    // LV → bool
    template <typename T = bool, typename LV>
    typename std::enable_if<std::is_same<T, bool>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv)
    {
        return lv.is_01() ? (lv.read() != '0') : false;
    }

    // FROM sc_lv<WIDTH> to integral types
    // LV → integral (not bool)
    template <typename T, typename LV>
    typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (std::is_integral<T>::value)
        {
            if (!lv.is_01())
            {
                SC_REPORT_WARNING("sc_lv_cast", "Input contains unknown (X/Z) bits. Returning 0.");
                return 0;
            }

            int64_t MaxValue = static_cast<int64_t>(std::numeric_limits<T>::max());
            int64_t MinValue = static_cast<int64_t>(std::numeric_limits<T>::min());

            uint64_t uMaxValue = static_cast<uint64_t>(std::numeric_limits<T>::max());
            uint64_t uMinValue = 0;

            if (std::is_signed<T>::value)
            {
                int64_t val = lv.to_int64();
                if (val < MinValue || val > MaxValue)
                    SC_REPORT_WARNING("sc_lv_cast", "Signed overflow in conversion from sc_lv to target type");
                return static_cast<T>(val);
            }
            else
            {
                uint64_t val = lv.to_uint64();
                if (val > uMaxValue)
                    SC_REPORT_WARNING("sc_lv_cast", "Unsigned overflow in conversion from sc_lv to target type");
                return static_cast<T>(val);
            }
        }
    }

    // FROM sc_lv<32> to float
    // LV → float
    template <typename T = float, typename LV>
    typename std::enable_if<std::is_same<T, float>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (WIDTH != 32)
            SC_REPORT_FATAL("sc_Lv_cast", "Float must be 32 bits for sc_lv->float");

        uint32_t raw = lv.to_uint();
        float f;
        std::memcpy(&f, &raw, sizeof(f));
        return f;
    }

    // FROM sc_lv<64> to double
    // LV → double
    template <typename T = double, typename LV>
    typename std::enable_if<std::is_same<T, double>::value&& sc_cast::is_sc_lv<LV>::value, T>::type
        sc_lv_cast(const LV& lv)
    {
        constexpr int WIDTH = sc_lv_traits<LV>::width;
        // auto-extracted from type
        if (WIDTH != 64)
            SC_REPORT_FATAL("sc_Lv_cast", "Float must be 64 bits for sc_lv->double");
        uint64_t raw = lv.to_uint64();
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    // -------------------------------
    // WORK WITH FILE STRINGS
    // -------------------------------

    template <typename T>
    T string_cast(std::string_view str_view, std::string_view mode_view, int base)
    {
        std::string str(str_view);
        std::string mode(mode_view);

        if constexpr (sc_cast::is_sc_lv<T>::value)
        {
            using LV = T;
            constexpr int WIDTH = sc_lv_traits<LV>::width;
            return sc_lv_cast<LV>(str, mode, base);
        }
        else if constexpr (std::is_integral<T>::value)
        {
            if ((str.find_first_not_of("xX") == std::string::npos))
                return static_cast<T>(0);

            int size = str.size();
            char c0 = str[0];
            char c1 = (size > 1) ? str[1] : 0;
            if (((str.find_first_not_of("01") == std::string::npos) && (base == 2)) || (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B') && (str.substr(2).find_first_not_of("01") == std::string::npos)))
                return static_cast<T>(stoull(str, nullptr, 2));
            else if (size > 2 && c0 == '0' && (c1 == 'X' || c1 == 'x'))
                return static_cast<T>(stoull(str, nullptr, 16));
            else if (size > 1 && c0 == '0')
                return static_cast<T>(stoull(str, nullptr, 8));

            return static_cast<T>(std::stoull(str));
        }
        else if constexpr (std::is_floating_point<T>::value)
        {
            if constexpr (std::is_same<T, double>::value)
                return static_cast<T>(std::stod(str));
            else if constexpr (std::is_same<T, float>::value)
                return static_cast<T>(std::stof(str));
        }
        else
        {
            std::stringstream ss(str);
            T val;
            ss >> val;
            return val;
        }

        return T{};
    }

}
