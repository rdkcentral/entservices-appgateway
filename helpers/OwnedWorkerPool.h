#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include <core/core.h>
#include "UtilsAppGatewayTelemetry.h"

template <typename Owner>
class OwnedWorkerPool {
private:
    struct ChannelState {
        ChannelState()
            : closed(false)
        {
        }

        std::atomic<bool> closed;
    };

    class DispatchJob : public WPEFramework::Core::IDispatch {
    public:
        DispatchJob(const std::shared_ptr<ChannelState>& channel, const std::weak_ptr<Owner>& owner, std::function<void(Owner&)> work)
            : mChannel(channel)
            , mOwner(owner)
            , mWork(std::move(work))
        {
        }

        DispatchJob() = delete;
        DispatchJob(const DispatchJob&) = delete;
        DispatchJob& operator=(const DispatchJob&) = delete;

        void Dispatch() override
        {
            if (mChannel->closed.load(std::memory_order_acquire)) {
                return;
            }
            std::shared_ptr<Owner> owner(mOwner.lock());
            if (!owner || mChannel->closed.load(std::memory_order_acquire)) {
                return;
            }
            mWork(*owner);
        }

    private:
        std::shared_ptr<ChannelState> mChannel;
        std::weak_ptr<Owner> mOwner;
        std::function<void(Owner&)> mWork;
    };

    class TimedDispatchJob : public WPEFramework::Core::IDispatch,
                             public WPEFramework::Plugin::AppGatewayTelemetryHelper::JobTiming {
    public:
        TimedDispatchJob(const std::shared_ptr<ChannelState>& channel, const std::weak_ptr<Owner>& owner,
            const std::string& name, uint32_t requestId, uint32_t connectionId,
            const std::string& appId, std::function<void(Owner&)> work)
            : mChannel(channel)
            , mOwner(owner)
            , mName(name)
            , mRequestId(requestId)
            , mConnectionId(connectionId)
            , mAppId(appId)
            , mWork(std::move(work))
        {
        }

        TimedDispatchJob() = delete;
        TimedDispatchJob(const TimedDispatchJob&) = delete;
        TimedDispatchJob& operator=(const TimedDispatchJob&) = delete;

        void Dispatch() override
        {
            AGW_TIME_JOB(timer, mName, mRequestId, mConnectionId, mAppId);
            if (mChannel->closed.load(std::memory_order_acquire)) {
                return;
            }
            std::shared_ptr<Owner> owner(mOwner.lock());
            if (!owner || mChannel->closed.load(std::memory_order_acquire)) {
                return;
            }
            mWork(*owner);
        }

    private:
        std::shared_ptr<ChannelState> mChannel;
        std::weak_ptr<Owner> mOwner;
        std::string mName;
        uint32_t mRequestId;
        uint32_t mConnectionId;
        std::string mAppId;
        std::function<void(Owner&)> mWork;
    };

public:
    OwnedWorkerPool()
        : mChannel(std::make_shared<ChannelState>())
    {
    }

    OwnedWorkerPool(const OwnedWorkerPool&) = delete;
    OwnedWorkerPool& operator=(const OwnedWorkerPool&) = delete;

    template <typename Work>
    WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch> Create(const std::shared_ptr<Owner>& owner, Work work)
    {
        if (!owner || mChannel->closed.load(std::memory_order_acquire)) {
            return WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
        }
        return WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>(WPEFramework::Core::ProxyType<DispatchJob>::Create(
            mChannel, std::weak_ptr<Owner>(owner), std::function<void(Owner&)>(std::move(work))));
    }

    template <typename Work>
    bool Submit(const std::shared_ptr<Owner>& owner, Work work)
    {
        WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch> job(Create(owner, std::move(work)));
        if (!job.IsValid()) {
            return false;
        }
        WPEFramework::Core::IWorkerPool::Instance().Submit(job);
        return true;
    }

    template <typename Work>
    WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch> CreateTimed(
        const std::shared_ptr<Owner>& owner, const std::string& name,
        uint32_t requestId, uint32_t connectionId, const std::string& appId, Work work)
    {
        if (!owner || mChannel->closed.load(std::memory_order_acquire)) {
            return WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
        }
        return WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>(
            WPEFramework::Core::ProxyType<TimedDispatchJob>::Create(
                mChannel, std::weak_ptr<Owner>(owner), name, requestId,
                connectionId, appId, std::function<void(Owner&)>(std::move(work))));
    }

    template <typename Work>
    bool SubmitTimed(const std::shared_ptr<Owner>& owner, const std::string& name,
        uint32_t requestId, uint32_t connectionId, const std::string& appId, Work work)
    {
        WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch> job(
            CreateTimed(owner, name, requestId, connectionId, appId, std::move(work)));
        if (!job.IsValid()) {
            return false;
        }
        WPEFramework::Core::IWorkerPool::Instance().Submit(job);
        return true;
    }

    template <typename Work>
    bool Submit(Owner*, Work) = delete;

    template <typename Work>
    bool Submit(Owner&, Work) = delete;

    void Close()
    {
        mChannel->closed.store(true, std::memory_order_release);
    }

    bool Closed() const
    {
        return mChannel->closed.load(std::memory_order_acquire);
    }

private:
    std::shared_ptr<ChannelState> mChannel;
};
