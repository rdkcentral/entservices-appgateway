/*
 * AppActionsServiceMock.h
 *
 * L0 test ServiceMock for AppActions plugin.
 * Implements PluginHost::IShell and ICOMLink so the plugin can:
 *   - Initialize/Deinitialize via service->Root<>()
 *   - Register/Unregister notifications
 *
 * Pattern mirrors Tests/L0Tests/AppNotifications/AppNotificationsServiceMock.h
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <mutex>
#include <algorithm>
#include <list>

#include <Module.h>
#include <core/core.h>
#include <plugins/IShell.h>

#include <interfaces/IAppActions.h>
#include <interfaces/IConfiguration.h>
#include <AppActions.h>

#include "L0TestTypes.hpp"

namespace L0Test {

using string = std::string;

// -----------------------------------------------------------------------
// Fake IAppActions::INotification
// Used to capture notification callbacks
// -----------------------------------------------------------------------
class AANotificationFake final : public WPEFramework::Exchange::IAppActions::INotification {
public:
    explicit AANotificationFake()
        : _refCount(1)
        , onActionStartRequestCount(0)
    {
    }

    ~AANotificationFake() override = default;

    void AddRef() const override
    {
        _refCount.fetch_add(1, std::memory_order_relaxed);
    }

    uint32_t Release() const override
    {
        const uint32_t n = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (0 == n) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return WPEFramework::Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t id) override
    {
        if (id == WPEFramework::Exchange::IAppActions::INotification::ID) {
            AddRef();
            return static_cast<WPEFramework::Exchange::IAppActions::INotification*>(this);
        }
        return nullptr;
    }

    // IAppActions::INotification
    void OnActionStartRequest(const string& initiator,
                              const string& intent,
                              const string& handlerAppId) override
    {
        std::lock_guard<std::mutex> lock(_mutex);
        onActionStartRequestCount++;
        lastInitiator = initiator;
        lastIntent = intent;
        lastHandlerAppId = handlerAppId;
    }

    // Observable state (access under _mutex for thread safety)
    uint32_t onActionStartRequestCount;
    string lastInitiator;
    string lastIntent;
    string lastHandlerAppId;

    mutable std::mutex _mutex;

private:
    mutable std::atomic<uint32_t> _refCount;
};

// -----------------------------------------------------------------------
// Fake RPC::IRemoteConnection
// Used for simulating plugin deactivation
// -----------------------------------------------------------------------
class AARemoteConnectionFake final : public WPEFramework::RPC::IRemoteConnection {
public:
    explicit AARemoteConnectionFake(uint32_t id = 1)
        : _refCount(1)
        , _id(id)
    {
    }

    ~AARemoteConnectionFake() override = default;

    void AddRef() const override
    {
        _refCount.fetch_add(1, std::memory_order_relaxed);
    }

    uint32_t Release() const override
    {
        const uint32_t n = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (0 == n) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return WPEFramework::Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t id) override
    {
        if (id == WPEFramework::RPC::IRemoteConnection::ID) {
            AddRef();
            return static_cast<WPEFramework::RPC::IRemoteConnection*>(this);
        }
        return nullptr;
    }

    uint32_t Id() const override
    {
        return _id;
    }

    uint32_t RemoteId() const override
    {
        return _id;
    }

    void* Acquire(const uint32_t /*waitTime*/, const string& /*className*/, const uint32_t /*interfaceId*/, const uint32_t /*version*/) override
    {
        return nullptr;
    }

    void Terminate() override
    {
        terminateCalled = true;
    }

    uint32_t Launch() override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    bool terminateCalled{false};

private:
    mutable std::atomic<uint32_t> _refCount;
    uint32_t _id;
};

// -----------------------------------------------------------------------
// Fake ICOMLink for L0 tests
// -----------------------------------------------------------------------
class AACOMFake final : public WPEFramework::PluginHost::IShell::ICOMLink {
public:
    explicit AACOMFake()
        : _refCount(1)
    {
    }

    ~AACOMFake() override = default;

    void AddRef() const override
    {
        _refCount.fetch_add(1, std::memory_order_relaxed);
    }

    uint32_t Release() const override
    {
        const uint32_t n = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (0 == n) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return WPEFramework::Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t id) override
    {
        if (id == WPEFramework::PluginHost::IShell::ICOMLink::ID) {
            AddRef();
            return static_cast<WPEFramework::PluginHost::IShell::ICOMLink*>(this);
        }
        return nullptr;
    }

    void Register(WPEFramework::RPC::IRemoteConnection::INotification* /*notification*/) override {}
    void Unregister(WPEFramework::RPC::IRemoteConnection::INotification* /*notification*/) override {}

    WPEFramework::RPC::IRemoteConnection* RemoteConnection(const uint32_t connectionId) override
    {
        if (provideRemoteConnection && connectionId == _connectionId) {
            auto* conn = new AARemoteConnectionFake(connectionId);
            return conn;
        }
        return nullptr;
    }

    void* Instantiate(const WPEFramework::RPC::Object& /*object*/,
                      const uint32_t /*waitTime*/,
                      uint32_t& connectionId) override
    {
        connectionId = _connectionId;
        if (provideImplementation) {
            // Use Core::Service to create the implementation
            auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppActionsImplementation>::Create<WPEFramework::Exchange::IAppActions>();
            return impl;
        }
        return nullptr;
    }

    // Configurable behavior
    bool provideImplementation{true};
    bool provideRemoteConnection{true};
    uint32_t _connectionId{1};

private:
    mutable std::atomic<uint32_t> _refCount;
};

// -----------------------------------------------------------------------
// AppActionsServiceMock
// Main mock implementing PluginHost::IShell for AppActions plugin testing
// -----------------------------------------------------------------------
class AppActionsServiceMock final : public WPEFramework::PluginHost::IShell {
public:
    struct Config {
        bool provideImplementation{true};
        bool provideRemoteConnection{true};
        uint32_t connectionId{1};
    };

    explicit AppActionsServiceMock(const Config& cfg = Config())
        : _refCount(1)
        , _state(WPEFramework::PluginHost::IShell::ACTIVATED)
        , _reason(WPEFramework::PluginHost::IShell::REQUESTED)
        , _comLink(new AACOMFake())
    {
        _comLink->provideImplementation = cfg.provideImplementation;
        _comLink->provideRemoteConnection = cfg.provideRemoteConnection;
        _comLink->_connectionId = cfg.connectionId;
    }

    ~AppActionsServiceMock() override
    {
        if (nullptr != _comLink) {
            _comLink->Release();
            _comLink = nullptr;
        }
    }

    void AddRef() const override
    {
        _refCount.fetch_add(1, std::memory_order_relaxed);
    }

    uint32_t Release() const override
    {
        const uint32_t n = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (0 == n) {
            delete this;
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
        if (id == WPEFramework::PluginHost::IShell::ICOMLink::ID) {
            if (nullptr != _comLink) {
                _comLink->AddRef();
                return static_cast<WPEFramework::PluginHost::IShell::ICOMLink*>(_comLink);
            }
        }
        return nullptr;
    }

    // IShell implementation
    string Versions() const override { return "1.0.0"; }
    string Locator() const override { return "libWPEFrameworkAppActionsImplementation.so"; }
    string ClassName() const override { return "AppActionsImplementation"; }
    string Callsign() const override { return "org.rdk.AppActions"; }
    string WebPrefix() const override { return "/AppActions"; }
    string ConfigLine() const override { return "{}"; }
    string PersistentPath() const override { return "/tmp/persistent"; }
    string VolatilePath() const override { return "/tmp/volatile"; }
    string DataPath() const override { return "/tmp/data"; }

    state State() const override { return _state; }

#ifdef USE_THUNDER_R4
    WPEFramework::Core::hresult Activate(const reason r) override
    {
        _state = ACTIVATED;
        _reason = r;
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult Deactivate(const reason r) override
    {
        _state = DEACTIVATED;
        _reason = r;
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult Unavailable(const reason r) override
    {
        _state = UNAVAILABLE;
        _reason = r;
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult ConfigLine(const string& /*config*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult SystemRootPath(const string& /*systemRootPath*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult Hibernate(const uint32_t /*timeout*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    string SystemPath() const override { return "/usr"; }
    string PluginPath() const override { return "/usr/lib/plugins"; }

    WPEFramework::PluginHost::IShell::startup Startup() const override
    {
        return WPEFramework::PluginHost::IShell::ACTIVATED;
    }

    WPEFramework::Core::hresult Startup(const startup /*value*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult Resumed(const bool /*value*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    WPEFramework::Core::hresult Metadata(string& /*info*/) const override
    {
        return WPEFramework::Core::ERROR_NONE;
    }
#else
    bool AutoStart() const override { return false; }
    string Version() const override { return "1.0.0"; }

    uint32_t Activate(const reason r) override
    {
        _state = ACTIVATED;
        _reason = r;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t Deactivate(const reason r) override
    {
        _state = DEACTIVATED;
        _reason = r;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t Unavailable(const reason r) override
    {
        _state = UNAVAILABLE;
        _reason = r;
        return WPEFramework::Core::ERROR_NONE;
    }

    uint8_t Major() const override { return 1; }
    uint8_t Minor() const override { return 0; }
    uint8_t Patch() const override { return 0; }

    uint32_t ConfigLine(const string& /*config*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t SystemRootPath(const string& /*systemRootPath*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t Hibernate(const string& /*processSequence*/, const uint32_t /*timeout*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    uint32_t Wakeup(const string& /*processSequence*/, const uint32_t /*timeout*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }
#endif

    bool Resumed() const override { return true; }
    bool IsSupported(const uint8_t /*version*/) const override { return true; }

    void EnableWebServer(const string& /*path*/, const string& /*prefix*/) override {}
    void DisableWebServer() override {}

    WPEFramework::PluginHost::ISubSystem* SubSystems() override { return nullptr; }
#ifndef USE_THUNDER_R4
    const WPEFramework::PluginHost::ISubSystem* SubSystems() const override { return nullptr; }
#endif

    uint32_t Submit(const uint32_t /*id*/, const WPEFramework::Core::ProxyType<WPEFramework::Core::JSON::IElement>& /*element*/) override
    {
        return WPEFramework::Core::ERROR_NONE;
    }

    void Notify(const string& /*message*/) override {}

    void* QueryInterfaceByCallsign(const uint32_t /*interfaceId*/, const string& /*callsign*/) override
    {
        return nullptr;
    }

    void Register(WPEFramework::PluginHost::IPlugin::INotification* notification) override
    {
        std::lock_guard<std::mutex> lock(_notificationMutex);
        _notifications.push_back(notification);
        notification->AddRef();
    }

    void Unregister(WPEFramework::PluginHost::IPlugin::INotification* notification) override
    {
        std::lock_guard<std::mutex> lock(_notificationMutex);
        auto it = std::find(_notifications.begin(), _notifications.end(), notification);
        if (it != _notifications.end()) {
            (*it)->Release();
            _notifications.erase(it);
        }
    }

    string Model() const override { return "TestModel"; }
    bool Background() const override { return false; }
    string Accessor() const override { return "127.0.0.1:9998"; }
    string ProxyStubPath() const override { return "/usr/lib/proxystubs"; }
    string HashKey() const override { return "testhash"; }
    string Substitute(const string& input) const override { return input; }

    WPEFramework::PluginHost::IShell::ICOMLink* COMLink() override
    {
        if (nullptr != _comLink) {
            _comLink->AddRef();
        }
        return _comLink;
    }

    reason Reason() const override { return _reason; }
    string SystemRootPath() const override { return "/"; }

    WPEFramework::RPC::IRemoteConnection* RemoteConnection(const uint32_t connectionId)
    {
        if (nullptr != _comLink) {
            return _comLink->RemoteConnection(connectionId);
        }
        return nullptr;
    }

    // Access to internal state for testing
    AACOMFake* GetCOMLink() const { return _comLink; }

private:
    mutable std::atomic<uint32_t> _refCount;
    state _state;
    reason _reason;
    AACOMFake* _comLink;
    std::mutex _notificationMutex;
    std::list<WPEFramework::PluginHost::IPlugin::INotification*> _notifications;
};

// -----------------------------------------------------------------------
// AAPluginAndService
// RAII helper that creates both the AppActions plugin shell and a service mock
// -----------------------------------------------------------------------
struct AAPluginAndService {
    WPEFramework::Plugin::AppActions* plugin{nullptr};
    AppActionsServiceMock* service{nullptr};

    explicit AAPluginAndService(const AppActionsServiceMock::Config& cfg = AppActionsServiceMock::Config())
    {
        plugin = WPEFramework::Core::Service<WPEFramework::Plugin::AppActions>::Create<WPEFramework::Plugin::AppActions>();
        service = new AppActionsServiceMock(cfg);
    }

    ~AAPluginAndService()
    {
        if (nullptr != plugin) {
            plugin->Release();
            plugin = nullptr;
        }
        if (nullptr != service) {
            service->Release();
            service = nullptr;
        }
    }

    // Non-copyable
    AAPluginAndService(const AAPluginAndService&) = delete;
    AAPluginAndService& operator=(const AAPluginAndService&) = delete;
};

} // namespace L0Test
