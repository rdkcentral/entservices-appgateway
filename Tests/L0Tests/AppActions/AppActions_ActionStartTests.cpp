/*
 * AppActions_ActionStartTests.cpp
 *
 * L0 tests for AppActions ActionStart method.
 * Tests AA-L0-020 through AA-L0-032.
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <core/core.h>
#include <plugins/IShell.h>

#include <AppActions.h>
#include <AppActionsImplementation.h>
#include "AppActionsServiceMock.h"
#include "AppActionsTestHelpers.h"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_GENERAL;
using WPEFramework::Plugin::AppActionsImplementation;

namespace {
// Brief sleep to allow WorkerPool to dispatch pending async NotifyJobs.
static void DrainNotifyJobs()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}
} // namespace

// ---------------------------------------------------------------------------
// AA-L0-020: ActionStart with valid parameters succeeds
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_ValidParams()
{
    /** ActionStart should return ERROR_NONE with valid parameters. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_ValidParams: impl should be non-null");

    if (nullptr != impl) {
        const auto rc = impl->ActionStart("testInitiator", "{\"action\":\"test\"}", "testApp");
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_ValidParams: should return ERROR_NONE");
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-021: ActionStart with empty initiator
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_EmptyInitiator()
{
    /** ActionStart should handle empty initiator string. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_EmptyInitiator: impl should be non-null");

    if (nullptr != impl) {
        const auto rc = impl->ActionStart("", "{\"action\":\"test\"}", "testApp");
        // Should still succeed - empty initiator is valid
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_EmptyInitiator: should return ERROR_NONE");
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-022: ActionStart with empty intent
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_EmptyIntent()
{
    /** ActionStart should handle empty intent string. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_EmptyIntent: impl should be non-null");

    if (nullptr != impl) {
        const auto rc = impl->ActionStart("testInitiator", "", "testApp");
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_EmptyIntent: should return ERROR_NONE");
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-023: ActionStart with empty handlerAppId
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_EmptyHandlerAppId()
{
    /** ActionStart should handle empty handlerAppId string. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_EmptyHandlerAppId: impl should be non-null");

    if (nullptr != impl) {
        const auto rc = impl->ActionStart("testInitiator", "{\"action\":\"test\"}", "");
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_EmptyHandlerAppId: should return ERROR_NONE");
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-024: ActionStart with all empty parameters
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_AllEmptyParams()
{
    /** ActionStart should handle all empty parameters without crashing. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_AllEmptyParams: impl should be non-null");

    if (nullptr != impl) {
        const auto rc = impl->ActionStart("", "", "");
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_AllEmptyParams: should return ERROR_NONE");
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-025: ActionStart dispatches to registered notifications
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_DispatchesToNotification()
{
    /** ActionStart should dispatch OnActionStartRequest to registered notifications. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_DispatchesToNotification: impl should be non-null");

    if (nullptr != impl) {
        // Create and register a notification
        auto* notification = new L0Test::AANotificationFake();
        const auto regRc = impl->Register(notification);
        L0Test::ExpectEqU32(tr, regRc, ERROR_NONE, "ActionStart_DispatchesToNotification: Register should succeed");

        // Call ActionStart
        const std::string initiator = "voice";
        const std::string intent = "{\"action\":\"play\",\"data\":{\"contentId\":\"123\"}}";
        const std::string handlerAppId = "netflix";
        
        const auto rc = impl->ActionStart(initiator, intent, handlerAppId);
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_DispatchesToNotification: ActionStart should succeed");

        DrainNotifyJobs();

        // Verify notification was called
        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqU32(tr, notification->onActionStartRequestCount, 1,
                "ActionStart_DispatchesToNotification: notification should be called once");
            L0Test::ExpectEqStr(tr, notification->lastInitiator, initiator,
                "ActionStart_DispatchesToNotification: initiator should match");
            L0Test::ExpectEqStr(tr, notification->lastIntent, intent,
                "ActionStart_DispatchesToNotification: intent should match");
            L0Test::ExpectEqStr(tr, notification->lastHandlerAppId, handlerAppId,
                "ActionStart_DispatchesToNotification: handlerAppId should match");
        }

        // Cleanup
        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-026: ActionStart with multiple registered notifications
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_MultipleNotifications()
{
    /** ActionStart should dispatch to all registered notifications. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_MultipleNotifications: impl should be non-null");

    if (nullptr != impl) {
        // Create and register two notifications
        auto* notification1 = new L0Test::AANotificationFake();
        auto* notification2 = new L0Test::AANotificationFake();
        
        impl->Register(notification1);
        impl->Register(notification2);

        // Call ActionStart
        impl->ActionStart("initiator", "intent", "appId");

        DrainNotifyJobs();

        // Verify both notifications were called
        {
            std::lock_guard<std::mutex> lock1(notification1->_mutex);
            L0Test::ExpectEqU32(tr, notification1->onActionStartRequestCount, 1,
                "ActionStart_MultipleNotifications: notification1 should be called");
        }
        {
            std::lock_guard<std::mutex> lock2(notification2->_mutex);
            L0Test::ExpectEqU32(tr, notification2->onActionStartRequestCount, 1,
                "ActionStart_MultipleNotifications: notification2 should be called");
        }

        // Cleanup
        impl->Unregister(notification1);
        impl->Unregister(notification2);
        notification1->Release();
        notification2->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-027: ActionStart with no registered notifications
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_NoNotifications()
{
    /** ActionStart should succeed even with no registered notifications. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_NoNotifications: impl should be non-null");

    if (nullptr != impl) {
        const auto rc = impl->ActionStart("initiator", "intent", "appId");
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "ActionStart_NoNotifications: should return ERROR_NONE");
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-028: ActionStart with special characters in initiator
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_SpecialCharsInitiator()
{
    /** ActionStart should handle special characters in initiator. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_SpecialCharsInitiator: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        const std::string specialInitiator = "voice-assistant/v2.0@test";
        impl->ActionStart(specialInitiator, "intent", "appId");

        DrainNotifyJobs();

        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqStr(tr, notification->lastInitiator, specialInitiator,
                "ActionStart_SpecialCharsInitiator: special chars should be preserved");
        }

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-029: ActionStart with JSON intent
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_JsonIntent()
{
    /** ActionStart should properly pass JSON intent. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_JsonIntent: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        const std::string jsonIntent = "{\"action\":\"play\",\"data\":{\"contentId\":\"movie123\",\"position\":0}}";
        impl->ActionStart("voice", jsonIntent, "player");

        DrainNotifyJobs();

        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqStr(tr, notification->lastIntent, jsonIntent,
                "ActionStart_JsonIntent: JSON intent should be preserved");
        }

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-030: ActionStart with long strings
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_LongStrings()
{
    /** ActionStart should handle long strings without truncation. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_LongStrings: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        // Create long strings (1000 characters each)
        std::string longInitiator(1000, 'a');
        std::string longIntent(1000, 'b');
        std::string longHandlerAppId(1000, 'c');

        impl->ActionStart(longInitiator, longIntent, longHandlerAppId);

        DrainNotifyJobs();

        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqU32(tr, notification->lastInitiator.length(), 1000,
                "ActionStart_LongStrings: initiator length should be preserved");
            L0Test::ExpectEqU32(tr, notification->lastIntent.length(), 1000,
                "ActionStart_LongStrings: intent length should be preserved");
            L0Test::ExpectEqU32(tr, notification->lastHandlerAppId.length(), 1000,
                "ActionStart_LongStrings: handlerAppId length should be preserved");
        }

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-031: ActionStart with Unicode characters
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_UnicodeChars()
{
    /** ActionStart should handle Unicode characters. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_UnicodeChars: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        const std::string unicodeIntent = "{\"title\":\"日本語テスト\"}";
        impl->ActionStart("voice", unicodeIntent, "app");

        DrainNotifyJobs();

        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqStr(tr, notification->lastIntent, unicodeIntent,
                "ActionStart_UnicodeChars: Unicode should be preserved");
        }

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-032: ActionStart multiple calls in sequence
// ---------------------------------------------------------------------------
uint32_t Test_AA_ActionStart_MultipleCalls()
{
    /** ActionStart should work correctly when called multiple times. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "ActionStart_MultipleCalls: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        // Make multiple calls
        impl->ActionStart("init1", "intent1", "app1");
        impl->ActionStart("init2", "intent2", "app2");
        impl->ActionStart("init3", "intent3", "app3");

        DrainNotifyJobs();

        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqU32(tr, notification->onActionStartRequestCount, 3,
                "ActionStart_MultipleCalls: should be called 3 times");
            // All three initiators must have been received (order is not guaranteed
            // with a multi-threaded WorkerPool, so check membership rather than ordering)
            L0Test::ExpectTrue(tr, notification->seenInitiators.count("init1") == 1,
                "ActionStart_MultipleCalls: init1 should be received");
            L0Test::ExpectTrue(tr, notification->seenInitiators.count("init2") == 1,
                "ActionStart_MultipleCalls: init2 should be received");
            L0Test::ExpectTrue(tr, notification->seenInitiators.count("init3") == 1,
                "ActionStart_MultipleCalls: init3 should be received");
        }

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}
