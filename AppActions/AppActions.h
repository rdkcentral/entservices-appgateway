#pragma once
#ifndef __APPACTIONS_H__
#define __APPACTIONS_H__
#include "Module.h"
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <interfaces/IAppActions.h>

namespace WPEFramework {
namespace Plugin {

    class AppActions : public PluginHost::IPlugin, public PluginHost::JSONRPC {

        public:
            AppActions(const AppActions&) = delete;
            AppActions& operator=(const AppActions&) = delete;
            static AppActions *_instance;
            AppActions();
            ~AppActions() override;

            // IPlugin methods
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

            BEGIN_INTERFACE_MAP(AppActions)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IAppActions, mAppActions)
            END_INTERFACE_MAP

        private:
            void Deactivated(RPC::IRemoteConnection* connection);
            PluginHost::IShell *mService{};
            uint32_t mConnectionId{};
            Exchange::IAppActions *mAppActions{};
            Exchange::IConfiguration* mAppActionsConfigure;
    };

} // namespace Plugin
} // namespace WPEFramework
#endif // __APPACTIONS_H__