#pragma once

#include <functional>
#include <string>
#include <utility>

#include <core/core.h>
#include "UtilsAppGatewayTelemetry.h"

template <typename Owner>
class RetainedJobQueue {
private:
    class RetainedOwner {
    public:
        explicit RetainedOwner(Owner* owner)
            : mOwner(owner)
        {
            if (mOwner != nullptr) {
                mOwner->AddRef();
            }
        }

        ~RetainedOwner()
        {
            if (mOwner != nullptr) {
                mOwner->Release();
            }
        }

        RetainedOwner(const RetainedOwner&) = delete;
        RetainedOwner& operator=(const RetainedOwner&) = delete;

        Owner* Get() const { return mOwner; }

    private:
        Owner* mOwner;
    };

    class DispatchJob : public WPEFramework::Core::IDispatch,
                        public WPEFramework::Plugin::AppGatewayTelemetryHelper::JobTiming {
    public:
        DispatchJob(Owner* owner, const std::string& name, uint32_t requestId,
            uint32_t connectionId, const std::string& appId,
            std::function<void(Owner&)> work)
            : mOwner(owner)
            , mName(name)
            , mRequestId(requestId)
            , mConnectionId(connectionId)
            , mAppId(appId)
            , mWork(std::move(work))
        {
        }

        void Dispatch() override
        {
            AGW_TIME_JOB(timer, mName, mRequestId, mConnectionId, mAppId);
            Owner* owner = mOwner.Get();
            if (owner != nullptr) {
                mWork(*owner);
            }
        }

    private:
        RetainedOwner mOwner;
        std::string mName;
        uint32_t mRequestId;
        uint32_t mConnectionId;
        std::string mAppId;
        std::function<void(Owner&)> mWork;
    };

public:
    template <typename Work>
    static WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch> CreateTimed(
        Owner* owner, const std::string& name, uint32_t requestId,
        uint32_t connectionId, const std::string& appId, Work work)
    {
        if (owner == nullptr) {
            return WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
        }
        return WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>(
            WPEFramework::Core::ProxyType<DispatchJob>::Create(owner, name, requestId,
                connectionId, appId, std::function<void(Owner&)>(std::move(work))));
    }

    template <typename Work>
    static bool SubmitTimed(Owner* owner, const std::string& name,
        uint32_t requestId, uint32_t connectionId, const std::string& appId, Work work)
    {
        auto job = CreateTimed(owner, name, requestId, connectionId, appId, std::move(work));
        if (!job.IsValid()) {
            return false;
        }
        WPEFramework::Core::IWorkerPool::Instance().Submit(job);
        return true;
    }
};
