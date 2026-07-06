#pragma once
#include "header.hpp"

namespace sc_cast
{

    using namespace sc_core;
    using namespace sc_dt;
    using namespace std;

    // Default: everything is NOT an sc_lv
    template <typename T>
    struct is_sc_lv : std::false_type
    {
    };
    // Special case: if T is sc_lv<W> for any int W, then it's TRUE
    template <int W>
    struct is_sc_lv<sc_lv<W>> : std::true_type
    {
    };

    // Forward declaration
    template <typename T, int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const T &value, const std::string &mode);
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const float &value);
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const double &value);
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const std::string &str);

    // ----------------------------
    // TO sc_lv<WIDTH> CONVERSIONS
    // ----------------------------

    template <typename T, int WIDTH>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, bool>::value, sc_lv<WIDTH>>::type
    sc_lv_cast(const T &value, const std::string &mode = "lsb")
    {
        std::string unknown_(WIDTH, 'X');
        const char *unknown = unknown_.c_str();

        static_assert(WIDTH > 0, "Width must be positive");
        std::string bitstr;

        constexpr uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        constexpr int64_t maxRange = (WIDTH >= 64) ? INT64_MAX : (1LL << (WIDTH - 1)) - 1;
        constexpr int64_t minRange = (WIDTH >= 64) ? INT64_MIN : -((1LL << (WIDTH - 1)));

        if constexpr (std::is_same<T, bool>::value)
        {
            // bool to sc_lv<WIDTH>: set LSB to 1 or 0, rest zeros
            sc_lv<WIDTH> lv;
            lv = sc_lv<WIDTH>(0);                     // zero all bits
            lv[0] = !value ? SC_LOGIC_0 : SC_LOGIC_1; // set LSB — rest bits zero
            return lv;
        }
        else if constexpr (std::is_integral<T>::value)
        {
            // ---- Overflow Check ----

            int64_t Value64 = static_cast<int64_t>(value);
            uint64_t uValue64 = static_cast<uint64_t>(value);

            constexpr int inputTypeBitSize = sizeof(T) * 8;
            if (mode == "lsb")
            {
                if (std::is_signed<T>::value)
                {
                    if (WIDTH < inputTypeBitSize && ((value > maxRange) || (value < minRange)))
                        SC_REPORT_WARNING("sc_lv_cast", ("Input type is larger than 64 bits, may cause overflow" + " **** " + "input: " value + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                    return sc_lv<WIDTH>(Value64);
                }
                else
                {
                    if (WIDTH < inputTypeBitSize && value > umaxRange)
                        SC_REPORT_WARNING("sc_lv_cast", ("Input type is larger than 64 bits, may cause unsigned overflow" + " **** " + "input: " value + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                    return sc_lv<WIDTH>(uValue64);
                }
            }
            // else if (mode == "msb")
            // {
            //     if constexpr (std::is_signed<T>::value)
            //         bitstr = std::bitset<64>(Value64).to_string();
            //     else
            //         bitstr = std::bitset<64>(uValue64).to_string();

            //     // Take MSB WIDTH bits
            //     bitstr = bitstr.substr(0, WIDTH);
            //     return sc_lv<WIDTH>(bitstr);
            // }
            else
            {
                SC_REPORT_ERROR("sc_lv_cast", ("Invalid mode. Use \"lsb\" or \"msb\"." + " **** "  + "input: " value + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return sc_lv<WIDTH>(unknown); // Return empty just in case
            }
        }
    }

    // Overload for float
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const float &value)
    {
        static_assert(WIDTH >= 32, "Width must be 32 bits for float to sc_lv");
        union
        {
            float floatRepresentation;
            uint32_t binaryRepresentation;
        } conv;
        conv.floatRepresentation = value;
        return sc_lv<WIDTH>(conv.binaryRepresentation);
    }

    // Overload for double
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const double &value)
    {
        static_assert(WIDTH >= 64, "Width must be 32 bits for double to sc_lv");
        union
        {
            double doubleRepresentation;
            uint64_t binaryRepresentation;
        } conv;
        conv.doubleRepresentation = value;
        return sc_lv<WIDTH>(conv.binaryRepresentation);
    }

    // Overload for string
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const std::string &input_str, const std::string &mode = "data", const int &base = 2)
    {
        static_assert(WIDTH > 0, "Width must be positive");

        std::string unknown_(WIDTH, 'X');
        const char *unknown = unknown_.c_str();

        std::string str = input_str; // make a copy to modify
        constexpr uint64_t umaxRange = (WIDTH >= 64) ? UINT64_MAX : (1ULL << WIDTH) - 1;
        constexpr int64_t maxRange = (WIDTH >= 64) ? INT64_MAX : (1LL << (WIDTH - 1)) - 1;
        constexpr int64_t minRange = (WIDTH >= 64) ? INT64_MIN : -((1LL << (WIDTH - 1)));

        // Remove whitespace and warn
        if (std::any_of(str.begin(), str.end(), ::isspace))
        {
            SC_REPORT_WARNING("sc_lv_cast", ("Whitespace found in input string — it was removed." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
            str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
        }

        size_t size = str.size();
        if (size == 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", ("Empty string input. X signal issue." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
            return sc_lv<WIDTH>(unknown);
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
                SC_REPORT_ERROR("sc_lv_cast", ("Invalid characters in binary string. X signal issue." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return sc_lv<WIDTH>(unknown);
            }
            if (substr2_size > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", ("Binary string longer than WIDTH — truncating LSBs." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());

            else if (substr2_size < WIDTH)
            {
                if (mode == "data")
                    substr2 = std::string(WIDTH - substr2_size, substr2[0]) + substr2;
                else if (mode == "address")
                    substr2 = std::string(WIDTH - substr2_size, '0') + substr2;
            }

            return sc_lv<WIDTH>(substr2.c_str());
        }
        // Another version of binary
        if ((str.find_first_not_of("01") == string::npos) && (base == 2))
        {
            if (size > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", ("Binary string longer than WIDTH — truncating LSBs." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());

            else if (size < WIDTH)
            {
                if (mode == "data")
                    str = std::string(WIDTH - size, str[0]) + str;
                else if (mode == "address")
                    str = std::string(WIDTH - size, '0') + str;
            }

            return sc_lv<WIDTH>(str.c_str());
        }

        // Hex prefix
        if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X'))
        {
            std::string hex_str = substr2; // Use the rest of the string after '0'

            if (!std::regex_match(hex_str, std::regex("^[0-9a-fA-FxzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", ("Invalid characters in hex string. X signal issue." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return sc_lv<WIDTH>(unknown);
            }

            int bitsNum = static_cast<int>(hex_str.size()) * 4;

            if (bitsNum > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", ("Hex string longer than WIDTH — truncating LSBs." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());

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
            return sc_lv<WIDTH>(bitstr.c_str());
        }

        // Octal prefix
        if (size >= 2 && c0 == '0' && std::isdigit(c1))
        {
            std::string oct_str = substr1; // Use the rest of the string after '0'

            if (!std::regex_match(oct_str, std::regex("^[0-7xzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", ("Invalid characters in octal string. X signal issue." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return sc_lv<WIDTH>(unknown);
            }

            int bitsNum = static_cast<int>(oct_str.size()) * 3;

            if (bitsNum > WIDTH)
                SC_REPORT_WARNING("sc_lv_cast", ("Octal string longer than WIDTH — truncating LSBs." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());

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
            return sc_lv<WIDTH>(bitstr.c_str());
        }

        // Decimal (signed or unsigned)
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$")))
        {
            std::string dec_str;
            if (mode == "address")
            {
                if (str[0] == '+' || str[0] == '-')
                    SC_REPORT_ERROR("sc_lv_cast", ("Address mode does not support signed decimal strings." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                else
                    dec_str = str;
                uint64_t uValue = stoull(dec_str);
                if (uValue > umaxRange)
                    SC_REPORT_WARNING("sc_lv_cast",( "Decimal value overflows max for WIDTH" + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return sc_lv_cast<uint64_t, WIDTH>(uValue);
            }
            else if (mode == "data")
            {
                char sign = str[0];
                dec_str = (sign == '+' || sign == '-') ? substr1 : str;
                uint64_t uValue = stoull(dec_str);
                if (uValue > maxRange)
                    SC_REPORT_WARNING("sc_lv_cast", ("Decimal value overflows max for WIDTH" + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                int64_t Value = (sign == '-') ? -int64_t(uValue) : int64_t(uValue);
                return sc_lv_cast<int64_t, WIDTH>(Value);
            }
        }

        SC_REPORT_FATAL("sc_lv_cast", ("Invalid string format for sc_lv conversion." + " **** "  + "input: " input_str + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
        return sc_lv<WIDTH>(unknown);
    }

    // -------------------------------
    // FROM sc_lv<WIDTH> CONVERSIONS
    // -------------------------------

    // FROM sc_lv<WIDTH> to integral and bool types
    template <typename T, int WIDTH>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, bool>::value, T>::type
    sc_lv_cast(const sc_lv<WIDTH> &lv)
    {
        if constexpr (std::is_same<T, bool>::value)
            return lv.is_01() ? (lv.read() != '0') : false;

        else if constexpr (std::is_integral<T>::value)
        {
            if (!lv.is_01())
            {
                SC_REPORT_WARNING("sc_lv_cast", ("Input contains unknown (X/Z) bits. Returning 0." + " **** "  + "input: " lv.read() + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return 0;
            }

            constexpr int64_t MaxValue = static_cast<int64_t>(std::numeric_limits<T>::max());
            constexpr int64_t MinValue = static_cast<int64_t>(std::numeric_limits<T>::min());

            constexpr uint64_t uMaxValue = static_cast<uint64_t>(std::numeric_limits<T>::max());
            constexpr uint64_t uMinValue = 0;

            if constexpr (std::is_signed<T>::value)
            {
                int64_t val = lv.to_int64();
                if (val < MinValue || val > MaxValue)
                    SC_REPORT_WARNING("sc_lv_cast", ("Signed overflow in conversion from sc_lv to target type" + " **** "  + "input: " lv.read() + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return static_cast<T>(val);
            }
            else
            {
                uint64_t val = lv.to_uint64();
                if (val > uMaxValue)
                    SC_REPORT_WARNING("sc_lv_cast", ("Unsigned overflow in conversion from sc_lv to target type" + " **** "  + "input: " lv.read() + ", time: " + sc_core::sc_time_stamp().to_string()).c_str());
                return static_cast<T>(val);
            }
        }
    }

    // FROM sc_lv<32> to float
    template <typename T, int WIDTH>
    typename std::enable_if<std::is_same<T, float>::value, T>::type
    sc_lv_cast(const sc_lv<WIDTH> &lv)
    {
        static_assert(WIDTH == 32, "Float must be 32 bits for sc_lv->float");

        uint32_t raw = lv.to_uint();
        float f;
        std::memcpy(&f, &raw, sizeof(f));
        return f;
    }

    // FROM sc_lv<64> to double
    template <typename T, int WIDTH>
    typename std::enable_if<std::is_same<T, double>::value, T>::type
    sc_lv_cast(const sc_lv<WIDTH> &lv)
    {
        static_assert(WIDTH == 64, "Double must be 64 bits for sc_lv->double");
        uint64_t raw = lv.to_uint64();
        double d;
        std::memcpy(&d, &raw, sizeof(d));
        return d;
    }

    // -------------------------------
    // WORK WITH FILE STRINGS
    // -------------------------------

    template <typename T>
    T string_cast(string &str, int base = 2)
    {
        // Case 1: sc_lv<N>
        if constexpr (sc_cast::is_sc_lv<T>::value) // Custom trait for sc_lv<> detection
        {
            using LV = T;
            constexpr int WIDTH = LV::length();
            return sc_lv_cast<WIDTH>(str, base);
        }
        // Case 2: Integral and bool types
        else if constexpr (std::is_integral<T>::value)
        {
            int size = str.size();
            char c0 = str[0];
            char c1 = (size > 1) ? str[1] : 0;

            if (((str.find_first_not_of("01") == string::npos) && (base == 2)) || (size >= 3 && c0 == '0' && (c1 == 'b' || c1 == 'B') && (str.substr(2).find_first_not_of("01") == string::npos)))
                return static_cast<T>(stoull(str, nullptr, 2));
            else if (size > 2 && c0 == '0' && (c1 == 'X' | c1 == 'x'))
                return static_cast<T>(stoull(str, nullptr, 16));
            else if (size > 1 && c0 == '0')
                return static_cast<T>(stoull(str, nullptr, 8));

            return static_cast<T>(std::stoull(str));
        }

        // Case 3: Floating-point types
        else if constexpr (std::is_floating_point<T>::value)
        {
            if (std::is_same<T, double>::value)
                return static_cast<T>(std::stod(str));
            else if (std::is_same<T, float>::value)
                return static_cast<T>(std::stof(str));
        }
        // Case 4: Fallback for other types
        else
        {
            stringstream ss(str);
            T val;
            ss >> val;
            return val;
        }
    }

}
