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
#include <interfaces/IDisplayInfo.h>

#include "BaseEventDelegate.h"
#include "UtilsController.h"
#include "UtilsJsonrpcDirectLink.h"
#include "UtilsLogging.h"

using namespace WPEFramework;

#ifndef DISPLAYSETTINGS_CALLSIGN
#define DISPLAYSETTINGS_CALLSIGN "org.rdk.DisplaySettings"
#endif

#ifndef HDCPPROFILE_CALLSIGN
#define HDCPPROFILE_CALLSIGN "org.rdk.HdcpProfile"
#endif

#ifndef HDMICECSOURCE_CALLSIGN
#define HDMICECSOURCE_CALLSIGN "org.rdk.HdmiCecSource"
#endif

#ifndef DISPLAYINFO_CALLSIGN
#define DISPLAYINFO_CALLSIGN "DisplayInfo"
#endif

#ifndef CALLSIGN_CALLER_APPGATEWAY_VIDEOOUTPUT
#define CALLSIGN_CALLER_APPGATEWAY_VIDEOOUTPUT "org.rdk.AppGatewayCommon.VideoOutputDelegate"
#endif

#ifndef VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS
#define VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS 2000
#endif

class VideoOutputDelegate : public BaseEventDelegate
{
public:
    // Firebolt VideoOutput event constants
    static constexpr const char* EVENT_ON_VO_RESOLUTION_CHANGED       = "VideoOutput.onResolutionChanged";
    static constexpr const char* EVENT_ON_VO_HDCP_CHANGED             = "VideoOutput.onHdcpChanged";
    static constexpr const char* EVENT_ON_VO_CEC_ACTIVE_STATE_CHANGED = "VideoOutput.onCecActiveStateChanged";
    static constexpr const char* EVENT_ON_VO_PORT_CHANGED             = "VideoOutput.onPortChanged";
    static constexpr const char* EVENT_ON_VO_REFRESH_RATE_CHANGED     = "VideoOutput.onRefreshRateChanged";

    explicit VideoOutputDelegate(PluginHost::IShell* shell)
        : BaseEventDelegate()
        , _shell(shell)
        , _displaySettingsRpc(nullptr)
        , _hdcpProfileRpc(nullptr)
        , _hdmiCecSourceRpc(nullptr)
        , _displayInfoRpc(nullptr)
        , _displaySettingsSubscribed(false)
        , _hdcpProfileSubscribed(false)
        , _hdmiCecSourceSubscribed(false)
        , _displayInfoSubscribed(false)
    {
    }

    ~VideoOutputDelegate()
    {
        try {
            if (_displaySettingsRpc && isDisplaySettingsSubscribed()) {
                _displaySettingsRpc->Unsubscribe(VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS, _T("resolutionChanged"));
                _displaySettingsRpc->Unsubscribe(VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS, _T("connectedVideoDisplaysUpdated"));
                _displaySettingsRpc->Unsubscribe(VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS, _T("videoFormatChanged"));
            }
            if (_hdcpProfileRpc && isHdcpProfileSubscribed()) {
                _hdcpProfileRpc->Unsubscribe(VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS, _T("onDisplayConnectionChanged"));
            }
            if (_hdmiCecSourceRpc && isHdmiCecSourceSubscribed()) {
                _hdmiCecSourceRpc->Unsubscribe(VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS, _T("onActiveSourceStatusUpdated"));
            }
            if (_displayInfoRpc && isDisplayInfoSubscribed()) {
                _displayInfoRpc->Unsubscribe(VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS, _T("updated"));
            }
        } catch (...) {
        }
        _displaySettingsRpc.reset();
        _hdcpProfileRpc.reset();
        _hdmiCecSourceRpc.reset();
        _displayInfoRpc.reset();
        _shell = nullptr;
    }

    // ─── PUBLIC API: 10 Firebolt VideoOutput getters ───────────────────────

    // AC1: VideoOutput.resolution
    Core::hresult GetVideoOutputResolution(std::string& result)
    {
        result = "{\"width\":0,\"height\":0}";
        auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: DisplaySettings link unavailable for GetVideoOutputResolution");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getCurrentResolution", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getCurrentResolution failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        int w = 0, h = 0;
        if (response.HasLabel(_T("resolution"))) {
            std::string res = response.Get(_T("resolution")).String();
            ParseResolutionString(res, w, h);
        } else if (response.HasLabel(_T("w")) && response.HasLabel(_T("h"))) {
            w = static_cast<int>(response.Get(_T("w")).Number());
            h = static_cast<int>(response.Get(_T("h")).Number());
        }

        result = "{\"width\":" + std::to_string(w) + ",\"height\":" + std::to_string(h) + "}";
        LOGDBG("VideoOutputDelegate: GetVideoOutputResolution -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC2: VideoOutput.hdcp
    Core::hresult GetVideoOutputHdcp(std::string& result)
    {
        result = "\"none\"";
        // TODO: TV device detection → return "\"direct\""
        auto link = AcquireLink(HDCPPROFILE_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: HdcpProfile link unavailable for GetVideoOutputHdcp");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getHDCPStatus", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getHDCPStatus failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        (void)ExtractHdcpValue(response, result);
        LOGDBG("VideoOutputDelegate: GetVideoOutputHdcp -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC3: VideoOutput.cecActiveState
    Core::hresult GetVideoOutputCecActiveState(std::string& result)
    {
        result = "\"unsupported\"";
        // TODO: TV device detection → return error "Wrong device class"
        auto link = AcquireLink(HDMICECSOURCE_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: HdmiCecSource link unavailable for GetVideoOutputCecActiveState");
            return Core::ERROR_NONE;
        }

        // Step 1: Check if CEC is enabled
        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer enabledResp;
        uint32_t rc = link->Invoke<decltype(params), decltype(enabledResp)>("getEnabled", params, enabledResp);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getEnabled failed rc=%u", rc);
            return Core::ERROR_NONE; // return "unsupported"
        }

        bool enabled = false;
        if (enabledResp.HasLabel(_T("enabled"))) {
            enabled = enabledResp.Get(_T("enabled")).Boolean();
        }
        if (false == enabled) {
            return Core::ERROR_NONE; // return "unsupported"
        }

        // Step 2: Check active source status
        Core::JSON::VariantContainer statusResp;
        rc = link->Invoke<decltype(params), decltype(statusResp)>("getActiveSourceStatus", params, statusResp);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getActiveSourceStatus failed rc=%u", rc);
            return Core::ERROR_NONE; // return "unsupported"
        }

        if (statusResp.HasLabel(_T("status"))) {
            result = statusResp.Get(_T("status")).Boolean() ? "\"active\"" : "\"inactive\"";
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputCecActiveState -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC4: VideoOutput.port
    Core::hresult GetVideoOutputPort(std::string& result)
    {
        result = "\"none\"";
        auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: DisplaySettings link unavailable for GetVideoOutputPort");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getConnectedVideoDisplays", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getConnectedVideoDisplays failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        if (response.HasLabel(_T("connectedVideoDisplays"))) {
            auto displays = response.Get(_T("connectedVideoDisplays"));
            if (displays.Content() == Core::JSON::Variant::type::ARRAY) {
                auto arr = displays.Array();
                if (arr.Length() > 0) {
                    std::string portName = arr[0].String();
                    std::string lowerPort = ToLower(portName);
                    if (lowerPort.find("hdmi") != std::string::npos) {
                        result = "\"hdmi\"";
                    } else if (lowerPort.find("internal") != std::string::npos) {
                        result = "\"internal\"";
                    }
                }
            }
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputPort -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC5: VideoOutput.refreshRate (COM-RPC)
    Core::hresult GetVideoOutputRefreshRate(std::string& result)
    {
        result = "0";
        if (nullptr == _shell) {
            LOGERR("VideoOutputDelegate: shell is null for GetVideoOutputRefreshRate");
            return Core::ERROR_NONE;
        }

        auto* displayProps = _shell->QueryInterfaceByCallsign<Exchange::IDisplayProperties>(DISPLAYINFO_CALLSIGN);
        if (nullptr == displayProps) {
            LOGWARN("VideoOutputDelegate: IDisplayProperties unavailable for refreshRate");
            return Core::ERROR_NONE;
        }

        Exchange::IDisplayProperties::FrameRateType rate{};
        const Core::hresult rc = displayProps->FrameRate(rate);
        displayProps->Release();

        if (Core::ERROR_NONE != rc) {
            LOGWARN("VideoOutputDelegate: FrameRate() failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        switch (rate) {
            case Exchange::IDisplayProperties::FRAMERATE_23_976: result = "23.976"; break;
            case Exchange::IDisplayProperties::FRAMERATE_24:     result = "24"; break;
            case Exchange::IDisplayProperties::FRAMERATE_25:     result = "25"; break;
            case Exchange::IDisplayProperties::FRAMERATE_29_97:  result = "29.97"; break;
            case Exchange::IDisplayProperties::FRAMERATE_30:     result = "30"; break;
            case Exchange::IDisplayProperties::FRAMERATE_50:     result = "50"; break;
            case Exchange::IDisplayProperties::FRAMERATE_59_94:  result = "59.94"; break;
            case Exchange::IDisplayProperties::FRAMERATE_60:     result = "60"; break;
            default: result = "0"; break;
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputRefreshRate -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC6: VideoOutput.colorDepth (COM-RPC)
    Core::hresult GetVideoOutputColorDepth(std::string& result)
    {
        result = "0";
        if (nullptr == _shell) {
            LOGERR("VideoOutputDelegate: shell is null for GetVideoOutputColorDepth");
            return Core::ERROR_NONE;
        }

        auto* displayProps = _shell->QueryInterfaceByCallsign<Exchange::IDisplayProperties>(DISPLAYINFO_CALLSIGN);
        if (nullptr == displayProps) {
            LOGWARN("VideoOutputDelegate: IDisplayProperties unavailable for colorDepth");
            return Core::ERROR_NONE;
        }

        Exchange::IDisplayProperties::ColourDepthType depth{};
        const Core::hresult rc = displayProps->ColourDepth(depth);
        displayProps->Release();

        if (Core::ERROR_NONE != rc) {
            LOGWARN("VideoOutputDelegate: ColourDepth() failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        switch (depth) {
            case Exchange::IDisplayProperties::COLORDEPTH_8_BIT:  result = "8"; break;
            case Exchange::IDisplayProperties::COLORDEPTH_10_BIT: result = "10"; break;
            case Exchange::IDisplayProperties::COLORDEPTH_12_BIT: result = "12"; break;
            default: result = "0"; break;
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputColorDepth -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC7: VideoOutput.colorFormat (JSON-RPC)
    Core::hresult GetVideoOutputColorFormat(std::string& result)
    {
        result = "\"none\"";
        auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: DisplayInfo link unavailable for GetVideoOutputColorFormat");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("colorspace", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: colorspace failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        if (response.HasLabel(_T("cs"))) {
            std::string cs = response.Get(_T("cs")).String();
            if (cs == "FormatYcbcr420")      result = "\"ycbcr420\"";
            else if (cs == "FormatYcbcr422") result = "\"ycbcr422\"";
            else if (cs == "FormatYcbcr444") result = "\"ycbcr444\"";
            else if (cs == "FormatRgb444")   result = "\"rgb444\"";
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputColorFormat -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC8: VideoOutput.colorimetry (JSON-RPC)
    Core::hresult GetVideoOutputColorimetry(std::string& result)
    {
        result = "\"none\"";
        auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: DisplayInfo link unavailable for GetVideoOutputColorimetry");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getCurrentColorimetry", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getCurrentColorimetry failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        if (response.HasLabel(_T("colorimetry"))) {
            std::string c = response.Get(_T("colorimetry")).String();
            if (c == "ColorimetryBt709")                result = "\"bt709\"";
            else if (c == "ColorimetrySmpte170M")       result = "\"smpte170m\"";
            else if (c == "ColorimetryXvycc709")        result = "\"xvycc709\"";
            else if (c == "ColorimetryXvycc601")        result = "\"xvycc601\"";
            else if (c == "ColorimetryBt2020rgbYcbcr")  result = "\"bt2020rgb\"";
            else if (c == "ColorimetryBt2020yccbcbrc")  result = "\"bt2020ycc\"";
            else if (c == "ColorimetryOprgb")           result = "\"oprgb\"";
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputColorimetry -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC9: VideoOutput.dynamicRange (JSON-RPC)
    Core::hresult GetVideoOutputDynamicRange(std::string& result)
    {
        result = "\"none\"";
        auto link = AcquireLink(DISPLAYSETTINGS_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: DisplaySettings link unavailable for GetVideoOutputDynamicRange");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("getVideoFormat", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: getVideoFormat failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        if (response.HasLabel(_T("currentVideoFormat"))) {
            std::string fmt = response.Get(_T("currentVideoFormat")).String();
            if (fmt == "HDR10")          result = "\"hdr10\"";
            else if (fmt == "HDR10PLUS") result = "\"hdr10plus\"";
            else if (fmt == "DV")        result = "\"dolbyVision\"";
            else if (fmt == "HLG")       result = "\"hlg\"";
            else if (fmt == "SDR")       result = "\"sdr\"";
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputDynamicRange -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // AC10: VideoOutput.quantizationRange (JSON-RPC)
    Core::hresult GetVideoOutputQuantizationRange(std::string& result)
    {
        result = "\"none\"";
        auto link = AcquireLink(DISPLAYINFO_CALLSIGN);
        if (nullptr == link) {
            LOGERR("VideoOutputDelegate: DisplayInfo link unavailable for GetVideoOutputQuantizationRange");
            return Core::ERROR_NONE;
        }

        Core::JSON::VariantContainer params;
        Core::JSON::VariantContainer response;
        const uint32_t rc = link->Invoke<decltype(params), decltype(response)>("quantizationrange", params, response);
        if (Core::ERROR_NONE != rc) {
            LOGERR("VideoOutputDelegate: quantizationrange failed rc=%u", rc);
            return Core::ERROR_NONE;
        }

        if (response.HasLabel(_T("qr"))) {
            std::string qr = response.Get(_T("qr")).String();
            if (qr == "QuantizationrangeLimited")    result = "\"limited\"";
            else if (qr == "QuantizationrangeFull")  result = "\"full\"";
        }

        LOGDBG("VideoOutputDelegate: GetVideoOutputQuantizationRange -> %s", result.c_str());
        return Core::ERROR_NONE;
    }

    // ─── EVENT EMITTERS ───────────────────────────────────────────────────

    bool EmitOnResolutionChanged()
    {
        std::string payload;
        if (Core::ERROR_NONE != GetVideoOutputResolution(payload)) {
            LOGERR("[AppGatewayCommon|VideoOutput.onResolutionChanged] GetVideoOutputResolution failed");
            return false;
        }
        return Dispatch(EVENT_ON_VO_RESOLUTION_CHANGED, payload);
    }

    bool EmitOnHdcpChanged()
    {
        std::string payload;
        if (Core::ERROR_NONE != GetVideoOutputHdcp(payload)) {
            LOGERR("[AppGatewayCommon|VideoOutput.onHdcpChanged] GetVideoOutputHdcp failed");
            return false;
        }
        return Dispatch(EVENT_ON_VO_HDCP_CHANGED, payload);
    }

    bool EmitOnCecActiveStateChanged()
    {
        std::string payload;
        if (Core::ERROR_NONE != GetVideoOutputCecActiveState(payload)) {
            LOGERR("[AppGatewayCommon|VideoOutput.onCecActiveStateChanged] GetVideoOutputCecActiveState failed");
            return false;
        }
        return Dispatch(EVENT_ON_VO_CEC_ACTIVE_STATE_CHANGED, payload);
    }

    bool EmitOnPortChanged()
    {
        std::string payload;
        if (Core::ERROR_NONE != GetVideoOutputPort(payload)) {
            LOGERR("[AppGatewayCommon|VideoOutput.onPortChanged] GetVideoOutputPort failed");
            return false;
        }
        return Dispatch(EVENT_ON_VO_PORT_CHANGED, payload);
    }

    bool EmitOnRefreshRateChanged()
    {
        std::string payload;
        if (Core::ERROR_NONE != GetVideoOutputRefreshRate(payload)) {
            LOGERR("[AppGatewayCommon|VideoOutput.onRefreshRateChanged] GetVideoOutputRefreshRate failed");
            return false;
        }
        return Dispatch(EVENT_ON_VO_REFRESH_RATE_CHANGED, payload);
    }

    // ─── HandleEvent (BaseEventDelegate) ──────────────────────────────────

    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter* cb, const std::string& event, const bool listen, bool& registrationError) override
    {
        registrationError = false;
        const std::string evLower = StringUtils::toLower(event);

        if (evLower == "videooutput.onresolutionchanged") {
            SetupDisplaySettingsSubscription();
        } else if (evLower == "videooutput.onhdcpchanged") {
            SetupHdcpProfileSubscription();
        } else if (evLower == "videooutput.oncecactivestatechanged") {
            SetupHdmiCecSourceSubscription();
        } else if (evLower == "videooutput.onportchanged") {
            SetupDisplaySettingsSubscription();
        } else if (evLower == "videooutput.onrefreshratechanged") {
            SetupDisplayInfoSubscription();
        } else {
            registrationError = true;
            return false;
        }

        if (!registrationError) {
            LOGINFO("[AppGatewayCommon|VideoOutput|EventRegistration] event=%s listen=%s", event.c_str(), listen ? "true" : "false");
            if (true == listen) {
                AddNotification(event, cb);
            } else {
                RemoveNotification(event, cb);
            }
        }

        return true;
    }

private:
    // ─── Resolution string parser ─────────────────────────────────────────

    static void ParseResolutionString(const std::string& res, int& w, int& h)
    {
        // Map common named resolutions to {width, height}
        std::string lower = ToLower(res);

        // Strip trailing frequency (e.g. "1080p60" → "1080p")
        std::string base;
        for (char c : lower) {
            if (std::isdigit(c) || c == 'p' || c == 'i' || c == 'x') {
                base += c;
            } else {
                break;
            }
        }

        if (base == "480p" || base == "480i" || base == "720x480") { w = 720; h = 480; }
        else if (base == "576p" || base == "576i" || base == "720x576") { w = 720; h = 576; }
        else if (base == "720p" || base == "1280x720") { w = 1280; h = 720; }
        else if (base == "1080p" || base == "1080i" || base == "1920x1080") { w = 1920; h = 1080; }
        else if (base == "2160p" || base == "2160p60" || base == "3840x2160") { w = 3840; h = 2160; }
        else {
            // Try WxH format
            auto xpos = base.find('x');
            if (xpos != std::string::npos) {
                try {
                    w = std::stoi(base.substr(0, xpos));
                    h = std::stoi(base.substr(xpos + 1));
                } catch (...) {
                    w = 0; h = 0;
                }
            } else {
                w = 0; h = 0;
            }
        }
    }

    // ─── HDCP extraction helper ───────────────────────────────────────────

    static bool ExtractHdcpValue(const Core::JSON::VariantContainer& response, std::string& result)
    {
        // Try nested "HDCPStatus" object
        Core::JSON::VariantContainer statusObj;
        bool hasStatus = false;

        if (response.HasLabel(_T("HDCPStatus"))) {
            auto s = response.Get(_T("HDCPStatus"));
            if (s.Content() == Core::JSON::Variant::type::OBJECT) {
                statusObj = s.Object();
                hasStatus = true;
            }
        } else if (response.HasLabel(_T("result"))) {
            auto r = response.Get(_T("result"));
            if (r.Content() == Core::JSON::Variant::type::OBJECT) {
                auto inner = r.Object();
                if (inner.HasLabel(_T("HDCPStatus"))) {
                    auto s = inner.Get(_T("HDCPStatus"));
                    if (s.Content() == Core::JSON::Variant::type::OBJECT) {
                        statusObj = s.Object();
                        hasStatus = true;
                    }
                }
            }
        }

        if (false == hasStatus) {
            return false;
        }

        bool isConnected = false;
        bool isHDCPEnabled = false;

        if (statusObj.HasLabel(_T("isConnected"))) {
            isConnected = statusObj.Get(_T("isConnected")).Boolean();
        }
        if (statusObj.HasLabel(_T("isHDCPEnabled"))) {
            isHDCPEnabled = statusObj.Get(_T("isHDCPEnabled")).Boolean();
        }

        if (isConnected && isHDCPEnabled) {
            if (statusObj.HasLabel(_T("currentHDCPVersion"))) {
                std::string version = statusObj.Get(_T("currentHDCPVersion")).String();
                if (version == "2.2") {
                    result = "\"hdcp2.2\"";
                } else if (version == "1.4") {
                    result = "\"hdcp1.4\"";
                }
            }
        }
        // else result stays "\"none\""

        return true;
    }

    // ─── String utilities ─────────────────────────────────────────────────

    static std::string ToLower(const std::string& in)
    {
        std::string out = in;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(::tolower(c));
        });
        return out;
    }

    // ─── Thunder event subscriptions ──────────────────────────────────────

    void SetupDisplaySettingsSubscription()
    {
        if (true == isDisplaySettingsSubscribed()) {
            return;
        }

        try {
            if (nullptr == _displaySettingsRpc) {
                _displaySettingsRpc = ::Utils::getThunderControllerClient(DISPLAYSETTINGS_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY_VIDEOOUTPUT);
            }

            if (nullptr != _displaySettingsRpc) {
                uint32_t status = _displaySettingsRpc->Subscribe<Core::JSON::VariantContainer>(
                    VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS,
                    _T("resolutionChanged"),
                    &VideoOutputDelegate::OnResolutionChanged,
                    this);

                if (Core::ERROR_NONE != status) {
                    LOGERR("VideoOutputDelegate: Failed to subscribe to resolutionChanged rc=%u", status);
                }

                status = _displaySettingsRpc->Subscribe<Core::JSON::VariantContainer>(
                    VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS,
                    _T("connectedVideoDisplaysUpdated"),
                    &VideoOutputDelegate::OnConnectedVideoDisplaysUpdated,
                    this);

                if (Core::ERROR_NONE != status) {
                    LOGERR("VideoOutputDelegate: Failed to subscribe to connectedVideoDisplaysUpdated rc=%u", status);
                }

                status = _displaySettingsRpc->Subscribe<Core::JSON::VariantContainer>(
                    VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS,
                    _T("videoFormatChanged"),
                    &VideoOutputDelegate::OnVideoFormatChanged,
                    this);

                if (Core::ERROR_NONE != status) {
                    LOGERR("VideoOutputDelegate: Failed to subscribe to videoFormatChanged rc=%u", status);
                }

                markDisplaySettingsSubscribed();
                LOGINFO("VideoOutputDelegate: Subscribed to %s events", DISPLAYSETTINGS_CALLSIGN);
            }
        } catch (...) {
            LOGERR("VideoOutputDelegate: exception during DisplaySettings subscription");
        }
    }

    void SetupDisplayInfoSubscription()
    {
        if (true == isDisplayInfoSubscribed()) {
            return;
        }

        try {
            if (nullptr == _displayInfoRpc) {
                _displayInfoRpc = ::Utils::getThunderControllerClient(DISPLAYINFO_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY_VIDEOOUTPUT);
            }

            if (nullptr != _displayInfoRpc) {
                const uint32_t status = _displayInfoRpc->Subscribe<Core::JSON::VariantContainer>(
                    VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS,
                    _T("updated"),
                    &VideoOutputDelegate::OnDisplayInfoUpdated,
                    this);

                if (Core::ERROR_NONE == status) {
                    markDisplayInfoSubscribed();
                    LOGINFO("VideoOutputDelegate: Subscribed to %s.updated", DISPLAYINFO_CALLSIGN);
                } else {
                    LOGERR("VideoOutputDelegate: Failed to subscribe to DisplayInfo.updated rc=%u", status);
                }
            }
        } catch (...) {
            LOGERR("VideoOutputDelegate: exception during DisplayInfo subscription");
        }
    }

    void SetupHdcpProfileSubscription()
    {
        if (true == isHdcpProfileSubscribed()) {
            return;
        }

        try {
            if (nullptr == _hdcpProfileRpc) {
                _hdcpProfileRpc = ::Utils::getThunderControllerClient(HDCPPROFILE_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY_VIDEOOUTPUT);
            }

            if (nullptr != _hdcpProfileRpc) {
                const uint32_t status = _hdcpProfileRpc->Subscribe<Core::JSON::VariantContainer>(
                    VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS,
                    _T("onDisplayConnectionChanged"),
                    &VideoOutputDelegate::OnDisplayConnectionChanged,
                    this);

                if (Core::ERROR_NONE == status) {
                    markHdcpProfileSubscribed();
                    LOGINFO("VideoOutputDelegate: Subscribed to %s.onDisplayConnectionChanged", HDCPPROFILE_CALLSIGN);
                } else {
                    LOGERR("VideoOutputDelegate: Failed to subscribe to onDisplayConnectionChanged rc=%u", status);
                }
            }
        } catch (...) {
            LOGERR("VideoOutputDelegate: exception during HdcpProfile subscription");
        }
    }

    void SetupHdmiCecSourceSubscription()
    {
        if (true == isHdmiCecSourceSubscribed()) {
            return;
        }

        try {
            if (nullptr == _hdmiCecSourceRpc) {
                _hdmiCecSourceRpc = ::Utils::getThunderControllerClient(HDMICECSOURCE_CALLSIGN, CALLSIGN_CALLER_APPGATEWAY_VIDEOOUTPUT);
            }

            if (nullptr != _hdmiCecSourceRpc) {
                const uint32_t status = _hdmiCecSourceRpc->Subscribe<Core::JSON::VariantContainer>(
                    VIDEOOUTPUT_SUBSCRIBE_TIMEOUT_MS,
                    _T("onActiveSourceStatusUpdated"),
                    &VideoOutputDelegate::OnActiveSourceStatusUpdated,
                    this);

                if (Core::ERROR_NONE == status) {
                    markHdmiCecSourceSubscribed();
                    LOGINFO("VideoOutputDelegate: Subscribed to %s.onActiveSourceStatusUpdated", HDMICECSOURCE_CALLSIGN);
                } else {
                    LOGERR("VideoOutputDelegate: Failed to subscribe to onActiveSourceStatusUpdated rc=%u", status);
                }
            }
        } catch (...) {
            LOGERR("VideoOutputDelegate: exception during HdmiCecSource subscription");
        }
    }

    // ─── Thunder event handlers ───────────────────────────────────────────

    void OnResolutionChanged(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|DisplaySettings.resolutionChanged] Incoming, re-querying resolution...");
        (void)EmitOnResolutionChanged();
    }

    void OnDisplayConnectionChanged(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|HdcpProfile.onDisplayConnectionChanged] Incoming, re-querying HDCP...");
        (void)EmitOnHdcpChanged();
    }

    void OnActiveSourceStatusUpdated(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|HdmiCecSource.onActiveSourceStatusUpdated] Incoming, re-querying CEC state...");
        (void)EmitOnCecActiveStateChanged();
    }

    void OnConnectedVideoDisplaysUpdated(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|DisplaySettings.connectedVideoDisplaysUpdated] Incoming, re-querying port...");
        (void)EmitOnPortChanged();
    }

    void OnDisplayInfoUpdated(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|DisplayInfo.updated] Incoming, re-querying refreshRate and colorimetry...");
        (void)EmitOnRefreshRateChanged();
        std::string colorimetry;
        (void)GetVideoOutputColorimetry(colorimetry);
    }

    void OnVideoFormatChanged(const Core::JSON::VariantContainer& params)
    {
        (void)params;
        LOGINFO("[AppGatewayCommon|DisplaySettings.videoFormatChanged] Incoming, re-querying dynamicRange...");
        // dynamicRange has no dedicated event entry in resolution.base.json,
        // but the subscription trigger re-query is done here for future use.
    }

    // ─── Subscription state tracking ──────────────────────────────────────

    bool isDisplaySettingsSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        return _displaySettingsSubscribed;
    }

    void markDisplaySettingsSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        _displaySettingsSubscribed = true;
    }

    bool isHdcpProfileSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        return _hdcpProfileSubscribed;
    }

    void markHdcpProfileSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        _hdcpProfileSubscribed = true;
    }

    bool isHdmiCecSourceSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        return _hdmiCecSourceSubscribed;
    }

    void markHdmiCecSourceSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        _hdmiCecSourceSubscribed = true;
    }

    bool isDisplayInfoSubscribed() const
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        return _displayInfoSubscribed;
    }

    void markDisplayInfoSubscribed()
    {
        Core::SafeSyncType<Core::CriticalSection> lock(_subscriptionLock);
        _displayInfoSubscribed = true;
    }

    // ─── JSON-RPC link acquisition ────────────────────────────────────────

    std::shared_ptr<WPEFramework::Utils::JSONRPCDirectLink> AcquireLink(const std::string& callsign) const
    {
        if (nullptr == _shell) {
            LOGERR("VideoOutputDelegate: shell is null");
            return nullptr;
        }
        return WPEFramework::Utils::GetThunderControllerClient(_shell, callsign);
    }

private:
    PluginHost::IShell* _shell;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _displaySettingsRpc;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _hdcpProfileRpc;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _hdmiCecSourceRpc;
    std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> _displayInfoRpc;
    bool _displaySettingsSubscribed;
    bool _hdcpProfileSubscribed;
    bool _hdmiCecSourceSubscribed;
    bool _displayInfoSubscribed;
    mutable Core::CriticalSection _subscriptionLock;
};
