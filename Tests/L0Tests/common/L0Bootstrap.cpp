#include "L0Bootstrap.hpp"

namespace L0Test {

namespace {

class TestOnlyDispatcher final : public WPEFramework::Core::ThreadPool::IDispatcher {
public:
    TestOnlyDispatcher() = default;
    ~TestOnlyDispatcher() override = default;

    TestOnlyDispatcher(const TestOnlyDispatcher&) = delete;
    TestOnlyDispatcher& operator=(const TestOnlyDispatcher&) = delete;

    void Initialize() override
    {
        // No-op for L0 tests.
    }

    void Deinitialize() override
    {
        // No-op for L0 tests.
    }

    void Dispatch(WPEFramework::Core::IDispatch* job) override
    {
        if (job != nullptr) {
            job->Dispatch();
        }
    }
};

static bool g_ownsWorkerPool = false;
static TestOnlyDispatcher* g_dispatcher = nullptr;
static WPEFramework::Core::WorkerPool* g_workerPool = nullptr;

} // namespace

// PUBLIC_INTERFACE
void L0Init()
{
    /** Initialize a minimal Core::WorkerPool if none is assigned yet. */
    if (WPEFramework::Core::IWorkerPool::IsAvailable() == false) {
        const uint8_t threadCount = 2;
        const uint32_t stackSize = 0;  // default stack size
        const uint32_t queueSize = 64; // sufficient for L0 tests

        g_dispatcher = new TestOnlyDispatcher();
        g_workerPool = new WPEFramework::Core::WorkerPool(threadCount, stackSize, queueSize, g_dispatcher);

        WPEFramework::Core::IWorkerPool::Assign(g_workerPool);
        g_workerPool->Run();

        g_ownsWorkerPool = true;
    }
}

// PUBLIC_INTERFACE
void L0Shutdown()
{
    /**
     * Stop and destroy the worker pool only if this module created it.
     * Always dispose remaining Core singletons to reduce shutdown noise and leaks.
     */
    if (g_ownsWorkerPool == true) {
        if (g_workerPool != nullptr) {
            g_workerPool->Stop();
        }

        WPEFramework::Core::IWorkerPool::Assign(nullptr);

        delete g_workerPool;
        g_workerPool = nullptr;

        delete g_dispatcher;
        g_dispatcher = nullptr;

        g_ownsWorkerPool = false;
    }

    WPEFramework::Core::Singleton::Dispose();
}

L0BootstrapGuard::L0BootstrapGuard()
    : _initialized(false)
{
    L0Init();
    _initialized = true;
}

L0BootstrapGuard::~L0BootstrapGuard()
{
    if (_initialized) {
        L0Shutdown();
    }
}

} // namespace L0Test
