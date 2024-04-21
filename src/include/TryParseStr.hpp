#pragma once

#include <sstream>
#include <type_traits>

/**
 * @brief Parses the given string into the given output value.
 * 
 * @tparam T the type of the output value
 * @param str the string to parse
 * @param outval the output value to fill with the parsed value
 * @return true if the parsing was successful, false otherwise
 */
template <typename T>
requires std::is_trivially_copyable_v<T>
bool try_parse(const std::string str, T* outval) {
    std::stringstream ss(str);
    std::string unused;
    T temp {};

    if (static_cast<bool>(ss >> temp)) {
        if (static_cast<bool>(ss >> unused)) {
            // This means the previous parsing didn't parse full string
            // hence we return false
            return false;
        }
        // We successfully parsed the full string
        *outval = temp;
        return true;
    }
    return false;
}