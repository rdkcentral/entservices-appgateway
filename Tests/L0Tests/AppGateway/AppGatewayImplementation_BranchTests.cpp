#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sys/stat.h>
#include <cstdlib>

#include <core/core.h>
#include <interfaces/IConfiguration.h>
#include <com/IIteratorType.h>

#include <AppGatewayImplementation.h>

#include "ServiceMock.h"

using WPEFramework::Core::ERROR_BAD_REQUEST;
using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_PRIVILIGED_REQUEST;
using WPEFramework::Core::ERROR_UNAVAILABLE;

namespace {

struct TestResult {
    uint32_t failures { 0 };
};

static void ExpectTrue(TestResult& tr, const bool condition, const std::string& what)
{
    if (!condition) {
        tr.failures++;
        std::cerr << "FAIL: " << what << std::endl;
    }
}

static void ExpectEqU32(TestResult& tr, const uint32_t actual, const uint32_t expected, const std::string& what)
{
    if (actual != expected) {
        tr.failures++;
        std::cerr << "FAIL: " << what << " expected=" << expected << " actual=" << actual << std::endl;
    }
}

static bool EnsureDir(const std::string& path)
{
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    // Simple mkdir -p (one level recursion)
    if (mkdir(path.c_str(), 0777) == 0) {
        return true;
    }
    const auto pos = path.find_last_of('/');
    if (pos != std::string::npos) {
        const std::string parent = path.substr(0, pos);
        if (!parent.empty() && EnsureDir(parent)) {
            return (mkdir(path.c_str(), 0777) == 0);
        }
    }
    return false;
}

static bool WriteTextFile(const std::string& path, const std::string& content)
{
    const auto slashPos = path.find_last_of('/');
    if (slashPos != std::string::npos) {
        const std::string dir = path.substr(0, slashPos);
        if (!EnsureDir(dir)) {
            return false;
        }
    }
    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }
    ofs << content;
    return true;
}

// Lightweight IStringIterator for Configure(paths)
class SimpleStringIterator : public WPEFramework::Exchange::IAppGatewayResolver::IStringIterator {
public:
    explicit SimpleStringIterator(const std::vector<std::string>& items)
        : _items(items)
        , _index(0)
        , _refCount(1)
    {
    }

    ~SimpleStringIterator() override = default;

    uint32_t AddRef() const override { return _refCount.fetch_add(1) + 1; }
    uint32_t Release() const override
    {
        const uint32_t n = _refCount.fetch_sub(1) - 1;
        if (n == 0) {
            delete this;
            return WPEFramework::Core::ERROR_DESTRUCTION_SUCCEEDED;
        }
        return WPEFramework::Core::ERROR_NONE;
    }

    void* QueryInterface(const uint32_t id) override
    {
        if (id == WPEFramework::Exchange::IAppGatewayResolver::IStringIterator::ID) {
            AddRef();
            return static_cast<WPEFramework::Exchange::IAppGatewayResolver::IStringIterator*>(this);
        }
        return nullptr;
    }

    bool Next(std::string& out) override
    {
        if (_index < _items.size()) {
            out = _items[_index++];
            return true;
        }
        return false;
    }

    bool Previous(std::string& out) override
    {
        if (_index == 0) {
            return false;
        }
        --_index;
        out = _items[_index];
        return true;
    }

    void Reset(const uint32_t position) override
    {
        if (position == 0) {
            _index = 0;
        } else if (position > _items.size()) {
            _index = static_cast<uint32_t>(_items.size());
        } else {
            _index = position;
        }
    }

    bool IsValid() const override { return (_index > 0) && (_index <= _items.size()); }
    uint32_t Count() const override { return static_cast<uint32_t>(_items.size()); }
    std::string Current() const override { return IsValid() ? _items[_index - 1] : std::string(); }

private:
    std::vector<std::string> _items;
    uint32_t _index;
    mutable std::atomic<uint32_t> _refCount;
};

static std::string ComputeRepoRoot()
{
    const std::string f = __FILE__;
    const std::string marker = "/tests/l0/appgateway/l0test/";
    const auto pos = f.rfind(marker);
    if (pos != std::string::npos) {
        return f.substr(0, pos);
    }
    return ".";
}

static std::string BaseResolutionsPath()
{
    return ComputeRepoRoot() + "/plugin/AppGateway/resolutions/resolution.base.json";
}

static WPEFramework::Exchange::GatewayContext MakeContext()
{
    WPEFramework::Exchange::GatewayContext ctx;
    ctx.requestId = 10;
    ctx.connectionId = 20;
    ctx.appId = "com.example.test";
    return ctx;
}

class EnvVarGuard final {
public:
    EnvVarGuard(const char* name, const char* value)
        : _name(name)
        , _hadOld(false)
    {
        const char* old = std::getenv(name);
        if (old != nullptr) {
            _hadOld = true;
            _oldValue = old;
        }
        if (value != nullptr) {
            setenv(name, value, 1);
        } else {
            unsetenv(name);
        }
    }

    ~EnvVarGuard()
    {
        if (_hadOld) {
            setenv(_name.c_str(), _oldValue.c_str(), 1);
        } else {
            unsetenv(_name.c_str());
        }
    }

    EnvVarGuard(const EnvVarGuard&) = delete;
    EnvVarGuard& operator=(const EnvVarGuard&) = delete;

private:
    std::string _name;
    bool _hadOld;
    std::string _oldValue;
};

static void ConfigureImplOrFail(TestResult& tr,
                                WPEFramework::Exchange::IAppGatewayResolver* impl,
                                WPEFramework::PluginHost::IShell* shell,
                                const std::vector<std::string>& paths)
{
    auto* configIfc = static_cast<WPEFramework::Exchange::IConfiguration*>(impl->QueryInterface(WPEFramework::Exchange::IConfiguration::ID));
    ExpectTrue(tr, configIfc != nullptr, "IConfiguration available on AppGatewayImplementation");
    if (configIfc != nullptr) {
        (void)configIfc->Configure(shell);
        configIfc->Release();
    }

    SimpleStringIterator* it = new SimpleStringIterator(paths);
    const uint32_t cfgRc = impl->Configure(it);
    it->Release();
    ExpectEqU32(tr, cfgRc, ERROR_NONE, "AppGatewayImplementation::Configure(paths) returns ERROR_NONE");
}

} // namespace

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayImplementation_PermissionGroup_Denied()
{
    /** Exercise permission-group path:
     *  - method requires permissionGroup (device.setName)
     *  - authenticator available but denies permission -> ERROR_PRIVILIGED_REQUEST
     */
    TestResult tr;

    auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppGatewayImplementation>::Create<WPEFramework::Exchange::IAppGatewayResolver>();
    ExpectTrue(tr, impl != nullptr, "Create AppGatewayImplementation instance");

    if (impl != nullptr) {
        L0Test::ServiceMock::Config cfg;
        cfg.provideAuthenticator = true;
        cfg.authenticatorAllowed = false;
        cfg.provideResponder = true; // ensures async respond path is schedulable

        auto* service = new L0Test::ServiceMock(cfg, true);

        ConfigureImplOrFail(tr, impl, service, { BaseResolutionsPath() });

        std::string result;
        const auto ctx = MakeContext();
        const uint32_t rc = impl->Resolve(ctx, "org.rdk.AppGateway", "device.setName", "{}", result);

        ExpectEqU32(tr, rc, ERROR_PRIVILIGED_REQUEST, "Denied permissionGroup returns ERROR_PRIVILIGED_REQUEST");

        // Verify authenticator was consulted.
        auto* auth = service->GetAuthenticatorFake();
        ExpectTrue(tr, auth != nullptr, "AuthenticatorFake cached");
        if (auth != nullptr) {
            ExpectTrue(tr, auth->checkPermissionCount > 0, "CheckPermissionGroup called");
        }

        impl->Release();
        service->Release();
    }

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayImplementation_PermissionGroup_Allowed_ComRpcDisabled()
{
    /** Exercise allowed permission-group path followed by COM-RPC disabled short-circuit. */
    TestResult tr;

    // Ensure deterministic COM-RPC disable branch is taken for this test.
    EnvVarGuard guard("APPGATEWAY_L0_DISABLE_COMRPC", "1");

    auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppGatewayImplementation>::Create<WPEFramework::Exchange::IAppGatewayResolver>();
    ExpectTrue(tr, impl != nullptr, "Create AppGatewayImplementation instance");

    if (impl != nullptr) {
        L0Test::ServiceMock::Config cfg;
        cfg.provideAuthenticator = true;
        cfg.authenticatorAllowed = true;
        cfg.provideResponder = true;

        auto* service = new L0Test::ServiceMock(cfg, true);

        ConfigureImplOrFail(tr, impl, service, { BaseResolutionsPath() });

        std::string result;
        const auto ctx = MakeContext();
        const uint32_t rc = impl->Resolve(ctx, "org.rdk.AppGateway", "device.setName", "{}", result);

        ExpectEqU32(tr, rc, ERROR_UNAVAILABLE, "Allowed permissionGroup + COM-RPC disabled returns ERROR_UNAVAILABLE");

        impl->Release();
        service->Release();
    }

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayImplementation_EventListen_TriggersNotify()
{
    /** Exercise event listen path:
     *  - method is an event (localization.onLocaleChanged)
     *  - params contains listen=true
     *  - verify Notify() called on IAppNotifications fake
     */
    TestResult tr;

    auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppGatewayImplementation>::Create<WPEFramework::Exchange::IAppGatewayResolver>();
    ExpectTrue(tr, impl != nullptr, "Create AppGatewayImplementation instance");

    if (impl != nullptr) {
        L0Test::ServiceMock::Config cfg;
        cfg.provideAppNotifications = true;
        cfg.provideResponder = true;

        auto* service = new L0Test::ServiceMock(cfg, true);

        ConfigureImplOrFail(tr, impl, service, { BaseResolutionsPath() });

        std::string resolution;
        const auto ctx = MakeContext();
        const uint32_t rc = impl->Resolve(ctx,
                                         "org.rdk.AppGateway",
                                         "localization.onLocaleChanged",
                                         "{\"listen\":true}",
                                         resolution);

        ExpectEqU32(tr, rc, ERROR_NONE, "Event listen resolve returns ERROR_NONE");
        ExpectTrue(tr, resolution.find("\"listening\":true") != std::string::npos, "Resolution includes listening=true");

        auto* notif = service->GetAppNotificationsFake();
        ExpectTrue(tr, notif != nullptr, "AppNotificationsFake cached");
        if (notif != nullptr) {
            ExpectTrue(tr, notif->notifyCount > 0, "Notify called");
            ExpectTrue(tr, notif->lastEvent == "appgateway.event.listen", "Notify event name matches");
            ExpectTrue(tr, notif->lastPayload.find("\"event\":\"localization.onLocaleChanged\"") != std::string::npos,
                       "Notify payload includes original event method");
        }

        impl->Release();
        service->Release();
    }

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayImplementation_IncludeContext_Path_Executes()
{
    /** Exercise includeContext branch (onlyAdditionalContext=false) by overriding a method to:
     *  - include additionalContext object (implies includeContext=true)
     *  - useComRpc=false so UpdateContext() inserts "context" object
     *  Alias points to dummy.method to keep offline deterministic.
     */
    TestResult tr;

    const std::string overridePath = ComputeRepoRoot() + "/tests/l0/appgateway/l0test/l0test/config/include_context.override.json";
    const std::string overrideJson = R"JSON(
{
  "resolutions": {
    "test.withContext": {
      "alias": "dummy.method",
      "useComRpc": false,
      "additionalContext": { "foo": "bar" }
    }
  }
}
)JSON";
    ExpectTrue(tr, WriteTextFile(overridePath, overrideJson), "Write includeContext override JSON");

    auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppGatewayImplementation>::Create<WPEFramework::Exchange::IAppGatewayResolver>();
    ExpectTrue(tr, impl != nullptr, "Create AppGatewayImplementation instance");

    if (impl != nullptr) {
        L0Test::ServiceMock::Config cfg;
        cfg.provideResponder = true;

        auto* service = new L0Test::ServiceMock(cfg, true);

        ConfigureImplOrFail(tr, impl, service, { BaseResolutionsPath(), overridePath });

        std::string resolution;
        const auto ctx = MakeContext();
        const uint32_t rc = impl->Resolve(ctx, "org.rdk.AppGateway", "test.withContext", "{\"x\":1}", resolution);

        ExpectEqU32(tr, rc, ERROR_NONE, "IncludeContext override resolves successfully");
        ExpectTrue(tr, !resolution.empty(), "Resolution not empty");

        impl->Release();
        service->Release();
    }

    return tr.failures;
}

// PUBLIC_INTERFACE
uint32_t Test_AppGatewayImplementation_ComRpc_RequestHandler_ReceivesAdditionalContext()
{
    /** Exercise COM-RPC handler path with UpdateContext(onlyAdditionalContext=true):
     *  - override method as useComRpc=true and has additionalContext object (so onlyAdditionalContext path builds _additionalContext)
     *  - provide RequestHandlerFake for alias callsign "org.rdk.FbSettings"
     */
    TestResult tr;

    EnvVarGuard guard("APPGATEWAY_L0_DISABLE_COMRPC", "0"); // enable COM-RPC path inside implementation

    const std::string overridePath = ComputeRepoRoot() + "/tests/l0/appgateway/l0test/l0test/config/comrpc_with_context.override.json";
    const std::string overrideJson = R"JSON(
{
  "resolutions": {
    "test.comrpc": {
      "alias": "org.rdk.FbSettings",
      "useComRpc": true,
      "additionalContext": { "scope": "l0" }
    }
  }
}
)JSON";
    ExpectTrue(tr, WriteTextFile(overridePath, overrideJson), "Write COM-RPC override JSON");

    auto* impl = WPEFramework::Core::Service<WPEFramework::Plugin::AppGatewayImplementation>::Create<WPEFramework::Exchange::IAppGatewayResolver>();
    ExpectTrue(tr, impl != nullptr, "Create AppGatewayImplementation instance");

    if (impl != nullptr) {
        L0Test::ServiceMock::Config cfg;
        cfg.provideResponder = true;
        cfg.provideRequestHandler = true;
        cfg.requestHandlerCallsign = "org.rdk.FbSettings";

        auto* service = new L0Test::ServiceMock(cfg, true);

        ConfigureImplOrFail(tr, impl, service, { BaseResolutionsPath(), overridePath });

        std::string resolution;
        const auto ctx = MakeContext();
        const uint32_t rc = impl->Resolve(ctx, "org.rdk.AppGateway", "test.comrpc", "{\"k\":1}", resolution);

        ExpectEqU32(tr, rc, ERROR_NONE, "COM-RPC handler returns ERROR_NONE");
        ExpectTrue(tr, resolution == "null" || !resolution.empty(), "Resolution present");

        auto* handler = service->GetRequestHandlerFake();
        ExpectTrue(tr, handler != nullptr, "RequestHandlerFake cached");
        if (handler != nullptr) {
            ExpectTrue(tr, handler->handleCount > 0, "HandleAppGatewayRequest called");
            // Verify _additionalContext presence (origin injected)
            ExpectTrue(tr, handler->lastPayload.find("\"_additionalContext\"") != std::string::npos, "Payload contains _additionalContext");
            ExpectTrue(tr, handler->lastPayload.find("\"origin\"") != std::string::npos, "Payload contains origin inside _additionalContext");
        }

        impl->Release();
        service->Release();
    }

    return tr.failures;
}
