#pragma once

#include <map>
#include <string>

#include <core/core.h>

namespace L0Test {

/**
 * @brief Lightweight base service mock/fake for L0 tests.
 *
 * Many plugin L0 tests need a minimal "host service" that can return a fake
 * implementation when a plugin queries by callsign and interface ID.
 *
 * This class provides a small registry:
 *   callsign -> interfaceId -> Core::IUnknown*
 *
 * Usage:
 *   class MyServiceMock : public L0Test::L0ServiceMock { ... };
 *   auto* fake = Core::Service<MyFake>::Create<Exchange::IMyIface>();
 *   service.RegisterInterface("SomeCallsign", Exchange::IMyIface::ID, fake);
 *
 * Notes:
 * - This is intentionally NOT a full PluginHost::IShell replacement.
 * - It is meant to be embedded/extended by per-plugin ServiceMock implementations.
 * - No gmock; use simple fakes.
 */
class L0ServiceMock {
public:
    L0ServiceMock() = default;
    virtual ~L0ServiceMock() = default;

    L0ServiceMock(const L0ServiceMock&) = delete;
    L0ServiceMock& operator=(const L0ServiceMock&) = delete;

    // PUBLIC_INTERFACE
    void RegisterInterface(const std::string& callsign, const uint32_t interfaceId, WPEFramework::Core::IUnknown* instance)
    {
        /**
         * Register an interface instance for a given callsign + interfaceId.
         * The registry does not take ownership; caller controls lifetime.
         */
        _registry[callsign][interfaceId] = instance;
    }

    // PUBLIC_INTERFACE
    void UnregisterInterface(const std::string& callsign, const uint32_t interfaceId)
    {
        /** Unregister an interface instance. */
        auto it = _registry.find(callsign);
        if (it != _registry.end()) {
            it->second.erase(interfaceId);
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
         */
        auto it = _registry.find(callsign);
        if (it == _registry.end()) {
            return nullptr;
        }
        auto jt = it->second.find(interfaceId);
        if (jt == it->second.end()) {
            return nullptr;
        }
        return jt->second;
    }

private:
    std::map<std::string, std::map<uint32_t, WPEFramework::Core::IUnknown*>> _registry;
};

} // namespace L0Test
