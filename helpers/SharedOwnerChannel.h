#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#include <core/core.h>
#include "UtilsAppGatewayTelemetry.h"

/**
 * SharedOwnerChannel
 *
 * A job submission helper for std::shared_ptr-managed owners.
 *
 * LIFECYCLE OVERVIEW
 * ==================
 *
 * 1. JOB CREATION AND SUBMISSION
 *    ---------------------------
 *    - Caller owns a std::shared_ptr<Owner>
 *    - Caller submits work with Submit(shared_ptr, callback)
 *    - Helper creates a DispatchJob envelope
 *    - Envelope stores std::weak_ptr<Owner> (NOT shared_ptr)
 *    - Envelope is submitted to Thunder WorkerPool
 *
 *    Key point: The envelope does NOT hold a strong reference.
 *    The owner can be destroyed while the job is queued.
 *
 * 2. OWNER LIFECYCLE WHILE PENDING IN QUEUE
 *    --------------------------------------
 *    - External owner resets its shared_ptr
 *    - Envelope still holds only weak_ptr
 *    - Owner destructor CAN run immediately
 *    - No blocking: shutdown does not wait for queued jobs
 *
 * 3. DISPATCH TIME
 *    ------------
 *    - WorkerPool calls DispatchJob::Dispatch()
 *    - Check closed flag → if true, discard
 *    - weak_ptr.lock() attempts to obtain shared_ptr
 *      - If owner destroyed → lock() returns nullptr → discard
 *      - If owner alive → lock() returns shared_ptr → proceed
 *    - Invoke callback with Owner&
 *    - shared_ptr released when callback completes
 *
 * 4. OWNER SHUTDOWN CASES
 *    --------------------
 *    Case A: Close before dispatch
 *    - Owner calls Close()
 *    - Sets closed flag to true
 *    - Job dispatched later
 *    - Dispatch sees closed=true → discards without callback
 *
 *    Case B: Owner destroyed before dispatch
 *    - External owner resets shared_ptr
 *    - Owner destructor runs
 *    - Job dispatched later
 *    - weak_ptr.lock() returns nullptr → discards
 *
 *    Case C: Dispatch wins race with Close
 *    - Dispatch locks weak_ptr successfully
 *    - Owner calls Close() during callback
 *    - Callback continues (owner still alive via temporary shared_ptr)
 *    - Double-check after lock prevents late dispatch
 *
 * WHEN TO USE
 * ===========
 * Use SharedOwnerChannel when:
 * - Owner is managed by std::shared_ptr
 * - Owner has std::enable_shared_from_this
 * - You want non-blocking shutdown (owner destroyable while queued)
 * - You have a Close() hook (destructor or Deinitialize)
 *
 * DO NOT USE when:
 * - Owner uses intrusive AddRef()/Release() → use IntrusiveOwnerChannel
 * - Owner has no Close() hook and you accept deferred execution → use RetainedJobQueue
 */
template <typename Owner>
class SharedOwnerChannel {
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
    SharedOwnerChannel()
        : mChannel(std::make_shared<ChannelState>())
    {
    }

    SharedOwnerChannel(const SharedOwnerChannel&) = delete;
    SharedOwnerChannel& operator=(const SharedOwnerChannel&) = delete;

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
