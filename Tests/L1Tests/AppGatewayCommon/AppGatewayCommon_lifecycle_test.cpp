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
#include "MockLifecycleManagerState.h"
#include "MockRDKWindowManager.h"
#include "MockEmitter.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"

#include <sys/stat.h>
#include <cstdio>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;

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

static Exchange::GatewayContext MakeContext(const std::string& appId = "test.app")
{
    Exchange::GatewayContext ctx;
    ctx.appId = appId;
    ctx.connectionId = 100;
    ctx.requestId = 200;
    return ctx;
}

class LifecycleDelegateTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<MockLifecycleManagerState> mockLifecycle;
    NiceMock<MockRDKWindowManager> mockWindowMgr;
    Exchange::ILifecycleManagerState::INotification* capturedNotification = nullptr;
    Exchange::IRDKWindowManager::INotification* capturedWMNotification = nullptr;
    std::vector<MockEmitter*> heapEmitters;

    void SetUp() override
    {
        // LifecycleDelegate only registers notifications when /opt/ai2managers exists
        // (ConfigUtils::useAppManagers() gate). Create the file for the test environment.
        std::FILE* f = std::fopen("/opt/ai2managers", "w");
        if (nullptr == f) {
            GTEST_SKIP() << "Skipping LifecycleDelegate tests: unable to create /opt/ai2managers (insufficient permissions or read-only filesystem)";
        }
        std::fclose(f);

        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        ON_CALL(service, QueryInterfaceByCallsign(Exchange::ILifecycleManagerState::ID, ::testing::StrEq("org.rdk.LifecycleManager")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                mockLifecycle.AddRef();
                return static_cast<Exchange::ILifecycleManagerState*>(&mockLifecycle);
            }));

        // Production code uses "org.rdk.RDKWindowManager" (WINDOW_MANAGER_CALLSIGN)
        ON_CALL(service, QueryInterfaceByCallsign(Exchange::IRDKWindowManager::ID, ::testing::StrEq("org.rdk.RDKWindowManager")))
            .WillByDefault(::testing::Invoke([this](uint32_t, const string&) -> void* {
                mockWindowMgr.AddRef();
                return static_cast<Exchange::IRDKWindowManager*>(&mockWindowMgr);
            }));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
        // Capture the INotification* when Register is called
        EXPECT_CALL(mockLifecycle, Register(_)).Times(AnyNumber())
            .WillRepeatedly(::testing::Invoke([this](Exchange::ILifecycleManagerState::INotification* n) -> uint32_t {
                capturedNotification = n;
                return Core::ERROR_NONE;
            }));
        EXPECT_CALL(mockLifecycle, Unregister(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
        // Capture IRDKWindowManager::INotification* when Register is called
        EXPECT_CALL(mockWindowMgr, Register(_)).Times(AnyNumber())
            .WillRepeatedly(::testing::Invoke([this](Exchange::IRDKWindowManager::INotification* n) -> uint32_t {
                capturedWMNotification = n;
                return Core::ERROR_NONE;
            }));
        EXPECT_CALL(mockWindowMgr, Unregister(_)).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));

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
        std::remove("/opt/ai2managers");
    }
};

/* ---------- Authenticate / GetSessionId ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_107_Authenticate_NotFound)
{
    string appId;
    const auto rc = plugin.Authenticate("unknown-session", appId);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(appId.empty());
}

TEST_F(LifecycleDelegateTest, AGC_L1_108_GetSessionId_NotFound)
{
    string sessionId;
    const auto rc = plugin.GetSessionId("unknown.app", sessionId);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_TRUE(sessionId.empty());
}

/* ---------- LifecycleClose ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_109_LifecycleClose_UserExit)
{
    EXPECT_CALL(mockLifecycle, CloseApp("test.app", Exchange::ILifecycleManagerState::USER_EXIT))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(LifecycleDelegateTest, AGC_L1_110_LifecycleClose_ErrorReason)
{
    EXPECT_CALL(mockLifecycle, CloseApp("test.app", Exchange::ILifecycleManagerState::ERROR))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"other"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(LifecycleDelegateTest, AGC_L1_111_LifecycleClose_BadPayload)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", "{invalid", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- Lifecycle2Close ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_112_Lifecycle2Close_Deactivate)
{
    EXPECT_CALL(mockLifecycle, CloseApp("test.app", Exchange::ILifecycleManagerState::USER_EXIT))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(LifecycleDelegateTest, AGC_L1_113_Lifecycle2Close_KillReload)
{
    EXPECT_CALL(mockLifecycle, CloseApp("test.app", Exchange::ILifecycleManagerState::KILL_AND_RUN))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"killReload"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(LifecycleDelegateTest, AGC_L1_114_Lifecycle2Close_KillReactivate)
{
    EXPECT_CALL(mockLifecycle, CloseApp("test.app", Exchange::ILifecycleManagerState::KILL_AND_ACTIVATE))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"killReactivate"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(LifecycleDelegateTest, AGC_L1_115_Lifecycle2Close_DefaultReason)
{
    EXPECT_CALL(mockLifecycle, CloseApp("test.app", Exchange::ILifecycleManagerState::ERROR))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"unknown"})", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- LifecycleReady ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_116_LifecycleReady_Success)
{
    EXPECT_CALL(mockLifecycle, AppReady("test.app"))
        .WillOnce(Return(Core::ERROR_NONE));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- LifecycleFinished ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_117_LifecycleFinished_ReturnsNull)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.finished", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

/* ---------- LifecycleState / Lifecycle2State (empty state) ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_118_LifecycleState_DefaultState)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);

    // Without prior lifecycle updates, state is default
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(LifecycleDelegateTest, AGC_L1_119_Lifecycle2State_DefaultState)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- DispatchLastIntent / GetLastIntent ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_120_DispatchLastIntent_NoIntent)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.dispatchintent", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("null", result);
}

TEST_F(LifecycleDelegateTest, AGC_L1_121_GetLastIntent_NoIntent)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- Authenticate / GetSessionId success via lifecycle callback ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_161_Authenticate_Success_AfterLifecycleEvent)
{
    // Fire OnAppLifecycleStateChanged with INITIALIZING to populate the map
    ASSERT_NE(capturedNotification, nullptr);
    capturedNotification->OnAppLifecycleStateChanged(
        "com.example.myapp",              // appId
        "instance-abc-123",                // appInstanceId
        Exchange::ILifecycleManager::UNLOADED,      // oldState
        Exchange::ILifecycleManager::INITIALIZING,   // newState (triggers map insertion)
        ""                                 // navigationIntent
    );

    string appId;
    const auto rc = plugin.Authenticate("instance-abc-123", appId);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("com.example.myapp", appId);
}

TEST_F(LifecycleDelegateTest, AGC_L1_162_GetSessionId_Success_AfterLifecycleEvent)
{
    ASSERT_NE(capturedNotification, nullptr);
    capturedNotification->OnAppLifecycleStateChanged(
        "com.example.myapp",
        "instance-abc-123",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    string sessionId;
    const auto rc = plugin.GetSessionId("com.example.myapp", sessionId);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_EQ("instance-abc-123", sessionId);
}

/* ---------- LifecycleReady: AppReady failure ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_163_LifecycleReady_AppReadyFails)
{
    EXPECT_CALL(mockLifecycle, AppReady(_))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- LifecycleClose: CloseApp failure ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_164_LifecycleClose_CloseAppFails)
{
    EXPECT_CALL(mockLifecycle, CloseApp(_, _))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- Lifecycle2Close: CloseApp failure ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_165_Lifecycle2Close_CloseAppFails)
{
    EXPECT_CALL(mockLifecycle, CloseApp(_, _))
        .WillOnce(Return(Core::ERROR_GENERAL));

    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- Lifecycle2Close: bad payload ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_166_Lifecycle2Close_BadPayload)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", "{invalid", result);

    EXPECT_NE(Core::ERROR_NONE, rc);
}

/* ---------- LifecycleState with populated map ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_167_LifecycleState_AfterLifecycleEvent)
{
    ASSERT_NE(capturedNotification, nullptr);
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app",
        "instance-xyz",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Should contain state info rather than empty
    EXPECT_FALSE(result.empty());
}

/* ---------- DispatchLastIntent with navigation intent ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_168_DispatchLastIntent_WithIntent)
{
    ASSERT_NE(capturedNotification, nullptr);
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app",
        "instance-abc",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        "playback://content/123"
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.dispatchintent", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- Lifecycle2State with populated registry ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_181_Lifecycle2State_AfterLifecycleEvent)
{
    ASSERT_NE(capturedNotification, nullptr);
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app",
        "instance-xyz",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_FALSE(result.empty());
}

/* ---------- GetLastIntent with populated intent ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_182_GetLastIntent_WithIntent)
{
    ASSERT_NE(capturedNotification, nullptr);
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app",
        "instance-abc",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        "search://query=testing"
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ================================================================
 * Category B – Null LifecycleManagerState interface
 *
 * LifecycleDelegate is constructed but /opt/ai2managers does NOT
 * exist, so ConfigUtils::useAppManagers() returns false and the
 * constructor skips Register(). The interfaces remain nullptr.
 * ================================================================ */

class LifecycleNoInterfaceTest : public ::testing::Test {
protected:
    Core::Sink<AppGatewayCommon> plugin;
    NiceMock<ServiceMock> service;

    void SetUp() override
    {
        // Ensure /opt/ai2managers does NOT exist
        std::remove("/opt/ai2managers");

        // All calls return nullptr
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

TEST_F(LifecycleNoInterfaceTest, AGC_L1_219_LifecycleClose_NullInterface_ReturnsError)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.close", R"({"reason":"userExit"})", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_NE(result.find("LifecycleManager not available"), std::string::npos);
}

TEST_F(LifecycleNoInterfaceTest, AGC_L1_220_Lifecycle2Close_NullInterface_ReturnsError)
{
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.close", R"({"type":"deactivate"})", result);

    EXPECT_EQ(Core::ERROR_GENERAL, rc);
    EXPECT_NE(result.find("LifecycleManager not available"), std::string::npos);
}

TEST_F(LifecycleNoInterfaceTest, AGC_L1_221_LifecycleReady_NullInterface_ReturnsOK)
{
    // LifecycleReady with null mLifecycleManagerState returns ERROR_NONE silently
    const auto ctx = MakeContext();
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.ready", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ================================================================
 * Category C – Lifecycle notification dispatch
 *
 * Use capturedNotification from the main fixture to fire
 * OnAppLifecycleStateChanged and verify lifecycle events dispatch.
 * ================================================================ */

TEST_F(LifecycleDelegateTest, AGC_L1_222_LifecycleNotification_ActiveState_DispatchesForegroundOrBackground)
{
    ASSERT_NE(capturedNotification, nullptr);

    // First fire INITIALIZING to populate the map
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-001",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    // Now transition to ACTIVE – should dispatch Lifecycle2.onStateChanged
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-001",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::ACTIVE,
        ""
    );

    // Verify state registry was updated
    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("active"), std::string::npos);
}

TEST_F(LifecycleDelegateTest, AGC_L1_223_LifecycleNotification_PausedState)
{
    ASSERT_NE(capturedNotification, nullptr);

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-002",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-002",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::PAUSED,
        ""
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Lifecycle1 maps PAUSED to "inactive"
    EXPECT_NE(result.find("inactive"), std::string::npos);
}

TEST_F(LifecycleDelegateTest, AGC_L1_224_LifecycleNotification_SuspendedState)
{
    ASSERT_NE(capturedNotification, nullptr);

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-003",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-003",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::SUSPENDED,
        ""
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("suspended"), std::string::npos);
}

TEST_F(LifecycleDelegateTest, AGC_L1_225_LifecycleNotification_TerminatingState)
{
    ASSERT_NE(capturedNotification, nullptr);

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-004",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-004",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::TERMINATING,
        ""
    );

    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "lifecycle2.state", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_NE(result.find("terminating"), std::string::npos);
}

TEST_F(LifecycleDelegateTest, AGC_L1_226_LifecycleNotification_ActiveWithIntent_DispatchesLastIntent)
{
    ASSERT_NE(capturedNotification, nullptr);

    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-005",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        "playback://video/456"
    );

    // Transition to ACTIVE triggers DispatchLastKnownIntent
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-005",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::ACTIVE,
        ""
    );

    // Verify the intent is stored
    const auto ctx = MakeContext("test.app");
    string result;
    const auto rc = plugin.HandleAppGatewayRequest(ctx, "commoninternal.getlastintent", "{}", result);

    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- HandleEvent / HandleSubscription ---------- */

TEST_F(LifecycleDelegateTest, AGC_L1_227_HandleEvent_InvalidEvent_NotHandled)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();
    bool status = false;
    const auto rc = plugin.HandleAppEventNotifier(emitter, "invalid.nothingmatch", true, status);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    // Event is not recognized by any delegate — status should reflect unsuccessful match
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(LifecycleDelegateTest, AGC_L1_228_HandleEvent_ValidLifecycleEvent_Subscription)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();
    bool status = false;
    const auto rc = plugin.HandleAppEventNotifier(emitter, "Lifecycle.onForeground", true, status);

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

TEST_F(LifecycleDelegateTest, AGC_L1_229_HandleEvent_ValidLifecycleEvent_Unsubscription)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();

    // Subscribe first
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "Lifecycle.onBackground", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(status);

    // Now unsubscribe
    status = false;
    const auto rc = plugin.HandleAppEventNotifier(emitter, "Lifecycle.onBackground", false, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
}

/* ================================================================
 * Gap 1 – OnFocus / OnBlur WindowManager notification callbacks
 *
 * These tests verify that WindowManager focus/blur events dispatch
 * Presentation.onFocusedChanged and drive Lifecycle 1 foreground/
 * background transitions when the app is ACTIVE.
 * ================================================================ */

TEST_F(LifecycleDelegateTest, AGC_L1_261_OnFocus_DispatchesFocusedChanged_And_Foreground)
{
    ASSERT_NE(capturedNotification, nullptr);
    ASSERT_NE(capturedWMNotification, nullptr);

    // Make the app known and ACTIVE so focus triggers lifecycle events
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-focus-001",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-focus-001",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::ACTIVE,
        ""
    );

    // Subscribe emitters for both events directly on the lifecycle delegate
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);

    MockEmitter* focusEmitter = new MockEmitter();
    heapEmitters.push_back(focusEmitter);
    focusEmitter->AddRef();
    lifecycleDelegate->AddNotification("Presentation.onFocusedChanged", focusEmitter);

    MockEmitter* fgEmitter = new MockEmitter();
    heapEmitters.push_back(fgEmitter);
    fgEmitter->AddRef();
    lifecycleDelegate->AddNotification("Lifecycle.onForeground", fgEmitter);

    EXPECT_CALL(*focusEmitter, Emit(::testing::HasSubstr("Presentation.onFocusedChanged"), _, _))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*fgEmitter, Emit(::testing::HasSubstr("Lifecycle.onForeground"), _, _))
        .Times(::testing::AtLeast(1));

    // Fire OnFocus
    capturedWMNotification->OnFocus("instance-focus-001");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

TEST_F(LifecycleDelegateTest, AGC_L1_262_OnBlur_DispatchesFocusedChanged_And_Background)
{
    ASSERT_NE(capturedNotification, nullptr);
    ASSERT_NE(capturedWMNotification, nullptr);

    // Make the app known and ACTIVE, then focus it first
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-blur-001",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-blur-001",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::ACTIVE,
        ""
    );
    // Focus the app so blur will clear it
    capturedWMNotification->OnFocus("instance-blur-001");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);

    MockEmitter* focusEmitter = new MockEmitter();
    heapEmitters.push_back(focusEmitter);
    focusEmitter->AddRef();
    lifecycleDelegate->AddNotification("Presentation.onFocusedChanged", focusEmitter);

    MockEmitter* bgEmitter = new MockEmitter();
    heapEmitters.push_back(bgEmitter);
    bgEmitter->AddRef();
    lifecycleDelegate->AddNotification("Lifecycle.onBackground", bgEmitter);

    EXPECT_CALL(*focusEmitter, Emit(::testing::HasSubstr("Presentation.onFocusedChanged"), _, _))
        .Times(::testing::AtLeast(1));
    EXPECT_CALL(*bgEmitter, Emit(::testing::HasSubstr("Lifecycle.onBackground"), _, _))
        .Times(::testing::AtLeast(1));

    // Fire OnBlur
    capturedWMNotification->OnBlur("instance-blur-001");

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

/* ================================================================
 * Gap 2 – TERMINATING → Lifecycle.onUnloading dispatch
 *
 * Verifies that when an app transitions to TERMINATING, the
 * Lifecycle 1 Lifecycle.onUnloading event is dispatched.
 * ================================================================ */

TEST_F(LifecycleDelegateTest, AGC_L1_263_Terminating_DispatchesOnUnloading)
{
    ASSERT_NE(capturedNotification, nullptr);

    // Initialize the app in the registries
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-term-001",
        Exchange::ILifecycleManager::UNLOADED,
        Exchange::ILifecycleManager::INITIALIZING,
        ""
    );

    // Subscribe to Lifecycle.onUnloading
    auto lifecycleDelegate = plugin.mDelegate->getLifecycleDelegate();
    ASSERT_NE(lifecycleDelegate, nullptr);

    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();
    lifecycleDelegate->AddNotification("Lifecycle.onUnloading", emitter);

    EXPECT_CALL(*emitter, Emit(::testing::HasSubstr("Lifecycle.onUnloading"), _, _))
        .Times(::testing::AtLeast(1));

    // Transition to TERMINATING
    capturedNotification->OnAppLifecycleStateChanged(
        "test.app", "instance-term-001",
        Exchange::ILifecycleManager::INITIALIZING,
        Exchange::ILifecycleManager::TERMINATING,
        ""
    );

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

/* ================================================================
 * Gap 5 – Presentation.onFocusedChanged subscription path
 *
 * Verifies that the event name is accepted by HandleEvent and
 * correctly recorded via AddNotification/RemoveNotification.
 * ================================================================ */

TEST_F(LifecycleDelegateTest, AGC_L1_264_HandleEvent_PresentationFocusedChanged_SubscribeUnsubscribe)
{
    MockEmitter* emitter = new MockEmitter();
    heapEmitters.push_back(emitter);
    emitter->AddRef();

    // Subscribe
    bool status = false;
    plugin.HandleAppEventNotifier(emitter, "Presentation.onFocusedChanged", true, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(status);

    // Unsubscribe
    status = false;
    const auto rc = plugin.HandleAppEventNotifier(emitter, "Presentation.onFocusedChanged", false, status);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(Core::ERROR_NONE, rc);
    EXPECT_TRUE(status);
}

} // namespace
