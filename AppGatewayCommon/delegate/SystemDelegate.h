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

#pragma once

#include <memory>
#include <string>
#include <unordered_set>

#include <plugins/plugins.h>
#include <core/JSON.h>
#include "UtilsLogging.h"
#include "UtilsJsonrpcDirectLink.h"
#include "UtilsController.h"
#include "BaseEventDelegate.h"
#include <algorithm>
#include "ContextUtils.h"
#include <mutex>
#include <interfaces/ISystemServices.h>


using namespace WPEFramework;

#ifndef CALLSIGN_CALLER_APPGATEWAY
#define CALLSIGN_CALLER_APPGATEWAY "org.rdk.AppGatewayCommon.SystemDelegate"
#endif

// Define a callsign constant to match the AUTHSERVICE_CALLSIGN-style pattern.
#ifndef SYSTEM_CALLSIGN
#define SYSTEM_CALLSIGN "org.rdk.System"
#endif

#ifndef DISPLAYSETTINGS_CALLSIGN
#define DISPLAYSETTINGS_CALLSIGN "org.rdk.DisplaySettings"
#endif

#ifndef HDCPPROFILE_CALLSIGN
#define HDCPPROFILE_CALLSIGN "org.rdk.HdcpProfile"
#endif

// Timeout (ms) for proactive Thunder event subscriptions during construction.
// Override at compile time (e.g. -DSYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS=100)
// to reduce startup latency in environments where Thunder is unavailable.
#ifndef SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS
#define SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS 2000
#endif

class SystemDelegate: public BaseEventDelegate
{
public:

    // Event names exposed by this delegate (consumer subscriptions may vary in case)
    static constexpr const char* EVENT_ON_VIDEO_RES_CHANGED   = "Device.onVideoResolutionChanged";
    static constexpr const char* EVENT_ON_SCREEN_RES_CHANGED  = "Device.onScreenResolutionChanged";
    static constexpr const char* EVENT_ON_HDR_CHANGED         = "Device.onHdrChanged";
    static constexpr const char* EVENT_ON_HDCP_CHANGED        = "Device.onHdcpChanged";
    static constexpr const char* EVENT_ON_AUDIO_CHANGED       = "Device.onAudioChanged";
    static constexpr const char* EVENT_ON_NAME_CHANGED        = "Device.onNameChanged";
    static constexpr const char* EVENT_ON_DEVICE_NAME_CHANGED = "Device.onDeviceNameChanged";
    static constexpr const char* EVENT_ON_TIMEZONE_CHANGED    = "Localization.onTimeZoneChanged";
    static constexpr const char* EVENT_ON_COUNTRY_CHANGED     = "Localization.onCountryChanged";

private:
    class SystemServicesNotification : public Exchange::ISystemServices::INotification
    {
    private:
        SystemServicesNotification(const SystemServicesNotification&) = delete;
        SystemServicesNotification& operator=(const SystemServicesNotification&) = delete;

    public:
        explicit SystemServicesNotification(SystemDelegate& parent)
            : _parent(parent)
        {
        }
        ~SystemServicesNotification() override = default;

    public:
        void OnFriendlyNameChanged(const string& friendlyName) override
        {
            LOGINFO("[AppGatewayCommon|OnFriendlyNameChanged] friendlyName=%s", friendlyName.c_str());
            WPEFramework::Core::JSON::VariantContainer params;
            params[_T("friendlyName")] = friendlyName;
            _parent.OnSystemFriendlyNameChanged(params);
        }

        void OnTimeZoneDSTChanged(const Exchange::ISystemServices::TimeZoneDSTChangedInfo& timeZoneDSTChangedInfo) override
        {
            LOGINFO("[AppGatewayCommon|OnTimeZoneDSTChanged] newTimeZone=%s", timeZoneDSTChangedInfo.newTimeZone.c_str());
            WPEFramework::Core::JSON::VariantContainer params;
            params[_T("oldTimeZone")] = timeZoneDSTChangedInfo.oldTimeZone;
            params[_T("newTimeZone")] = timeZoneDSTChangedInfo.newTimeZone;
            params[_T("oldAccuracy")] = timeZoneDSTChangedInfo.oldAccuracy;
            params[_T("newAccuracy")] = timeZoneDSTChangedInfo.newAccuracy;
            _parent.OnSystemTimezoneChanged(params);
        }

        void OnTerritoryChanged(const Exchange::ISystemServices::TerritoryChangedInfo& territoryChangedInfo) override
        {
            LOGINFO("[AppGatewayCommon|OnTerritoryChanged] newTerritory=%s", territoryChangedInfo.newTerritory.c_str());
            WPEFramework::Core::JSON::VariantContainer params;
            params[_T("oldTerritory")] = territoryChangedInfo.oldTerritory;
            params[_T("newTerritory")] = territoryChangedInfo.newTerritory;
            params[_T("oldRegion")] = territoryChangedInfo.oldRegion;
            params[_T("newRegion")] = territoryChangedInfo.newRegion;
            _parent.OnSystemTimezoneChanged(params);
        }

        BEGIN_INTERFACE_MAP(SystemServicesNotification)
        INTERFACE_ENTRY(Exchange::ISystemServices::INotification)
        END_INTERFACE_MAP
    private:
        SystemDelegate& _parent;
    };

public:
    SystemDelegate(PluginHost::IShell *shell)
        : BaseEventDelegate()
        , _shell(shell)
        , _subscriptions()
        , _displayRpc(nullptr)
        , _hdcpRpc(nullptr)
        , _systemServicesPlugin(nullptr)
        , _systemServicesNotification(*this)
        , _registeredSystemEventHandlers(false)
        , _displaySubscribed(false)
        , _displayAudioSubscribed(false)
        , _hdcpSubscribed(false)
        , _systemSubscribed(false)
        , _timezoneSubscribed(false)
    {
            SetupFriendlyNameSystemSub();
            LOGINFO("SystemDelegate initialized");
    }

    ~SystemDelegate()
    {
        // Cleanup subscriptions
        try {
            if (_displayRpc) {
                if (_displaySubscribed) {
                    _displayRpc->Unsubscribe(2000, _T("resolutionChanged"));
                }
                if (_displayAudioSubscribed) {
                    _displayRpc->Unsubscribe(2000, _T("audioFormatChanged"));
                }
            }
            if (_hdcpRpc && _hdcpSubscribed) {
                _hdcpRpc->Unsubscribe(2000, _T("onDisplayConnectionChanged"));
            }
            if (_systemServicesPlugin != nullptr) {
                if (_registeredSystemEventHandlers) {
                    _systemServicesPlugin->Unregister(&_systemServicesNotification);
                    _registeredSystemEventHandlers = false;
                }
                _systemServicesPlugin->Release();
                _systemServicesPlugin = nullptr;
            }
        } catch (...) {
            // Safe-guard against destructor exceptions
        }
        _displayRpc.reset();
        _hdcpRpc.reset();
        _shell = nullptr;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetDeviceMake(std::string &make)
    {
        /** Retrieve the device make using org.rdk.System.getDeviceInfo */
        LOGINFO("GetDeviceMake AppGatewayCommon Delegate");
        make.clear();
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            make = "unknown";
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::ISystemServices::DeviceInfo deviceInfo;
        // The first GetDeviceInfo parameter is an optional selector/context.
        // Passing nullptr here is the expected way to query the default/current
        // device rather than a specific target.
        const uint32_t rc = sysServices->GetDeviceInfo(nullptr, deviceInfo);
        if (rc == Core::ERROR_NONE && deviceInfo.success && !deviceInfo.make.empty())
        {
            make = deviceInfo.make;
        }
        else
        {
            // Per transform: return_or_else(.result.make, "unknown")
            make = "unknown";
        }
        // Wrap in quotes to make it a valid JSON string
        make = "\"" + make + "\"";
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetDeviceName(std::string &name)
    {
        /** Retrieve the friendly name using org.rdk.System.getFriendlyName */
        name.clear();
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            name = "Living Room";
            return Core::ERROR_UNAVAILABLE;
        }

        string friendlyName;
        bool success = false;
        const uint32_t rc = sysServices->GetFriendlyName(friendlyName, success);
        if (rc == Core::ERROR_NONE && success && !friendlyName.empty())
        {
            name = friendlyName;
        }
        else
        {
            name = "Living Room";
        }
        // Wrap in quotes to make it a valid JSON string
        name = "\"" + name + "\"";
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult SetDeviceName(const std::string &name)
    {
        /** Set the friendly name using org.rdk.System.setFriendlyName */
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::ISystemServices::SystemResult result;
        const uint32_t rc = sysServices->SetFriendlyName(name, result);
        if (rc == Core::ERROR_NONE && result.success)
        {
            return Core::ERROR_NONE;
        }
        LOGERR("SystemDelegate: couldn't set name");
        return Core::ERROR_GENERAL;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetDeviceSku(std::string &skuOut)
    {
        /** Retrieve the device SKU from org.rdk.System.getSystemVersions.stbVersion */
        skuOut.clear();
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::ISystemServices::SystemVersionsInfo systemVersionsInfo;
        const uint32_t rc = sysServices->GetSystemVersions(systemVersionsInfo);
        if (rc != Core::ERROR_NONE || !systemVersionsInfo.success)
        {
            LOGERR("SystemDelegate: GetSystemVersions failed rc=%u success=%d", rc, systemVersionsInfo.success);
            return Core::ERROR_UNAVAILABLE;
        }
        if (systemVersionsInfo.stbVersion.empty())
        {
            LOGERR("SystemDelegate: GetSystemVersions returned empty stbVersion");
            return Core::ERROR_UNAVAILABLE;
        }
        // Per transform: split("_")[0]
        auto pos = systemVersionsInfo.stbVersion.find('_');
        skuOut = (pos == std::string::npos) ? systemVersionsInfo.stbVersion : systemVersionsInfo.stbVersion.substr(0, pos);
        if (skuOut.empty())
        {
            LOGERR("SystemDelegate: Failed to get SKU");
            return Core::ERROR_UNAVAILABLE;
        }
        // Wrap in quotes to make it a valid JSON string
        skuOut = "\"" + skuOut + "\"";
        return Core::ERROR_NONE;
    }

    Core::hresult GetFirmwareVersion(std::string &version)
    {
        mAdminLock.Lock();
        version = mVersionResponse;
        mAdminLock.Unlock();
        
        if (!version.empty()) {
            return Core::ERROR_NONE;
        }

        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        Exchange::ISystemServices::SystemVersionsInfo systemVersionsInfo;
        const uint32_t rc = sysServices->GetSystemVersions(systemVersionsInfo);
        if (rc != Core::ERROR_NONE || !systemVersionsInfo.success)
        {
            LOGERR("SystemDelegate: GetSystemVersions failed rc=%u success=%d", rc, systemVersionsInfo.success);
            return Core::ERROR_UNAVAILABLE;
        }
        if (systemVersionsInfo.receiverVersion.empty()) {
            LOGERR("SystemDelegate: GetSystemVersions returned empty receiverVersion");
            return Core::ERROR_UNAVAILABLE;
        }
        std::string receiverVersion = systemVersionsInfo.receiverVersion;
        std::string stbVersion = systemVersionsInfo.stbVersion;
        if (stbVersion.empty())
        {
            LOGERR("SystemDelegate: Failed to get STB Version");
            return Core::ERROR_UNAVAILABLE;
        }

        // receiver version is typically in 99.99.15.07 format need to set extract the first three parts only for major.minor.patch
        // if receiverversion is not in number format return error
        uint32_t major;
        uint32_t minor;
        uint32_t patch;

        if (sscanf(receiverVersion.c_str(), "%u.%u.%u", &major, &minor, &patch) != 3)
        {
            LOGERR("SystemDelegate: receiverVersion is not in number format");
            return Core::ERROR_UNAVAILABLE;
        }

        JsonObject versionObj;
        JsonObject api;
        api["major"] = 1;
        api["minor"] = 7;
        api["patch"] = 0;
        api["readable"] = "Firebolt API v1.7.0";

        JsonObject firmwareInfo;
        firmwareInfo["major"] = major;
        firmwareInfo["minor"] = minor;
        firmwareInfo["patch"] = patch;
        firmwareInfo["readable"] = stbVersion;
        // Build this json data structure {"api":{"major":1,"minor":7,"patch":0,"readable":"Firebolt API v1.7.0"},"firmware":{"major":99,"minor":99,"patch":15,"readable":"SKXI11ADS_MIDDLEWARE_DEV_develop_20251101123542_TEST_CD"},"os":{"major":99,"minor":99,"patch":15,"readable":"SKXI11ADS_MIDDLEWARE_DEV_develop_20251101123542_TEST_CD"},"debug":"4.0.0"}
        versionObj["api"] = api;
        versionObj["firmware"] = firmwareInfo;
        versionObj["os"] = firmwareInfo;
        versionObj["debug"] = "4.0.0";

        mAdminLock.Lock();
        versionObj.ToString(mVersionResponse);
        version = mVersionResponse;
        mAdminLock.Unlock();

        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetCountryCode(std::string &code)
    {
        /** Retrieve Firebolt country code derived from org.rdk.System.getTerritory */
        code.clear();
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            code = "US";
            return Core::ERROR_UNAVAILABLE;
        }

        string territory;
        string region;
        bool success = false;
        const uint32_t rc = sysServices->GetTerritory(territory, region, success);
        if (rc == Core::ERROR_NONE && success)
        {
            code = TerritoryThunderToFirebolt(territory, "");
        }
        // Wrap in quotes to make it a valid JSON string
        code = "\"" + code + "\"";
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult SetCountryCode(const std::string &code)
    {
        /** Set territory using org.rdk.System.setTerritory mapped from Firebolt country code */
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        const std::string territory = TerritoryFireboltToThunder(code, "USA");
        const std::string region = "";  // Empty region as it's not used in current implementation
        Exchange::ISystemServices::SystemError error;
        bool success = false;
        const uint32_t rc = sysServices->SetTerritory(territory, region, error, success);
        if (rc == Core::ERROR_NONE && success)
        {
            return Core::ERROR_NONE;
        }
        LOGERR("SystemDelegate: couldn't set countrycode");
        return Core::ERROR_GENERAL;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetTimeZone(std::string &tz)
    {
        /** Retrieve timezone using org.rdk.System.getTimeZoneDST */
        tz.clear();
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        string timeZone;
        string accuracy;
        bool success = false;
        const uint32_t rc = sysServices->GetTimeZoneDST(timeZone, accuracy, success);
        if (rc == Core::ERROR_NONE && success)
        {
            tz = timeZone;
            // Wrap in quotes to make it a valid JSON string
            tz = "\"" + tz + "\"";
            return Core::ERROR_NONE;
        }
        LOGERR("SystemDelegate: couldn't get timezone");
        return Core::ERROR_UNAVAILABLE;
    }

    // PUBLIC_INTERFACE
    Core::hresult SetTimeZone(const std::string &tz)
    {
        /** Set timezone using org.rdk.System.setTimeZoneDST */
        auto sysServices = AcquireSystemServices();
        if (!sysServices)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        const std::string accuracy = "";
        uint32_t serviceStatus = 0;
        string errorMessage;
        bool success = false;
        const uint32_t rc = sysServices->SetTimeZoneDST(tz, accuracy, serviceStatus, errorMessage, success);
        if (rc == Core::ERROR_NONE && success)
        {
            return Core::ERROR_NONE;
        }
        LOGERR("SystemDelegate: couldn't set timezone, error: %s", errorMessage.c_str());
        return Core::ERROR_GENERAL;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetSecondScreenFriendlyName(std::string &name)
    {
        /** Alias to GetDeviceName using org.rdk.System.getFriendlyName */
        return GetDeviceName(name);
    }

    // PUBLIC_INTERFACE
    Core::hresult GetScreenResolution(std::string &jsonArray)
    {
        /**
         * Get [w, h] screen resolution using DisplaySettings.getCurrentResolution.
         * Returns "[1920,1080]" as fallback when unavailable.
         */
        LOGDBG("[AppGatewayCommon|GetScreenResolution] Invoked");
        jsonArray = "[1920,1080]";
        auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
        if (!link) {
            LOGERR("[AppGatewayCommon|GetScreenResolution] DisplaySettings link unavailable, returning default %s", jsonArray.c_str());
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getCurrentResolution", params, response);
        if (rc != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|GetScreenResolution] getCurrentResolution failed rc=%u, returning default %s", rc, jsonArray.c_str());
            return Core::ERROR_GENERAL;
        }

        int w = 1920, h = 1080;

        // Try top-level first
        if (response.HasLabel(_T("w")) && response.HasLabel(_T("h"))) {
            auto wv = response.Get(_T("w"));
            auto hv = response.Get(_T("h"));
            if (wv.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER &&
                hv.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER) {
                w = static_cast<int>(wv.Number());
                h = static_cast<int>(hv.Number());
            }
        } else if (response.HasLabel(_T("result"))) {
            // Try nested "result"
            auto r = response.Get(_T("result"));
            if (r.Content() == WPEFramework::Core::JSON::Variant::type::OBJECT) {
                auto wnv = r.Object().Get(_T("w"));
                auto hnv = r.Object().Get(_T("h"));
                auto wdv = r.Object().Get(_T("width"));
                auto hdv = r.Object().Get(_T("height"));

                if (wnv.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER &&
                    hnv.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER) {
                    w = static_cast<int>(wnv.Number());
                    h = static_cast<int>(hnv.Number());
                } else if (wdv.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER &&
                           hdv.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER) {
                    w = static_cast<int>(wdv.Number());
                    h = static_cast<int>(hdv.Number());
                }
            }
        }

        jsonArray = "[" + std::to_string(w) + "," + std::to_string(h) + "]";
        LOGDBG("[AppGatewayCommon|GetScreenResolution] Computed screenResolution: w=%d h=%d -> %s", w, h, jsonArray.c_str());
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetVideoResolution(std::string &jsonArray)
    {
        /**
         * Get [w, h] video resolution. Prefer DisplaySettings.getCurrentResolution width
         * to infer UHD vs FHD; else default to 1080p.
         * This is a stubbed approximation of the /system/hdmi.uhdConfigured logic.
         */
        std::string sr;
        (void)GetScreenResolution(sr);
        // sr expected format: "[w,h]"
        int w = 1920, h = 1080;
        if (sr.size() > 2 && sr.front() == '[' && sr.back() == ']') {
            try {
                // Simple parsing without heavy JSON dependencies
                auto comma = sr.find(',');
                if (comma != std::string::npos) {
                    int sw = std::stoi(sr.substr(1, comma - 1));
                    int sh = std::stoi(sr.substr(comma + 1, sr.size() - comma - 2));
                    if (sw >= 3840 || sh >= 2160) {
                        w = 3840; h = 2160;
                    } else {
                        w = 1920; h = 1080;
                    }
                    LOGDBG("[AppGatewayCommon|GetVideoResolution] Transform screen(%d x %d) -> video(%d x %d)", sw, sh, w, h);
                }
            } catch (...) {
                // keep defaults
                LOGDBG("[AppGatewayCommon|GetVideoResolution] Transform parse error for %s -> using defaults (%d x %d)", sr.c_str(), w, h);
            }
        }
        jsonArray = "[" + std::to_string(w) + "," + std::to_string(h) + "]";
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetHdcp(std::string &jsonObject)
    {
        /**
         * Get HDCP status via HdcpProfile.getHDCPStatus.
         * Return {"hdcp1.4":bool,"hdcp2.2":bool} with sensible defaults.
         */
        jsonObject = "{\"hdcp1.4\":false,\"hdcp2.2\":false}";
        LOGDBG("[AppGatewayCommon|GetHdcp] Invoked");
        auto link = AcquireLink(HDCPPROFILE_CALLSIGN);
        if (!link) {
            LOGERR("[AppGatewayCommon|GetHdcp] HdcpProfile link unavailable, returning default %s", jsonObject.c_str());
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getHDCPStatus", params, response);
        if (rc != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|GetHdcp] getHDCPStatus failed rc=%u, returning default %s", rc, jsonObject.c_str());
            return Core::ERROR_GENERAL;
        }

        bool hdcp14 = false;
        bool hdcp22 = false;

        // Prefer nested "result" if available
        if (response.HasLabel(_T("result"))) {
            auto r = response.Get(_T("result"));
            if (r.Content() == WPEFramework::Core::JSON::Variant::type::OBJECT) {
                auto succ = r.Object().Get(_T("success"));
                auto status = r.Object().Get(_T("HDCPStatus"));
                if (succ.Content() == WPEFramework::Core::JSON::Variant::type::BOOLEAN && succ.Boolean() &&
                    status.Content() == WPEFramework::Core::JSON::Variant::type::OBJECT) {
                    auto reason = status.Object().Get(_T("hdcpReason"));
                    auto version = status.Object().Get(_T("currentHDCPVersion"));
                    if (reason.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER &&
                        static_cast<int>(reason.Number()) == 2 &&
                        version.Content() == WPEFramework::Core::JSON::Variant::type::STRING) {
                        const std::string v = version.String();
                        if (v == "1.4") { hdcp14 = true; }
                        else { hdcp22 = true; }
                    }
                }
            }
        } else {
            // Fallback: try top-level fields if present
            auto status = response.Get(_T("HDCPStatus"));
            if (status.Content() == WPEFramework::Core::JSON::Variant::type::OBJECT) {
                auto reason = status.Object().Get(_T("hdcpReason"));
                auto version = status.Object().Get(_T("currentHDCPVersion"));
                if (reason.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER &&
                    static_cast<int>(reason.Number()) == 2 &&
                    version.Content() == WPEFramework::Core::JSON::Variant::type::STRING) {
                    const std::string v = version.String();
                    if (v == "1.4") { hdcp14 = true; }
                    else { hdcp22 = true; }
                }
            }
        }

        jsonObject = std::string("{\"hdcp1.4\":") + (hdcp14 ? "true" : "false")
                   + ",\"hdcp2.2\":" + (hdcp22 ? "true" : "false") + "}";
        LOGDBG("[AppGatewayCommon|GetHdcp] Computed HDCP flags: hdcp1.4=%s hdcp2.2=%s -> %s",
               hdcp14 ? "true" : "false", hdcp22 ? "true" : "false", jsonObject.c_str());
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetHdr(std::string &jsonObject)
    {
        /**
         * Retrieve HDR capability/state via DisplaySettings.getTVHDRCapabilities.
         * Returns object with hdr10, dolbyVision, hlg, hdr10Plus flags (defaults false).
         */
        jsonObject = "{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}";
        LOGDBG("[AppGatewayCommon|GetHdr] Invoked");
        auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
        if (!link) {
            LOGERR("[AppGatewayCommon|GetHdr] DisplaySettings link unavailable, returning default %s", jsonObject.c_str());
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getTVHDRCapabilities", params, response);
        if (rc != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|GetHdr] getTVHDRCapabilities failed rc=%u, returning default %s", rc, jsonObject.c_str());
            return Core::ERROR_GENERAL;
        }

        bool hdr10 = false, dv = false, hlg = false, hdr10plus = false;

        // Parse HDR capabilities bitmask
        // HDRSTANDARD_NONE = 0x0
        // HDRSTANDARD_HDR10 = 0x1
        // HDRSTANDARD_HLG = 0x2
        // HDRSTANDARD_DolbyVision = 0x4
        // HDRSTANDARD_TechnicolorPrime = 0x8
        // HDRSTANDARD_HDR10PLUS = 0x10
        // HDRSTANDARD_SDR = 0x20

        auto parseCapabilities = [&](const WPEFramework::Core::JSON::Variant& vobj) {
            uint32_t capabilities = 0;

            // For ex. if DisplaySettings returns: {"capabilities":32,"success":true}
            // extract the "capabilities" field from the object
            if (vobj.Content() == WPEFramework::Core::JSON::Variant::type::OBJECT) {
                auto caps = vobj.Object().Get(_T("capabilities"));
                if (caps.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER) {
                    capabilities = static_cast<uint32_t>(caps.Number());
                    LOGDBG("[AppGatewayCommon|GetHdr] Got capabilities from object: 0x%x (%d)",
                           capabilities, capabilities);
                }
            }

            // Parse bitmask - always parse, even if 0 (HDRSTANDARD_NONE is valid)
            hdr10     = (capabilities & 0x01) != 0;  // HDRSTANDARD_HDR10
            hlg       = (capabilities & 0x02) != 0;  // HDRSTANDARD_HLG
            dv        = (capabilities & 0x04) != 0;  // HDRSTANDARD_DolbyVision
            hdr10plus = (capabilities & 0x10) != 0;  // HDRSTANDARD_HDR10PLUS
            LOGDBG("[AppGatewayCommon|GetHdr] Parsed capabilities bitmask: 0x%x -> hdr10=%d hlg=%d dv=%d hdr10plus=%d",
                   capabilities, hdr10, hlg, dv, hdr10plus);
        };

        // Response is at top level: {"capabilities":32,"success":true}
        parseCapabilities(response);

        jsonObject = std::string("{\"hdr10\":") + (hdr10 ? "true" : "false")
                   + ",\"dolbyVision\":" + (dv ? "true" : "false")
                   + ",\"hlg\":" + (hlg ? "true" : "false")
                   + ",\"hdr10Plus\":" + (hdr10plus ? "true" : "false") + "}";
        LOGDBG("[AppGatewayCommon|GetHdr] Computed HDR flags: hdr10=%s dolbyVision=%s hlg=%s hdr10Plus=%s -> %s",
               hdr10 ? "true" : "false",
               dv ? "true" : "false",
               hlg ? "true" : "false",
               hdr10plus ? "true" : "false",
               jsonObject.c_str());
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
     Core::hresult GetAudio(std::string &jsonObject)
     {
         /**
          * Retrieve supported audio formats from DisplaySettings.getAudioFormat(audioPort: "HDMI0") and
          * compute flags from supportedAudioFormat array only. Multiple true values are allowed.
          * Flags:
          *  - stereo: true if a token contains "PCM" or "STEREO"
          *  - dolbyDigital5.1: true if a token contains "AC3" or "DOLBY AC3"
          *  - dolbyDigital5.1+: true if a token contains any of "EAC3", "DD+", or "AC4"
          *  - dolbyAtmos: true if a token contains "ATMOS"
          */
         bool stereo = false;
         bool dd51 = false;
         bool dd51p = false;
         bool atmos = false;

         auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
         if (!link) {
             LOGERR("[AppGatewayCommon|GetAudio] DisplaySettings link unavailable, returning default audio flags");
             jsonObject = "{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}";
             return Core::ERROR_UNAVAILABLE;
         }

         const std::string paramsStr = "{\"audioPort\":\"HDMI0\"}";
         std::string response;
         const Core::hresult rc = link->Invoke<std::string, std::string>("getAudioFormat", paramsStr, response);
         if (rc != Core::ERROR_NONE) {
             jsonObject = "{\"stereo\":false,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}";
             return Core::ERROR_GENERAL;
         }

         WPEFramework::Core::JSON::VariantContainer v;
         WPEFramework::Core::OptionalType<WPEFramework::Core::JSON::Error> error;
         if (v.FromString(response, error)) {
             WPEFramework::Core::JSON::Variant supported;
             if (v.HasLabel(_T("result"))) {
                 auto r = v.Get(_T("result"));
                 if (r.Content() == WPEFramework::Core::JSON::Variant::type::OBJECT) {
                     supported = r.Object().Get(_T("supportedAudioFormat"));
                 }
             }
             if (supported.Content() != WPEFramework::Core::JSON::Variant::type::ARRAY) {
                 supported = v.Get(_T("supportedAudioFormat"));
             }
             // Aggregate flags only from supportedAudioFormat
             (void)SetFlagsFromSupported(supported, stereo, dd51, dd51p, atmos);
         }

         jsonObject = std::string("{\"stereo\":") + (stereo ? "true" : "false")
                    + ",\"dolbyDigital5.1\":" + (dd51 ? "true" : "false")
                    + ",\"dolbyDigital5.1+\":" + (dd51p ? "true" : "false")
                    + ",\"dolbyAtmos\":" + (atmos ? "true" : "false") + "}";
         return Core::ERROR_NONE;
     }

     /**
      * Helper: Parse supportedAudioFormat array and set flags. Returns true iff an array was found
      * and at least one recognized token was matched. Tokens are matched case-insensitively:
      * - stereo: contains "PCM" or "STEREO"
      * - dolbyDigital5.1: contains "AC3" or "DOLBY AC3" or "DOLBY DIGITAL"
      * - dolbyDigital5.1+: contains "EAC3" or "DD+" or "DOLBY DIGITAL PLUS" or "AC4"
      * - dolbyAtmos: contains "ATMOS"
      */
     static bool SetFlagsFromSupported(const WPEFramework::Core::JSON::Variant& supportedNode,
                                       bool& stereo, bool& dd51, bool& dd51p, bool& atmos)
     {
         using Var = WPEFramework::Core::JSON::Variant;
         bool anyRecognized = false;
         if (supportedNode.Content() == Var::type::ARRAY) {
             auto arr = supportedNode.Array();
             const uint16_t n = arr.Length();
             for (uint16_t i = 0; i < n; ++i) {
                 std::string token = arr[i].String();
                 if (token.empty()) {
                     continue;
                 }
                 std::string u = token;
                 std::transform(u.begin(), u.end(), u.begin(), [](unsigned char c){ return static_cast<char>(::toupper(c)); });

                 // Stereo detection
                 if (u.find("PCM") != std::string::npos || u.find("STEREO") != std::string::npos) {
                     stereo = true; anyRecognized = true;
                 }
                 // Dolby Digital (AC3)
                 // Only match "AC3" if not preceded by 'E' (i.e., not "EAC3")
                 bool isAC3 = false;
                 // Check for "AC3" not preceded by 'E'
                 size_t ac3_pos = u.find("AC3");
                 if (ac3_pos != std::string::npos) {
                     if (ac3_pos == 0 || u[ac3_pos - 1] != 'E') {
                         isAC3 = true;
                     }
                 }
                 if (isAC3 || u.find("DOLBY AC3") != std::string::npos || u.find("DOLBY DIGITAL") != std::string::npos) {
                     dd51 = true; anyRecognized = true;
                 }
                 // Dolby Digital Plus (EAC3/DD+/AC4)
                 if (u.find("EAC3") != std::string::npos || u.find("DD+") != std::string::npos || u.find("DOLBY DIGITAL PLUS") != std::string::npos || u.find("AC4") != std::string::npos) {
                     dd51p = true; anyRecognized = true;
                 }
                 // Atmos (any transport)
                 if (u.find("ATMOS") != std::string::npos) {
                     atmos = true; anyRecognized = true;
                 }
             }
         }
         return anyRecognized;
     }

    // ---- Event exposure (Emit helpers) ----

    // PUBLIC_INTERFACE
    bool EmitOnVideoResolutionChanged()
    {
        std::string payload;
        if (GetVideoResolution(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|VideoResolutionChanged] handler=GetVideoResolution failed to compute payload");
            return false;
        }
        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|VideoResolutionChanged] handler=GetVideoResolution returned empty payload");
            return false;
        }
        Dispatch(EVENT_ON_VIDEO_RES_CHANGED, payload);
            return true;
    }

    // PUBLIC_INTERFACE
    bool EmitOnScreenResolutionChanged()
    {
        std::string payload;
        if (GetScreenResolution(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|ScreenResolutionChanged] handler=GetScreenResolution failed to compute payload");
            return false;
        }

        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|ScreenResolutionChanged] handler=GetScreenResolution returned empty payload");
            return false;
        }
        
        Dispatch(EVENT_ON_SCREEN_RES_CHANGED, payload);
        return true;

    }

    // PUBLIC_INTERFACE
    bool EmitOnHdcpChanged()
    {
        std::string payload;
        if (GetHdcp(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|HdcpChanged] handler=GetHdcp failed to compute payload");
            return false;
        }

        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|HdcpChanged] handler=GetHdcp returned empty payload");
            return false;
        }

        Dispatch(EVENT_ON_HDCP_CHANGED, payload);
        return true;

    }

    // PUBLIC_INTERFACE
    bool EmitOnHdrChanged()
    {
        std::string payload;
        if (GetHdr(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|HdrChanged] handler=GetHdr failed to compute payload");
            return false;
        }

        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|HdrChanged] handler=GetHdr returned empty payload");
            return false;
        }

        // Legacy payload 
        Dispatch(EVENT_ON_HDR_CHANGED, payload);
        
        return true;

    }

    // PUBLIC_INTERFACE
    bool EmitOnNameChanged()
    {
        std::string payload;
        if (GetDeviceName(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|NameChanged] handler=GetDeviceName failed to compute payload");
            return false;
        }

        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|NameChanged] handler=GetDeviceName returned empty payload");
            return false;
        }

        Dispatch(EVENT_ON_NAME_CHANGED, payload);
        Dispatch(EVENT_ON_DEVICE_NAME_CHANGED, payload);
        return true;
    }

    // PUBLIC_INTERFACE
    bool EmitOnAudioChanged()
    {
        std::string payload;
        if (GetAudio(payload) != Core::ERROR_NONE) {
            LOGERR("[AppGatewayCommon|AudioChanged] handler=GetAudio failed to compute payload");
            return false;
        }

        if (payload.empty()) {
            LOGERR("[AppGatewayCommon|AudioChanged] handler=GetAudio returned empty payload");
            return false;
        }

        Dispatch(EVENT_ON_AUDIO_CHANGED, payload);
        return true;
    }

    // PUBLIC_INTERFACE
    bool EmitOnTimezoneChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        if (!params.HasLabel(_T("newTimeZone"))) {
            LOGERR("[AppGatewayCommon|TimezoneChanged] missing newTimeZone parameter in event");
            return false;
        }

        const std::string newTz =  params[_T("newTimeZone")].String();
        if (newTz.empty()) {
            LOGERR("[AppGatewayCommon|TimezoneChanged] newTimeZone parameter is empty");
            return false;
        }
        Dispatch(EVENT_ON_TIMEZONE_CHANGED, "\"" + newTz + "\"");
        return true;
    }

    bool EmitOnTerritoryChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        if (!params.HasLabel(_T("newTerritory"))) {
            LOGERR("[AppGatewayCommon|TerritoryChanged] missing newTerritory parameter in event");
            return false;
        }

        const std::string newTerritory =  params[_T("newTerritory")].String();
        if (newTerritory.empty()) {
            LOGERR("[AppGatewayCommon|TerritoryChanged] newTerritory parameter is empty");
            return false;
        }
        const std::string code = TerritoryThunderToFirebolt(newTerritory, "");

        if (code.empty()) {
            LOGERR("[AppGatewayCommon|TerritoryChanged] unrecognized territory code %s in event", newTerritory.c_str());
            return false;
        }

        JsonObject object;
        object["value"] = code;
        string result;
        object.ToString(result);

        Dispatch(EVENT_ON_COUNTRY_CHANGED, result);
        return true;
    }

    // ---- AppNotifications registration hook ----
    // Called by SettingsDelegate when app subscribes/unsubscribes to events.
    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const std::string &event, const bool listen, bool &registrationError)
    {
        registrationError = false;

        const std::string evLower = ToLower(event);

        if (evLower == "device.onvideoresolutionchanged" || evLower == "device.onscreenresolutionchanged") {
            SetupDisplaySettingsSubscription();
        } else if (evLower == "device.onhdcpchanged" || evLower == "device.onhdrchanged") {
            SetupHdcpProfileSubscription();
        } else if (evLower == "device.onaudiochanged") {
            SetupDisplaySettingsAudioSubscription();
        } else if (evLower == "device.ondevicenamechanged" || evLower == "device.onnamechanged") {
            SetupFriendlyNameSystemSub();
        } else if (evLower == "localization.ontimezonechanged") {
            SetupTimezoneSystemSub();
        } else if (evLower == "localization.oncountrychanged") {
            SetupCountrySystemSub();
        } else {
            registrationError = true; // event not recognized - signal error to caller
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
    inline std::shared_ptr<WPEFramework::Utils::JSONRPCDirectLink> AcquireLink(const std::string& callsign) const
    {
        // Create a direct JSON-RPC link to the specified Thunder plugin using the Supporting_Files helper.
        if (_shell == nullptr)
        {
            LOGERR("SystemDelegate: shell is null");
            return nullptr;
        }
        return WPEFramework::Utils::GetThunderControllerClient(_shell, callsign);
    }

    inline Exchange::ISystemServices* AcquireSystemServices() const
    {
        if (_systemServicesPlugin == nullptr && _shell != nullptr)
        {
            const_cast<SystemDelegate*>(this)->_systemServicesPlugin = 
                _shell->QueryInterfaceByCallsign<Exchange::ISystemServices>(SYSTEM_CALLSIGN);
            if (_systemServicesPlugin != nullptr)
            {
                LOGINFO("SystemDelegate: Successfully acquired ISystemServices interface");
            }
            else
            {
                LOGERR("SystemDelegate: Failed to acquire ISystemServices interface");
            }
        }
        return _systemServicesPlugin;
    }
    
    static std::string ToLower(const std::string &in)
    {
        std::string out;
        out.reserve(in.size());
        for (char c : in)
        {
            out.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
        }
        return out;
    }

    static std::string TerritoryThunderToFirebolt(const std::string &terr, const std::string &deflt)
    {
        if (EqualsIgnoreCase(terr, "USA"))
            return "US";
        if (EqualsIgnoreCase(terr, "CAN"))
            return "CA";
        if (EqualsIgnoreCase(terr, "ITA"))
            return "IT";
        if (EqualsIgnoreCase(terr, "GBR"))
            return "GB";
        if (EqualsIgnoreCase(terr, "IRL"))
            return "IE";
        if (EqualsIgnoreCase(terr, "AUS"))
            return "AU";
        if (EqualsIgnoreCase(terr, "AUT"))
            return "AT";
        if (EqualsIgnoreCase(terr, "CHE"))
            return "CH";
        if (EqualsIgnoreCase(terr, "DEU"))
            return "DE";
        return deflt;
    }

    static std::string TerritoryFireboltToThunder(const std::string &code, const std::string &deflt)
    {
        if (EqualsIgnoreCase(code, "US"))
            return "USA";
        if (EqualsIgnoreCase(code, "CA"))
            return "CAN";
        if (EqualsIgnoreCase(code, "IT"))
            return "ITA";
        if (EqualsIgnoreCase(code, "GB"))
            return "GBR";
        if (EqualsIgnoreCase(code, "IE"))
            return "IRL";
        if (EqualsIgnoreCase(code, "AU"))
            return "AUS";
        if (EqualsIgnoreCase(code, "AT"))
            return "AUT";
        if (EqualsIgnoreCase(code, "CH"))
            return "CHE";
        if (EqualsIgnoreCase(code, "DE"))
            return "DEU";
        return deflt;
    }

    static bool EqualsIgnoreCase(const std::string &a, const std::string &b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (::tolower(static_cast<unsigned char>(a[i])) != ::tolower(static_cast<unsigned char>(b[i])))
            {
                return false;
            }
        }
        return true;
    }
    // Setup subscriptions to underlying Thunder plugin events
    void SetupDisplaySettingsSubscription()
    {
        if (isDisplaySubscribed()) return;
        try {
            if (!_displayRpc) {
                _displayRpc = ::Utils::getThunderControllerClient(DISPLAYSETTINGS_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY);
            }
            if (_displayRpc) {
                const uint32_t status = _displayRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS, _T("resolutionChanged"), &SystemDelegate::OnDisplaySettingsResolutionChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.resolutionChanged", DISPLAYSETTINGS_CALLSIGN);
                    markDisplaySubscribed();
                } else {
                    LOGERR("SystemDelegate: Failed to subscribe to %s.resolutionChanged rc=%u", DISPLAYSETTINGS_CALLSIGN, status);
                }
            }
        } catch (...) {
            LOGERR("SystemDelegate: exception during DisplaySettings (resolution) subscription");
        }
    }

    void SetupDisplaySettingsAudioSubscription()
    {
        if (isDisplayAudioSubscribed()) return;
        try {
            if (!_displayRpc) {
                _displayRpc = ::Utils::getThunderControllerClient(DISPLAYSETTINGS_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY);
            }
            if (_displayRpc) {
                const uint32_t status = _displayRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS, _T("audioFormatChanged"), &SystemDelegate::OnDisplaySettingsAudioFormatChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.audioFormatChanged", DISPLAYSETTINGS_CALLSIGN);
                    markDisplayAudioSubscribed();
                } else {
                    LOGERR("SystemDelegate: Failed to subscribe to %s.audioFormatChanged rc=%u", DISPLAYSETTINGS_CALLSIGN, status);
                }
            }
        } catch (...) {
            LOGERR("SystemDelegate: exception during DisplaySettings (audio) subscription");
        }
    }

    void SetupHdcpProfileSubscription()
    {
        if (isHdcpSubscribed()) return;
        try {
            if (!_hdcpRpc) {
                _hdcpRpc = ::Utils::getThunderControllerClient(HDCPPROFILE_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY);
            }
            if (_hdcpRpc) {
                const uint32_t status = _hdcpRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    SYSTEM_DELEGATE_SUBSCRIBE_TIMEOUT_MS, _T("onDisplayConnectionChanged"), &SystemDelegate::OnHdcpProfileDisplayConnectionChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.onDisplayConnectionChanged", HDCPPROFILE_CALLSIGN);
                    markHdcpSubscribed();
                } else {
                    LOGERR("SystemDelegate: Failed to subscribe to %s.onDisplayConnectionChanged rc=%u", HDCPPROFILE_CALLSIGN, status);
                }
            }
        } catch (...) {
            LOGERR("SystemDelegate: exception during HdcpProfile subscription");
        }
    }

    void SetupFriendlyNameSystemSub()
    {
        if (isSystemRegistered()) return;
        try {
            auto sysServices = AcquireSystemServices();
            if (sysServices) {
                const uint32_t status = sysServices->Register(&_systemServicesNotification);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Registered ISystemServices notifications (FriendlyName + TimeZone + Country)");
                    markSystemRegistered();
                } else {
                    LOGERR("SystemDelegate: Failed to register ISystemServices notifications rc=%u", status);
                }
            }
        } catch (...) {
            LOGERR("SystemDelegate: exception during ISystemServices registration");
        }
    }

    void SetupTimezoneSystemSub()
    {
        // Both FriendlyName,Timezone and Country use the same ISystemServices registration
        SetupFriendlyNameSystemSub();
    }

    void SetupCountrySystemSub()
    {
        // FriendlyName,Timezone and Country use the same ISystemServices registration
        SetupFriendlyNameSystemSub();
    }

    // Event handlers invoked by Thunder JSON-RPC subscription
    void OnDisplaySettingsResolutionChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|DisplaySettings.resolutionChanged] Incoming alias=%s.%s, invoking handlers...",
                DISPLAYSETTINGS_CALLSIGN, "resolutionChanged");
        // Re-query state and dispatch debounced events
        const bool screenEmitted = EmitOnScreenResolutionChanged();
        const bool videoEmitted = EmitOnVideoResolutionChanged();
        LOGINFO("[AppGatewayCommon|DisplaySettings.resolutionChanged] Handler responses: onScreenResolutionChanged=%s onVideoResolutionChanged=%s",
                screenEmitted ? "emitted" : "skipped", videoEmitted ? "emitted" : "skipped");
    }

    void OnHdcpProfileDisplayConnectionChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|HdcpProfile.onDisplayConnectionChanged] Incoming alias=%s.%s, invoking handlers...",
                HDCPPROFILE_CALLSIGN, "onDisplayConnectionChanged");
        // Re-query state and dispatch debounced events
        const bool hdcpEmitted = EmitOnHdcpChanged();
        const bool hdrEmitted = EmitOnHdrChanged();
        LOGINFO("[AppGatewayCommon|HdcpProfile.onDisplayConnectionChanged] Handler responses: onHdcpChanged=%s onHdrChanged=%s",
                hdcpEmitted ? "emitted" : "skipped", hdrEmitted ? "emitted" : "skipped");
    }

    void OnSystemFriendlyNameChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|System.onFriendlyNameChanged] Incoming alias=%s.%s, invoking handlers...",
                SYSTEM_CALLSIGN, "onFriendlyNameChanged");
        // Re-query state and dispatch event
        const bool nameEmitted = EmitOnNameChanged();
        LOGINFO("[AppGatewayCommon|System.onFriendlyNameChanged] Handler responses: onNameChanged=%s",
                nameEmitted ? "emitted" : "skipped");
    }

    void OnDisplaySettingsAudioFormatChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|DisplaySettings.audioFormatChanged] Incoming alias=%s.%s, invoking handlers...",
                DISPLAYSETTINGS_CALLSIGN, "audioFormatChanged");
        // Re-query state and dispatch event
        const bool audioEmitted = EmitOnAudioChanged();
        LOGINFO("[AppGatewayCommon|DisplaySettings.audioFormatChanged] Handler responses: onAudioChanged=%s",
                audioEmitted ? "emitted" : "skipped");
    }

    void OnSystemTimezoneChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        LOGINFO("[AppGatewayCommon|System.onTimezoneChanged] Incoming alias=%s.%s, invoking handlers...",
                SYSTEM_CALLSIGN, "onTimezoneChanged");
        // Re-query state and dispatch event
        const bool timezoneEmitted = EmitOnTimezoneChanged(params);
        LOGINFO("[AppGatewayCommon|System.onTimezoneChanged] Handler responses: onTimezoneChanged=%s",
                timezoneEmitted ? "emitted" : "skipped");
    }

    void OnSystemTerritoryChanged(const WPEFramework::Core::JSON::VariantContainer& params)
    {
        LOGINFO("[AppGatewayCommon|System.onTerritoryChanged] Incoming alias=%s.%s, invoking handlers...",
                SYSTEM_CALLSIGN, "onTerritoryChanged");
        // Re-query state and dispatch event
        const bool territoryEmitted = EmitOnTerritoryChanged(params);
        LOGINFO("[AppGatewayCommon|System.onTerritoryChanged] Handler responses: onTerritoryChanged=%s",
                territoryEmitted ? "emitted" : "skipped");
    }

    bool isDisplaySubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_displaySubscriptionLock);
        return _displaySubscribed;
    }

    void markDisplaySubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_displaySubscriptionLock);
        _displaySubscribed = true;
    }

    bool isDisplayAudioSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_displayAudioSubscriptionLock);
        return _displayAudioSubscribed;
    }

    void markDisplayAudioSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_displayAudioSubscriptionLock);
        _displayAudioSubscribed = true;
    }

    bool isHdcpSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_hdcpSubscriptionLock);
        return _hdcpSubscribed;
    }

     void markHdcpSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_hdcpSubscriptionLock);
        _hdcpSubscribed = true;
    }

    bool isSystemRegistered() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_systemRegistrationLock);
        return _registeredSystemEventHandlers;
    }

    void markSystemRegistered()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_systemRegistrationLock);
        _registeredSystemEventHandlers = true;
    }


private:
    PluginHost::IShell *_shell;
    std::unordered_set<std::string> _subscriptions;
    mutable Core::CriticalSection mAdminLock;
    std::string mVersionResponse;

    // JSONRPC clients for event subscriptions
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _displayRpc;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _hdcpRpc;

    // COM-RPC interface for SystemServices
    mutable Exchange::ISystemServices* _systemServicesPlugin;
    Core::Sink<SystemServicesNotification> _systemServicesNotification;
    bool _registeredSystemEventHandlers;

    bool _displaySubscribed;
    mutable Core::CriticalSection _displaySubscriptionLock;
    bool _displayAudioSubscribed;
    mutable Core::CriticalSection _displayAudioSubscriptionLock;
    bool _hdcpSubscribed;
    mutable Core::CriticalSection _hdcpSubscriptionLock;
    mutable Core::CriticalSection _systemRegistrationLock;
};

