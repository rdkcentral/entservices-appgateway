/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#pragma once

#include <string>
#include <limits>
#include <core/JSON.h>
#include "UtilsLogging.h"

namespace WPEFramework {

class JsonValidation {
public:
    /**
     * @brief Safely validates JSON payload and extracts string value
     * @param payload The JSON payload to parse
     * @param extractedValue Output string for the extracted value
     * @param fieldName The field name to extract (default: "value")
     * @param allowEmpty Whether an empty string is considered valid (default: false)
     * @return true if validation successful, false otherwise
     */
    static bool ValidateAndExtractString(const std::string& payload, std::string& extractedValue, const std::string& fieldName = "value", const bool allowEmpty = false) {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            LOGWARN("ValidateAndExtractString: Failed to parse JSON payload");
            return false;
        }
        if (!params.HasLabel(fieldName.c_str())) {
            LOGERR("ValidateAndExtractString: Missing field '%s' in payload", fieldName.c_str());
            return false;
        }
        const Core::JSON::Variant& value = params.Get(fieldName.c_str());
        if (Core::JSON::Variant::type::STRING != value.Content()) {
            LOGERR("ValidateAndExtractString: Field '%s' is not a string", fieldName.c_str());
            return false;
        }
        extractedValue = value.String();
        if ((true != allowEmpty) && extractedValue.empty()) {
            LOGERR("ValidateAndExtractString: Field '%s' contains empty string", fieldName.c_str());
            return false;
        }
        
        return true;
    }

    /**
     * @brief Safely validates JSON payload and extracts boolean value
     * @param payload The JSON payload to parse
     * @param extractedValue Output boolean for the extracted value
     * @param fieldName The field name to extract (default: "value")
     * @return true if validation successful, false otherwise
     */
    static bool ValidateAndExtractBool(const std::string& payload, bool& extractedValue, const std::string& fieldName = "value") {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            LOGWARN("ValidateAndExtractBool: Failed to parse JSON payload");
            return false;
        }
        
        if (!params.HasLabel(fieldName.c_str())) {
            LOGERR("ValidateAndExtractBool: Missing field '%s' in payload", fieldName.c_str());
            return false;
        }
        
        const Core::JSON::Variant& value = params.Get(fieldName.c_str());
        if (Core::JSON::Variant::type::BOOLEAN != value.Content()) {
            LOGERR("ValidateAndExtractBool: Field '%s' is not a boolean", fieldName.c_str());
            return false;
        }
        
        extractedValue = value.Boolean();
        return true;
    }

    /**
     * @brief Safely validates JSON payload and extracts numeric value with bounds checking
     * @param payload The JSON payload to parse
     * @param extractedValue Output double for the extracted value
     * @param fieldName The field name to extract (default: "value")
     * @param minValue Minimum allowed value (used only if checkMinValue is true)
     * @param maxValue Maximum allowed value (used only if checkMaxValue is true)
     * @param checkMinValue Whether to apply minimum value checking (default: false)
     * @param checkMaxValue Whether to apply maximum value checking (default: false)
     * @return true if validation successful, false otherwise
     */
    static bool ValidateAndExtractDouble(const std::string& payload, 
                                       double& extractedValue,
                                       const std::string& fieldName = "value", 
                                       double minValue = 0.0, 
                                       double maxValue = 0.0,
                                       bool checkMinValue = false,
                                       bool checkMaxValue = false) {
        Core::JSON::VariantContainer params;
        if (!params.FromString(payload)) {
            LOGWARN("ValidateAndExtractDouble: Failed to parse JSON payload");
            return false;
        }
        
        if (!params.HasLabel(fieldName.c_str())) {
            LOGERR("ValidateAndExtractDouble: Missing field '%s' in payload", fieldName.c_str());
            return false;
        }
        
        const Core::JSON::Variant& value = params.Get(fieldName.c_str());
        if (Core::JSON::Variant::type::NUMBER != value.Content()) {
            LOGERR("ValidateAndExtractDouble: Field '%s' is not a number", fieldName.c_str());
            return false;
        }
        
        extractedValue = value.Number();
        
        // Bounds checking only if explicitly requested
        if (checkMinValue && extractedValue < minValue) {
            LOGERR("ValidateAndExtractDouble: Value %.2f is below minimum %.2f", extractedValue, minValue);
            return false;
        }
        
        if (checkMaxValue && extractedValue > maxValue) {
            LOGERR("ValidateAndExtractDouble: Value %.2f exceeds maximum %.2f", extractedValue, maxValue);
            return false;
        }
        
        return true;
    }
};

} // namespace WPEFramework
