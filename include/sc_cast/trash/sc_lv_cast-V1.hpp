#pragma once

// SystemC core
#include <systemc.h> // SystemC main header

// Standard C++ utilities
#include <iostream>    // std::cout, std::cerr
#include <fstream>     // file input/output streams
#include <sstream>     // stringstream
#include <string>      // std::string
#include <vector>      // std::vector
#include <array>       // std::array if needed
#include <map>         // std::map if needed
#include <algorithm>   // std::sort, std::find
#include <cassert>     // assert()
#include <type_traits> // enable_if, is_integral, etc.
#include <limits>      // numeric limits
#include <cstdint>     // fixed width integer types
#include <cstring>     // memcpy, memmove
#include <bitset>      // std::bitset for bit manipulation
#include <cmath>       // math functions

// CSV parsing library (if you use one)
#include <csv.hpp> // or your chosen CSV lib header

// Optional for timing or concurrency (usually host-side)
#include <chrono>
#include <thread>

#include <regex>

namespace sc_cast
{
    using namespace sc_core;
    using namespace sc_dt;
    using namespace std;

    // Forward declaration
    template <typename T, int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const T &value, const std::string &mode);
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const float &value);
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const double &value);
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const std::string &str);

    template <typename T, int WIDTH>
    T sc_lv_cast(const sc_lv<WIDTH> &lv);

    // ----------------------------
    // TO sc_lv<WIDTH> CONVERSIONS
    // ----------------------------

    template <typename T, int WIDTH>
    typename std::enable_if<std::is_integral<T>::value || std::is_same<T, bool>::value, sc_lv<WIDTH>>::type
    sc_lv_cast(const T &value, const std::string &mode = "lsb")
    {
        static_assert(WIDTH > 0, "Width must be positive");
        std::string bitstr;
        constexpr uint64_t umaxRange = 1 << (WIDTH);   // Maximum value for WIDTH bits unsigned
        constexpr int64_t maxRange = 1 << (WIDTH - 1); // Maximum value for WIDTH bits signed

        if constexpr (std::is_same<T, bool>::value)
        {
            // bool to sc_lv<WIDTH>: set LSB to 1 or 0, rest zeros
            sc_lv<WIDTH> lv;
            lv = sc_lv<WIDTH>(0);                    // zero all bits
            lv[0] = value ? SC_LOGIC_1 : SC_LOGIC_0; // set LSB — rest bits zero
            return lv;
        }
        else if constexpr (std::is_integral<T>::value)
        {

            // ---- Overflow Check ----

            constexpr int inputTypeBitSize = sizeof(T) * 8;
            if (std::is_signed<T>::value)
            {
                int64_t Value64 = static_cast<int64_t>(value);
                if (WIDTH < inputTypeBitSize && ((value > maxRange) || (value < -(maxRange + 1))))
                    SC_REPORT_WARNING("sc_lv_cast", "Input type is larger than 64 bits, may cause overflow");
            }
            else
            {
                uint64_t uValue64 = static_cast<uint64_t>(value);
                if (WIDTH < inputTypeBitSize && value > umaxRange)
                    SC_REPORT_WARNING("sc_lv_cast", "Input type is larger than 64 bits, may cause unsigned overflow");
            }

            if (mode == "lsb")
                return sc_lv<WIDTH>(value);

            else if (mode == "msb")
            {
                if constexpr (std::is_signed<T>::value)
                {
                    int64_t val64 = static_cast<int64_t>(value);
                    bitstr = std::bitset<64>(val64).to_string();
                }
                else
                {
                    uint64_t val64 = static_cast<uint64_t>(value);
                    bitstr = std::bitset<64>(val64).to_string();
                }

                // Take MSB WIDTH bits
                bitstr = bitstr.substr(0, WIDTH);
                return sc_lv<WIDTH>(bitstr);
            }
            else
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid mode. Use \"lsb\" or \"msb\".");
                return sc_lv<WIDTH>(); // Return empty just in case
            }
        }
    }

    // Overload for float
    template <int WIDTH>
    sc_lv<WIDTH> sc_lv_cast(const float &value)
    {
        static_assert(WIDTH == 32, "Width must be 32 bits for float to sc_lv");
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
    sc_lv<WIDTH> sc_lv_cast(const std::string &input_str)
    {
        static_assert(WIDTH > 0, "Width must be positive");

        std::string unknown(WIDTH, 'X');
        std::string str = input_str; // make a copy to modify
        constexpr int64_t signedMax(int width)
        {
            return (int64_t(1) << (width - 1)) - 1;
        }
        constexpr int64_t signedMin(int width)
        {
            return -(int64_t(1) << (width - 1));
        }
        constexpr uint64_t unsignedMax(int width)
        {
            return (uint64_t(1) << width) - 1;
        }

        // Remove whitespace and warn
        if (std::any_of(str.begin(), str.end(), ::isspace))
        {
            SC_REPORT_WARNING("sc_lv_cast", "Whitespace found in input string — it was removed.");
            str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
        }

        size_t size = str.size();
        if (size == 0)
        {
            SC_REPORT_ERROR("sc_lv_cast", "Empty string input");
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
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in binary string");

                return sc_lv<WIDTH>(unknown);
            }
            if (substr2_size > WIDTH)
            {
                SC_REPORT_WARNING("sc_lv_cast", "Binary string longer than WIDTH — truncating LSBs.");
                substr2 = substr2.substr(substr2_size - WIDTH);
            }
            else if (substr2_size < WIDTH)
            {
                substr2 = std::string(WIDTH - substr2_size, '0') + substr2;
            }
            return sc_lv<WIDTH>(substr2);
        }

        // Hex prefix
        if (size >= 3 && c0 == '0' && (c1 == 'x' || c1 == 'X'))
        {
            if (!std::regex_match(substr2, std::regex("^[0-9a-fA-FxzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in hex string");
                return sc_lv<WIDTH>(unknown);
            }
            int bitsNum = static_cast<int>(substr2_size) * 4;
            if (bitsNum > WIDTH)
            {
                SC_REPORT_WARNING("sc_lv_cast", "Hex string longer than WIDTH — truncating MSBs.");
                substr2 = substr2.substr(substr2_size - int(WIDTH / 4));
            }
            else if (bitsNum < WIDTH)
            {
                substr2 = std::string(int((WIDTH - bitsNum) / 4), '0') + substr2;
            }

            std::string bitstr;
            for (char ch : substr2)
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
            return sc_lv<WIDTH>(bitstr);
        }

        // Octal prefix
        if (size >= 2 && c0 == '0' && std::isdigit(c1))
        {
            std::string oct_str = substr1; // Use the rest of the string after '0'
            if (!std::regex_match(oct_str, std::regex("^[0-7xzXZ]+$")))
            {
                SC_REPORT_ERROR("sc_lv_cast", "Invalid characters in octal string");
                return sc_lv<WIDTH>(unknown);
            }
            int bitsNum = static_cast<int>(oct_str.size()) * 3;
            if (bitsNum > WIDTH)
            {
                SC_REPORT_WARNING("sc_lv_cast", "Octal string longer than WIDTH — truncating LSBs.");
                oct_str = oct_str.substr(oct_str.size() - int(WIDTH / 3));
            }
            else if (bitsNum < WIDTH)
            {
                oct_str = std::string(int((WIDTH - bitsNum) / 3), '0') + oct_str;
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
            return sc_lv<WIDTH>(bitstr);
        }

        // Decimal (signed or unsigned)
        if (std::regex_match(str, std::regex("^[+-]?[0-9]+$")))
        {
            try
            {
                bool isNegative = !str.empty() && str[0] == '-';
                uint64_t absVal = 0;

                // Use unsigned stoull for magnitude (strip + or -)
                std::string numStr = (str[0] == '+' || str[0] == '-') ? str.substr(1) : str;
                absVal = std::stoull(numStr, nullptr, 10);

                // Check overflow based on WIDTH and signedness
                bool isSigned = true; // You can decide or template this logic
                // For simplicity assume signed (common case)

                int64_t val = isNegative ? -int64_t(absVal) : int64_t(absVal);

                if (val > signedMax(WIDTH))
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value overflows signed max for WIDTH");
                else if (val < signedMin(WIDTH))
                    SC_REPORT_WARNING("sc_lv_cast", "Decimal value underflows signed min for WIDTH");

                // Convert val to sc_lv with sign extension if needed
                return sc_lv_cast<int64_t, WIDTH>(val);
            }
            catch (const std::exception &e)
            {
                SC_REPORT_ERROR("sc_lv_cast", ("Decimal string conversion failed: " + std::string(e.what())).c_str());
                return sc_lv<WIDTH>(unknown);
            }
        }

        SC_REPORT_ERROR("sc_lv_cast", "Invalid string format for sc_lv conversion.");
        return sc_lv<WIDTH>();
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
                SC_REPORT_WARNING("sc_lv_cast", "Input contains unknown (X/Z) bits. Returning 0.");
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
    T string_cast(string &str, int base = 0)
    {
        // Case 1: sc_lv<N>
        if constexpr (sc_is_lv<T>::value) // Custom trait for sc_lv<> detection
        {

            using LV = T;
            constexpr int WIDTH = LV::length();
            return sc_lv_cast<WIDTH>(str, base);

            // std::string bin;

            // std::string::substr
            //     Purpose:
            //     Returns a substring from a given string.

            //     Syntax:
            // std::string substr(size_t pos = 0, size_t len = npos) const;

            // Parameters:
            //     pos — Starting position (index) in the original string from where the substring begins.
            //     len — Length of the substring to extract. If omitted or exceeds the string length, substring goes to the end.

            // Returns:
            // A new std::string containing the substring starting at pos with length len.

            // std::string::rfind

            //     Purpose:
            //     Finds the last occurrence of a substring or character in a string.

            //     Syntax:

            // size_t rfind(const std::string& str, size_t pos = npos) const;
            // size_t rfind(const char* s, size_t pos = npos) const;
            // size_t rfind(char c, size_t pos = npos) const;

            // Parameters:

            //     str, s, or c — The substring, C-string, or character you want to find.
            //     pos — The position in the string where the search starts backwards from. Default is npos which means start from the end of the string. I meant the right side (the end of the string).

            // Returns:
            // The index of the last occurrence of the substring/character found before or at pos.
            // Returns std::string::npos if not found.

            // ✅ Summary:

            // Use find() if you want the first occurrence (e.g. searching entire string).

            // Use rfind("pattern", 0) if you want to check whether a string starts with "pattern".

            // It's a trick:
            // str.rfind("0x", 0) == 0  // ⇨ starts with "0x"

            if (base == 0)
            {
                if (str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0)
                {
                    base = 16;
                    str = str.substr(2);
                }
                else if (str.rfind("0b", 0) == 0 || str.rfind("0B", 0) == 0)
                {
                    base = 2;
                    str = str.substr(2);
                }
                else if (str.size() > 1 && str[0] == '0')
                    base = 8;
                else
                    base = 10;
            }

            // Now convert to binary string using bitset
            uint64_t num = std::stoull(str, nullptr, base);
            bin = std::bitset<64>(num).to_string(); // up to 64 bits
            bin = bin.substr(64 - WIDTH, WIDTH);    // take rightmost WIDTH bits

            // return LV(bin);
        }

        // Case 2: Integral and bool types
        else if constexpr (std::is_integral<T>::value)
        {
            if (base == 0)
            {
                if (str.rfind("0x", 0) == 0 || str.rfind("0X", 0) == 0)
                    base = 16;
                else if (str.rfind("0b", 0) == 0 || str.rfind("0B", 0) == 0)
                {
                    base = 2;
                    str = str.substr(2);
                }
                else if (str.size() > 1 && str[0] == '0')
                    base = 8;
                else
                    base = 10;
            }

            return static_cast<T>(std::stoull(str, nullptr, base));
        }

        // Case 3: Floating-point types
        else if constexpr (std::is_floating_point<T>::value)
        {
            return static_cast<T>(std::stod(str));
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
