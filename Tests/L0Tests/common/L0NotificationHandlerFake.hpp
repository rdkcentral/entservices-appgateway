#pragma once

/**
 * @file L0NotificationHandlerFake.hpp
 * @brief Reusable IAppNotificationHandler fake for L0 tests.
 *
 * This is a simple, non-GMock fake that implements IAppNotificationHandler.
 * It records calls to HandleAppEventNotifier() and allows tests to control
 * the return value and status output.
 *
 * Placed in common/ so any plugin L0 test needing IAppNotificationHandler
 * can reuse it without depending on GMock.
 */

#include <atomic>
#include <cstdint>
#include <string>

#include <core/core.h>
#include <interfaces/IAppNotifications.h>

namespace L0Test {

class NotificationHandlerFake final : public WPEFramework::Exchange::IAppNotificationHandler {
public:
    explicit NotificationHandlerFake(bool statusResult = true,
                                     uint32_t handleRc = WPEFramework::Core::ERROR_NONE)
        : _refCount(1)
        , _statusResult(statusResult)
        , _handleRc(handleRc)
    {
    }

    ~NotificationHandlerFake() override = default;

    // Core::IUnknown
    void AddRef() const override
    {
        _refCount.fetch_add(1, std::memory_order_relaxed);
    }

    uint32_t Release() const override
    {
        const uint32_t newCount = _refCount.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (newCount == 0) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return WPEFramework::Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t id) override
    {
        if (id == WPEFramework::Exchange::IAppNotificationHandler::ID) {
            AddRef();
            return static_cast<WPEFramework::Exchange::IAppNotificationHandler*>(this);
        }
        return nullptr;
    }

    // IAppNotificationHandler
    WPEFramework::Core::hresult HandleAppEventNotifier(
        IEmitter* emitCb,
        const std::string& event,
        bool listen,
        bool& status) override
    {
        lastEmitter = emitCb;
        lastEvent = event;
        lastListen = listen;
        handleCount++;

        status = _statusResult;
        return _handleRc;
    }

    // Test helpers
    void SetStatusResult(bool status) { _statusResult = status; }
    void SetHandleRc(uint32_t rc) { _handleRc = rc; }
    void Reset()
    {
        handleCount = 0;
        lastEmitter = nullptr;
        lastEvent.clear();
        lastListen = false;
    }

    // Observable state
    uint32_t handleCount{0};
    IEmitter* lastEmitter{nullptr};
    std::string lastEvent;
    bool lastListen{false};

private:
    mutable std::atomic<uint32_t> _refCount;
    bool _statusResult;
    uint32_t _handleRc;
};

} // namespace L0Test
