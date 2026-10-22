#pragma once

#include <functional>
#include <string>
#include <utility>

#include <core/core.h>
#include "UtilsAppGatewayTelemetry.h"

/**
 * RetainedJobQueue
 *
 * A self-retaining job submission helper for intrusive AddRef()/Release() owners.
 *
 * LIFECYCLE OVERVIEW
 * ==================
 *
 * 1. JOB CREATION AND SUBMISSION
 *    ---------------------------
 *    - Caller has Owner* (intrusive reference counting)
 *    - Caller submits work with SubmitTimed(owner, callback)
 *    - Helper creates a DispatchJob envelope
 *    - Envelope calls owner->AddRef() IMMEDIATELY at submission
 *    - Envelope is submitted to Thunder WorkerPool
 *
 *    Key point: The envelope DOES call AddRef() at submission.
 *    The owner CANNOT be destroyed while the job is queued.
 *
 * 2. OWNER LIFECYCLE WHILE PENDING IN QUEUE
 *    --------------------------------------
 *    - External owner releases its reference
 *    - Envelope still holds a retained reference (AddRef was called)
 *    - Owner destructor CANNOT run until job dispatches
 *    - Blocking: shutdown waits for queued jobs to complete
 *
 * 3. DISPATCH TIME
 *    ------------
 *    - WorkerPool calls DispatchJob::Dispatch()
 *    - Check if owner pointer is non-null
 *      - If nullptr → discard (should not happen if AddRef succeeded)
 *      - If valid → invoke callback with Owner&
 *    - Envelope destructor calls owner->Release()
 *    - If this was the last reference, owner destructor runs
 *
 * 4. OWNER SHUTDOWN CASES
 *    --------------------
 *    Case A: Owner releases reference before dispatch
 *    - External owner calls Release()
 *    - Envelope still holds reference from AddRef()
 *    - Owner destructor CANNOT run
 *    - Job dispatched later
 *    - Callback runs (owner is still alive)
 *    - Envelope Release() may trigger final destruction
 *
 *    Case B: Owner releases after dispatch
 *    - Job dispatched, callback runs
 *    - Envelope Release() after callback
 *    - If no other references, owner destructor runs
 *
 *    Case C: No cancellation
 *    - There is NO Close() method
 *    - There is NO closed flag
 *    - Jobs are NEVER discarded at dispatch time
 *    - All submitted jobs will run
 *
 * WHEN TO USE
 * ===========
 * Use RetainedJobQueue when:
 * - Owner has AddRef()/Release() (intrusive reference counting)
 * - Owner has NO safe Close() hook (cannot add interface method)
 * - You accept that jobs will run after logical shutdown
 * - Resources are destructor-bound (released only in ~Owner)
 * - Callbacks have their own resource-lifecycle checks
 *
 * DO NOT USE when:
 * - Owner is managed by std::shared_ptr → use SharedOwnerChannel
 * - Owner has a Close() hook and you want cancellation → use IntrusiveOwnerChannel
 * - You need non-blocking shutdown → use IntrusiveOwnerChannel instead
 *
 * IMPORTANT LIMITATIONS
 * =====================
 * - Queued jobs WILL run after logical shutdown (no cancellation)
 * - Callbacks must tolerate resources released during Deinitialize()
 * - If resources are released before destructor, callbacks need null/state checks
 * - Owner destruction is BLOCKED until all queued jobs dispatch
 */
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
        uint32_t requestId, uint32_t connectionId, const std::string& appId, Work&& work)
    {
        auto job = CreateTimed(owner, name, requestId, connectionId, appId, std::forward<Work>(work));
        if (!job.IsValid()) {
            return false;
        }
        WPEFramework::Core::IWorkerPool::Instance().Submit(job);
        return true;
    }
};
