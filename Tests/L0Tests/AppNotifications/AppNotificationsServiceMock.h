#pragma once

/*
 * L0 test ServiceMock for AppNotifications plugin.
 *
 * Implements PluginHost::IShell so AppNotifications plugin shell and
 * AppNotificationsImplementation can Initialize/Deinitialize/Configure
 * in-proc without a real Thunder host.
 *
 * Also implements IShell::ICOMLink so IShell::Root<T>() can instantiate
 * AppNotificationsImplementation in-proc via COMLink()->Instantiate(...).
 *
 * QueryInterfaceByCallsign routes to:
 *   - ResponderFake for APP_GATEWAY_CALLSIGN and INTERNAL_GATEWAY_CALLSIGN
 *   - NotificationHandlerFake for handler module callsigns (e.g. "org.rdk.FbSettings")
 *
 * By default, Release() does NOT delete 'this' to support stack allocation.
 */

#include <atomic>
#include <cstdint>
#include <string>
#include <map>
#include <mutex>

#include <Module.h>

#include <core/core.h>
#include <plugins/IShell.h>

#include <interfaces/IAppGateway.h>
#include <interfaces/IAppNotifications.h>
#include <interfaces/IConfiguration.h>

#include "L0ResponderFake.hpp"
#include "L0NotificationHandlerFake.hpp"

namespace L0Test {

    using string = std::string;

    class AppNotificationsServiceMock final
        : public WPEFramework::PluginHost::IShell
        , public WPEFramework::PluginHost::IShell::ICOMLink {
    public:

        struct Config {
            bool provideImplementation;        // Instantiate() returns impl or nullptr
            bool provideGatewayResponder;      // QueryInterfaceByCallsign returns ResponderFake for APP_GATEWAY_CALLSIGN
            bool provideInternalResponder;     // QueryInterfaceByCallsign returns ResponderFake for INTERNAL_GATEWAY_CALLSIGN
            bool provideNotificationHandler;   // QueryInterfaceByCallsign returns NotificationHandlerFake for handler module

            string handlerModuleCallsign;      // e.g. "org.rdk.FbSettings" — the module callsign for IAppNotificationHandler

            explicit Config(const bool impl = true,
                            const bool gwResponder = true,
                            const bool intResponder = true,
                            const bool handler = true)
                : provideImplementation(impl)
                , provideGatewayResponder(gwResponder)
                , provideInternalResponder(intResponder)
                , provideNotificationHandler(handler)
                , handlerModuleCallsign("org.rdk.FbSettings")
            {
            }
        };

        explicit AppNotificationsServiceMock(Config cfg = Config())
            : _refCount(1)
            , _instantiateCount(0)
            , _callsign("org.rdk.AppNotifications")
            , _className("AppNotifications")
            , _cfg(cfg)
            , _gatewayResponder(nullptr)
            , _internalResponder(nullptr)
            , _notificationHandler(nullptr)
        {
        }

        ~AppNotificationsServiceMock() override
        {
            if (_gatewayResponder != nullptr) {
                _gatewayResponder->Release();
                _gatewayResponder = nullptr;
            }
            if (_internalResponder != nullptr) {
                _internalResponder->Release();
                _internalResponder = nullptr;
            }
            if (_notificationHandler != nullptr) {
                _notificationHandler->Release();
                _notificationHandler = nullptr;
            }
        }

        // Core::IUnknown
        void AddRef() const override
        {
            _refCount.fetch_add(1, std::memory_order_relaxed);
        }

        uint32_t Release() const override
        {
            const uint32_t newCount = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
            if (newCount == 0) {
                return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
            }
            return WPEFramework::Core::ERROR_NONE;
        }

        void* QueryInterface(const uint32_t id) override
        {
            if (id == WPEFramework::PluginHost::IShell::ID) {
                AddRef();
                return static_cast<WPEFramework::PluginHost::IShell*>(this);
            }
            return nullptr;
        }

        // IShell
        void EnableWebServer(const string& /*URLPath*/, const string& /*fileSystemPath*/) override {}
        void DisableWebServer() override {}

        string Model() const override { return "l0test-device"; }
        bool Background() const override { return false; }
        string Accessor() const override { return "127.0.0.1:9998"; }
        string WebPrefix() const override { return "/jsonrpc"; }
        string Locator() const override { return "libWPEFrameworkAppNotifications.so"; }
        string ClassName() const override { return _className; }
        string Versions() const override { return "1.0.0"; }
        string Callsign() const override { return _callsign; }

        string PersistentPath() const override { return "/tmp"; }
        string VolatilePath() const override { return "/tmp"; }
        string DataPath() const override { return "/tmp"; }
        string ProxyStubPath() const override { return "/tmp"; }
        string SystemPath() const override { return "/tmp"; }
        string PluginPath() const override { return "/tmp"; }
        string SystemRootPath() const override { return "/"; }

        WPEFramework::Core::hresult SystemRootPath(const string& /*systemRootPath*/) override { return WPEFramework::Core::ERROR_NONE; }

        startup Startup() const override { return startup::ACTIVATED; }
        WPEFramework::Core::hresult Startup(const startup /*value*/) override { return WPEFramework::Core::ERROR_NONE; }

        string Substitute(const string& input) const override { return input; }

        bool Resumed() const override { return false; }
        WPEFramework::Core::hresult Resumed(const bool /*value*/) override { return WPEFramework::Core::ERROR_NONE; }

        string HashKey() const override { return "hash"; }

        string ConfigLine() const override { return ""; }
        WPEFramework::Core::hresult ConfigLine(const string& /*config*/) override { return WPEFramework::Core::ERROR_NONE; }

        WPEFramework::Core::hresult Metadata(string& info /* @out */) const override
        {
            info = R"({"name":"AppNotifications","version":"1.0.0"})";
            return WPEFramework::Core::ERROR_NONE;
        }

        bool IsSupported(const uint8_t /*version*/) const override { return true; }

        WPEFramework::PluginHost::ISubSystem* SubSystems() override { return nullptr; }

        void Notify(const string& /*message*/) override {}

        void Register(WPEFramework::PluginHost::IPlugin::INotification* /*sink*/) override {}
        void Unregister(WPEFramework::PluginHost::IPlugin::INotification* /*sink*/) override {}

        state State() const override { return state::ACTIVATED; }

        void* QueryInterfaceByCallsign(const uint32_t id, const string& name) override
        {
            // IAppGatewayResponder — APP_GATEWAY_CALLSIGN
            if (id == WPEFramework::Exchange::IAppGatewayResponder::ID) {
                if (name == "org.rdk.AppGateway" && _cfg.provideGatewayResponder) {
                    if (_gatewayResponder == nullptr) {
                        _gatewayResponder = new ResponderFake(true);
                    }
                    _gatewayResponder->AddRef();
                    return static_cast<WPEFramework::Exchange::IAppGatewayResponder*>(_gatewayResponder);
                }
                if (name == "org.rdk.LaunchDelegate" && _cfg.provideInternalResponder) {
                    if (_internalResponder == nullptr) {
                        _internalResponder = new ResponderFake(true);
                    }
                    _internalResponder->AddRef();
                    return static_cast<WPEFramework::Exchange::IAppGatewayResponder*>(_internalResponder);
                }
                return nullptr;
            }

            // IAppNotificationHandler — handler module callsign
            if (id == WPEFramework::Exchange::IAppNotificationHandler::ID) {
                if (_cfg.provideNotificationHandler && name == _cfg.handlerModuleCallsign) {
                    if (_notificationHandler == nullptr) {
                        _notificationHandler = new NotificationHandlerFake(true, WPEFramework::Core::ERROR_NONE);
                    }
                    _notificationHandler->AddRef();
                    return static_cast<WPEFramework::Exchange::IAppNotificationHandler*>(_notificationHandler);
                }
                return nullptr;
            }

            return nullptr;
        }

        WPEFramework::Core::hresult Activate(const reason /*why*/) override { return WPEFramework::Core::ERROR_NONE; }
        WPEFramework::Core::hresult Deactivate(const reason /*why*/) override { return WPEFramework::Core::ERROR_NONE; }
        WPEFramework::Core::hresult Unavailable(const reason /*why*/) override { return WPEFramework::Core::ERROR_NONE; }
        WPEFramework::Core::hresult Hibernate(const uint32_t /*timeout*/) override { return WPEFramework::Core::ERROR_NONE; }
        reason Reason() const override { return reason::REQUESTED; }

        uint32_t Submit(const uint32_t /*Id*/, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>& /*response*/) override
        {
            return WPEFramework::Core::ERROR_NONE;
        }

        WPEFramework::PluginHost::IShell::ICOMLink* COMLink() override
        {
            return this;
        }

        // ICOMLink
        void Register(WPEFramework::RPC::IRemoteConnection::INotification* /*sink*/) override {}
        void Unregister(const WPEFramework::RPC::IRemoteConnection::INotification* /*sink*/) override {}

        void Register(WPEFramework::PluginHost::IShell::ICOMLink::INotification* /*sink*/) override {}
        void Unregister(WPEFramework::PluginHost::IShell::ICOMLink::INotification* /*sink*/) override {}

        WPEFramework::RPC::IRemoteConnection* RemoteConnection(const uint32_t /*connectionId*/) override
        {
            return nullptr;
        }

        void* Instantiate(const WPEFramework::RPC::Object& /*object*/, const uint32_t /*waitTime*/, uint32_t& connectionId) override
        {
            connectionId = 1;
            _instantiateCount.fetch_add(1, std::memory_order_acq_rel);

            if (_cfg.provideImplementation == false) {
                return nullptr;
            }

            // Create the real AppNotificationsImplementation via the Thunder service registration
            // mechanism, which is available because we link AppNotificationsImplementation.cpp.
            // Use Core::Service to provide AddRef/Release.
            auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppNotificationsImplementation>::Create<WPEFramework::Exchange::IAppNotifications>();
            return impl;
        }

        // Test helpers
        void SetCallsign(const std::string& cs) { _callsign = cs; }
        void SetClassName(const std::string& cn) { _className = cn; }

        ResponderFake* GetGatewayResponder() const { return _gatewayResponder; }
        ResponderFake* GetInternalResponder() const { return _internalResponder; }
        NotificationHandlerFake* GetNotificationHandler() const { return _notificationHandler; }

        uint32_t GetInstantiateCount() const { return _instantiateCount.load(); }

        Config& GetConfig() { return _cfg; }

    private:
        mutable std::atomic<uint32_t> _refCount;
        std::atomic<uint32_t> _instantiateCount;
        std::string _callsign;
        std::string _className;
        Config _cfg;

        ResponderFake* _gatewayResponder;
        ResponderFake* _internalResponder;
        NotificationHandlerFake* _notificationHandler;
    };

} // namespace L0Test
