## Context

See [proposal.md](proposal.md) for motivation and scope. Current production uses C++11, Thunder IDispatch, shared-owned delegates, and intrusive plugin/implementation objects. SharedOwnerChannel and RetainedJobQueue are separate repository helpers; IntrusiveOwnerChannel exists only in app-gateway-cpc.

This is the consolidated design for the repository's unmerged RDKEMW-24834 change, alongside its [specification](specs/async-job-cancellation/spec.md) and tasks. It supersedes the drain-based design. The spec distinguishes channel cancellation from retained-envelope ownership; its activation-isolation and comprehensive enforcement requirements remain targets not fully implemented. Current behavior and those outstanding obligations are separated below. This documentation revision changes no implementation.

## Goals / Non-Goals

**Goals:**

- Remove helper-level WorkerPool drains from migrated paths while making owner retention explicit.
- Discard pending shared-channel work after observed closure/owner expiry and retain admitted callbacks through execution.
- Centralize submission-time intrusive retention for reviewed retained-job paths.
- Preserve event payloads, hibernation behavior, emitter references, and timing identities.
- Accurately separate implemented checks from unfinished activation/enforcement commitments.

**Non-Goals:**

- Change Thunder, dependencies, public interfaces, or platform metadata to implement the helpers.
- Forcefully interrupt callbacks or guarantee bounded-time plugin teardown.
- Treat object retention as synchronization for all members, producer callbacks, or captured dependencies.
- Add the CPC intrusive helper to this repository as part of a documentation update.

## Decisions

### Use weak ownership for shared delegate channels

SharedOwnerChannel stores shared closed-state and weak owner handles in each envelope. Submission accepts an existing shared owner, rejects null or observed closure, and stores std::function<void(Owner&)> work. Dispatch checks closure, upgrades weak ownership, checks closure again, then invokes the callback.

Close is idempotent and does not drain queued or admitted work. Closure after the final check can race with callback entry; that callback may run under retained ownership. The API has no Open operation, so activation reuse requires new channel state or an integration-specific generation policy.

Shared Submit deletes raw pointer/reference overloads. Callable compatibility does not forbid captures. Current helper callable parameters are passed by value; this is not a zero-copy or general move-only callable API.

**Alternative considered:** active counters and condition-variable drains. Rejected because they make teardown depend on queue/downstream progress. A closed flag plus unprotected raw owner was rejected because a flag check cannot establish lifetime.

### Use retained envelopes for reviewed intrusive paths

RetainedJobQueue acquires AddRef during envelope construction and Releases when the envelope is destroyed, including destruction without dispatch. Dispatch return alone does not release a reference held by a still-live proxy. There is no Close operation or execution/delivery guarantee.

This policy is used for AppGateway implementation/responder, AppNotifications, AppActions, and AppGatewayCommon registration. It avoids requiring new remote shutdown interfaces, but cannot promise queued-work cancellation. The caller must already guarantee lifetime before acquisition; retained ownership does not protect explicitly cleaned resources.

This repository's retained helper uses Work&& and std::forward into std::function. CPC's copy still takes Work by value. IntrusiveOwnerChannel is a cross-repository alternative only; its close-before-final-release contract cannot be assumed available here.

**Alternative considered:** applying a weak shared-pointer policy to all intrusive owners. Rejected without an ownership redesign because unrelated shared-pointer control blocks do not track all Thunder references.

### Snapshot and detach children under one lock

SettingsDelegate initialization, getters, and registration snapshots use mChildrenMutex. Cleanup swaps child pointers into detached storage under that mutex and releases detached storage outside it. Snapshots can retain children while cleanup drops the parent's copies.

AppGatewayCommon detaches its SettingsDelegate under mDelegateMutex and cleans it outside that lock. These steps address pointer races and lock/reentrancy boundaries; they do not close every surviving child's event channel at logical cleanup.

Actual destructor order differs by delegate: System/VideoOutput unsubscribe and reset resources before closing worker channels; Network closes its worker channel first. BaseEventDelegate closes its event channel before map-owned emitter cleanup. There is no uniform close-unregister-drain-release implementation.

### Keep AppGatewayCommon's current registration policy explicit

Submission checks current shutdown/delegate state, retains the emitter, and uses RetainedJobQueue<AppGatewayCommon>. Dispatch looks up GetDelegateSafe rather than capturing activation-specific state.

Deinitialize sets shutdown and detaches the delegate; Initialize installs a new delegate and clears shutdown. Consequently old queued registration work can reach a new activation on same-instance reuse. Activation isolation is not implemented on this path and remains a code/test follow-up. The current shutdown checks must not be described as a generation capability.

### Preserve production event and response behavior

Normal BaseEventDelegate Dispatch uses the weak event channel, but the legacy public strong-owner EventDelegateDispatchJob remains and AppGateway L1 tests still instantiate it. AddNotification is mutex-protected, not gated by an independent public logical event Close operation.

Emitter snapshots acquire temporary references under mRegisterMutex; the normal emission loop Releases outside that lock. It has no per-emitter RAII guard for throwing Emit calls.

Respond uses RetainedJobQueue, copies response inputs, and performs submission-time and dispatch-time hibernation checks. It has no channel close/rejection/discard contract. Timed envelopes inherit JobTiming; timed shared dispatch constructs telemetry before cancellation checks.

### Treat configure checks as limited guardrails

AppGatewayCommon/CMakeLists.txt supplies an explicit list to EnforceAsyncJobOwnership.cmake. Regex checks detect direct WorkerPool Submit syntax, a limited raw-pointer mParent/mOwner Job pattern, and raw-this/equals capture syntax passed specifically to PostToWorkerPool. A matched Thunder shell-job form is excluded; other reviewed boundaries include synchronously waited activation work and helper internals.

There is no comprehensive SFINAE/AST ownership analysis, capture checking for every helper, or proof of lifecycle order. Broader raw-reference/mixed-capture fixtures and source coverage remain unfinished. The existing tasks are reopened where they claimed those guarantees.

**Alternative considered:** documenting limited regex results as full ownership enforcement. Rejected; review and explicit negative coverage remain necessary. An AST implementation is not introduced by this documentation revision.

## Risks / Trade-offs

- **[Old registration reaches a new activation]** → Keep activation regression/fix work open; do not infer generations from shutdown flags.
- **[Explicit cleanup races with an admitted callback]** → Audit member ownership independently; shared retention defers final destruction only.
- **[Raw notification ingress precedes retention]** → Establish producer lifetime before submission rather than relying on a later AddRef/shared_from_this.
- **[Final destruction runs on a worker thread]** → Audit destructor thread assumptions and avoid lifecycle locks around downstream cleanup.
- **[Unsubscribe or destruction waits]** → Distinguish removal of queue drains from bounded-time teardown; external operations retain their own blocking behavior.
- **[Regex misses unsafe code]** → Keep broader enforcement and negative tests open; do not claim task completion based on existing simple fixtures.
- **[Reference cycles or permanently retained envelopes]** → Review captures and proxy disposal; reference counting alone does not prove absence of leaks.

## Migration and Verification Status

The shared/retained migrations are present. The earlier drain plan is preserved only in workspace `async-job-lifecycle-docs/drain-lambda-worker-jobs-history/`, outside this repository's OpenSpec changes. This change is the only current plan; do not reintroduce a drain to match historical text.

The consolidated specification replaces the earlier universal cancellation text with policy-specific contracts and carries forward emitter ownership, business/timing preservation, compatibility, and testable shutdown outcomes from the earlier effort. Blocking-drain requirements are superseded, not implemented by another name. Remaining implementation work in tasks.md covers activation isolation and enforcement/coverage; this planning revision implements none of those fixes.

Use controlled jobs/barriers for closed/expired dispatch, both admission race outcomes, admitted-owner retention, and envelope disposal. Test retained jobs at envelope destruction, not callback return; test resources and generations independently. Code changes require plugin builds and applicable L0/L1 execution in CI-parity Docker. Existing checked verification entries are historical evidence, not tests rerun for this documentation update.

Rollback of future code changes should keep the relevant policy, integration, and regression tests together. No persisted data or public API migration is introduced here.
