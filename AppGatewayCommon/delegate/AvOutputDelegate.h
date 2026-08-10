/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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

#include <algorithm>
#include <memory>
#include <string>

#include <core/JSON.h>
#include <plugins/plugins.h>

#include "BaseEventDelegate.h"
#include "UtilsController.h"
#include "UtilsJsonrpcDirectLink.h"
#include "UtilsLogging.h"

using namespace WPEFramework;

#ifndef AVOUTPUT_CALLSIGN
#define AVOUTPUT_CALLSIGN "org.rdk.AudioOutput"
#endif

#ifndef CALLSIGN_CALLER_APPGATEWAY_AVOUTPUT
#define CALLSIGN_CALLER_APPGATEWAY_AVOUTPUT "org.rdk.AppGatewayCommon.AvOutputDelegate"
#endif

class AvOutputDelegate : public BaseEventDelegate
{
public:
    static constexpr const char* EVENT_ON_DOLBY_ATMOS_EXPERIENCE_AVAILABLE_CHANGED = "Device.onDolbyAtmosExperienceAvailableChanged";

    explicit AvOutputDelegate(PluginHost::IShell* shell)
        : BaseEventDelegate()
        , _shell(shell)
        , _audioOutputRpc(nullptr)
        , _audioOutputSubscribed(false)
    {
    }

    ~AvOutputDelegate()
    {
        try {
            if (_audioOutputRpc && isAudioOutputSubscribed()) {
                _audioOutputRpc->Unsubscribe(2000, _T("onDolbyAtmosExperienceChanged"));
            }
        } catch (...) {
        }
        _audioOutputRpc.reset();
        _shell = nullptr;
    }

    Core::hresult GetDolbyAtmosExperience(std::string& result)
    {
        bool available = false;
        auto link = AcquireLink(AVOUTPUT_CALLSIGN);
        if (!link) {
            LOGERR("AvOutputDelegate: AudioOutput link unavailable, returning false");
            result = "false";
            return Core::ERROR_UNAVAILABLE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("dolbyAtmosExperience", params, response);
        if (rc != Core::ERROR_NONE) {
            LOGERR("AvOutputDelegate: AudioOutput.dolbyAtmosExperience failed rc=%u", rc);
            result = "false";
            return Core::ERROR_GENERAL;
        }

        (void)ExtractAtmosAvailable(response, available);
        result = available ? "true" : "false";
        return Core::ERROR_NONE;
    }

    bool EmitOnDolbyAtmosExperienceAvailableChanged()
    {
        std::string payload;
        if (GetDolbyAtmosExperience(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|DolbyAtmosExperienceChanged] handler=GetDolbyAtmosExperience failed to compute payload");
            return false;
        }
        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|DolbyAtmosExperienceChanged] handler=GetDolbyAtmosExperience returned empty payload");
            return false;
        }

        Dispatch(EVENT_ON_DOLBY_ATMOS_EXPERIENCE_AVAILABLE_CHANGED, payload);
        return true;
    }

    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter* cb, const std::string& event, const bool listen, bool& registrationError) override
    {
        registrationError = false;
        const std::string evLower = StringUtils::toLower(event);

        if (evLower == "device.ondolbyatmosexperienceavailablechanged") {
            SetupAudioOutputSubscription();
        } else {
            registrationError = true;
            return false;
        }

        if (!registrationError) {
            LOGINFO("[AppGatewayCommon|EventRegistration] event=%s listen=%s", event.c_str(), listen ? "true" : "false");
            if (listen) {
                AddNotification(event, cb);
            } else {
                RemoveNotification(event, cb);
            }
        }

        return true;
    }

private:
    static std::string ToUpper(const std::string& in)
    {
        std::string out = in;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(::toupper(c));
        });
        return out;
    }

    static bool ParseAtmosString(const std::string& value, bool& available)
    {
        if (value.empty()) {
            return false;
        }

        const std::string upper = ToUpper(value);
        if (upper == "ATMOS_AVAILABLE" || upper == "TRUE") {
            available = true;
            return true;
        }
        if (upper == "ATMOS_NOT_AVAILABLE" || upper == "FALSE") {
            available = false;
            return true;
        }
        return false;
    }

    static bool ExtractAtmosAvailable(const Core::JSON::Variant& value, bool& available)
    {
        using Variant = Core::JSON::Variant;
        if (value.Content() == Variant::type::BOOLEAN) {
            available = value.Boolean();
            return true;
        }

        if (value.Content() == Variant::type::STRING) {
            return ParseAtmosString(value.String(), available);
        }

        if (value.Content() == Variant::type::OBJECT) {
            const auto& obj = value.Object();
            if (obj.HasLabel(_T("value")) && ExtractAtmosAvailable(obj.Get(_T("value")), available)) {
                return true;
            }
            if (obj.HasLabel(_T("dolbyAtmosExperience")) && ExtractAtmosAvailable(obj.Get(_T("dolbyAtmosExperience")), available)) {
                return true;
            }
            if (obj.HasLabel(_T("status")) && ExtractAtmosAvailable(obj.Get(_T("status")), available)) {
                return true;
            }
            if (obj.HasLabel(_T("result")) && ExtractAtmosAvailable(obj.Get(_T("result")), available)) {
                return true;
            }
        }

        return false;
    }

    void SetupAudioOutputSubscription()
    {
        if (isAudioOutputSubscribed()) {
            return;
        }

        try {
            if (!_audioOutputRpc) {
                _audioOutputRpc = ::Utils::getThunderControllerClient(AVOUTPUT_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY_AVOUTPUT);
            }

            if (_audioOutputRpc) {
                const uint32_t status = _audioOutputRpc->Subscribe<Core::JSON::VariantContainer>(
                    2000,
                    _T("onDolbyAtmosExperienceChanged"),
                    &AvOutputDelegate::OnDolbyAtmosExperienceChanged,
                    this);

                if (status == Core::ERROR_NONE) {
                    markAudioOutputSubscribed();
                    LOGINFO("AvOutputDelegate: Subscribed to %s.onDolbyAtmosExperienceChanged", AVOUTPUT_CALLSIGN);
                } else {
                    LOGERR("AvOutputDelegate: Failed to subscribe to %s.onDolbyAtmosExperienceChanged rc=%u", AVOUTPUT_CALLSIGN, status);
                }
            }
        } catch (...) {
            LOGERR("AvOutputDelegate: exception during AudioOutput subscription");
        }
    }

    void OnDolbyAtmosExperienceChanged(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|AudioOutput.onDolbyAtmosExperienceChanged] Incoming alias=%s.%s, invoking handlers...",
                AVOUTPUT_CALLSIGN,
                "onDolbyAtmosExperienceChanged");

        const bool emitted = EmitOnDolbyAtmosExperienceAvailableChanged();
        LOGINFO("[AppGatewayCommon|AudioOutput.onDolbyAtmosExperienceChanged] Handler responses: onDolbyAtmosExperienceAvailableChanged=%s",
                emitted ? "emitted" : "skipped");
    }

    bool isAudioOutputSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_audioOutputSubscriptionLock);
        return _audioOutputSubscribed;
    }

    void markAudioOutputSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_audioOutputSubscriptionLock);
        _audioOutputSubscribed = true;
    }

    std::shared_ptr<WPEFramework::Utils::JSONRPCDirectLink> AcquireLink(const std::string& callsign) const
    {
        if (nullptr == _shell) {
            LOGERR("AvOutputDelegate: shell is null");
            return nullptr;
        }
        return WPEFramework::Utils::GetThunderControllerClient(_shell, callsign);
    }

private:
    PluginHost::IShell* _shell;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _audioOutputRpc;
    bool _audioOutputSubscribed;
    mutable Core::CriticalSection _audioOutputSubscriptionLock;
};