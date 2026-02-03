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
#ifndef __USERSETTINGSDELEGATE_H__
#define __USERSETTINGSDELEGATE_H__
#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/IUserSettings.h>
#include <interfaces/ITextTrack.h>
#include "UtilsLogging.h"
#include "ObjectUtils.h"
#include <set>
#include <sstream>
using namespace WPEFramework;
#define USERSETTINGS_CALLSIGN "org.rdk.UserSettings"
#define TEXTTRACK_CALLSIGN "org.rdk.TextTrack"

static const std::set<string> VALID_USER_SETTINGS_EVENT = {
    "localization.onlanguagechanged",
    "localization.onlocalechanged",
    "localization.onpreferredaudiolanguageschanged",
    "accessibility.onaudiodescriptionsettingschanged",
    "accessibility.onhighcontrastuichanged",
    "closedcaptions.onenabledchanged",
    "closedcaptions.onpreferredlanguageschanged",
    "accessibility.onclosedcaptionssettingschanged",
    "accessibility.onvoiceguidancesettingschanged",
};

// Events that require TextTrack interface registration
static const std::set<string> TEXTTRACK_EVENTS = {
    "accessibility.onclosedcaptionssettingschanged"
};

// Helper functions to convert enums to strings matching TextTrack API format
static const char* FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily family) {
    switch (family) {
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CONTENT_DEFAULT:
            return "null";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::MONOSPACED_SERIF:
            return "monospaced_serif";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::PROPORTIONAL_SERIF:
            return "proportional_serif";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::MONOSPACE_SANS_SERIF:
            return "monospaced_sanserif";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::PROPORTIONAL_SANS_SERIF:
            return "proportional_sanserif";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CASUAL:
            return "casual";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CURSIVE:
            return "cursive";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::SMALL_CAPITAL:
            return "smallcaps";
        default:
            return "null";
    }
}

static int FontSizeToNumber(Exchange::ITextTrackClosedCaptionsStyle::FontSize size) {
    switch (size) {
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::CONTENT_DEFAULT:
            return -1;  // Never added to JSON when CONTENT_DEFAULT
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::SMALL:
            return 0;
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::REGULAR:
            return 1;
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::LARGE:
            return 2;
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::EXTRA_LARGE:
            return 3;
        default:
            return -1;
    }
}

static const char* FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge edge) {
    switch (edge) {
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::CONTENT_DEFAULT:
            return "null";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::NONE:
            return "none";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RAISED:
            return "raised";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::DEPRESSED:
            return "depressed";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::UNIFORM:
            return "uniform";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::LEFT_DROP_SHADOW:
            return "drop_shadow_left";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RIGHT_DROP_SHADOW:
            return "drop_shadow_right";
        default:
            return "null";
    }
}

// Helper function to parse comma-separated languages into JSON array
// Parses: "eng,fra,spa" -> JSON array ["eng","fra","spa"]
static void ParseCommaSeparatedLanguages(const string& commaSeparatedLanguages, JsonArray& jsonArray) {
    if (!commaSeparatedLanguages.empty()) {
        // Split comma-separated string into JSON array
        std::istringstream stream(commaSeparatedLanguages);
        string token;
        while (std::getline(stream, token, ',')) {
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t");
            if (start == string::npos) {
                continue; // Skip all-whitespace tokens
            }
            token.erase(0, start);
            size_t end = token.find_last_not_of(" \t");
            if (end != string::npos) {
                token.erase(end + 1);
            }
            if (!token.empty()) {
                jsonArray.Add(token);
            }
        }
    }
}

// Helper function to convert JSON array or string to comma-separated format
// Handles: JSON array ["eng","fra","spa"] -> "eng,fra,spa"
static string ConvertToCommaSeparatedLanguages(const string& languages) {
    string commaSeparatedLanguages;
    
    // Try parsing as JSON array first
    JsonArray jsonArray;
    if (jsonArray.FromString(languages)) {
        // Successfully parsed as JSON array
        bool first = true;
        JsonArray::Iterator it = jsonArray.Elements();
        while (it.Next()) {
            if (it.Current().Content() == JsonValue::type::STRING) {
                if (!first) commaSeparatedLanguages += ",";
                commaSeparatedLanguages += it.Current().String();
                first = false;
            } else {
                LOGWARN("ConvertToCommaSeparatedLanguages: ignoring non-string element in languages array");
            }
        }
    } else {
        // Not a JSON array, treat as single string value
        // Remove quotes if present
        if (languages.length() >= 2 && languages[0] == '"' && languages.back() == '"') {
            commaSeparatedLanguages = languages.substr(1, languages.length() - 2);
        } else if (languages == "[]") {
            commaSeparatedLanguages = "";  // Empty array
        } else {
            commaSeparatedLanguages = languages;
        }
    }
    
    return commaSeparatedLanguages;
}

// Helper function to build JSON styles object from ClosedCaptionsStyle struct
static void BuildClosedCaptionsStyleJson(const Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle& style, JsonObject& styles) {
    // Only add fontFamily if not CONTENT_DEFAULT
    if (style.fontFamily != Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CONTENT_DEFAULT) {
        styles["fontFamily"] = FontFamilyToString(style.fontFamily);
    }

    // Only add fontSize if not CONTENT_DEFAULT
    if (style.fontSize != Exchange::ITextTrackClosedCaptionsStyle::FontSize::CONTENT_DEFAULT) {
        styles["fontSize"] = FontSizeToNumber(style.fontSize);
    }

    // Only add fontColor if not empty
    if (!style.fontColor.empty()) {
        styles["fontColor"] = style.fontColor;
    }

    // Only add fontOpacity if >= 0
    int fontOpacity = static_cast<int>(style.fontOpacity);
    if (fontOpacity >= 0) {
        if (fontOpacity > 255) {
            LOGWARN("fontOpacity out of range (%d), clamping to 255", fontOpacity);
            fontOpacity = 255;
        }
        styles["fontOpacity"] = fontOpacity;
    }

    // Only add fontEdge if not CONTENT_DEFAULT
    if (style.fontEdge != Exchange::ITextTrackClosedCaptionsStyle::FontEdge::CONTENT_DEFAULT) {
        styles["fontEdge"] = FontEdgeToString(style.fontEdge);
    }

    // Only add fontEdgeColor if not empty
    if (!style.fontEdgeColor.empty()) {
        styles["fontEdgeColor"] = style.fontEdgeColor;
    }

    // Only add backgroundColor if not empty
    if (!style.backgroundColor.empty()) {
        styles["backgroundColor"] = style.backgroundColor;
    }

    // Only add backgroundOpacity if >= 0
    int backgroundOpacity = static_cast<int>(style.backgroundOpacity);
    if (backgroundOpacity >= 0) {
        if (backgroundOpacity > 255) {
            LOGWARN("backgroundOpacity out of range (%d), clamping to 255", backgroundOpacity);
            backgroundOpacity = 255;
        }
        styles["backgroundOpacity"] = backgroundOpacity;
    }

    // Only add windowColor if not empty
    if (!style.windowColor.empty()) {
        styles["windowColor"] = style.windowColor;
    }

    // Only add windowOpacity if >= 0
    int windowOpacity = static_cast<int>(style.windowOpacity);
    if (windowOpacity >= 0) {
        if (windowOpacity > 255) {
            LOGWARN("windowOpacity out of range (%d), clamping to 255", windowOpacity);
            windowOpacity = 255;
        }
        styles["windowOpacity"] = windowOpacity;
    }
}

class UserSettingsDelegate : public BaseEventDelegate{
    public:
        UserSettingsDelegate(PluginHost::IShell* shell):
            BaseEventDelegate(), mUserSettings(nullptr), mTextTrack(nullptr), mShell(shell),
            mNotificationHandler(*this), mTextTrackNotificationHandler(*this) {}

        ~UserSettingsDelegate() {
            std::scoped_lock<std::mutex, std::mutex> lock(mRegistrationMutex, mInterfaceMutex);
            // Unregister notification handlers before releasing interfaces
            if (mUserSettings != nullptr) {
                if (mNotificationHandler.GetRegistered()) {
                    mUserSettings->Unregister(&mNotificationHandler);
                }
                mUserSettings->Release();
                mUserSettings = nullptr;
            }
            if (mTextTrack != nullptr) {
                if (mTextTrackNotificationHandler.GetRegistered()) {
                    mTextTrack->Unregister(&mTextTrackNotificationHandler);
                }
                mTextTrack->Release();
                mTextTrack = nullptr;
            }
        }

        bool HandleSubscription(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen) {
            if (listen) {
                Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
                if (userSettings == nullptr) {
                    LOGERR("UserSettings interface not available");
                    return false;
                }

                // Protect UserSettings registration with lock to prevent race condition
                {
                    std::lock_guard<std::mutex> lock(mRegistrationMutex);
                    if (!mNotificationHandler.GetRegistered()) {
                        LOGINFO("Registering for UserSettings notifications");
                        userSettings->Register(&mNotificationHandler);
                        mNotificationHandler.SetRegistered(true);
                    }
                }

                // Register for TextTrack notifications only for closed captions related events
                std::string lowerEvent = StringUtils::toLower(event);
                if (TEXTTRACK_EVENTS.find(lowerEvent) != TEXTTRACK_EVENTS.end()) {
                    // Acquire TextTrack interface outside of mRegistrationMutex to maintain lock ordering
                    Exchange::ITextTrackClosedCaptionsStyle* textTrack = GetTextTrackInterface();
                    if (textTrack != nullptr) {
                        std::lock_guard<std::mutex> lock(mRegistrationMutex);
                        if (!mTextTrackNotificationHandler.GetRegistered()) {
                            LOGINFO("Registering for TextTrack notifications (event: %s)", event.c_str());
                            textTrack->Register(&mTextTrackNotificationHandler);
                            mTextTrackNotificationHandler.SetRegistered(true);
                        }
                    }
                }

                AddNotification(event, cb);

                return true;
            } else {
                // Not removing the notification subscription for cases where only one event is removed 
                // Registration is lazy one but we need to evaluate if there is any value in unregistering
                // given these API calls are always made
                RemoveNotification(event, cb);
            }
            return false;
        }

        bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen, bool &registrationError) {
            LOGDBG("Checking for handle event");
            // Check if event is present in VALID_USER_SETTINGS_EVENT make check case insensitive
            if (VALID_USER_SETTINGS_EVENT.find(StringUtils::toLower(event)) != VALID_USER_SETTINGS_EVENT.end()) {
                // Handle TextToSpeech event
                registrationError = HandleSubscription(cb, event, listen);
                return true;
            }
            return false;
        }

	 // Common method to ensure mUserSettings is available for all APIs and notifications
        Exchange::IUserSettings* GetUserSettingsInterface() {
            std::lock_guard<std::mutex> lock(mInterfaceMutex);
            if (mUserSettings == nullptr && mShell != nullptr) {
                mUserSettings = mShell->QueryInterfaceByCallsign<Exchange::IUserSettings>(USERSETTINGS_CALLSIGN);
                if (mUserSettings == nullptr) {
                    LOGERR("Failed to get UserSettings COM interface");
                }
            }
            return mUserSettings;
        }

        // Common method to ensure mTextTrack is available for all APIs and notifications
        Exchange::ITextTrackClosedCaptionsStyle* GetTextTrackInterface() {
            std::lock_guard<std::mutex> lock(mInterfaceMutex);
            if (mTextTrack == nullptr && mShell != nullptr) {
                mTextTrack = mShell->QueryInterfaceByCallsign<Exchange::ITextTrackClosedCaptionsStyle>(TEXTTRACK_CALLSIGN);
                if (mTextTrack == nullptr) {
                    LOGERR("Failed to get TextTrack COM interface");
                }
            }
            return mTextTrack;
        }

        Core::hresult GetVoiceGuidance(string& result) {
            LOGINFO("GetVoiceGuidance from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetVoiceGuidance(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldn't get voiceguidance state")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetVoiceGuidance on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get voiceguidance state\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetAudioDescription(string& result) {
            LOGINFO("GetAudioDescription from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetAudioDescription(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error({ enabled: .result }, "couldn't get audio description settings")
                // Create JSON response with enabled state
                result = ObjectUtils::CreateBooleanJsonString("enabled", enabled);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetAudioDescription on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get audio description settings\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetAudioDescriptionsEnabled(string& result) {
            LOGINFO("GetAudioDescriptionsEnabled from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetAudioDescription(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldn't get audio descriptions enabled")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetAudioDescription on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get audio descriptions enabled\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetHighContrast(string& result) {
            LOGINFO("GetHighContrast from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetHighContrast(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldn't get high contrast state")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetHighContrast on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get high contrast state\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetCaptions(string& result) {
            LOGINFO("GetCaptions from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetCaptions(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldn't get captions state")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetCaptions on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get captions state\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetClosedCaptionsStyle(string& result) {
            LOGINFO("GetClosedCaptionsStyle from TextTrack COM interface");
            result.clear();

            Exchange::ITextTrackClosedCaptionsStyle* textTrack = GetTextTrackInterface();
            if (textTrack == nullptr) {
                LOGERR("TextTrack COM interface not available");
                result = "{\"error\":\"couldn't get closed captions style\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle style;
            Core::hresult rc = textTrack->GetClosedCaptionsStyle(style);

            if (rc == Core::ERROR_NONE) {
                // Build JSON response with all style properties using helper function
                JsonObject styles;
                BuildClosedCaptionsStyleJson(style, styles);
                styles.ToString(result);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetClosedCaptionsStyle on TextTrack COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get closed captions style\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetVoiceGuidance(const bool enabled) {
            LOGINFO("SetVoiceGuidance to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetVoiceGuidance(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set voiceguidance enabled")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetVoiceGuidance on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetAudioDescriptionsEnabled(const bool enabled) {
            LOGINFO("SetAudioDescriptionsEnabled to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetAudioDescription(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set audio descriptions enabled")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetAudioDescription on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetCaptions(const bool enabled) {
            LOGINFO("SetCaptions to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetCaptions(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set captions enabled")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetCaptions on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetVoiceGuidanceRate(const double rate) {
            LOGINFO("SetVoiceGuidanceRate to UserSettings COM interface: %f", rate);

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { rate: transformed_rate }
            // The rate parameter is already the transformed value from vg_speed_firebolt2thunder
            Core::hresult rc = userSettings->SetVoiceGuidanceRate(rate);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set speed")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetVoiceGuidanceRate on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetVoiceGuidanceHints(const bool enabled) {
            LOGINFO("SetVoiceGuidanceHints to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetVoiceGuidanceHints(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set voice guidance hints")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetVoiceGuidanceHints on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetVoiceGuidanceRate(double& rate) {
            LOGINFO("GetVoiceGuidanceRate from UserSettings COM interface");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            Core::hresult rc = userSettings->GetVoiceGuidanceRate(rate);

            if (rc == Core::ERROR_NONE) {
                LOGINFO("Got voice guidance rate: %f", rate);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetVoiceGuidanceRate on UserSettings COM interface, error: %u", rc);
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetVoiceGuidanceHints(string& result) {
            LOGINFO("GetVoiceGuidanceHints from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool hints = false;
            Core::hresult rc = userSettings->GetVoiceGuidanceHints(hints);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_error(.result, "couldn't get navigationHints")
                // Return the boolean result directly as per transform specification
                result = hints ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetVoiceGuidanceHints on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get navigationHints\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetPresentationLanguage(string& result) {
            LOGINFO("GetPresentationLanguage from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            string presentationLanguage;
            Core::hresult rc = userSettings->GetPresentationLanguage(presentationLanguage);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_error(.result | split("-") | .[0], "couldn't get language")
                if (!presentationLanguage.empty()) {
                    // Extract language part (before "-") from locale like "en-US" -> "en"
                    size_t dashPos = presentationLanguage.find('-');
                    string language;
                    if (dashPos != string::npos) {
                        language = presentationLanguage.substr(0, dashPos);
                    } else {
                        // If no dash found, return the whole string
                        language = presentationLanguage;
                    }
                    // Wrap in quotes to make it a valid JSON string
                    result = "\"" + language + "\"";
                    return Core::ERROR_NONE;
                } else {
                    result = "{\"error\":\"couldn't get language\"}";
                    return Core::ERROR_GENERAL;
                }
            } else {
                LOGERR("Failed to call GetPresentationLanguage on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetLocale(string& result) {
            LOGINFO("GetLocale from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            string presentationLanguage;
            Core::hresult rc = userSettings->GetPresentationLanguage(presentationLanguage);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_error(.result, "couldn't get locale")
                // Return the full locale without any transformation
                if (!presentationLanguage.empty()) {
                    // Wrap in quotes to make it a valid JSON string
                    result = "\"" + presentationLanguage + "\"";
                    return Core::ERROR_NONE;
                } else {
                    result = "{\"error\":\"couldn't get locale\"}";
                    return Core::ERROR_GENERAL;
                }
            } else {
                LOGERR("Failed to call GetPresentationLanguage on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetLocale(const string& locale) {
            LOGINFO("SetLocale to UserSettings COM interface: %s", locale.c_str());

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { presentationLanguage: .value }
            // The locale parameter is the .value from the request
            Core::hresult rc = userSettings->SetPresentationLanguage(locale);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set locale")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetPresentationLanguage on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetPreferredAudioLanguages(string& result) {
            LOGINFO("GetPreferredAudioLanguages from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "[]";  // Return empty array on error
                return Core::ERROR_UNAVAILABLE;
            }

            string preferredLanguages;
            Core::hresult rc = userSettings->GetPreferredAudioLanguages(preferredLanguages);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_else(.result | split(","), [])
                JsonArray jsonArray;
                ParseCommaSeparatedLanguages(preferredLanguages, jsonArray);
                jsonArray.ToString(result);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetPreferredAudioLanguages on UserSettings COM interface, error: %u", rc);
                result = "[]";  // Return empty array on error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetPreferredCaptionsLanguages(string& result) {
            LOGINFO("GetPreferredCaptionsLanguages from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "[\"eng\"]";  // Return default ["eng"] on error
                return Core::ERROR_UNAVAILABLE;
            }

            string preferredLanguages;
            Core::hresult rc = userSettings->GetPreferredCaptionsLanguages(preferredLanguages);

            if (rc == Core::ERROR_NONE) {
                // Transform: if .result | length > 0 then .result | split(",") else ["eng"] end
                JsonArray jsonArray;
                ParseCommaSeparatedLanguages(preferredLanguages, jsonArray);

                if (jsonArray.Length() == 0) {
                    // Empty array - return default ["eng"]
                    jsonArray.Add("eng");
                }

                jsonArray.ToString(result);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetPreferredCaptionsLanguages on UserSettings COM interface, error: %u", rc);
                result = "[\"eng\"]";  // Return default ["eng"] on error
                return Core::ERROR_GENERAL;
            }
        }

	Core::hresult SetPreferredAudioLanguages(const string& languages) {
            LOGINFO("SetPreferredAudioLanguages to UserSettings COM interface: %s", languages.c_str());

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { preferredLanguages: (.value | join(","))}
            // The languages parameter can be either:
            // 1. A JSON array: ["eng","fra","spa"] -> "eng,fra,spa"
            // 2. A single string: "tam" -> "tam"

            string commaSeparatedLanguages = ConvertToCommaSeparatedLanguages(languages);
            LOGINFO("Converted to comma-separated: %s", commaSeparatedLanguages.c_str());

            Core::hresult rc = userSettings->SetPreferredAudioLanguages(commaSeparatedLanguages);

            if (rc == Core::ERROR_NONE) {
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetPreferredAudioLanguages on UserSettings COM interface, error: %u", rc);
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetPreferredCaptionsLanguages(const string& preferredLanguages) {
            LOGINFO("SetPreferredCaptionsLanguages to UserSettings COM interface: %s", preferredLanguages.c_str());

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { preferredLanguages: (.value | join(","))}
            // The preferredLanguages parameter can be either:
            // 1. A JSON array: ["eng","fra","spa"] -> "eng,fra,spa"
            // 2. A single string: "tam" -> "tam"

            string commaSeparatedLanguages = ConvertToCommaSeparatedLanguages(preferredLanguages);
            LOGINFO("Converted to comma-separated: %s", commaSeparatedLanguages.c_str());

            Core::hresult rc = userSettings->SetPreferredCaptionsLanguages(commaSeparatedLanguages);

            if (rc == Core::ERROR_NONE) {
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetPreferredCaptionsLanguages on UserSettings COM interface, error: %u", rc);
                return Core::ERROR_GENERAL;
            }
        }

    private:
        // Helper method to build and dispatch the combined closed captions settings notification
        void DispatchClosedCaptionsSettingsChanged(const char* callerContext) {
            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            Exchange::ITextTrackClosedCaptionsStyle* textTrack = GetTextTrackInterface();
            bool enabled = false;
            string preferredLanguages;

            // Get enabled state from UserSettings
            if (userSettings != nullptr) {
                Core::hresult captionsResult = userSettings->GetCaptions(enabled);
                if (captionsResult != Core::ERROR_NONE) {
                    LOGWARN("%s: GetCaptions failed with error %u, using default enabled=false", callerContext, captionsResult);
                }

                // Get preferred languages from UserSettings
                Core::hresult langsResult = userSettings->GetPreferredCaptionsLanguages(preferredLanguages);
                if (langsResult != Core::ERROR_NONE) {
                    LOGWARN("%s: GetPreferredCaptionsLanguages failed with error %u, using default [\"eng\"]", callerContext, langsResult);
                    preferredLanguages = "";
                }
            } else {
                LOGWARN("%s: UserSettings interface unavailable, using defaults (enabled=false, preferredLanguages=[\"eng\"])", callerContext);
            }

            // Build JSON response
            JsonObject response;
            response["enabled"] = enabled;

            // Add styles - get from TextTrack if available, otherwise use empty
            JsonObject styles;
            if (textTrack != nullptr) {
                Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle ccStyle;
                Core::hresult styleResult = textTrack->GetClosedCaptionsStyle(ccStyle);
                if (styleResult == Core::ERROR_NONE) {
                    BuildClosedCaptionsStyleJson(ccStyle, styles);
                } else {
                    LOGWARN("%s: GetClosedCaptionsStyle failed with error %u, using empty styles", callerContext, styleResult);
                }
            } else {
                LOGWARN("%s: TextTrack interface unavailable, using empty styles", callerContext);
            }
            response["styles"] = styles;

            // Add preferredLanguages array
            JsonArray languagesArray;
            ParseCommaSeparatedLanguages(preferredLanguages, languagesArray);
            if (languagesArray.Length() == 0) {
                languagesArray.Add("eng");  // Default to ["eng"] if empty
            }
            response["preferredLanguages"] = languagesArray;

            string result;
            response.ToString(result);
            Dispatch("accessibility.onclosedcaptionssettingschanged", result);
        }

        class UserSettingsNotificationHandler: public Exchange::IUserSettings::INotification {
            public:
                 UserSettingsNotificationHandler(UserSettingsDelegate& parent) : mParent(parent),registered(false){}
                ~UserSettingsNotificationHandler(){}

        void OnAudioDescriptionChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onaudiodescriptionsettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnPreferredAudioLanguagesChanged(const string& preferredLanguages) {
            mParent.Dispatch( "localization.onpreferredaudiolanguageschanged", preferredLanguages);
        }

        void OnPresentationLanguageChanged(const string& presentationLanguage) {
            
            mParent.Dispatch( "localization.onlocalechanged", presentationLanguage);

            // check presentationLanguage is a delimitted string like "en-US"
            // add logic to get the "en" if the value is "en-US"
            if (presentationLanguage.find('-') != string::npos) {
                string language = presentationLanguage.substr(0, presentationLanguage.find('-'));
                // Wrap in quotes to make it a valid JSON string
                string languageJson = "\"" + language + "\"";
                mParent.Dispatch( "localization.onlanguagechanged", languageJson);
            } else {
                LOGWARN("invalid value=%s set it must be a delimited string like en-US", presentationLanguage.c_str());
            }
        }

        void OnCaptionsChanged(const bool enabled) {
           mParent.Dispatch( "closedcaptions.onenabledchanged", ObjectUtils::BoolToJsonString(enabled));
           // Also dispatch accessibility.onclosedcaptionssettingschanged with combined settings
           mParent.DispatchClosedCaptionsSettingsChanged("OnCaptionsChanged");
        }

        void OnPreferredCaptionsLanguagesChanged(const string& preferredLanguages) {
            mParent.Dispatch( "closedcaptions.onpreferredlanguageschanged", preferredLanguages);
            // Also dispatch accessibility.onclosedcaptionssettingschanged with combined settings
            mParent.DispatchClosedCaptionsSettingsChanged("OnPreferredCaptionsLanguagesChanged");
        }

        void OnPreferredClosedCaptionServiceChanged(const string& service) {
            mParent.Dispatch( "OnPreferredClosedCaptionServiceChanged", service);
        }

        void OnPrivacyModeChanged(const string& privacyMode) {
            mParent.Dispatch( "OnPrivacyModeChanged", privacyMode);
        }

        void OnPinControlChanged(const bool pinControl) {
            mParent.Dispatch( "OnPinControlChanged", ObjectUtils::BoolToJsonString(pinControl));
        }

        void OnViewingRestrictionsChanged(const string& viewingRestrictions) {
            mParent.Dispatch( "OnViewingRestrictionsChanged", viewingRestrictions);
        }

        void OnViewingRestrictionsWindowChanged(const string& viewingRestrictionsWindow) {
            mParent.Dispatch( "OnViewingRestrictionsWindowChanged", viewingRestrictionsWindow);
        }

        void OnLiveWatershedChanged(const bool liveWatershed) {
            mParent.Dispatch( "OnLiveWatershedChanged", ObjectUtils::BoolToJsonString(liveWatershed));
        }

        void OnPlaybackWatershedChanged(const bool playbackWatershed) {
            mParent.Dispatch( "OnPlaybackWatershedChanged", ObjectUtils::BoolToJsonString(playbackWatershed));
        }

        void OnBlockNotRatedContentChanged(const bool blockNotRatedContent) {
            mParent.Dispatch( "OnBlockNotRatedContentChanged", ObjectUtils::BoolToJsonString(blockNotRatedContent));
        }

        void OnPinOnPurchaseChanged(const bool pinOnPurchase) {
            mParent.Dispatch( "OnPinOnPurchaseChanged", ObjectUtils::BoolToJsonString(pinOnPurchase));
        }

        void OnHighContrastChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onhighcontrastuichanged", ObjectUtils::BoolToJsonString(enabled));
        }

        void OnVoiceGuidanceChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onvoiceguidancesettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnVoiceGuidanceRateChanged(const double rate) {
            mParent.Dispatch( "OnVoiceGuidanceRateChanged", std::to_string(rate));
        }

        void OnVoiceGuidanceHintsChanged(const bool hints) {
            mParent.Dispatch( "OnVoiceGuidanceHintsChanged", std::to_string(hints));
        }

        void OnContentPinChanged(const string& contentPin) {
            mParent.Dispatch( "OnContentPinChanged", contentPin);
        }

        // Method to set registered state
        void SetRegistered(bool state) {
            std::lock_guard<std::mutex> lock(registerMutex);
            registered = state;
        }

        // Method to get registered state
        bool GetRegistered() {
            std::lock_guard<std::mutex> lock(registerMutex);
            return registered;
        }

                BEGIN_INTERFACE_MAP(UserSettingsNotificationHandler)
                INTERFACE_ENTRY(Exchange::IUserSettings::INotification)
                END_INTERFACE_MAP
            private:
                    UserSettingsDelegate& mParent;
                    bool registered;
                    std::mutex registerMutex;

        };

        class TextTrackNotificationHandler: public Exchange::ITextTrackClosedCaptionsStyle::INotification {
            public:
                 TextTrackNotificationHandler(UserSettingsDelegate& parent) : mParent(parent), registered(false) {}
                ~TextTrackNotificationHandler() {}

                void OnClosedCaptionsStyleChanged(const Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle &style) override {
                    LOGINFO("OnClosedCaptionsStyleChanged received");
                    // Dispatch accessibility.onclosedcaptionssettingschanged with combined settings
                    mParent.DispatchClosedCaptionsSettingsChanged("OnClosedCaptionsStyleChanged");
                }

        // Method to set registered state
        void SetRegistered(bool state) {
            std::lock_guard<std::mutex> lock(registerMutex);
            registered = state;
        }

        // Method to get registered state
        bool GetRegistered() {
            std::lock_guard<std::mutex> lock(registerMutex);
            return registered;
        }

                BEGIN_INTERFACE_MAP(TextTrackNotificationHandler)
                INTERFACE_ENTRY(Exchange::ITextTrackClosedCaptionsStyle::INotification)
                END_INTERFACE_MAP
            private:
                    UserSettingsDelegate& mParent;
                    bool registered;
                    std::mutex registerMutex;

        };

        Exchange::IUserSettings *mUserSettings;
        Exchange::ITextTrackClosedCaptionsStyle *mTextTrack;
        PluginHost::IShell* mShell;
        Core::Sink<UserSettingsNotificationHandler> mNotificationHandler;
        Core::Sink<TextTrackNotificationHandler> mTextTrackNotificationHandler;
        std::mutex mRegistrationMutex;
        std::mutex mInterfaceMutex;

};
#endif
