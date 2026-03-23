/*
 * AppNotificationsTestHelpers.h
 *
 * Declares factory helpers for AppNotificationsImplementation that are
 * compiled into a single translation unit (AppNotificationsTestHelpers.cpp).
 *
 * This avoids ODR violations caused by UtilsController.h containing
 * non-inline function definitions: including AppNotificationsImplementation.h
 * (which pulls in UtilsController.h) from multiple test TUs causes
 * "multiple definition" linker errors.  Centralising the include here
 * keeps UtilsController.h in exactly one TU.
 */

#pragma once

#include <cstdint>

// Forward declarations — test files only need the interface pointer.
namespace WPEFramework {
namespace Exchange {
    struct IAppNotifications;
} // namespace Exchange
} // namespace WPEFramework

namespace L0Test {
class AppNotificationsServiceMock;

/**
 * Create a new AppNotificationsImplementation instance (via
 * Core::Service<>::Create) and call Configure(shell) on it.
 *
 * The caller owns the returned pointer and must call Release() when done.
 * Returns nullptr if creation or configuration fails.
 *
 * IMPORTANT: Configure the shell with provideNotificationHandler=false
 * (MakeSafeConfig) to avoid the known destructor-ordering segfault in
 * AppNotificationsImplementation when mRegisteredNotifications is non-empty.
 */
WPEFramework::Exchange::IAppNotifications*
CreateConfiguredImpl(AppNotificationsServiceMock* shell);

/**
 * Create a new AppNotificationsImplementation instance without calling
 * Configure().  The mShell pointer inside the implementation will be null.
 *
 * The caller owns the returned pointer and must call Release() when done.
 */
WPEFramework::Exchange::IAppNotifications* CreateRawImpl();

} // namespace L0Test
