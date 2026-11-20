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

Resource/producer lifetime, throwing emitter behavior, and legacy-job removal decisions remain explicit review boundaries in design.md and the lifecycle guide. This documentation revision does not implement those changes.
