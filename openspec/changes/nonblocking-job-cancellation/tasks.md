## 1. Reconcile Existing Work

This is the single current task list for the consolidated repository change. Checked entries preserve historical implementation/verification records, not tests rerun for this documentation revision. The specification now separates channel cancellation from retained-envelope lifetime. Activation isolation and comprehensive enforcement remain unmet requirements with open tasks below; consolidation does not mark them implemented.

- [x] 1.1 Re-inventory production WorkerPool submissions, callback setters, job classes, and owner captures changed by RDKEMW-24834; classify shared-channel, retained-owner, and detached/value-only paths.
- [x] 1.2 Replace the earlier blocking helper design and drain-based tests with the implemented shared/retained policy split, without keeping helper active-job drains.
- [x] 1.3 Confirm C++11, CMake, L0/L1 workflow, and ownership constraints before selecting the available shared/retained helpers; IntrusiveOwnerChannel is CPC-only.

## 2. Nonblocking Job Framework

- [x] 2.1 Add L0 coverage for shared-channel closure, observed-closed rejection, queued discard, expired owner, admission/closure outcomes, and teardown with queued work.
- [x] 2.2 Implement SharedOwnerChannel with idempotent closure and no active-job wait.
- [x] 2.3 Implement weak-owner envelopes with two closure checks and dispatch-local shared retention; allow already-admitted callbacks to finish.
- [ ] 2.4 Complete and verify the originally promised compile-positive/negative ownership fixtures beyond the existing shared Submit deleted overloads and std::function signature compatibility; do not claim those reject arbitrary captures.
- [x] 2.5 Run focused framework tests for queued weak ownership and dispatch-local retention; retained-queue references instead last until envelope destruction.

## 3. AppGatewayCommon Migration

- [x] 3.1 Use RetainedJobQueue for registration with retained emitters/current shutdown checks, and SharedOwnerChannel for production BaseEventDelegate event dispatch; registration is not channel-canceled.
- [x] 3.2 Migrate SystemDelegate's three and VideoOutputDelegate's four deferred jobs to weak-owner envelopes and remove their helper drains.
- [x] 3.3 Migrate NetworkDelegate worker work to the shared channel and remove its active-worker drain.
- [x] 3.4 Add lifecycle coverage for delegate teardown and pending shared-channel discard; do not interpret current-shutdown registration checks as activation isolation.

## 4. Remaining Plugin Jobs

- [x] 4.1 Use the retained policy for AppGateway responder/implementation jobs while preserving WebSocket and hibernation behavior; do not claim close/reject/discard semantics for these jobs.
- [x] 4.2 Use the retained policy for AppNotifications jobs and review emitter/subscriber reference behavior.
- [x] 4.3 Use the retained policy for AppActions jobs and review action/notification behavior.
- [x] 4.4 Migrate normal BaseEventDelegate Dispatch to the shared channel; the public legacy strong-owner job remains, including its direct L1 tests.

## 5. Enforcement

- [ ] 5.1 Complete the promised production-source enforcement coverage beyond the current explicit file list and narrow direct-submit/raw-pointer/PostToWorkerPool regex checks; verify capture/reference bypasses rather than assuming they are rejected.
- [ ] 5.2 Add the missing positive/negative cases for raw references, mixed/implicit/raw-this captures through approved helpers, and relevant source-list bypasses; preserve reviewed framework exceptions.
- [x] 5.3 Integrate the existing limited CMake guardrails without a runtime dependency or Thunder change and describe the reviewed boundaries.
- [x] 5.4 Reconcile docs/AsyncJobLifecycle.md with actual policies, cleanup order, telemetry, emitter ownership, enforcement limitations, and known gaps.

## 6. Verification

These checked command entries are preserved historical records, not a fresh execution or independent recertification of the original all-pass claims. Consult revision-specific logs and known infrastructure warnings before relying on them. Future code fixes require plugin builds and actual applicable L0/L1 execution again.

- [x] 6.1 Run `NO_TTY=1 ./docker-run.sh build.sh entservices-appgateway plugins --clean` locally and resolve failures.
- [x] 6.2 Run `NO_TTY=1 ./docker-run.sh build.sh entservices-appgateway l0 run --clean` locally and confirm all discovered L0 executables pass.
- [x] 6.3 Reproduce `.github/workflows/L1-tests.yml`, build and run every affected L1 executable, and confirm zero failures.
- [ ] 6.4 Re-run the full production inventory after follow-up fixes and verify each channel/retained/approved detached path against its actual contract; the original assertion that every job uses a nonblocking channel is not satisfied.
- [x] 6.5 Verify the historical source change excludes local scaffolding and changes to Thunder/dependencies/platform metadata; separate local-build/recipe work is not evidence about helper behavior.

## 7. Remaining Activation and Requirement Work

- [ ] 7.1 Add a controlled regression test that queues registration, deinitializes/reinitializes the same AppGatewayCommon instance, then dispatches the old job; demonstrate the current new-delegate access before fixing it.
- [ ] 7.2 Implement and verify activation isolation for AppGatewayCommon registration so old pending work cannot target the new delegate, without restoring a WorkerPool drain.
- [x] 7.3 Consolidate specification, design, proposal, and tasks in this repository's nonblocking-job-cancellation change; scope cancellation to channels, define retained-envelope lifetime separately, and retain unmet activation/enforcement requirements as open tasks.

## 8. Review Follow-up: Surviving Dependencies and Logical Shutdown

- [x] 8.1 Reproduce missing child shell ownership and post-cleanup event delivery in L0; reproduce AppNotifications destruction with a live subscription before fixing production code.
- [x] 8.2 Give each shell-dependent child balanced shell ownership through final destruction, and close all child notification channels plus System/Network/VideoOutput worker channels during SettingsDelegate cleanup without draining or prematurely releasing callback resources.
- [x] 8.3 Clean up AppNotifications subscriptions while shell and emitter are alive; use an independently reference-counted emitter with closed shared subscriber state so accepted asynchronous unregisters cannot leave callbacks pointing into a destroyed implementation.
- [x] 8.4 Check event-hook FetchResolvedData results, log genuine failures, preserve ERROR_NONE and asynchronous-pending semantics, and avoid unsolicited responses.
- [x] 8.5 Add and register L0/L1 regressions for retained-child shutdown, admitted post-deinitialize lookup, final subscription-job destruction, late emitter callbacks, and event-hook result handling.
- [x] 8.6 Build all four AppGateway plugins and run the full clean L0 suite against unchanged repository-specific dependencies.
- [x] 8.7 Configure and compile the CI L1 targets directly in Docker, execute all four test binaries, repeat focused lifecycle regressions, and record actual results.
- [x] 8.8 Reconcile the lifecycle guide and OpenSpec artifacts with these fixes, validate the change strictly, and review the final diff.

Broader producer-lifetime audits, throwing emitter behavior, and legacy-job removal decisions remain explicit review boundaries in design.md and the lifecycle guide. The activation-isolation and enforcement tasks above remain open.

### Review-fix verification

Revision: uncommitted working tree against 106eeec. Strict OpenSpec validation and git diff --check passed after final review. No dependency, build-support, or public interface changes were required.

All four plugins built: AppGateway, AppGatewayCommon, AppNotifications, and AppActions. The clean full L0 run executed 361 tests, all passing. The four L1 executables passed with exit status zero: AppGatewayL1Test 99, AppGatewayCommonL1Test 355, AppNotificationsL1Test 156, and AppActionsL1Test 35 (645 total, none skipped). Three new L0 cases and four new L1 cases cover the review fixes. Existing shell-reference expectations now require nine balanced references (plugin plus eight children), not unrestricted calls. All four focused L1 cases passed 50 repetitions each (200 additional executions).

Before the fixes, the retained-child L0 regression failed 17 assertions and AppNotifications final release with a live subscription exited with SIGSEGV (139). Both now pass. The unsupported build.sh entservices-appgateway l1 invocation returned 2 and is not test evidence; L1 was configured, compiled, and executed directly as below.

Verification used Ubuntu linux/amd64 Docker and unchanged repository-specific dependencies. Non-fatal CMake/compiler and mock-service/socket diagnostics remain. Recompiling the two shell-expectation test files produced stale gcov timestamp warnings on the next full run; test execution passed, and no coverage percentage is claimed. Valgrind was unavailable in the image. Coverity itself and hardware/L2 integration were not run.

Commands from local-build/ (paths factored into variables for readability):

```bash
NO_TTY=1 ./docker-run.sh env BUILD_JOBS=4 bash /workspace/local-build/build.sh entservices-appgateway plugins
NO_TTY=1 ./docker-run.sh env BUILD_JOBS=4 bash /workspace/local-build/build.sh entservices-appgateway l0 run --clean
P=/workspace/local-build/deps/entservices-appgateway/install/usr
B=/workspace/local-build/builds/entservices-appgateway/l1-review-fixes
NO_TTY=1 ./docker-run.sh cmake -G Ninja \
  -S /workspace/entservices-appgateway/Tests/L1Tests -B "$B" \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="$P" -DCMAKE_PREFIX_PATH="$P" \
  -DCMAKE_MODULE_PATH="${P%/usr}/tools/cmake" -DGENERIC_CMAKE_MODULE_PATH="${P%/usr}/tools/cmake" \
  -DHIDE_NON_EXTERNAL_SYMBOLS=OFF -DDISABLE_SECURITY_TOKEN=ON -DUSE_THUNDER_R4=ON \
  -DPLUGIN_APPGATEWAY=ON -DPLUGIN_APPGATEWAYCOMMON=ON -DPLUGIN_APPNOTIFICATIONS=ON -DPLUGIN_APPACTIONS=ON \
  '-DCMAKE_C_FLAGS=-O0 -g --coverage -fprofile-arcs -ftest-coverage' \
  '-DCMAKE_CXX_FLAGS=-O0 -g --coverage -fprofile-arcs -ftest-coverage' \
  -DCMAKE_EXE_LINKER_FLAGS=--coverage -DCMAKE_SHARED_LINKER_FLAGS=--coverage
NO_TTY=1 ./docker-run.sh cmake --build "$B" \
  --target AppGatewayL1Test AppGatewayCommonL1Test AppNotificationsL1Test AppActionsL1Test -j4
NO_TTY=1 ./docker-run.sh bash -c '
  set -e
  B=/workspace/local-build/builds/entservices-appgateway/l1-review-fixes
  P=/workspace/local-build/deps/entservices-appgateway/install/usr
  mkdir -p /tmp/app-gateway
  cp /workspace/entservices-appgateway/AppGateway/resolutions/resolution.base.json /tmp/app-gateway/resolution.base.json
  touch /etc/rdkappmanagers
  chmod 666 /etc/rdkappmanagers
  export APPGATEWAY_CONFIG_PATH=/tmp/app-gateway
  export LD_LIBRARY_PATH="$B/AppGateway:$B/AppGatewayCommon:$B/AppNotifications:$B/AppActions:$P/lib:$P/lib/wpeframework:$P/lib/wpeframework/plugins:$P/lib/wpeframework/proxystubs"
  status=0
  for target in AppGatewayL1Test AppGatewayCommonL1Test AppNotificationsL1Test AppActionsL1Test; do
    test -x "$B/$target"
    "$B/$target" || status=1
  done
  exit "$status"
'
```

Focused repeats used the same runtime library paths and `--gtest_repeat=50 --gtest_break_on_failure`, with filters `SettingsDelegateShutdown.*`, `AppNotificationsTest.FinalSubscriptionJobCleansUpBeforeReleasingDependencies`, and `AppGatewayPluginTest.EventHookReportsOnlyRealFailuresAndPreservesSubscriptionResponse` on their respective executables.
