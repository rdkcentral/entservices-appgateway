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
#ifndef __TEXTTRACKDELEGATE_H__
#define __TEXTTRACKDELEGATE_H__
#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/ITextTrack.h>
#include "UtilsLogging.h"
#include "ObjectUtils.h"
#include <set>
#include <sstream>
using namespace WPEFramework;
#define TEXTTRACK_CALLSIGN "org.rdk.TextTrack"

static const std::set<string> VALID_TEXTTRACK_EVENT = {
    "accessibility.onclosedcaptionssettingschanged",
};

// Helper functions to convert enums to strings matching TextTrack API format
static const char* FontFamilyToString(Exchange::ITextTrackClosedCaptionsStyle::FontFamily family) {
    switch (family) {
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CONTENT_DEFAULT:
            return "CONTENT_DEFAULT";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::MONOSPACED_SERIF:
            return "MONOSPACED_SERIF";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::PROPORTIONAL_SERIF:
            return "PROPORTIONAL_SERIF";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::MONOSPACE_SANS_SERIF:
            return "MONOSPACE_SANS_SERIF";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::PROPORTIONAL_SANS_SERIF:
            return "PROPORTIONAL_SANS_SERIF";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CASUAL:
            return "CASUAL";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::CURSIVE:
            return "CURSIVE";
        case Exchange::ITextTrackClosedCaptionsStyle::FontFamily::SMALL_CAPITAL:
            return "SMALL_CAPITAL";
        default:
            return "CONTENT_DEFAULT";
    }
}

static const char* FontSizeToString(Exchange::ITextTrackClosedCaptionsStyle::FontSize size) {
    switch (size) {
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::CONTENT_DEFAULT:
            return "CONTENT_DEFAULT";
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::SMALL:
            return "SMALL";
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::REGULAR:
            return "REGULAR";
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::LARGE:
            return "LARGE";
        case Exchange::ITextTrackClosedCaptionsStyle::FontSize::EXTRA_LARGE:
            return "EXTRA_LARGE";
        default:
            return "CONTENT_DEFAULT";
    }
}

static const char* FontEdgeToString(Exchange::ITextTrackClosedCaptionsStyle::FontEdge edge) {
    switch (edge) {
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::CONTENT_DEFAULT:
            return "CONTENT_DEFAULT";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::NONE:
            return "NONE";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RAISED:
            return "RAISED";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::DEPRESSED:
            return "DEPRESSED";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::UNIFORM:
            return "UNIFORM";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::LEFT_DROP_SHADOW:
            return "LEFT_DROP_SHADOW";
        case Exchange::ITextTrackClosedCaptionsStyle::FontEdge::RIGHT_DROP_SHADOW:
            return "RIGHT_DROP_SHADOW";
        default:
            return "CONTENT_DEFAULT";
    }
}

class TextTrackDelegate : public BaseEventDelegate{
    public:
        TextTrackDelegate(PluginHost::IShell* shell):
            BaseEventDelegate(), mTextTrack(nullptr), mShell(shell), mNotificationHandler(*this) {}

        ~TextTrackDelegate() {
            if (mTextTrack != nullptr) {
                mTextTrack->Release();
                mTextTrack = nullptr;
            }
        }

        bool HandleSubscription(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen) {
            if (listen) {
                Exchange::ITextTrackClosedCaptionsStyle* textTrack = GetTextTrackInterface();
                if (textTrack == nullptr) {
                    LOGERR("TextTrack interface not available");
                    return false;
                }
	
                AddNotification(event, cb);

                if (!mNotificationHandler.GetRegistered()) {
                    LOGINFO("Registering for TextTrack notifications");
                    mTextTrack->Register(&mNotificationHandler);
                    mNotificationHandler.SetRegistered(true);
                    return true;
                } else {
                    LOGTRACE("Is TextTrack registered = %s", mNotificationHandler.GetRegistered() ? "true" : "false");
                }
                
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
            // Check if event is present in VALID_TEXTTRACK_EVENT make check case insensitive
            if (VALID_TEXTTRACK_EVENT.find(StringUtils::toLower(event)) != VALID_TEXTTRACK_EVENT.end()) {
                // Handle TextTrack event
                registrationError = HandleSubscription(cb, event, listen);
                return true;
            }
            return false;
        }

	 // Common method to ensure mTextTrack is available for all APIs and notifications
        Exchange::ITextTrackClosedCaptionsStyle* GetTextTrackInterface() {
            if (mTextTrack == nullptr && mShell != nullptr) {
                mTextTrack = mShell->QueryInterfaceByCallsign<Exchange::ITextTrackClosedCaptionsStyle>(TEXTTRACK_CALLSIGN);
                if (mTextTrack == nullptr) {
                    LOGERR("Failed to get TextTrack COM interface");
                }
            }
            return mTextTrack;
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
                // Build JSON response with all style properties
                // Use helper functions to convert enums to string values
                std::ostringstream jsonStream;
                jsonStream << "{";
                jsonStream << "\"fontFamily\":\"" << FontFamilyToString(style.fontFamily) << "\"";
                jsonStream << ",\"fontSize\":\"" << FontSizeToString(style.fontSize) << "\"";
                jsonStream << ",\"fontColor\":\"" << style.fontColor << "\"";
                jsonStream << ",\"fontOpacity\":" << static_cast<int>(style.fontOpacity);
                jsonStream << ",\"fontEdge\":\"" << FontEdgeToString(style.fontEdge) << "\"";
                jsonStream << ",\"fontEdgeColor\":\"" << style.fontEdgeColor << "\"";
                jsonStream << ",\"backgroundColor\":\"" << style.backgroundColor << "\"";
                jsonStream << ",\"backgroundOpacity\":" << static_cast<int>(style.backgroundOpacity);
                jsonStream << ",\"windowColor\":\"" << style.windowColor << "\"";
                jsonStream << ",\"windowOpacity\":" << static_cast<int>(style.windowOpacity);
                jsonStream << "}";
                
                result = jsonStream.str();
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetClosedCaptionsStyle on TextTrack COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get closed captions style\"}";
                return Core::ERROR_GENERAL;
            }
        }

    private:
        class TextTrackNotificationHandler: public Exchange::ITextTrackClosedCaptionsStyle::INotification {
            public:
                 TextTrackNotificationHandler(TextTrackDelegate& parent) : mParent(parent), registered(false) {}
                ~TextTrackNotificationHandler() {}

        void OnClosedCaptionsStyleChanged(const Exchange::ITextTrackClosedCaptionsStyle::ClosedCaptionsStyle &style) override {
            LOGINFO("OnClosedCaptionsStyleChanged received");
            
            // Build JSON response with all style properties
            // Use helper functions to convert enums to string values
            std::ostringstream jsonStream;
            jsonStream << "{";
            jsonStream << "\"fontFamily\":\"" << FontFamilyToString(style.fontFamily) << "\"";
            jsonStream << ",\"fontSize\":\"" << FontSizeToString(style.fontSize) << "\"";
            jsonStream << ",\"fontColor\":\"" << style.fontColor << "\"";
            jsonStream << ",\"fontOpacity\":" << static_cast<int>(style.fontOpacity);
            jsonStream << ",\"fontEdge\":\"" << FontEdgeToString(style.fontEdge) << "\"";
            jsonStream << ",\"fontEdgeColor\":\"" << style.fontEdgeColor << "\"";
            jsonStream << ",\"backgroundColor\":\"" << style.backgroundColor << "\"";
            jsonStream << ",\"backgroundOpacity\":" << static_cast<int>(style.backgroundOpacity);
            jsonStream << ",\"windowColor\":\"" << style.windowColor << "\"";
            jsonStream << ",\"windowOpacity\":" << static_cast<int>(style.windowOpacity);
            jsonStream << "}";

            mParent.Dispatch("accessibility.onclosedcaptionssettingschanged", jsonStream.str());
        }
                
                // New Method for Set registered
                void SetRegistered(bool state) {
                    std::lock_guard<std::mutex> lock(registerMutex);
                    registered = state;
                }

                // New Method for get registered
                bool GetRegistered() {
                    std::lock_guard<std::mutex> lock(registerMutex);
                    return registered;
                }

                BEGIN_INTERFACE_MAP(NotificationHandler)
                INTERFACE_ENTRY(Exchange::ITextTrackClosedCaptionsStyle::INotification)
                END_INTERFACE_MAP
            private:
                    TextTrackDelegate& mParent;
                    bool registered;
                    std::mutex registerMutex;

        };
        Exchange::ITextTrackClosedCaptionsStyle *mTextTrack;
        PluginHost::IShell* mShell;
        Core::Sink<TextTrackNotificationHandler> mNotificationHandler;

        
};
#endif

