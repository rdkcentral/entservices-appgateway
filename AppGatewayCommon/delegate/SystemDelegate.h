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

#include <array>
#include <cstdint>
#include <cstdio>
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

using namespace WPEFramework;

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

#ifndef DISPLAYINFO_CALLSIGN
#define DISPLAYINFO_CALLSIGN "DisplayInfo"
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
    static constexpr const char* EVENT_ON_NAME_CHANGED        = "Device.onDeviceNameChanged";

    SystemDelegate(PluginHost::IShell *shell)
        : BaseEventDelegate()
        , _shell(shell)
        , _subscriptions()
        , _displayRpc(nullptr)
        , _hdcpRpc(nullptr)
        , _systemRpc(nullptr)
        , _displaySubscribed(false)
        , _displayAudioSubscribed(false)
        , _hdcpSubscribed(false)
        , _systemSubscribed(false)
    {
        // Proactively subscribe to underlying Thunder events so we can react quickly.
        // Actual dispatch to apps only happens if registrations exist (BaseEventDelegate check).
        SetupDisplaySettingsSubscription();
        SetupDisplaySettingsAudioSubscription();
        SetupHdcpProfileSubscription();
        SetupSystemSubscription();
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
            if (_systemRpc && _systemSubscribed) {
                _systemRpc->Unsubscribe(2000, _T("onFriendlyNameChanged"));
            }
        } catch (...) {
            // Safe-guard against destructor exceptions
        }
        _displayRpc.reset();
        _hdcpRpc.reset();
        _systemRpc.reset();
        _shell = nullptr;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetDeviceMake(std::string &make)
    {
        /** Retrieve the device make using org.rdk.System.getDeviceInfo */
        LOGINFO("GetDeviceMake AppGatewayCommon Delegate");
        make.clear();
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            make = "unknown";
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getDeviceInfo", params, response);
        if (rc == Core::ERROR_NONE)
        {
            if (response.HasLabel(_T("make")))
            {
                make = response[_T("make")].String();
            }
        }

        if (make.empty())
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
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            name = "Living Room";
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getFriendlyName", params, response);
        if (rc == Core::ERROR_NONE && response.HasLabel(_T("friendlyName")))
        {
            name = response[_T("friendlyName")].String();
        }

        // Default if empty
        if (name.empty())
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
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        params[_T("friendlyName")] = name;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("setFriendlyName", params, response);
        if (rc == Core::ERROR_NONE && response.HasLabel(_T("success")) && response[_T("success")].Boolean())
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
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getSystemVersions", params, response);
        if (rc != Core::ERROR_NONE)
        {
            LOGERR("SystemDelegate: getSystemVersions failed rc=%u", rc);
            return Core::ERROR_UNAVAILABLE;
        }
        if (!response.HasLabel(_T("stbVersion")))
        {
            LOGERR("SystemDelegate: getSystemVersions missing stbVersion");
            return Core::ERROR_UNAVAILABLE;
        }

        const std::string stbVersion = response[_T("stbVersion")].String();
        // Per transform: split("_")[0]
        auto pos = stbVersion.find('_');
        skuOut = (pos == std::string::npos) ? stbVersion : stbVersion.substr(0, pos);
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

        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getSystemVersions", params, response);
        if (rc != Core::ERROR_NONE)
        {
            LOGERR("SystemDelegate: getSystemVersions failed rc=%u", rc);
            return Core::ERROR_UNAVAILABLE;
        }
        if (!response.HasLabel(_T("receiverVersion"))) {
            LOGERR("SystemDelegate: getSystemVersions missing receiverVersion");
            return Core::ERROR_UNAVAILABLE;
        }
        std::string receiverVersion = response[_T("receiverVersion")].String();
        if (receiverVersion.empty())
        {
            LOGERR("SystemDelegate: Failed to get Version");
            return Core::ERROR_UNAVAILABLE;
        }

        std::string stbVersion = response[_T("stbVersion")].String();
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
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            code = "US";
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getTerritory", params, response);
        if (rc == Core::ERROR_NONE && response.HasLabel(_T("territory")))
        {
            const std::string terr = response[_T("territory")].String();
            code = TerritoryThunderToFirebolt(terr, "");
        }

        // Wrap in quotes to make it a valid JSON string
        code = "\"" + code + "\"";
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    Core::hresult SetCountryCode(const std::string &code)
    {
        /** Set territory using org.rdk.System.setTerritory mapped from Firebolt country code */
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        const std::string territory = TerritoryFireboltToThunder(code, "USA");
        WPEFramework::Core::JSON::VariantContainer params;
        params[_T("territory")] = territory;

        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("setTerritory", params, response);
        if (rc == Core::ERROR_NONE && response.HasLabel(_T("success")) && response[_T("success")].Boolean())
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
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getTimeZoneDST", params, response);
        if (rc == Core::ERROR_NONE && response.HasLabel(_T("success")) && response[_T("success")].Boolean())
        {
            if (response.HasLabel(_T("timeZone")))
            {
                tz = response[_T("timeZone")].String();
                // Wrap in quotes to make it a valid JSON string
                tz = "\"" + tz + "\"";
                return Core::ERROR_NONE;
            }
        }
        LOGERR("SystemDelegate: couldn't get timezone");
        return Core::ERROR_UNAVAILABLE;
    }

    // PUBLIC_INTERFACE
    Core::hresult SetTimeZone(const std::string &tz)
    {
        /** Set timezone using org.rdk.System.setTimeZoneDST */
        auto link = AcquireLink(SYSTEM_CALLSIGN);
        if (!link)
        {
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        params[_T("timeZone")] = tz;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("setTimeZoneDST", params, response);
        if (rc == Core::ERROR_NONE && response.HasLabel(_T("success")) && response[_T("success")].Boolean())
        {
            return Core::ERROR_NONE;
        }
        LOGERR("SystemDelegate: couldn't set timezone");
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
        // Transform to rpcv2_event wrapper: { "videoResolution": $event_handler_response }
        const std::string wrapped = std::string("{\"videoResolution\":") + payload + "}";
        Dispatch(EVENT_ON_VIDEO_RES_CHANGED, wrapped);
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
        // Transform to rpcv2_event wrapper: { "screenResolution": $event }
        const std::string wrapped = std::string("{\"screenResolution\":") + payload + "}";
        Dispatch(EVENT_ON_SCREEN_RES_CHANGED, wrapped);
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
        // Transform to rpcv2_event wrapper: { "friendlyName": $event }
        const std::string wrapped = std::string("{\"friendlyName\":") + payload + "}";
        Dispatch(EVENT_ON_NAME_CHANGED, wrapped);
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
        Dispatch(EVENT_ON_AUDIO_CHANGED, payload);
        return true;
    }

    // ---- AppNotifications registration hook ----
    // Called by SettingsDelegate when app subscribes/unsubscribes to events.
    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const std::string &event, const bool listen, bool &registrationError)
    {
        registrationError = false;

        const std::string evLower = ToLower(event);

        // Supported events (case-insensitive)
        if (evLower == "device.onvideoresolutionchanged"
            || evLower == "device.onscreenresolutionchanged"
            || evLower == "device.onhdcpchanged"
            || evLower == "device.onhdrchanged"
            || evLower == "device.onaudiochanged"
            || evLower == "device.ondevicenamechanged"
            || evLower == "device.onnamechanged")
        {
            LOGINFO("[AppGatewayCommon|EventRegistration] event=%s listen=%s", event.c_str(), listen ? "true" : "false");
            if (listen) {
                AddNotification(event, cb);
                // Ensure underlying Thunder subscriptions are active
                SetupDisplaySettingsSubscription();
                SetupDisplaySettingsAudioSubscription();
                SetupHdcpProfileSubscription();
                SetupSystemSubscription();
                registrationError = false; // no error - successfully handled
                return true;
            } else {
                RemoveNotification(event, cb);
                registrationError = false; // no error - successfully handled
                return true;
            }
        }
        registrationError = true; // event not recognized - signal error to caller
        return false;
    }

    // ---- Display APIs (Firebolt Display module) ----

    // PUBLIC_INTERFACE
    // Display.edid — reads EDID from org.rdk.DisplaySettings.readEDID, returns Base64 string.
    // Returns "" when no display is connected (STB/OTT).
    Core::hresult GetDisplayEdid(std::string &result)
    {
        result = "\"\"";
        auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
        if (!link)
        {
            LOGERR("SystemDelegate: DisplaySettings link unavailable for readEDID");
            return Core::ERROR_UNAVAILABLE;
        }

        WPEFramework::Core::JSON::VariantContainer params;
        WPEFramework::Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke("readEDID", params, response);
        if (rc != Core::ERROR_NONE || !response.HasLabel(_T("EDID")))
        {
            // Treat as "no display connected" — valid for OTT/STB without HDMI output.
            // Return ERROR_NONE with empty string per the API contract.
            LOGWARN("SystemDelegate: DisplaySettings.readEDID rc=%u (no display or not supported)", rc);
            return Core::ERROR_NONE;
        }

        std::string edid = response[_T("EDID")].String();

        // Normalize: some Thunder versions JSON-escape forward slashes as \/ inside the EDID
        // Base64 string. Replace \/ -> / to produce canonical standard Base64
        // (RFC 4648 Base64 alphabet uses plain '/', not '\/').
        std::string normalized;
        normalized.reserve(edid.size());
        for (size_t i = 0; i < edid.size(); ++i)
        {
            if (edid[i] == '\\' && (i + 1) < edid.size() && edid[i + 1] == '/')
            {
                normalized += '/';
                ++i;
            }
            else if (edid[i] != '\r' && edid[i] != '\n' && edid[i] != ' ')
            {
                normalized += edid[i];
            }
        }

        LogEdidInfo(normalized);
        result = "\"" + normalized + "\"";
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    // Display.size — reads widthincentimeters / heightincentimeters from DisplayInfo.
    // Returns {"width":0,"height":0} when no display is connected (STB/OTT).
    // DisplayInfo may return either a bare integer ("48") or a JSON object
    // ({"result":48}) depending on the Thunder version; ParseDisplayInfoInt handles both.
    Core::hresult GetDisplaySize(std::string &result)
    {
        result = "{\"width\":0,\"height\":0}";
        auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
        if (!link)
        {
            LOGWARN("SystemDelegate: DisplayInfo link unavailable for size (no display or plugin absent)");
            return Core::ERROR_NONE;
        }

        const std::string emptyParams = "{}";
        std::string wStr, hStr;
        const uint32_t wRc = link->Invoke<std::string, std::string>("widthincentimeters", emptyParams, wStr);
        const uint32_t hRc = link->Invoke<std::string, std::string>("heightincentimeters", emptyParams, hStr);

        if (wRc != Core::ERROR_NONE && hRc != Core::ERROR_NONE)
        {
            LOGWARN("SystemDelegate: DisplayInfo widthincentimeters rc=%u heightincentimeters rc=%u (no display)", wRc, hRc);
            return Core::ERROR_NONE;
        }

        const int width  = (wRc == Core::ERROR_NONE) ? ParseDisplayInfoInt(wStr)  : 0;
        const int height = (hRc == Core::ERROR_NONE) ? ParseDisplayInfoInt(hStr) : 0;

        LOGDBG("SystemDelegate: GetDisplaySize widthincentimeters=%s(%d) heightincentimeters=%s(%d)",
               wStr.c_str(), width, hStr.c_str(), height);

        JsonObject obj;
        obj["width"]  = width;
        obj["height"] = height;
        obj.ToString(result);
        return Core::ERROR_NONE;
    }

    // PUBLIC_INTERFACE
    // Display.maxResolution — reads width / height (pixels) from DisplayInfo.
    // Returns {"width":0,"height":0} when no display is connected (STB/OTT).
    // DisplayInfo may return either a bare integer ("1920") or a JSON object
    // ({"result":1920}) depending on the Thunder version; ParseDisplayInfoInt handles both.
    Core::hresult GetDisplayMaxResolution(std::string &result)
    {
        result = "{\"width\":0,\"height\":0}";
        auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
        if (!link)
        {
            LOGWARN("SystemDelegate: DisplayInfo link unavailable for maxResolution (no display or plugin absent)");
            return Core::ERROR_NONE;
        }

        const std::string emptyParams = "{}";
        std::string wStr, hStr;
        const uint32_t wRc = link->Invoke<std::string, std::string>("width", emptyParams, wStr);
        const uint32_t hRc = link->Invoke<std::string, std::string>("height", emptyParams, hStr);

        if (wRc != Core::ERROR_NONE && hRc != Core::ERROR_NONE)
        {
            LOGWARN("SystemDelegate: DisplayInfo width rc=%u height rc=%u (no display)", wRc, hRc);
            return Core::ERROR_NONE;
        }

        const int width  = (wRc == Core::ERROR_NONE) ? ParseDisplayInfoInt(wStr) : 0;
        const int height = (hRc == Core::ERROR_NONE) ? ParseDisplayInfoInt(hStr) : 0;

        LOGDBG("SystemDelegate: GetDisplayMaxResolution width=%s(%d) height=%s(%d)",
               wStr.c_str(), width, hStr.c_str(), height);

        JsonObject obj;
        obj["width"]  = width;
        obj["height"] = height;
        obj.ToString(result);
        return Core::ERROR_NONE;
    }


private:
    // Parse a DisplayInfo scalar response that may be either:
    //   - a bare integer string:   "1920"
    //   - a JSON object:           {"result":1920}
    // Returns the integer value, or 0 on any parse failure.
    static int ParseDisplayInfoInt(const std::string& s)
    {
        if (s.empty()) return 0;
        // Fast path: bare integer (most common after JSONRPCDirectLink envelope unwrap)
        if (s[0] != '{')
        {
            try { return std::stoi(s); } catch (...) { return 0; }
        }
        // Slow path: JSON object — parse and extract "result"
        WPEFramework::Core::JSON::VariantContainer v;
        WPEFramework::Core::OptionalType<WPEFramework::Core::JSON::Error> err;
        if (v.FromString(s, err) && v.HasLabel(_T("result")))
        {
            const auto r = v.Get(_T("result"));
            if (r.Content() == WPEFramework::Core::JSON::Variant::type::NUMBER)
                return static_cast<int>(r.Number());
        }
        return 0;
    }

    // Decode a single Base64 character to its 6-bit value; returns -1 for padding/invalid.
    static int B64CharVal(char c)
    {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    // Decode a Base64 string (already normalized, no escaped slashes) into raw bytes.
    // Returns number of bytes written into out[0..outMax-1].
    // Uses a uint32_t accumulator to avoid signed-integer overflow (which would be UB).
    // After emitting each byte the consumed bits are masked off, keeping acc bounded.
    // '=' padding characters are treated as terminators (B64CharVal returns -1 for them).
    static size_t Base64Decode(const std::string& in, uint8_t* out, size_t outMax)
    {
        size_t   outLen = 0;
        uint32_t acc    = 0;
        int      bits   = 0;
        for (char c : in)
        {
            if (c == '=') break;          // padding marks end of data
            const int val = B64CharVal(c);
            if (val < 0) continue;        // skip whitespace / invalid chars
            acc   = (acc << 6) | static_cast<uint32_t>(val);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                if (outLen < outMax)
                    out[outLen++] = static_cast<uint8_t>((acc >> bits) & 0xFFu);
                acc &= (1u << bits) - 1u; // discard emitted bits, keep only remainder
            }
        }
        return outLen;
    }

    // Parse the normalized Base64 EDID string and log display information via LOGINFO
    // in a tabular ASCII format. Collects base-block fields (manufacturer, product, year,
    // interface, physical size, monitor name, checksum) and CEA-861 extension capabilities
    // (audio flags, video SVDs, colorimetry, HDR EOTFs), then prints them as a single table.
    // Fully failsafe: all accesses are bounds-checked; any unexpected input is caught
    // by the outer try/catch and logged without crashing.
    static void LogEdidInfo(const std::string& base64edid)
    {
        if (base64edid.empty()) return;

        try
        {
            std::array<uint8_t, 512> raw{};
            const size_t rawLen = Base64Decode(base64edid, raw.data(), raw.size());
            if (rawLen < 128)
            {
                LOGERR("SystemDelegate: [EDID] Too short to parse (%zu bytes)", rawLen);
                return;
            }

            // cap to avoid arithmetic overflows; rawLen is at most raw.size()==512
            const size_t safeLen = (rawLen < raw.size()) ? rawLen : raw.size();
            const uint8_t* e = raw.data();

            // Validate magic header: 00 FF FF FF FF FF FF 00
            static const uint8_t kHdr[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
            for (int i = 0; i < 8; ++i)
            {
                if (e[i] != kHdr[i])
                {
                    LOGERR("SystemDelegate: [EDID] Invalid header byte[%d]=0x%02X — not a valid EDID", i, e[i]);
                    return;
                }
            }

            // ---- Collect base-block fields ----

            // Manufacturer ID: 3 letters packed in 2 big-endian bytes (each field 1-26 → A-Z)
            const uint16_t mfrRaw = static_cast<uint16_t>((e[8] << 8) | e[9]);
            char mfr[4] = { '?', '?', '?', '\0' };
            {
                const uint8_t f0 = (mfrRaw >> 10) & 0x1F;
                const uint8_t f1 = (mfrRaw >>  5) & 0x1F;
                const uint8_t f2 =  mfrRaw        & 0x1F;
                if (f0 >= 1 && f0 <= 26) mfr[0] = static_cast<char>(f0 + 'A' - 1);
                if (f1 >= 1 && f1 <= 26) mfr[1] = static_cast<char>(f1 + 'A' - 1);
                if (f2 >= 1 && f2 <= 26) mfr[2] = static_cast<char>(f2 + 'A' - 1);
            }

            char prodStr[12];
            snprintf(prodStr, sizeof(prodStr), "0x%04X",
                     static_cast<unsigned>(e[10] | (e[11] << 8)));

            char yearStr[20];
            snprintf(yearStr, sizeof(yearStr), "%d / Week %d", 1990 + (int)e[17], (int)e[16]);

            char edidVerStr[8];
            snprintf(edidVerStr, sizeof(edidVerStr), "%d.%d", (int)e[18], (int)e[19]);

            // Interface type
            const uint8_t inp      = e[20];
            const bool    isDigital = (inp & 0x80) != 0;
            const char*   ifaceStr  = "Analog";
            if (isDigital)
            {
                static const char* kIface[] = { "undef","DVI","HDMIa","HDMIb","MDDI","DisplayPort" };
                const uint8_t idx = inp & 0x0F;
                ifaceStr = (idx < 6) ? kIface[idx] : "Digital";
            }
            static const char* kBpc[] = { "undef","6bpc","8bpc","10bpc","12bpc","14bpc","16bpc" };
            const uint8_t bpcIdx = (inp >> 4) & 0x07;
            const char*   bpcStr = (bpcIdx < 7) ? kBpc[bpcIdx] : "undef";
            char ifaceFullStr[24];
            snprintf(ifaceFullStr, sizeof(ifaceFullStr), "%s / %s", ifaceStr, bpcStr);

            char sizeStr[20];
            snprintf(sizeStr, sizeof(sizeStr), "%d x %d cm", (int)e[21], (int)e[22]);

            const int numExts = static_cast<int>(e[126]);
            char numExtsStr[8];
            snprintf(numExtsStr, sizeof(numExtsStr), "%d", numExts);

            // Base checksum
            uint32_t baseSum = 0;
            for (int i = 0; i < 128; ++i) baseSum += e[i];
            const char* baseCsumStr = (baseSum % 256 == 0) ? "OK" : "FAIL";

            // Monitor name from descriptor blocks (tag 0xFC)
            char monName[14] = {};
            bool foundMonName = false;
            for (int d = 0; d < 4; ++d)
            {
                const int descOff = 54 + d * 18;
                if (descOff + 18 > 128) break;
                const uint8_t* desc = e + descOff;
                if (desc[0] == 0 && desc[1] == 0 && desc[3] == 0xFC)
                {
                    int ni = 0;
                    for (int j = 5; j < 18 && ni < 13; ++j)
                    {
                        if (desc[j] == 0x0A || desc[j] == 0x00) break;
                        const uint8_t ch = desc[j];
                        monName[ni++] = (ch >= 0x20 && ch <= 0x7E) ? static_cast<char>(ch) : '?';
                    }
                    monName[ni] = '\0';
                    foundMonName = true;
                    break;
                }
            }
            const char* monNameDisp = foundMonName ? monName : "(not found)";

            // ---- Collect CEA-861 extension fields (first CEA block only) ----
            bool        hasCea     = false;
            int         ceaRev     = 0;
            bool        underscan  = false;
            bool        basicAudio = false;
            bool        ycbcr444   = false;
            bool        ycbcr422   = false;
            int         videoSVDs  = 0;
            char        nativeVicStr[28] = "N/A";
            bool        has4k      = false;
            std::string colorimetry = "none";
            std::string hdrEotfs    = "none";
            const char* extCsumStr  = "N/A";

            for (int xi = 0; xi < numExts; ++xi)
            {
                const size_t extBase = 128 + static_cast<size_t>(xi) * 128;
                if (extBase + 128 > safeLen) break;
                const uint8_t* ext = raw.data() + extBase;
                if (ext[0] != 0x02) continue;  // only CEA-861 extensions

                hasCea     = true;
                ceaRev     = static_cast<int>(ext[1]);
                const uint8_t flags = ext[3];
                underscan  = (flags & 0x80) != 0;
                basicAudio = (flags & 0x40) != 0;
                ycbcr444   = (flags & 0x20) != 0;
                ycbcr422   = (flags & 0x10) != 0;

                uint32_t extSumLocal = 0;
                for (int i = 0; i < 128; ++i) extSumLocal += ext[i];
                extCsumStr = (extSumLocal % 256 == 0) ? "OK" : "FAIL";

                // dtdOff: valid range 4..127; clamp to [4,127]
                const int dtdOff = (ext[2] >= 4 && ext[2] <= 127) ? static_cast<int>(ext[2])
                                 : (ext[2] == 0 ? 4 : 127);

                // Walk CEA data blocks (int arithmetic avoids uint8_t wrap-around)
                int pos = 4;
                while (pos < dtdOff && pos < 127)
                {
                    const uint8_t blkHdr = ext[pos];
                    const int     blkTag = (blkHdr >> 5) & 0x07;
                    const int     blkLen =  blkHdr       & 0x1F;
                    if (pos + 1 + blkLen > 127) break;
                    const uint8_t* bd = ext + pos + 1;
                    pos += 1 + blkLen;

                    if (blkTag == 2 && blkLen > 0)
                    {
                        // Video Data Block
                        videoSVDs = blkLen;
                        const int nativeVIC = static_cast<int>(bd[0] & 0x7F);
                        for (int vi = 0; vi < blkLen; ++vi)
                        {
                            const uint8_t vic = bd[vi] & 0x7F;
                            if (vic == 95 || vic == 96 || vic == 97) has4k = true;
                        }
                        // VIC → timing name lookup (common VICs)
                        const char* vicName = nullptr;
                        if      (nativeVIC == 1)  vicName = "640x480p@60";
                        else if (nativeVIC == 4)  vicName = "1280x720p@60";
                        else if (nativeVIC == 16) vicName = "1920x1080p@60";
                        else if (nativeVIC == 17) vicName = "720x576p@50";
                        else if (nativeVIC == 19) vicName = "1280x720p@50";
                        else if (nativeVIC == 31) vicName = "1920x1080p@50";
                        else if (nativeVIC == 32) vicName = "1920x1080p@24";
                        else if (nativeVIC == 95) vicName = "3840x2160p@30";
                        else if (nativeVIC == 96) vicName = "3840x2160p@50";
                        else if (nativeVIC == 97) vicName = "3840x2160p@60";
                        if (vicName)
                            snprintf(nativeVicStr, sizeof(nativeVicStr), "%d (%s)", nativeVIC, vicName);
                        else
                            snprintf(nativeVicStr, sizeof(nativeVicStr), "%d", nativeVIC);
                    }
                    else if (blkTag == 7 && blkLen >= 1)
                    {
                        const uint8_t etag = bd[0];
                        if (etag == 13 && blkLen >= 3)
                        {
                            // Colorimetry Data Block — CEA-861-F §7.5.5
                            const uint8_t m1 = bd[1];
                            const uint8_t m2 = bd[2];
                            std::string cmts;
                            if (m1 & 0x01) cmts += "xvYCC601 ";
                            if (m1 & 0x02) cmts += "xvYCC709 ";
                            if (m1 & 0x04) cmts += "sYCC601 ";
                            if (m1 & 0x08) cmts += "opYCC601 ";
                            if (m1 & 0x10) cmts += "opRGB ";
                            if (m1 & 0x20) cmts += "BT2020cYCC ";
                            if (m1 & 0x40) cmts += "BT2020YCC ";
                            if (m1 & 0x80) cmts += "BT2020RGB ";
                            if (m2 & 0x80) cmts += "DCI-P3 ";
                            if (!cmts.empty() && cmts.back() == ' ') cmts.pop_back();
                            colorimetry = cmts.empty() ? "none" : cmts;
                        }
                        else if (etag == 14 && blkLen >= 2)
                        {
                            // HDR Static Metadata Block — CEA-861-3 §4.2
                            const uint8_t ef = bd[1];
                            std::string eotfs;
                            if (ef & 0x01) eotfs += "SDR ";
                            if (ef & 0x02) eotfs += "HDR ";
                            if (ef & 0x04) eotfs += "HDR10/ST2084 ";
                            if (ef & 0x08) eotfs += "HLG ";
                            if (!eotfs.empty() && eotfs.back() == ' ') eotfs.pop_back();
                            hdrEotfs = eotfs.empty() ? "none" : eotfs;
                        }
                    }
                }
                break;  // process only the first CEA-861 extension
            }

            // ---- Print ASCII table ----
            // Layout: | %-22s | %-30s |
            //         +------------------------+--------------------------------+
            //          ^24 dashes               ^32 dashes  (total row = 59 chars)
            const char* kDiv = "+------------------------+--------------------------------+";
            LOGINFO("SystemDelegate: [EDID] %s", kDiv);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Field",               "Value");
            LOGINFO("SystemDelegate: [EDID] %s", kDiv);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Manufacturer",         mfr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Product Code",          prodStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Year / Week",           yearStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "EDID Version",          edidVerStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Interface",             ifaceFullStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Physical Size (cm)",    sizeStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Extensions",            numExtsStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Base Checksum",         baseCsumStr);
            LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Monitor Name",          monNameDisp);
            if (hasCea)
            {
                char ceaRevStr[8]; snprintf(ceaRevStr, sizeof(ceaRevStr), "%d", ceaRev);
                char svdStr[8];    snprintf(svdStr,    sizeof(svdStr),    "%d", videoSVDs);
                LOGINFO("SystemDelegate: [EDID] %s", kDiv);
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "CEA-861 Revision",  ceaRevStr);
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Underscan",         underscan  ? "Yes" : "No");
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Basic Audio",       basicAudio ? "Yes" : "No");
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "YCbCr 4:4:4",      ycbcr444   ? "Yes" : "No");
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "YCbCr 4:2:2",      ycbcr422   ? "Yes" : "No");
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Video SVDs",        svdStr);
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Native VIC",        nativeVicStr);
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "4K Support",        has4k ? "Yes" : "No");
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Colorimetry",       colorimetry.c_str());
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "HDR EOTFs",         hdrEotfs.c_str());
                LOGINFO("SystemDelegate: [EDID] | %-22s | %-30s |", "Ext Checksum",      extCsumStr);
            }
            LOGINFO("SystemDelegate: [EDID] %s", kDiv);
        }
        catch (...)
        {
            LOGERR("SystemDelegate: [EDID] LogEdidInfo caught unexpected exception — skipping EDID logging");
        }
    }

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
        if (_displaySubscribed) return;
        try {
            if (!_displayRpc) {
                _displayRpc = ::Utils::getThunderControllerClient(DISPLAYSETTINGS_CALLSIGN);
            }
            if (_displayRpc) {
                const uint32_t status = _displayRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    2000, _T("resolutionChanged"), &SystemDelegate::OnDisplaySettingsResolutionChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.resolutionChanged", DISPLAYSETTINGS_CALLSIGN);
                    _displaySubscribed = true;
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
        if (_displayAudioSubscribed) return;
        try {
            if (!_displayRpc) {
                _displayRpc = ::Utils::getThunderControllerClient(DISPLAYSETTINGS_CALLSIGN);
            }
            if (_displayRpc) {
                const uint32_t status = _displayRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    2000, _T("audioFormatChanged"), &SystemDelegate::OnDisplaySettingsAudioFormatChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.audioFormatChanged", DISPLAYSETTINGS_CALLSIGN);
                    _displayAudioSubscribed = true;
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
        if (_hdcpSubscribed) return;
        try {
            if (!_hdcpRpc) {
                _hdcpRpc = ::Utils::getThunderControllerClient(HDCPPROFILE_CALLSIGN);
            }
            if (_hdcpRpc) {
                const uint32_t status = _hdcpRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    2000, _T("onDisplayConnectionChanged"), &SystemDelegate::OnHdcpProfileDisplayConnectionChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.onDisplayConnectionChanged", HDCPPROFILE_CALLSIGN);
                    _hdcpSubscribed = true;
                } else {
                    LOGERR("SystemDelegate: Failed to subscribe to %s.onDisplayConnectionChanged rc=%u", HDCPPROFILE_CALLSIGN, status);
                }
            }
        } catch (...) {
            LOGERR("SystemDelegate: exception during HdcpProfile subscription");
        }
    }

    void SetupSystemSubscription()
    {
        if (_systemSubscribed) return;
        try {
            if (!_systemRpc) {
                _systemRpc = ::Utils::getThunderControllerClient(SYSTEM_CALLSIGN);
            }
            if (_systemRpc) {
                const uint32_t status = _systemRpc->Subscribe<WPEFramework::Core::JSON::VariantContainer>(
                    2000, _T("onFriendlyNameChanged"), &SystemDelegate::OnSystemFriendlyNameChanged, this);
                if (status == Core::ERROR_NONE) {
                    LOGINFO("SystemDelegate: Subscribed to %s.onFriendlyNameChanged", SYSTEM_CALLSIGN);
                    _systemSubscribed = true;
                } else {
                    LOGERR("SystemDelegate: Failed to subscribe to %s.onFriendlyNameChanged rc=%u", SYSTEM_CALLSIGN, status);
                }
            }
        } catch (...) {
            LOGERR("SystemDelegate: exception during System subscription");
        }
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

private:
    PluginHost::IShell *_shell;
    std::unordered_set<std::string> _subscriptions;
    mutable Core::CriticalSection mAdminLock;
    std::string mVersionResponse;

    // JSONRPC clients for event subscriptions
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _displayRpc;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _hdcpRpc;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _systemRpc;
    bool _displaySubscribed;
    bool _displayAudioSubscribed;
    bool _hdcpSubscribed;
    bool _systemSubscribed;
};

