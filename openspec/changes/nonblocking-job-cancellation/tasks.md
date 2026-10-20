## 1. Reconcile Existing Work

- [x] 1.1 Re-inventory every production WorkerPool submission, callback setter, job class, and raw owner capture changed by RDKEMW-24834; classify owner-dependent and detached/value-only work.
- [x] 1.2 Replace the uncommitted blocking `OwnedWorkerPool` design and its drain-based tests rather than retaining active counters, permits, condition variables, or destructor waits.
- [x] 1.3 Confirm C++11, CMake, L0/L1 workflow, and ownership constraints for every affected plugin before selecting shared and intrusive policies.

## 2. Nonblocking Job Framework

- [x] 2.1 Add failing L0 tests for channel closure, rejection after closure, queued discard, expired weak owner, dispatch/close race outcomes, and shutdown returning with queued work.
- [x] 2.2 Implement the closeable shared channel with idempotent nonblocking closure and no active-job wait.
- [x] 2.3 Implement the shared-pointer weak-owner envelope with two closure checks, dispatch-time weak upgrade, and callback-scoped retained ownership.
- [x] 2.4 Implement C++11 type constraints and deleted/inaccessible raw owner overloads, with compile-positive and compile-negative fixtures.
- [x] 2.5 Run focused framework tests and verify queued jobs do not retain owners while running jobs retain owners only through callback completion.

## 3. AppGatewayCommon Migration

- [x] 3.1 Migrate registration and shared event-dispatch jobs from blocking active-job accounting to nonblocking channel cancellation.
- [x] 3.2 Migrate SystemDelegate's three and VideoOutputDelegate's four deferred jobs to weak-owner envelopes and remove destructor drains.
- [x] 3.3 Migrate NetworkDelegate worker work and remove its blocking active-worker drain after equivalent race tests pass.
- [x] 3.4 Add deterministic L0/L1 tests proving AppGatewayCommon, SystemDelegate, VideoOutputDelegate, and NetworkDelegate teardown returns while pending jobs later discard.

## 4. Remaining Plugin Jobs

- [x] 4.1 Review AppGateway responder and implementation jobs; migrate only owners with an existing logical shutdown hook and classify remote implementation jobs without one as self-retaining exceptions while preserving WebSocket and hibernation behavior.
- [x] 4.2 Review AppNotifications jobs; preserve self-retaining implementation jobs without an existing shutdown hook as allowlist entries and verify emitter/subscriber reference semantics.
- [x] 4.3 Review AppActions jobs; preserve self-retaining implementation jobs without an existing shutdown hook as allowlist entries and verify action/notification behavior.
- [x] 4.4 Confirm BaseEventDelegate and every migrated plugin job either uses the framework or is a reviewed detached/value-only allowlist entry.

## 5. Enforcement

- [x] 5.1 Extend repository configure/static enforcement across all affected production paths to reject direct owner-dependent submissions, job raw-owner fields, and asynchronous `[this]` or implicit `[=]` captures.
- [x] 5.2 Add positive and negative enforcement fixtures for shared weak ownership, closure, raw pointers/references, mixed capture lists, and allowlisted Thunder-owned jobs.
- [x] 5.3 Integrate enforcement without a runtime dependency or any Thunder change and document the narrow allowlist in repository configuration.
- [x] 5.4 Document lifecycle semantics, ownership policies, event notifications, telemetry, enforcement, tests, and review requirements in `docs/AsyncJobLifecycle.md`.

## 6. Verification

- [x] 6.1 Run `NO_TTY=1 ./docker-run.sh build.sh entservices-appgateway plugins --clean` locally and resolve failures.
- [x] 6.2 Run `NO_TTY=1 ./docker-run.sh build.sh entservices-appgateway l0 run --clean` locally and confirm all discovered L0 executables pass.
- [x] 6.3 Reproduce `.github/workflows/L1-tests.yml`, build and run every affected L1 executable, and confirm zero failures.
- [x] 6.4 Re-run the production inventory and verify every owner-dependent RDKEMW-24834 job uses the nonblocking framework and no destruction path waits for WorkerPool completion.
- [x] 6.5 Verify Git status contains changes only in `entservices-appgateway`, excludes local scaffolding, and does not modify Thunder, dependency sources, local-build, or Yocto metadata.
