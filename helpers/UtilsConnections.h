/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2024 RDK Management
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
#pragma once

#include "UtilsLogging.h"
#include <plugins/plugins.h>

namespace WPEFramework
{
    namespace Utils
    {

        std::string ResolveQuery(const std::string &query, const std::string &key)
        {
            // Check if query is empty
            if (query.empty())
            {
                LOGWARN("Query is empty");
                return "";
            }

            // Find key position
            size_t pos = query.find(key);
            if (pos == std::string::npos)
            {
                LOGWARN("%s not found in query: %s\n", key.c_str(), query.c_str());
                return "";
            }

            std::string value = query.substr(pos + key.length()+1);

            if (value.empty())
            {
                LOGERR("ResolveQuery: '%s' value missing in query: %s\n", key.c_str(), query.c_str());
                return "";
            }

            // Check if there any additional parameter keys
            size_t additional_param_key = query.find("&");
            if (additional_param_key != std::string::npos)
            {
                std::string new_value = value.substr(0,additional_param_key);
                if (new_value.empty()) {
                    LOGWARN("%s query params looks incorrect", value.c_str());
                    return value;
                } else {
                    return new_value;
                }
            }
            return value;
        }
    }
}