/*
 * AppNotifications_ContextEqualityTests.cpp
 *
 * L0 tests for the AppNotificationContext operator== defined in
 * AppNotificationsImplementation.cpp.
 * Tests AN-L0-068 to AN-L0-073.
 *
 * The operator== is defined in WPEFramework::Exchange namespace and compares:
 *   requestId, connectionId, appId, origin, version
 *
 * These tests exercise it directly by creating AppNotificationContext structs
 * and verifying equality/inequality behavior.
 */

#include <iostream>
#include <string>

#include <core/core.h>
#include <plugins/IShell.h>

#include <interfaces/IAppNotifications.h>
#include "AppNotificationsServiceMock.h"
#include "L0Expect.hpp"
#include "L0TestTypes.hpp"

// operator== is defined in AppNotificationsImplementation.cpp (compiled into the
// same binary). Declare it here so the compiler can resolve the call site.
namespace WPEFramework {
namespace Exchange {
bool operator==(const IAppNotifications::AppNotificationContext& lhs,
                const IAppNotifications::AppNotificationContext& rhs);
} // namespace Exchange
} // namespace WPEFramework

using WPEFramework::Exchange::IAppNotifications;

namespace {

IAppNotifications::AppNotificationContext MakeCtx(uint32_t requestId,
                                                   uint32_t connectionId,
                                                   const std::string& appId,
                                                   const std::string& origin,
                                                   const std::string& version)
{
    IAppNotifications::AppNotificationContext ctx;
    ctx.requestId    = requestId;
    ctx.connectionId = connectionId;
    ctx.appId        = appId;
    ctx.origin       = origin;
    ctx.version      = version;
    return ctx;
}

} // namespace

// ---------------------------------------------------------------------------
// AN-L0-068: Two identical contexts are equal
// ---------------------------------------------------------------------------
uint32_t Test_AN_AppNotificationContext_Equality()
{
    /** operator== returns true for two contexts with identical fields. */
    L0Test::TestResult tr;

    auto a = MakeCtx(1001, 42, "com.test.app", "org.rdk.AppGateway", "0");
    auto b = MakeCtx(1001, 42, "com.test.app", "org.rdk.AppGateway", "0");

    L0Test::ExpectTrue(tr, (a == b), "AppNotificationContext_Equality: identical contexts should be equal");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-069: Contexts differing in requestId are not equal
// ---------------------------------------------------------------------------
uint32_t Test_AN_AppNotificationContext_Inequality_RequestId()
{
    /** operator== returns false when requestId differs. */
    L0Test::TestResult tr;

    auto a = MakeCtx(1001, 42, "com.test.app", "org.rdk.AppGateway", "0");
    auto b = MakeCtx(9999, 42, "com.test.app", "org.rdk.AppGateway", "0");

    L0Test::ExpectTrue(tr, !(a == b),
        "AppNotificationContext_Inequality_RequestId: contexts with different requestId should not be equal");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-070: Contexts differing in connectionId are not equal
// ---------------------------------------------------------------------------
uint32_t Test_AN_AppNotificationContext_Inequality_ConnectionId()
{
    /** operator== returns false when connectionId differs. */
    L0Test::TestResult tr;

    auto a = MakeCtx(1001, 42,  "com.test.app", "org.rdk.AppGateway", "0");
    auto b = MakeCtx(1001, 999, "com.test.app", "org.rdk.AppGateway", "0");

    L0Test::ExpectTrue(tr, !(a == b),
        "AppNotificationContext_Inequality_ConnectionId: contexts with different connectionId should not be equal");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-071: Contexts differing in appId are not equal
// ---------------------------------------------------------------------------
uint32_t Test_AN_AppNotificationContext_Inequality_AppId()
{
    /** operator== returns false when appId differs. */
    L0Test::TestResult tr;

    auto a = MakeCtx(1001, 42, "com.app.one", "org.rdk.AppGateway", "0");
    auto b = MakeCtx(1001, 42, "com.app.two", "org.rdk.AppGateway", "0");

    L0Test::ExpectTrue(tr, !(a == b),
        "AppNotificationContext_Inequality_AppId: contexts with different appId should not be equal");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-072: Contexts differing in origin are not equal
// ---------------------------------------------------------------------------
uint32_t Test_AN_AppNotificationContext_Inequality_Origin()
{
    /** operator== returns false when origin differs. */
    L0Test::TestResult tr;

    auto a = MakeCtx(1001, 42, "com.test.app", "org.rdk.AppGateway",    "0");
    auto b = MakeCtx(1001, 42, "com.test.app", "org.rdk.LaunchDelegate", "0");

    L0Test::ExpectTrue(tr, !(a == b),
        "AppNotificationContext_Inequality_Origin: contexts with different origin should not be equal");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// AN-L0-073: Contexts differing in version are not equal
// ---------------------------------------------------------------------------
uint32_t Test_AN_AppNotificationContext_Inequality_Version()
{
    /** operator== returns false when version differs. */
    L0Test::TestResult tr;

    auto a = MakeCtx(1001, 42, "com.test.app", "org.rdk.AppGateway", "0");
    auto b = MakeCtx(1001, 42, "com.test.app", "org.rdk.AppGateway", "8");

    L0Test::ExpectTrue(tr, !(a == b),
        "AppNotificationContext_Inequality_Version: contexts with different version should not be equal");

    return tr.failures;
}
