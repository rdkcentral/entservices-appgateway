#include <gtest/gtest.h>

#include "SharedOwnerChannel.h"
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

struct CountedWork {
    CountedWork(int& copies, int& moves) : copies(copies), moves(moves) {}
    CountedWork(const CountedWork& other) : copies(other.copies), moves(other.moves) { ++copies; }
    CountedWork(CountedWork&& other) : copies(other.copies), moves(other.moves) { ++moves; }
    void operator()(L1RetainedOwner&) const {}
    int& copies;
    int& moves;
    char payload[136]{};
};

TEST(RetainedJobQueueL1, CreateTimedForwardsLargeCallable)
{
    L1RetainedOwner owner;
    int copies = 0;
    int moves = 0;
    CountedWork work(copies, moves);
    auto copied = RetainedJobQueue<L1RetainedOwner>::CreateTimed(&owner, "Lvalue", 1, 2, "app", work);
    ASSERT_TRUE(copied.IsValid());
    EXPECT_EQ(1, copies);
    EXPECT_EQ(0, moves);
    copies = moves = 0;
    auto moved = RetainedJobQueue<L1RetainedOwner>::CreateTimed(&owner, "Rvalue", 1, 2, "app", std::move(work));
    ASSERT_TRUE(moved.IsValid());
    EXPECT_EQ(0, copies);
    EXPECT_EQ(1, moves);
    copied->Dispatch();
    moved->Dispatch();
}

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

TEST(SharedOwnerChannelL1, QueuedJobDoesNotRetainOwner)
{
    SharedOwnerChannel<L1SharedOwner> channel;
    std::shared_ptr<L1SharedOwner> owner(std::make_shared<L1SharedOwner>());
    std::weak_ptr<L1SharedOwner> observer(owner);
    auto job = channel.Create(owner, [](L1SharedOwner& value) { ++value.calls; });
    ASSERT_TRUE(job.IsValid());
    owner.reset();
    EXPECT_TRUE(observer.expired());
    job->Dispatch();
    EXPECT_TRUE(observer.expired());
}

TEST(SharedOwnerChannelL1, CloseDiscardsPreviouslyQueuedJob)
{
    SharedOwnerChannel<L1SharedOwner> channel;
    std::shared_ptr<L1SharedOwner> owner(std::make_shared<L1SharedOwner>());
    auto job = channel.Create(owner, [](L1SharedOwner& value) { ++value.calls; });
    channel.Close();
    job->Dispatch();
    EXPECT_EQ(0u, owner->calls.load());
}

TEST(SharedOwnerChannelL1, RunningCallbackRetainsOwnerWithoutBlockingClose)
{
    SharedOwnerChannel<L1SharedOwner> channel;
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
