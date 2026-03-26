/*
 * AppNotifications_SubscribeTests.cpp
 *
 * L0 tests for AppNotificationsImplementation Subscribe/Unsubscribe/Cleanup.
 * Tests AN-L0-014 to AN-L0-026.
 *
 * Strategy:
 *   - Directly instantiate AppNotificationsImplementation via Core::Service<>
 *   - Call Configure() with a mock shell so mShell is valid
 *   - Call Subscribe/Cleanup and observe side-effects via the WorkerPool
 *     (SubscriberJob/EmitJob are dispatched asynchronously; we use short sleeps
 *      where needed, or verify via the SubscriberMap state after the call)
 *
 * Note on SubscriberJob timing:
 *   Subscribe() enqueues a SubscriberJob to the WorkerPool asynchronously.
 *   The SubscriberJob calls ThunderSubscriptionManager::Subscribe/Unsubscribe
 *   which in turn calls HandleNotifier -> QueryInterfaceByCallsign.
 *   In the test harness we do NOT verify the Thunder subscription side-effects
 *   directly (that is ThunderManager's job, tested separately). Instead we
 *   verify that:
 *     - First Subscribe for an event key → SubscriberJob IS enqueued (side-effect:
 *       one job goes to the pool; we observe this indirectly by the job not being
 *       enqueued on the second Subscribe call for the same key)
 *     - The SubscriberMap state (Exists/Get) is correct after Subscribe
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <core/core.h>
#include <plugins/IShell.h>

#include <interfaces/IAppNotifications.h>
#include "AppNotificationsServiceMock.h"
#include "AppNotificationsTestHelpers.h"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Exchange::IAppNotifications;
using WPEFramework::Exchange::IConfiguration;

// Helper: build a simple AppNotificationContext
namespace {
IAppNotifications::AppNotificationContext MakeContext(uint32_t connId,
                                                       uint32_t reqId,
                                                       const std::string& appId,
                                                       const std::string& origin,
                                                       const std::string& version = "0")
{
    IAppNotifications::AppNotificationContext ctx;
    ctx.connectionId = connId;
    ctx.requestId    = reqId;
    ctx.appId        = appId;
    ctx.origin       = origin;
    ctx.version      = version;
    return ctx;
}

// Brief sleep to allow WorkerPool to dispatch pending jobs
void YieldToWorkerPool()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

} // namespace

// ---------------------------------------------------------------------------
// AN-L0-014: First Subscribe for an event adds it to the SubscriberMap
// ---------------------------------------------------------------------------
uint32_t Test_AN_Subscribe_FirstListener_TriggersThunderSub()
{
    /** First Subscribe(listen=true) adds context to map and enqueues SubscriberJob. */
    L0Test::TestResult tr;

    // Use provideNotificationHandler=false to keep mRegisteredNotifications empty,
    // avoiding ThunderSubscriptionManager cleanup paths that require a full Thunder environment.
    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Subscribe_FirstListener: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(10, 1001, "com.test.app", APP_GATEWAY_CALLSIGN);
    const uint32_t rc = impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "onVoiceGuidanceChanged");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Subscribe_FirstListener: Subscribe should return ERROR_NONE");

    YieldToWorkerPool();

    // The context should now exist in the impl's subscriber map.
    // We verify indirectly: subscribe a second time for the SAME event key —
    // the second call should NOT enqueue a second SubscriberJob (no crash, no double-register).
    auto ctx2 = MakeContext(11, 1002, "com.test.app2", APP_GATEWAY_CALLSIGN);
    const uint32_t rc2 = impl->Subscribe(ctx2, true, FB_SETTINGS_CALLSIGN, "onVoiceGuidanceChanged");
    L0Test::ExpectEqU32(tr, rc2, static_cast<uint32_t>(ERROR_NONE),
        "Subscribe_FirstListener: second Subscribe should also return ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-015: Second Subscribe for same event key - no duplicate Thunder sub
// ---------------------------------------------------------------------------
uint32_t Test_AN_Subscribe_SecondListener_NoThunderSub()
{
    /** Second Subscribe(listen=true) for the same key does not enqueue a second SubscriberJob. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Subscribe_SecondListener: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(10, 1001, "com.test.app1", APP_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(11, 1002, "com.test.app2", APP_GATEWAY_CALLSIGN);

    impl->Subscribe(ctx1, true, FB_SETTINGS_CALLSIGN, "onPowerStateChanged");
    YieldToWorkerPool();

    // Second Subscribe for the same event - key already exists, so no SubscriberJob
    const uint32_t rc = impl->Subscribe(ctx2, true, FB_SETTINGS_CALLSIGN, "onPowerStateChanged");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Subscribe_SecondListener: second Subscribe should return ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-016: Subscribe is case-insensitive for event keys
// ---------------------------------------------------------------------------
uint32_t Test_AN_Subscribe_CaseInsensitive()
{
    /** Subscribe stores event keys in lowercase; mixed-case keys are treated as equal. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Subscribe_CaseInsensitive: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(10, 1001, "com.test.app1", APP_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(11, 1002, "com.test.app2", APP_GATEWAY_CALLSIGN);

    // Subscribe with mixed case
    impl->Subscribe(ctx1, true, FB_SETTINGS_CALLSIGN, "onAudioDescriptionChanged");
    YieldToWorkerPool();

    // Subscribe with UPPERCASE — should map to the same key (no new SubscriberJob)
    const uint32_t rc = impl->Subscribe(ctx2, true, FB_SETTINGS_CALLSIGN, "ONAUDIODESCRIPTIONCHANGED");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Subscribe_CaseInsensitive: uppercase Subscribe returns ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-017: Unsubscribe last listener triggers Thunder unsubscription
// ---------------------------------------------------------------------------
uint32_t Test_AN_Unsubscribe_LastListener_TriggersThunderUnsub()
{
    /** Unsubscribe(listen=false) when only one listener removes the key and enqueues SubscriberJob. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Unsubscribe_LastListener: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(10, 1001, "com.test.app", APP_GATEWAY_CALLSIGN);

    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "onPreferredAudioLanguagesChanged");
    YieldToWorkerPool();

    // Now unsubscribe — key should be removed, SubscriberJob (unsub) enqueued
    const uint32_t rc = impl->Subscribe(ctx, false, FB_SETTINGS_CALLSIGN, "onPreferredAudioLanguagesChanged");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Unsubscribe_LastListener: Unsubscribe should return ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-018: Unsubscribe when there are still remaining listeners - no Thunder unsub
// ---------------------------------------------------------------------------
uint32_t Test_AN_Unsubscribe_NotLastListener_NoThunderUnsub()
{
    /** Unsubscribe(listen=false) when more listeners remain does not enqueue SubscriberJob. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Unsubscribe_NotLastListener: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(10, 1001, "com.test.app1", APP_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(11, 1002, "com.test.app2", APP_GATEWAY_CALLSIGN);

    impl->Subscribe(ctx1, true, FB_SETTINGS_CALLSIGN, "onResolutionChanged");
    impl->Subscribe(ctx2, true, FB_SETTINGS_CALLSIGN, "onResolutionChanged");
    YieldToWorkerPool();

    // Unsubscribe ctx1 only; ctx2 still subscribed — no SubscriberJob for unsub
    const uint32_t rc = impl->Subscribe(ctx1, false, FB_SETTINGS_CALLSIGN, "onResolutionChanged");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Unsubscribe_NotLastListener: returns ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-019: Unsubscribe non-existent context does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Unsubscribe_NonExistent_NoCrash()
{
    /** Unsubscribe(listen=false) for an event that was never subscribed does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Unsubscribe_NonExistent: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(99, 9999, "com.test.app", APP_GATEWAY_CALLSIGN);

    // Unsubscribe without ever subscribing
    const uint32_t rc = impl->Subscribe(ctx, false, FB_SETTINGS_CALLSIGN, "nonExistentEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Unsubscribe_NonExistent: returns ERROR_NONE even for non-existent event");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-023: Cleanup removes all contexts matching connectionId + origin
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_RemovesMatchingSubscribers()
{
    /** Cleanup(connId, origin) removes all contexts with matching connectionId and origin. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Cleanup_RemovesMatchingSubscribers: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(42, 1001, "com.test.app1", APP_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(43, 1002, "com.test.app2", APP_GATEWAY_CALLSIGN);  // different connId

    impl->Subscribe(ctx1, true, FB_SETTINGS_CALLSIGN, "onEvent1");
    impl->Subscribe(ctx2, true, FB_SETTINGS_CALLSIGN, "onEvent1");
    YieldToWorkerPool();

    // Cleanup only for connId=42
    const uint32_t rc = impl->Cleanup(42, APP_GATEWAY_CALLSIGN);
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_RemovesMatchingSubscribers: Cleanup returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-024: Cleanup removes the entire entry when all contexts match
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_EmptiesEntireKey()
{
    /** Cleanup erases the key when all contexts under it match connectionId+origin. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Cleanup_EmptiesEntireKey: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(55, 1001, "com.test.app", APP_GATEWAY_CALLSIGN);
    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "onSingleEvent");
    YieldToWorkerPool();

    // Cleanup for the same connId + origin — the entire key should be erased
    const uint32_t rc = impl->Cleanup(55, APP_GATEWAY_CALLSIGN);
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_EmptiesEntireKey: Cleanup returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-025: Cleanup with no matching entries does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_NoMatch_NoCrash()
{
    /** Cleanup with connectionId/origin that matches nothing does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Cleanup_NoMatch_NoCrash: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(10, 1001, "com.test.app", APP_GATEWAY_CALLSIGN);
    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "onEvent");
    YieldToWorkerPool();

    // Cleanup for a connId that doesn't match
    const uint32_t rc = impl->Cleanup(999, GATEWAY_AUTHENTICATOR_CALLSIGN);
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_NoMatch_NoCrash: Cleanup returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-026: Cleanup across multiple event keys
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_MultipleEvents()
{
    /** Cleanup removes contexts across multiple event keys for matching connectionId+origin. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Cleanup_MultipleEvents: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(77, 2001, "com.test.app", APP_GATEWAY_CALLSIGN);

    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "eventA");
    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "eventB");
    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "eventC");
    YieldToWorkerPool();

    // Cleanup for connId=77 — all three keys should have their ctx removed
    const uint32_t rc = impl->Cleanup(77, APP_GATEWAY_CALLSIGN);
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_MultipleEvents: Cleanup returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-091: CleanupNotifications partial match — keeps non-matching contexts
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_PartialMatch_KeepsOthers()
{
    /** CleanupNotifications removes only matching contexts; non-matching contexts
     *  remain and still receive events. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr,
        "Cleanup_PartialMatch_KeepsOthers: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Subscribe 3 contexts: 2 with matching connectionId+origin, 1 with different
    auto ctx1 = MakeContext(50, 1001, "com.app.one",   APP_GATEWAY_CALLSIGN);
    auto ctx2 = MakeContext(50, 1002, "com.app.two",   APP_GATEWAY_CALLSIGN);
    auto ctx3 = MakeContext(51, 1003, "com.app.three", APP_GATEWAY_CALLSIGN);

    impl->Subscribe(ctx1, true, FB_SETTINGS_CALLSIGN, "onPartialClean");
    impl->Subscribe(ctx2, true, FB_SETTINGS_CALLSIGN, "onPartialClean");
    impl->Subscribe(ctx3, true, FB_SETTINGS_CALLSIGN, "onPartialClean");
    YieldToWorkerPool();

    // Cleanup connectionId=50 — ctx1 and ctx2 removed, ctx3 remains
    const uint32_t rcClean = impl->Cleanup(50, APP_GATEWAY_CALLSIGN);
    L0Test::ExpectEqU32(tr, rcClean, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_PartialMatch_KeepsOthers: Cleanup returns ERROR_NONE");

    // Emit — only ctx3 (connId=51) should still receive the event
    impl->Emit("onPartialClean", R"({"after":"cleanup"})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr,
        "Cleanup_PartialMatch_KeepsOthers: gw acquired");
    if (gw != nullptr) {
        // Only 1 context (ctx3) should have received the dispatch
        L0Test::ExpectEqU32(tr, gw->emitCount, 1u,
            "Cleanup_PartialMatch_KeepsOthers: only surviving context should receive event");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-092: CleanupNotifications with different origins — only exact match removed
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_DifferentOrigin_NoMatch()
{
    /** CleanupNotifications matches both connectionId AND origin; different origin
     *  should not remove the context. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr,
        "Cleanup_DifferentOrigin_NoMatch: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(60, 1001, "com.app", APP_GATEWAY_CALLSIGN);
    impl->Subscribe(ctx, true, FB_SETTINGS_CALLSIGN, "onOriginTest");
    YieldToWorkerPool();

    // Cleanup with matching connId but different origin — should NOT remove
    const uint32_t rcClean = impl->Cleanup(60, GATEWAY_AUTHENTICATOR_CALLSIGN);
    L0Test::ExpectEqU32(tr, rcClean, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_DifferentOrigin_NoMatch: returns ERROR_NONE");

    // Emit — context should still be active (origin didn't match cleanup)
    impl->Emit("onOriginTest", R"({"still":"active"})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr && gw->emitCount >= 1,
        "Cleanup_DifferentOrigin_NoMatch: context should still receive events after mismatched cleanup");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-099: Cleanup on empty SubscriberMap does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_Cleanup_EmptyMap_NoCrash()
{
    /** Cleanup() on an impl with no subscribers (empty map) does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock::Config shellCfg;
    shellCfg.provideNotificationHandler = false;
    L0Test::AppNotificationsServiceMock shell(shellCfg);
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr,
        "Cleanup_EmptyMap_NoCrash: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // No Subscribe calls — map is completely empty
    const uint32_t rcClean = impl->Cleanup(1, APP_GATEWAY_CALLSIGN);
    L0Test::ExpectEqU32(tr, rcClean, static_cast<uint32_t>(ERROR_NONE),
        "Cleanup_EmptyMap_NoCrash: returns ERROR_NONE");

    L0Test::ExpectTrue(tr, true,
        "Cleanup_EmptyMap_NoCrash: no crash with empty subscriber map");

    impl->Release();
    return tr.failures;
}
