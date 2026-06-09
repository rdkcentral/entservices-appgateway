/*
 * AppActionsTestHelpers.cpp
 *
 * Implementation of factory helpers for AppActionsImplementation.
 * This file centralizes includes that can cause ODR issues if pulled
 * from multiple translation units.
 */

#include "AppActionsTestHelpers.h"
#include "AppActionsServiceMock.h"

#include <core/core.h>
#include <interfaces/IAppActions.h>
#include <interfaces/IConfiguration.h>
#include <AppActionsImplementation.h>

namespace L0Test {

WPEFramework::Exchange::IAppActions*
CreateConfiguredImpl(AppActionsServiceMock* shell)
{
    if (nullptr == shell) {
        return nullptr;
    }

    auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppActionsImplementation>::Create<WPEFramework::Exchange::IAppActions>();
    if (nullptr == impl) {
        return nullptr;
    }

    auto* configIface = impl->QueryInterface<WPEFramework::Exchange::IConfiguration>();
    if (nullptr != configIface) {
        configIface->Configure(shell);
        configIface->Release();
    }

    return impl;
}

WPEFramework::Exchange::IAppActions* CreateRawImpl()
{
    return WPEFramework::Core::Service<WPEFramework::Plugin::AppActionsImplementation>::Create<WPEFramework::Exchange::IAppActions>();
}

} // namespace L0Test
