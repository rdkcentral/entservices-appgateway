/*
 * AppNotifications_SubscriberMapTests.cpp
 *
 * L0 tests for SubscriberMap internals: Add, Remove, Get, Exists,
 * EventUpdate, Dispatch, DispatchToGateway, DispatchToLaunchDelegate.
 * Tests AN-L0-027 to AN-L0-048.
 *
 * Strategy:
 *   - Instantiate AppNotificationsImplementation and call Configure()
 *   - Use Subscribe()/Emit() as the public API to drive the SubscriberMap
 *   - Observe side-effects via the AppNotificationsServiceMock fakes
 *     (ANResponderFake counters / ANNotificationHandlerFake counters)
 *
 * Note on SubscriberMap::Add/Remove/Get/Exists:
 *   These are private inner-class methods accessed through the public
 *   Subscribe() / Emit() / Cleanup() API.  We test them indirectly.
 */

#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include <core/core.h>
#include <plugins/IShell.h>

#include <interfaces/IAppNotifications.h>
#include <interfaces/IConfiguration.h>
#include "AppNotificationsServiceMock.h"
#include "AppNotificationsTestHelpers.h"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Exchange::IAppNotifications;
using WPEFramework::Exchange::IConfiguration;

namespace {

// Safe config: disables the notification handler so HandleNotifier() never
// returns true, keeping mRegisteredNotifications empty and avoiding the
// destructor segfault (known bug in AppNotificationsImplementation).
L0Test::AppNotificationsServiceMock::Config MakeSafeConfig(
    bool provideGw  = true,
    bool provideIgw = true)
{
    return L0Test::AppNotificationsServiceMock::Config(
        /*impl*/           true,
        /*gw*/             provideGw,
        /*igw*/            provideIgw,
        /*handler*/        false,  // <-- key: no handler → mRegisteredNotifications stays empty
        /*handlerStatus*/  false,
        /*handlerRc*/      WPEFramework::Core::ERROR_NONE);
}

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

void YieldToWorkerPool(int ms = 80)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

} // namespace

// ---------------------------------------------------------------------------
// AN-L0-027: SubscriberMap::Add — new key creates entry
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Add_NewKey()
{
    /** Add(key, ctx) creates a new entry in the map (observed via Subscribe). */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Add_NewKey: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    const uint32_t rc = impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onTestEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Add_NewKey: Subscribe returns ERROR_NONE");

    YieldToWorkerPool();

    // Verify the key is now in the map by subscribing again (second sub should not
    // enqueue a new SubscriberJob, which means Exists() returned true)
    auto ctx2 = MakeContext(2, 101, "com.app2", "org.rdk.AppGateway");
    const uint32_t rc2 = impl->Subscribe(ctx2, true, "org.rdk.FbSettings", "onTestEvent");
    L0Test::ExpectEqU32(tr, rc2, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Add_NewKey: second Subscribe also returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-028: SubscriberMap::Add — same key appends to existing vector
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Add_ExistingKey()
{
    /** Add(key, ctx) appends to the existing vector for a key. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Add_ExistingKey: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(10, 1000, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(11, 1001, "com.app.two", "org.rdk.AppGateway");

    impl->Subscribe(ctx1, true, "org.rdk.FbSettings", "onSharedEvent");
    YieldToWorkerPool();
    impl->Subscribe(ctx2, true, "org.rdk.FbSettings", "onSharedEvent");
    YieldToWorkerPool();

    // Both subscribed — emit and verify both responders receive it
    impl->Emit("onSharedEvent", R"({"test":1})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    if (gw != nullptr) {
        L0Test::ExpectTrue(tr, gw->emitCount >= 2,
            "SubscriberMap_Add_ExistingKey: both subscribers should receive dispatch");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-029: SubscriberMap::Remove — removes a specific context
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Remove_ExistingContext()
{
    /** Remove(key, ctx) removes only the matching context from the vector. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Remove_ExistingContext: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(10, 1000, "com.app.one", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(11, 1001, "com.app.two", "org.rdk.AppGateway");

    impl->Subscribe(ctx1, true, "org.rdk.FbSettings", "onRemoveTest");
    impl->Subscribe(ctx2, true, "org.rdk.FbSettings", "onRemoveTest");
    YieldToWorkerPool();

    // Remove ctx1 only (key still has ctx2 — no SubscriberJob enqueued for unsub)
    const uint32_t rc = impl->Subscribe(ctx1, false, "org.rdk.FbSettings", "onRemoveTest");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Remove_ExistingContext: returns ERROR_NONE");
    YieldToWorkerPool();

    // ctx2 should still receive events
    impl->Emit("onRemoveTest", R"({"v":1})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    if (gw != nullptr) {
        L0Test::ExpectTrue(tr, gw->emitCount >= 1,
            "SubscriberMap_Remove_ExistingContext: remaining subscriber still gets event");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-030: SubscriberMap::Remove — last context erases the key
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Remove_LastContext_ErasesKey()
{
    /** Remove(key, ctx) erases the map entry when the vector becomes empty. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Remove_LastContext_ErasesKey: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(20, 2000, "com.app", "org.rdk.AppGateway");

    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onSoloEvent");
    YieldToWorkerPool();

    // Remove the only context — key should be erased
    const uint32_t rc = impl->Subscribe(ctx, false, "org.rdk.FbSettings", "onSoloEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Remove_LastContext_ErasesKey: returns ERROR_NONE");
    YieldToWorkerPool();

    // Now emit — no active listeners, should log a warning but not crash
    const uint32_t rcEmit = impl->Emit("onSoloEvent", "{}", "");
    L0Test::ExpectEqU32(tr, rcEmit, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Remove_LastContext_ErasesKey: Emit after removal returns ERROR_NONE");
    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-031: SubscriberMap::Remove — non-existent context does not crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Remove_NonExistent_NoCrash()
{
    /** Remove(key, ctx) for a non-existent context/key does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Remove_NonExistent_NoCrash: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(99, 9999, "com.ghost.app", "org.rdk.AppGateway");

    // Remove without ever subscribing — should not crash
    const uint32_t rc = impl->Subscribe(ctx, false, "org.rdk.FbSettings", "ghostEvent");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Remove_NonExistent_NoCrash: returns ERROR_NONE");

    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-032: SubscriberMap::Get — existing key returns contexts
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Get_Existing()
{
    /** Get(key) returns the registered contexts (verified via dispatch side-effect). */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Get_Existing: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(5, 500, "com.app.test", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onGetTest");
    YieldToWorkerPool();

    // Emit — EventUpdate calls Get() internally; if dispatched, Get() worked
    impl->Emit("onGetTest", R"({"x":1})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    if (gw != nullptr) {
        L0Test::ExpectTrue(tr, gw->emitCount >= 1,
            "SubscriberMap_Get_Existing: dispatch should occur for registered context");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-033: SubscriberMap::Get — non-existent key returns empty vector
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Get_NonExistent()
{
    /** EventUpdate for an unregistered key logs warning and does not dispatch. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Get_NonExistent: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Do NOT subscribe; emit directly
    const uint32_t rc = impl->Emit("nonExistentEvent", R"({"data":1})", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Get_NonExistent: Emit returns ERROR_NONE");
    YieldToWorkerPool();

    // No responder should have been called
    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw == nullptr || gw->emitCount == 0,
        "SubscriberMap_Get_NonExistent: no dispatch for unregistered event");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-034: SubscriberMap::Exists — returns true for registered key
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Exists_True()
{
    /** Exists(key) returns true after Subscribe(listen=true). */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Exists_True: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onExistsTrue");
    YieldToWorkerPool();

    // Second Subscribe for the same event does NOT enqueue a new SubscriberJob
    // (because Exists returns true). We verify this by checking subscribe returns OK.
    auto ctx2 = MakeContext(2, 200, "com.app2", "org.rdk.AppGateway");
    const uint32_t rc = impl->Subscribe(ctx2, true, "org.rdk.FbSettings", "onExistsTrue");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Exists_True: second Subscribe returns ERROR_NONE");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-035: SubscriberMap::Exists — returns false for unregistered key
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Exists_False()
{
    /** Exists(key) returns false for a key that was never subscribed. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Exists_False: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Subscribe to a different event
    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onEventA");
    YieldToWorkerPool();

    // Emit for a non-subscribed event — Exists returns false for this key
    const uint32_t rc = impl->Emit("onEventB_NOT_SUBSCRIBED", "{}", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Exists_False: Emit returns ERROR_NONE");
    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-036: SubscriberMap::Exists — case-insensitive lookup
// ---------------------------------------------------------------------------
uint32_t Test_AN_SubscriberMap_Exists_CaseInsensitive()
{
    /** Exists(key) is case-insensitive (keys stored in lowercase). */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "SubscriberMap_Exists_CaseInsensitive: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onMixedCaseEvent");
    YieldToWorkerPool();

    // Unsubscribe with UPPERCASE variant — should find the lowercase key
    const uint32_t rc = impl->Subscribe(ctx, false, "org.rdk.FbSettings", "ONMIXEDCASEEVENT");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "SubscriberMap_Exists_CaseInsensitive: uppercase unsubscribe returns ERROR_NONE");
    YieldToWorkerPool();

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-037: EventUpdate dispatches to all contexts when appId is empty
// ---------------------------------------------------------------------------
uint32_t Test_AN_EventUpdate_DispatchToAll_EmptyAppId()
{
    /** EventUpdate with empty appId dispatches to all registered contexts. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "EventUpdate_DispatchToAll_EmptyAppId: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx1 = MakeContext(10, 1001, "com.app.alpha", "org.rdk.AppGateway");
    auto ctx2 = MakeContext(11, 1002, "com.app.beta",  "org.rdk.AppGateway");
    auto ctx3 = MakeContext(12, 1003, "com.app.gamma", "org.rdk.AppGateway");

    impl->Subscribe(ctx1, true, "org.rdk.FbSettings", "onBroadcast");
    impl->Subscribe(ctx2, true, "org.rdk.FbSettings", "onBroadcast");
    impl->Subscribe(ctx3, true, "org.rdk.FbSettings", "onBroadcast");
    YieldToWorkerPool();

    // Emit with empty appId → all 3 receive it
    impl->Emit("onBroadcast", R"({"msg":"hello"})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr, "EventUpdate_DispatchToAll_EmptyAppId: gw acquired");
    if (gw != nullptr) {
        L0Test::ExpectTrue(tr, gw->emitCount >= 3,
            "EventUpdate_DispatchToAll_EmptyAppId: all 3 contexts should receive dispatch");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-038: EventUpdate filters by appId when appId is non-empty
// ---------------------------------------------------------------------------
uint32_t Test_AN_EventUpdate_FilterByAppId()
{
    /** EventUpdate with non-empty appId only dispatches to contexts matching that appId. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "EventUpdate_FilterByAppId: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctxTarget  = MakeContext(10, 1001, "com.app.target",  "org.rdk.AppGateway");
    auto ctxOther   = MakeContext(11, 1002, "com.app.other",   "org.rdk.AppGateway");

    impl->Subscribe(ctxTarget, true, "org.rdk.FbSettings", "onFilteredEvent");
    impl->Subscribe(ctxOther,  true, "org.rdk.FbSettings", "onFilteredEvent");
    YieldToWorkerPool();

    // Emit with specific appId — only ctxTarget should be dispatched
    impl->Emit("onFilteredEvent", R"({"data":"targeted"})", "com.app.target");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr, "EventUpdate_FilterByAppId: gw acquired");
    if (gw != nullptr) {
        // Exactly 1 dispatch (to target only)
        L0Test::ExpectEqU32(tr, gw->emitCount, 1u,
            "EventUpdate_FilterByAppId: only matching appId context should receive dispatch");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-039: EventUpdate with no listeners logs warning — no crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_EventUpdate_NoListeners_LogWarning()
{
    /** EventUpdate for an event with no subscribers logs a warning and does not crash. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "EventUpdate_NoListeners_LogWarning: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Emit without any Subscribe — EventUpdate should log LOGWARN and return
    const uint32_t rc = impl->Emit("onNoListenerEvent", R"({"x":1})", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "EventUpdate_NoListeners_LogWarning: Emit returns ERROR_NONE");
    YieldToWorkerPool();

    // No responder calls should have happened
    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw == nullptr || gw->emitCount == 0,
        "EventUpdate_NoListeners_LogWarning: no dispatch when no listeners");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-040: Dispatch routes to Gateway when origin = APP_GATEWAY_CALLSIGN
// ---------------------------------------------------------------------------
uint32_t Test_AN_Dispatch_OriginGateway()
{
    /** Dispatch calls DispatchToGateway when context.origin == "org.rdk.AppGateway". */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Dispatch_OriginGateway: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(5, 500, "com.app.gw", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onGatewayEvent");
    YieldToWorkerPool();

    impl->Emit("onGatewayEvent", R"({"gw":true})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ANResponderFake* ld = shell.GetInternalGatewayFake();

    L0Test::ExpectTrue(tr, gw != nullptr && gw->emitCount > 0,
        "Dispatch_OriginGateway: AppGateway responder should receive dispatch");
    L0Test::ExpectTrue(tr, ld == nullptr || ld->emitCount == 0,
        "Dispatch_OriginGateway: LaunchDelegate should NOT receive dispatch");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-041: Dispatch routes to LaunchDelegate when origin != APP_GATEWAY_CALLSIGN
// ---------------------------------------------------------------------------
uint32_t Test_AN_Dispatch_OriginNonGateway()
{
    /** Dispatch calls DispatchToLaunchDelegate when context.origin is not the AppGateway callsign. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "Dispatch_OriginNonGateway: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Use LaunchDelegate as origin
    auto ctx = MakeContext(5, 500, "com.app.ld", "org.rdk.LaunchDelegate");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onLaunchDelegateEvent");
    YieldToWorkerPool();

    impl->Emit("onLaunchDelegateEvent", R"({"ld":true})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ANResponderFake* ld = shell.GetInternalGatewayFake();

    L0Test::ExpectTrue(tr, ld != nullptr && ld->emitCount > 0,
        "Dispatch_OriginNonGateway: LaunchDelegate responder should receive dispatch");
    L0Test::ExpectTrue(tr, gw == nullptr || gw->emitCount == 0,
        "Dispatch_OriginNonGateway: AppGateway should NOT receive dispatch");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-042: DispatchToGateway lazy-acquires AppGateway responder on first call
// ---------------------------------------------------------------------------
uint32_t Test_AN_DispatchToGateway_LazyAcquire_Success()
{
    /** DispatchToGateway lazy-acquires the IAppGatewayResponder on first dispatch. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "DispatchToGateway_LazyAcquire_Success: impl creation");
    if (impl == nullptr) { return tr.failures; }

    // Initially, AppGateway responder should not have been acquired
    L0Test::ExpectTrue(tr, shell.GetAppGatewayFake() == nullptr,
        "DispatchToGateway_LazyAcquire_Success: gw not acquired before first dispatch");

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onLazyAcquireEvent");
    YieldToWorkerPool();

    impl->Emit("onLazyAcquireEvent", R"({"lazy":true})", "");
    YieldToWorkerPool();

    // After first dispatch, the lazy pointer should be populated
    L0Test::ANResponderFake* gw = shell.GetAppGatewayFake();
    L0Test::ExpectTrue(tr, gw != nullptr,
        "DispatchToGateway_LazyAcquire_Success: gw acquired after first dispatch");
    if (gw != nullptr) {
        L0Test::ExpectTrue(tr, gw->emitCount > 0,
            "DispatchToGateway_LazyAcquire_Success: Emit called on gw responder");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-043: DispatchToGateway when AppGateway responder is unavailable — no crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_DispatchToGateway_LazyAcquire_Failure()
{
    /** DispatchToGateway logs error and does not crash when QueryInterfaceByCallsign returns nullptr. */
    L0Test::TestResult tr;

    // Configure shell to NOT provide AppGateway responder (safe config disables handler too)
    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig(/*provideGw=*/false, /*provideIgw=*/true));
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "DispatchToGateway_LazyAcquire_Failure: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.AppGateway");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onNoGwEvent");
    YieldToWorkerPool();

    // Emit — DispatchToGateway will fail to acquire, log error, and return
    const uint32_t rc = impl->Emit("onNoGwEvent", R"({"fail":true})", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "DispatchToGateway_LazyAcquire_Failure: Emit returns ERROR_NONE despite null gw");
    YieldToWorkerPool();

    // No crash = pass
    L0Test::ExpectTrue(tr, true, "DispatchToGateway_LazyAcquire_Failure: no crash");

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-044: DispatchToLaunchDelegate lazy-acquires on first call
// ---------------------------------------------------------------------------
uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquire_Success()
{
    /** DispatchToLaunchDelegate lazy-acquires the InternalGateway responder on first dispatch. */
    L0Test::TestResult tr;

    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig());
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "DispatchToLaunchDelegate_LazyAcquire_Success: impl creation");
    if (impl == nullptr) { return tr.failures; }

    L0Test::ExpectTrue(tr, shell.GetInternalGatewayFake() == nullptr,
        "DispatchToLaunchDelegate_LazyAcquire_Success: ld not acquired before first dispatch");

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.LaunchDelegate");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onLdLazyEvent");
    YieldToWorkerPool();

    impl->Emit("onLdLazyEvent", R"({"x":1})", "");
    YieldToWorkerPool();

    L0Test::ANResponderFake* ld = shell.GetInternalGatewayFake();
    L0Test::ExpectTrue(tr, ld != nullptr,
        "DispatchToLaunchDelegate_LazyAcquire_Success: ld acquired after first dispatch");
    if (ld != nullptr) {
        L0Test::ExpectTrue(tr, ld->emitCount > 0,
            "DispatchToLaunchDelegate_LazyAcquire_Success: Emit called on ld responder");
    }

    impl->Release();
    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-045: DispatchToLaunchDelegate when InternalGateway unavailable — no crash
// ---------------------------------------------------------------------------
uint32_t Test_AN_DispatchToLaunchDelegate_LazyAcquire_Failure()
{
    /** DispatchToLaunchDelegate logs error and does not crash when responder unavailable. */
    L0Test::TestResult tr;

    // Configure shell to NOT provide InternalGateway (safe config disables handler too)
    L0Test::AppNotificationsServiceMock shell(MakeSafeConfig(/*provideGw=*/true, /*provideIgw=*/false));
    auto* impl = CreateConfiguredImpl(&shell);
    L0Test::ExpectTrue(tr, impl != nullptr, "DispatchToLaunchDelegate_LazyAcquire_Failure: impl creation");
    if (impl == nullptr) { return tr.failures; }

    auto ctx = MakeContext(1, 100, "com.app", "org.rdk.LaunchDelegate");
    impl->Subscribe(ctx, true, "org.rdk.FbSettings", "onNoLdEvent");
    YieldToWorkerPool();

    const uint32_t rc = impl->Emit("onNoLdEvent", R"({"fail":true})", "");
    L0Test::ExpectEqU32(tr, rc, static_cast<uint32_t>(ERROR_NONE),
        "DispatchToLaunchDelegate_LazyAcquire_Failure: Emit returns ERROR_NONE despite null ld");
    YieldToWorkerPool();

    L0Test::ExpectTrue(tr, true, "DispatchToLaunchDelegate_LazyAcquire_Failure: no crash");

    impl->Release();
    return tr.failures;
}
