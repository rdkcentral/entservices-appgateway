/*
 * AppNotificationsTestHelpers.cpp
 *
 * Single translation unit that pulls in AppNotificationsImplementation.cpp
 * (which itself includes AppNotificationsImplementation.h and transitively
 * UtilsController.h with non-inline function definitions).  By including the
 * production .cpp here and removing it from APPNOTIF_L0_SOURCES, all
 * Utils::* symbols are defined exactly once and the ODR linker error is
 * avoided.  All other test TUs include only AppNotificationsTestHelpers.h.
 */

// Pull in the production implementation as part of this TU so that
// UtilsController.h functions are emitted exactly once.
// NOLINTNEXTLINE(build/include)
#include <AppNotificationsImplementation.cpp>

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
