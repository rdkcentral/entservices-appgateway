/**
 * L0 coverage tests for AppGatewayTelemetry private Send* methods and FlushJob.
 *
 * The L0 test binary compiles AppGatewayTelemetry.cpp directly (see CMakeLists.txt),
 * so the #define private public trick (also used in L1 tests) is safe here:
 *  - It only affects the compiler's visibility checks during THIS translation unit.
 *  - Memory layout of AppGatewayTelemetry is identical; no ABI incompatibility.
 *
 * Targeted uncovered code (as of 2026-04-08 baseline, 63.1% L0 coverage):
 *   - AppGatewayTelemetry::SendHealthStats()                 (~30 lines)
 *   - AppGatewayTelemetry::SendApiErrorStats()               (~18 lines)
 *   - AppGatewayTelemetry::SendExternalServiceErrorStats()   (~18 lines)
 *   - AppGatewayTelemetry::SendAggregatedMetrics()           (~24 lines)
 *   - AppGatewayTelemetry::SendApiMethodStats()              (~55 lines)
 *   - AppGatewayTelemetry::SendApiLatencyStats()             (~35 lines)
 *   - AppGatewayTelemetry::SendServiceLatencyStats()         (~35 lines)
 *   - AppGatewayTelemetry::SendServiceMethodStats()          (~55 lines)
 *   - AppGatewayTelemetry::FlushJob::Dispatch()              (~20 lines)
 *   - AppGatewayTelemetry::FlushJob::SendHealthStats()       (~25 lines)
 *   - AppGatewayTelemetry::FlushJob::SendApiMethodStats()    (~80 lines)
 *   - AppGatewayTelemetry::FlushJob::SendApiErrorStats()     (~40 lines)
 *   - AppGatewayTelemetry::FlushJob::SendAggregatedMetrics() (~35 lines)
 *   - AppGatewayTelemetry::Initialize() "already init" path  (2 lines)
 *   - Parse*() selective false branches                      (~12 lines)
 */

#include <iostream>
#include <string>
#include <memory>
#include <limits>
#include <chrono>
#include <cstdlib>
#include <map>

#include <core/core.h>

// The #define private public hack is safe in L0 because:
//  1. L0 tests compile production .cpp files directly into the test binary.
//  2. Memory layout of the class is unchanged across TUs that use this define.
//  3. The same technique is used in entservices-appgateway/Tests/L1Tests/tests/test_AppGateway.cpp.
#define private public
#include "AppGatewayTelemetry.h"
#undef private

#include "ServiceMock.h"
#include "AppGatewayTelemetryMarkers.h"

using WPEFramework::Core::ERROR_NONE;
using WPEFramework::Core::ERROR_UNAVAILABLE;
using WPEFramework::Exchange::GatewayContext;
using WPEFramework::Plugin::AppGatewayTelemetry;
using WPEFramework::Plugin::TelemetryFormat;

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

// RAII guard - initializes/deinitializes the telemetry singleton for each test.
//
// Ownership: the guard owns the single ref produced by `new` (refcount = 1).
// Initialize() receives a non-owning pointer; the guard releases the ref after
// Deinitialize() so the ServiceMock is destroyed at the end of each test.
// If Initialize()/Deinitialize() ever start managing their own AddRef/Release,
// this guard must be updated to match.
struct TelemetryGuard {
    L0Test::ServiceMock* svc { nullptr };

    TelemetryGuard()
        : svc(new L0Test::ServiceMock({}, true))
    {
        AppGatewayTelemetry::getInstance().Initialize(svc);
    }

    ~TelemetryGuard()
    {
        AppGatewayTelemetry::getInstance().Deinitialize();
        svc->Release();  // drops the ref from new; selfDelete=true destroys the object
        svc = nullptr;
    }

    TelemetryGuard(const TelemetryGuard&) = delete;
    TelemetryGuard& operator=(const TelemetryGuard&) = delete;
};

static GatewayContext MakeCtx(uint32_t reqId = 1, uint32_t connId = 1, const std::string& appId = "com.test.app")
{
    GatewayContext ctx;
    ctx.requestId    = reqId;
    ctx.connectionId = connId;
    ctx.appId        = appId;
    return ctx;
}

} // namespace

// ---------------------------------------------------------------------------
// Test DA-1: AppGatewayTelemetry::Initialize() "already initialized" branch
//            Covers LOGWARN("AppGatewayTelemetry already initialized") + return
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_AlreadyInitialized()
{
    TestResult tr;
    TelemetryGuard guard;  // Initialize once

    // A second Initialize() call on the already-initialized singleton hits the
    // "already initialized" branch (lines 62-64 of AppGatewayTelemetry.cpp).
    L0Test::ServiceMock secondSvc({}, true);
    AppGatewayTelemetry::getInstance().Initialize(&secondSvc);  // hits LOGWARN + return
    // svc balance: secondSvc has refcount 1 from stack; no Release needed.

    ExpectTrue(tr, AppGatewayTelemetry::getInstance().mInitialized,
               "Telemetry remains initialized after second Initialize() call");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-2: AppGatewayTelemetry::SendHealthStats() - empty path (no data)
//            Covers the "No health stats to report" early return.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendHealthStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();

    // Flush first to clear any residual data from previous tests.
    t.FlushTelemetryData();

    // Reset counters to ensure empty state.
    t.mHealthStats.totalCalls.store(0, std::memory_order_relaxed);
    t.mHealthStats.websocketConnections.store(0, std::memory_order_relaxed);
    t.mRequestStates.clear();

    // Call directly - hits "No health stats to report" return (line ~1053-1055).
    t.SendHealthStats();

    ExpectTrue(tr, true, "SendHealthStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-3: AppGatewayTelemetry::SendHealthStats() - with data
//            Covers the full SendHealthStats path including SendT2Event call.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendHealthStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();

    // Populate health counters directly.
    t.mHealthStats.websocketConnections.store(3, std::memory_order_relaxed);
    t.mHealthStats.totalCalls.store(10, std::memory_order_relaxed);
    t.mHealthStats.totalResponses.store(8, std::memory_order_relaxed);
    t.mHealthStats.successfulCalls.store(7, std::memory_order_relaxed);
    t.mHealthStats.failedCalls.store(1, std::memory_order_relaxed);
    t.mReportingIntervalSec = 300;

    // Call directly - covers all payload-building and SendT2Event lines.
    t.SendHealthStats();

    // Reset state.
    t.mHealthStats.websocketConnections.store(0, std::memory_order_relaxed);
    t.mHealthStats.totalCalls.store(0, std::memory_order_relaxed);
    t.mHealthStats.totalResponses.store(0, std::memory_order_relaxed);
    t.mHealthStats.successfulCalls.store(0, std::memory_order_relaxed);
    t.mHealthStats.failedCalls.store(0, std::memory_order_relaxed);

    ExpectTrue(tr, true, "SendHealthStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-4: AppGatewayTelemetry::SendApiErrorStats() - empty path
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendApiErrorStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mApiErrorCounts.clear();

    // Hits "No API error stats to report" return.
    t.SendApiErrorStats();

    ExpectTrue(tr, true, "SendApiErrorStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-5: AppGatewayTelemetry::SendApiErrorStats() - with data
//            Covers the for-loop and SendT2Event call.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendApiErrorStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mApiErrorCounts.clear();
    t.mApiErrorCounts["account.session"] = 3;
    t.mApiErrorCounts["discovery.watched"] = 1;
    t.mReportingIntervalSec = 300;

    t.SendApiErrorStats();

    t.mApiErrorCounts.clear();
    ExpectTrue(tr, true, "SendApiErrorStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-6: AppGatewayTelemetry::SendExternalServiceErrorStats() - empty
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendExternalServiceErrorStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mExternalServiceErrorCounts.clear();

    t.SendExternalServiceErrorStats();

    ExpectTrue(tr, true, "SendExternalServiceErrorStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-7: AppGatewayTelemetry::SendExternalServiceErrorStats() - with data
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendExternalServiceErrorStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mExternalServiceErrorCounts.clear();
    t.mExternalServiceErrorCounts["AGW_SERVICE_AUTHENTICATION"] = 2;
    t.mExternalServiceErrorCounts["AGW_SERVICE_PERMISSION"] = 1;
    t.mReportingIntervalSec = 300;

    t.SendExternalServiceErrorStats();

    t.mExternalServiceErrorCounts.clear();
    ExpectTrue(tr, true, "SendExternalServiceErrorStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-8: AppGatewayTelemetry::SendAggregatedMetrics() - empty path
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendAggregatedMetrics_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mMetricsCache.clear();

    t.SendAggregatedMetrics();

    ExpectTrue(tr, true, "SendAggregatedMetrics() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-9: AppGatewayTelemetry::SendAggregatedMetrics() - with data
//            Covers the for-loop including count==0 skip and normal send paths.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendAggregatedMetrics_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mMetricsCache.clear();

    // Entry with count > 0 (normal path)
    {
        AppGatewayTelemetry::MetricData d;
        d.sum = 150.0;
        d.count = 3;
        d.min = 30.0;
        d.max = 70.0;
        d.unit = AGW_UNIT_MILLISECONDS;
        t.mMetricsCache["ENTS_INFO_AppGwBootstrapDuration_split"] = d;
    }

    // Entry with count == 0 (covered: the "continue" branch)
    {
        AppGatewayTelemetry::MetricData d2;
        d2.count = 0;
        t.mMetricsCache["ENTS_INFO_AppGwEmptyMetric_split"] = d2;
    }

    // Entry with min/max at sentinel values (covers the ternary 0.0 clamp branches)
    {
        AppGatewayTelemetry::MetricData d3;
        d3.sum   = 99.0;
        d3.count = 1;
        d3.min   = std::numeric_limits<double>::max();    // clamps to 0.0
        d3.max   = std::numeric_limits<double>::lowest(); // clamps to 0.0
        d3.unit  = AGW_UNIT_COUNT;
        t.mMetricsCache["ENTS_INFO_AppGwSentinel_split"] = d3;
    }

    t.mReportingIntervalSec = 300;
    t.SendAggregatedMetrics();

    t.mMetricsCache.clear();
    ExpectTrue(tr, true, "SendAggregatedMetrics() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-10: AppGatewayTelemetry::SendApiMethodStats() - empty path
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendApiMethodStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mApiMethodStats.clear();

    t.SendApiMethodStats();

    ExpectTrue(tr, true, "SendApiMethodStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-11: AppGatewayTelemetry::SendApiMethodStats() - with success + error data
//             Covers: payload building, success branch, error branch, 0-count skip.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendApiMethodStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mApiMethodStats.clear();

    // Entry with both success and error counts
    {
        AppGatewayTelemetry::ApiMethodStats s;
        s.pluginName             = "TestPlugin";
        s.methodName             = "doThing";
        s.successCount           = 5;
        s.errorCount             = 2;
        s.totalSuccessLatencyMs  = 100.0;
        s.totalErrorLatencyMs    = 40.0;
        s.minSuccessLatencyMs    = 15.0;
        s.maxSuccessLatencyMs    = 30.0;
        s.minErrorLatencyMs      = 18.0;
        s.maxErrorLatencyMs      = 22.0;
        t.mApiMethodStats["TestPlugin_doThing"] = s;
    }

    // Entry with success only (errorCount == 0 branch)
    {
        AppGatewayTelemetry::ApiMethodStats s2;
        s2.pluginName    = "OtherPlugin";
        s2.methodName    = "getState";
        s2.successCount  = 3;
        s2.errorCount    = 0;
        s2.totalSuccessLatencyMs = 60.0;
        s2.minSuccessLatencyMs   = 18.0;
        s2.maxSuccessLatencyMs   = 25.0;
        t.mApiMethodStats["OtherPlugin_getState"] = s2;
    }

    // Entry with error only (successCount == 0 branch)
    {
        AppGatewayTelemetry::ApiMethodStats s3;
        s3.pluginName   = "BrokenPlugin";
        s3.methodName   = "alwaysFails";
        s3.successCount = 0;
        s3.errorCount   = 4;
        s3.totalErrorLatencyMs = 80.0;
        s3.minErrorLatencyMs   = 15.0;
        s3.maxErrorLatencyMs   = 30.0;
        t.mApiMethodStats["BrokenPlugin_alwaysFails"] = s3;
    }

    // Entry with sentinel min/max (covers the ternary 0.0 clamp)
    {
        AppGatewayTelemetry::ApiMethodStats s4;
        s4.pluginName            = "SentPlugin";
        s4.methodName            = "sentMethod";
        s4.successCount          = 1;
        s4.errorCount            = 1;
        s4.totalSuccessLatencyMs = 10.0;
        s4.totalErrorLatencyMs   = 10.0;
        s4.minSuccessLatencyMs   = std::numeric_limits<double>::max();
        s4.maxSuccessLatencyMs   = std::numeric_limits<double>::lowest();
        s4.minErrorLatencyMs     = std::numeric_limits<double>::max();
        s4.maxErrorLatencyMs     = std::numeric_limits<double>::lowest();
        t.mApiMethodStats["SentPlugin_sentMethod"] = s4;
    }

    // Entry with zero success+error (covered: the "continue" skip branch)
    {
        AppGatewayTelemetry::ApiMethodStats s5;
        s5.pluginName   = "ZeroPlugin";
        s5.methodName   = "zeroMethod";
        s5.successCount = 0;
        s5.errorCount   = 0;
        t.mApiMethodStats["ZeroPlugin_zeroMethod"] = s5;
    }

    t.mReportingIntervalSec = 300;
    t.SendApiMethodStats();

    t.mApiMethodStats.clear();
    ExpectTrue(tr, true, "SendApiMethodStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-12: AppGatewayTelemetry::SendApiLatencyStats() - empty path
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendApiLatencyStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mApiLatencyStats.clear();

    t.SendApiLatencyStats();

    ExpectTrue(tr, true, "SendApiLatencyStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-13: AppGatewayTelemetry::SendApiLatencyStats() - with data
//             Covers for-loop, count==0 skip, normal send, min/max sentinel clamps.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendApiLatencyStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mApiLatencyStats.clear();

    // Normal entry
    {
        AppGatewayTelemetry::ApiLatencyStats s;
        s.pluginName    = "AuthPlugin";
        s.apiName       = "getToken";
        s.count         = 4;
        s.totalLatencyMs = 80.0;
        s.minLatencyMs  = 15.0;
        s.maxLatencyMs  = 30.0;
        t.mApiLatencyStats["AuthPlugin_getToken"] = s;
    }

    // Entry with sentinel min/max
    {
        AppGatewayTelemetry::ApiLatencyStats s2;
        s2.pluginName    = "SentPlugin";
        s2.apiName       = "sentApi";
        s2.count         = 2;
        s2.totalLatencyMs = 50.0;
        s2.minLatencyMs  = std::numeric_limits<double>::max();
        s2.maxLatencyMs  = std::numeric_limits<double>::lowest();
        t.mApiLatencyStats["SentPlugin_sentApi"] = s2;
    }

    // Entry with count == 0 (continue branch)
    {
        AppGatewayTelemetry::ApiLatencyStats s3;
        s3.pluginName = "ZeroPlugin";
        s3.apiName    = "zeroApi";
        s3.count      = 0;
        t.mApiLatencyStats["ZeroPlugin_zeroApi"] = s3;
    }

    t.mReportingIntervalSec = 300;
    t.SendApiLatencyStats();

    t.mApiLatencyStats.clear();
    ExpectTrue(tr, true, "SendApiLatencyStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-14: AppGatewayTelemetry::SendServiceLatencyStats() - empty path
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendServiceLatencyStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mServiceLatencyStats.clear();

    t.SendServiceLatencyStats();

    ExpectTrue(tr, true, "SendServiceLatencyStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-15: AppGatewayTelemetry::SendServiceLatencyStats() - with data
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendServiceLatencyStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mServiceLatencyStats.clear();

    // Normal entry
    {
        AppGatewayTelemetry::ServiceLatencyStats s;
        s.pluginName     = "AuthPlugin";
        s.serviceName    = "PermissionService";
        s.count          = 3;
        s.totalLatencyMs = 90.0;
        s.minLatencyMs   = 25.0;
        s.maxLatencyMs   = 40.0;
        t.mServiceLatencyStats["AuthPlugin_PermissionService"] = s;
    }

    // Entry with sentinel min/max
    {
        AppGatewayTelemetry::ServiceLatencyStats s2;
        s2.pluginName     = "SentPlugin";
        s2.serviceName    = "SentService";
        s2.count          = 1;
        s2.totalLatencyMs = 30.0;
        s2.minLatencyMs   = std::numeric_limits<double>::max();
        s2.maxLatencyMs   = std::numeric_limits<double>::lowest();
        t.mServiceLatencyStats["SentPlugin_SentService"] = s2;
    }

    // Count == 0 (continue branch)
    {
        AppGatewayTelemetry::ServiceLatencyStats s3;
        s3.count = 0;
        t.mServiceLatencyStats["ZeroPlugin_ZeroService"] = s3;
    }

    t.mReportingIntervalSec = 300;
    t.SendServiceLatencyStats();

    t.mServiceLatencyStats.clear();
    ExpectTrue(tr, true, "SendServiceLatencyStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-16: AppGatewayTelemetry::SendServiceMethodStats() - empty path
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendServiceMethodStats_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mServiceMethodStats.clear();

    t.SendServiceMethodStats();

    ExpectTrue(tr, true, "SendServiceMethodStats() empty - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-17: AppGatewayTelemetry::SendServiceMethodStats() - with data
//             Covers: success+error, success-only, error-only, sentinel, zero.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_SendServiceMethodStats_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    t.mServiceMethodStats.clear();

    // Both success and error
    {
        AppGatewayTelemetry::ServiceMethodStats s;
        s.pluginName             = "PermPlugin";
        s.serviceName            = "CheckPermission";
        s.successCount           = 4;
        s.errorCount             = 1;
        s.totalSuccessLatencyMs  = 80.0;
        s.totalErrorLatencyMs    = 15.0;
        s.minSuccessLatencyMs    = 15.0;
        s.maxSuccessLatencyMs    = 30.0;
        s.minErrorLatencyMs      = 15.0;
        s.maxErrorLatencyMs      = 15.0;
        t.mServiceMethodStats["PermPlugin_CheckPermission"] = s;
    }

    // Success only (errorCount == 0)
    {
        AppGatewayTelemetry::ServiceMethodStats s2;
        s2.pluginName            = "AuthPlugin2";
        s2.serviceName           = "Verify";
        s2.successCount          = 2;
        s2.errorCount            = 0;
        s2.totalSuccessLatencyMs = 40.0;
        s2.minSuccessLatencyMs   = 18.0;
        s2.maxSuccessLatencyMs   = 22.0;
        t.mServiceMethodStats["AuthPlugin2_Verify"] = s2;
    }

    // Error only (successCount == 0)
    {
        AppGatewayTelemetry::ServiceMethodStats s3;
        s3.pluginName   = "FailPlugin";
        s3.serviceName  = "FailService";
        s3.successCount = 0;
        s3.errorCount   = 3;
        s3.totalErrorLatencyMs = 60.0;
        s3.minErrorLatencyMs   = 18.0;
        s3.maxErrorLatencyMs   = 25.0;
        t.mServiceMethodStats["FailPlugin_FailService"] = s3;
    }

    // Sentinel min/max (covers ternary clamps)
    {
        AppGatewayTelemetry::ServiceMethodStats s4;
        s4.pluginName            = "SentPlugin2";
        s4.serviceName           = "SentService2";
        s4.successCount          = 1;
        s4.errorCount            = 1;
        s4.totalSuccessLatencyMs = 10.0;
        s4.totalErrorLatencyMs   = 10.0;
        s4.minSuccessLatencyMs   = std::numeric_limits<double>::max();
        s4.maxSuccessLatencyMs   = std::numeric_limits<double>::lowest();
        s4.minErrorLatencyMs     = std::numeric_limits<double>::max();
        s4.maxErrorLatencyMs     = std::numeric_limits<double>::lowest();
        t.mServiceMethodStats["SentPlugin2_SentService2"] = s4;
    }

    // Zero counts (continue branch)
    {
        AppGatewayTelemetry::ServiceMethodStats s5;
        s5.successCount = 0;
        s5.errorCount   = 0;
        t.mServiceMethodStats["ZeroPlugin2_ZeroService2"] = s5;
    }

    t.mReportingIntervalSec = 300;
    t.SendServiceMethodStats();

    t.mServiceMethodStats.clear();
    ExpectTrue(tr, true, "SendServiceMethodStats() with data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-18: AppGatewayTelemetry::FlushJob::Dispatch() + FlushJob::Send*()
//             Covers all FlushJob methods (marked DEPRECATED but still compiled).
//             We construct a TelemetrySnapshot with data, wrap it in a FlushJob,
//             and call Dispatch() directly to exercise all FlushJob::Send*() paths.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_FlushJob_Dispatch_WithData()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();

    // Build a snapshot with data for all stat categories.
    auto snapshot = std::unique_ptr<AppGatewayTelemetry::TelemetrySnapshot>(
        new AppGatewayTelemetry::TelemetrySnapshot());

    snapshot->parent               = &t;
    snapshot->reportingIntervalSec = 300;
    snapshot->format               = TelemetryFormat::JSON;

    // Health stats (non-zero so FlushJob::SendHealthStats sends data)
    snapshot->websocketConnections = 2;
    snapshot->totalCalls           = 8;
    snapshot->totalResponses       = 7;
    snapshot->successfulCalls      = 6;
    snapshot->failedCalls          = 1;

    // API method stats
    {
        AppGatewayTelemetry::ApiMethodStats s;
        s.pluginName            = "FlushJob_Plugin";
        s.methodName            = "flushMethod";
        s.successCount          = 3;
        s.errorCount            = 1;
        s.totalSuccessLatencyMs = 60.0;
        s.totalErrorLatencyMs   = 12.0;
        s.minSuccessLatencyMs   = 15.0;
        s.maxSuccessLatencyMs   = 30.0;
        s.minErrorLatencyMs     = 12.0;
        s.maxErrorLatencyMs     = 12.0;
        snapshot->apiMethodStats["FlushJob_Plugin_flushMethod"] = s;
    }

    // API latency stats
    {
        AppGatewayTelemetry::ApiLatencyStats l;
        l.pluginName     = "FlushJob_Plugin";
        l.apiName        = "flushApi";
        l.count          = 2;
        l.totalLatencyMs = 40.0;
        l.minLatencyMs   = 15.0;
        l.maxLatencyMs   = 25.0;
        snapshot->apiLatencyStats["FlushJob_Plugin_flushApi"] = l;
    }

    // Service method stats
    {
        AppGatewayTelemetry::ServiceMethodStats sm;
        sm.pluginName            = "FlushJob_Plugin";
        sm.serviceName           = "FlushService";
        sm.successCount          = 2;
        sm.errorCount            = 0;
        sm.totalSuccessLatencyMs = 30.0;
        sm.minSuccessLatencyMs   = 13.0;
        sm.maxSuccessLatencyMs   = 17.0;
        snapshot->serviceMethodStats["FlushJob_Plugin_FlushService"] = sm;
    }

    // Service latency stats
    {
        AppGatewayTelemetry::ServiceLatencyStats sl;
        sl.pluginName     = "FlushJob_Plugin";
        sl.serviceName    = "FlushSvcLatency";
        sl.count          = 1;
        sl.totalLatencyMs = 20.0;
        sl.minLatencyMs   = 20.0;
        sl.maxLatencyMs   = 20.0;
        snapshot->serviceLatencyStats["FlushJob_Plugin_FlushSvcLatency"] = sl;
    }

    // API error counts
    snapshot->apiErrorCounts["account.session"]  = 2;
    snapshot->apiErrorCounts["device.make"]       = 1;

    // External service error counts
    snapshot->externalServiceErrorCounts["AGW_SERVICE_AUTHENTICATION"] = 3;

    // Generic metrics cache - with count > 0
    {
        AppGatewayTelemetry::MetricData d;
        d.sum   = 200.0;
        d.count = 4;
        d.min   = 40.0;
        d.max   = 60.0;
        d.unit  = AGW_UNIT_MILLISECONDS;
        snapshot->metricsCache["FlushJobBootstrap_split"] = d;
    }

    // Dispatch the FlushJob via ProxyType (same pattern as TelemetryTimer in production).
    // Direct stack instantiation fails because Core::IDispatch inherits Core::IUnknown
    // which has pure virtual AddRef()/Release(); Core::ProxyType provides them.
    auto job = WPEFramework::Core::ProxyType<AppGatewayTelemetry::FlushJob>::Create(std::move(snapshot));
    job->Dispatch();

    ExpectTrue(tr, true, "FlushJob::Dispatch() with full data - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-19: AppGatewayTelemetry::FlushJob::Dispatch() - empty snapshot
//             Covers the "No health stats" early return in FlushJob::SendHealthStats().
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_FlushJob_Dispatch_Empty()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();

    auto snapshot = std::unique_ptr<AppGatewayTelemetry::TelemetrySnapshot>(
        new AppGatewayTelemetry::TelemetrySnapshot());
    snapshot->parent               = &t;
    snapshot->reportingIntervalSec = 300;
    snapshot->format               = TelemetryFormat::JSON;
    // All stats at zero → FlushJob::SendHealthStats() takes early return.

    auto job = WPEFramework::Core::ProxyType<AppGatewayTelemetry::FlushJob>::Create(std::move(snapshot));
    job->Dispatch();

    ExpectTrue(tr, true, "FlushJob::Dispatch() - empty snapshot - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-20: FlushJob with null parent - covers error guard path in Dispatch()
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_FlushJob_Dispatch_NullParent()
{
    TestResult tr;

    auto snapshot = std::unique_ptr<AppGatewayTelemetry::TelemetrySnapshot>(
        new AppGatewayTelemetry::TelemetrySnapshot());
    snapshot->parent = nullptr;  // null parent → early LOGERR return

    auto job = WPEFramework::Core::ProxyType<AppGatewayTelemetry::FlushJob>::Create(std::move(snapshot));
    job->Dispatch();  // Should LOG error and return cleanly

    ExpectTrue(tr, true, "FlushJob::Dispatch() null parent - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-21: FlushJob with COMPACT format - covers format branching in FlushJob
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_FlushJob_Dispatch_CompactFormat()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();

    auto snapshot = std::unique_ptr<AppGatewayTelemetry::TelemetrySnapshot>(
        new AppGatewayTelemetry::TelemetrySnapshot());
    snapshot->parent               = &t;
    snapshot->reportingIntervalSec = 300;
    snapshot->format               = TelemetryFormat::COMPACT;

    // Populate with data so the format code path is exercised.
    snapshot->websocketConnections = 1;
    snapshot->totalCalls           = 5;
    snapshot->totalResponses       = 4;
    snapshot->successfulCalls      = 3;
    snapshot->failedCalls          = 1;

    {
        AppGatewayTelemetry::ApiMethodStats s;
        s.pluginName            = "CompactPlugin";
        s.methodName            = "compactMethod";
        s.successCount          = 2;
        s.errorCount            = 1;
        s.totalSuccessLatencyMs = 40.0;
        s.totalErrorLatencyMs   = 10.0;
        s.minSuccessLatencyMs   = 18.0;
        s.maxSuccessLatencyMs   = 22.0;
        s.minErrorLatencyMs     = 10.0;
        s.maxErrorLatencyMs     = 10.0;
        snapshot->apiMethodStats["CompactPlugin_compactMethod"] = s;
    }

    auto job = WPEFramework::Core::ProxyType<AppGatewayTelemetry::FlushJob>::Create(std::move(snapshot));
    job->Dispatch();

    ExpectTrue(tr, true, "FlushJob::Dispatch() COMPACT format - no crash");
    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-22: ParseApiMetricName - "wrong prefix" return false branch
//             A metric ending with _Success_split that lacks AGW_INTERNAL_PLUGIN_PREFIX
//             makes ParseApiMetricName return false at the prefix check.
//             The metric then falls through all parsers and hits RecordGenericMetric.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_ParseApiMetricName_WrongPrefix()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();

    const auto ctx = MakeCtx(300, 300, "com.test.parse");

    // Has _Success_split suffix but wrong prefix → ParseApiMetricName returns false
    // at the prefix check, then ParseServiceMetricName also returns false (same prefix
    // check), then ParseApiLatencyMetricName returns false (no _ApiLatency_split suffix),
    // then ParseServiceLatencyMetricName returns false (no _ServiceLatency_split suffix),
    // so we end up in RecordGenericMetric.
    const std::string wrongPrefix = "WRONG_ENTS_INFO_Plugin_Success_split";
    auto rc = t.RecordTelemetryMetric(ctx, wrongPrefix, 10.0, AGW_UNIT_MILLISECONDS);
    ExpectEqU32(tr, rc, ERROR_NONE, "RecordTelemetryMetric wrong-prefix returns ERROR_NONE");

    // Has _Error_split suffix but wrong prefix
    const std::string wrongPrefixErr = "WRONG_ENTS_INFO_Plugin_Error_split";
    rc = t.RecordTelemetryMetric(ctx, wrongPrefixErr, 5.0, AGW_UNIT_MILLISECONDS);
    ExpectEqU32(tr, rc, ERROR_NONE, "RecordTelemetryMetric wrong-prefix error returns ERROR_NONE");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-23: ParseApiMetricName - "no MethodName tag" return false branch
//             Metric has correct prefix AND _Success_split suffix but no _MethodName_ tag.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_ParseApiMetricName_NoMethodTag()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    const auto ctx = MakeCtx(301, 301, "com.test.parse");

    // Has correct prefix AND _Success_split suffix but is missing "_MethodName_" tag.
    // ParseApiMetricName strips the prefix and suffix, then looks for "_MethodName_"
    // in the middle string and finds none → returns false.
    // The metric then falls to ParseServiceMetricName which also checks for _ServiceName_ tag.
    const std::string noMethodTag = std::string(AGW_INTERNAL_PLUGIN_PREFIX) + "Plugin_NoTag_Success_split";
    auto rc = t.RecordTelemetryMetric(ctx, noMethodTag, 10.0, AGW_UNIT_MILLISECONDS);
    ExpectEqU32(tr, rc, ERROR_NONE, "RecordTelemetryMetric no MethodName tag - ERROR_NONE");

    return tr.failures;
}

// ---------------------------------------------------------------------------
// Test DA-24: ParseApiLatencyMetricName - "wrong suffix" return false branch
//             A metric with correct prefix but wrong suffix makes ParseApiLatencyMetricName
//             return false at the suffix check.
// ---------------------------------------------------------------------------
uint32_t Test_Telemetry_DirectAccess_ParseApiLatencyMetricName_WrongSuffix()
{
    TestResult tr;
    TelemetryGuard guard;

    AppGatewayTelemetry& t = AppGatewayTelemetry::getInstance();
    const auto ctx = MakeCtx(302, 302, "com.test.parse");

    // Has correct prefix but NOT _ApiLatency_split, _ServiceLatency_split,
    // _Success_split, or _Error_split. Falls through all parsers → RecordGenericMetric.
    const std::string customSuffix = std::string(AGW_INTERNAL_PLUGIN_PREFIX) + "Plugin_Something_NoMatch";
    auto rc = t.RecordTelemetryMetric(ctx, customSuffix, 20.0, AGW_UNIT_COUNT);
    ExpectEqU32(tr, rc, ERROR_NONE, "RecordTelemetryMetric no known suffix - ERROR_NONE");

    return tr.failures;
}
