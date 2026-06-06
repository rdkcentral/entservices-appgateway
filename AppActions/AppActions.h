#pragma once
#ifndef __APPACTIONS_H__
#define __APPACTIONS_H__
#include "Module.h"
#include <interfaces/json/JAppActions.h>
#include <interfaces/IAppActions.h>
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework {
namespace Plugin {

    class AppActions : public PluginHost::IPlugin, public PluginHost::JSONRPC {

        private:
        class Notification : public RPC::IRemoteConnection::INotification,
                            public Exchange::IAppActions::INotification
            {
            private:
                Notification() = delete;
                Notification(const Notification &) = delete;
                Notification &operator=(const Notification &) = delete;

            public:
                explicit Notification(AppActions *parent)
                    : _parent(*parent)
                {
                    LOGINFO("AppActions: Notification constructor");
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification()
                {
                    LOGINFO("AppActions: Notification destructor");
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::IAppActions::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

                void Activated(RPC::IRemoteConnection *) override
                {
                    LOGINFO("AppActions Notification Activated");
                }

                void Deactivated(RPC::IRemoteConnection *connection) override
                {
                    LOGINFO("AppActions Notification Deactivated");
                    _parent.Deactivated(connection);
                }

                void OnActionStartRequest(const string& initiator, const string& intent, const string& handlerAppId) override
                {
                    // Log only metadata (lengths) to avoid leaking user/content data into logs
                    // at high volume; full values are forwarded to JSONRPC subscribers below.
                    LOGDBG("AppActions OnActionStartRequest: initiator=%s intentLen=%zu handlerAppId=%s",
                           initiator.c_str(), intent.size(), handlerAppId.c_str());
                    Exchange::JAppActions::Event::OnActionStartRequest(_parent, initiator, intent, handlerAppId);
                }

            private:
                AppActions &_parent;
            };

        public:
            AppActions(const AppActions&) = delete;
            AppActions& operator=(const AppActions&) = delete;
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
            Core::Sink<Notification> mAppActionsNotification;
    };

} // namespace Plugin
} // namespace WPEFramework
#endif // __APPACTIONS_H__