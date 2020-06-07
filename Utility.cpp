/*
 * Utility.cpp
 *
 *  Created on: 07.06.2020
 *      Author: jierr
 */

#include "Utility.hpp"

#include <algorithm>

namespace Utility
{
std::string toLower(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c){ return std::tolower(c); });
    return result;
}
} // Utility
