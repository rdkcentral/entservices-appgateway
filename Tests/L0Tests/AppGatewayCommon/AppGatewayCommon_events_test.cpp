#include "AppGatewayCommon_common_test.h"

// TEST_ID: AGC_L0_009
// CheckPermissionGroup always returns allowed=true (no permission groups defined yet).
uint32_t Test_CheckPermissionGroup_DefaultAllowed()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    bool allowed = false;
    const uint32_t rc = agc->CheckPermissionGroup("com.test.app", "someGroup", allowed);
    ExpectEqU32(tr, rc, ERROR_NONE, "CheckPermissionGroup returns ERROR_NONE");
    ExpectTrue(tr, allowed, "CheckPermissionGroup defaults to allowed=true");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_010
// Authenticate delegates to LifecycleDelegate via InvokeLifecycleDelegate.
// In L0 mDelegate and lifecycleDelegate are both non-null (created in Initialize),
// so LifecycleDelegate::Authenticate is reached.  The session ID is not in the
// AppIdInstanceIdMap, so GetAppId returns empty → ERROR_GENERAL.
uint32_t Test_Authenticate_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string appId;
    const uint32_t rc = agc->Authenticate("test-session-id", appId);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "Authenticate returns ERROR_GENERAL in L0 (session not in map)");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_011
// GetSessionId delegates to LifecycleDelegate via InvokeLifecycleDelegate.
// In L0 the delegate chain is non-null, so LifecycleDelegate::GetSessionId is reached.
// The appId is not in the map → GetAppInstanceId returns empty → ERROR_GENERAL.
uint32_t Test_GetSessionId_DelegateUnavailable()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    std::string sessionId;
    const uint32_t rc = agc->GetSessionId("com.test.app", sessionId);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "GetSessionId returns ERROR_GENERAL in L0 (appId not in map)");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_085
// HandleAppEventNotifier with null cb → ERROR_GENERAL, status==false
uint32_t Test_HandleAppEventNotifier_NullCb()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    bool status = true;
    const uint32_t rc = agc->HandleAppEventNotifier(nullptr, "lifecycle.onbackground", true, status);
    ExpectEqU32(tr, rc, ERROR_GENERAL, "HandleAppEventNotifier with null cb returns ERROR_GENERAL");
    ExpectTrue(tr, false == status, "HandleAppEventNotifier with null cb sets status false");
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_086
// HandleAppEventNotifier with valid emitter → ERROR_NONE, status==true; exercise listen and unlisten
uint32_t Test_HandleAppEventNotifier_ValidCb()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* agc = static_cast<AppGatewayCommon*>(ps.plugin);
    auto* emitter = new StubEmitter(); // refcount=1

    // listen=true
    bool status1 = false;
    const uint32_t rc1 = agc->HandleAppEventNotifier(emitter, "lifecycle.onbackground", true, status1);
    ExpectEqU32(tr, rc1, ERROR_NONE, "HandleAppEventNotifier listen=true returns ERROR_NONE");
    ExpectTrue(tr, status1, "HandleAppEventNotifier listen=true sets status true");

    // listen=false (unlisten)
    bool status2 = false;
    const uint32_t rc2 = agc->HandleAppEventNotifier(emitter, "lifecycle.onbackground", false, status2);
    ExpectEqU32(tr, rc2, ERROR_NONE, "HandleAppEventNotifier listen=false returns ERROR_NONE");
    ExpectTrue(tr, status2, "HandleAppEventNotifier listen=false sets status true");

    // Allow WorkerPool to drain the two async EventRegistrationJobs before teardown,
    // preventing a use-after-free on the emitter/delegate pointers.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    emitter->Release(); // drop our initial ref
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_089
// HandleAppEventNotifier before Initialize → SafeSubmitEventRegistrationJob hits null mDelegate → ERROR_GENERAL
// Covers the mDelegate null-check branch inside SafeSubmitEventRegistrationJob (lines 481-483).
uint32_t Test_HandleAppEventNotifier_BeforeInit()
{
    TestResult tr;
    PluginAndService ps;
    // Deliberately do NOT call Initialize — mDelegate remains nullptr
    auto* handler = ps.plugin->QueryInterface<Exchange::IAppNotificationHandler>();
    ExpectTrue(tr, nullptr != handler, "QueryInterface<IAppNotificationHandler> returns non-null");
    if (nullptr != handler) {
        auto* emitter = new StubEmitter();
        bool status = true;
        const uint32_t rc = handler->HandleAppEventNotifier(emitter, "lifecycle.onbackground", true, status);
        ExpectEqU32(tr, rc, ERROR_GENERAL, "HandleAppEventNotifier before Init returns ERROR_GENERAL");
        ExpectTrue(tr, false == status, "status is false when delegate is null");
        emitter->Release();
        handler->Release();
    }
    return tr.failures;
}
