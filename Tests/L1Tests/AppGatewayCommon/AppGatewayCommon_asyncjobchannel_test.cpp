#include <gtest/gtest.h>

#include "OwnedWorkerPool.h"
#include "RetainedJobQueue.h"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {

struct L1SharedOwner {
    std::atomic<uint32_t> calls{0};
};

struct L1RetainedOwner {
    uint32_t AddRef() const { ++addRefs; return ++references; }
    uint32_t Release() const { ++releases; return --references; }
    mutable std::atomic<uint32_t> references{1};
    mutable std::atomic<uint32_t> addRefs{0};
    mutable std::atomic<uint32_t> releases{0};
};

TEST(RetainedJobQueueL1, EarlyReturnReleasesOwner)
{
    L1RetainedOwner owner;
    auto job = RetainedJobQueue<L1RetainedOwner>::CreateTimed(
        &owner, "EarlyReturn", 1, 2, "app", [](L1RetainedOwner&) { return; });
    ASSERT_TRUE(job.IsValid());
    job->Dispatch();
    job = WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
    EXPECT_EQ(1u, owner.addRefs.load());
    EXPECT_EQ(1u, owner.releases.load());
}

TEST(RetainedJobQueueL1, ExceptionReleasesOwner)
{
    L1RetainedOwner owner;
    auto job = RetainedJobQueue<L1RetainedOwner>::CreateTimed(
        &owner, "Exception", 1, 2, "app", [](L1RetainedOwner&) { throw std::runtime_error("expected"); });
    ASSERT_THROW(job->Dispatch(), std::runtime_error);
    job = WPEFramework::Core::ProxyType<WPEFramework::Core::IDispatch>();
    EXPECT_EQ(1u, owner.addRefs.load());
    EXPECT_EQ(1u, owner.releases.load());
}

TEST(OwnedWorkerPoolL1, QueuedJobDoesNotRetainOwner)
{
    OwnedWorkerPool<L1SharedOwner> channel;
    std::shared_ptr<L1SharedOwner> owner(std::make_shared<L1SharedOwner>());
    std::weak_ptr<L1SharedOwner> observer(owner);
    auto job = channel.Create(owner, [](L1SharedOwner& value) { ++value.calls; });
    ASSERT_TRUE(job.IsValid());
    owner.reset();
    EXPECT_TRUE(observer.expired());
    job->Dispatch();
    EXPECT_TRUE(observer.expired());
}

TEST(OwnedWorkerPoolL1, CloseDiscardsPreviouslyQueuedJob)
{
    OwnedWorkerPool<L1SharedOwner> channel;
    std::shared_ptr<L1SharedOwner> owner(std::make_shared<L1SharedOwner>());
    auto job = channel.Create(owner, [](L1SharedOwner& value) { ++value.calls; });
    channel.Close();
    job->Dispatch();
    EXPECT_EQ(0u, owner->calls.load());
}

TEST(OwnedWorkerPoolL1, RunningCallbackRetainsOwnerWithoutBlockingClose)
{
    OwnedWorkerPool<L1SharedOwner> channel;
    std::shared_ptr<L1SharedOwner> owner(std::make_shared<L1SharedOwner>());
    std::weak_ptr<L1SharedOwner> observer(owner);
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool release = false;
    auto job = channel.Create(owner, [&](L1SharedOwner& value) {
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
    channel.Close();
    owner.reset();
    EXPECT_FALSE(observer.expired());
    {
        std::lock_guard<std::mutex> lock(mutex);
        release = true;
    }
    condition.notify_all();
    worker.join();
    EXPECT_TRUE(observer.expired());
}

}
