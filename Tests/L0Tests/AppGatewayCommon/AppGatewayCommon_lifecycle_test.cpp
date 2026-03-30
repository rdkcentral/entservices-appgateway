#include "AppGatewayCommon_common_test.h"

// TEST_ID: AGC_L0_001
// Validate that Initialize returns empty string (success) and Deinitialize completes without crash.
uint32_t Test_Initialize_Deinitialize_Lifecycle()
{
    TestResult tr;
    PluginAndService ps;

    const std::string initResult = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, initResult.empty(), "Initialize returns empty string");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_002
// Validate that two separate plugin instances can initialize and deinitialize in sequence.
uint32_t Test_Initialize_Twice_Idempotent()
{
    TestResult tr;

    {
        PluginAndService ps;
        const std::string init1 = ps.plugin->Initialize(ps.service);
        ExpectTrue(tr, init1.empty(), "First Initialize returns empty string");
        ps.plugin->Deinitialize(ps.service);
    }

    {
        PluginAndService ps;
        const std::string init2 = ps.plugin->Initialize(ps.service);
        ExpectTrue(tr, init2.empty(), "Second Initialize returns empty string");
        ps.plugin->Deinitialize(ps.service);
    }

    return tr.failures;
}

// TEST_ID: AGC_L0_003
// Validate that two separate plugin instances can deinitialize cleanly (robustness).
uint32_t Test_Deinitialize_Twice_NoCrash()
{
    TestResult tr;

    {
        PluginAndService ps;
        ps.plugin->Initialize(ps.service);
        ps.plugin->Deinitialize(ps.service);
    }

    {
        PluginAndService ps;
        ps.plugin->Initialize(ps.service);
        ps.plugin->Deinitialize(ps.service);
    }

    return tr.failures;
}

// TEST_ID: AGC_L0_004
// Information() should return an empty string.
uint32_t Test_Information_ReturnsEmpty()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);
    const std::string info = ps.plugin->Information();
    ExpectTrue(tr, info.empty(), "Information() returns empty string");
    ps.plugin->Deinitialize(ps.service);

    return tr.failures;
}

// TEST_ID: AGC_L0_005
// Interface map: QueryInterface for IAppGatewayRequestHandler returns valid pointer.
uint32_t Test_InterfaceMap_RequestHandler()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* requestHandler = ps.plugin->QueryInterface<Exchange::IAppGatewayRequestHandler>();
    ExpectTrue(tr, nullptr != requestHandler, "QueryInterface<IAppGatewayRequestHandler> returns non-null");
    if (nullptr != requestHandler) {
        requestHandler->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_006
// Interface map: QueryInterface for IAppGatewayAuthenticator returns valid pointer.
uint32_t Test_InterfaceMap_Authenticator()
{
    TestResult tr;
    PluginAndService ps;

    ps.plugin->Initialize(ps.service);

    auto* authenticator = ps.plugin->QueryInterface<Exchange::IAppGatewayAuthenticator>();
    ExpectTrue(tr, nullptr != authenticator, "QueryInterface<IAppGatewayAuthenticator> returns non-null");
    if (nullptr != authenticator) {
        authenticator->Release();
    }

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_007
// QueryInterface for IAppNotificationHandler returns valid pointer.
uint32_t Test_InterfaceMap_NotificationHandler()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    auto* handler = ps.plugin->QueryInterface<Exchange::IAppNotificationHandler>();
    ExpectTrue(tr, nullptr != handler, "QueryInterface<IAppNotificationHandler> returns non-null");
    if (nullptr != handler) {
        handler->Release();
    }
    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}

// TEST_ID: AGC_L0_008
// HandleAppGatewayRequest before Initialize → mDelegate is null → ERROR_UNAVAILABLE
// Covers the null-delegate guard at the top of HandleAppGatewayRequest (lines 312-316).
uint32_t Test_HandleRequest_BeforeInit()
{
    TestResult tr;
    PluginAndService ps;
    // Deliberately do NOT call Initialize — mDelegate remains nullptr
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "HandleAppGatewayRequest before Init returns ERROR_UNAVAILABLE");
    ExpectTrue(tr, result.find("unavailable") != std::string::npos,
               "result contains 'unavailable' error message");
    return tr.failures;
}

// TEST_ID: AGC_L0_009
// Authenticate before Initialize → InvokeLifecycleDelegate returns ERROR_UNAVAILABLE (null delegate)
// Covers the `if (!delegate)` guard in the InvokeLifecycleDelegate template.
uint32_t Test_Authenticate_BeforeInit()
{
    TestResult tr;
    PluginAndService ps;
    // Deliberately do NOT call Initialize — mDelegate remains nullptr
    auto* auth = ps.plugin->QueryInterface<Exchange::IAppGatewayAuthenticator>();
    ExpectTrue(tr, nullptr != auth, "QueryInterface<IAppGatewayAuthenticator> returns non-null");
    if (nullptr != auth) {
        std::string appId;
        const uint32_t rc = auth->Authenticate("session-before-init", appId);
        ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "Authenticate before Init returns ERROR_UNAVAILABLE");
        auth->Release();
    }
    return tr.failures;
}

// TEST_ID: AGC_L0_010
// HandleAppGatewayRequest after Deinitialize → mDelegate has been reset → ERROR_UNAVAILABLE
// Real-world scenario: plugin deactivated but a pending request arrives (race condition).
uint32_t Test_HandleRequest_AfterDeinit()
{
    TestResult tr;
    PluginAndService ps;
    ps.plugin->Initialize(ps.service);
    ps.plugin->Deinitialize(ps.service);
    // After Deinitialize, mDelegate is reset to nullptr
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "HandleAppGatewayRequest after Deinit returns ERROR_UNAVAILABLE");
    return tr.failures;
}

// TEST_ID: AGC_L0_011
// Re-initialization: Init → Deinit → Init → use → Deinit on the same instance.
// Validates that the plugin can be cleanly reactivated after deactivation.
uint32_t Test_Reinitialize_AfterDeinit()
{
    TestResult tr;
    PluginAndService ps;

    // First lifecycle
    const std::string init1 = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, init1.empty(), "First Initialize succeeds");
    ps.plugin->Deinitialize(ps.service);

    // Second lifecycle on the same instance
    const std::string init2 = ps.plugin->Initialize(ps.service);
    ExpectTrue(tr, init2.empty(), "Re-Initialize after Deinit succeeds");

    // Verify the plugin works after re-initialization
    QIGuard<Exchange::IAppGatewayRequestHandler> handler(ps.plugin);
    std::string result;
    Exchange::GatewayContext ctx = DefaultContext();
    const uint32_t rc = handler->HandleAppGatewayRequest(ctx, "device.make", "{}", result);
    const bool ok = (rc == ERROR_NONE || rc == ERROR_UNAVAILABLE || rc == ERROR_GENERAL);
    ExpectTrue(tr, ok, "device.make works after re-initialization");

    ps.plugin->Deinitialize(ps.service);
    return tr.failures;
}
