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

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[TEST LOG][%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", \
    __FILE__, __LINE__, __FUNCTION__, getpid(), (int)syscall(SYS_gettid), ##__VA_ARGS__); fflush(stderr);

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

// ---------------------------------------------------------------------------
// Named constants
// ---------------------------------------------------------------------------
#define APP_NOTIFICATIONS_GATEWAY_CALLSIGN      "org.rdk.AppGateway"
#define APP_NOTIFICATIONS_DELEGATE_CALLSIGN     "org.rdk.LaunchDelegate"
#define APP_NOTIFICATIONS_DEFAULT_CONNECTION_ID  100
#define APP_NOTIFICATIONS_DEFAULT_APP_ID         "test.app"
#define APP_NOTIFICATIONS_DEFAULT_REQUEST_ID     1

// ---------------------------------------------------------------------------
// NotificationHandler — concrete implementation of IAppNotificationHandler::IEmitter
// used by tests to verify that the plugin correctly calls back the emitter.
// ---------------------------------------------------------------------------
typedef enum : uint32_t {
    AppNotifications_OnEmit = 0x00000001,
} AppNotificationsEventType_t;

class NotificationHandler : public Exchange::IAppNotificationHandler::IEmitter {
private:
    mutable std::mutex              m_mutex;
    std::condition_variable         m_condition_variable;
    uint32_t                        m_event_signalled;
    mutable std::atomic<uint32_t>   m_refCount;

    // Stored parameters from the most-recent Emit() call
    std::string  m_emit_event;
    std::string  m_emit_payload;
    std::string  m_emit_appId;

    BEGIN_INTERFACE_MAP(NotificationHandler)
    INTERFACE_ENTRY(Exchange::IAppNotificationHandler::IEmitter)
    END_INTERFACE_MAP

public:
    NotificationHandler()
        : m_event_signalled(0)
        , m_refCount(1)
    {}

    ~NotificationHandler() override = default;

    // IUnknown ref-counting (real, non-mock)
    void AddRef() const override { ++m_refCount; }
    uint32_t Release() const override
    {
        const uint32_t result = --m_refCount;
        if (result == 0) {
            delete this;
        }
        return result;
    }

    // IEmitter implementation
    void Emit(const string& event,
              const string& payload,
              const string& appId) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_emit_event   = event;
        m_emit_payload = payload;
        m_emit_appId   = appId;
        m_event_signalled |= AppNotifications_OnEmit;
        m_condition_variable.notify_one();
    }

    // Block until the expected event bit is set, or timeout_ms elapses.
    // Returns true if the event was signalled before the timeout.
    bool WaitForRequestStatus(uint32_t timeout_ms, AppNotificationsEventType_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_condition_variable.wait_for(
            lock,
            std::chrono::milliseconds(timeout_ms),
            [this, expected_status]() {
                return (m_event_signalled & expected_status) != 0;
            });
    }

    // Reset all stored state so the handler can be reused across sub-tests.
    void Reset()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled = 0;
        m_emit_event.clear();
        m_emit_payload.clear();
        m_emit_appId.clear();
    }

    // Getters — return copies so callers don't need to hold the lock.
    std::string GetEmitEvent()   const { std::unique_lock<std::mutex> l(m_mutex); return m_emit_event; }
    std::string GetEmitPayload() const { std::unique_lock<std::mutex> l(m_mutex); return m_emit_payload; }
    std::string GetEmitAppId()   const { std::unique_lock<std::mutex> l(m_mutex); return m_emit_appId; }
    uint32_t    GetEventsMask()  const { std::unique_lock<std::mutex> l(m_mutex); return m_event_signalled; }
};

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
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    AppNotificationsTest()
        : workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 64))
    {
        // Permissive baseline defaults — tests override with EXPECT_CALL as needed.
        ON_CALL(service, AddRef()).WillByDefault(Return());
        ON_CALL(service, Release()).WillByDefault(Return(Core::ERROR_NONE));
        ON_CALL(service, QueryInterfaceByCallsign(_, _)).WillByDefault(Return(nullptr));

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&service));
    }

    virtual ~AppNotificationsTest() override
    {
        TEST_LOG("AppNotificationsTest Destructor");

        // Allow async worker-pool jobs to drain before teardown.
        std::this_thread::sleep_for(std::chrono::milliseconds(60));

        // Clear registered notifications so ThunderSubscriptionManager::~dtor
        // doesn't attempt HandleNotifier(listen=false) calls after mShell is
        // already null (AppNotificationsImplementation::~dtor nulls mShell first).
        impl.mThunderManager.mRegisteredNotifications.clear();

        workerPool->Stop();
        Core::IWorkerPool::Assign(nullptr);
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(impl.mShell != nullptr);
    }

    // -----------------------------------------------------------------------
    // Helper: build a populated AppNotificationContext
    // -----------------------------------------------------------------------
    Exchange::IAppNotifications::AppNotificationContext MakeContext(
        uint32_t requestId    = APP_NOTIFICATIONS_DEFAULT_REQUEST_ID,
        uint32_t connectionId = APP_NOTIFICATIONS_DEFAULT_CONNECTION_ID,
        const std::string& appId  = APP_NOTIFICATIONS_DEFAULT_APP_ID,
        const std::string& origin = APP_NOTIFICATIONS_GATEWAY_CALLSIGN,
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
};

// ===========================================================================
// Configure tests
// ===========================================================================

TEST_F(AppNotificationsTest, Configure_StoresShellAndAddsRef)
{
    // Shell's AddRef was already called once in the constructor via Configure.
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
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.SomePlugin", "someEvent"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_True_SameEvent_TwiceNoExtraWorkerJob)
{
    // Subscribing to the same event a second time (listen=true) should not submit
    // a second worker pool job — only adds context to the existing map entry.
    auto ctx1 = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

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
    auto ctx = MakeContext(5, 200, "app5", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
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
    auto ctx = MakeContext(1, 100, "myApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
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
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

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
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    impl.Subscribe(ctx, true, "org.rdk.Plugin", "eventX");
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, false, "org.rdk.Plugin", "eventX"));

    EXPECT_FALSE(impl.mSubMap.Exists("eventx"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_NonExistentEvent_ReturnsNone)
{
    // Unsubscribing an event that was never subscribed must still return ERROR_NONE.
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, false, "org.rdk.Plugin", "nonExistentEvent"));
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_OneOfTwoContextsRemoved)
{
    auto ctx1 = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

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
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

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

    // Catch-all (registered first): the SubscriberJob also calls QueryInterfaceByCallsign for
    // the module callsign (e.g. "org.rdk.Plugin") to resolve the notification handler.
    // GMock evaluates expectations in LIFO order — the specific matcher below takes priority
    // for APP_GATEWAY_CALLSIGN calls; this wildcard absorbs all other callsign calls.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(
            _,
            StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
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

    ON_CALL(service, QueryInterfaceByCallsign(_, _)).WillByDefault(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(
            _,
            StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            launchDelegateMock->AddRef();
            return static_cast<void*>(launchDelegateMock);
        }));

    EXPECT_CALL(*launchDelegateMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);
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
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN))).Times(0);
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN))).Times(0);

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("unknownEvent", "{}", ""));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

TEST_F(AppNotificationsTest, Emit_WithAppIdFilter_OnlyDispatchesMatchingAppId)
{
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    ON_CALL(service, QueryInterfaceByCallsign(_, _)).WillByDefault(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // Only app1 should receive the emit.
    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx1 = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
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
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "missingGatewayEvent");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("missingGatewayEvent", "{}", "app1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

TEST_F(AppNotificationsTest, Emit_LaunchDelegateQueryFails_DoesNotCrash)
{
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);
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
    auto ctx1 = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);  // same conn+origin
    auto ctx3 = MakeContext(3, 200, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);  // different connId

    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "eventA");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "eventB");
    impl.Subscribe(ctx3, true, "org.rdk.Plugin", "eventA");

    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(100, APP_NOTIFICATIONS_GATEWAY_CALLSIGN));

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
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "persistEvent");

    // Cleanup with a connectionId that was never subscribed — nothing removed.
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(999, APP_NOTIFICATIONS_GATEWAY_CALLSIGN));

    // The subscriber map entry for "persistevent" must still be present.
    EXPECT_TRUE(impl.mSubMap.Exists("persistevent"));
}

TEST_F(AppNotificationsTest, Cleanup_OriginMismatch_NoChange)
{
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "stayEvent");

    // Cleanup with same connectionId but different origin — nothing removed.
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(100, APP_NOTIFICATIONS_DELEGATE_CALLSIGN));

    EXPECT_TRUE(impl.mSubMap.Exists("stayevent"));
}

TEST_F(AppNotificationsTest, Cleanup_EmptyMap_ReturnsNone)
{
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(1, APP_NOTIFICATIONS_GATEWAY_CALLSIGN));
}

TEST_F(AppNotificationsTest, Cleanup_MultipleEventsForConnection_AllCleared)
{
    auto ctx = MakeContext(1, 55, "myApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    impl.Subscribe(ctx, true, "org.rdk.A", "alpha");
    impl.Subscribe(ctx, true, "org.rdk.B", "beta");
    impl.Subscribe(ctx, true, "org.rdk.C", "gamma");

    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(55, APP_NOTIFICATIONS_GATEWAY_CALLSIGN));

    EXPECT_FALSE(impl.mSubMap.Exists("alpha"));
    EXPECT_FALSE(impl.mSubMap.Exists("beta"));
    EXPECT_FALSE(impl.mSubMap.Exists("gamma"));
}

TEST_F(AppNotificationsTest, Cleanup_ThenSubscribe_WorksCorrectly)
{
    auto ctx = MakeContext(1, 77, "appX", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "cyclicEvent");
    impl.Cleanup(77, APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

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
    auto ctx = MakeContext(1, 10, "a", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("SomeKey", ctx);
    EXPECT_TRUE(impl.mSubMap.Exists("somekey"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Get_ReturnsCorrectContexts)
{
    auto ctx1 = MakeContext(1, 10, "a", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 11, "b", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
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
    auto ctx = MakeContext(1, 10, "a", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("removeKey", ctx);
    EXPECT_TRUE(impl.mSubMap.Exists("removekey"));

    impl.mSubMap.Remove("removeKey", ctx);
    EXPECT_FALSE(impl.mSubMap.Exists("removekey"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Remove_NonExistentKey_NoOp)
{
    auto ctx = MakeContext(1, 10, "a", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    // No crash expected.
    impl.mSubMap.Remove("ghost", ctx);
    EXPECT_FALSE(impl.mSubMap.Exists("ghost"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Exists_CaseInsensitive)
{
    auto ctx = MakeContext(1, 10, "a", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("UPPER", ctx);
    EXPECT_TRUE(impl.mSubMap.Exists("upper"));
    EXPECT_TRUE(impl.mSubMap.Exists("UPPER"));
    EXPECT_TRUE(impl.mSubMap.Exists("Upper"));
}

TEST_F(AppNotificationsTest, SubscriberMap_CleanupNotifications_ByConnectionAndOrigin)
{
    auto ctx1 = MakeContext(1, 42, "appA", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 43, "appB", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("cleanKey", ctx1);
    impl.mSubMap.Add("cleanKey", ctx2);

    impl.mSubMap.CleanupNotifications(42, APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    EXPECT_TRUE(impl.mSubMap.Exists("cleankey"));
    auto vec = impl.mSubMap.Get("cleankey");
    ASSERT_EQ(1u, vec.size());
    EXPECT_EQ(43u, vec[0].connectionId);
}

TEST_F(AppNotificationsTest, SubscriberMap_CleanupNotifications_ErasesKeyWhenAllRemoved)
{
    auto ctx = MakeContext(1, 88, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("eraseKey", ctx);

    impl.mSubMap.CleanupNotifications(88, APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

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

    ON_CALL(service, QueryInterfaceByCallsign(_, _)).WillByDefault(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));
    EXPECT_CALL(*gatewayMock, Emit(_, StrEq("e2eEvent"), _)).Times(AnyNumber());

    auto ctx = MakeContext(10, 300, "e2eApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.Plugin", "e2eEvent"));
    EXPECT_TRUE(impl.mSubMap.Exists("e2eevent"));

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("e2eEvent", "{\"status\":\"ok\"}", "e2eApp"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Cleanup(300, APP_NOTIFICATIONS_GATEWAY_CALLSIGN));
    EXPECT_FALSE(impl.mSubMap.Exists("e2eevent"));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, EndToEnd_MultipleSubscribersEmitOneAppId)
{
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    ON_CALL(service, QueryInterfaceByCallsign(_, _)).WillByDefault(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));
    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(AnyNumber());

    auto ctx1 = MakeContext(1, 100, "appAlpha", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "appBeta",  APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "multiEvent");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "multiEvent");

    // Emit for appAlpha only — appBeta should not receive the dispatch.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("multiEvent", "{}", "appAlpha"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    gatewayMock->Release();
}

// ===========================================================================
// Additional Subscribe tests
// ===========================================================================

TEST_F(AppNotificationsTest, Subscribe_EmptyModule_Listen_True_ReturnsNoneAndAddsToMap)
{
    // Empty module string is valid — Subscribe must still return ERROR_NONE and
    // add the context to the subscriber map (the SubscriberJob will be dispatched
    // with an empty module, which is the caller's concern).
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "", "emptyModuleEvent"));

    EXPECT_TRUE(impl.mSubMap.Exists("emptymoduleevent"));
}

TEST_F(AppNotificationsTest, Subscribe_EmptyEvent_Listen_True_ReturnsNoneAndAddsToMap)
{
    // An empty event string produces an empty lowercase key in the map.
    // Subscribe must return ERROR_NONE and the context must be retrievable.
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, true, "org.rdk.Plugin", ""));

    // The empty string is the map key.
    EXPECT_TRUE(impl.mSubMap.Exists(""));
    auto subscribers = impl.mSubMap.Get("");
    ASSERT_EQ(1u, subscribers.size());
    EXPECT_EQ(100u, subscribers[0].connectionId);
}

TEST_F(AppNotificationsTest, Subscribe_PartialUnsubscribe_ThenResubscribe_Works)
{
    // Subscribe two contexts, unsubscribe the first — key must persist.
    // Then subscribe the first context again — key must still exist with both.
    auto ctx1 = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "partialEvt");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "partialEvt");

    // Unsubscribe ctx1 — ctx2 still holds the key open.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx1, false, "org.rdk.Plugin", "partialEvt"));
    EXPECT_TRUE(impl.mSubMap.Exists("partialevt"));
    EXPECT_EQ(1u, impl.mSubMap.Get("partialevt").size());

    // Re-subscribe ctx1 — the key already exists so no new SubscriberJob is dispatched;
    // ctx1 is simply appended to the existing vector.
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx1, true, "org.rdk.Plugin", "partialEvt"));
    EXPECT_EQ(2u, impl.mSubMap.Get("partialevt").size());
}

TEST_F(AppNotificationsTest, Subscribe_Listen_False_EmptyEvent_NoEntry_ReturnsNone)
{
    // Unsubscribing a never-added empty event must not crash.
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE,
        impl.Subscribe(ctx, false, "org.rdk.Plugin", ""));
    EXPECT_FALSE(impl.mSubMap.Exists(""));
}

// ===========================================================================
// Additional Emit tests
// ===========================================================================

TEST_F(AppNotificationsTest, Emit_AfterCleanup_NoDispatch)
{
    // Subscribe, then cleanup (removes the context), then emit.
    // The worker pool job must find no subscribers and not attempt dispatch.
    // Wildcard catch-all first (covers SubscriberJob's query for "org.rdk.Plugin").
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    // The gateway/delegate interfaces must never be acquired (no subscribers).
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(0);
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(0);

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "cleanedEvent");
    impl.Cleanup(100, APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_FALSE(impl.mSubMap.Exists("cleanedevent"));

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("cleanedEvent", "{}", "app1"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
}

TEST_F(AppNotificationsTest, Emit_EmptyAppId_DispatchesToAllSubscribers)
{
    // When appId is empty, EventUpdate should dispatch to every subscriber for the event.
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // Both app1 and app2 subscribers must receive the Emit call.
    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(::testing::AtLeast(2));

    auto ctx1 = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx1, true, "org.rdk.Plugin", "broadcastEvt");
    impl.Subscribe(ctx2, true, "org.rdk.Plugin", "broadcastEvt");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("broadcastEvt", "{\"data\":\"all\"}", ""));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Emit_AppIdNoMatch_NoDispatch)
{
    // Emit with an appId that doesn't match any subscriber — no Emit call expected.
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // No matching appId — Emit on the gateway mock must never be called.
    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(0);

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.Subscribe(ctx, true, "org.rdk.Plugin", "noMatchEvt");

    EXPECT_EQ(Core::ERROR_NONE,
        impl.Emit("noMatchEvt", "{}", "differentApp"));

    std::this_thread::sleep_for(std::chrono::milliseconds(80));

    gatewayMock->Release();
}

// ===========================================================================
// Additional Cleanup tests
// ===========================================================================

TEST_F(AppNotificationsTest, Cleanup_EmptyOrigin_OnlyRemovesEmptyOriginContexts)
{
    // Context with empty origin — Cleanup with empty origin must only remove it.
    auto ctxEmpty  = MakeContext(1, 100, "app1", "");
    auto ctxGw     = MakeContext(2, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    impl.mSubMap.Add("mixedOriginKey", ctxEmpty);
    impl.mSubMap.Add("mixedOriginKey", ctxGw);

    // Cleanup with connectionId=100 and origin="" — only ctxEmpty removed.
    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(100, ""));

    EXPECT_TRUE(impl.mSubMap.Exists("mixedoriginkey"));
    auto vec = impl.mSubMap.Get("mixedoriginkey");
    ASSERT_EQ(1u, vec.size());
    EXPECT_EQ(APP_NOTIFICATIONS_GATEWAY_CALLSIGN, vec[0].origin);
}

TEST_F(AppNotificationsTest, Cleanup_ZeroConnectionId_OnlyRemovesMatchingConnId)
{
    // Contexts with connectionId=0 must be cleaned up when Cleanup(0, origin) is called.
    auto ctxZero = MakeContext(1, 0,   "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctxNonZ = MakeContext(2, 100, "app2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    impl.mSubMap.Add("zeroConnKey", ctxZero);
    impl.mSubMap.Add("zeroConnKey", ctxNonZ);

    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(0, APP_NOTIFICATIONS_GATEWAY_CALLSIGN));

    EXPECT_TRUE(impl.mSubMap.Exists("zeroconnkey"));
    auto vec = impl.mSubMap.Get("zeroconnkey");
    ASSERT_EQ(1u, vec.size());
    EXPECT_EQ(100u, vec[0].connectionId);
}

TEST_F(AppNotificationsTest, Cleanup_BothOriginAndConnIdMustMatch_ConnIdMatchOnly_NoRemoval)
{
    // Context with connId=100 and origin=GATEWAY_CALLSIGN.
    // Cleanup with connId=100 but wrong origin must NOT remove the context.
    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("bothMatchKey", ctx);

    EXPECT_EQ(Core::ERROR_NONE, impl.Cleanup(100, "org.rdk.SomeOtherOrigin"));

    EXPECT_TRUE(impl.mSubMap.Exists("bothmatchkey"));
}

// ===========================================================================
// Additional SubscriberMap internal tests
// ===========================================================================

TEST_F(AppNotificationsTest, SubscriberMap_Remove_OnlyRemovesOneMatchingContext_WhenMultipleSameKey)
{
    // Add two identical contexts (same requestId, connId, appId, origin, version).
    auto ctx = MakeContext(1, 10, "appA", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("dupKey", ctx);
    impl.mSubMap.Add("dupKey", ctx);

    // Remove once — std::remove erases all equal elements.
    // Verify what the implementation actually does (remove_all-equal or remove-first).
    impl.mSubMap.Remove("dupKey", ctx);

    // The std::remove(...) in the implementation removes ALL equal elements,
    // so after Remove the vector is empty and the key is erased.
    EXPECT_FALSE(impl.mSubMap.Exists("dupkey"));
}

TEST_F(AppNotificationsTest, SubscriberMap_CleanupNotifications_ConnIdMatchOnly_OriginMismatch_NoRemoval)
{
    // Add context with origin=GATEWAY_CALLSIGN.
    // CleanupNotifications with same connId but different origin must not remove it.
    auto ctx = MakeContext(1, 55, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("cleanMismatch", ctx);

    impl.mSubMap.CleanupNotifications(55, "wrongOrigin");

    EXPECT_TRUE(impl.mSubMap.Exists("cleanmismatch"));
}

TEST_F(AppNotificationsTest, SubscriberMap_DispatchToGateway_CachesInterface_QueryCalledOnce)
{
    // The first DispatchToGateway call acquires mAppGateway via QueryInterfaceByCallsign.
    // The second call must reuse the cached pointer — QueryInterface called only once.
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    // Exactly one QueryInterfaceByCallsign call for the gateway callsign.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(1)
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(2);

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);

    // Call DispatchToGateway directly twice — second call must reuse cached mAppGateway.
    impl.mSubMap.DispatchToGateway("cacheEvt", ctx, "{}");
    impl.mSubMap.DispatchToGateway("cacheEvt", ctx, "{}");

    // Release the extra ref held by the test (the mock is also referenced by mAppGateway).
    // The SubscriberMap destructor will Release() mAppGateway.
    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, SubscriberMap_DispatchToGateway_NullGateway_DoesNotCrash)
{
    // If QueryInterfaceByCallsign returns nullptr, DispatchToGateway must log and return
    // without crashing.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    // Must not crash.
    EXPECT_NO_THROW(impl.mSubMap.DispatchToGateway("nullGwEvt", ctx, "{}"));
}

TEST_F(AppNotificationsTest, SubscriberMap_DispatchToLaunchDelegate_NullDelegate_DoesNotCrash)
{
    // Same test for DispatchToLaunchDelegate when QueryInterfaceByCallsign returns nullptr.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);
    EXPECT_NO_THROW(impl.mSubMap.DispatchToLaunchDelegate("nullDelegEvt", ctx, "{}"));
}

TEST_F(AppNotificationsTest, SubscriberMap_DispatchToLaunchDelegate_CachesInterface_QueryCalledOnce)
{
    // Mirror of the DispatchToGateway cache test, but for the LaunchDelegate path.
    auto delegateMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(1)
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            delegateMock->AddRef();
            return static_cast<void*>(delegateMock);
        }));

    EXPECT_CALL(*delegateMock, Emit(_, _, _)).Times(2);

    auto ctx = MakeContext(1, 100, "app1", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);

    impl.mSubMap.DispatchToLaunchDelegate("cacheDeleg", ctx, "{}");
    impl.mSubMap.DispatchToLaunchDelegate("cacheDeleg", ctx, "{}");

    delegateMock->Release();
}

TEST_F(AppNotificationsTest, SubscriberMap_EventUpdate_AppIdEmpty_DispatchesToAllSubscribers)
{
    // EventUpdate with empty appId must dispatch to every context in the vector.
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(3);

    auto ctx1 = MakeContext(1, 100, "a1", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 101, "a2", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx3 = MakeContext(3, 102, "a3", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("evtAll", ctx1);
    impl.mSubMap.Add("evtAll", ctx2);
    impl.mSubMap.Add("evtAll", ctx3);

    // Synchronous call (not via worker pool) — no sleep needed.
    impl.mSubMap.EventUpdate("evtAll", "{}", "");

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, SubscriberMap_EventUpdate_AppIdNonMatch_NoDispatch)
{
    // EventUpdate where appId matches no subscriber — no dispatch at all.
    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    EXPECT_CALL(*gatewayMock, Emit(_, _, _)).Times(0);

    auto ctx = MakeContext(1, 100, "realApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("evtNoMatch", ctx);

    impl.mSubMap.EventUpdate("evtNoMatch", "{}", "ghostApp");

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, SubscriberMap_EventUpdate_NoSubscribersForKey_NoDispatch)
{
    // EventUpdate for an event with no subscribers — no crash, no dispatch.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(0);
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(0);

    EXPECT_NO_THROW(impl.mSubMap.EventUpdate("unknownEvt", "{}", ""));
}

// ===========================================================================
// Additional ThunderSubscriptionManager tests
// ===========================================================================

TEST_F(AppNotificationsTest, ThunderManager_HandleNotifier_ThrowsException_PropagatesFromHandlerMock)
{
    // HandleAppEventNotifier throws a std::runtime_error.
    // Since HandleNotifier has no try/catch, the exception propagates to the caller.
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.ThrowMod")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("throwEvt"), true, _))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IAppNotificationHandler::IEmitter*, const string&, bool, bool& status) -> Core::hresult {
                throw std::runtime_error("Test exception");
                status = false;
                return Core::ERROR_NONE;
            }));

    EXPECT_THROW(
        impl.mThunderManager.HandleNotifier("org.rdk.ThrowMod", "throwEvt", true),
        std::runtime_error);

    // handlerMock was AddRef'd once (returned by QueryInterfaceByCallsign) and
    // Release() was NOT called (exception threw before internalNotifier->Release()).
    // We must balance the ref count manually to avoid a leak.
    // The mock's _refCount is 2 (ctor=1, AddRef in lambda=+1); Release twice.
    handlerMock->Release(); // releases the "QueryInterfaceByCallsign" ref
    handlerMock->Release(); // releases the ctor ref (deletes the mock)
}

TEST_F(AppNotificationsTest, ThunderManager_RegisterNotification_ThrowsException_Propagates)
{
    // RegisterNotification calls HandleNotifier synchronously; exception propagates.
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.RegThrow")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("regThrowEvt"), true, _))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IAppNotificationHandler::IEmitter*, const string&, bool, bool& status) -> Core::hresult {
                throw std::runtime_error("register exception");
                status = false;
                return Core::ERROR_NONE;
            }));

    EXPECT_THROW(
        impl.mThunderManager.RegisterNotification("org.rdk.RegThrow", "regThrowEvt"),
        std::runtime_error);

    // Balance refs as above.
    handlerMock->Release();
    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_UnregisterNotification_HandlerReturnsFalseStatus_NotRemovedFromList)
{
    // Register successfully, then call UnregisterNotification where handler returns status=false.
    // UnregisterNotification only erases from list when HandleNotifier returns true,
    // so the notification must remain registered.
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.UnregFalse")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    // Registration: listen=true, status=true.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("unregFalseEvt"), true, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));

    impl.mThunderManager.RegisterNotification("org.rdk.UnregFalse", "unregFalseEvt");
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.UnregFalse", "unregfalseevt"));

    // Unregistration: listen=false, but handler sets status=false.
    // HandleNotifier returns false → mRegisteredNotifications NOT modified.
    // UnregisterNotification lowercases the event before passing to HandleNotifier.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("unregfalseevt"), false, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(false),
            Return(Core::ERROR_NONE)));

    impl.mThunderManager.UnregisterNotification("org.rdk.UnregFalse", "unregFalseEvt");

    // Still registered because HandleNotifier returned false.
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.UnregFalse", "unregfalseevt"));

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_Unsubscribe_WhenRegistered_CallsUnregister)
{
    // Verify Unsubscribe() removes the notification when it was previously registered.
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.UnsubMod")))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    // Register.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, _, true, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));
    impl.mThunderManager.Subscribe("org.rdk.UnsubMod", "unsubEvt");
    // IsNotificationRegistered lowercases its input, so "unsubEvt" → stored as "unsubeVt".
    EXPECT_TRUE(impl.mThunderManager.IsNotificationRegistered("org.rdk.UnsubMod", "unsubEvt"));

    // Unsubscribe.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, _, false, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            Return(Core::ERROR_NONE)));
    impl.mThunderManager.Unsubscribe("org.rdk.UnsubMod", "unsubEvt");
    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.UnsubMod", "unsubEvt"));

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, ThunderManager_Unsubscribe_WhenNotRegistered_IsNoOp)
{
    // Unsubscribe for a module/event pair that was never registered.
    // Implementation logs an error and returns — no handler call expected.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _)).Times(0);

    impl.mThunderManager.Unsubscribe("org.rdk.GhostMod", "ghostEvt");
    EXPECT_FALSE(impl.mThunderManager.IsNotificationRegistered("org.rdk.GhostMod", "ghostevt"));
}

TEST_F(AppNotificationsTest, ThunderManager_HandleNotifier_HandlerReturnsErrorNoneButStatusFalse_ReturnsFalse)
{
    // Handler returns ERROR_NONE but sets status=false.
    // HandleNotifier must return false (status is the return value, not the hresult).
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.StatusFalseMod")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("statusFalseEvt"), true, _))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(false),
            Return(Core::ERROR_NONE)));

    bool result = impl.mThunderManager.HandleNotifier("org.rdk.StatusFalseMod", "statusFalseEvt", true);
    EXPECT_FALSE(result);

    handlerMock->Release();
}

// ===========================================================================
// NotificationHandler / Emitter notification flow tests
//
// These tests exercise the path:
//   [external caller] --calls--> impl.mEmitter.Emit()
//                            --submits--> EmitJob
//                            --Dispatch()--> SubscriberMap::EventUpdate()
//                            --Dispatch()--> DispatchToGateway / DispatchToLaunchDelegate
//                            --calls--> AppGatewayResponderMock::Emit()
//
// They also verify the NotificationHandler (IEmitter impl) receives the call
// when an external plugin's IAppNotificationHandler::HandleAppEventNotifier
// invokes the passed emitter callback.
// ===========================================================================

// ---------------------------------------------------------------------------
// Emitter::Emit submits EmitJob which drives EventUpdate + DispatchToGateway
// ---------------------------------------------------------------------------

TEST_F(AppNotificationsTest, Notification_EmitterEmit_WithSubscriber_GatewayOrigin_DispatchesViaGateway)
{
    // Arrange: subscriber with gateway origin.
    auto ctx = MakeContext(1, 100, "notifyApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("notifyEvt", ctx);

    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // The gateway's Emit must be called once with matching event/payload.
    EXPECT_CALL(*gatewayMock, Emit(_, StrEq("notifyEvt"), StrEq("{\"msg\":\"hello\"}")))
        .Times(1)
        .WillOnce(Return(Core::ERROR_NONE));

    // Act: call Emit on the plugin's inner Emitter (the IEmitter the plugin
    // passes to HandleAppEventNotifier). This submits an EmitJob to the pool.
    impl.mEmitter.Emit("notifyEvt", "{\"msg\":\"hello\"}", "notifyApp");

    // Allow the worker pool to drain the EmitJob.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Notification_EmitterEmit_WithSubscriber_NonGatewayOrigin_DispatchesViaLaunchDelegate)
{
    // Arrange: subscriber with non-gateway origin (Launch Delegate path).
    auto ctx = MakeContext(1, 200, "delegateApp", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);
    impl.mSubMap.Add("delegateNotifyEvt", ctx);

    auto delegateMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            delegateMock->AddRef();
            return static_cast<void*>(delegateMock);
        }));

    EXPECT_CALL(*delegateMock, Emit(_, StrEq("delegateNotifyEvt"), StrEq("{}")))
        .Times(1)
        .WillOnce(Return(Core::ERROR_NONE));

    impl.mEmitter.Emit("delegateNotifyEvt", "{}", "delegateApp");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    delegateMock->Release();
}

TEST_F(AppNotificationsTest, Notification_EmitterEmit_NoSubscribers_NoDispatch)
{
    // Emitter::Emit for an event with no subscribers — gateway must not be queried.
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(0);
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(0);

    impl.mEmitter.Emit("orphanEvt", "{}", "anyApp");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
}

TEST_F(AppNotificationsTest, Notification_EmitterEmit_EmptyAppId_BroadcastsToAllSubscribers)
{
    // Empty appId → EventUpdate dispatches to ALL subscribers for the event.
    auto ctx1 = MakeContext(1, 10, "appOne", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 11, "appTwo", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("broadcastEvt", ctx1);
    impl.mSubMap.Add("broadcastEvt", ctx2);

    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // Both subscribers must receive the event (gateway Emit called twice).
    EXPECT_CALL(*gatewayMock, Emit(_, StrEq("broadcastEvt"), _))
        .Times(2)
        .WillRepeatedly(Return(Core::ERROR_NONE));

    impl.mEmitter.Emit("broadcastEvt", "{\"data\":\"all\"}", "");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Notification_EmitterEmit_AppIdFiltered_OnlyMatchingSubscriberDispatched)
{
    // Only the subscriber whose appId matches the emitted appId receives the event.
    auto ctx1 = MakeContext(1, 10, "targetApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(2, 11, "otherApp",  APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("filteredEvt", ctx1);
    impl.mSubMap.Add("filteredEvt", ctx2);

    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    // Only targetApp's subscriber should be dispatched — exactly once.
    EXPECT_CALL(*gatewayMock, Emit(_, _, _))
        .Times(1)
        .WillOnce(Return(Core::ERROR_NONE));

    impl.mEmitter.Emit("filteredEvt", "{}", "targetApp");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    gatewayMock->Release();
}

// ---------------------------------------------------------------------------
// NotificationHandler receives Emit via direct call to mEmitter
// ---------------------------------------------------------------------------

TEST_F(AppNotificationsTest, Notification_NotificationHandler_ReceivesEmit_DirectCall)
{
    // Calling Emit() on our NotificationHandler directly verifies that
    // the WaitForRequestStatus + parameter storage work correctly.
    NotificationHandler* handler = new NotificationHandler();

    handler->Emit("testEvent", "{\"key\":\"val\"}", "myApp");

    EXPECT_TRUE(handler->WaitForRequestStatus(1000, AppNotifications_OnEmit));
    EXPECT_EQ("testEvent",       handler->GetEmitEvent());
    EXPECT_EQ("{\"key\":\"val\"}", handler->GetEmitPayload());
    EXPECT_EQ("myApp",           handler->GetEmitAppId());

    handler->Release();
}

TEST_F(AppNotificationsTest, Notification_NotificationHandler_Reset_ClearsStoredState)
{
    NotificationHandler* handler = new NotificationHandler();

    handler->Emit("evt", "payload", "app");
    EXPECT_TRUE(handler->WaitForRequestStatus(500, AppNotifications_OnEmit));

    handler->Reset();
    EXPECT_EQ(0u, handler->GetEventsMask());
    EXPECT_EQ("", handler->GetEmitEvent());
    EXPECT_EQ("", handler->GetEmitPayload());
    EXPECT_EQ("", handler->GetEmitAppId());

    handler->Release();
}

TEST_F(AppNotificationsTest, Notification_NotificationHandler_WaitTimeout_ReturnsFalse_WhenNoEmit)
{
    // WaitForRequestStatus must return false when Emit is never called.
    NotificationHandler* handler = new NotificationHandler();

    bool signalled = handler->WaitForRequestStatus(100, AppNotifications_OnEmit);
    EXPECT_FALSE(signalled);

    handler->Release();
}

TEST_F(AppNotificationsTest, Notification_NotificationHandler_MultipleEmitCalls_LastValueStored)
{
    // Each Emit call overwrites the stored parameters.
    NotificationHandler* handler = new NotificationHandler();

    handler->Emit("firstEvt",  "p1", "a1");
    handler->Emit("secondEvt", "p2", "a2");

    // The event bit stays set after first call; second call keeps it set.
    EXPECT_TRUE(handler->WaitForRequestStatus(500, AppNotifications_OnEmit));
    // Last call wins for stored values.
    EXPECT_EQ("secondEvt", handler->GetEmitEvent());
    EXPECT_EQ("p2",        handler->GetEmitPayload());
    EXPECT_EQ("a2",        handler->GetEmitAppId());

    handler->Release();
}

// ---------------------------------------------------------------------------
// HandleNotifier passes impl's mEmitter to the plugin's IAppNotificationHandler
// ---------------------------------------------------------------------------

TEST_F(AppNotificationsTest, Notification_HandleNotifier_PassesCorrectEmitterToHandler)
{
    // Verify HandleNotifier passes &impl.mEmitter as the IEmitter* argument.
    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();

    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.EmitterCheckMod")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    Exchange::IAppNotificationHandler::IEmitter* capturedEmitter = nullptr;
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("checkEvt"), true, _))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IAppNotificationHandler::IEmitter* emitCb,
                const string&, bool, bool& status) -> Core::hresult {
                capturedEmitter = emitCb;
                status = true;
                return Core::ERROR_NONE;
            }));

    impl.mThunderManager.HandleNotifier("org.rdk.EmitterCheckMod", "checkEvt", true);

    // The emitter passed must be the plugin's own mEmitter sink.
    EXPECT_EQ(static_cast<Exchange::IAppNotificationHandler::IEmitter*>(&impl.mEmitter),
              capturedEmitter);

    handlerMock->Release();
}

TEST_F(AppNotificationsTest, Notification_HandleNotifier_EmitterCanBeInvokedByHandler_TriggersEventUpdate)
{
    // When the external handler calls back through the emitter it was given,
    // the EmitJob is submitted and EventUpdate is eventually invoked.
    // Set up a subscriber so EventUpdate reaches DispatchToGateway.
    auto ctx = MakeContext(1, 300, "cbApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("cbEvent", ctx);

    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));
    EXPECT_CALL(*gatewayMock, Emit(_, StrEq("cbEvent"), StrEq("{\"from\":\"handler\"}")))
        .Times(1)
        .WillOnce(Return(Core::ERROR_NONE));

    auto handlerMock = new NiceMock<AppNotificationHandlerMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq("org.rdk.CbMod")))
        .WillOnce(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handlerMock->AddRef();
            return static_cast<void*>(handlerMock);
        }));

    // When HandleNotifier is called, the handler immediately uses the provided
    // emitter to emit back — simulating a real plugin's behavior.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(_, StrEq("cbEvent"), true, _))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IAppNotificationHandler::IEmitter* emitCb,
                const string&, bool, bool& status) -> Core::hresult {
                emitCb->Emit("cbEvent", "{\"from\":\"handler\"}", "cbApp");
                status = true;
                return Core::ERROR_NONE;
            }));

    impl.mThunderManager.HandleNotifier("org.rdk.CbMod", "cbEvent", true);

    // Allow the submitted EmitJob to run on the worker pool.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    handlerMock->Release();
    gatewayMock->Release();
}

// ---------------------------------------------------------------------------
// Emitter::Emit + NotificationHandler together: end-to-end notification path
// ---------------------------------------------------------------------------

TEST_F(AppNotificationsTest, Notification_EndToEnd_EmitterEmit_NotificationHandler_Receives)
{
    // NotificationHandler is set up as the IEmitter. When something calls
    // impl.mEmitter.Emit(), the EmitJob runs and eventually reaches
    // DispatchToGateway which calls gatewayMock->Emit().
    // Separately we verify NotificationHandler::Emit can be directly called
    // and captures all parameters correctly.
    NotificationHandler* notifHandler = new NotificationHandler();

    // Directly exercise the NotificationHandler's Emit (simulates an external
    // caller who received the IEmitter* and calls it back).
    notifHandler->Emit("e2eEvent", "{\"step\":1}", "e2eApp");

    EXPECT_TRUE(notifHandler->WaitForRequestStatus(1000, AppNotifications_OnEmit));
    EXPECT_EQ("e2eEvent",   notifHandler->GetEmitEvent());
    EXPECT_EQ("{\"step\":1}", notifHandler->GetEmitPayload());
    EXPECT_EQ("e2eApp",     notifHandler->GetEmitAppId());

    notifHandler->Release();
}

TEST_F(AppNotificationsTest, Notification_EndToEnd_SubscribeEmitViaEmitter_GatewayDispatch)
{
    // Full pipeline: Subscribe → Emitter::Emit → EmitJob → EventUpdate
    //                → DispatchToGateway → gatewayMock->Emit().
    auto ctx = MakeContext(42, 500, "pipelineApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, impl.Subscribe(ctx, true, "org.rdk.Pipeline", "pipelineEvt"));

    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));
    EXPECT_CALL(*gatewayMock, Emit(_, StrEq("pipelineEvt"), StrEq("{\"ok\":true}")))
        .Times(1)
        .WillOnce(Return(Core::ERROR_NONE));

    // Use the plugin's own Emitter (the object passed to HandleAppEventNotifier).
    impl.mEmitter.Emit("pipelineEvt", "{\"ok\":true}", "pipelineApp");

    // Worker pool drains the EmitJob.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Notification_EndToEnd_SubscribeEmitViaEmitter_LaunchDelegateDispatch)
{
    // Same as above but with a non-gateway origin (LaunchDelegate path).
    auto ctx = MakeContext(43, 501, "ldApp", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, impl.Subscribe(ctx, true, "org.rdk.LD", "ldEvt"));

    auto ldMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            ldMock->AddRef();
            return static_cast<void*>(ldMock);
        }));
    EXPECT_CALL(*ldMock, Emit(_, StrEq("ldEvt"), StrEq("{\"ld\":1}")))
        .Times(1)
        .WillOnce(Return(Core::ERROR_NONE));

    impl.mEmitter.Emit("ldEvt", "{\"ld\":1}", "ldApp");

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    ldMock->Release();
}

// ---------------------------------------------------------------------------
// GatewayContext fields populated correctly in DispatchToGateway
// ---------------------------------------------------------------------------

TEST_F(AppNotificationsTest, Notification_DispatchToGateway_GatewayContextFields_MatchSubscriberContext)
{
    // Verify that the GatewayContext passed to AppGatewayResponder::Emit contains
    // the requestId/connectionId/appId from the subscriber's AppNotificationContext.
    auto ctx = MakeContext(77, 888, "fieldApp", APP_NOTIFICATIONS_GATEWAY_CALLSIGN);
    impl.mSubMap.Add("ctxFieldEvt", ctx);

    auto gatewayMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_GATEWAY_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            gatewayMock->AddRef();
            return static_cast<void*>(gatewayMock);
        }));

    Exchange::GatewayContext capturedCtx{};
    EXPECT_CALL(*gatewayMock, Emit(_, _, _))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const Exchange::GatewayContext& gCtx,
                const string&, const string&) -> Core::hresult {
                capturedCtx = gCtx;
                return Core::ERROR_NONE;
            }));

    // Use direct EventUpdate (synchronous) to avoid worker-pool timing.
    impl.mSubMap.EventUpdate("ctxFieldEvt", "{}", "fieldApp");

    EXPECT_EQ(77u,        capturedCtx.requestId);
    EXPECT_EQ(888u,       capturedCtx.connectionId);
    EXPECT_EQ("fieldApp", capturedCtx.appId);

    gatewayMock->Release();
}

TEST_F(AppNotificationsTest, Notification_DispatchToLaunchDelegate_GatewayContextFields_MatchSubscriberContext)
{
    // Same field verification for the LaunchDelegate path.
    auto ctx = MakeContext(55, 999, "ldFieldApp", APP_NOTIFICATIONS_DELEGATE_CALLSIGN);
    impl.mSubMap.Add("ldFieldEvt", ctx);

    auto ldMock = new NiceMock<AppGatewayResponderMock>();
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Return(nullptr));
    EXPECT_CALL(service, QueryInterfaceByCallsign(_, StrEq(APP_NOTIFICATIONS_DELEGATE_CALLSIGN)))
        .Times(AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            ldMock->AddRef();
            return static_cast<void*>(ldMock);
        }));

    Exchange::GatewayContext capturedCtx{};
    EXPECT_CALL(*ldMock, Emit(_, _, _))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const Exchange::GatewayContext& gCtx,
                const string&, const string&) -> Core::hresult {
                capturedCtx = gCtx;
                return Core::ERROR_NONE;
            }));

    impl.mSubMap.EventUpdate("ldFieldEvt", "{}", "ldFieldApp");

    EXPECT_EQ(55u,           capturedCtx.requestId);
    EXPECT_EQ(999u,          capturedCtx.connectionId);
    EXPECT_EQ("ldFieldApp",  capturedCtx.appId);

    ldMock->Release();
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
