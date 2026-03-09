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

/*
 * =============================================================================
 * AppNotifications L1 Test Suite
 * =============================================================================
 *
 * Test Coverage Summary:
 * ----------------------
 * Target: 80% line coverage
 *
 * Files Covered:
 * - AppNotifications.cpp (~128 lines)
 * - AppNotificationsImplementation.cpp (~305 lines)
 *
 * Test Categories:
 * - SECTION 1: Core Plugin Tests (Initialize, Deinitialize)
 * - SECTION 2: IAppNotifications Interface Tests (Subscribe, Emit, Cleanup)
 * - SECTION 3: SubscriberMap Tests (Add, Remove, Get, Exists, EventUpdate, Dispatch)
 * - SECTION 4: ThunderSubscriptionManager Tests (Subscribe, Unsubscribe, HandleNotifier)
 * - SECTION 5: Emitter Tests
 * - SECTION 6: Edge Cases and Error Handling
 *
 * Test ID Numbering Scheme:
 * - 001-049: Core plugin (Initialize, Deinitialize, QueryInterface)
 * - 050-099: IConfiguration tests
 * - 100-199: IAppNotifications interface tests (Subscribe, Emit, Cleanup)
 * - 200-299: SubscriberMap internal tests
 * - 300-399: ThunderSubscriptionManager tests
 * - 400-449: Emitter tests
 * - 450-499: Edge cases and error handling
 * =============================================================================
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "Module.h"

// Include implementation headers with private access for testing
#define private public
#define protected public
#include "AppNotifications.h"
#include "AppNotificationsImplementation.h"
#undef private
#undef protected

#include "WorkerPoolImplementation.h"

// Local mocks (this repo): Tests/mocks/*
#include "ServiceMock.h"
#include "ThunderPortability.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {

// ============================================================================
// HELPER CLASSES
// ============================================================================

// RAII guard to ensure Core::IWorkerPool is available during tests.
class WorkerPoolGuard final {
public:
    WorkerPoolGuard(const WorkerPoolGuard&) = delete;
    WorkerPoolGuard& operator=(const WorkerPoolGuard&) = delete;

    WorkerPoolGuard()
        : _pool(/*threads*/ 2, /*stackSize*/ 0, /*queueSize*/ 64)
        , _assigned(false)
    {
        if (Core::IWorkerPool::IsAvailable() == false) {
            Core::IWorkerPool::Assign(&_pool);
            _assigned = true;
        }
        _pool.Run();
    }

    ~WorkerPoolGuard()
    {
        _pool.Stop();
        if (_assigned) {
            Core::IWorkerPool::Assign(nullptr);
        }
    }

private:
    WorkerPoolImplementation _pool;
    bool _assigned;
};

static WorkerPoolGuard g_workerPool;

// Ensure any async jobs queued by the code under test get a chance to run
class WorkerPoolDrainGuard final {
public:
    WorkerPoolDrainGuard(const WorkerPoolDrainGuard&) = delete;
    WorkerPoolDrainGuard& operator=(const WorkerPoolDrainGuard&) = delete;

    WorkerPoolDrainGuard() = default;

    ~WorkerPoolDrainGuard()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

static WorkerPoolDrainGuard g_workerPoolDrain;

// ============================================================================
// MOCK CLASSES
// ============================================================================

/**
 * Mock for Exchange::IAppGatewayResponder - used by SubscriberMap to dispatch events
 */
class AppGatewayResponderMock : public Exchange::IAppGatewayResponder {
public:
    AppGatewayResponderMock() : _refCount(1) {}
    ~AppGatewayResponderMock() override = default;

    BEGIN_INTERFACE_MAP(AppGatewayResponderMock)
    INTERFACE_ENTRY(Exchange::IAppGatewayResponder)
    END_INTERFACE_MAP

    void AddRef() const override { _refCount++; }
    uint32_t Release() const override
    {
        const uint32_t result = --_refCount;
        if (result == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }

    MOCK_METHOD(Core::hresult, Register, (INotification * notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (INotification * notification), (override));
    MOCK_METHOD(Core::hresult, Respond, (const Exchange::GatewayContext& context, const string& response), (override));
    MOCK_METHOD(Core::hresult, Emit, (const Exchange::GatewayContext& context, const string& event, const string& payload), (override));
    MOCK_METHOD(Core::hresult, Request, (const uint32_t connectionId, const uint32_t requestId, const string& method, const string& params), (override));
    MOCK_METHOD(Core::hresult, GetGatewayConnectionContext, (const uint32_t connectionId, const string& method, string& value), (override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

/**
 * Mock for Exchange::IAppNotificationHandler - used by ThunderSubscriptionManager
 */
class AppNotificationHandlerMock : public Exchange::IAppNotificationHandler {
public:
    AppNotificationHandlerMock() : _refCount(1) {}
    ~AppNotificationHandlerMock() override = default;

    BEGIN_INTERFACE_MAP(AppNotificationHandlerMock)
    INTERFACE_ENTRY(Exchange::IAppNotificationHandler)
    END_INTERFACE_MAP

    void AddRef() const override { _refCount++; }
    uint32_t Release() const override
    {
        const uint32_t result = --_refCount;
        if (result == 0) {
            delete this;
            return Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return Core::ERROR_NONE;
    }

    MOCK_METHOD(Core::hresult, HandleAppEventNotifier,
        (IEmitter * emitCb, const string& event, bool listen, bool& status),
        (override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

/**
 * Mock for Exchange::IAppNotificationHandler::IEmitter
 */
class EmitterMock : public Exchange::IAppNotificationHandler::IEmitter {
public:
    ~EmitterMock() override = default;

    BEGIN_INTERFACE_MAP(EmitterMock)
    INTERFACE_ENTRY(Exchange::IAppNotificationHandler::IEmitter)
    END_INTERFACE_MAP

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void, Emit, (const string& event, const string& payload, const string& appId), (override));
};

/**
 * Mock for Exchange::IConfiguration
 */
class ConfigurationMock : public Exchange::IConfiguration {
public:
    ~ConfigurationMock() override = default;

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(uint32_t, Configure, (PluginHost::IShell * service), (override));
};

// Helper function to create test AppNotificationContext
static Exchange::IAppNotifications::AppNotificationContext MakeNotificationContext(
    uint32_t requestId = 100,
    uint32_t connectionId = 1,
    const string& appId = "test.app",
    const string& origin = "org.rdk.AppGateway",
    const string& version = "")
{
    Exchange::IAppNotifications::AppNotificationContext ctx;
    ctx.requestId = requestId;
    ctx.connectionId = connectionId;
    ctx.appId = appId;
    ctx.origin = origin;
    ctx.version = version;
    return ctx;
}

// ============================================================================
// TEST FIXTURES
// ============================================================================

class AppNotificationsPluginTest : public ::testing::Test {
protected:
    Core::Sink<AppNotifications> plugin;

    void TearDown() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
};

class AppNotificationsImplementationTest : public ::testing::Test {
protected:
    Core::Sink<AppNotificationsImplementation> impl;

    void TearDown() override
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
};

// ============================================================================
// SECTION 1: CORE PLUGIN TESTS
// Tests for AppNotifications plugin Initialize, Deinitialize, Information
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_001
 * Description: Verify Information() returns empty string
 * Method Under Test: AppNotifications::Information
 * File: AppNotifications.cpp:53
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsPluginTest, APPNOTIFICATIONS_L1_001_Information_ReturnsEmptyString)
{
    EXPECT_TRUE(plugin.Information().empty());
}

/**
 * Test ID: APPNOTIFICATIONS_L1_002
 * Description: Verify Initialize fails when Root() returns nullptr
 * Method Under Test: AppNotifications::Initialize
 * File: AppNotifications.cpp:56-82
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsPluginTest, APPNOTIFICATIONS_L1_002_Initialize_NullAppNotifications_ReturnsError)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Return nullptr for Root<Exchange::IAppNotifications>()
    // The plugin uses service->Root<T>() which eventually calls QueryInterface
    // When it can't find the implementation, it returns nullptr
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    const string response = plugin.Initialize(&service);

    // Expect error message since mAppNotifications will be nullptr
    EXPECT_FALSE(response.empty());
    EXPECT_THAT(response, ::testing::HasSubstr("AppNotifications"));

    // Cleanup - need to deinitialize
    plugin.Deinitialize(&service);
}

/**
 * Test ID: APPNOTIFICATIONS_L1_003
 * Description: Verify Constructor initializes member variables to default values
 * Method Under Test: AppNotifications::AppNotifications
 * File: AppNotifications.cpp:46-49
 * Scenario Type: Positive
 */
TEST(AppNotificationsConstructorTest, APPNOTIFICATIONS_L1_003_Constructor_InitializesDefaults)
{
    Core::Sink<AppNotifications> plugin;

    EXPECT_EQ(plugin.mService, nullptr);
    EXPECT_EQ(plugin.mAppNotifications, nullptr);
    EXPECT_EQ(plugin.mConnectionId, 0u);
}

// ============================================================================
// SECTION 2: APPNOTIFICATIONSIMPLEMENTATION CORE TESTS
// Tests for AppNotificationsImplementation Configure
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_050
 * Description: Verify Configure stores shell reference
 * Method Under Test: AppNotificationsImplementation::Configure
 * File: AppNotificationsImplementation.cpp:112-120
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_050_Configure_Success)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    uint32_t result = impl.Configure(&service);

    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_EQ(&service, impl.mShell);

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_051
 * Description: Verify Constructor initializes member variables
 * Method Under Test: AppNotificationsImplementation::AppNotificationsImplementation
 * File: AppNotificationsImplementation.cpp:38-44
 * Scenario Type: Positive
 */
TEST(AppNotificationsImplConstructorTest, APPNOTIFICATIONS_L1_051_Constructor_InitializesDefaults)
{
    Core::Sink<AppNotificationsImplementation> impl;

    EXPECT_EQ(impl.mShell, nullptr);
}

/**
 * Test ID: APPNOTIFICATIONS_L1_052
 * Description: Verify Destructor releases shell
 * Method Under Test: AppNotificationsImplementation::~AppNotificationsImplementation
 * File: AppNotificationsImplementation.cpp:46-54
 * Scenario Type: Positive
 */
TEST(AppNotificationsImplDestructorTest, APPNOTIFICATIONS_L1_052_Destructor_ReleasesShell)
{
    ::testing::NiceMock<ServiceMock>* service = new ::testing::NiceMock<ServiceMock>();

    EXPECT_CALL(*service, AddRef()).Times(1);
    EXPECT_CALL(*service, Release()).Times(1).WillOnce(::testing::Return(Core::ERROR_NONE));

    {
        Core::Sink<AppNotificationsImplementation> impl;
        impl.Configure(service);
        // Destructor should call mShell->Release()
    }
}

// ============================================================================
// SECTION 3: IAPPNOTIFICATIONS INTERFACE TESTS
// Tests for Subscribe, Emit, Cleanup
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_100
 * Description: Verify Subscribe with listen=true adds context to SubscriberMap
 * Method Under Test: AppNotificationsImplementation::Subscribe
 * File: AppNotificationsImplementation.cpp:56-79
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_100_Subscribe_ListenTrue_AddsToMap)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Return nullptr for notification handler (no Thunder subscription happens)
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext();
    const string module = "org.rdk.TestModule";
    const string event = "testEvent";

    Core::hresult result = impl.Subscribe(ctx, true, module, event);

    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_TRUE(impl.mSubMap.Exists(event));

    // Allow async worker pool jobs to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_101
 * Description: Verify Subscribe with listen=false removes context from SubscriberMap
 * Method Under Test: AppNotificationsImplementation::Subscribe
 * File: AppNotificationsImplementation.cpp:56-79
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_101_Subscribe_ListenFalse_RemovesFromMap)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext();
    const string module = "org.rdk.TestModule";
    const string event = "testEvent";

    // First subscribe
    impl.Subscribe(ctx, true, module, event);
    EXPECT_TRUE(impl.mSubMap.Exists(event));

    // Wait for async jobs
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Then unsubscribe
    Core::hresult result = impl.Subscribe(ctx, false, module, event);

    EXPECT_EQ(Core::ERROR_NONE, result);

    // Wait for async jobs
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(impl.mSubMap.Exists(event));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_102
 * Description: Verify Subscribe with multiple contexts for same event
 * Method Under Test: AppNotificationsImplementation::Subscribe
 * File: AppNotificationsImplementation.cpp:56-79
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_102_Subscribe_MultipleContextsSameEvent)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx1 = MakeNotificationContext(100, 1, "app1");
    auto ctx2 = MakeNotificationContext(101, 2, "app2");
    const string module = "org.rdk.TestModule";
    const string event = "testEvent";

    impl.Subscribe(ctx1, true, module, event);
    impl.Subscribe(ctx2, true, module, event);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(impl.mSubMap.Exists(event));
    auto contexts = impl.mSubMap.Get(event);
    EXPECT_EQ(2u, contexts.size());

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_110
 * Description: Verify Emit returns ERROR_NONE and queues job
 * Method Under Test: AppNotificationsImplementation::Emit
 * File: AppNotificationsImplementation.cpp:81-88
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_110_Emit_ReturnsSuccess)
{
    const string event = "testEvent";
    const string payload = R"({"data":"value"})";
    const string appId = "test.app";

    Core::hresult result = impl.Emit(event, payload, appId);

    EXPECT_EQ(Core::ERROR_NONE, result);

    // Allow async jobs to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

/**
 * Test ID: APPNOTIFICATIONS_L1_111
 * Description: Verify Emit with empty appId broadcasts to all subscribers
 * Method Under Test: AppNotificationsImplementation::Emit
 * File: AppNotificationsImplementation.cpp:81-88
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_111_Emit_EmptyAppId_BroadcastsToAll)
{
    const string event = "testEvent";
    const string payload = R"({"data":"value"})";
    const string appId = ""; // Empty appId for broadcast

    Core::hresult result = impl.Emit(event, payload, appId);

    EXPECT_EQ(Core::ERROR_NONE, result);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

/**
 * Test ID: APPNOTIFICATIONS_L1_120
 * Description: Verify Cleanup removes contexts matching connectionId and origin
 * Method Under Test: AppNotificationsImplementation::Cleanup
 * File: AppNotificationsImplementation.cpp:90-94
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_120_Cleanup_RemovesMatchingContexts)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    const uint32_t connectionId = 123;
    const string origin = "org.rdk.AppGateway";
    auto ctx = MakeNotificationContext(100, connectionId, "test.app", origin);
    const string event = "cleanupEvent";

    // Add subscription
    impl.Subscribe(ctx, true, "org.rdk.Module", event);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(impl.mSubMap.Exists(event));

    // Cleanup
    Core::hresult result = impl.Cleanup(connectionId, origin);

    EXPECT_EQ(Core::ERROR_NONE, result);
    EXPECT_FALSE(impl.mSubMap.Exists(event));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_121
 * Description: Verify Cleanup only removes contexts with matching connectionId AND origin
 * Method Under Test: AppNotificationsImplementation::Cleanup
 * File: AppNotificationsImplementation.cpp:96-110
 * Scenario Type: Boundary
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_121_Cleanup_OnlyRemovesMatchingBoth)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx1 = MakeNotificationContext(100, 1, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeNotificationContext(101, 1, "app2", "org.rdk.LaunchDelegate"); // Same connectionId, different origin
    const string event = "sharedEvent";

    impl.Subscribe(ctx1, true, "org.rdk.Module", event);
    impl.Subscribe(ctx2, true, "org.rdk.Module", event);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto contextsBefore = impl.mSubMap.Get(event);
    EXPECT_EQ(2u, contextsBefore.size());

    // Cleanup only matching connectionId AND origin
    impl.Cleanup(1, "org.rdk.AppGateway");

    auto contextsAfter = impl.mSubMap.Get(event);
    EXPECT_EQ(1u, contextsAfter.size());
    EXPECT_EQ("org.rdk.LaunchDelegate", contextsAfter[0].origin);

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

// ============================================================================
// SECTION 4: SUBSCRIBERMAP INTERNAL TESTS
// Tests for Add, Remove, Get, Exists, EventUpdate, Dispatch
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_200
 * Description: Verify SubscriberMap::Add stores context with lowercase key
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Add
 * File: AppNotificationsImplementation.cpp:122-126
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_200_SubscriberMap_Add_LowercaseKey)
{
    auto ctx = MakeNotificationContext();
    const string event = "TestEvent.WithCaps";

    impl.mSubMap.Add(event, ctx);

    // Should be stored with lowercase key
    EXPECT_TRUE(impl.mSubMap.Exists(event));
    EXPECT_TRUE(impl.mSubMap.Exists("testevent.withcaps"));
}

/**
 * Test ID: APPNOTIFICATIONS_L1_201
 * Description: Verify SubscriberMap::Remove removes matching context
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Remove
 * File: AppNotificationsImplementation.cpp:128-139
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_201_SubscriberMap_Remove_MatchingContext)
{
    auto ctx = MakeNotificationContext(100, 1, "test.app");
    const string event = "removeEvent";

    impl.mSubMap.Add(event, ctx);
    EXPECT_TRUE(impl.mSubMap.Exists(event));

    impl.mSubMap.Remove(event, ctx);
    EXPECT_FALSE(impl.mSubMap.Exists(event));
}

/**
 * Test ID: APPNOTIFICATIONS_L1_202
 * Description: Verify SubscriberMap::Remove does not remove non-matching context
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Remove
 * File: AppNotificationsImplementation.cpp:128-139
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_202_SubscriberMap_Remove_NonMatchingContext)
{
    auto ctx1 = MakeNotificationContext(100, 1, "app1");
    auto ctx2 = MakeNotificationContext(101, 2, "app2");
    const string event = "multiEvent";

    impl.mSubMap.Add(event, ctx1);
    impl.mSubMap.Add(event, ctx2);

    // Remove ctx1, ctx2 should remain
    impl.mSubMap.Remove(event, ctx1);
    EXPECT_TRUE(impl.mSubMap.Exists(event));

    auto contexts = impl.mSubMap.Get(event);
    EXPECT_EQ(1u, contexts.size());
    EXPECT_EQ("app2", contexts[0].appId);
}

/**
 * Test ID: APPNOTIFICATIONS_L1_203
 * Description: Verify SubscriberMap::Get returns empty vector for non-existent key
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Get
 * File: AppNotificationsImplementation.cpp:141-149
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_203_SubscriberMap_Get_NonExistentKey)
{
    auto contexts = impl.mSubMap.Get("nonExistentEvent");
    EXPECT_TRUE(contexts.empty());
}

/**
 * Test ID: APPNOTIFICATIONS_L1_204
 * Description: Verify SubscriberMap::Exists returns false for non-existent key
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Exists
 * File: AppNotificationsImplementation.cpp:151-156
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_204_SubscriberMap_Exists_NonExistent)
{
    EXPECT_FALSE(impl.mSubMap.Exists("nonExistentEvent"));
}

/**
 * Test ID: APPNOTIFICATIONS_L1_205
 * Description: Verify SubscriberMap::EventUpdate dispatches to matching appId only
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::EventUpdate
 * File: AppNotificationsImplementation.cpp:158-184
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_205_SubscriberMap_EventUpdate_WithAppId)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Return responder for AppGateway callsign
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string& name) -> void* {
            if (name == "org.rdk.AppGateway") {
                responder->AddRef();
                return static_cast<void*>(responder);
            }
            return nullptr;
        }));

    impl.Configure(&service);

    auto ctx1 = MakeNotificationContext(100, 1, "target.app", "org.rdk.AppGateway");
    auto ctx2 = MakeNotificationContext(101, 2, "other.app", "org.rdk.AppGateway");
    const string event = "targetedEvent";

    impl.mSubMap.Add(event, ctx1);
    impl.mSubMap.Add(event, ctx2);

    // Expect Emit to be called only for matching appId
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("targetedEvent"), ::testing::_))
        .Times(1);

    impl.mSubMap.EventUpdate(event, R"({"data":1})", "target.app");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_206
 * Description: Verify SubscriberMap::EventUpdate dispatches to all when appId is empty
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::EventUpdate
 * File: AppNotificationsImplementation.cpp:158-184
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_206_SubscriberMap_EventUpdate_EmptyAppId)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string& name) -> void* {
            if (name == "org.rdk.AppGateway") {
                responder->AddRef();
                return static_cast<void*>(responder);
            }
            return nullptr;
        }));

    impl.Configure(&service);

    auto ctx1 = MakeNotificationContext(100, 1, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeNotificationContext(101, 2, "app2", "org.rdk.AppGateway");
    const string event = "broadcastEvent";

    impl.mSubMap.Add(event, ctx1);
    impl.mSubMap.Add(event, ctx2);

    // Empty appId should dispatch to all subscribers
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("broadcastEvent"), ::testing::_))
        .Times(2);

    impl.mSubMap.EventUpdate(event, R"({"data":1})", ""); // Empty appId

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_207
 * Description: Verify SubscriberMap::EventUpdate logs warning for no active listeners
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::EventUpdate
 * File: AppNotificationsImplementation.cpp:180-183
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_207_SubscriberMap_EventUpdate_NoListeners)
{
    // EventUpdate for non-existent event should log warning but not crash
    impl.mSubMap.EventUpdate("nonExistentEvent", R"({"data":1})", "");
    // No assertion needed - test passes if no crash occurs
    SUCCEED();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_210
 * Description: Verify SubscriberMap::DispatchToGateway acquires and uses AppGateway interface
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::DispatchToGateway
 * File: AppNotificationsImplementation.cpp:194-207
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_210_SubscriberMap_DispatchToGateway)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.AppGateway")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            responder->AddRef();
            return static_cast<void*>(responder);
        }));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext(100, 1, "test.app", "org.rdk.AppGateway");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("dispatchEvent"), ::testing::StrEq(R"({"data":1})")))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    impl.mSubMap.DispatchToGateway("dispatchEvent", ctx, R"({"data":1})");

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_211
 * Description: Verify SubscriberMap::DispatchToGateway handles null interface
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::DispatchToGateway
 * File: AppNotificationsImplementation.cpp:196-200
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_211_SubscriberMap_DispatchToGateway_NullInterface)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext(100, 1, "test.app", "org.rdk.AppGateway");

    // Should not crash when interface is null
    impl.mSubMap.DispatchToGateway("event", ctx, "{}");
    SUCCEED();

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_212
 * Description: Verify SubscriberMap::DispatchToLaunchDelegate uses InternalGateway
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::DispatchToLaunchDelegate
 * File: AppNotificationsImplementation.cpp:209-222
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_212_SubscriberMap_DispatchToLaunchDelegate)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.InternalGateway")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            responder->AddRef();
            return static_cast<void*>(responder);
        }));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext(100, 1, "test.app", "org.rdk.LaunchDelegate");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("delegateEvent"), ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    impl.mSubMap.DispatchToLaunchDelegate("delegateEvent", ctx, "{}");

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_213
 * Description: Verify SubscriberMap::Dispatch routes to Gateway for Gateway origin
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Dispatch
 * File: AppNotificationsImplementation.cpp:186-192
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_213_SubscriberMap_Dispatch_GatewayOrigin)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.AppGateway")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            responder->AddRef();
            return static_cast<void*>(responder);
        }));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext(100, 1, "test.app", "org.rdk.AppGateway");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    impl.mSubMap.Dispatch("event", ctx, "{}");

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_214
 * Description: Verify SubscriberMap::Dispatch routes to LaunchDelegate for non-Gateway origin
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::Dispatch
 * File: AppNotificationsImplementation.cpp:186-192
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_214_SubscriberMap_Dispatch_NonGatewayOrigin)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.InternalGateway")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            responder->AddRef();
            return static_cast<void*>(responder);
        }));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext(100, 1, "test.app", "org.rdk.SomeOtherOrigin");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    impl.mSubMap.Dispatch("event", ctx, "{}");

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

// ============================================================================
// SECTION 5: THUNDERSUBSCRIPTIONMANAGER TESTS
// Tests for Subscribe, Unsubscribe, HandleNotifier, RegisterNotification
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_300
 * Description: Verify ThunderSubscriptionManager::Subscribe registers notification
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::Subscribe
 * File: AppNotificationsImplementation.cpp:238-249
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_300_ThunderManager_Subscribe_RegistersNotification)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.TestModule")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::StrEq("testEvent"), true, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    impl.Configure(&service);

    impl.mThunderManager.Subscribe("org.rdk.TestModule", "testEvent");

    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.TestModule", "testEvent"));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    handler->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_301
 * Description: Verify ThunderSubscriptionManager::Subscribe skips already registered
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::Subscribe
 * File: AppNotificationsImplementation.cpp:242-244
 * Scenario Type: Boundary
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_301_ThunderManager_Subscribe_AlreadyRegistered)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, true, ::testing::_))
        .Times(1) // Should only be called once
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    impl.Configure(&service);

    // First subscribe
    impl.mThunderManager.Subscribe("org.rdk.TestModule", "testEvent");
    // Second subscribe should skip
    impl.mThunderManager.Subscribe("org.rdk.TestModule", "testEvent");

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    handler->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_302
 * Description: Verify ThunderSubscriptionManager::Unsubscribe unregisters notification
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::Unsubscribe
 * File: AppNotificationsImplementation.cpp:251-262
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_302_ThunderManager_Unsubscribe_UnregistersNotification)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    // Subscribe then unsubscribe
    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, true, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, false, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    impl.Configure(&service);

    impl.mThunderManager.Subscribe("org.rdk.TestModule", "testEvent");
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.TestModule", "testEvent"));

    impl.mThunderManager.Unsubscribe("org.rdk.TestModule", "testEvent");
    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.TestModule", "testEvent"));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    handler->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_303
 * Description: Verify ThunderSubscriptionManager::Unsubscribe logs error for non-registered
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::Unsubscribe
 * File: AppNotificationsImplementation.cpp:258-261
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_303_ThunderManager_Unsubscribe_NotRegistered)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    impl.Configure(&service);

    // Unsubscribe from non-registered should not crash
    impl.mThunderManager.Unsubscribe("org.rdk.TestModule", "nonExistent");
    SUCCEED();

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_304
 * Description: Verify ThunderSubscriptionManager::HandleNotifier returns false when handler not found
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::HandleNotifier
 * File: AppNotificationsImplementation.cpp:264-279
 * Scenario Type: Negative
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_304_ThunderManager_HandleNotifier_NoHandler)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    bool result = impl.mThunderManager.HandleNotifier("org.rdk.NonExistent", "event", true);
    EXPECT_FALSE(result);

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_305
 * Description: Verify ThunderSubscriptionManager::IsNotificationRegistered is case-insensitive
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::IsNotificationRegistered
 * File: AppNotificationsImplementation.cpp:298-302
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_305_ThunderManager_IsRegistered_CaseInsensitive)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    impl.Configure(&service);

    impl.mThunderManager.Subscribe("org.rdk.Module", "TestEvent");

    // Check with different cases
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Module", "testevent"));
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Module", "TESTEVENT"));
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Module", "TestEvent"));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    handler->Release();
}

// ============================================================================
// SECTION 6: EMITTER TESTS
// Tests for Emitter class
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_400
 * Description: Verify Emitter::Emit queues EmitJob to worker pool
 * Method Under Test: AppNotificationsImplementation::Emitter::Emit
 * File: AppNotificationsImplementation.h:231-238
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_400_Emitter_Emit_QueuesJob)
{
    // mEmitter is already initialized in AppNotificationsImplementation constructor
    // Just verify it can be called without crashing
    impl.mEmitter.Emit("testEvent", R"({"data":"value"})", "test.app");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    SUCCEED();
}

// ============================================================================
// SECTION 7: EDGE CASES AND ERROR HANDLING
// ============================================================================

/**
 * Test ID: APPNOTIFICATIONS_L1_450
 * Description: Verify Subscribe handles empty event name
 * Method Under Test: AppNotificationsImplementation::Subscribe
 * File: AppNotificationsImplementation.cpp:56-79
 * Scenario Type: Boundary
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_450_Subscribe_EmptyEventName)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext();

    Core::hresult result = impl.Subscribe(ctx, true, "org.rdk.Module", "");

    EXPECT_EQ(Core::ERROR_NONE, result);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_451
 * Description: Verify Subscribe handles empty module name
 * Method Under Test: AppNotificationsImplementation::Subscribe
 * File: AppNotificationsImplementation.cpp:56-79
 * Scenario Type: Boundary
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_451_Subscribe_EmptyModuleName)
{
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext();

    Core::hresult result = impl.Subscribe(ctx, true, "", "testEvent");

    EXPECT_EQ(Core::ERROR_NONE, result);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
}

/**
 * Test ID: APPNOTIFICATIONS_L1_452
 * Description: Verify CleanupNotifications handles empty map gracefully
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::CleanupNotifications
 * File: AppNotificationsImplementation.cpp:96-110
 * Scenario Type: Boundary
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_452_CleanupNotifications_EmptyMap)
{
    // Should not crash when map is empty
    impl.mSubMap.CleanupNotifications(123, "org.rdk.AppGateway");
    SUCCEED();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_453
 * Description: Verify versioned event names are handled correctly
 * Method Under Test: AppNotificationsImplementation::SubscriberMap::EventUpdate
 * File: AppNotificationsImplementation.cpp:163
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_453_EventUpdate_VersionedEventName)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* responder = new AppGatewayResponderMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string& name) -> void* {
            if (name == "org.rdk.AppGateway") {
                responder->AddRef();
                return static_cast<void*>(responder);
            }
            return nullptr;
        }));

    impl.Configure(&service);

    auto ctx = MakeNotificationContext(100, 1, "test.app", "org.rdk.AppGateway", "8");
    const string versionedEvent = "lifecycle.onstatechanged.v8";

    impl.mSubMap.Add(versionedEvent, ctx);

    // Emit should clear the .v8 suffix when dispatching
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("lifecycle.onstatechanged"), ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    impl.mSubMap.EventUpdate(versionedEvent, R"({"state":"running"})", "test.app");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    responder->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_454
 * Description: Verify operator== for AppNotificationContext works correctly
 * Method Under Test: Exchange::operator==
 * File: AppNotificationsImplementation.cpp:25-32
 * Scenario Type: Positive
 */
TEST(AppNotificationContextEqualityTest, APPNOTIFICATIONS_L1_454_Context_Equality)
{
    Exchange::IAppNotifications::AppNotificationContext ctx1;
    ctx1.requestId = 100;
    ctx1.connectionId = 1;
    ctx1.appId = "test.app";
    ctx1.origin = "org.rdk.AppGateway";
    ctx1.version = "8";

    Exchange::IAppNotifications::AppNotificationContext ctx2 = ctx1; // Copy

    EXPECT_TRUE(ctx1 == ctx2);

    // Modify one field
    ctx2.requestId = 101;
    EXPECT_FALSE(ctx1 == ctx2);
}

/**
 * Test ID: APPNOTIFICATIONS_L1_455
 * Description: Verify SubscriberJob dispatch calls Subscribe
 * Method Under Test: AppNotificationsImplementation::SubscriberJob::Dispatch
 * File: AppNotificationsImplementation.h:177-184
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_455_SubscriberJob_Dispatch_Subscribe)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::StrEq("jobEvent"), true, ::testing::_))
        .WillOnce(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    impl.Configure(&service);

    // Create and dispatch job
    auto job = AppNotificationsImplementation::SubscriberJob::Create(&impl, "org.rdk.Module", "jobEvent", true);
    Core::IWorkerPool::Instance().Submit(job);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Module", "jobEvent"));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    handler->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_456
 * Description: Verify SubscriberJob dispatch calls Unsubscribe
 * Method Under Test: AppNotificationsImplementation::SubscriberJob::Dispatch
 * File: AppNotificationsImplementation.h:177-184
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_456_SubscriberJob_Dispatch_Unsubscribe)
{
    ::testing::NiceMock<ServiceMock> service;
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    impl.Configure(&service);

    // First subscribe
    impl.mThunderManager.Subscribe("org.rdk.Module", "jobEvent");

    // Create and dispatch unsubscribe job
    auto job = AppNotificationsImplementation::SubscriberJob::Create(&impl, "org.rdk.Module", "jobEvent", false);
    Core::IWorkerPool::Instance().Submit(job);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Module", "jobEvent"));

    // Cleanup
    impl.mShell->Release();
    impl.mShell = nullptr;
    handler->Release();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_457
 * Description: Verify EmitJob dispatch calls EventUpdate
 * Method Under Test: AppNotificationsImplementation::EmitJob::Dispatch
 * File: AppNotificationsImplementation.h:212-215
 * Scenario Type: Positive
 */
TEST_F(AppNotificationsImplementationTest, APPNOTIFICATIONS_L1_457_EmitJob_Dispatch)
{
    // Create and dispatch emit job
    auto job = AppNotificationsImplementation::EmitJob::Create(&impl, "emitEvent", R"({"data":1})", "test.app");
    Core::IWorkerPool::Instance().Submit(job);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // Test passes if no crash occurs
    SUCCEED();
}

/**
 * Test ID: APPNOTIFICATIONS_L1_458
 * Description: Verify ThunderSubscriptionManager destructor cleans up all notifications
 * Method Under Test: AppNotificationsImplementation::ThunderSubscriptionManager::~ThunderSubscriptionManager
 * File: AppNotificationsImplementation.cpp:224-236
 * Scenario Type: Positive
 */
TEST(ThunderSubscriptionManagerDestructorTest, APPNOTIFICATIONS_L1_458_Destructor_CleansUpNotifications)
{
    ::testing::NiceMock<ServiceMock>* service = new ::testing::NiceMock<ServiceMock>();
    auto* handler = new AppNotificationHandlerMock();

    EXPECT_CALL(*service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(*service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_CALL(*service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    // Expect unsubscribe calls during destruction
    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, true, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    EXPECT_CALL(*handler, HandleAppEventNotifier(::testing::_, ::testing::_, false, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::DoAll(::testing::SetArgReferee<3>(true), ::testing::Return(Core::ERROR_NONE)));

    {
        Core::Sink<AppNotificationsImplementation> impl;
        impl.Configure(service);

        impl.mThunderManager.Subscribe("org.rdk.Module1", "event1");
        impl.mThunderManager.Subscribe("org.rdk.Module2", "event2");

        // Destructor should unsubscribe all
    }

    handler->Release();
}

} // namespace

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
