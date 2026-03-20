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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

#include "Module.h"

#define private public
#include "AppGatewayCommon.h"
#undef private

#include "ServiceMock.h"
#include "MockNetworkManager.h"
#include "MockEmitter.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SetArgReferee;

namespace {

class WorkerPoolGuard final {
public:
    WorkerPoolGuard(const WorkerPoolGuard&) = delete;
    WorkerPoolGuard& operator=(const WorkerPoolGuard&) = delete;

    WorkerPoolGuard()
        : mPool(2, 0, 64)
        , mAssigned(false)
    {
        if (Core::IWorkerPool::IsAvailable() == false) {
            Core::IWorkerPool::Assign(&mPool);
            mAssigned = true;
        }
        mPool.Run();
    }

    ~WorkerPoolGuard()
    {
        mPool.Stop();
        if (mAssigned) {
            Core::IWorkerPool::Assign(nullptr);
        }
    }

private:
    WorkerPoolImplementation mPool;
    bool mAssigned;
};

static WorkerPoolGuard gWorkerPool;

static Exchange::GatewayContext MakeContext()
{
    Exchange::GatewayContext ctx;
    ctx.appId = "test.app";
    ctx.connectionId = 100;
    ctx.requestId = 200;
    return ctx;
}

class NetworkDelegateTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<MockNetworkManager> mockNetwork;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(service, QueryInterfaceByCallsign(Exchange::INetworkManager::ID, ::testing::StrEq("org.rdk.NetworkManager")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                mockNetwork.AddRef();
                return static_cast<Exchange::INetworkManager*>(&mockNetwork);
            }));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
        EXPECT_CALL(mockNetwork, Register(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
        EXPECT_CALL(mockNetwork, Unregister(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = plugin.Initialize(&service);
        ASSERT_TRUE(response.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

/* ---------- GetNetworkConnected ---------- */

TEST_F(NetworkDelegateTest, AGC_L1_099_GetNetworkConnected_Connected)
{
    EXPECT_CALL(mockNetwork, GetPrimaryInterface(_))
        .WillOnce(DoAll(SetArgReferee<0>("eth0"), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("true", result);
}

TEST_F(NetworkDelegateTest, AGC_L1_100_GetNetworkConnected_Disconnected)
{
    EXPECT_CALL(mockNetwork, GetPrimaryInterface(_))
        .WillOnce(DoAll(SetArgReferee<0>(""), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("false", result);
}

TEST_F(NetworkDelegateTest, AGC_L1_101_GetNetworkConnected_CallFails)
{
    EXPECT_CALL(mockNetwork, GetPrimaryInterface(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- GetInternetConnectionStatus ---------- */

TEST_F(NetworkDelegateTest, AGC_L1_102_GetInternetConnectionStatus_Ethernet)
{
    auto* mockIterator = new NiceMock<MockInterfaceDetailsIterator>();
    
    Exchange::INetworkManager::InterfaceDetails ethernetIface;
    ethernetIface.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
    ethernetIface.name = "eth0";
    ethernetIface.connected = true;

    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce(DoAll(SetArgReferee<0>(ethernetIface), Return(true)))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mockIterator, Release())
        .WillOnce(::testing::Invoke([mockIterator]() { delete mockIterator; return 0; }));

    EXPECT_CALL(mockNetwork, GetAvailableInterfaces(_))
        .WillOnce(DoAll(SetArgReferee<0>(mockIterator), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("ethernet"), std::string::npos);
    EXPECT_NE(result.find("connected"), std::string::npos);
}

TEST_F(NetworkDelegateTest, AGC_L1_103_GetInternetConnectionStatus_WiFi)
{
    auto* mockIterator = new NiceMock<MockInterfaceDetailsIterator>();
    
    Exchange::INetworkManager::InterfaceDetails wifiIface;
    wifiIface.type = Exchange::INetworkManager::INTERFACE_TYPE_WIFI;
    wifiIface.name = "wlan0";
    wifiIface.connected = true;

    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce(DoAll(SetArgReferee<0>(wifiIface), Return(true)))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mockIterator, Release())
        .WillOnce(::testing::Invoke([mockIterator]() { delete mockIterator; return 0; }));

    EXPECT_CALL(mockNetwork, GetAvailableInterfaces(_))
        .WillOnce(DoAll(SetArgReferee<0>(mockIterator), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("wifi"), std::string::npos);
}

TEST_F(NetworkDelegateTest, AGC_L1_104_GetInternetConnectionStatus_NoneConnected)
{
    auto* mockIterator = new NiceMock<MockInterfaceDetailsIterator>();
    
    Exchange::INetworkManager::InterfaceDetails disconnectedIface;
    disconnectedIface.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
    disconnectedIface.name = "eth0";
    disconnectedIface.connected = false;

    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce(DoAll(SetArgReferee<0>(disconnectedIface), Return(true)))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mockIterator, Release())
        .WillOnce(::testing::Invoke([mockIterator]() { delete mockIterator; return 0; }));

    EXPECT_CALL(mockNetwork, GetAvailableInterfaces(_))
        .WillOnce(DoAll(SetArgReferee<0>(mockIterator), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{}", result);
}

TEST_F(NetworkDelegateTest, AGC_L1_105_GetInternetConnectionStatus_NullIterator)
{
    EXPECT_CALL(mockNetwork, GetAvailableInterfaces(_))
        .WillOnce(DoAll(SetArgReferee<0>(nullptr), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("{}", result);
}

TEST_F(NetworkDelegateTest, AGC_L1_106_GetInternetConnectionStatus_CallFails)
{
    EXPECT_CALL(mockNetwork, GetAvailableInterfaces(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- Additional network error paths ---------- */

TEST_F(NetworkDelegateTest, AGC_L1_173_GetNetworkConnected_EmptyPrimaryInterface)
{
    EXPECT_CALL(mockNetwork, GetPrimaryInterface(_))
        .WillOnce(DoAll(SetArgReferee<0>(string("")), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("false"), std::string::npos);
}

TEST_F(NetworkDelegateTest, AGC_L1_174_GetInternetConnectionStatus_BothEthernetAndWifi)
{
    auto* mockIterator = new NiceMock<MockInterfaceDetailsIterator>();
    
    Exchange::INetworkManager::InterfaceDetails ethIface;
    ethIface.type = Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET;
    ethIface.name = "eth0";
    ethIface.connected = true;

    Exchange::INetworkManager::InterfaceDetails wifiIface;
    wifiIface.type = Exchange::INetworkManager::INTERFACE_TYPE_WIFI;
    wifiIface.name = "wlan0";
    wifiIface.connected = true;

    // Production code breaks on the first connected interface, so Next() is called
    // only once (ethernet is found and returned). The wifi entry is available in the
    // iterator but never reached — ethernet takes priority by iteration order.
    EXPECT_CALL(*mockIterator, Next(_))
        .WillOnce(DoAll(SetArgReferee<0>(ethIface), Return(true)))
        .WillRepeatedly(DoAll(SetArgReferee<0>(wifiIface), Return(false)));
    EXPECT_CALL(*mockIterator, Release())
        .WillOnce(::testing::Invoke([mockIterator]() { delete mockIterator; return 0; }));

    EXPECT_CALL(mockNetwork, GetAvailableInterfaces(_))
        .WillOnce(DoAll(SetArgReferee<0>(mockIterator), Return(Core::ERROR_NONE)));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Ethernet takes priority (first connected interface wins)
    EXPECT_NE(result.find("ethernet"), std::string::npos);
}

/* ================================================================
 * Category B – Null NetworkManager interface
 *
 * All QueryInterfaceByCallsign calls return nullptr, so
 * NetworkDelegate cannot acquire the INetworkManager interface.
 * ================================================================ */

class NetworkNoInterfaceTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = plugin.Initialize(&service);
        ASSERT_TRUE(response.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
};

TEST_F(NetworkNoInterfaceTest, AGC_L1_230_GetNetworkConnected_NoInterface_ReturnsUnavailable)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "network.connected", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_NE(result.find("NetworkManager not available"), std::string::npos);
}

TEST_F(NetworkNoInterfaceTest, AGC_L1_231_GetInternetConnectionStatus_NoInterface_ReturnsUnavailable)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "device.network", "{}", result);

    EXPECT_EQ(Core::ERROR_UNAVAILABLE, rc);
    EXPECT_NE(result.find("NetworkManager not available"), std::string::npos);
}

/* ================================================================
 * Category C – Network notification dispatch
 *
 * Capture the INetworkManager::INotification pointer during
 * subscription and fire notification callbacks to verify dispatch.
 * ================================================================ */

class NetworkNotificationTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<MockNetworkManager> mockNetwork;
    Exchange::INetworkManager::INotification* capturedNotification = nullptr;
    std::vector<MockEmitter*> heapEmitters;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(service, QueryInterfaceByCallsign(Exchange::INetworkManager::ID, ::testing::StrEq("org.rdk.NetworkManager")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                mockNetwork.AddRef();
                return static_cast<Exchange::INetworkManager*>(&mockNetwork);
            }));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        // Capture notification pointer on Register
        EXPECT_CALL(mockNetwork, Register(_)).Times(AnyNumber())
            .WillRepeatedly(::testing::Invoke([this](Exchange::INetworkManager::INotification* n) -> uint32_t {
                capturedNotification = n;
                return Core::ERROR_NONE;
            }));
        EXPECT_CALL(mockNetwork, Unregister(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

        const string response = plugin.Initialize(&service);
        ASSERT_TRUE(response.empty());
    }

    void TearDown() override
    {
        plugin.Deinitialize(&service);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        for (auto* e : heapEmitters) {
            testing::Mock::VerifyAndClearExpectations(e);
            delete e;
        }
        heapEmitters.clear();
    }
};

TEST_F(NetworkNotificationTest, AGC_L1_232_NetworkSubscription_RegistersAndCapturesNotification)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();
    bool status = false;
    const auto rc = plugin.HandleAppEventNotifier(emitter, "Network.onConnectedChanged", true, status);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);

    // Wait for the async EventRegistrationJob to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // The subscription dispatches asynchronously; verify notification was captured
    EXPECT_NE(capturedNotification, nullptr);
}

TEST_F(NetworkNotificationTest, AGC_L1_233_NetworkNotification_onActiveInterfaceChange_Dispatches)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();

    // Subscribe to Network.onConnectedChanged
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "Network.onConnectedChanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ASSERT_NE(capturedNotification, nullptr);

    // Fire onActiveInterfaceChange: empty current → disconnected
    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Network.onConnectedChanged"), _, _)).Times(::testing::AtLeast(1));
    capturedNotification->onActiveInterfaceChange("eth0", "");

    // Give worker pool time to dispatch
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

TEST_F(NetworkNotificationTest, AGC_L1_234_NetworkNotification_onInternetStatusChange_Dispatches)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();

    // Subscribe to device.onNetworkChanged
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "device.onNetworkChanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    ASSERT_NE(capturedNotification, nullptr);

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("device.onNetworkChanged"), _, _)).Times(::testing::AtLeast(1));
    capturedNotification->onInternetStatusChange(
        Exchange::INetworkManager::INTERNET_NOT_AVAILABLE,
        Exchange::INetworkManager::INTERNET_FULLY_CONNECTED,
        "eth0"
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(300));
}

} // namespace
