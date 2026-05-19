#pragma once
#ifndef __APPACTIONSIMPLEMENTATION_H__
#define __APPACTIONSIMPLEMENTATION_H__
#include <interfaces/IConfiguration.h>
#include <interfaces/IAppActions.h>

namespace WPEFramework {
namespace Plugin {

    class AppActionsImplementation : public PluginHost::IPlugin, Exchange::IAppActions, public Exchange::IConfiguration {
        public:
            AppActionsImplementation();
            ~AppActionsImplementation() override;

            // We do not allow this plugin to be copied !!
            AppActionsImplementation(const AppActionsImplementation&) = delete;
            AppActionsImplementation& operator=(const AppActionsImplementation&) = delete;

            BEGIN_INTERFACE_MAP(AppActionsImplementation)
            INTERFACE_ENTRY(Exchange::IAppActions)
            INTERFACE_ENTRY(Exchange::IConfiguration)
            END_INTERFACE_MAP

            Core::hresult ActionStart(const string& initiator, const string& intent, const string& handlerAppId) override;
            Core::hresult Register(Exchange::IAppActions::INotification *notification) override;

            /** Unregister notification interface */
            Core::hresult Unregister(Exchange::IAppActions::INotification *notification) override;
        
        private:
            std::list<Exchange::IAppActions::INotification*> mAppActionsNotifications;
            mutable Core::CriticalSection mAdminLock;
    };

} // namespace Plugin
} // namespace WPEFramework
#endif // __APPACTIONSIMPLEMENTATION_H__