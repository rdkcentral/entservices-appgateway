## Why

Owner-dependent WorkerPool jobs can outlive plugin teardown, while draining every queued job can make shutdown depend indefinitely on WorkerPool progress or downstream calls. RDKEMW-24834 introduces explicit owner-lifetime policies without changing Thunder; this proposal records the implemented policy split and the remaining gaps rather than claiming universal cancellation.

## What Changes

- Use SharedOwnerChannel for shared-owned domain delegates and normal BaseEventDelegate event dispatch: pending envelopes hold weak owner handles, and dispatch retains an owner only after a successful upgrade and closure checks.
- Allow admitted channel callbacks to finish without a helper job drain. Retaining an object does not automatically protect members explicitly cleaned during logical shutdown.
- Use RetainedJobQueue for AppGateway implementation/responder, AppNotifications, AppActions, and AppGatewayCommon registration. These reviewed paths retain their owners from envelope construction to destruction and have no helper-level close/cancellation gate.
- Protect SettingsDelegate child-pointer snapshots and detachment with one mutex, then release detached children outside it and outside AppGatewayCommon's outer delegate lock.
- Preserve WebSocket/hibernation behavior, event payloads, emitter ownership, and existing timing identities.
- Apply the existing CMake regex guardrails to their explicit source list. Broader capture/reference detection and negative coverage remain incomplete; callable compatibility is not capture safety.
- Keep AppGatewayCommon registration activation isolation open: its jobs look up the current delegate at execution and can reach a new activation after same-instance reinitialization.
- Preserve reviewed Thunder-owned/synchronously waited work without treating allowlist descriptions as a general ownership proof.

## Capabilities

### New Capabilities

- `async-job-cancellation`: Channel-based queued-work discard and dispatch-time owner retention, with the implementation limitations and retained-job exceptions recorded in the design.

### Modified Capabilities

None.

## Impact

- Affected production areas include AppGateway, AppGatewayCommon and its delegates, AppNotifications, AppActions, shared event helpers, and their tests.
- The helpers remain repository-local and C++11-compatible. They do not require changes to Thunder, dependencies, public wire APIs, or Yocto metadata. Separate build-support/recipe work is not part of this helper contract.
- IntrusiveOwnerChannel is available only in app-gateway-cpc, not as an API in this repository.
- The earlier drain-lambda-worker-jobs plan has been moved out of OpenSpec into workspace-only `async-job-lifecycle-docs/drain-lambda-worker-jobs-history/`. Its original requirements and checked tasks are historical, not another active change or a dependency of this repository.

## Consolidated change status

This is the single authoritative unmerged change for RDKEMW-24834 in this repository. Its [specification](specs/async-job-cancellation/spec.md), [design](design.md), and [tasks](tasks.md) consolidate the prior drain-based and nonblocking plans. Pending-work cancellation is scoped to channels; reviewed retained paths have an explicit envelope-lifetime contract. Activation isolation and comprehensive enforcement remain target requirements with open implementation/test tasks, not claims of completed behavior. The older drain artifacts are historical only. No new build/test pass is claimed by this revision.
