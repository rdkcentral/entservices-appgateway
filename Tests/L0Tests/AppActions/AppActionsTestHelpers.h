/*
 * AppActionsTestHelpers.h
 *
 * Declares factory helpers for AppActionsImplementation that are
 * compiled into a single translation unit (AppActionsTestHelpers.cpp).
 */

#pragma once

#include <cstdint>

// Forward declarations — test files only need the interface pointer.
namespace WPEFramework {
namespace Exchange {
    struct IAppActions;
} // namespace Exchange
} // namespace WPEFramework

namespace L0Test {
class AppActionsServiceMock;

/**
 * Create a new AppActionsImplementation instance (via
 * Core::Service<>::Create) and call Configure(shell) on it.
 *
 * The caller owns the returned pointer and must call Release() when done.
 * Returns nullptr if creation or configuration fails.
 */
WPEFramework::Exchange::IAppActions*
CreateConfiguredImpl(AppActionsServiceMock* shell);

/**
 * Create a new AppActionsImplementation instance without calling
 * Configure().  The mService pointer inside the implementation will be null.
 *
 * The caller owns the returned pointer and must call Release() when done.
 */
WPEFramework::Exchange::IAppActions* CreateRawImpl();

} // namespace L0Test
