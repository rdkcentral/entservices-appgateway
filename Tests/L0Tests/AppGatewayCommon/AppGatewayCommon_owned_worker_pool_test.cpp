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

struct CountedWork {
    CountedWork(int& copies, int& moves) : copies(copies), moves(moves) {}
    CountedWork(const CountedWork& other) : copies(other.copies), moves(other.moves) { ++copies; }
    CountedWork(CountedWork&& other) : copies(other.copies), moves(other.moves) { ++moves; }
    void operator()(TestRetainedOwner&) const {}
    int& copies;
    int& moves;
    char payload[136]{};
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
    int copies = 0;
    int moves = 0;
    CountedWork work(copies, moves);
    auto copied = RetainedJobQueue<TestRetainedOwner>::CreateTimed(&owner, "Lvalue", 1, 2, "app", work);
    ExpectTrue(tr, copied.IsValid(), "lvalue callable creates job");
    ExpectEqU32(tr, copies, 1, "lvalue callable copied once into storage");
    ExpectEqU32(tr, moves, 0, "lvalue callable has no intermediate move");
    copies = moves = 0;
    auto moved = RetainedJobQueue<TestRetainedOwner>::CreateTimed(&owner, "Rvalue", 1, 2, "app", std::move(work));
    ExpectTrue(tr, moved.IsValid(), "rvalue callable creates job");
    ExpectEqU32(tr, copies, 0, "rvalue callable is not copied");
    ExpectEqU32(tr, moves, 1, "rvalue callable moved once into storage");
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

uint32_t Test_SettingsCleanup_RetainedChildrenOwnShellAndCloseEvents()
{
    L0Test::ServiceMock shell;
    auto references = [&]() -> uint32_t {
        const auto count = shell.AddRef();
        shell.Release();
        return count - 1;
    };
    struct Emitter : Exchange::IAppNotificationHandler::IEmitter {
        uint32_t AddRef() const override { return ++references; }
        uint32_t Release() const override { return --references; }
        void* QueryInterface(uint32_t) override { return nullptr; }
        void Emit(const std::string&, const std::string&, const std::string&) override { ++calls; }
        mutable uint32_t references{1};
        uint32_t calls{0};
    } emitter;
    TestResult tr;
    {
        SettingsDelegate settings;
        settings.setShell(&shell);
        std::vector<std::shared_ptr<BaseEventDelegate>> children = {
            settings.getSystemDelegate(), settings.getNetworkDelegate(), settings.getUserSettings(),
            settings.getLifecycleDelegate(), settings.getTTSDelegate(), settings.getAvOutputDelegate(),
            settings.getVideoOutputDelegate()};
        std::vector<WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>> jobs;
        for (const auto& child : children) {
            child->AddNotification("lifetime", &emitter);
            jobs.push_back(BaseEventDelegate::EventDelegateDispatchJob::Create(child, "lifetime", "old", "app"));
        }
        settings.Cleanup();
        settings.Cleanup();
        ExpectEqU32(tr, references(), 8, "retained children each retain the shell");
        for (size_t i = 0; i < children.size(); ++i) {
            jobs[i]->Dispatch();
            children[i]->AddNotification("late", &emitter);
            ExpectTrue(tr, !children[i]->IsNotificationRegistered("late"), "closed child rejects late registration");
            ExpectTrue(tr, !children[i]->Dispatch("lifetime", "new"), "closed child rejects event submission");
        }
        ExpectEqU32(tr, emitter.calls, 0, "old notification jobs are discarded while children survive");
        ExpectEqU32(tr, emitter.references, 8, "cleanup preserves existing references for admitted callbacks");
    }
    ExpectEqU32(tr, emitter.references, 1, "final destruction balances emitter references");
    ExpectEqU32(tr, references(), 1, "final child release balances shell references");
    return tr.failures;
}
