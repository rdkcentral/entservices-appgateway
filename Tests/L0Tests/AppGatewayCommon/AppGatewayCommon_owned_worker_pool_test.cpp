#include "AppGatewayCommon_common_test.h"
#include "SharedOwnerChannel.h"
#include "RetainedJobQueue.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace {

struct TestOwner {
    int calls{0};
};

struct TestRetainedOwner {
    uint32_t AddRef() const { ++addRefs; return ++references; }
    uint32_t Release() const { ++releases; return --references; }
    mutable std::atomic<uint32_t> references{1};
    mutable std::atomic<uint32_t> addRefs{0};
    mutable std::atomic<uint32_t> releases{0};
    std::atomic<uint32_t> calls{0};
};

}

uint32_t Test_SharedOwnerChannel_ClosedChannelRejectsSubmission()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> pool;
    std::shared_ptr<TestOwner> owner(std::make_shared<TestOwner>());
    pool.Close();
    auto job = pool.Create(owner, [](TestOwner& value) { ++value.calls; });
    ExpectTrue(tr, !job.IsValid(), "closed channel rejects job creation");
    ExpectEqU32(tr, owner->calls, 0, "rejected work does not invoke owner");
    return tr.failures;
}

uint32_t Test_SharedOwnerChannel_ClosedChannelDiscardsQueuedJob()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> pool;
    std::shared_ptr<TestOwner> owner(std::make_shared<TestOwner>());
    auto job = pool.Create(owner, [](TestOwner& value) { ++value.calls; });
    ExpectTrue(tr, job.IsValid(), "open channel creates queued job");
    pool.Close();
    job->Dispatch();
    ExpectEqU32(tr, owner->calls, 0, "closed channel discards queued job");
    return tr.failures;
}

uint32_t Test_SharedOwnerChannel_ExpiredOwnerDiscardsQueuedJob()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> pool;
    std::shared_ptr<TestOwner> owner(std::make_shared<TestOwner>());
    std::weak_ptr<TestOwner> observer(owner);
    auto job = pool.Create(owner, [](TestOwner& value) { ++value.calls; });
    owner.reset();
    ExpectTrue(tr, observer.expired(), "queued job does not retain owner");
    job->Dispatch();
    ExpectTrue(tr, observer.expired(), "expired owner remains discarded after dispatch");
    return tr.failures;
}

uint32_t Test_SharedOwnerChannel_OpenChannelDispatchesWithRetainedOwner()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> pool;
    std::shared_ptr<TestOwner> owner(std::make_shared<TestOwner>());
    auto job = pool.Create(owner, [](TestOwner& value) { ++value.calls; });
    job->Dispatch();
    ExpectEqU32(tr, owner->calls, 1, "open channel dispatch invokes retained owner");
    return tr.failures;
}

uint32_t Test_SharedOwnerChannel_CloseIsIdempotentAndNonblocking()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> pool;
    pool.Close();
    pool.Close();
    ExpectTrue(tr, pool.Closed(), "repeated close leaves channel closed");
    return tr.failures;
}

uint32_t Test_SharedOwnerChannel_RunningJobRetainsOwnerAfterClose()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> pool;
    std::shared_ptr<TestOwner> owner(std::make_shared<TestOwner>());
    std::weak_ptr<TestOwner> observer(owner);
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool release = false;
    auto job = pool.Create(owner, [&](TestOwner& value) {
        std::unique_lock<std::mutex> lock(mutex);
        entered = true;
        condition.notify_all();
        condition.wait(lock, [&] { return release; });
        ++value.calls;
    });

    std::thread worker([&] { job->Dispatch(); });
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&] { return entered; });
    }
    pool.Close();
    owner.reset();
    ExpectTrue(tr, !observer.expired(), "running job retains owner after close");
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    condition.notify_all();
    worker.join();
    ExpectTrue(tr, observer.expired(), "owner releases when running callback completes");
    return tr.failures;
}

uint32_t Test_RetainedJobQueue_AllMigratedJobNamesBalanceOwnerReference()
{
    TestResult tr;
    const char* names[] = {
        "EventRegistrationJob[sub:test]", "NetworkWorkerPoolTask", "WsMsgJob[test]",
        "ConnStatusJob[connect]", "ConnStatusJob[disconnect]", "RespondJob",
        "EmitJob[test]", "RequestJob[test]", "ImplRespondJob", "EventHookJob[test]",
        "SubscriberJob[sub:test]", "SubscriberJob[unsub:test]", "NotifEmitJob[test]",
        "NotifyJob[test]", "EventDispatchJob[test]"
    };
    for (const char* name : names) {
        TestRetainedOwner owner;
        auto job = RetainedJobQueue<TestRetainedOwner>::CreateTimed(
            &owner, name, 1, 2, "app", [](TestRetainedOwner& value) { ++value.calls; });
        job->Dispatch();
        job = WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
        ExpectEqU32(tr, owner.calls.load(), 1, name);
        ExpectEqU32(tr, owner.addRefs.load(), 1, name);
        ExpectEqU32(tr, owner.releases.load(), 1, name);
    }
    return tr.failures;
}

uint32_t Test_RetainedJobQueue_BalancesOwnerReference()
{
    TestResult tr;
    TestRetainedOwner owner;
    auto job = RetainedJobQueue<TestRetainedOwner>::CreateTimed(
        &owner, "RetainedJob", 1, 2, "app", [](TestRetainedOwner& value) { ++value.calls; });
    ExpectEqU32(tr, owner.addRefs.load(), 1, "queued retained job acquires owner reference");
    job->Dispatch();
    job = WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
    ExpectEqU32(tr, owner.calls.load(), 1, "retained job invokes callback");
    ExpectEqU32(tr, owner.releases.load(), 1, "job destruction releases owner reference");
    return tr.failures;
}

uint32_t Test_SharedOwnerChannel_OldJobStaysCanceledAfterReopenReplacement()
{
    TestResult tr;
    SharedOwnerChannel<TestOwner> oldPool;
    std::shared_ptr<TestOwner> oldOwner(std::make_shared<TestOwner>());
    auto oldJob = oldPool.Create(oldOwner, [](TestOwner& value) { ++value.calls; });
    oldPool.Close();

    SharedOwnerChannel<TestOwner> newPool;
    std::shared_ptr<TestOwner> newOwner(std::make_shared<TestOwner>());
    auto newJob = newPool.Create(newOwner, [](TestOwner& value) { ++value.calls; });
    oldJob->Dispatch();
    newJob->Dispatch();

    ExpectEqU32(tr, oldOwner->calls, 0, "old closed channel continues to discard queued work");
    ExpectEqU32(tr, newOwner->calls, 1, "replacement open channel dispatches new work");
    return tr.failures;
}
