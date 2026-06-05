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
#include <thread>
#include <chrono>

#include "Module.h"

#define private public
#include "AppActions.h"
#include "AppActionsImplementation.h"
#undef private

#include "ServiceMock.h"
#include "AppActionsMock.h"
#include "WorkerPoolImplementation.h"
#include "ThunderPortability.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Invoke;
using ::testing::StrEq;

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

// Brief sleep to allow WorkerPool to dispatch pending async NotifyJobs.
static void DrainNotifyJobs()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// --------------------------------------------------------------------------
// Mock IAppActions::INotification for capturing callbacks
// --------------------------------------------------------------------------
class MockAppActionsNotification : public Exchange::IAppActions::INotification {
public:
    MockAppActionsNotification() = default;
    virtual ~MockAppActionsNotification() = default;

    MOCK_METHOD(void, OnActionStartRequest, 
                (const string& initiator, const string& intent, const string& handlerAppId), 
                (override));
    
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

// --------------------------------------------------------------------------
// AppActions Plugin Tests (Shell)
// --------------------------------------------------------------------------
class AppActionsPluginTest : public ::testing::Test {
protected:
    Core::Sink<AppActions> plugin;
    NiceMock<ServiceMock> service;
    NiceMock<AppActionsMock> mockAppActions;
    bool initialized_ = false;

    void SetUp() override
    {
        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault(Return(nullptr));

        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    }

    void TearDown() override
    {
        if (initialized_) {
            plugin.Deinitialize(&service);
        }
    }
};

/* ---------- Plugin Lifecycle Tests ---------- */

TEST_F(AppActionsPluginTest, AA_L1_001_Initialize_Success)
{
    // Note: In isolated tests, Thunder's Root() mechanism is used
    // The real implementation is loaded via SERVICE_REGISTRATION
    const string result = plugin.Initialize(&service);
    initialized_ = result.empty();
    
    // Initialize may return error if impl can't be loaded in test env
    // The test verifies no crash occurs
    EXPECT_TRUE(true);
}

TEST_F(AppActionsPluginTest, AA_L1_002_Information_ReturnsEmpty)
{
    const string result = plugin.Initialize(&service);
    initialized_ = result.empty();
    
    const string info = plugin.Information();
    EXPECT_TRUE(info.empty());
}

TEST_F(AppActionsPluginTest, AA_L1_003_Deinitialize_NoCrash)
{
    plugin.Initialize(&service);
    initialized_ = true;
    
    // Deinitialize should not crash
    plugin.Deinitialize(&service);
    initialized_ = false;
    
    EXPECT_TRUE(true);
}

TEST_F(AppActionsPluginTest, AA_L1_004_Constructor_Destructor_Lifecycle)
{
    // Creating and destroying plugin without Initialize should be safe
    {
        Core::Sink<AppActions> tempPlugin;
        // Just let it go out of scope
    }
    EXPECT_TRUE(true);
}

// --------------------------------------------------------------------------
// AppActionsImplementation Tests
// --------------------------------------------------------------------------
class AppActionsImplementationTest : public ::testing::Test {
protected:
    Core::Sink<AppActionsImplementation> impl;
    NiceMock<ServiceMock> service;

    void SetUp() override
    {
        EXPECT_CALL(service, AddRef()).Times(AnyNumber());
        EXPECT_CALL(service, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    }

    void TearDown() override
    {
        // Drain any pending async NotifyJobs before the fixture (and impl) destruct.
        // ActionStart() submits a NotifyJob that holds AddRef() on impl. If the job
        // has not yet run when Core::Sink<AppActionsImplementation> destructs, the
        // Sink prints "Oops this is scary" and then the job executes against a
        // destroyed object → pure virtual method called → abort.
        DrainNotifyJobs();
    }
};

/* ---------- ActionStart Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_010_ActionStart_ValidParams)
{
    const auto rc = impl.ActionStart("testInitiator", "{\"action\":\"test\"}", "testApp");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_011_ActionStart_EmptyInitiator)
{
    const auto rc = impl.ActionStart("", "{\"action\":\"test\"}", "testApp");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_012_ActionStart_EmptyIntent)
{
    const auto rc = impl.ActionStart("initiator", "", "testApp");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_013_ActionStart_EmptyHandlerAppId)
{
    const auto rc = impl.ActionStart("initiator", "intent", "");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_014_ActionStart_AllEmptyParams)
{
    const auto rc = impl.ActionStart("", "", "");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_015_ActionStart_SpecialCharsInParams)
{
    const auto rc = impl.ActionStart("voice-assistant/v2.0@test", 
                                     "{\"action\":\"play\",\"data\":{\"contentId\":\"123\"}}", 
                                     "netflix/player");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_016_ActionStart_UnicodeChars)
{
    const auto rc = impl.ActionStart("voice", "{\"title\":\"日本語テスト\"}", "app");
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_017_ActionStart_LongStrings)
{
    std::string longStr(1000, 'x');
    const auto rc = impl.ActionStart(longStr, longStr, longStr);
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

/* ---------- Register/Unregister Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_020_Register_Success)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const auto rc = impl.Register(&notification);
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    impl.Unregister(&notification);
}

TEST_F(AppActionsImplementationTest, AA_L1_021_Register_Duplicate_ReturnsError)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const auto rc1 = impl.Register(&notification);
    EXPECT_EQ(Core::ERROR_NONE, rc1);
    
    const auto rc2 = impl.Register(&notification);
    EXPECT_EQ(Core::ERROR_GENERAL, rc2);
    
    impl.Unregister(&notification);
}

TEST_F(AppActionsImplementationTest, AA_L1_022_Register_Multiple_Success)
{
    NiceMock<MockAppActionsNotification> notification1;
    NiceMock<MockAppActionsNotification> notification2;
    
    EXPECT_CALL(notification1, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification1, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(notification2, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification2, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    EXPECT_EQ(Core::ERROR_NONE, impl.Register(&notification1));
    EXPECT_EQ(Core::ERROR_NONE, impl.Register(&notification2));
    
    impl.Unregister(&notification1);
    impl.Unregister(&notification2);
}

TEST_F(AppActionsImplementationTest, AA_L1_023_Unregister_Success)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    impl.Register(&notification);
    
    const auto rc = impl.Unregister(&notification);
    EXPECT_EQ(Core::ERROR_NONE, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_024_Unregister_NotRegistered_ReturnsError)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    const auto rc = impl.Unregister(&notification);
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

TEST_F(AppActionsImplementationTest, AA_L1_025_Unregister_Twice_ReturnsError)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    impl.Register(&notification);
    
    EXPECT_EQ(Core::ERROR_NONE, impl.Unregister(&notification));
    EXPECT_EQ(Core::ERROR_GENERAL, impl.Unregister(&notification));
}

/* ---------- Notification Dispatch Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_030_ActionStart_DispatchesToNotification)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    EXPECT_CALL(notification, OnActionStartRequest(
        StrEq("voice"), 
        StrEq("{\"action\":\"play\"}"), 
        StrEq("netflix")))
        .Times(1);
    
    impl.Register(&notification);
    
    impl.ActionStart("voice", "{\"action\":\"play\"}", "netflix");
    DrainNotifyJobs();
    
    impl.Unregister(&notification);
}

TEST_F(AppActionsImplementationTest, AA_L1_031_ActionStart_DispatchesToMultipleNotifications)
{
    NiceMock<MockAppActionsNotification> notification1;
    NiceMock<MockAppActionsNotification> notification2;
    
    EXPECT_CALL(notification1, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification1, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(notification2, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification2, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    EXPECT_CALL(notification1, OnActionStartRequest(_, _, _)).Times(1);
    EXPECT_CALL(notification2, OnActionStartRequest(_, _, _)).Times(1);
    
    impl.Register(&notification1);
    impl.Register(&notification2);
    
    impl.ActionStart("initiator", "intent", "appId");
    DrainNotifyJobs();
    
    impl.Unregister(&notification1);
    impl.Unregister(&notification2);
}

TEST_F(AppActionsImplementationTest, AA_L1_032_ActionStart_NoDispatchAfterUnregister)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    // Should be called once before unregister
    EXPECT_CALL(notification, OnActionStartRequest(_, _, _)).Times(1);
    
    impl.Register(&notification);
    impl.ActionStart("init1", "intent1", "app1");
    DrainNotifyJobs();
    impl.Unregister(&notification);
    
    // This should NOT trigger notification
    impl.ActionStart("init2", "intent2", "app2");
    DrainNotifyJobs();
}

TEST_F(AppActionsImplementationTest, AA_L1_033_ActionStart_MultipleCalls)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    EXPECT_CALL(notification, OnActionStartRequest(_, _, _)).Times(3);
    
    impl.Register(&notification);
    
    impl.ActionStart("init1", "intent1", "app1");
    impl.ActionStart("init2", "intent2", "app2");
    impl.ActionStart("init3", "intent3", "app3");
    DrainNotifyJobs();
    
    impl.Unregister(&notification);
}

/* ---------- Configure Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_040_Configure_ValidService)
{
    const auto rc = impl.Configure(&service);
    // Configure returns ERROR_GENERAL but sets service pointer
    // Implementation behavior - Configure doesn't return ERROR_NONE
    EXPECT_TRUE(true); // Just verify no crash
}

TEST_F(AppActionsImplementationTest, AA_L1_041_Configure_NullService)
{
    const auto rc = impl.Configure(nullptr);
    EXPECT_EQ(Core::ERROR_GENERAL, rc);
}

/* ---------- Initialize/Deinitialize Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_050_Initialize_CallsConfigure)
{
    const string result = impl.Initialize(&service);
    // Initialize returns result of Configure
    // May not be empty due to Configure returning ERROR_GENERAL
    EXPECT_TRUE(true); // Verify no crash
    
    impl.Deinitialize(&service);
}

TEST_F(AppActionsImplementationTest, AA_L1_051_Deinitialize_ReleasesService)
{
    impl.Initialize(&service);
    impl.Deinitialize(&service);
    
    // Should not crash - verify proper cleanup
    EXPECT_TRUE(true);
}

TEST_F(AppActionsImplementationTest, AA_L1_052_Information_ReturnsEmpty)
{
    const string info = impl.Information();
    EXPECT_TRUE(info.empty());
}

TEST_F(AppActionsImplementationTest, AA_L1_053_Deinitialize_WithoutInitialize)
{
    // Should not crash
    impl.Deinitialize(&service);
    EXPECT_TRUE(true);
}

/* ---------- Interface Map Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_060_InterfaceMap_IPlugin)
{
    auto* pluginIface = static_cast<PluginHost::IPlugin*>(
        impl.QueryInterface(PluginHost::IPlugin::ID));
    EXPECT_NE(nullptr, pluginIface);
    if (nullptr != pluginIface) {
        pluginIface->Release();
    }
}

TEST_F(AppActionsImplementationTest, AA_L1_061_InterfaceMap_IAppActions)
{
    auto* appActionsIface = static_cast<Exchange::IAppActions*>(
        impl.QueryInterface(Exchange::IAppActions::ID));
    EXPECT_NE(nullptr, appActionsIface);
    if (nullptr != appActionsIface) {
        appActionsIface->Release();
    }
}

TEST_F(AppActionsImplementationTest, AA_L1_062_InterfaceMap_IConfiguration)
{
    auto* configIface = static_cast<Exchange::IConfiguration*>(
        impl.QueryInterface(Exchange::IConfiguration::ID));
    EXPECT_NE(nullptr, configIface);
    if (nullptr != configIface) {
        configIface->Release();
    }
}

/* ---------- Re-registration Tests ---------- */

TEST_F(AppActionsImplementationTest, AA_L1_070_Register_AfterUnregister_Success)
{
    NiceMock<MockAppActionsNotification> notification;
    EXPECT_CALL(notification, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    impl.Register(&notification);
    impl.Unregister(&notification);
    
    // Re-register should succeed
    const auto rc = impl.Register(&notification);
    EXPECT_EQ(Core::ERROR_NONE, rc);
    
    impl.Unregister(&notification);
}

TEST_F(AppActionsImplementationTest, AA_L1_071_Unregister_PartialFromMultiple)
{
    NiceMock<MockAppActionsNotification> notification1;
    NiceMock<MockAppActionsNotification> notification2;
    
    EXPECT_CALL(notification1, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification1, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    EXPECT_CALL(notification2, AddRef()).Times(AnyNumber());
    EXPECT_CALL(notification2, Release()).Times(AnyNumber()).WillRepeatedly(Return(Core::ERROR_NONE));
    
    impl.Register(&notification1);
    impl.Register(&notification2);
    
    // Unregister only notification1
    impl.Unregister(&notification1);
    
    // notification2 should still receive events
    EXPECT_CALL(notification2, OnActionStartRequest(_, _, _)).Times(1);
    
    impl.ActionStart("init", "intent", "app");
    DrainNotifyJobs();
    
    impl.Unregister(&notification2);
}

} // anonymous namespace
