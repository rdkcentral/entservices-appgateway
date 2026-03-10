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

// Local mocks (this repo): Tests/mocks/*
#include "ServiceMock.h"
#include "ThunderPortability.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {

// ---------------------------------------------------------------------------
// COM-RPC mock for Exchange::IAppGatewayResponder
//
// AppNotificationsImplementation acquires this via:
//   mShell->QueryInterfaceByCallsign<Exchange::IAppGatewayResponder>(callsign)
// and then calls Emit() on it. Real refcounting ensures no use-after-free when
// production code calls Release().
// ---------------------------------------------------------------------------
class AppGatewayResponderMock : public Exchange::IAppGatewayResponder {
public:
    AppGatewayResponderMock()
        : _refCount(1)
    {
    }

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
        }
        return result;
    }

    MOCK_METHOD(Core::hresult, Respond,
        (const Exchange::GatewayContext& context,
         const string& payload),
        (override));

    MOCK_METHOD(Core::hresult, Emit,
        (const Exchange::GatewayContext& context,
         const string& method,
         const string& payload),
        (override));

    MOCK_METHOD(Core::hresult, Request,
        (const uint32_t connectionId,
         const uint32_t id,
         const string& method,
         const string& params),
        (override));

    MOCK_METHOD(Core::hresult, GetGatewayConnectionContext,
        (const uint32_t connectionId,
         const string& contextKey,
         string& contextValue),
        (override));

    MOCK_METHOD(Core::hresult, RecordGatewayConnectionContext,
        (const uint32_t connectionId,
         const string& contextKey,
         const string& contextValue),
        (override));

    MOCK_METHOD(Core::hresult, Register,
        (Exchange::IAppGatewayResponder::INotification* notification),
        (override));

    MOCK_METHOD(Core::hresult, Unregister,
        (Exchange::IAppGatewayResponder::INotification* notification),
        (override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

// ---------------------------------------------------------------------------
// COM-RPC mock for Exchange::IAppNotificationHandler
//
// AppNotificationsImplementation acquires this via:
//   mShell->QueryInterfaceByCallsign<Exchange::IAppNotificationHandler>(module)
// and then calls HandleAppEventNotifier() on it. Real refcounting is used.
// ---------------------------------------------------------------------------
class AppNotificationHandlerMock : public Exchange::IAppNotificationHandler {
public:
    AppNotificationHandlerMock()
        : _refCount(1)
    {
    }

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
        }
        return result;
    }

    MOCK_METHOD(Core::hresult, HandleAppEventNotifier,
        (Exchange::IAppNotificationHandler::IEmitter* emitCb,
         const string& event,
         bool listen,
         bool& status),
        (override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

// ---------------------------------------------------------------------------
// Factory helpers
// ---------------------------------------------------------------------------
static Exchange::IAppNotifications::AppNotificationContext MakeContext(
    uint32_t requestId     = 1,
    uint32_t connectionId  = 100,
    const string& appId    = "test.app",
    const string& origin   = "org.rdk.AppGateway",
    const string& version  = "0")
{
    Exchange::IAppNotifications::AppNotificationContext ctx;
    ctx.requestId    = requestId;
    ctx.connectionId = connectionId;
    ctx.appId        = appId;
    ctx.origin       = origin;
    ctx.version      = version;
    return ctx;
}

// ---------------------------------------------------------------------------
// Shared async-drain constant and helper
// ---------------------------------------------------------------------------
constexpr int kWorkerDrainMs = 100;

void DrainWorkerPool()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(kWorkerDrainMs));
}

// ---------------------------------------------------------------------------
// Async-event flags for IAppNotificationHandler::IEmitter callbacks
// ---------------------------------------------------------------------------
typedef enum : uint32_t {
    AppNotifications_StateInvalid = 0x00000000,
    AppNotifications_onEmit       = 0x00000001,
} AppNotificationsL1test_async_events_t;

// ---------------------------------------------------------------------------
// EmitterNotificationHandler — implements IAppNotificationHandler::IEmitter
//
// Used by tests that need to simulate the full round-trip:
//   Subscribe() → HandleAppEventNotifier() captures emitCb
//   → emitCb->Emit(...) triggers EmitJob
//   → assert mAppGateway->Emit() is called.
//
// Pattern follows test_AppManager.cpp::NotificationHandler.
// ---------------------------------------------------------------------------
class EmitterNotificationHandler : public Exchange::IAppNotificationHandler::IEmitter {
private:
    BEGIN_INTERFACE_MAP(EmitterNotificationHandler)
    INTERFACE_ENTRY(Exchange::IAppNotificationHandler::IEmitter)
    END_INTERFACE_MAP

public:
    /** @brief Mutex protecting all members below */
    std::mutex m_mutex;

    /** @brief Condition variable for WaitForRequestStatus */
    std::condition_variable m_condition_variable;

    /** @brief Bitfield of received events; OR-assigned in callbacks */
    uint32_t m_event_signalled = AppNotifications_StateInvalid;

    // Parameter storage — populated when Emit() is invoked
    string m_event;
    string m_payload;
    string m_appId;

    EmitterNotificationHandler() {}
    ~EmitterNotificationHandler() override {}

    // IAppNotificationHandler::IEmitter
    void Emit(const string& event, const string& payload, const string& appId) override
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event   = event;
        m_payload = payload;
        m_appId   = appId;
        m_event_signalled |= AppNotifications_onEmit;
        m_condition_variable.notify_one();
    }

    /**
     * Block until `expected_status` bits appear in m_event_signalled or
     * `timeout_ms` elapses.  Returns the snapshot of m_event_signalled and
     * resets it to StateInvalid ready for the next wait.
     */
    uint32_t WaitForRequestStatus(uint32_t timeout_ms,
                                  AppNotificationsL1test_async_events_t expected_status)
    {
        uint32_t signalled = AppNotifications_StateInvalid;
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now     = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(timeout_ms);
        while (!(expected_status & m_event_signalled))
        {
            if (m_condition_variable.wait_until(lock, now + timeout) ==
                std::cv_status::timeout)
            {
                std::cerr << "[AppNotificationsL1Test] Timeout waiting for AppNotifications emitter event\n";
                break;
            }
        }
        signalled         = m_event_signalled;
        m_event_signalled = AppNotifications_StateInvalid;
        return signalled;
    }

    /** Reset the event-signalled flag without waiting */
    void ResetEventSignal()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled = AppNotifications_StateInvalid;
    }

    string GetEvent()   const { return m_event;   }
    string GetPayload() const { return m_payload; }
    string GetAppId()   const { return m_appId;   }
};

} // namespace

// =============================================================================
// Base test fixture
// =============================================================================
class AppNotificationsTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Start a per-test worker pool.  Using a fresh pool each test guarantees
        // that SubscriberJob / EmitJob instances from the previous test (which hold
        // a raw reference to the previous _impl) cannot fire after _impl is gone.
        _pool = std::make_unique<WorkerPoolImplementation>(
                    /*threads*/ 2, /*stackSize*/ 0, /*queueSize*/ 64);
        Core::IWorkerPool::Assign(_pool.get());
        _pool->Run();

        // AppNotificationsImplementation stores the shell with AddRef().
        EXPECT_CALL(_service, AddRef()).Times(1);
        // Release() is called in the destructor — allow any number of calls.
        EXPECT_CALL(_service, Release())
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
        // By default return nullptr for all QueryInterfaceByCallsign calls
        // so that tests that don't care about dispatch don't fail on it.
        EXPECT_CALL(_service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .Times(::testing::AnyNumber())
            .WillRepeatedly(::testing::Return(nullptr));

        _impl = std::make_unique<Core::Sink<AppNotificationsImplementation>>();
        EXPECT_EQ(Core::ERROR_NONE, _impl->Configure(&_service));
    }

    void TearDown() override
    {
        // Stop the worker pool first — Stop() joins the worker thread, so all
        // pending SubscriberJob / EmitJob dispatches that hold a raw reference
        // to _impl have fully completed before _impl is destroyed.
        if (_pool) {
            _pool->Stop();
            Core::IWorkerPool::Assign(nullptr);
            _pool.reset();
        }

        // Clear mRegisteredNotifications before _impl is destroyed.
        // ~ThunderSubscriptionManager calls HandleNotifier(&mEmitter, ...) for each
        // registered entry, but mEmitter is declared after mThunderManager and is
        // destroyed before it — so any non-empty list would pass a dangling pointer.
        // We clear it here while _impl is still fully valid.
        if (_impl) {
            std::lock_guard<std::mutex> lk(_impl->mThunderManager.mThunderSubscriberMutex);
            _impl->mThunderManager.mRegisteredNotifications.clear();
        }

        // Now safe to destroy the implementation.  ~ThunderSubscriptionManager
        // will find mRegisteredNotifications empty and skip all HandleNotifier calls.
        // _service is still alive (declared first in the fixture, destroyed last).
        _impl.reset();
    }

    ::testing::NiceMock<ServiceMock> _service;
    std::unique_ptr<Core::Sink<AppNotificationsImplementation>> _impl;

private:
    std::unique_ptr<WorkerPoolImplementation> _pool;
};

// =============================================================================
// Sub-fixture: Subscribe() map-state / return-code tests
// =============================================================================
class AppNotificationsSubscribeTest : public AppNotificationsTest {
};

// =============================================================================
// Sub-fixture: Cleanup() tests
// =============================================================================
class AppNotificationsCleanupTest : public AppNotificationsTest {
};

// =============================================================================
// Sub-fixture: ThunderSubscriptionManager integration tests
// (subscribe/unsubscribe async path via HandleAppEventNotifier)
// =============================================================================
class AppNotificationsThunderTest : public AppNotificationsTest {
protected:
    // Create and wire a handler mock for `module`. The mock's
    // HandleAppEventNotifier expectation matches any emitter and, if `event`
    // is non-empty, the exact event string; it sets `status = returnStatus`
    // and returns `returnCode`. Pass `repeatably = true` for tests that call
    // Subscribe more than once (e.g. subscribe then unsubscribe).
    //
    // Ownership: the mock is heap-allocated with refcount=1.  An extra AddRef()
    // is called here to simulate the one performed by QueryInterfaceByCallsign,
    // so production code's Release() brings it back to 1 and does NOT free it
    // until the test ends and the NiceMock wrapper is destroyed.
    AppNotificationHandlerMock* SetupHandlerMock(
        const string& module,
        const string& event,
        bool returnStatus,
        Core::hresult returnCode,
        bool repeatably = false)
    {
        auto* mock = new ::testing::NiceMock<AppNotificationHandlerMock>();
        mock->AddRef(); // simulate QueryInterfaceByCallsign AddRef

        auto& callsignCall = EXPECT_CALL(
            _service,
            QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                     ::testing::StrEq(module)));

        if (repeatably) {
            callsignCall.WillRepeatedly(::testing::Return(static_cast<void*>(mock)));
        } else {
            callsignCall.WillOnce(::testing::Return(static_cast<void*>(mock)));
        }

        auto eventMatcher = event.empty()
            ? ::testing::Matcher<const string&>(::testing::_)
            : ::testing::Matcher<const string&>(::testing::StrEq(event));

        auto& handlerCall = EXPECT_CALL(
            *mock,
            HandleAppEventNotifier(::testing::_, eventMatcher, ::testing::_, ::testing::_));

        if (repeatably) {
            handlerCall.WillRepeatedly(::testing::DoAll(
                ::testing::SetArgReferee<3>(returnStatus),
                ::testing::Return(returnCode)));
        } else {
            handlerCall.WillOnce(::testing::DoAll(
                ::testing::SetArgReferee<3>(returnStatus),
                ::testing::Return(returnCode)));
        }

        return mock;
    }
};

// =============================================================================
// Sub-fixture: Emit() dispatch tests
// (async EmitJob path via IAppGatewayResponder)
// =============================================================================
class AppNotificationsDispatchTest : public AppNotificationsTest {
protected:
    // Create and wire a responder mock for `callsign`. The mock is returned
    // from QueryInterfaceByCallsign for IAppGatewayResponder::ID + callsign,
    // repeatedly, so any number of Emit() dispatch calls can use it.
    AppGatewayResponderMock* SetupResponderMock(const string& callsign)
    {
        auto* mock = new ::testing::NiceMock<AppGatewayResponderMock>();
        mock->AddRef(); // simulate QueryInterfaceByCallsign AddRef

        EXPECT_CALL(_service,
                    QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                             ::testing::StrEq(callsign)))
            .WillRepeatedly(::testing::Return(static_cast<void*>(mock)));

        return mock;
    }
};

// =============================================================================
// Configure() tests  (base fixture)
// =============================================================================

TEST_F(AppNotificationsTest, Configure_StoresShellReference)
{
    // SetUp() already called Configure(); just verify the shell pointer was retained.
    EXPECT_NE(nullptr, _impl->mShell);
}

// =============================================================================
// Subscribe() — listen = true (first subscriber for an event)
// =============================================================================

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_FirstSubscriber_ReturnsErrorNone)
{
    auto ctx = MakeContext();

    const Core::hresult rc = _impl->Subscribe(ctx, /*listen=*/true, "org.rdk.SomePlugin", "someEvent");

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_AddsContextToSubscriberMap)
{
    auto ctx = MakeContext(/*requestId=*/1, /*connectionId=*/100, "app1", "org.rdk.AppGateway");

    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "someEvent");

    // The event should now exist in the subscriber map (key is lowercased).
    EXPECT_TRUE(_impl->mSubMap.Exists("someevent"));
}

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_SecondSubscriberSameEvent_BothPresent)
{
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");

    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "someEvent");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "someEvent");

    const auto entries = _impl->mSubMap.Get("someevent");
    EXPECT_EQ(2u, entries.size());
}

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_EventKeyStoredLowercase)
{
    auto ctx = MakeContext();

    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "SomeUpperCaseEvent");

    // Stored under lowercased key.
    EXPECT_TRUE(_impl->mSubMap.Exists("someuppercaseevent"));
    EXPECT_FALSE(_impl->mSubMap.Exists("SomeUpperCaseEvent"));
}

// =============================================================================
// Subscribe() — listen = false (unsubscribe)
// =============================================================================

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_False_ExistingSubscriber_ReturnsErrorNone)
{
    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "someEvent");

    const Core::hresult rc = _impl->Subscribe(ctx, /*listen=*/false, "org.rdk.SomePlugin", "someEvent");

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_False_RemovesContextFromMap)
{
    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "someEvent");
    ASSERT_TRUE(_impl->mSubMap.Exists("someevent"));

    _impl->Subscribe(ctx, false, "org.rdk.SomePlugin", "someEvent");

    EXPECT_FALSE(_impl->mSubMap.Exists("someevent"));
}

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_False_OneOfTwoSubscribersRemoved_EventStillExists)
{
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");
    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "someEvent");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "someEvent");

    _impl->Subscribe(ctx1, false, "org.rdk.SomePlugin", "someEvent");

    EXPECT_TRUE(_impl->mSubMap.Exists("someevent"));
    const auto entries = _impl->mSubMap.Get("someevent");
    EXPECT_EQ(1u, entries.size());
    EXPECT_EQ(ctx2.connectionId, entries[0].connectionId);
}

TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_False_NoExistingSubscriber_ReturnsErrorNone)
{
    // Unsubscribing for an event that was never subscribed should still succeed.
    auto ctx = MakeContext();

    const Core::hresult rc = _impl->Subscribe(ctx, false, "org.rdk.SomePlugin", "nonexistentEvent");

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

// =============================================================================
// Subscribe() — ThunderSubscriptionManager integration
// (verify that HandleAppEventNotifier is called when a new event is subscribed)
// =============================================================================

TEST_F(AppNotificationsThunderTest, Subscribe_Listen_True_TriggersThunderSubscription)
{
    SetupHandlerMock("org.rdk.SomePlugin", "newEvent", /*returnStatus=*/true, Core::ERROR_NONE);

    auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "newEvent"));

    DrainWorkerPool();
}

TEST_F(AppNotificationsThunderTest, Subscribe_Listen_False_LastSubscriber_TriggersThunderUnsubscription)
{
    SetupHandlerMock("org.rdk.SomePlugin", "myevent",
                     /*returnStatus=*/true, Core::ERROR_NONE, /*repeatably=*/true);

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "myEvent");
    DrainWorkerPool();

    // Now unsubscribe — should submit unsubscription job.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(ctx, false, "org.rdk.SomePlugin", "myEvent"));
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mSubMap.Exists("myevent"));
}

// =============================================================================
// Emit() — basic return-code tests  (base fixture)
// =============================================================================

TEST_F(AppNotificationsTest, Emit_AlwaysReturnsErrorNone)
{
    const Core::hresult rc = _impl->Emit("someEvent", "{\"key\":\"value\"}", "app1");

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppNotificationsTest, Emit_NoSubscribers_ReturnsErrorNone)
{
    // No subscribers registered; Emit should still succeed.
    const Core::hresult rc = _impl->Emit("unknownEvent", "{}", "");

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

// =============================================================================
// Emit() — dispatch path tests  (AppNotificationsDispatchTest fixture)
// =============================================================================

TEST_F(AppNotificationsDispatchTest, Emit_EmptyAppId_BroadcastsToAllSubscribers)
{
    // Subscribe two contexts for the same event.
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");
    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "testEvent");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "testEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Both subscribers should receive the event.
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("testEvent"), ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("testEvent", "{}", /*appId=*/""));
    DrainWorkerPool();
}

TEST_F(AppNotificationsDispatchTest, Emit_SpecificAppId_OnlyMatchingSubscriberReceivesEvent)
{
    auto ctx1 = MakeContext(1, 100, "target.app", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "other.app",  "org.rdk.AppGateway");
    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "targetEvent");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "targetEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Only the subscriber with appId="target.app" should be dispatched.
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("targetEvent"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("targetEvent", "{}", "target.app"));
    DrainWorkerPool();
}

TEST_F(AppNotificationsDispatchTest, Emit_DispatchesToLaunchDelegate_WhenOriginIsNotGateway)
{
    // Subscribe with origin != APP_GATEWAY_CALLSIGN → DispatchToLaunchDelegate path.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.LaunchDelegate");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "delegateEvent");

    auto* responder = SetupResponderMock("org.rdk.LaunchDelegate");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("delegateEvent"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("delegateEvent", "{}", ""));
    DrainWorkerPool();
}

TEST_F(AppNotificationsDispatchTest, Emit_VersionedEvent_StripsVersionSuffixBeforeDispatch)
{
    // Subscribe to the unversioned event name.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "myevent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Emit the versioned event "myevent.v8"; dispatch should strip ".v8" → "myevent".
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("myevent"), ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("myevent.v8", "{}", ""));
    DrainWorkerPool();
}

TEST_F(AppNotificationsDispatchTest, Emit_GatewayOrigin_DispatchesToAppGatewayCallsign)
{
    auto ctx = MakeContext(1, 10, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "gwEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Responder for LaunchDelegate must NOT be contacted.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                                    ::testing::StrEq("org.rdk.LaunchDelegate")))
        .Times(0);

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    _impl->Emit("gwEvent", "{}", "");
    DrainWorkerPool();
}

TEST_F(AppNotificationsDispatchTest, Emit_NonGatewayOrigin_DispatchesToLaunchDelegateCallsign)
{
    auto ctx = MakeContext(1, 10, "app1", "org.rdk.LaunchDelegate");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "ldEvent");

    auto* responder = SetupResponderMock("org.rdk.LaunchDelegate");

    // AppGateway responder must NOT be contacted.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                                    ::testing::StrEq("org.rdk.AppGateway")))
        .Times(0);

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    _impl->Emit("ldEvent", "{}", "");
    DrainWorkerPool();
}

TEST_F(AppNotificationsDispatchTest, Emit_ResponderUnavailable_DoesNotCrash)
{
    auto ctx = MakeContext(1, 10, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "noResponderEvent");

    // Return nullptr → DispatchToGateway logs an error but must not crash.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                                    ::testing::StrEq("org.rdk.AppGateway")))
        .WillRepeatedly(::testing::Return(nullptr));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("noResponderEvent", "{}", ""));
    DrainWorkerPool();
}

// =============================================================================
// Cleanup() tests
// =============================================================================

TEST_F(AppNotificationsCleanupTest, Cleanup_AlwaysReturnsErrorNone)
{
    const Core::hresult rc = _impl->Cleanup(/*connectionId=*/42, /*origin=*/"org.rdk.AppGateway");

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppNotificationsCleanupTest, Cleanup_RemovesAllContextsMatchingConnectionIdAndOrigin)
{
    const uint32_t connId = 200;
    const string origin   = "org.rdk.AppGateway";

    auto ctx1 = MakeContext(1, connId,   "app1", origin);
    auto ctx2 = MakeContext(2, connId,   "app2", origin);
    auto ctx3 = MakeContext(3, connId+1, "app3", origin); // different connection

    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "event1");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "event2");
    _impl->Subscribe(ctx3, true, "org.rdk.SomePlugin", "event3");

    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connId, origin));

    // event1 and event2 belong to connId — they should be gone.
    EXPECT_FALSE(_impl->mSubMap.Exists("event1"));
    EXPECT_FALSE(_impl->mSubMap.Exists("event2"));
    // event3 belongs to connId+1 — should remain.
    EXPECT_TRUE(_impl->mSubMap.Exists("event3"));
}

TEST_F(AppNotificationsCleanupTest, Cleanup_DifferentOrigin_DoesNotRemoveContexts)
{
    const uint32_t connId       = 300;
    const string originA        = "org.rdk.AppGateway";
    const string originB        = "org.rdk.LaunchDelegate";

    auto ctx = MakeContext(1, connId, "app1", originA);
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "myEvent");

    // Cleanup with same connectionId but different origin — should not remove.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connId, originB));

    EXPECT_TRUE(_impl->mSubMap.Exists("myevent"));
}

TEST_F(AppNotificationsCleanupTest, Cleanup_NoSubscribers_ReturnsErrorNone)
{
    // No subscribers at all; Cleanup should succeed without any side effects.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(999, "org.rdk.AppGateway"));
}

TEST_F(AppNotificationsCleanupTest, Cleanup_MultipleEventsForSameConnection_AllRemoved)
{
    const uint32_t connId = 400;
    const string origin   = "org.rdk.AppGateway";

    auto ctx = MakeContext(1, connId, "appX", origin);

    _impl->Subscribe(ctx, true, "org.rdk.PluginA", "alpha");
    _impl->Subscribe(ctx, true, "org.rdk.PluginB", "beta");
    _impl->Subscribe(ctx, true, "org.rdk.PluginC", "gamma");

    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connId, origin));

    EXPECT_FALSE(_impl->mSubMap.Exists("alpha"));
    EXPECT_FALSE(_impl->mSubMap.Exists("beta"));
    EXPECT_FALSE(_impl->mSubMap.Exists("gamma"));
}

// =============================================================================
// SubscriberMap::Exists / Get internals  (base fixture)
// =============================================================================

TEST_F(AppNotificationsTest, SubscriberMap_Exists_ReturnsFalseForUnknownEvent)
{
    EXPECT_FALSE(_impl->mSubMap.Exists("nonexistent"));
}

TEST_F(AppNotificationsTest, SubscriberMap_Get_ReturnsEmptyVectorForUnknownEvent)
{
    const auto result = _impl->mSubMap.Get("nonexistent");
    EXPECT_TRUE(result.empty());
}

TEST_F(AppNotificationsTest, SubscriberMap_Get_ReturnsCorrectContextAfterSubscribe)
{
    auto ctx = MakeContext(7, 77, "specific.app", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "checkEvent");

    const auto entries = _impl->mSubMap.Get("checkevent");
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(7u,  entries[0].requestId);
    EXPECT_EQ(77u, entries[0].connectionId);
    EXPECT_EQ(string("specific.app"), entries[0].appId);
}

// =============================================================================
// ThunderSubscriptionManager::IsNotificationRegistered  (Thunder sub-fixture)
// =============================================================================

TEST_F(AppNotificationsThunderTest, ThunderManager_NotRegisteredByDefault)
{
    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.SomePlugin", "someEvent"));
}

TEST_F(AppNotificationsThunderTest, ThunderManager_RegisteredAfterSuccessfulSubscription)
{
    SetupHandlerMock("org.rdk.SomePlugin", "registeredevent",
                     /*returnStatus=*/true, Core::ERROR_NONE);

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "registeredEvent");
    DrainWorkerPool();

    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.SomePlugin", "registeredEvent"));
}

TEST_F(AppNotificationsThunderTest, ThunderManager_NotRegisteredWhenHandlerUnavailable)
{
    // QueryInterfaceByCallsign returns nullptr → HandleNotifier fails → not registered.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.MissingPlugin")))
        .WillOnce(::testing::Return(nullptr));

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.MissingPlugin", "ghostEvent");
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.MissingPlugin", "ghostEvent"));
}

TEST_F(AppNotificationsThunderTest, ThunderManager_NotRegisteredWhenHandlerReturnsFailure)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.FailingPlugin")))
        .WillOnce(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.FailingPlugin", "failEvent");
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.FailingPlugin", "failEvent"));
}

// =============================================================================
// Negative / edge-case tests — Subscribe()
// =============================================================================

// When listen=true is called a second time for the same event the map entry is
// already present, so NO new SubscriberJob should be submitted to the worker
// pool (i.e. HandleAppEventNotifier must only be called once, not twice).
TEST_F(AppNotificationsThunderTest, Subscribe_Listen_True_SecondCallSameEvent_DoesNotResubscribeToThunder)
{
    // Expect exactly one HandleAppEventNotifier call across both Subscribe calls.
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.SomePlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::StrEq("dupevent"),
                                                      ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");

    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "dupEvent");
    DrainWorkerPool();
    // Second subscriber for same event: map already has the key, no new Thunder job.
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "dupEvent");
    DrainWorkerPool();

    // Both contexts are in the map.
    EXPECT_EQ(2u, _impl->mSubMap.Get("dupevent").size());
}

// When listen=false is called but the map still has remaining subscribers after
// the removal, no unsubscription job should be submitted to Thunder.
TEST_F(AppNotificationsThunderTest, Subscribe_Listen_False_MapNotEmpty_DoesNotUnsubscribeFromThunder)
{
    // Subscribe Thunder once on the first Subscribe call.
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.SomePlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    // HandleAppEventNotifier must only be called once (subscribe), NOT a second
    // time for the partial unsubscription attempt.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::StrEq("sharedevent"),
                                                      ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");

    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "sharedEvent");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "sharedEvent");
    DrainWorkerPool();

    // Remove only ctx1 — ctx2 remains, map is non-empty, no unsubscription job.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(ctx1, false, "org.rdk.SomePlugin", "sharedEvent"));
    DrainWorkerPool();

    EXPECT_TRUE(_impl->mSubMap.Exists("sharedevent"));
}

// listen=false for a context that does not match any entry in an existing event
// bucket: the entry is not removed, the map key survives, no unsubscription job.
TEST_F(AppNotificationsThunderTest, Subscribe_Listen_False_UnknownContext_MapUnchanged_NoUnsubscribeJob)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.SomePlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    // Unsubscription HandleAppEventNotifier must NOT be called.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_,
                                                      /*listen=*/false, ::testing::_))
        .Times(0);
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_,
                                                      /*listen=*/true, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    auto knownCtx   = MakeContext(1, 100, "known.app", "org.rdk.AppGateway");
    auto unknownCtx = MakeContext(9, 999, "ghost.app", "org.rdk.AppGateway");

    _impl->Subscribe(knownCtx, true, "org.rdk.SomePlugin", "stableEvent");
    DrainWorkerPool();

    // Try to unsubscribe a context that was never subscribed for this event.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(unknownCtx, false, "org.rdk.SomePlugin", "stableEvent"));
    DrainWorkerPool();

    // The original subscriber is still present.
    const auto entries = _impl->mSubMap.Get("stableevent");
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(knownCtx.connectionId, entries[0].connectionId);
}

// Subscribe with an empty event string: stored under the empty key "".
TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_EmptyEvent_StoredUnderEmptyKey)
{
    auto ctx = MakeContext();

    const Core::hresult rc = _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", /*event=*/"");

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(_impl->mSubMap.Exists(""));
}

// Subscribe with an empty module string: returns ERROR_NONE; map entry is still
// created — there is no input-validation gate in Subscribe().
TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_EmptyModule_ReturnsErrorNone)
{
    auto ctx = MakeContext();

    const Core::hresult rc = _impl->Subscribe(ctx, true, /*module=*/"", "someEvent");

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(_impl->mSubMap.Exists("someevent"));
}

// subscribe connectionId=0 is treated as a valid value (no special-casing).
TEST_F(AppNotificationsSubscribeTest, Subscribe_Listen_True_ZeroConnectionId_StoredCorrectly)
{
    auto ctx = MakeContext(/*requestId=*/0, /*connectionId=*/0, "app0", "org.rdk.AppGateway");

    const Core::hresult rc = _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "zeroConnEvent");

    EXPECT_EQ(Core::ERROR_NONE, rc);
    const auto entries = _impl->mSubMap.Get("zeroconnevent");
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(0u, entries[0].connectionId);
}

// HandleAppEventNotifier throws: the worker-pool job catches (or propagates) the
// exception; the event must NOT appear in the registered notifications list and
// the overall implementation must survive.
TEST_F(AppNotificationsThunderTest, Subscribe_Listen_True_HandlerThrows_NotRegistered_NoServerCrash)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.ThrowingPlugin")))
        .WillOnce(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IAppNotificationHandler::IEmitter* /*emitCb*/,
                const string& /*event*/,
                bool /*listen*/,
                bool& status) -> Core::hresult {
                throw std::runtime_error("Test exception from HandleAppEventNotifier");
                status = true;
                return Core::ERROR_NONE;
            }));

    auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(ctx, true, "org.rdk.ThrowingPlugin", "throwEvent"));
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.ThrowingPlugin", "throwEvent"));
}

// HandleAppEventNotifier returns ERROR_NONE but sets status=false: the event
// must NOT be tracked as registered even though the call succeeded.
TEST_F(AppNotificationsThunderTest, Subscribe_Listen_True_HandlerStatusFalse_NotRegistered)
{
    SetupHandlerMock("org.rdk.SomePlugin", "statusFalseEvent",
                     /*returnStatus=*/false, Core::ERROR_NONE);

    auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "statusFalseEvent"));
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.SomePlugin", "statusFalseEvent"));
}

// =============================================================================
// Negative / edge-case tests — Emit()
// =============================================================================

// Emit with an empty event name: returns ERROR_NONE; no subscriber is present so
// the responder must never be contacted.
TEST_F(AppNotificationsDispatchTest, Emit_EmptyEventName_ReturnsErrorNone_NoDispatch)
{
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID, ::testing::_))
        .Times(0);

    const Core::hresult rc = _impl->Emit(/*event=*/"", "{}", "");

    EXPECT_EQ(Core::ERROR_NONE, rc);
    DrainWorkerPool();
}

// Emit with an empty payload string: dispatch still reaches the responder when a
// subscriber is present; the empty payload is forwarded as-is.
TEST_F(AppNotificationsDispatchTest, Emit_EmptyPayload_DispatchesWithEmptyPayload)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "emptyPayloadEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::StrEq("emptyPayloadEvent"),
                                  ::testing::StrEq("")))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("emptyPayloadEvent", /*payload=*/"", ""));
    DrainWorkerPool();
}

// Emit with a specific appId that does not match any subscriber: the responder
// Emit() must never be called.
TEST_F(AppNotificationsDispatchTest, Emit_AppIdNoMatch_ResponderNeverCalled)
{
    auto ctx = MakeContext(1, 100, "registered.app", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "noMatchEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("noMatchEvent", "{}", /*appId=*/"unregistered.app"));
    DrainWorkerPool();
}

// Emit for an event that has no subscribers: returns ERROR_NONE, and the
// responder QueryInterfaceByCallsign must never be called.
TEST_F(AppNotificationsDispatchTest, Emit_NoSubscribersForEvent_ResponderNeverQueried)
{
    // Subscribe a different event so the map is not completely empty.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "otherEvent");

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("unknownEvent", "{}", ""));
    DrainWorkerPool();
}

// When the responder's Emit() returns an error code, the overall Emit() still
// returns ERROR_NONE (fire-and-forget; the job does not propagate errors).
TEST_F(AppNotificationsDispatchTest, Emit_ResponderEmitReturnsError_TopLevelStillReturnsErrorNone)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "errorEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("errorEvent", "{}", ""));
    DrainWorkerPool();
}

// When the responder's Emit() throws an exception, the worker-pool job must not
// propagate the exception and crash the process.
TEST_F(AppNotificationsDispatchTest, Emit_ResponderEmitThrows_DoesNotCrash)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "throwingEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const Exchange::GatewayContext& /*ctx*/,
                const string& /*method*/,
                const string& /*payload*/) -> Core::hresult {
                throw std::runtime_error("Test exception from Emit");
                return Core::ERROR_NONE;
            }));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("throwingEvent", "{}", ""));
    DrainWorkerPool();
}

// After the cached gateway responder pointer is acquired on the first dispatch,
// subsequent dispatches must reuse it and must NOT call QueryInterfaceByCallsign
// a second time.
TEST_F(AppNotificationsDispatchTest, Emit_GatewayResponderCached_QueryCalledOnlyOnce)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "cachedEvent");

    auto* responder = new ::testing::NiceMock<AppGatewayResponderMock>();
    responder->AddRef();

    // QueryInterfaceByCallsign must be called exactly once across two Emit calls.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                                    ::testing::StrEq("org.rdk.AppGateway")))
        .Times(1)
        .WillOnce(::testing::Return(static_cast<void*>(responder)));

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    _impl->Emit("cachedEvent", "{}", "");
    DrainWorkerPool();
    _impl->Emit("cachedEvent", "{}", "");
    DrainWorkerPool();
}

// LaunchDelegate responder path: after first acquisition the pointer is cached;
// QueryInterfaceByCallsign must not be called again on subsequent Emit calls.
TEST_F(AppNotificationsDispatchTest, Emit_LaunchDelegateResponderCached_QueryCalledOnlyOnce)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.LaunchDelegate");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "ldCachedEvent");

    auto* responder = new ::testing::NiceMock<AppGatewayResponderMock>();
    responder->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                                    ::testing::StrEq("org.rdk.LaunchDelegate")))
        .Times(1)
        .WillOnce(::testing::Return(static_cast<void*>(responder)));

    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    _impl->Emit("ldCachedEvent", "{}", "");
    DrainWorkerPool();
    _impl->Emit("ldCachedEvent", "{}", "");
    DrainWorkerPool();
}

// LaunchDelegate responder unavailable: returns nullptr; no crash; Emit still
// returns ERROR_NONE.
TEST_F(AppNotificationsDispatchTest, Emit_LaunchDelegateResponderUnavailable_DoesNotCrash)
{
    auto ctx = MakeContext(1, 10, "app1", "org.rdk.LaunchDelegate");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "ldNullEvent");

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID,
                                                    ::testing::StrEq("org.rdk.LaunchDelegate")))
        .WillRepeatedly(::testing::Return(nullptr));

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("ldNullEvent", "{}", ""));
    DrainWorkerPool();
}

// =============================================================================
// Negative / edge-case tests — Cleanup()
// =============================================================================

// connectionId=0 is a valid value; Cleanup must remove contexts that carry it.
TEST_F(AppNotificationsCleanupTest, Cleanup_ZeroConnectionId_RemovesMatchingContexts)
{
    const string origin = "org.rdk.AppGateway";
    auto ctx = MakeContext(1, /*connectionId=*/0, "app0", origin);
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "zeroConnCleanEvent");

    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(/*connectionId=*/0, origin));

    EXPECT_FALSE(_impl->mSubMap.Exists("zeroconncleanevent"));
}

// Cleanup with an empty origin string: only removes entries whose origin is also
// the empty string; entries with a real origin are left untouched.
TEST_F(AppNotificationsCleanupTest, Cleanup_EmptyOrigin_OnlyRemovesEmptyOriginContexts)
{
    const uint32_t connId = 500;

    auto ctxEmpty  = MakeContext(1, connId, "app1", /*origin=*/"");
    auto ctxReal   = MakeContext(2, connId, "app2", "org.rdk.AppGateway");

    _impl->Subscribe(ctxEmpty, true, "org.rdk.SomePlugin", "emptyOriginEvent");
    _impl->Subscribe(ctxReal,  true, "org.rdk.SomePlugin", "realOriginEvent");

    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connId, /*origin=*/""));

    EXPECT_FALSE(_impl->mSubMap.Exists("emptyoriginevent"));
    EXPECT_TRUE(_impl->mSubMap.Exists("realoriginevent"));
}

// Cleanup with a matching origin but a different connectionId must leave all
// entries intact.
TEST_F(AppNotificationsCleanupTest, Cleanup_DifferentConnectionId_LeavesAllContextsIntact)
{
    const string origin = "org.rdk.AppGateway";
    auto ctx = MakeContext(1, 600, "app1", origin);
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "connMismatchEvent");

    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(/*connectionId=*/601, origin));

    EXPECT_TRUE(_impl->mSubMap.Exists("connmismatchevent"));
    EXPECT_EQ(1u, _impl->mSubMap.Get("connmismatchevent").size());
}

// After Cleanup removes all subscribers for an event, a subsequent Emit for that
// event must not reach the responder.
TEST_F(AppNotificationsDispatchTest, Emit_AfterCleanup_ResponderNotCalled)
{
    const uint32_t connId = 700;
    const string origin   = "org.rdk.AppGateway";
    auto ctx = MakeContext(1, connId, "app1", origin);
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "cleanedEvent");

    // Cleanup all entries for this connection.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connId, origin));
    EXPECT_FALSE(_impl->mSubMap.Exists("cleanedevent"));

    // Responder must never be contacted since there are no subscribers left.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("cleanedEvent", "{}", ""));
    DrainWorkerPool();
}

// =============================================================================
// Negative / edge-case tests — ThunderSubscriptionManager
// =============================================================================

// Subscribe called twice for the same module+event: Thunder must only be
// contacted once (idempotent guard in ThunderSubscriptionManager::Subscribe).
TEST_F(AppNotificationsThunderTest, ThunderManager_SubscribeSameEventTwice_HandlerCalledOnce)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.IdempotentPlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    // HandleAppEventNotifier must be called only once across two Subscribe calls.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::StrEq("idemevent"),
                                                      /*listen=*/true, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    _impl->mThunderManager.Subscribe("org.rdk.IdempotentPlugin", "idemEvent");
    DrainWorkerPool();
    _impl->mThunderManager.Subscribe("org.rdk.IdempotentPlugin", "idemEvent");
    DrainWorkerPool();

    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.IdempotentPlugin", "idemEvent"));
}

// Unsubscribe called for an event that is not registered: HandleNotifier must
// NOT be called (guarded by IsNotificationRegistered check).
TEST_F(AppNotificationsThunderTest, ThunderManager_UnsubscribeUnregisteredEvent_HandlerNotCalled)
{
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID, ::testing::_))
        .Times(0);

    // Directly exercise the manager — the event was never registered.
    _impl->mThunderManager.Unsubscribe("org.rdk.SomePlugin", "neverRegisteredEvent");
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.SomePlugin", "neverRegisteredEvent"));
}

// HandleAppEventNotifier returns ERROR_GENERAL; the event must not be added to
// the registered list and IsNotificationRegistered returns false.
TEST_F(AppNotificationsThunderTest, ThunderManager_HandlerReturnsErrorGeneral_NotRegistered)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.ErrorPlugin")))
        .WillOnce(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_GENERAL));

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.ErrorPlugin", "errorGeneralEvent");
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.ErrorPlugin", "errorGeneralEvent"));
}

// HandleAppEventNotifier sets status=false even though it returns ERROR_NONE:
// the event must still not be tracked as registered.
TEST_F(AppNotificationsThunderTest, ThunderManager_HandlerStatusFalse_NotRegisteredAfterErrorNone)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.StatusFalsePlugin")))
        .WillOnce(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(false),
            ::testing::Return(Core::ERROR_NONE)));

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.StatusFalsePlugin", "statusFalseDirectEvent");
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.StatusFalsePlugin", "statusFalseDirectEvent"));
}

// HandleAppEventNotifier throws an exception during a Subscribe job: the
// registered notifications list must remain empty and the implementation must
// not crash.
TEST_F(AppNotificationsThunderTest, ThunderManager_HandlerThrows_NotRegistered_NoProcessCrash)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.BombPlugin")))
        .WillOnce(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](Exchange::IAppNotificationHandler::IEmitter* /*emitCb*/,
                const string& /*event*/,
                bool /*listen*/,
                bool& status) -> Core::hresult {
                throw std::runtime_error("Test exception from ThunderManager handler");
                status = true;
                return Core::ERROR_NONE;
            }));

    auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_NONE, _impl->Subscribe(ctx, true, "org.rdk.BombPlugin", "bombEvent"));
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.BombPlugin", "bombEvent"));
}

// QueryInterfaceByCallsign returns nullptr for the handler: HandleNotifier
// returns false; the event is not registered and Unsubscribe is therefore a
// no-op.
TEST_F(AppNotificationsThunderTest, ThunderManager_HandlerQueryReturnsNull_NotRegistered)
{
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.AbsentPlugin")))
        .Times(1)
        .WillOnce(::testing::Return(nullptr));

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.AbsentPlugin", "absentEvent");
    DrainWorkerPool();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.AbsentPlugin", "absentEvent"));
    // Unsubscribe without a prior successful registration must not call the handler.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID, ::testing::_))
        .Times(0);
    _impl->mThunderManager.Unsubscribe("org.rdk.AbsentPlugin", "absentEvent");
    DrainWorkerPool();
}

// =============================================================================
// Coverage gap — CleanupNotifications: partial removal (++it branch, L107)
//
// When a Cleanup call removes only *some* contexts from a multi-subscriber event
// bucket, CleanupNotifications must advance the iterator (++it) and leave the
// remaining entries intact.  This exercises the else-branch at L107 where vec is
// non-empty after the erase and the loop continues to the next map key.
// =============================================================================

TEST_F(AppNotificationsCleanupTest, Cleanup_PartialRemoval_IteratorAdvances_RemainingEntryPreserved)
{
    const uint32_t connIdA = 800;
    const uint32_t connIdB = 801;
    const string origin    = "org.rdk.AppGateway";

    // Two subscribers for the same event key, different connectionIds.
    auto ctxA = MakeContext(1, connIdA, "appA", origin);
    auto ctxB = MakeContext(2, connIdB, "appB", origin);

    _impl->Subscribe(ctxA, true, "org.rdk.SomePlugin", "partialEvent");
    _impl->Subscribe(ctxB, true, "org.rdk.SomePlugin", "partialEvent");

    // Remove only connIdA — the bucket still holds ctxB so ++it must execute.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connIdA, origin));

    // Map key must still exist with exactly one remaining entry.
    EXPECT_TRUE(_impl->mSubMap.Exists("partialevent"));
    const auto entries = _impl->mSubMap.Get("partialevent");
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(connIdB, entries[0].connectionId);
}

// A second event key exists alongside the cleaned one.  After cleaning connIdA
// from "partialEvent" the iterator advances and leaves "siblingEvent" untouched,
// covering the ++it path when the map has more than one key.
TEST_F(AppNotificationsCleanupTest, Cleanup_PartialRemoval_SiblingEventKeyUntouched)
{
    const uint32_t connIdA = 810;
    const uint32_t connIdB = 811;
    const string origin    = "org.rdk.AppGateway";

    auto ctxA        = MakeContext(1, connIdA, "appA", origin);
    auto ctxB        = MakeContext(2, connIdB, "appB", origin);
    auto ctxSibling  = MakeContext(3, connIdA, "appA", origin);

    // ctxA subscribes to two different events; ctxB shares "mainEvent".
    _impl->Subscribe(ctxA,       true, "org.rdk.SomePlugin", "mainEvent");
    _impl->Subscribe(ctxB,       true, "org.rdk.SomePlugin", "mainEvent");
    _impl->Subscribe(ctxSibling, true, "org.rdk.SomePlugin", "siblingEvent");

    // Clean connIdA: removes ctxA from "mainEvent" (ctxB remains → ++it)
    // and removes ctxSibling from "siblingEvent" (bucket empties → erase).
    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(connIdA, origin));

    EXPECT_TRUE(_impl->mSubMap.Exists("mainevent"));
    EXPECT_EQ(1u, _impl->mSubMap.Get("mainevent").size());
    EXPECT_FALSE(_impl->mSubMap.Exists("siblingevent"));
}

// =============================================================================
// Coverage gap — EventUpdate else-branch (L182): LOGWARN path when there are
// no active listeners for the emitted event.
//
// The base-fixture test Emit_NoSubscribers_ReturnsErrorNone does not drain the
// worker pool, so EmitJob::Dispatch() → EventUpdate() is never executed there.
// This test explicitly drains the pool to reach L182.
// =============================================================================

TEST_F(AppNotificationsTest, Emit_NoSubscribers_WorkerJobExecutes_LogWarningPath)
{
    // No subscribers registered for "ghostEvent".  After draining the pool the
    // EmitJob fires, EventUpdate finds no entry, and hits the LOGWARN branch.
    // The responder must never be contacted.
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("ghostEvent", "{}", ""));
    DrainWorkerPool();  // drives EmitJob::Dispatch → EventUpdate else-branch
}

// Same path but with a non-empty appId: EventUpdate still finds no subscriber
// map entry and logs the warning.
TEST_F(AppNotificationsTest, Emit_NoSubscribersWithAppId_WorkerJobExecutes_LogWarningPath)
{
    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppGatewayResponder::ID, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_NONE, _impl->Emit("ghostEvent2", "{}", "specific.app"));
    DrainWorkerPool();
}

// =============================================================================
// Coverage gap — UnregisterNotification success path (L292-294):
// HandleNotifier returns true on listen=false → entry erased from
// mRegisteredNotifications.
//
// The full round-trip: Subscribe (registers), then Subscribe(listen=false) with
// the last subscriber (submits unsubscription SubscriberJob) → Unsubscribe →
// UnregisterNotification → HandleNotifier returns status=true → entry erased.
// After draining IsNotificationRegistered must return false.
// =============================================================================

TEST_F(AppNotificationsThunderTest, UnregisterNotification_HandlerReturnsTrueOnUnsubscribe_EntryErased)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.FullCyclePlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    // subscribe call: status=true → RegisterNotification records the entry.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_,
                                                      ::testing::StrEq("fullcycleevent"),
                                                      /*listen=*/true,
                                                      ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    // unsubscribe call: status=true → UnregisterNotification erases the entry.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_,
                                                      ::testing::StrEq("fullcycleevent"),
                                                      /*listen=*/false,
                                                      ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");

    // Subscribe → SubscriberJob fires → RegisterNotification → HandleNotifier(listen=true)
    _impl->Subscribe(ctx, true, "org.rdk.FullCyclePlugin", "fullCycleEvent");
    DrainWorkerPool();
    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.FullCyclePlugin", "fullCycleEvent"));

    // Unsubscribe → SubscriberJob fires → UnregisterNotification → HandleNotifier(listen=false)
    _impl->Subscribe(ctx, false, "org.rdk.FullCyclePlugin", "fullCycleEvent");
    DrainWorkerPool();

    // Entry must be gone from mRegisteredNotifications.
    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.FullCyclePlugin", "fullCycleEvent"));
}

// UnregisterNotification: HandleNotifier returns false on listen=false (e.g.
// handler returns ERROR_NONE but status=false) → entry is NOT erased.
// This is the false-branch of the if() guard at L292 inside UnregisterNotification.
TEST_F(AppNotificationsThunderTest, UnregisterNotification_HandlerReturnsFalseOnUnsubscribe_EntryRetained)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.RetainPlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    // subscribe: status=true → entry recorded.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_,
                                                      ::testing::StrEq("retainevent"),
                                                      /*listen=*/true,
                                                      ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    // unsubscribe: status=false → UnregisterNotification does NOT erase the entry.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_,
                                                      ::testing::StrEq("retainevent"),
                                                      /*listen=*/false,
                                                      ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(false),
            ::testing::Return(Core::ERROR_NONE)));

    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");

    _impl->Subscribe(ctx, true, "org.rdk.RetainPlugin", "retainEvent");
    DrainWorkerPool();
    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.RetainPlugin", "retainEvent"));

    _impl->Subscribe(ctx, false, "org.rdk.RetainPlugin", "retainEvent");
    DrainWorkerPool();

    // Entry was NOT erased because HandleNotifier returned false.
    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.RetainPlugin", "retainEvent"));
}

// =============================================================================
// Coverage gap — ThunderSubscriptionManager destructor (L224-235):
// Registered notifications are unsubscribed during object teardown.
//
// The destructor is exercised every time _impl is destroyed at end of test, but
// only if mRegisteredNotifications is non-empty.  Here we explicitly verify that
// HandleAppEventNotifier(listen=false) is called for every registered event
// when the implementation object goes out of scope.
// =============================================================================

TEST_F(AppNotificationsThunderTest, ThunderManager_Destructor_UnsubscribesAllRegisteredNotifications)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service, QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                                    ::testing::StrEq("org.rdk.DtorPlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    // subscribe calls: two distinct events registered.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_,
                                                      /*listen=*/true, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    // destructor must call HandleAppEventNotifier(listen=false) once per event.
    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(::testing::_, ::testing::_,
                                                      /*listen=*/false, ::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    auto ctx = MakeContext();
    _impl->Subscribe(ctx, true, "org.rdk.DtorPlugin", "dtorEvent1");
    _impl->Subscribe(ctx, true, "org.rdk.DtorPlugin", "dtorEvent2");
    DrainWorkerPool();

    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.DtorPlugin", "dtorEvent1"));
    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered("org.rdk.DtorPlugin", "dtorEvent2"));

    // Destroy _impl HERE — do NOT pre-clear mRegisteredNotifications first so
    // that ~ThunderSubscriptionManager exercises the unsubscription path.
    // TearDown will see _impl == nullptr and skip its own pre-clear + reset.
    // The worker pool is still running, so HandleNotifier's
    // QueryInterfaceByCallsign call is safe.
    _impl.reset();
    // GMock verifies the Times(2) listen=false expectation was satisfied.
}

// =============================================================================
// End-to-end round-trip tests using EmitterNotificationHandler
//
// These tests exercise the full flow:
//   Subscribe()
//     → SubscriberJob dispatched
//     → HandleAppEventNotifier() called with the plugin's IEmitter (emitCb)
//     → test captures emitCb and calls emitCb->Emit(event, payload, appId)
//     → EmitJob dispatched
//     → AppNotificationsImplementation::EventUpdate() finds subscriber
//     → mAppGateway->Emit() called on the responder
//
// AppNotificationsEndToEndTest inherits both SetupHandlerMock (Thunder path)
// and SetupResponderMock (dispatch path).
// =============================================================================

class AppNotificationsEndToEndTest : public AppNotificationsDispatchTest {
protected:
    // Mirrors SetupHandlerMock from AppNotificationsThunderTest; re-defined
    // here so the combined fixture does not require multiple inheritance.
    AppNotificationHandlerMock* SetupHandlerMockE2E(
        const string& module,
        const string& event,
        Exchange::IAppNotificationHandler::IEmitter** capturedEmitter)
    {
        auto* mock = new ::testing::NiceMock<AppNotificationHandlerMock>();
        mock->AddRef();

        EXPECT_CALL(_service,
                    QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                             ::testing::StrEq(module)))
            .WillOnce(::testing::Return(static_cast<void*>(mock)));

        EXPECT_CALL(*mock, HandleAppEventNotifier(
                        ::testing::_, ::testing::StrEq(event),
                        /*listen=*/true, ::testing::_))
            .WillOnce(::testing::Invoke(
                [capturedEmitter](
                    Exchange::IAppNotificationHandler::IEmitter* emitCb,
                    const string& /*event*/,
                    bool /*listen*/,
                    bool& status) -> Core::hresult {
                    *capturedEmitter = emitCb;
                    status = true;
                    return Core::ERROR_NONE;
                }));

        return mock;
    }
};

// A subscribed app receives an emit triggered by an external plugin calling
// emitCb->Emit().  The responder's Emit() must be called with the exact event
// name and payload forwarded by EventUpdate().
TEST_F(AppNotificationsEndToEndTest,
       EmitterEmit_TriggersResponderEmit_MatchingSubscriber)
{
    constexpr uint32_t TIMEOUT_MS = 2000;

    Exchange::IAppNotificationHandler::IEmitter* capturedEmitter = nullptr;
    auto* handlerMock = SetupHandlerMockE2E(
        "org.rdk.FireboltPlugin", "onScreenChanged", &capturedEmitter);
    (void)handlerMock;

    // Subscribe one context for "onScreenChanged".
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    EXPECT_EQ(Core::ERROR_NONE,
              _impl->Subscribe(ctx, true, "org.rdk.FireboltPlugin", "onScreenChanged"));
    DrainWorkerPool();

    ASSERT_NE(nullptr, capturedEmitter)
        << "HandleAppEventNotifier was not called — emitCb not captured";

    // Wire up the responder so the EmitJob can dispatch.
    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    Core::Sink<EmitterNotificationHandler> emitterHandler;

    EXPECT_CALL(*responder,
                Emit(::testing::_,
                     ::testing::StrEq("onScreenChanged"),
                     ::testing::StrEq("{\"screen\":\"home\"}")))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&emitterHandler](const Exchange::GatewayContext& /*ctx*/,
                              const string& /*method*/,
                              const string& /*payload*/) -> Core::hresult {
                // Notify the handler so WaitForRequestStatus unblocks.
                emitterHandler.Emit("onScreenChanged", "{\"screen\":\"home\"}", "");
                return Core::ERROR_NONE;
            }));

    // External plugin calls emitCb->Emit() — this is the notification path.
    capturedEmitter->Emit("onScreenChanged", "{\"screen\":\"home\"}", "");

    // Wait for the end-to-end round-trip to complete.
    const uint32_t signalled =
        emitterHandler.WaitForRequestStatus(TIMEOUT_MS, AppNotifications_onEmit);
    EXPECT_NE(AppNotifications_StateInvalid, signalled & AppNotifications_onEmit);
}

// When emitCb->Emit() is called with a specific appId, EventUpdate() should
// only dispatch to the subscriber whose appId matches; the responder Emit()
// must still be called exactly once with the correct appId-filtered payload.
TEST_F(AppNotificationsEndToEndTest,
       EmitterEmit_WithMatchingAppId_DispatchesOnlyToThatApp)
{
    constexpr uint32_t TIMEOUT_MS = 2000;

    Exchange::IAppNotificationHandler::IEmitter* capturedEmitter = nullptr;
    auto* handlerMock = SetupHandlerMockE2E(
        "org.rdk.FireboltPlugin", "onStateChange", &capturedEmitter);
    (void)handlerMock;

    // Two subscribers for the same event, different appIds.
    auto ctxA = MakeContext(1, 100, "target.app",  "org.rdk.AppGateway");
    auto ctxB = MakeContext(2, 101, "other.app",   "org.rdk.AppGateway");

    _impl->Subscribe(ctxA, true, "org.rdk.FireboltPlugin", "onStateChange");
    _impl->Subscribe(ctxB, true, "org.rdk.FireboltPlugin", "onStateChange");
    DrainWorkerPool();

    ASSERT_NE(nullptr, capturedEmitter);

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    Core::Sink<EmitterNotificationHandler> emitterHandler;

    // Only one Emit() call expected — the one for "target.app".
    EXPECT_CALL(*responder,
                Emit(::testing::_,
                     ::testing::StrEq("onStateChange"),
                     ::testing::StrEq("{\"state\":\"active\"}")))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&emitterHandler](const Exchange::GatewayContext& /*ctx*/,
                              const string& /*method*/,
                              const string& /*payload*/) -> Core::hresult {
                emitterHandler.Emit("onStateChange", "{\"state\":\"active\"}", "target.app");
                return Core::ERROR_NONE;
            }));

    // Emit targeted at "target.app" only.
    capturedEmitter->Emit("onStateChange", "{\"state\":\"active\"}", "target.app");

    const uint32_t signalled =
        emitterHandler.WaitForRequestStatus(TIMEOUT_MS, AppNotifications_onEmit);
    EXPECT_NE(AppNotifications_StateInvalid, signalled & AppNotifications_onEmit);
    EXPECT_EQ("onStateChange", emitterHandler.GetEvent());
}

// =============================================================================
// Notification tests via EmitJob::Create + Dispatch (direct, synchronous)
//
// EmitJob::Create() + casting to ProxyType<EmitJob> + Dispatch() bypasses the
// worker pool entirely, making assertions synchronous.  This is the most
// direct way to exercise EventUpdate() → Dispatch() → responder->Emit().
// =============================================================================

// EmitJob dispatched directly: broadcasts to all subscribers when appId is empty.
// Three subscribers for the same event must each trigger a responder->Emit() call.
TEST_F(AppNotificationsEndToEndTest,
       EmitJob_DirectDispatch_BroadcastsToAllSubscribersWhenAppIdEmpty)
{
    auto ctx1 = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(2, 101, "app2", "org.rdk.AppGateway");
    auto ctx3 = MakeContext(3, 102, "app3", "org.rdk.AppGateway");

    _impl->Subscribe(ctx1, true, "org.rdk.SomePlugin", "broadcastEvent");
    _impl->Subscribe(ctx2, true, "org.rdk.SomePlugin", "broadcastEvent");
    _impl->Subscribe(ctx3, true, "org.rdk.SomePlugin", "broadcastEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // All three subscribers must receive the event.
    EXPECT_CALL(*responder,
                Emit(::testing::_, ::testing::StrEq("broadcastEvent"),
                     ::testing::StrEq("{\"key\":\"val\"}")))
        .Times(3)
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Direct dispatch — synchronous, no DrainWorkerPool needed.
    auto job = Core::ProxyType<AppNotificationsImplementation::EmitJob>::Create(
        _impl.get(), "broadcastEvent", "{\"key\":\"val\"}", /*appId=*/"");
    job->Dispatch();
}

// EmitJob dispatched directly with a specific appId: only the matching subscriber
// receives the dispatch.  The other subscriber must never trigger responder->Emit().
TEST_F(AppNotificationsEndToEndTest,
       EmitJob_DirectDispatch_OnlyMatchingAppIdReceivesEvent)
{
    auto ctxMatch  = MakeContext(1, 100, "match.app", "org.rdk.AppGateway");
    auto ctxOther  = MakeContext(2, 101, "other.app", "org.rdk.AppGateway");

    _impl->Subscribe(ctxMatch, true, "org.rdk.SomePlugin", "filteredEvent");
    _impl->Subscribe(ctxOther, true, "org.rdk.SomePlugin", "filteredEvent");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Exactly one call: only the matching appId.
    EXPECT_CALL(*responder,
                Emit(::testing::_, ::testing::StrEq("filteredEvent"),
                     ::testing::StrEq("{\"data\":1}")))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    auto job = Core::ProxyType<AppNotificationsImplementation::EmitJob>::Create(
        _impl.get(), "filteredEvent", "{\"data\":1}", /*appId=*/"match.app");
    job->Dispatch();
}

// EmitJob dispatched with a versioned event name ("eventName.v8"): EventUpdate()
// strips the ".v8" suffix when calling Dispatch(), so the responder receives the
// base event name without the version suffix.
TEST_F(AppNotificationsEndToEndTest,
       EmitJob_DirectDispatch_VersionedEvent_SuffixStrippedBeforeDispatch)
{
    // Subscribe using the base event name (no suffix) — this is what Subscribe() stores.
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "onDiscovery");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Responder must receive the BASE name ("onDiscovery"), not "onDiscovery.v8".
    EXPECT_CALL(*responder,
                Emit(::testing::_, ::testing::StrEq("onDiscovery"),
                     ::testing::StrEq("{}")))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    // Emit the versioned form — EventUpdate performs the lookup on the lowercased
    // base name and dispatches using clearKey (suffix stripped).
    auto job = Core::ProxyType<AppNotificationsImplementation::EmitJob>::Create(
        _impl.get(), "onDiscovery.v8", "{}", /*appId=*/"");
    job->Dispatch();
}

// EmitJob dispatched after Cleanup() removed all subscribers: the responder
// must never be called.
TEST_F(AppNotificationsEndToEndTest,
       EmitJob_DirectDispatch_AfterCleanupRemovesAllSubscribers_ResponderNeverCalled)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.SomePlugin", "cleanedEvent");
    ASSERT_TRUE(_impl->mSubMap.Exists("cleanedevent"));

    // Remove all subscribers for this connection.
    EXPECT_EQ(Core::ERROR_NONE, _impl->Cleanup(100, "org.rdk.AppGateway"));
    EXPECT_FALSE(_impl->mSubMap.Exists("cleanedevent"));

    auto* responder = SetupResponderMock("org.rdk.AppGateway");
    EXPECT_CALL(*responder, Emit(::testing::_, ::testing::_, ::testing::_)).Times(0);

    auto job = Core::ProxyType<AppNotificationsImplementation::EmitJob>::Create(
        _impl.get(), "cleanedEvent", "{}", /*appId=*/"");
    job->Dispatch();
}

// =============================================================================
// Notification tests via _impl->mEmitter.Emit() (asynchronous through worker pool)
//
// _impl->mEmitter is the Core::Sink<Emitter> member.  Calling Emit() on it
// submits an EmitJob to the worker pool, making the dispatch asynchronous.
// DrainWorkerPool() is required before asserting responder calls.
// =============================================================================

// Calling _impl->mEmitter.Emit() submits an EmitJob; after draining the pool
// the responder receives the event.
TEST_F(AppNotificationsEndToEndTest,
       InternalEmitter_Emit_SubmitsEmitJob_ResponderCalledAfterDrain)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    _impl->Subscribe(ctx, true, "org.rdk.FireboltPlugin", "onPowerState");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    EXPECT_CALL(*responder,
                Emit(::testing::_, ::testing::StrEq("onPowerState"),
                     ::testing::StrEq("{\"state\":\"on\"}")))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    // Call the internal Emitter directly — this is the path triggered when an
    // external plugin calls HandleAppEventNotifier() with mEmitter as the callback.
    _impl->mEmitter.Emit("onPowerState", "{\"state\":\"on\"}", /*appId=*/"");

    DrainWorkerPool();
}

// Calling _impl->mEmitter.Emit() with a specific appId: only the matching
// subscriber receives the dispatch after the worker pool drains.
TEST_F(AppNotificationsEndToEndTest,
       InternalEmitter_Emit_WithAppId_OnlyMatchingSubscriberDispatched)
{
    auto ctxTarget = MakeContext(1, 100, "tv.app",    "org.rdk.AppGateway");
    auto ctxOther  = MakeContext(2, 101, "radio.app", "org.rdk.AppGateway");

    _impl->Subscribe(ctxTarget, true, "org.rdk.FireboltPlugin", "onVolumeChange");
    _impl->Subscribe(ctxOther,  true, "org.rdk.FireboltPlugin", "onVolumeChange");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Only one responder call — for "tv.app".
    EXPECT_CALL(*responder,
                Emit(::testing::_, ::testing::StrEq("onVolumeChange"),
                     ::testing::StrEq("{\"volume\":50}")))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    _impl->mEmitter.Emit("onVolumeChange", "{\"volume\":50}", /*appId=*/"tv.app");

    DrainWorkerPool();
}

// Calling _impl->mEmitter.Emit() with a versioned event name: the EmitJob
// dispatches with the base name (suffix stripped) after draining the pool.
TEST_F(AppNotificationsEndToEndTest,
       InternalEmitter_Emit_VersionedEvent_SuffixStrippedAfterDrain)
{
    auto ctx = MakeContext(1, 100, "app1", "org.rdk.AppGateway");
    // Subscribe to the base name.
    _impl->Subscribe(ctx, true, "org.rdk.FireboltPlugin", "onMediaStatus");

    auto* responder = SetupResponderMock("org.rdk.AppGateway");

    // Responder must receive the base name ("onMediaStatus"), not "onMediaStatus.v8".
    EXPECT_CALL(*responder,
                Emit(::testing::_, ::testing::StrEq("onMediaStatus"),
                     ::testing::StrEq("{\"status\":\"playing\"}")))
        .Times(1)
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    // Emitter called with the versioned name.
    _impl->mEmitter.Emit("onMediaStatus.v8", "{\"status\":\"playing\"}", /*appId=*/"");

    DrainWorkerPool();
}

// =============================================================================
// Notification tests via SubscriberJob::Create + Dispatch (direct, synchronous)
//
// SubscriberJob::Create(..., subscribe=true)->Dispatch() calls
// ThunderSubscriptionManager::Subscribe() synchronously, testing the
// RegisterNotification path without the worker pool.
// =============================================================================

// SubscriberJob dispatched directly with subscribe=true: calls
// ThunderSubscriptionManager::Subscribe() which invokes HandleAppEventNotifier.
// After dispatch the event must appear in the registered-notifications list.
TEST_F(AppNotificationsEndToEndTest,
       SubscriberJob_DirectDispatch_Subscribe_RegistersNotification)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service,
                QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                         ::testing::StrEq("org.rdk.DirectPlugin")))
        .WillOnce(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(
                    ::testing::_, ::testing::StrEq("directEvent"),
                    /*listen=*/true, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    // Direct synchronous dispatch — no worker pool.
    auto job = Core::ProxyType<AppNotificationsImplementation::SubscriberJob>::Create(
        _impl.get(), "org.rdk.DirectPlugin", "directEvent", /*subscribe=*/true);
    job->Dispatch();

    EXPECT_TRUE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.DirectPlugin", "directEvent"));
}

// SubscriberJob dispatched directly with subscribe=false after a successful
// registration: calls UnregisterNotification.  The event must be removed from
// the registered-notifications list after dispatch.
TEST_F(AppNotificationsEndToEndTest,
       SubscriberJob_DirectDispatch_Unsubscribe_UnregistersNotification)
{
    auto* handlerMock = new ::testing::NiceMock<AppNotificationHandlerMock>();
    handlerMock->AddRef();

    EXPECT_CALL(_service,
                QueryInterfaceByCallsign(Exchange::IAppNotificationHandler::ID,
                                         ::testing::StrEq("org.rdk.UnsubPlugin")))
        .WillRepeatedly(::testing::Return(static_cast<void*>(handlerMock)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(
                    ::testing::_, ::testing::StrEq("unsubEvent"),
                    /*listen=*/true, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    EXPECT_CALL(*handlerMock, HandleAppEventNotifier(
                    ::testing::_, ::testing::StrEq("unsubevent"),
                    /*listen=*/false, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgReferee<3>(true),
            ::testing::Return(Core::ERROR_NONE)));

    // Subscribe first.
    auto subJob = Core::ProxyType<AppNotificationsImplementation::SubscriberJob>::Create(
        _impl.get(), "org.rdk.UnsubPlugin", "unsubEvent", /*subscribe=*/true);
    subJob->Dispatch();
    ASSERT_TRUE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.UnsubPlugin", "unsubEvent"));

    // Now unsubscribe directly.
    auto unsubJob = Core::ProxyType<AppNotificationsImplementation::SubscriberJob>::Create(
        _impl.get(), "org.rdk.UnsubPlugin", "unsubEvent", /*subscribe=*/false);
    unsubJob->Dispatch();

    EXPECT_FALSE(_impl->mThunderManager.IsNotificationRegistered(
        "org.rdk.UnsubPlugin", "unsubEvent"));
}
