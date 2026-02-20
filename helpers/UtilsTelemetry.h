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

// telemetry
#ifdef ENABLE_TELEMETRY_LOGGING
#include <telemetry_busmessage_sender.h>
#endif

namespace Utils
{
    struct Telemetry
    {
        static void init()
        {
        LOGINFO("[Telemetry2] Initializing telemetry for AppGateway");
#ifdef ENABLE_TELEMETRY_LOGGING
        LOGINFO("[Telemetry2] Initializing telemetry for AppGateway FLAG ENABLE_TELEMETRY_LOGGING is defined");
            t2_init((char *) "appgateway");
#endif
        LOGINFO("[Telemetry2] Telemetry initialized");
        };

        static void sendMessage(char* message)
        {
#ifdef ENABLE_TELEMETRY_LOGGING
            t2_event_s((char *)"APPGATEWAY_MESSAGE", message);
#endif
        };

        static void sendMessage(char *marker, char* message)
        {
        LOGINFO("[Telemetry2] Sending telemetry message: %s: %s", marker, message);
#ifdef ENABLE_TELEMETRY_LOGGING
            t2_event_s(marker, message);
#endif
        };

        static void sendError(const char* format, ...)
        {
        LOGINFO("[Telemetry2] Sending telemetry error: %s", format);
#ifdef ENABLE_TELEMETRY_LOGGING
            va_list parameters;
            va_start(parameters, format);
            std::string message;
            WPEFramework::Trace::Format(message, format, parameters);
            va_end(parameters);

            // get rid of const for t2_event_s
            char* error = strdup(message.c_str());
            t2_event_s((char *)"APPGATEWAY_ERROR", error);
            if (error)
            {
                free(error);
            }
#endif
        };
    };
}
