/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#ifndef __STRINGUTILS_H__
#define __STRINGUTILS_H__

#include <string>
#include <algorithm>
#include <cctype>
#include "UtilsLogging.h"

class StringUtils {
    public:
    static std::string toLower(const std::string& input) {
        std::string result = input;
        std::transform(result.begin(), result.end(), result.begin(),
                  [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    static bool rfindInsensitive(const std::string& reference, const std::string& key) {
        std::string lowerRef = toLower(reference);
        std::string lowerKey = toLower(key);
        return lowerRef.rfind(lowerKey) != std::string::npos;
    }

    static bool checkStartsWithCaseInsensitive(const std::string& method, const std::string& key) {
        std::string lowerRef = toLower(method);
        std::string lowerKey = toLower(key);
        return lowerRef.rfind(lowerKey) == 0;
    }

    static std::string extractMethodName(const std::string& method) {
        size_t lastDot = method.rfind('.');
        if (lastDot == std::string::npos || lastDot + 1 >= method.length()) {
            LOGERR("Invalid method format, cannot extract appId: %s", method.c_str());
            return "";
        }
        std::string extracted = method.substr(lastDot + 1);
        return StringUtils::toLower(extracted);
    }
};

#endif