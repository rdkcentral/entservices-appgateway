#pragma once

#include <map>
#include <string>

#include <core/core.h>

namespace L0Test {

/**
 * @brief Lightweight base service mock for L0 tests.
 *
 * Many plugin L0 tests need a minimal "host service" that can return a mock
 * implementation when a plugin queries by callsign and interface ID.
 *
 * This class provides a small registry:
 *   callsign -> interfaceId -> Core::IUnknown*
 *
 * Usage:
 *   class MyServiceMock : public L0Test::L0ServiceMock { ... };
 *   auto* mock = Core::Service<MyMock>::Create<Exchange::IMyIface>();
 *   service.RegisterInterface("SomeCallsign", Exchange::IMyIface::ID, mock);
 *
 * Notes:
 * - This is intentionally NOT a full PluginHost::IShell replacement.
 * - It is meant to be embedded/extended by per-plugin ServiceMock implementations.
 * - No gmock; use simple mocks.
 */
class L0ServiceMock {
public:
    L0ServiceMock() = default;
    virtual ~L0ServiceMock()
    {
        ClearRegistry();
    }

    L0ServiceMock(const L0ServiceMock&) = delete;
    L0ServiceMock& operator=(const L0ServiceMock&) = delete;

    // PUBLIC_INTERFACE
    void RegisterInterface(const std::string& callsign, const uint32_t interfaceId, WPEFramework::Core::IUnknown* instance)
    {
        /**
         * Register an interface instance for a given callsign + interfaceId.
         * The registry owns one COM reference to the stored interface.
         * A nullptr instance is treated as unregister for this key.
         */
        if (instance == nullptr) {
            UnregisterInterface(callsign, interfaceId);
            return;
        }

        instance->AddRef();

        auto& byId = _registry[callsign];
        auto it = byId.find(interfaceId);
        if (it != byId.end()) {
            if (it->second != nullptr) {
                it->second->Release();
            }
            it->second = instance;
            return;
        }

        byId.emplace(interfaceId, instance);
    }

    // PUBLIC_INTERFACE
    void UnregisterInterface(const std::string& callsign, const uint32_t interfaceId)
    {
        /** Unregister an interface instance. */
        auto it = _registry.find(callsign);
        if (it != _registry.end()) {
            auto jt = it->second.find(interfaceId);
            if (jt != it->second.end()) {
                if (jt->second != nullptr) {
                    jt->second->Release();
                }
                it->second.erase(jt);
            }
            if (it->second.empty()) {
                _registry.erase(it);
            }
        }
    }

    // PUBLIC_INTERFACE
    WPEFramework::Core::IUnknown* QueryInterfaceByCallsign(const std::string& callsign, const uint32_t interfaceId) const
    {
        /**
         * Return the registered interface for (callsign, interfaceId) or nullptr.
         * This is a helper used by per-plugin ServiceMock implementations.
         * Caller receives an AddRef'd pointer and is expected to Release().
         */
        auto it = _registry.find(callsign);
        if (it == _registry.end()) {
            return nullptr;
        }
        auto jt = it->second.find(interfaceId);
        if (jt == it->second.end()) {
            return nullptr;
        }
        if (jt->second != nullptr) {
            jt->second->AddRef();
        }
        return jt->second;
    }

private:
    void ClearRegistry()
    {
        for (auto& byCallsign : _registry) {
            for (auto& byInterface : byCallsign.second) {
                if (byInterface.second != nullptr) {
                    byInterface.second->Release();
                }
            }
        }
        _registry.clear();
    }

    std::map<std::string, std::map<uint32_t, WPEFramework::Core::IUnknown*>> _registry;
};

} // namespace L0Test
