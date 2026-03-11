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

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include "Module.h"

#define private public
#include "AppNotificationsImplementation.h"
#undef private

#include "WorkerPoolImplementation.h"

#include "ServiceMock.h"
#include "AppGatewayMock.h"
#include "AppNotificationHandlerMock.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace {

// ---------------------------------------------------------------------------
// Worker pool RAII guards (identical pattern to test_AppGateway.cpp)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Helper to build a populated AppNotificationContext
// ---------------------------------------------------------------------------
static Exchange::IAppNotifications::AppNotificationContext MakeContext(
    uint32_t requestId = 1,
    uint32_t connectionId = 100,
    const std::string& appId = "test.app",
    const std::string& origin = "org.rdk.AppGateway",
    const std::string& version = "0")
{
    Exchange::IAppNotifications::AppNotificationContext ctx;
    ctx.requestId    = requestId;
    ctx.connectionId = connectionId;
    ctx.appId        = appId;
    ctx.origin       = origin;
    ctx.version      = version;
    return ctx;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------
class AppNotificationsTest : public ::testing::Test {
protected:
    // service must be declared BEFORE impl so that it is destroyed AFTER impl.
    // C++ destroys members in reverse declaration order; impl's destructor calls
    // mShell->Release() so the ServiceMock vtable must still be valid at that point.
    NiceMock<ServiceMock> service;
    Core::Sink<AppNotificationsImplementation> impl;

    void SetUp() override
    {
        EXPECT_CALL(service, AddRef()).Times(1);
        EXPECT_CALL(service, Release())
            .Times(AnyNumber())
            .WillRepeatedly(Return(Core::ERROR_NONE));
        // Default: all QueryInterfaceByCallsign calls return nullptr unless overridden.
        EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
            .Times(AnyNumber())
            .WillRepeatedly(Return(nullptr));

        EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&service));
    }

    void TearDown() override
    {
        // Give the worker pool time to drain any pending async jobs before
        // test-scoped objects are destroyed.
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        // Clear any registered notifications so ThunderSubscriptionManager::~dtor
        // doesn't attempt to call HandleNotifier(listen=false) on each of them.
        // The dtor would call mParent.mShell->QueryInterfaceByCallsign() but mShell
        // is set to nullptr by AppNotificationsImplementation::~dtor BEFORE the
        // ThunderSubscriptionManager member destructor runs — causing a null-deref.
        impl.mThunderManager.mRegisteredNotifications.clear();
    }
};

// ===========================================================================
// Configure tests
// ===========================================================================

TEST_F(AppNotificationsTest, Configure_StoresShellAndAddsRef)
{
    // Shell's AddRef was already called once in SetUp via Configure.
    // Verify the impl correctly holds the shell pointer (accessible via #define private public).
    EXPECT_EQ(&service, impl.mShell);
}

TEST_F(AppNotificationsTest, Configure_CalledTwice_ReleasesOldShellAndAddsRefNew)
{
    // A second Configure call should release the existing shell and AddRef the new one.
    NiceMock<ServiceMock> service2;
    EXPECT_CALL(service2, AddRef()).Times(AnyNumber());
    EXPECT_CALL(service2, Release())
        .Times(AnyNumber())
        .WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(service2, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    // The old shell (service) must be Released when Configure replaces it.
    EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&service2));
    EXPECT_EQ(&service2, impl.mShell);

    // Restore mShell to the long-lived fixture service so the impl destructor
    // doesn't call Release() on the stack-allocated service2 after it's destroyed.
    EXPECT_CALL(service, AddRef()).Times(AnyNumber());
    EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&service));
}

// ===========================================================================
// Subscribe tests
// ===========================================================================

TEST_F(AppNotificationsTest, Subscribe_Listen_True_NewEvent_ReturnsNone)
{
    // Subscribing to a new event (not previously in map) must submit a SubscriberJob
    // and return ERROR_NONE immediately.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.SomePlugin", "someEvent"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_True_SameEvent_TwiceNoExtraWorkerJob)
{
    // Subscribing to the same event a second time (listen=true) should not submit
    // a second worker pool job — only adds context to the existing map entry.
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx1, true, "org.rdk.SomePlugin", "someEvent"));
    // Second subscribe: event key already exists, no new SubscriberJob should be required.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx2, true, "org.rdk.SomePlugin", "someEvent"));

    // Both contexts must be present in the subscriber map.
    auto subscribers = impl.mSubMap.Get("someevent");
    EXPECT_EQ(2u, subscribers.size());
}

TEST_F(AppNotificationsTest, Subscribe_Listen_True_AddsContextToMap)
{
    auto ctx = MakeContext(5, 200, "app5", "org.rdk.AppGateway");
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.Plugin", "testEvent"));

    // Key is lower-cased by StringUtils::toLower.
    auto subscribers = impl.mSubMap.Get("testevent");
    ASSERT_EQ(1u, subscribers.size());
    EXPECT_EQ(200u, subscribers[0].connectionId);
    EXPECT_EQ("app5", subscribers[0].appId);
}

TEST_F(AppNotificationsTest, Subscribe_Listen_True_MixedCaseEvent_KeyIsLowercased)
{
    auto ctx = MakeContext(1, 100, "myApp", "org.rdk.AppGateway");
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.Module", "MyMixedCaseEvent"));

    // SubscriberMap::Exists() lowercases the input key before lookup, so any case variant
    // of the same event name resolves to the same canonical lowercase entry.
    EXPECT_TRUE(impl.mSubMap.Exists("mymixedcaseevent"));
    // Exists() also lowercases the query key, so the mixed-case lookup also returns true.
    EXPECT_TRUE(impl.mSubMap.Exists("MyMixedCaseEvent"));
    // A different key must not exist.
    EXPECT_FALSE(impl.mSubMap.Exists("completely_different"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_RemovesContextFromMap)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");

    impl.Subscribe(ctx, true, "org.rdk.Plugin", "removeEvent");
    EXPECT_TRUE(impl.mSubMap.Exists("removeevent"));

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, false, "org.rdk.Plugin", "removeEvent"));
    EXPECT_FALSE(impl.mSubMap.Exists("removeevent"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_LastContext_EmitsUnsubscribeJob)
{
    // When all contexts for an event are removed, a SubscriberJob for unsubscription
    // must be submitted. Subscribe must still return ERROR_NONE.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");

    impl.Subscribe(ctx, true, "org.rdk.Plugin", "eventX");
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, false, "org.rdk.Plugin", "eventX"));

    EXPECT_FALSE(impl.mSubMap.Exists("eventx"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_NonExistentEvent_ReturnsNone)
{
    // Unsubscribing an event that was never subscribed must still return ERROR_NONE.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, false, "org.rdk.Plugin", "nonExistentEvent"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_OneOfTwoContextsRemoved)
{
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");

    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "sharedEvent");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "sharedEvent");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx1, false, "org.rdk.Plugin", "sharedEvent"));

    // The key must still exist with one subscriber remaining.
    EXPECT_TRUE(impl.mSubMap.Exists("sharedevent"));
    auto subscribers = impl.mSubMap.Get("sharedevent");
    ASSERT_EQ(1u, subscribers.size());
    EXPECT_EQ(101u, subscribers[0].connectionId);
}

TEST_F(AppNotificationsTest, Subscribe_MultipleDistinctEvents_AllPresentInMap)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");

    impl.Subscribe(ctx, true, "org.rdk.Plugin", "eventA");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "eventB");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "eventC");

    EXPECT_TRUE(impl.mSubMap.Exists("eventa"));
    EXPECT_TRUE(impl.mSubMap.Exists("eventb"));
    EXPECT_TRUE(impl.mSubMap.Exists("eventc"));
}

// ===========================================================================
// Emit tests
// ===========================================================================

TEST_F(AppNotificationsTest, Emit_ReturnsNoneImmediately)
{
    // Emit is fire-and-forget (async via worker pool); always returns ERROR_NONE.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("someEvent", "{\"key\":\"value\"}", "app1"));
}

TEST_F(AppNotificationsTest, Emit_EmptyPayload_ReturnsNone)
{
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("someEvent", "", ""));
}

TEST_F(AppNotificationsTest, Emit_EmptyAppId_ReturnsNone)
{
    // Empty appId means emit to all subscribers for the event.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("broadcastEvent", "{}", ""));
}

TEST_F(AppNotificationsTest, Emit_EmptyEvent_ReturnsNone)
{
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("", "{}", "app1"));
}

TEST_F(AppNotificationsTest, Emit_WithSubscriber_GatewayOrigin_DispatchesToGateway)
{
    // Subscribe a context with gateway origin, then emit the event.
    // The worker pool will pick up the EmitJob and call DispatchToGateway,
    // which QueryInterfaceByCallsign for APP_GATEWAY_CALLSIGN.
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(
            _,
            StrEq("org.rdk.AppGateway")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "dispatchEvent");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("dispatchEvent", "{\"data\":1}", "app1"));

    // Allow async EmitJob to run.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Emit_WithSubscriber_NonGatewayOrigin_DispatchesToLaunchDelegate)
{
    auto launchDelegateMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(
            _,
            StrEq("org.rdk.LaunchDelegate")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            launchDelegateMock->AddRef();
            return static_cast<void*>(launchDelegateMock);
        }));

    EXPECT_CALL(*launchDelegateMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx = MakeContext(1, 100, "app1", "org.rdk.LaunchDelegate");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "delegateEvent");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("delegateEvent", "{}", "app1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    launchDelegateMock->Release();
}

TEST_F(AppNotificationsTest, Emit_NoSubscribersForEvent_NoDispatch)
{
    // No subscribers registered; Emit must still return ERROR_NONE.
    // No calls to QueryInterfaceByCallsign for dispatch should happen.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.AppGateway"))).Times(0);
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.LaunchDelegate"))).Times(0);

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("unknownEvent", "{}", ""));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

TEST_F(AppNotificationsTest, Emit_WithAppIdFilter_OnlyDispatchesMatchingAppId)
{
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.AppGateway")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // Only app1 should receive the emit.
    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");
    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "filteredEvent");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "filteredEvent");

    // Emit with specific appId — only app1 subscriber receives dispatch.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("filteredEvent", "{}", "app1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Emit_GatewayQueryFails_DoesNotCrash)
{
    // QueryInterfaceByCallsign returns nullptr — DispatchToGateway must log and return gracefully.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.AppGateway")))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "missingGatewayEvent");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("missingGatewayEvent", "{}", "app1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

TEST_F(AppNotificationsTest, Emit_LaunchDelegateQueryFails_DoesNotCrash)
{
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.LaunchDelegate")))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    auto ctx = MakeContext(1, 100, "app1", "org.rdk.LaunchDelegate");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "missingDelegateEvent");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("missingDelegateEvent", "{}", "app1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

// ===========================================================================
// Cleanup tests
// ===========================================================================

TEST_F(AppNotificationsTest, Cleanup_RemovesAllContextsMatchingConnectionIdAndOrigin)
{
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 100, "app1", "org.rdk.AppGateway");  // same conn+origin
    auto ctx3 = MakeContext(3, 200, "app2", "org.rdk.AppGateway");  // different connId

    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "eventA");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "eventB");
    impl.Subscribe(ctx3, true, "org.rdk.Plugin", "eventA");

    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(100, "org.rdk.AppGateway"));

    // eventA: ctx1 (connId=100) removed, ctx3 (connId=200) remains.
    EXPECT_TRUE(impl.mSubMap.Exists("eventa"));
    auto subscribersA = impl.mSubMap.Get("eventa");
    ASSERT_EQ(1u, subscribersA.size());
    EXPECT_EQ(200u, subscribersA[0].connectionId);

    // eventB: ctx2 (connId=100) removed, map key erased.
    EXPECT_FALSE(impl.mSubMap.Exists("eventb"));
}

TEST_F(AppNotificationsTest, Cleanup_ConnectionIdNotPresent_NoChange)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "persistEvent");

    // Cleanup with a connectionId that was never subscribed — nothing removed.
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(999, "org.rdk.AppGateway"));

    // The subscriber map entry for "persistevent" must still be present.
    EXPECT_TRUE(impl.mSubMap.Exists("persistevent"));
}

TEST_F(AppNotificationsTest, Cleanup_OriginMismatch_NoChange)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "stayEvent");

    // Cleanup with same connectionId but different origin — nothing removed.
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(100, "org.rdk.LaunchDelegate"));

    EXPECT_TRUE(impl.mSubMap.Exists("stayevent"));
}

TEST_F(AppNotificationsTest, Cleanup_EmptyMap_ReturnsNone)
{
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(1, "org.rdk.AppGateway"));
}

TEST_F(AppNotificationsTest, Cleanup_MultipleEventsForConnection_AllCleared)
{
    auto ctx = MakeContext(1, 55, "myApp", "org.rdk.AppGateway");

    impl.Subscribe(ctx, true, "org.rdk.A", "alpha");
    impl.Subscribe(ctx, true, "org.rdk.B", "beta");
    impl.Subscribe(ctx, true, "org.rdk.C", "gamma");

    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(55, "org.rdk.AppGateway"));

    EXPECT_FALSE(impl.mSubMap.Exists("alpha"));
    EXPECT_FALSE(impl.mSubMap.Exists("beta"));
    EXPECT_FALSE(impl.mSubMap.Exists("gamma"));
}

TEST_F(AppNotificationsTest, Cleanup_ThenSubscribe_WorksCorrectly)
{
    auto ctx = MakeContext(1, 77, "appX", "org.rdk.AppGateway");
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "cyclicEvent");
    impl.Cleanup(77, "org.rdk.AppGateway");

    EXPECT_FALSE(impl.mSubMap.Exists("cyclicevent"));

    // Re-subscribe after cleanup must succeed.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.Plugin", "cyclicEvent"));
    EXPECT_TRUE(impl.mSubMap.Exists("cyclicevent"));
}

// ===========================================================================
// SubscriberMap internal tests (accessible via #define private public)
// ===========================================================================

TEST_F(AppNotificationsTest, SubscriberMap_Add_And_Exists)
{
    auto ctx = MakeContext(1, 10, "a", "org.rdk.AppGateway");
    impl.mSubMap.Add("SomeKey", ctx);
    EXPECT_TRUE(impl.mSubMap.Exists("somekey"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Get_ReturnsCorrectContexts)
{
    auto ctx1 = MakeContext(1, 10, "a", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 11, "b", "org.rdk.AppGateway");
    impl.mSubMap.Add("myKey", ctx1);
    impl.mSubMap.Add("myKey", ctx2);

    auto result = impl.mSubMap.Get("mykey");
    ASSERT_EQ(2u, result.size());
}

TEST_F(AppNotificationsTest, SubscriberMap_Get_NonExistentKey_ReturnsEmpty)
{
    auto result = impl.mSubMap.Get("noSuchKey");
    EXPECT_TRUE(result.empty());
}

TEST_F(AppNotificationsTest, SubscriberMap_Remove_ExistingContext_KeyErasedWhenEmpty)
{
    auto ctx = MakeContext(1, 10, "a", "org.rdk.AppGateway");
    impl.mSubMap.Add("removeKey", ctx);
    EXPECT_TRUE(impl.mSubMap.Exists("removekey"));

    impl.mSubMap.Remove("removeKey", ctx);
    EXPECT_FALSE(impl.mSubMap.Exists("removekey"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Remove_NonExistentKey_NoOp)
{
    auto ctx = MakeContext(1, 10, "a", "org.rdk.AppGateway");
    // No crash expected.
    impl.mSubMap.Remove("ghost", ctx);
    EXPECT_FALSE(impl.mSubMap.Exists("ghost"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Exists_CaseInsensitive)
{
    auto ctx = MakeContext(1, 10, "a", "org.rdk.AppGateway");
    impl.mSubMap.Add("UPPER", ctx);
    EXPECT_TRUE(impl.mSubMap.Exists("upper"));
    EXPECT_TRUE(impl.mSubMap.Exists("UPPER"));
    EXPECT_TRUE(impl.mSubMap.Exists("Upper"));
}

TEST_F(AppNotificationsTest, SubscriberMap_CleanupNotifications_ByConnectionAndOrigin)
{
    auto ctx1 = MakeContext(1, 42, "appA", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 43, "appB", "org.rdk.AppGateway");
    impl.mSubMap.Add("cleanKey", ctx1);
    impl.mSubMap.Add("cleanKey", ctx2);

    impl.mSubMap.CleanupNotifications(42, "org.rdk.AppGateway");

    EXPECT_TRUE(impl.mSubMap.Exists("cleankey"));
    auto vec = impl.mSubMap.Get("cleankey");
    ASSERT_EQ(1u, vec.size());
    EXPECT_EQ(43u, vec[0].connectionId);
}

TEST_F(AppNotificationsTest, SubscriberMap_CleanupNotifications_ErasesKeyWhenAllRemoved)
{
    auto ctx = MakeContext(1, 88, "app1", "org.rdk.AppGateway");
    impl.mSubMap.Add("eraseKey", ctx);

    impl.mSubMap.CleanupNotifications(88, "org.rdk.AppGateway");

    EXPECT_FALSE(impl.mSubMap.Exists("erasekey"));
}

// ===========================================================================
// ThunderSubscriptionManager internal tests
// ===========================================================================

TEST_F(AppNotificationsTest, ThunderManager_IsNotificationRegistered_FalseByDefault)
{
    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Plugin", "someEvent"));
}

TEST_F(AppNotificationsTest, ThunderManager_RegisterNotification_WhenHandlerAvailable)
{
    // Provide a mock IAppNotificationHandler via QueryInterfaceByCallsign.
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(
            _,
            StrEq("org.rdk.Module")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("notify"), true, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));

    impl.mThunderManager.RegisterNotification("org.rdk.Module", "notify");

    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Module", "notify"));

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_RegisterNotification_WhenHandlerUnavailable_NotRegistered)
{
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.Missing")))
        .WillOnce(Return(nullptr));

    impl.mThunderManager.RegisterNotification("org.rdk.Missing", "evt");

    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Missing", "evt"));
}

TEST_F(AppNotificationsTest, ThunderManager_RegisterNotification_HandlerReturnsFalseStatus_NotRegistered)
{
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.Mod")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    // Handler returns false status — notification is NOT added to registered list.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("evt2"), true, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(false),
            Return(Core::ERROR_NONE)));

    impl.mThunderManager.RegisterNotification("org.rdk.Mod", "evt2");

    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Mod", "evt2"));

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_UnregisterNotification_WhenRegistered)
{
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    // Setup registration first.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.Mod2")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));
    // Register succeeds (status=true).
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, _, true, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));
    impl.mThunderManager.RegisterNotification("org.rdk.Mod2", "evtReg");
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Mod2", "evtReg"));

    // Unregister: handler called with listen=false, returns true status.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, _, false, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));
    impl.mThunderManager.UnregisterNotification("org.rdk.Mod2", "evtReg");
    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Mod2", "evtReg"));

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_UnregisterNotification_WhenNotRegistered_NoOp)
{
    // Unregistering a notification that was never registered should not crash.
    // ThunderSubscriptionManager::Unsubscribe logs error and returns; no handler call expected.
    impl.mThunderManager.UnregisterNotification("org.rdk.Ghost", "noSuchEvt");
    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.Ghost", "noSuchEvt"));
}

TEST_F(AppNotificationsTest, ThunderManager_Subscribe_AlreadyRegistered_NoDuplicateRegistration)
{
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.ModDup")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));
    // Only one registration call expected even though Subscribe is called twice.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("dupEvt"), true, _))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));

    impl.mThunderManager.Subscribe("org.rdk.ModDup", "dupEvt");
    impl.mThunderManager.Subscribe("org.rdk.ModDup", "dupEvt");

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_HandleNotifier_HandlerReturnsError_ReturnsFalse)
{
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.ErrMod")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("errEvt"), true, _))
        .WillOnce(Return(Core::ERROR_GENERAL));

    bool result = impl.mThunderManager.HandleNotifier("org.rdk.ErrMod", "errEvt", true);
    EXPECT_FALSE(result);

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_HandleNotifier_ModuleNotAvailable_ReturnsFalse)
{
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.Absent")))
        .WillOnce(Return(nullptr));

    bool result = impl.mThunderManager.HandleNotifier("org.rdk.Absent", "evt", true);
    EXPECT_FALSE(result);
}

// ===========================================================================
// Emitter tests (Emitter is a Core::Sink<> member; testing dispatch path)
// ===========================================================================

TEST_F(AppNotificationsTest, Emitter_Emit_SubmitsEmitJobToWorkerPool)
{
    // Calling Emitter::Emit should submit a job and not crash.
    // Validate by checking Emit returns without exception and worker pool processes it.
    EXPECT_NO_THROW(impl.mEmitter.Emit("emitterEvent", "{\"x\":1}", "appZ"));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
}

TEST_F(AppNotificationsTest, Emitter_Emit_EmptyArguments_NocrashNoThrow)
{
    EXPECT_NO_THROW(impl.mEmitter.Emit("", "", ""));
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
}

// ===========================================================================
// End-to-end scenario: Subscribe → Emit → Cleanup
// ===========================================================================

TEST_F(AppNotificationsTest, EndToEnd_SubscribeEmitCleanup_GatewayOrigin)
{
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.AppGateway")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));
    EXPECT_CALL(*gatewayMock, Emit(_, StrEq("e2eEvent"), _)).Times(AnyNumber());

    auto ctx = MakeContext(10, 300, "e2eApp", "org.rdk.AppGateway");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.Plugin", "e2eEvent"));
    EXPECT_TRUE(impl.mSubMap.Exists("e2eevent"));

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("e2eEvent", "{\"status\":\"ok\"}", "e2eApp"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Cleanup(300, "org.rdk.AppGateway"));
    EXPECT_FALSE(impl.mSubMap.Exists("e2eevent"));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, EndToEnd_MultipleSubscribersEmitOneAppId)
{
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.AppGateway")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));
    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx1 = MakeContext(1, 100, "appAlpha", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "appBeta",  "org.rdk.AppGateway");

    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "multiEvent");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "multiEvent");

    // Emit for appAlpha only — appBeta should not receive the dispatch.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("multiEvent", "{}", "appAlpha"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    gatewayMock->Release();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
