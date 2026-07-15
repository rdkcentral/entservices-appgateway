/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

// L1 tests for the five Firebolt Display module APIs:
//   Display.edid, Display.size, Display.maxResolution  — via IConnectionProperties (ComRPC)
//   Display.colorimetry                                — via IDisplayProperties    (ComRPC)
//   Display.videoResolutions                           — via org.rdk.DisplaySettings (JSONRPC)

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>

#include "Module.h"

#define private public
#include "AppGatewayCommon.h"
#undef private

#include "ServiceMock.h"
#include "MockJSONRPCDirectLink.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

#include <interfaces/IDisplayInfo.h>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::StrEq;

// ============================================================================
// Worker-pool guard (shared with other L1 test suites via TU-local singleton)
// ============================================================================
namespace {

class WorkerPoolGuard final {
public:
    WorkerPoolGuard(const WorkerPoolGuard&) = delete;
    WorkerPoolGuard& operator=(const WorkerPoolGuard&) = delete;
    WorkerPoolGuard() : mPool(2, 0, 64), mAssigned(false)
    {
        if (!Core::IWorkerPool::IsAvailable()) {
            Core::IWorkerPool::Assign(&mPool);
            mAssigned = true;
            mPool.Run();
        }
    }
    ~WorkerPoolGuard()
    {
        if (mAssigned) {
            mPool.Stop();
            Core::IWorkerPool::Assign(nullptr);
        }
    }
private:
    WorkerPoolImplementation mPool;
    bool mAssigned;
};

static WorkerPoolGuard gWorkerPool;

static Exchange::GatewayContext MakeCtx()
{
    Exchange::GatewayContext ctx;
    ctx.appId = "test.display";
    ctx.connectionId = 1;
    ctx.requestId = 42;
    return ctx;
}

// ============================================================================
// Mock: Exchange::IConnectionProperties
// Provides Width, Height, WidthInCentimeters, HeightInCentimeters, EDID,
// Connected, and the rest as no-ops.
// ============================================================================
class MockConnectionProperties : public Exchange::IConnectionProperties {
public:
    explicit MockConnectionProperties() : _refCount(1) {}
    virtual ~MockConnectionProperties() = default;

    void AddRef() const override { _refCount.fetch_add(1, std::memory_order_relaxed); }
    uint32_t Release() const override
    {
        const uint32_t r = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (r == 0) { delete this; return Core::ERROR_DESTRUCTION_SUCCEEDED; }
        return Core::ERROR_NONE;
    }
    void* QueryInterface(const uint32_t id) override
    {
        if (id == Exchange::IConnectionProperties::ID || id == Core::IUnknown::ID) {
            AddRef();
            return static_cast<Exchange::IConnectionProperties*>(this);
        }
        return nullptr;
    }

    MOCK_METHOD(Core::hresult, Register,   (Exchange::IConnectionProperties::INotification*), (override));
    MOCK_METHOD(Core::hresult, Unregister, (Exchange::IConnectionProperties::INotification*), (override));
    MOCK_METHOD(Core::hresult, IsAudioPassthrough, (bool&), (const, override));
    MOCK_METHOD(Core::hresult, Connected,           (bool&), (const, override));
    MOCK_METHOD(Core::hresult, Width,               (uint32_t&), (const, override));
    MOCK_METHOD(Core::hresult, Height,              (uint32_t&), (const, override));
    MOCK_METHOD(Core::hresult, VerticalFreq,        (uint32_t&), (const, override));
    MOCK_METHOD(Core::hresult, EDID,                (uint16_t&, uint8_t[]), (const, override));
    MOCK_METHOD(Core::hresult, WidthInCentimeters,  (uint8_t&), (const, override));
    MOCK_METHOD(Core::hresult, HeightInCentimeters, (uint8_t&), (const, override));
    MOCK_METHOD(Core::hresult, HDCPProtection,      (Exchange::IConnectionProperties::HDCPProtectionType&), (const, override));
    MOCK_METHOD(Core::hresult, HDCPProtection,      (const Exchange::IConnectionProperties::HDCPProtectionType), (override));
    MOCK_METHOD(Core::hresult, PortName,            (string&), (const, override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

// ============================================================================
// Mock: Exchange::IDisplayProperties::IColorimetryIterator
// ============================================================================
class MockColorimetryIterator : public Exchange::IDisplayProperties::IColorimetryIterator {
public:
    explicit MockColorimetryIterator(std::vector<Exchange::IDisplayProperties::ColorimetryType> values)
        : _values(std::move(values)), _pos(0), _refCount(1) {}
    virtual ~MockColorimetryIterator() = default;

    void AddRef() const override { _refCount.fetch_add(1, std::memory_order_relaxed); }
    uint32_t Release() const override
    {
        const uint32_t r = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (r == 0) { delete this; return Core::ERROR_DESTRUCTION_SUCCEEDED; }
        return Core::ERROR_NONE;
    }
    void* QueryInterface(const uint32_t id) override
    {
        if (id == Exchange::IDisplayProperties::IColorimetryIterator::ID || id == Core::IUnknown::ID) {
            AddRef();
            return static_cast<Exchange::IDisplayProperties::IColorimetryIterator*>(this);
        }
        return nullptr;
    }

    bool Next(Exchange::IDisplayProperties::ColorimetryType& value) override
    {
        if (_pos < _values.size()) {
            value = _values[_pos++];
            return true;
        }
        return false;
    }
    bool Previous(Exchange::IDisplayProperties::ColorimetryType&) override { return false; }
    // IIteratorType::Reset(const uint32_t position) — position 0 resets to start
    void Reset(const uint32_t position) override { _pos = static_cast<size_t>(position); }
    bool IsValid() const override { return _pos < _values.size(); }
    uint32_t Count() const override { return static_cast<uint32_t>(_values.size()); }
    // IIteratorType::Current() returns element by value
    Exchange::IDisplayProperties::ColorimetryType Current() const override
    {
        if (_pos > 0 && _pos <= _values.size()) { return _values[_pos - 1]; }
        return Exchange::IDisplayProperties::COLORIMETRY_OTHER;
    }

private:
    std::vector<Exchange::IDisplayProperties::ColorimetryType> _values;
    size_t _pos;
    mutable std::atomic<uint32_t> _refCount;
};

// ============================================================================
// Mock: Exchange::IDisplayProperties
// ============================================================================
class MockDisplayProperties : public Exchange::IDisplayProperties {
public:
    explicit MockDisplayProperties() : _refCount(1) {}
    virtual ~MockDisplayProperties() = default;

    void AddRef() const override { _refCount.fetch_add(1, std::memory_order_relaxed); }
    uint32_t Release() const override
    {
        const uint32_t r = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (r == 0) { delete this; return Core::ERROR_DESTRUCTION_SUCCEEDED; }
        return Core::ERROR_NONE;
    }
    void* QueryInterface(const uint32_t id) override
    {
        if (id == Exchange::IDisplayProperties::ID || id == Core::IUnknown::ID) {
            AddRef();
            return static_cast<Exchange::IDisplayProperties*>(this);
        }
        return nullptr;
    }

    MOCK_METHOD(Core::hresult, ColorSpace,        (Exchange::IDisplayProperties::ColourSpaceType&), (const, override));
    MOCK_METHOD(Core::hresult, FrameRate,          (Exchange::IDisplayProperties::FrameRateType&), (const, override));
    MOCK_METHOD(Core::hresult, ColourDepth,        (Exchange::IDisplayProperties::ColourDepthType&), (const, override));
    MOCK_METHOD(Core::hresult, Colorimetry,        (Exchange::IDisplayProperties::IColorimetryIterator*&), (const, override));
    MOCK_METHOD(Core::hresult, QuantizationRange,  (Exchange::IDisplayProperties::QuantizationRangeType&), (const, override));
    MOCK_METHOD(Core::hresult, EOTF,               (Exchange::IDisplayProperties::EotfType&), (const, override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

// ============================================================================
// Test fixture — per-test plugin instance with full mock service wiring
// ============================================================================
class DisplayDelegateTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock>        service;
    NiceMock<MockConnectionProperties>* connProps { nullptr };
    NiceMock<MockDisplayProperties>*    dispProps { nullptr };
    Core::Sink<MockJSONRPC::MockLocalDispatcher> displaySettingsDisp;

    void SetUp() override
    {
        connProps = new NiceMock<MockConnectionProperties>();
        dispProps = new NiceMock<MockDisplayProperties>();

        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        // Wire IConnectionProperties (edid, size, maxResolution) to "DisplayInfo"
        ON_CALL(service, QueryInterfaceByCallsign(Exchange::IConnectionProperties::ID, StrEq("DisplayInfo")))
            .WillByDefault(Invoke([this](uint32_t, const string&) -> void* {
                connProps->AddRef();
                return static_cast<Exchange::IConnectionProperties*>(connProps);
            }));

        // Wire IDisplayProperties (colorimetry) to "DisplayInfo"
        ON_CALL(service, QueryInterfaceByCallsign(Exchange::IDisplayProperties::ID, StrEq("DisplayInfo")))
            .WillByDefault(Invoke([this](uint32_t, const string&) -> void* {
                dispProps->AddRef();
                return static_cast<Exchange::IDisplayProperties*>(dispProps);
            }));

        // Wire DisplaySettings JSONRPC dispatcher (videoResolutions)
        ON_CALL(service, QueryInterfaceByCallsign(PluginHost::ILocalDispatcher::ID, StrEq("org.rdk.DisplaySettings")))
            .WillByDefault(Invoke([this](uint32_t, const string&) -> void* {
                displaySettingsDisp.AddRef();
                return static_cast<PluginHost::ILocalDispatcher*>(&displaySettingsDisp);
            }));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string err = plugin.Initialize(&service);
        ASSERT_TRUE(err.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // connProps and dispProps are ref-counted; plugin holds no reference after Deinitialize.
        connProps->Release();
        connProps = nullptr;
        dispProps->Release();
        dispProps = nullptr;
    }
};

// ============================================================================
// Display.edid tests
// ============================================================================

// TEST_ID: AGC_L1_147
// Display.edid: display connected, EDID bytes returned → Base64-encoded string.
// The raw bytes 0x00, 0xFF, 0xFF encode to "AP//" in Base64.
TEST_F(DisplayDelegateTest, AGC_L1_147_DisplayEdid_Connected_ReturnBase64)
{
    ON_CALL(*connProps, Connected(_))
        .WillByDefault(Invoke([](bool& out) { out = true; return Core::ERROR_NONE; }));
    ON_CALL(*connProps, EDID(_, _))
        .WillByDefault(Invoke([](uint16_t& len, uint8_t* data) -> Core::hresult {
            // Write 3 sample bytes; caller passes len = max capacity
            data[0] = 0x00; data[1] = 0xFF; data[2] = 0xFF;
            len = 3;
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.edid", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Guard against UB: assert non-empty before accessing front()/back()
    ASSERT_GT(result.size(), 2u); // more than just "" — safe to access front/back
    // Result must be a quoted Base64 string
    EXPECT_EQ(result.front(), '"');
    EXPECT_EQ(result.back(),  '"');
}

// TEST_ID: AGC_L1_148
// Display.edid: display not connected → empty string "".
TEST_F(DisplayDelegateTest, AGC_L1_148_DisplayEdid_NotConnected_ReturnsEmpty)
{
    ON_CALL(*connProps, Connected(_))
        .WillByDefault(Invoke([](bool& out) { out = false; return Core::ERROR_NONE; }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.edid", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "\"\"");
}

// TEST_ID: AGC_L1_149
// Display.edid: EDID() returns an error → falls back to empty string "".
TEST_F(DisplayDelegateTest, AGC_L1_149_DisplayEdid_EDIDCallFails_ReturnsEmpty)
{
    ON_CALL(*connProps, Connected(_))
        .WillByDefault(Invoke([](bool& out) { out = true; return Core::ERROR_NONE; }));
    ON_CALL(*connProps, EDID(_, _))
        .WillByDefault(Invoke([](uint16_t& len, uint8_t*) -> Core::hresult {
            len = 0;
            return Core::ERROR_GENERAL;
        }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.edid", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "\"\"");
}

// ============================================================================
// Display.size tests
// ============================================================================

// TEST_ID: AGC_L1_150
// Display.size: display connected, dimensions available → correct {width, height}.
TEST_F(DisplayDelegateTest, AGC_L1_150_DisplaySize_Connected_ReturnsDimensions)
{
    ON_CALL(*connProps, WidthInCentimeters(_))
        .WillByDefault(Invoke([](uint8_t& w) { w = 48; return Core::ERROR_NONE; }));
    ON_CALL(*connProps, HeightInCentimeters(_))
        .WillByDefault(Invoke([](uint8_t& h) { h = 27; return Core::ERROR_NONE; }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.size", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("48"), std::string::npos);
    EXPECT_NE(result.find("27"), std::string::npos);
    EXPECT_NE(result.find("width"), std::string::npos);
    EXPECT_NE(result.find("height"), std::string::npos);
}

// TEST_ID: AGC_L1_151
// Display.size: both dimension calls fail → {width:0, height:0}.
TEST_F(DisplayDelegateTest, AGC_L1_151_DisplaySize_CallsFail_ReturnsZero)
{
    ON_CALL(*connProps, WidthInCentimeters(_))
        .WillByDefault(Return(Core::ERROR_GENERAL));
    ON_CALL(*connProps, HeightInCentimeters(_))
        .WillByDefault(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.size", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find(":0"), std::string::npos);
}

// ============================================================================
// Display.maxResolution tests
// ============================================================================

// TEST_ID: AGC_L1_152
// Display.maxResolution: 1080p display → {width:1920, height:1080}.
TEST_F(DisplayDelegateTest, AGC_L1_152_DisplayMaxResolution_1080p)
{
    ON_CALL(*connProps, Width(_))
        .WillByDefault(Invoke([](uint32_t& w) { w = 1920; return Core::ERROR_NONE; }));
    ON_CALL(*connProps, Height(_))
        .WillByDefault(Invoke([](uint32_t& h) { h = 1080; return Core::ERROR_NONE; }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.maxresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("1920"), std::string::npos);
    EXPECT_NE(result.find("1080"), std::string::npos);
}

// TEST_ID: AGC_L1_153
// Display.maxResolution: 4K display → {width:3840, height:2160}.
TEST_F(DisplayDelegateTest, AGC_L1_153_DisplayMaxResolution_4K)
{
    ON_CALL(*connProps, Width(_))
        .WillByDefault(Invoke([](uint32_t& w) { w = 3840; return Core::ERROR_NONE; }));
    ON_CALL(*connProps, Height(_))
        .WillByDefault(Invoke([](uint32_t& h) { h = 2160; return Core::ERROR_NONE; }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.maxresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("3840"), std::string::npos);
    EXPECT_NE(result.find("2160"), std::string::npos);
}

// TEST_ID: AGC_L1_154
// Display.maxResolution: both calls fail → {width:0, height:0}.
TEST_F(DisplayDelegateTest, AGC_L1_154_DisplayMaxResolution_CallsFail_ReturnsZero)
{
    ON_CALL(*connProps, Width(_)).WillByDefault(Return(Core::ERROR_GENERAL));
    ON_CALL(*connProps, Height(_)).WillByDefault(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.maxresolution", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find(":0"), std::string::npos);
}

// ============================================================================
// Display.colorimetry tests
// ============================================================================

// TEST_ID: AGC_L1_155
// Display.colorimetry: HDR display with BT.709 and BT.2020 → ["bt709","bt2020"].
TEST_F(DisplayDelegateTest, AGC_L1_155_DisplayColorimetry_Bt709AndBt2020)
{
    ON_CALL(*dispProps, Colorimetry(_))
        .WillByDefault(Invoke([](Exchange::IDisplayProperties::IColorimetryIterator*& iter) -> Core::hresult {
            iter = new MockColorimetryIterator({
                Exchange::IDisplayProperties::COLORIMETRY_BT709,
                Exchange::IDisplayProperties::COLORIMETRY_BT2020YCCBCBRC,
                Exchange::IDisplayProperties::COLORIMETRY_BT2020RGB_YCBCR
            });
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.colorimetry", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("bt709"),  std::string::npos);
    EXPECT_NE(result.find("bt2020"), std::string::npos);
    // Exact spec output: ["bt709","bt2020"]
    EXPECT_EQ(result, "[\"bt709\",\"bt2020\"]");
}

// TEST_ID: AGC_L1_156
// Display.colorimetry: SDR-only display with only BT.709 → ["bt709"].
TEST_F(DisplayDelegateTest, AGC_L1_156_DisplayColorimetry_Bt709Only)
{
    ON_CALL(*dispProps, Colorimetry(_))
        .WillByDefault(Invoke([](Exchange::IDisplayProperties::IColorimetryIterator*& iter) -> Core::hresult {
            iter = new MockColorimetryIterator({
                Exchange::IDisplayProperties::COLORIMETRY_BT709
            });
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.colorimetry", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("bt709"),  std::string::npos);
    EXPECT_EQ(result.find("bt2020"), std::string::npos);
    EXPECT_EQ(result, "[\"bt709\"]");
}

// TEST_ID: AGC_L1_157
// Display.colorimetry: iterator returns only unrecognized values → [].
TEST_F(DisplayDelegateTest, AGC_L1_157_DisplayColorimetry_UnknownValues_ReturnsEmpty)
{
    ON_CALL(*dispProps, Colorimetry(_))
        .WillByDefault(Invoke([](Exchange::IDisplayProperties::IColorimetryIterator*& iter) -> Core::hresult {
            iter = new MockColorimetryIterator({
                Exchange::IDisplayProperties::COLORIMETRY_UNKNOWN,
                Exchange::IDisplayProperties::COLORIMETRY_OTHER
            });
            return Core::ERROR_NONE;
        }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.colorimetry", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "[]");
}

// TEST_ID: AGC_L1_158
// Display.colorimetry: Colorimetry() call returns an error → [].
TEST_F(DisplayDelegateTest, AGC_L1_158_DisplayColorimetry_CallFails_ReturnsEmpty)
{
    ON_CALL(*dispProps, Colorimetry(_))
        .WillByDefault(Invoke([](Exchange::IDisplayProperties::IColorimetryIterator*& iter) -> Core::hresult {
            iter = nullptr;
            return Core::ERROR_GENERAL;
        }));

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.colorimetry", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "[]");
}

// ============================================================================
// Display.videoResolutions tests
// ============================================================================

// TEST_ID: AGC_L1_159
// Display.videoResolutions: UHD-capable display → all 6 Firebolt enum values returned.
TEST_F(DisplayDelegateTest, AGC_L1_159_DisplayVideoResolutions_UHD_AllEnums)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedResolutions":["480i","480p","720p50","720p60","1080p50","1080p60","2160p50","2160p60"],"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // All 6 Firebolt VideoResolution enum strings must be present
    EXPECT_NE(result.find("720p50"),  std::string::npos);
    EXPECT_NE(result.find("720p60"),  std::string::npos);
    EXPECT_NE(result.find("1080p50"), std::string::npos);
    EXPECT_NE(result.find("1080p60"), std::string::npos);
    EXPECT_NE(result.find("2160p50"), std::string::npos);
    EXPECT_NE(result.find("2160p60"), std::string::npos);
    // Non-enum values (SD, generic, non-60Hz 4K) must not appear
    EXPECT_EQ(result.find("480"),    std::string::npos);
    EXPECT_EQ(result.find("2160p30"), std::string::npos);
}

// TEST_ID: AGC_L1_160
// Display.videoResolutions: HD-only display (720p + 1080p, no 4K) → 4 enum values, no 2160p.
TEST_F(DisplayDelegateTest, AGC_L1_160_DisplayVideoResolutions_HD_NoUHD)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedResolutions":["480p","720p50","720p60","1080p50","1080p60"],"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("720p50"),  std::string::npos);
    EXPECT_NE(result.find("720p60"),  std::string::npos);
    EXPECT_NE(result.find("1080p50"), std::string::npos);
    EXPECT_NE(result.find("1080p60"), std::string::npos);
    EXPECT_EQ(result.find("2160p"),   std::string::npos);  // no 4K
}

// TEST_ID: AGC_L1_161
// Display.videoResolutions: only SD resolutions in list → [].
TEST_F(DisplayDelegateTest, AGC_L1_161_DisplayVideoResolutions_SDOnly_ReturnsEmpty)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedResolutions":["480i","480p","576p50"],"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "[]");
}

// TEST_ID: AGC_L1_162
// Display.videoResolutions: getSupportedResolutions returns an error → [].
TEST_F(DisplayDelegateTest, AGC_L1_162_DisplayVideoResolutions_CallFails_ReturnsEmpty)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) -> Core::hresult {
        resp = "{}";
        return Core::ERROR_GENERAL;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "[]");
}

// TEST_ID: AGC_L1_163
// Display.videoResolutions: response missing "supportedResolutions" field → [].
TEST_F(DisplayDelegateTest, AGC_L1_163_DisplayVideoResolutions_MissingField_ReturnsEmpty)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ(result, "[]");
}

// TEST_ID: AGC_L1_164
// Display.videoResolutions: generic "2160p" token (no framerate suffix) expands to both
// "2160p50" and "2160p60". Verifies the expansion rule for the 4K resolution class.
TEST_F(DisplayDelegateTest, AGC_L1_164_DisplayVideoResolutions_Generic2160p_ExpandsToBoth)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedResolutions":["720p50","720p60","1080p50","1080p60","2160p"],"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Generic "2160p" must expand to both 2160p50 and 2160p60
    EXPECT_NE(result.find("2160p50"), std::string::npos);
    EXPECT_NE(result.find("2160p60"), std::string::npos);
    // Other explicit tokens must also be present
    EXPECT_NE(result.find("720p50"),  std::string::npos);
    EXPECT_NE(result.find("720p60"),  std::string::npos);
    EXPECT_NE(result.find("1080p50"), std::string::npos);
    EXPECT_NE(result.find("1080p60"), std::string::npos);
}

// TEST_ID: AGC_L1_165
// Display.videoResolutions: real-world Thunder response with generic tokens ("720p", "1080p") and
// non-Firebolt tokens ("576p50", "768p60", "1080p24", "1080i50", "2160p30") → generic tokens expand
// to both 50Hz and 60Hz Firebolt enums; non-Firebolt tokens are dropped; result is deduplicated.
// Input:  ["480i","480p","576p50","720p","720p50","768p60","1080p24","1080p","1080i50","1080i","2160p30","2160p60"]
// Output: ["720p50","720p60","1080p50","1080p60","2160p60"]
TEST_F(DisplayDelegateTest, AGC_L1_165_DisplayVideoResolutions_RealWorldResponse_GenericTokenExpansion)
{
    displaySettingsDisp.SetHandler("getSupportedResolutions", [](const std::string&, const std::string&, std::string& resp) {
        resp = R"({"supportedResolutions":["480i","480p","576p50","720p","720p50","768p60","1080p24","1080p","1080i50","1080i","2160p30","2160p60"],"success":true})";
        return Core::ERROR_NONE;
    });

    const auto ctx = MakeCtx();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "display.videoresolutions", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Generic "720p" must expand to both 720p50 and 720p60 (deduped with explicit "720p50" token)
    EXPECT_NE(result.find("720p50"),  std::string::npos);
    EXPECT_NE(result.find("720p60"),  std::string::npos);
    // Generic "1080p" must expand to both 1080p50 and 1080p60
    EXPECT_NE(result.find("1080p50"), std::string::npos);
    EXPECT_NE(result.find("1080p60"), std::string::npos);
    // Explicit "2160p60" must be present; "2160p30" has no Firebolt mapping → absent
    EXPECT_NE(result.find("2160p60"), std::string::npos);
    EXPECT_EQ(result.find("2160p50"), std::string::npos);
    // Non-Firebolt tokens must not appear
    EXPECT_EQ(result.find("480"),     std::string::npos);
    EXPECT_EQ(result.find("576"),     std::string::npos);
    EXPECT_EQ(result.find("768"),     std::string::npos);
    EXPECT_EQ(result.find("1080p24"), std::string::npos);
    EXPECT_EQ(result.find("1080i"),   std::string::npos);
    EXPECT_EQ(result.find("2160p30"), std::string::npos);
}

} // anonymous namespace
