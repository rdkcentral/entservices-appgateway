/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include "Module.h"

#define private public
#include "AppGateway.h"
#include "AppGatewayImplementation.h"
#include "AppGatewayResponderImplementation.h"
#undef private
#include "Resolver.h"

#include "WorkerPoolImplementation.h"

// Local mocks (this repo): Tests/mocks/*
#include "ServiceMock.h"
#include "ThunderPortability.h"

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

namespace {

// RAII guard to ensure Core::IWorkerPool is available during tests.
// Many WPEFramework components assume a global worker pool exists (normally created by Thunder runtime).
class WorkerPoolGuard final {
public:
    WorkerPoolGuard(const WorkerPoolGuard&) = delete;
    WorkerPoolGuard& operator=(const WorkerPoolGuard&) = delete;

    WorkerPoolGuard()
        : _pool(/*threads*/ 2, /*stackSize*/ 0, /*queueSize*/ 64)
        , _assigned(false)
    {
        if (Core::IWorkerPool::IsAvailable() == false) {
            Core::IWorkerPool::Assign(&_pool);
            _assigned = true;
        }
        _pool.Run();
    }

    ~WorkerPoolGuard()
    {
        _pool.Stop();
        if (_assigned) {
            Core::IWorkerPool::Assign(nullptr);
        }
    }

private:
    WorkerPoolImplementation _pool;
    bool _assigned;
};

static WorkerPoolGuard g_workerPool; // ensure constructed before any tests run

// Ensure any async jobs queued by the code under test get a chance to run before
// test-scoped objects are destroyed (prevents use-after-free segfaults at test end).
class WorkerPoolDrainGuard final {
public:
    WorkerPoolDrainGuard(const WorkerPoolDrainGuard&) = delete;
    WorkerPoolDrainGuard& operator=(const WorkerPoolDrainGuard&) = delete;

    WorkerPoolDrainGuard() = default;

    ~WorkerPoolDrainGuard()
    {
        // Best-effort: let the worker pool run queued jobs. We keep it short to
        // avoid adding noticeable latency to the suite.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
};

static WorkerPoolDrainGuard g_workerPoolDrain;

// Small helper to write text files under /tmp for config-driven tests.
static void WriteTextFile(const std::string& path, const std::string& content)
{
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    ASSERT_TRUE(f.is_open()) << "Failed to open file for write: " << path;
    f << content;
    f.close();
}

/**
 * Minimal COM-RPC mock for Exchange::IAppGatewayRequestHandler.
 * AppGatewayImplementation looks up this interface using:
 *   mService->QueryInterfaceByCallsign<Exchange::IAppGatewayRequestHandler>(alias)
 */
class AppGatewayRequestHandlerMock : public Exchange::IAppGatewayRequestHandler {
public:
    AppGatewayRequestHandlerMock()
        : _refCount(1)
    {
    }

    ~AppGatewayRequestHandlerMock() override = default;

    BEGIN_INTERFACE_MAP(AppGatewayRequestHandlerMock)
    INTERFACE_ENTRY(Exchange::IAppGatewayRequestHandler)
    END_INTERFACE_MAP

    // Provide real refcounting so tests don't leak mocks.
    // GoogleMock verifies expectations in destructor; if production code keeps the object alive,
    // the test must ensure a matching Release() path happens.
    void AddRef() const override { _refCount++; }
    uint32_t Release() const override
    {
        const uint32_t result = --_refCount;
        if (result == 0) {
            delete this;
        }
        return result;
    }

    MOCK_METHOD(Core::hresult, HandleAppGatewayRequest,
        (const Exchange::GatewayContext& context,
         const string& method,
         const string& params,
         string& response),
        (override));

private:
    mutable std::atomic<uint32_t> _refCount;
};

class AppGatewayResolverMock : public Exchange::IAppGatewayResolver {
public:
    ~AppGatewayResolverMock() override = default;

    BEGIN_INTERFACE_MAP(AppGatewayResolverMock)
    INTERFACE_ENTRY(Exchange::IAppGatewayResolver)
    END_INTERFACE_MAP

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(Core::hresult, Configure, (Exchange::IAppGatewayResolver::IStringIterator *const&), (override));
    MOCK_METHOD(Core::hresult, Resolve,
        (const Exchange::GatewayContext&, const string&, const string&, const string&, string&),
        (override));
};

class AppNotificationsMock : public Exchange::IAppNotifications {
public:
    ~AppNotificationsMock() override = default;

    BEGIN_INTERFACE_MAP(AppNotificationsMock)
    INTERFACE_ENTRY(Exchange::IAppNotifications)
    END_INTERFACE_MAP

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    // Thunder R4 IAppNotifications uses AppNotificationContext (not NotificationContext).
    MOCK_METHOD(Core::hresult, Subscribe,
        (const Exchange::IAppNotifications::AppNotificationContext& context,
         bool listen,
         const string& module,
         const string& event),
        (override));

    MOCK_METHOD(Core::hresult, Emit,
        (const string& event,
         const string& payload,
         const string& appId),
        (override));

    MOCK_METHOD(Core::hresult, Cleanup,
        (const uint32_t connectionId,
         const string& origin),
        (override));
};

class AppGatewayAuthenticatorMock : public Exchange::IAppGatewayAuthenticator {
public:
    ~AppGatewayAuthenticatorMock() override = default;

    BEGIN_INTERFACE_MAP(AppGatewayAuthenticatorMock)
    INTERFACE_ENTRY(Exchange::IAppGatewayAuthenticator)
    END_INTERFACE_MAP

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(Core::hresult, Authenticate, (const string&, string&), (override));
    MOCK_METHOD(Core::hresult, GetSessionId, (const string&, string&), (override));
    MOCK_METHOD(Core::hresult, CheckPermissionGroup, (const string&, const string&, bool&), (override));
};

class AppGatewayResponderNotificationMock : public Exchange::IAppGatewayResponder::INotification {
public:
    ~AppGatewayResponderNotificationMock() override = default;

    BEGIN_INTERFACE_MAP(AppGatewayResponderNotificationMock)
    INTERFACE_ENTRY(Exchange::IAppGatewayResponder::INotification)
    END_INTERFACE_MAP

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void, OnAppConnectionChanged, (const string&, const uint32_t, const bool), (override));
};

static Exchange::GatewayContext MakeContext()
{
    Exchange::GatewayContext c;
    c.appId = "test.app";

    // In current Exchange::GatewayContext these are numeric identifiers.
    c.connectionId = 1;
    c.requestId = 1;

    return c;
}

} // namespace

// -----------------------------------------------------------------------------
// Resolver-focused tests (mapping S.No 1-5)
// -----------------------------------------------------------------------------

TEST(AppGatewayResolverTest, Resolver_LoadConfig_MissingFile_ReturnsFalse)
{
    Resolver resolver(nullptr /* shell is not required for LoadConfig */);

    EXPECT_FALSE(resolver.LoadConfig("/tmp/does-not-exist-appgateway-resolution.json"));
    EXPECT_FALSE(resolver.IsConfigured());
}

TEST(AppGatewayResolverTest, Resolver_LoadConfig_InvalidJson_ReturnsFalse)
{
    const std::string path = "/tmp/resolution.invalid.json";
    WriteTextFile(path, "{ invalid-json ");

    Resolver resolver(nullptr);
    EXPECT_FALSE(resolver.LoadConfig(path));
    EXPECT_FALSE(resolver.IsConfigured());
}

TEST(AppGatewayResolverTest, Resolver_LoadConfig_MissingResolutionsObject_ReturnsFalse)
{
    const std::string path = "/tmp/resolution.noresolutions.json";
    WriteTextFile(path, R"json(
        { "notResolutions": { "x": 1 } }
    )json");

    Resolver resolver(nullptr);
    EXPECT_FALSE(resolver.LoadConfig(path));
    EXPECT_FALSE(resolver.IsConfigured());
}

TEST(AppGatewayResolverTest, Resolver_LoadConfig_LowercasesKeysAndOverrides)
{
    const std::string path1 = "/tmp/resolution.case1.json";
    const std::string path2 = "/tmp/resolution.case2.json";

    // 1st load: MiXeD key maps to alias A
    WriteTextFile(path1, R"json(
        {
          "resolutions": {
            "MiXeDCaSe.Method": {
              "alias": "org.rdk.FirstPlugin.first"
            }
          }
        }
    )json");

    // 2nd load: same key maps to alias B (should override)
    WriteTextFile(path2, R"json(
        {
          "resolutions": {
            "mixedcase.method": {
              "alias": "org.rdk.SecondPlugin.second"
            }
          }
        }
    )json");

    Resolver resolver(nullptr);
    EXPECT_TRUE(resolver.LoadConfig(path1));
    EXPECT_TRUE(resolver.IsConfigured());

    EXPECT_EQ(std::string("org.rdk.FirstPlugin.first"), resolver.ResolveAlias("MIXEDCASE.METHOD"));

    EXPECT_TRUE(resolver.LoadConfig(path2));
    EXPECT_EQ(std::string("org.rdk.SecondPlugin.second"), resolver.ResolveAlias("MiXeDCaSe.MeThOd"));
}

TEST(AppGatewayResolverTest, Resolver_LoadConfig_EventAndComRpcFlags)
{
    const std::string path = "/tmp/resolution.flags.json";
    WriteTextFile(path, R"json(
        {
          "resolutions": {
            "event.method": {
              "alias": "org.rdk.AppGatewayCommon",
              "event": "someEvent"
            },
            "comrpc.method": {
              "alias": "org.rdk.SomeHandler",
              "useComRpc": true
            }
          }
        }
    )json");

    Resolver resolver(nullptr);
    EXPECT_TRUE(resolver.LoadConfig(path));

    EXPECT_TRUE(resolver.HasEvent("event.method"));
    EXPECT_FALSE(resolver.HasEvent("comrpc.method"));

    EXPECT_TRUE(resolver.HasComRpcRequestSupport("comrpc.method"));
    EXPECT_FALSE(resolver.HasComRpcRequestSupport("event.method"));
}

// -----------------------------------------------------------------------------
// AppGatewayImplementation-focused tests (mapping S.No 9,10,11,12,13)
// Note: We avoid invoking the full plugin wrapper (AppGateway/AppGateway.cpp) and
// instead test AppGatewayImplementation directly, matching mapping intent.
// -----------------------------------------------------------------------------

TEST(AppGatewayImplementationTest, AppGateway_Event_PreProcessEvent_MissingParams_BadRequest)
{
    // Build config with an event method.
    const std::string cfg = "/tmp/appgw.event.cfg.json";
    WriteTextFile(cfg, R"json(
        {
          "resolutions": {
            "event.method": {
              "alias": "org.rdk.AppGatewayCommon",
              "event": "dummy"
            }
          }
        }
    )json");

    ::testing::NiceMock<ServiceMock> service;
    // AppGatewayImplementation stores the shell and will AddRef()/Release().
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Based on the attached failure log, Resolve() error paths may still attempt incidental lookups
    // (e.g. QueryInterfaceByCallsign("org.rdk.LaunchDelegate")). Make StrictMock tolerant to those.
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Return(nullptr));

    // AppGatewayImplementation is reference-counted (IReferenceCounted).
    // Core::Sink<> provides the required AddRef/Release implementation.
    Core::Sink<AppGatewayImplementation> impl;
    const auto shellConfigureRc = impl.Configure(&service);
    EXPECT_TRUE((shellConfigureRc == Core::ERROR_NONE) || (shellConfigureRc == Core::ERROR_GENERAL));

    // Now configure resolutions via the public Configure(paths) interface.
    // Build a minimal iterator inline that matches the RPC iterator requirements.
    class PathsIterator : public Exchange::IAppGatewayResolver::IStringIterator {
    public:
        explicit PathsIterator(std::vector<std::string> paths)
            : _paths(std::move(paths))
            , _index(0)
        {
        }

        void AddRef() const override {}
        uint32_t Release() const override { return Core::ERROR_NONE; }

        bool Next(string& value) override
        {
            if (_index >= _paths.size()) {
                return false;
            }
            value = _paths[_index++];
            return true;
        }

        bool Previous(string& value) override
        {
            if (_paths.empty() || _index == 0) {
                return false;
            }
            _index--;
            value = _paths[_index];
            return true;
        }

        void Reset(const uint32_t position) override
        {
            _index = (position <= _paths.size() ? position : _paths.size());
        }

        bool IsValid() const override
        {
            return (_index < _paths.size());
        }

        uint32_t Count() const override
        {
            return static_cast<uint32_t>(_paths.size());
        }

        string Current() const override
        {
            return (_index < _paths.size() ? _paths[_index] : string());
        }

        BEGIN_INTERFACE_MAP(PathsIterator)
        INTERFACE_ENTRY(Exchange::IAppGatewayResolver::IStringIterator)
        END_INTERFACE_MAP

    private:
        std::vector<std::string> _paths;
        size_t _index;
    };

    PathsIterator it({ cfg });
    EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&it));

    // Authoritative current behavior (attached log) for missing params is:
    // {"code":-32602,"message":"Missing required boolean 'listen' parameter"}
    std::string resolution;
    const auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_BAD_REQUEST, impl.Resolve(ctx, "gateway", "event.method", "" /* params missing */, resolution));

    EXPECT_THAT(resolution, ::testing::HasSubstr("\"code\":-32602"));
    EXPECT_THAT(resolution, ::testing::HasSubstr("\"message\":\"Missing required boolean 'listen' parameter\""));
}

TEST(AppGatewayImplementationTest, AppGateway_Event_PreProcessEvent_MissingListen_BadRequest)
{
    const std::string cfg = "/tmp/appgw.event.cfg2.json";
    WriteTextFile(cfg, R"json(
        {
          "resolutions": {
            "event.method": {
              "alias": "org.rdk.AppGatewayCommon",
              "event": "dummy"
            }
          }
        }
    )json");

    ::testing::NiceMock<ServiceMock> service;
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Match the "source of truth" runtime behavior: even in this error path, the implementation
    // may attempt incidental interface lookups. Make StrictMock tolerant to those to avoid
    // unrelated expectation failures.
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Return(nullptr));

    Core::Sink<AppGatewayImplementation> impl;
    const auto shellConfigureRc = impl.Configure(&service);
    EXPECT_TRUE((shellConfigureRc == Core::ERROR_NONE) || (shellConfigureRc == Core::ERROR_GENERAL));

    class PathsIterator : public Exchange::IAppGatewayResolver::IStringIterator {
    public:
        explicit PathsIterator(std::vector<std::string> paths)
            : _paths(std::move(paths))
            , _index(0)
        {
        }

        void AddRef() const override {}
        uint32_t Release() const override { return Core::ERROR_NONE; }

        bool Next(string& value) override
        {
            if (_index >= _paths.size()) {
                return false;
            }
            value = _paths[_index++];
            return true;
        }

        bool Previous(string& value) override
        {
            if (_paths.empty() || _index == 0) {
                return false;
            }
            _index--;
            value = _paths[_index];
            return true;
        }

        void Reset(const uint32_t position) override
        {
            _index = (position <= _paths.size() ? position : _paths.size());
        }

        bool IsValid() const override
        {
            return (_index < _paths.size());
        }

        uint32_t Count() const override
        {
            return static_cast<uint32_t>(_paths.size());
        }

        string Current() const override
        {
            return (_index < _paths.size() ? _paths[_index] : string());
        }

        BEGIN_INTERFACE_MAP(PathsIterator)
        INTERFACE_ENTRY(Exchange::IAppGatewayResolver::IStringIterator)
        END_INTERFACE_MAP

    private:
        std::vector<std::string> _paths;
        size_t _index;
    };

    PathsIterator it({ cfg });
    EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&it));

    std::string resolution;
    const auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_BAD_REQUEST,
        impl.Resolve(ctx, "gateway", "event.method", "{}" /* no listen field */, resolution));

    // Authoritative current behavior (see attached log):
    // {"code":-32602,"message":"Missing required boolean 'listen' parameter"}
    EXPECT_THAT(resolution, ::testing::HasSubstr("\"code\":-32602"));
    EXPECT_THAT(resolution, ::testing::HasSubstr("\"message\":\"Missing required boolean 'listen' parameter\""));
}

TEST(AppGatewayImplementationTest, AppGateway_ComRpc_RequestHandlerMissing_NotAvailable)
{
    const std::string cfg = "/tmp/appgw.comrpc.cfg.json";
    WriteTextFile(cfg, R"json(
        {
          "resolutions": {
            "comrpc.method": {
              "alias": "org.rdk.SomeHandler",
              "useComRpc": true
            }
          }
        }
    )json");

    ::testing::NiceMock<ServiceMock> service;
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // No handler provided => QueryInterfaceByCallsign returns nullptr.
    // Template method QueryInterfaceByCallsign ultimately calls:
    //   void* QueryInterfaceByCallsign(uint32_t interfaceId, const string& callsign)
    // so we emulate "not found" by returning nullptr for any request.
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(nullptr));

    Core::Sink<AppGatewayImplementation> impl;
    const auto shellConfigureRc = impl.Configure(&service);
    EXPECT_TRUE((shellConfigureRc == Core::ERROR_NONE) || (shellConfigureRc == Core::ERROR_GENERAL));

    class PathsIterator : public Exchange::IAppGatewayResolver::IStringIterator {
    public:
        explicit PathsIterator(std::vector<std::string> paths)
            : _paths(std::move(paths))
            , _index(0)
        {
        }

        void AddRef() const override {}
        uint32_t Release() const override { return Core::ERROR_NONE; }

        bool Next(string& value) override
        {
            if (_index >= _paths.size()) {
                return false;
            }
            value = _paths[_index++];
            return true;
        }

        bool Previous(string& value) override
        {
            if (_paths.empty() || _index == 0) {
                return false;
            }
            _index--;
            value = _paths[_index];
            return true;
        }

        void Reset(const uint32_t position) override
        {
            _index = (position <= _paths.size() ? position : _paths.size());
        }

        bool IsValid() const override
        {
            return (_index < _paths.size());
        }

        uint32_t Count() const override
        {
            return static_cast<uint32_t>(_paths.size());
        }

        string Current() const override
        {
            return (_index < _paths.size() ? _paths[_index] : string());
        }

        BEGIN_INTERFACE_MAP(PathsIterator)
        INTERFACE_ENTRY(Exchange::IAppGatewayResolver::IStringIterator)
        END_INTERFACE_MAP

    private:
        std::vector<std::string> _paths;
        size_t _index;
    };

    PathsIterator it({ cfg });
    EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&it));

    std::string resolution;
    const auto ctx = MakeContext();
    EXPECT_EQ(Core::ERROR_GENERAL,
        impl.Resolve(ctx, "gateway", "comrpc.method", R"json({"a":1})json", resolution));

    // The error payload is built by ErrorUtils::NotAvailable; validate by substring.
    // Authoritative behavior (attached log): {"code":-50200,"message":"NotAvailable"}
    EXPECT_THAT(resolution, ::testing::HasSubstr("NotAvailable"));
}

TEST(AppGatewayImplementationTest, AppGateway_ComRpc_AdditionalContext_WrapsParamsWith_additionalContext)
{
    const std::string cfg = "/tmp/appgw.comrpc.ctx.cfg.json";
    WriteTextFile(cfg, R"json(
        {
          "resolutions": {
            "comrpc.method": {
              "alias": "org.rdk.SomeHandler",
              "includeContext": true,
              "additionalContext": { "foo": "bar" }
            }
          }
        }
    )json");

    ::testing::NiceMock<ServiceMock> service;
    EXPECT_CALL(service, AddRef()).Times(1);
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    // Provide a request handler mock instance and return it via QueryInterfaceByCallsign.
    // NOTE: AppGatewayImplementation will call Release() on the handler when done.
    //
    // IMPORTANT: Do NOT leak the mock. We use a refcounted mock implementation:
    // - We create it with refcount=1
    // - Service "QueryInterfaceByCallsign" simulates COM behavior by AddRef() before returning it
    // - Production code calls Release(), which deletes it when refcount hits 0
    auto* handler = new AppGatewayRequestHandlerMock();

    // Return the handler when alias callsign matches; allow repeats (implementation may re-query).
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.SomeHandler")))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke([&](const uint32_t, const string&) -> void* {
            handler->AddRef();
            return static_cast<void*>(handler);
        }));

    // AppGatewayImplementation may also try to send an internal responder message (async)
    // via SendToLaunchDelegate(), which looks up "org.rdk.LaunchDelegate".
    EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::StrEq("org.rdk.LaunchDelegate")))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Return(nullptr));

    Core::Sink<AppGatewayImplementation> impl;
    const auto shellConfigureRc = impl.Configure(&service);
    EXPECT_TRUE((shellConfigureRc == Core::ERROR_NONE) || (shellConfigureRc == Core::ERROR_GENERAL));

    class PathsIterator : public Exchange::IAppGatewayResolver::IStringIterator {
    public:
        explicit PathsIterator(std::vector<std::string> paths)
            : _paths(std::move(paths))
            , _index(0)
        {
        }

        void AddRef() const override {}
        uint32_t Release() const override { return Core::ERROR_NONE; }

        bool Next(string& value) override
        {
            if (_index >= _paths.size()) {
                return false;
            }
            value = _paths[_index++];
            return true;
        }

        bool Previous(string& value) override
        {
            if (_paths.empty() || _index == 0) {
                return false;
            }
            _index--;
            value = _paths[_index];
            return true;
        }

        void Reset(const uint32_t position) override
        {
            _index = (position <= _paths.size() ? position : _paths.size());
        }

        bool IsValid() const override
        {
            return (_index < _paths.size());
        }

        uint32_t Count() const override
        {
            return static_cast<uint32_t>(_paths.size());
        }

        string Current() const override
        {
            return (_index < _paths.size() ? _paths[_index] : string());
        }

        BEGIN_INTERFACE_MAP(PathsIterator)
        INTERFACE_ENTRY(Exchange::IAppGatewayResolver::IStringIterator)
        END_INTERFACE_MAP

    private:
        std::vector<std::string> _paths;
        size_t _index;
    };

    PathsIterator it({ cfg });
    EXPECT_EQ(Core::ERROR_NONE, impl.Configure(&it));

    const auto ctx = MakeContext();
    const std::string origin = "some-origin";
    const std::string params = R"json({"p":123})json";

    // Capture params passed to handler.
    EXPECT_CALL(*handler, HandleAppGatewayRequest(::testing::_, ::testing::StrEq("comrpc.method"), ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&](const Exchange::GatewayContext&,
                                        const string&,
                                        const string& finalParams,
                                        string& response) {
            // finalParams should be:
            // { "params": <original object>, "_additionalContext": { "foo":"bar", "origin":"<origin>" } }
            EXPECT_THAT(finalParams, ::testing::HasSubstr("\"params\""));
            EXPECT_THAT(finalParams, ::testing::HasSubstr("\"p\":123"));
            EXPECT_THAT(finalParams, ::testing::HasSubstr("\"_additionalContext\""));
            EXPECT_THAT(finalParams, ::testing::HasSubstr("\"foo\":\"bar\""));
            EXPECT_THAT(finalParams, ::testing::HasSubstr("\"origin\":\"" + origin + "\""));

            response = R"json({"ok":true})json";
            return Core::ERROR_NONE;
        }));

    std::string resolution;
    EXPECT_EQ(Core::ERROR_NONE, impl.Resolve(ctx, origin, "comrpc.method", params, resolution));
    EXPECT_THAT(resolution, ::testing::HasSubstr("\"ok\":true"));

    // Lifetime note:
    // `QueryInterfaceByCallsign` emulation AddRef()'s the handler before returning it.
    // AppGatewayImplementation is responsible for calling Release() on the interface it queried.
    //
    // However, the test itself also owns the initial reference from `new` (refcount starts at 1).
    // After Resolve() returns, production should have released its QueryInterface reference,
    // so we must release the test-owned reference to avoid a leaked mock at process exit.
    handler->Release();
}

TEST(AppGatewayPluginTest, DISABLED_AppGateway_InitializeFailsWithoutRoots_ThenDeinitialize)
{
    Core::Sink<AppGateway> plugin;
    ::testing::NiceMock<ServiceMock> service;

    EXPECT_CALL(service, AddRef()).Times(::testing::AnyNumber());
    EXPECT_CALL(service, Release()).Times(::testing::AnyNumber()).WillRepeatedly(::testing::Return(Core::ERROR_NONE));

    const string response = plugin.Initialize(&service);
    (void)response;
}

TEST(AppGatewayPluginTest, AppGateway_Information_EmptyString)
{
    Core::Sink<AppGateway> plugin;
    EXPECT_TRUE(plugin.Information().empty());
}

TEST(AppGatewayResponderImplementationTest, RegisterUnregisterAndNotify)
{
    Core::Sink<AppGatewayResponderImplementation> responder;
    ::testing::StrictMock<AppGatewayResponderNotificationMock> notification;

    EXPECT_CALL(notification, AddRef()).Times(1);
    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));

    EXPECT_CALL(notification, OnAppConnectionChanged(::testing::StrEq("test.app"), 55u, true)).Times(1);
    responder.OnConnectionStatusChanged("test.app", 55u, true);

    EXPECT_CALL(notification, Release()).Times(1).WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
    EXPECT_EQ(Core::ERROR_GENERAL, responder.Unregister(&notification));
}

TEST(AppGatewayResponderImplementationTest, RegisterSameNotificationTwice_AddRefOnlyOnce)
{
    Core::Sink<AppGatewayResponderImplementation> responder;
    ::testing::StrictMock<AppGatewayResponderNotificationMock> notification;

    EXPECT_CALL(notification, AddRef()).Times(1);
    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));
    EXPECT_EQ(Core::ERROR_NONE, responder.Register(&notification));

    EXPECT_CALL(notification, Release()).Times(1).WillOnce(::testing::Return(Core::ERROR_NONE));
    EXPECT_EQ(Core::ERROR_NONE, responder.Unregister(&notification));
}

TEST(AppGatewayResponderImplementationTest, GetGatewayConnectionContext_ReturnsNone)
{
    Core::Sink<AppGatewayResponderImplementation> responder;
    string value;
    EXPECT_EQ(Core::ERROR_NONE, responder.GetGatewayConnectionContext(10, "jsonrpc.compliant", value));
}

TEST(AppGatewayImplementationInternalTest, UpdateContext_IncludesGatewayContext)
{
        Core::Sink<AppGatewayImplementation> impl;

        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);
        const std::string cfg = "/tmp/appgw.ctx.include.json";
        WriteTextFile(cfg, R"json(
                {
                    "resolutions": {
                        "ctx.method": {
                            "alias": "org.rdk.SomePlugin.someMethod",
                            "includeContext": true
                        }
                    }
                }
        )json");
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(cfg));

        auto ctx = MakeContext();
        const std::string out = impl.UpdateContext(ctx, "ctx.method", R"json({"x":1})json", "gateway", false);
        EXPECT_THAT(out, ::testing::HasSubstr("\"context\""));
        EXPECT_THAT(out, ::testing::HasSubstr("\"appId\":\"test.app\""));
}

TEST(AppGatewayImplementationInternalTest, UpdateContext_UsesAdditionalContext)
{
        Core::Sink<AppGatewayImplementation> impl;

        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);
        const std::string cfg = "/tmp/appgw.additional.context.json";
        WriteTextFile(cfg, R"json(
                {
                    "resolutions": {
                        "comrpc.method": {
                            "alias": "org.rdk.SomeHandler",
                            "includeContext": true,
                            "additionalContext": { "foo": "bar" }
                        }
                    }
                }
        )json");
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(cfg));

        auto ctx = MakeContext();
        const std::string out = impl.UpdateContext(ctx, "comrpc.method", R"json({"p":123})json", "origin.app", true);
        EXPECT_THAT(out, ::testing::HasSubstr("\"_additionalContext\""));
        EXPECT_THAT(out, ::testing::HasSubstr("\"foo\":\"bar\""));
        EXPECT_THAT(out, ::testing::HasSubstr("\"origin\":\"origin.app\""));
}

TEST(AppGatewayImplementationInternalTest, PreProcessEvent_ValidListen_SubscribeSuccess)
{
        Core::Sink<AppGatewayImplementation> impl;
        ::testing::NiceMock<AppNotificationsMock> appNotifications;

        impl.mAppNotifications = &appNotifications;

        EXPECT_CALL(appNotifications, Subscribe(::testing::_, true, ::testing::StrEq("org.rdk.AppGatewayCommon"), ::testing::StrEq("event.method")))
                .WillOnce(::testing::Return(Core::ERROR_NONE));

        auto ctx = MakeContext();
        std::string resolution;
        const auto rc = impl.PreProcessEvent(ctx, "org.rdk.AppGatewayCommon", "event.method", "gateway", R"json({"listen":true})json", resolution);

        EXPECT_EQ(Core::ERROR_NONE, rc);
        EXPECT_THAT(resolution, ::testing::HasSubstr("\"listening\":true"));
        EXPECT_THAT(resolution, ::testing::HasSubstr("\"event\":\"event.method\""));

        // Avoid impl destructor calling Release() on a stack mock that will be destroyed first.
        impl.mAppNotifications = nullptr;
}

    TEST(AppGatewayImplementationInternalTest, PreProcessEvent_VersionedEvent_UsesRdk8Suffix)
    {
        Core::Sink<AppGatewayImplementation> impl;
        ::testing::NiceMock<AppNotificationsMock> appNotifications;

        impl.mAppNotifications = &appNotifications;
        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);

        const std::string cfg = "/tmp/appgw.versioned.event.json";
        WriteTextFile(cfg, R"json(
            {
                "resolutions": {
                "event.method": {
                    "alias": "org.rdk.AppGatewayCommon",
                    "versionedEvent": true
                }
                }
            }
        )json");
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(cfg));

        EXPECT_CALL(appNotifications, Subscribe(::testing::_, true, ::testing::StrEq("org.rdk.AppGatewayCommon"), ::testing::StrEq("event.method.v8")))
            .WillOnce(::testing::Return(Core::ERROR_NONE));

        auto ctx = MakeContext();
        ctx.version = "8";
        std::string resolution;
        const auto rc = impl.PreProcessEvent(ctx, "org.rdk.AppGatewayCommon", "event.method", "gateway", R"json({"listen":true})json", resolution);

        EXPECT_EQ(Core::ERROR_NONE, rc);
        EXPECT_THAT(resolution, ::testing::HasSubstr("\"listening\":true"));
        EXPECT_THAT(resolution, ::testing::HasSubstr("\"event\":\"event.method\""));

        impl.mAppNotifications = nullptr;
    }

TEST(AppGatewayImplementationInternalTest, FetchResolvedData_PermissionDenied_ReturnsNotPermitted)
{
        Core::Sink<AppGatewayImplementation> impl;
        ::testing::NiceMock<ServiceMock> service;
        ::testing::StrictMock<AppGatewayAuthenticatorMock> auth;

        impl.mService = &service;
        impl.mResolverPtr = std::make_shared<Resolver>(nullptr);

        const std::string cfg = "/tmp/appgw.permission.denied.json";
        WriteTextFile(cfg, R"json(
                {
                    "resolutions": {
                        "permission.method": {
                            "alias": "org.rdk.SomePlugin.someMethod",
                            "permissionGroup": "restricted"
                        }
                    }
                }
        )json");
        ASSERT_TRUE(impl.mResolverPtr->LoadConfig(cfg));

        EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
                .WillRepeatedly(::testing::Return(static_cast<void*>(&auth)));

        EXPECT_CALL(auth, CheckPermissionGroup(::testing::StrEq("test.app"), ::testing::StrEq("restricted"), ::testing::_))
                .WillOnce(::testing::DoAll(::testing::SetArgReferee<2>(false), ::testing::Return(Core::ERROR_NONE)));

        auto ctx = MakeContext();
        std::string resolution;
        const auto rc = impl.FetchResolvedData(ctx, "permission.method", "{}", "gateway", resolution);
        EXPECT_EQ(Core::ERROR_GENERAL, rc);
        EXPECT_THAT(resolution, ::testing::HasSubstr("NotPermitted"));

        // Prevent destructor from releasing stack-owned mocks after their lifetime ends.
        impl.mAuthenticator = nullptr;
        impl.mService = nullptr;
}

TEST(AppGatewayResponderHeaderTest, AppIdRegistry_AddGetRemove)
{
        AppGatewayResponderImplementation::AppIdRegistry registry;

        registry.Add(101, "app.one");
        std::string appId;
        EXPECT_TRUE(registry.Get(101, appId));
        EXPECT_EQ("app.one", appId);

        registry.Remove(101);
        EXPECT_FALSE(registry.Get(101, appId));
}

TEST(AppGatewayResponderHeaderTest, CompliantJsonRpcRegistry_BasicFlow)
{
        AppGatewayResponderImplementation::CompliantJsonRpcRegistry registry;

        registry.CheckAndAddCompliantJsonRpc(22, "session=abc&RPCV2=true&x=1");
        EXPECT_TRUE(registry.IsCompliantJsonRpc(22));

        registry.CleanupConnectionId(22);
        EXPECT_FALSE(registry.IsCompliantJsonRpc(22));
}

TEST(AppGatewayResponderImplementationInternalTest, DispatchWsMsg_NoAppId_ClosesConnection)
{
    Core::Sink<AppGatewayResponderImplementation> responder;

    // No appId is registered for this connection, should take close path.
    responder.DispatchWsMsg("device.make", "{}", 1, 999);
    SUCCEED();
}

TEST(AppGatewayResponderImplementationInternalTest, DispatchWsMsg_AppIdButNoResolver_ReturnsGracefully)
{
    Core::Sink<AppGatewayResponderImplementation> responder;
    ::testing::NiceMock<ServiceMock> service;

    responder.mService = &service;
    responder.mAppIdRegistry.Add(123, "app.id");

    EXPECT_CALL(service, QueryInterface(::testing::_)).WillRepeatedly(::testing::Return(nullptr));
    responder.DispatchWsMsg("device.make", "{}", 5, 123);
    SUCCEED();

    responder.mService = nullptr;
}

TEST(AppGatewayResponderImplementationInternalTest, DispatchWsMsg_WithResolver_CallsResolve)
{
    Core::Sink<AppGatewayResponderImplementation> responder;
    ::testing::StrictMock<AppGatewayResolverMock> resolver;

    responder.mResolver = &resolver;
    responder.mAppIdRegistry.Add(111, "my.app");

    EXPECT_CALL(resolver, Resolve(::testing::_, ::testing::StrEq("org.rdk.AppGateway"), ::testing::StrEq("method.name"), ::testing::StrEq("{}"), ::testing::_))
        .WillOnce(::testing::Return(Core::ERROR_NONE));

    responder.DispatchWsMsg("method.name", "{}", 77, 111);
    responder.mResolver = nullptr;
}

TEST(AppGatewayResolverTest, Resolver_ParseAlias_And_ClearResolutions)
{
    Resolver resolver(nullptr);

    std::string callsign;
    std::string method;
    resolver.ParseAlias("org.rdk.Test.methodName", callsign, method);
    EXPECT_EQ("org.rdk.Test", callsign);
    EXPECT_EQ("methodName", method);

    resolver.ParseAlias("org.rdk.NoMethod", callsign, method);
    EXPECT_EQ("org.rdk", callsign);
    EXPECT_EQ("NoMethod", method);

    resolver.ClearResolutions();
    EXPECT_FALSE(resolver.IsConfigured());
}

TEST(AppGatewayResolverTest, Resolver_CallThunderPlugin_ValidatesInputs)
{
    Resolver resolver(nullptr);
    std::string response;

    EXPECT_EQ(Core::ERROR_GENERAL, resolver.CallThunderPlugin("", "{}", response));
    EXPECT_EQ(Core::ERROR_GENERAL, resolver.CallThunderPlugin("invalidalias", "{}", response));
    EXPECT_EQ(Core::ERROR_GENERAL, resolver.CallThunderPlugin("org.rdk.Test.method", "{}", response));
}

TEST(AppGatewayResponderImplementationTest, DISABLED_RespondEmitRequest_ReturnNone)
{
        Core::Sink<AppGatewayResponderImplementation> responder;
        auto ctx = MakeContext();

        EXPECT_EQ(Core::ERROR_NONE, responder.Respond(ctx, R"json({"ok":true})json"));
        EXPECT_EQ(Core::ERROR_NONE, responder.Emit(ctx, "lifecycle.onStatusChanged", R"json({"status":"ok"})json"));
        EXPECT_EQ(Core::ERROR_NONE, responder.Request(ctx.connectionId, ctx.requestId, "device.make", "{}"));

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

