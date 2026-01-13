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

#include "AppGatewayCommon.h"
#include <interfaces/IConfiguration.h>
#include "StringUtils.h"
#include "UtilsFirebolt.h"


#define API_VERSION_NUMBER_MAJOR    APPGATEWAYCOMMON_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    APPGATEWAYCOMMON_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    APPGATEWAYCOMMON_PATCH_VERSION

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::AppGatewayCommon> metadata(
        // Version (Major, Minor, Patch)
        API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
        // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {}
    );
}

namespace Plugin {
    SERVICE_REGISTRATION(AppGatewayCommon, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    AppGatewayCommon::AppGatewayCommon(): mShell(nullptr), mConnectionId(0)
    {
        SYSLOG(Logging::Startup, (_T("AppGatewayCommon Constructor")));
    }

    AppGatewayCommon::~AppGatewayCommon()
    {
        SYSLOG(Logging::Shutdown, (string(_T("AppGatewayCommon Destructor"))));
    }

    /* virtual */ const string AppGatewayCommon::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);

        SYSLOG(Logging::Startup, (_T("AppGatewayCommon::Initialize: PID=%u"), getpid()));

        mShell = service;
        mShell->AddRef();

        // Initialize the settings delegate
        mDelegate = std::make_shared<SettingsDelegate>();
        mDelegate->setShell(mShell);

        return EMPTY_STRING;
    }

    /* virtual */ void AppGatewayCommon::Deinitialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Shutdown, (string(_T("AppGatewayCommon::Deinitialize"))));
        ASSERT(service == mShell);
        mConnectionId = 0;

        mDelegate->Cleanup();
        // Clean up the delegate
        mDelegate.reset();

        mShell->Release();
        mShell = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("AppGatewayCommon de-initialised"))));
    }

    void AppGatewayCommon::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == mConnectionId) {

            ASSERT(mShell != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mShell, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

    Core::hresult AppGatewayCommon::HandleAppEventNotifier(Exchange::IAppNotificationHandler::IEmitter *cb, const string& event /* @in */,
                                    bool listen /* @in */,
                                    bool &status /* @out */) {
            LOGTRACE("HandleFireboltNotifier [event=%s listen=%s]",
                    event.c_str(), listen ? "true" : "false");
            status = true;
            Core::IWorkerPool::Instance().Submit(EventRegistrationJob::Create(this, cb, event, listen));
            return Core::ERROR_NONE;
    }

    Core::hresult AppGatewayCommon::HandleAppGatewayRequest(const Exchange::GatewayContext &context /* @in */,
                                          const string& method /* @in */,
                                          const string &payload /* @in @opaque */,
                                          string& result /*@out @opaque */)
        {
            LOGTRACE("HandleAppGatewayRequest: method=%s, payload=%s, appId=%s",
                    method.c_str(), payload.c_str(), context.appId.c_str());

            std::string lowerMethod = StringUtils::toLower(method);

            // Map method names to handlers
            using MethodHandler = std::function<Core::hresult(AppGatewayCommon*, const Exchange::GatewayContext&, const string&, string&)>;
            static const std::unordered_map<string, MethodHandler> handlers = {
                // Device methods
                {"device.make", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetDeviceMake(r); 
                }},
                {"device.name", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetDeviceName(r); 
                }},
                {"device.setname", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        string name = params.Get("value").String();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetDeviceName(name), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"device.sku", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetDeviceSku(r); 
                }},
                {"device.network", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetInternetConnectionStatus(r); 
                }},
                {"device.version", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetFirmwareVersion(r); 
                }},
                {"device.screenresolution", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetScreenResolution(r); 
                }},
                {"device.videoresolution", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetVideoResolution(r); 
                }},
                {"device.hdcp", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetHdcp(r); 
                }},
                {"device.hdr", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetHdr(r); 
                }},
                {"device.audio", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetAudio(r); 
                }},

                // Localization methods
                {"localization.countrycode", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetCountryCode(r); 
                }},
                {"localization.setcountrycode", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        string countryCode = params.Get("value").String();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetCountryCode(countryCode), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"localization.timezone", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetTimeZone(r); 
                }},
                {"localization.settimezone", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        string timeZone = params.Get("value").String();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetTimeZone(timeZone), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"localization.addadditionalinfo", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) {
                    return ResponseUtils::SetNullResponseForSuccess(self->AddAdditionalInfo(p, r), r);
                }},
                {"localization.language", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetPresentationLanguage(r); 
                }},
                {"localization.locale", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetLocale(r); 
                }},
                {"localization.setlocale", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        string locale = params.Get("value").String();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetLocale(locale), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"localization.preferredaudiolanguages", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetPreferredAudioLanguages(r); 
                }},
                {"localization.setpreferredaudiolanguages", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        string languages = params.Get("value").String();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetPreferredAudioLanguages(languages), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},

                // Second screen methods
                {"secondscreen.friendlyname", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetSecondScreenFriendlyName(r); 
                }},

                // Voice guidance methods
                {"voiceguidance.enabled", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetVoiceGuidance(r); 
                }},
                {"voiceguidance.setenabled", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        bool enabled = params.Get("value").Boolean();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetVoiceGuidance(enabled), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"voiceguidance.speed", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) {
                    double speed;
                    Core::hresult status = self->GetSpeed(speed);
                    if (status == Core::ERROR_NONE) {
                        std::ostringstream jsonStream;
                        jsonStream << speed;
                        r = jsonStream.str();
                    }
                    return status;
                }},
                {"voiceguidance.rate", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) {
                    double speed;
                    Core::hresult status = self->GetSpeed(speed);
                    if (status == Core::ERROR_NONE) {
                        std::ostringstream jsonStream;
                        jsonStream << speed;
                        r = jsonStream.str();
                    }
                    return status;
                }},
                {"voiceguidance.setspeed", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        double speed = params.Get("value").Number();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetSpeed(speed), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"voiceguidance.setrate", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        double speed = params.Get("value").Number();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetSpeed(speed), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"voiceguidance.navigationhints", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetVoiceGuidanceHints(r); 
                }},
                {"voiceguidance.setnavigationhints", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        bool enabled = params.Get("value").Boolean();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetVoiceGuidanceHints(enabled), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"accessibility.voiceguidancesettings", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetVoiceGuidanceSettings(r); 
                }},
                {"accessibility.voiceguidance", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetVoiceGuidanceSettings(r); 
                }},

                // Audio description methods
                {"accessibility.audiodescriptionsettings", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetAudioDescription(r); 
                }},
                {"audiodescriptions.enabled", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetAudioDescriptionsEnabled(r); 
                }},
                {"audiodescriptions.setenabled", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        bool enabled = params.Get("value").Boolean();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetAudioDescriptionsEnabled(enabled), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},

                // Accessibility methods
                {"accessibility.highcontrastui", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetHighContrast(r); 
                }},

                // Closed captions methods
                {"closedcaptions.enabled", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetCaptions(r); 
                }},
                {"closedcaptions.setenabled", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        bool enabled = params.Get("value").Boolean();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetCaptions(enabled), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"closedcaptions.preferredlanguages", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetPreferredCaptionsLanguages(r); 
                }},
                {"closedcaptions.setpreferredlanguages", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string& p, string& r) -> Core::hresult {
                    JsonObject params;
                    if (params.FromString(p)) {
                        string languages = params.Get("value").String();
                        return ResponseUtils::SetNullResponseForSuccess(self->SetPreferredCaptionsLanguages(languages), r);
                    }
                    r = "{\"error\":\"Invalid payload\"}";
                    return Core::ERROR_BAD_REQUEST;
                }},
                {"accessibility.closedcaptions", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetClosedCaptionsSettings(r); 
                }},
                {"accessibility.closedcaptionssettings", [](AppGatewayCommon* self, const Exchange::GatewayContext&, const string&, string& r) { 
                    return self->GetClosedCaptionsSettings(r); 
                }}
            };

            // Find and execute the handler
            auto it = handlers.find(lowerMethod);
            if (it != handlers.end()) {
                return it->second(this, context, payload, result);
            }

            // If method not found, return error
            ErrorUtils::NotSupported(result);
            LOGERR("Unsupported method: %s", method.c_str());
            return Core::ERROR_UNKNOWN_KEY;
        }
    
    Core::hresult AppGatewayCommon::SetName(const string &value /* @in */, string &result)
        {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayCommon::AddAdditionalInfo(const string &value /* @in @opaque */, string &result)
        {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }
        // Delegated alias methods

        Core::hresult AppGatewayCommon::GetDeviceMake(string &make)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceMake(make);
        }

        Core::hresult AppGatewayCommon::GetDeviceName(string &name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceName(name);
        }

        Core::hresult AppGatewayCommon::SetDeviceName(const string name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetDeviceName(name);
        }

        Core::hresult AppGatewayCommon::GetDeviceSku(string &sku)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceSku(sku);
        }

        Core::hresult AppGatewayCommon::GetCountryCode(string &countryCode)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetCountryCode(countryCode);
        }

        Core::hresult AppGatewayCommon::SetCountryCode(const string countryCode)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetCountryCode(countryCode);
        }

        Core::hresult AppGatewayCommon::GetTimeZone(string &timeZone)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetTimeZone(timeZone);
        }

        Core::hresult AppGatewayCommon::SetTimeZone(const string timeZone)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetTimeZone(timeZone);
        }

        Core::hresult AppGatewayCommon::GetSecondScreenFriendlyName(string &name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetSecondScreenFriendlyName(name);
        }

        // UserSettings APIs
        Core::hresult AppGatewayCommon::GetVoiceGuidance(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetVoiceGuidance(result);
        }

        Core::hresult AppGatewayCommon::GetAudioDescription(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetAudioDescription(result);
        }

        Core::hresult AppGatewayCommon::GetAudioDescriptionsEnabled(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetAudioDescriptionsEnabled(result);
        }

        Core::hresult AppGatewayCommon::GetHighContrast(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetHighContrast(result);
        }

        Core::hresult AppGatewayCommon::GetCaptions(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetCaptions(result);
        }

        Core::hresult AppGatewayCommon::GetPresentationLanguage(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPresentationLanguage(result);
        }

        Core::hresult AppGatewayCommon::GetLocale(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetLocale(result);
        }

        Core::hresult AppGatewayCommon::SetLocale(const string &locale)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetLocale(locale);
        }

        Core::hresult AppGatewayCommon::GetPreferredAudioLanguages(string &result)
        {
            if (!mDelegate)
            {
                result = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPreferredAudioLanguages(result);
        }

        Core::hresult AppGatewayCommon::GetPreferredCaptionsLanguages(string &result)
        {
            if (!mDelegate)
            {
                result = "[\"eng\"]";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "[\"eng\"]";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPreferredCaptionsLanguages(result);
        }

        Core::hresult AppGatewayCommon::SetPreferredAudioLanguages(const string &languages)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetPreferredAudioLanguages(languages);
        }

        Core::hresult AppGatewayCommon::SetPreferredCaptionsLanguages(const string &preferredLanguages)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetPreferredCaptionsLanguages(preferredLanguages);
        }

        Core::hresult AppGatewayCommon::SetVoiceGuidance(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetVoiceGuidance(enabled);
        }

        Core::hresult AppGatewayCommon::SetAudioDescriptionsEnabled(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetAudioDescriptionsEnabled(enabled);
        }

        Core::hresult AppGatewayCommon::SetCaptions(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetCaptions(enabled);
        }

        Core::hresult AppGatewayCommon::SetSpeed(const double speed)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform the speed using vg_speed_firebolt2thunder function logic:
            // (if $speed == 2 then 10 elif $speed >= 1.67 then 1.38 elif $speed >= 1.33 then 1.19 elif $speed >= 1 then 1 else 0.1 end)
            double transformedRate;
            if (speed == 2.0)
            {
                transformedRate = 10.0;
            }
            else if (speed >= 1.67)
            {
                transformedRate = 1.38;
            }
            else if (speed >= 1.33)
            {
                transformedRate = 1.19;
            }
            else if (speed >= 1.0)
            {
                transformedRate = 1.0;
            }
            else
            {
                transformedRate = 0.1;
            }

            LOGINFO("SetSpeed: transforming speed %f to rate %f", speed, transformedRate);

            return userSettingsDelegate->SetVoiceGuidanceRate(transformedRate);
        }

        Core::hresult AppGatewayCommon::GetSpeed(double &speed)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            double rate;
            Core::hresult result = userSettingsDelegate->GetVoiceGuidanceRate(rate);

            if (result != Core::ERROR_NONE)
            {
                LOGERR("Failed to get voice guidance rate");
                return result;
            }

            // Transform the rate using vg_speed_thunder2firebolt function logic:
            // (if $speed >= 1.56 then 2 elif $speed >= 1.38 then 1.67 elif $speed >= 1.19 then 1.33 elif $speed >= 1 then 1 else 0.5 end)
            if (rate >= 1.56)
            {
                speed = 2.0;
            }
            else if (rate >= 1.38)
            {
                speed = 1.67;
            }
            else if (rate >= 1.19)
            {
                speed = 1.33;
            }
            else if (rate >= 1.0)
            {
                speed = 1.0;
            }
            else
            {
                speed = 0.5;
            }

            LOGINFO("GetSpeed: transforming rate %f to speed %f", rate, speed);

            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayCommon::GetVoiceGuidanceHints(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetVoiceGuidanceHints(result);
        }

        Core::hresult AppGatewayCommon::SetVoiceGuidanceHints(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetVoiceGuidanceHints(enabled);
        }

        Core::hresult AppGatewayCommon::GetVoiceGuidanceSettings(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get voice guidance settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get voice guidance settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            // Get voice guidance enabled state
            string enabledResult;
            Core::hresult enabledStatus = userSettingsDelegate->GetVoiceGuidance(enabledResult);
            if (enabledStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance enabled state\"}";
                return enabledStatus;
            }

            // Get voice guidance rate (speed)
            double rate;
            Core::hresult rateStatus = userSettingsDelegate->GetVoiceGuidanceRate(rate);
            if (rateStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance rate\"}";
                return rateStatus;
            }

            // Get navigation hints
            string hintsResult;
            Core::hresult hintsStatus = userSettingsDelegate->GetVoiceGuidanceHints(hintsResult);
            if (hintsStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance hints\"}";
                return hintsStatus;
            }

            // Construct the combined JSON response
            // Format: {"enabled": <bool>, "speed": <rate>, "rate": <rate>, "navigationHints": <bool>}
            std::ostringstream jsonStream;
            jsonStream << "{\"enabled\": " << enabledResult
                       << ", \"speed\": " << rate
                       << ", \"rate\": " << rate
                       << ", \"navigationHints\": " << hintsResult << "}";

            result = jsonStream.str();

            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayCommon::GetClosedCaptionsSettings(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get closed captions settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get closed captions settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            // Get closed captions enabled state
            string enabledResult;
            Core::hresult enabledStatus = userSettingsDelegate->GetCaptions(enabledResult);
            if (enabledStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get closed captions enabled state\"}";
                return enabledStatus;
            }

            // Get preferred captions languages
            string languagesResult;
            Core::hresult languagesStatus = userSettingsDelegate->GetPreferredCaptionsLanguages(languagesResult);
            if (languagesStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get preferred captions languages\"}";
                return languagesStatus;
            }

            // Get closed captions styles from UserSettings delegate
            string stylesResult = "{}";
            Core::hresult stylesStatus = userSettingsDelegate->GetClosedCaptionsStyle(stylesResult);
            if (stylesStatus != Core::ERROR_NONE)
            {
                LOGWARN("Couldn't get closed captions styles, using empty object");
                stylesResult = "{}";
            }

            // Construct the combined JSON response
            // Format: {"enabled": <bool>, "preferredLanguages": <array>, "styles": {<style properties>}}
            std::ostringstream jsonStream;
            jsonStream << "{\"enabled\": " << enabledResult
                       << ", \"preferredLanguages\": " << languagesResult
                       << ", \"styles\": " << stylesResult << "}";

            result = jsonStream.str();

            return Core::ERROR_NONE;
        }

        Core::hresult AppGatewayCommon::GetInternetConnectionStatus(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get internet connection status\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto networkDelegate = mDelegate->getNetworkDelegate();
            if (!networkDelegate)
            {
                result = "{\"error\":\"couldn't get internet connection status\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return networkDelegate->GetInternetConnectionStatus(result);
        }

        Core::hresult AppGatewayCommon::GetFirmwareVersion(string &result /* @out */)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetFirmwareVersion(result);
        }

        // Core::hresult AppGatewayCommon::GetScreenResolution(string &result /* out */) {
        //     result = R"([1920,1080])";
        //     return Core::ERROR_NONE;
        // }

        // Core::hresult AppGatewayCommon::GetVideoResolution(string &result /* out */) {
        //     result = R"([1920,1080])";
        //     return Core::ERROR_NONE;
        // }

        Core::hresult AppGatewayCommon::GetScreenResolution(string &result)
        {
            LOGINFO("GetScreenResolution AppGatewayCommon");
            if (!mDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetScreenResolution(result);
        }

        Core::hresult AppGatewayCommon::GetVideoResolution(string &result)
        {
            LOGINFO("GetVideoResolution AppGatewayCommon");
            if (!mDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetVideoResolution(result);
        }

        Core::hresult AppGatewayCommon::GetHdcp(string &result)
        {
            LOGINFO("GetHdcp AppGatewayCommon");
            if (!mDelegate) {
                result = "{\"hdcp1.4\":false,\"hdcp2.2\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "{\"hdcp1.4\":false,\"hdcp2.2\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetHdcp(result);
        }

        Core::hresult AppGatewayCommon::GetHdr(string &result)
        {
            LOGINFO("GetHdr AppGatewayCommon");
            if (!mDelegate) {
                result = "{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetHdr(result);
        }

        Core::hresult AppGatewayCommon::GetAudio(string &result)
        {
            LOGINFO("GetAudio AppGatewayCommon");
            if (!mDelegate) {
                result = "{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetAudio(result);
        }


} // namespace Plugin
} // namespace WPEFramework

