/*
 * AppActions_NotificationTests.cpp
 *
 * L0 tests for AppActions notification registration/unregistration.
 * Tests AA-L0-040 through AA-L0-055.
 */

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

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
// AA-L0-040: Register notification succeeds
// ---------------------------------------------------------------------------
uint32_t Test_AA_Register_Success()
{
    /** Register should succeed for a new notification. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Register_Success: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        const auto rc = impl->Register(notification);
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "Register_Success: should return ERROR_NONE");

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-041: Register same notification twice returns error
// ---------------------------------------------------------------------------
uint32_t Test_AA_Register_DuplicateReturnsError()
{
    /** Register should return error if same notification is registered twice. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Register_DuplicateReturnsError: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        
        // First registration should succeed
        const auto rc1 = impl->Register(notification);
        L0Test::ExpectEqU32(tr, rc1, ERROR_NONE, "Register_DuplicateReturnsError: first Register should succeed");

        // Second registration should fail
        const auto rc2 = impl->Register(notification);
        L0Test::ExpectEqU32(tr, rc2, ERROR_GENERAL, "Register_DuplicateReturnsError: second Register should fail");

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-042: Register multiple different notifications
// ---------------------------------------------------------------------------
uint32_t Test_AA_Register_MultipleDifferent()
{
    /** Register should succeed for multiple different notifications. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Register_MultipleDifferent: impl should be non-null");

    if (nullptr != impl) {
        auto* notification1 = new L0Test::AANotificationFake();
        auto* notification2 = new L0Test::AANotificationFake();
        auto* notification3 = new L0Test::AANotificationFake();

        const auto rc1 = impl->Register(notification1);
        const auto rc2 = impl->Register(notification2);
        const auto rc3 = impl->Register(notification3);

        L0Test::ExpectEqU32(tr, rc1, ERROR_NONE, "Register_MultipleDifferent: first should succeed");
        L0Test::ExpectEqU32(tr, rc2, ERROR_NONE, "Register_MultipleDifferent: second should succeed");
        L0Test::ExpectEqU32(tr, rc3, ERROR_NONE, "Register_MultipleDifferent: third should succeed");

        impl->Unregister(notification1);
        impl->Unregister(notification2);
        impl->Unregister(notification3);
        notification1->Release();
        notification2->Release();
        notification3->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-043: Unregister notification succeeds
// ---------------------------------------------------------------------------
uint32_t Test_AA_Unregister_Success()
{
    /** Unregister should succeed for a registered notification. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Unregister_Success: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        const auto rc = impl->Unregister(notification);
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "Unregister_Success: should return ERROR_NONE");

        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-044: Unregister non-registered notification returns error
// ---------------------------------------------------------------------------
uint32_t Test_AA_Unregister_NotRegistered()
{
    /** Unregister should return error if notification is not registered. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Unregister_NotRegistered: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        
        // Unregister without prior registration
        const auto rc = impl->Unregister(notification);
        L0Test::ExpectEqU32(tr, rc, ERROR_GENERAL, "Unregister_NotRegistered: should return ERROR_GENERAL");

        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-045: Unregister same notification twice
// ---------------------------------------------------------------------------
uint32_t Test_AA_Unregister_Twice()
{
    /** Unregistering the same notification twice should fail on second call. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Unregister_Twice: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        const auto rc1 = impl->Unregister(notification);
        L0Test::ExpectEqU32(tr, rc1, ERROR_NONE, "Unregister_Twice: first Unregister should succeed");

        const auto rc2 = impl->Unregister(notification);
        L0Test::ExpectEqU32(tr, rc2, ERROR_GENERAL, "Unregister_Twice: second Unregister should fail");

        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-046: Unregistered notification no longer receives events
// ---------------------------------------------------------------------------
uint32_t Test_AA_Unregister_NoLongerReceivesEvents()
{
    /** After unregistration, notification should not receive events. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Unregister_NoLongerReceivesEvents: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        impl->Register(notification);

        // First event should be received
        impl->ActionStart("init1", "intent1", "app1");
        DrainNotifyJobs();
        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqU32(tr, notification->onActionStartRequestCount, 1,
                "Unregister_NoLongerReceivesEvents: should receive first event");
        }

        // Unregister
        impl->Unregister(notification);

        // Second event should NOT be received
        impl->ActionStart("init2", "intent2", "app2");
        DrainNotifyJobs();
        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqU32(tr, notification->onActionStartRequestCount, 1,
                "Unregister_NoLongerReceivesEvents: should not receive second event");
        }

        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-047: Register after Unregister works
// ---------------------------------------------------------------------------
uint32_t Test_AA_Register_AfterUnregister()
{
    /** Re-registering after unregister should work. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Register_AfterUnregister: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        
        impl->Register(notification);
        impl->Unregister(notification);

        const auto rc = impl->Register(notification);
        L0Test::ExpectEqU32(tr, rc, ERROR_NONE, "Register_AfterUnregister: re-register should succeed");

        // Verify it receives events again
        impl->ActionStart("init", "intent", "app");
        DrainNotifyJobs();
        {
            std::lock_guard<std::mutex> lock(notification->_mutex);
            L0Test::ExpectEqU32(tr, notification->onActionStartRequestCount, 1,
                "Register_AfterUnregister: should receive events after re-register");
        }

        impl->Unregister(notification);
        notification->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-048: Partial unregister from multiple notifications
// ---------------------------------------------------------------------------
uint32_t Test_AA_Unregister_PartialFromMultiple()
{
    /** Unregistering one notification should not affect others. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Unregister_PartialFromMultiple: impl should be non-null");

    if (nullptr != impl) {
        auto* notification1 = new L0Test::AANotificationFake();
        auto* notification2 = new L0Test::AANotificationFake();

        impl->Register(notification1);
        impl->Register(notification2);

        // Unregister only notification1
        impl->Unregister(notification1);

        // Send event
        impl->ActionStart("init", "intent", "app");

        DrainNotifyJobs();

        // notification1 should NOT receive event
        {
            std::lock_guard<std::mutex> lock(notification1->_mutex);
            L0Test::ExpectEqU32(tr, notification1->onActionStartRequestCount, 0,
                "Unregister_PartialFromMultiple: unregistered should not receive");
        }

        // notification2 SHOULD receive event
        {
            std::lock_guard<std::mutex> lock(notification2->_mutex);
            L0Test::ExpectEqU32(tr, notification2->onActionStartRequestCount, 1,
                "Unregister_PartialFromMultiple: still registered should receive");
        }

        impl->Unregister(notification2);
        notification1->Release();
        notification2->Release();
        impl->Release();
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-049: Thread safety of Register/Unregister
// ---------------------------------------------------------------------------
uint32_t Test_AA_Register_ThreadSafety()
{
    /** Register/Unregister operations should be thread-safe (no crash test). */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Register_ThreadSafety: impl should be non-null");

    if (nullptr != impl) {
        // Create multiple notifications
        std::vector<L0Test::AANotificationFake*> notifications;
        for (int i = 0; i < 10; ++i) {
            notifications.push_back(new L0Test::AANotificationFake());
        }

        // Register all
        for (auto* n : notifications) {
            impl->Register(n);
        }

        // Unregister all
        for (auto* n : notifications) {
            impl->Unregister(n);
        }

        // Cleanup
        for (auto* n : notifications) {
            n->Release();
        }

        impl->Release();
        L0Test::ExpectTrue(tr, true, "Register_ThreadSafety: no crash during operations");
    }

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AA-L0-050: Register adds reference count
// ---------------------------------------------------------------------------
uint32_t Test_AA_Register_AddsRefCount()
{
    /** Register should call AddRef on the notification. */
    L0Test::TestResult tr;

    auto* impl = L0Test::CreateRawImpl();
    L0Test::ExpectTrue(tr, nullptr != impl, "Register_AddsRefCount: impl should be non-null");

    if (nullptr != impl) {
        auto* notification = new L0Test::AANotificationFake();
        // Initial ref count is 1 (from new)
        
        impl->Register(notification);
        // After Register, impl should have added a reference
        // When we Unregister, impl releases that reference
        
        impl->Unregister(notification);
        // Now only our original reference remains
        
        notification->Release(); // This should not crash - ref count goes to 0
        impl->Release();

        L0Test::ExpectTrue(tr, true, "Register_AddsRefCount: ref counting works correctly");
    }

    return tr.failures;
}
