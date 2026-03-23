/*
 * AppNotificationsTestHelpers.cpp
 *
 * Single translation unit that includes AppNotificationsImplementation.h
 * (which transitively includes UtilsController.h with non-inline function
 * definitions).  All other test TUs include only the forward-declaring
 * AppNotificationsTestHelpers.h, so UtilsController.h symbols are defined
 * exactly once and the ODR linker error is avoided.
 */

#include <core/core.h>
#include <plugins/IShell.h>
#include <interfaces/IAppNotifications.h>
#include <interfaces/IConfiguration.h>
#include <AppNotificationsImplementation.h>

#include "AppNotificationsServiceMock.h"
#include "AppNotificationsTestHelpers.h"

using WPEFramework::Exchange::IAppNotifications;
using WPEFramework::Exchange::IConfiguration;
using WPEFramework::Plugin::AppNotificationsImplementation;

namespace L0Test {

IAppNotifications* CreateConfiguredImpl(AppNotificationsServiceMock* shell)
{
    auto* impl = WPEFramework::Core::Service<AppNotificationsImplementation>::Create<IAppNotifications>();
    if (impl == nullptr) {
        return nullptr;
    }
    auto* cfg = impl->QueryInterface<IConfiguration>();
    if (cfg != nullptr) {
        cfg->Configure(shell);
        cfg->Release();
    }
    return impl;
}

IAppNotifications* CreateRawImpl()
{
    return WPEFramework::Core::Service<AppNotificationsImplementation>::Create<IAppNotifications>();
}

} // namespace L0Test
