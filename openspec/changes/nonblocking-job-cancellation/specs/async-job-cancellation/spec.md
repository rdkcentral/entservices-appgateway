## Purpose

Define the consolidated owner-lifetime and shutdown contract for asynchronous work in entservices-appgateway, distinguishing cancelable delegate work from reviewed retained implementation work without modifying Thunder.

## ADDED Requirements

### Requirement: Explicit asynchronous ownership policy

Owner-dependent jobs SHALL use an approved typed callback receiving a valid owner. Shared-owned delegate work SHALL use cancelable weak-owner channels. Reviewed intrusive implementation and registration paths SHALL use submission-time retained envelopes where cancellation is not provided. The design SHALL identify each policy's integrations and exceptions; neither policy SHALL be described as a synchronous queue drain.

#### Scenario: Delegate job is queued

- **WHEN** shared-owned delegate work is queued through its channel
- **THEN** the envelope's owner handle SHALL NOT retain the owner before dispatch

#### Scenario: Reviewed implementation job is queued

- **WHEN** an intrusive implementation job uses the reviewed retained policy
- **THEN** its envelope SHALL retain the owner from construction until envelope destruction without claiming channel cancellation

### Requirement: Channel closure and pending discard

Closing a delegate channel SHALL be idempotent and SHALL NOT wait for queued jobs or admitted callbacks to finish. Submission that observes closure SHALL be rejected. Dispatch SHALL discard business work when it observes closure or cannot acquire shared owner lifetime. An admitted callback that passes the final closure check SHALL be permitted to complete even when closure precedes callback entry.

#### Scenario: Submission observes a closed channel

- **WHEN** creation observes that its channel is closed
- **THEN** no valid envelope SHALL be created for that submission

#### Scenario: Pending job observes closure or expiry

- **WHEN** queued work dispatches and observes a closed channel or expired owner
- **THEN** it SHALL return without invoking its business callback

#### Scenario: Closure follows final admission check

- **WHEN** dispatch retains the owner and passes its final closure check before concurrent closure
- **THEN** closure SHALL NOT wait for or forcibly interrupt that callback

### Requirement: Dispatch-local delegate retention

An admitted channel callback SHALL retain the delegate throughout invocation and release its dispatch-local ownership on normal return or stack unwinding. Final destruction SHALL NOT occur while that ownership remains. Explicit resource cleanup SHALL NOT be assumed safe merely because the delegate object remains alive.

#### Scenario: External ownership ends during dispatch

- **WHEN** the final external shared owner is released while an admitted callback retains the delegate
- **THEN** final destruction SHALL remain deferred until the dispatch-local ownership is released

### Requirement: Retained envelope lifetime

A retained job SHALL acquire its owner reference while the caller already guarantees that the owner is alive, and SHALL balance that reference when the envelope is destroyed, including disposal without dispatch. Callback return SHALL NOT imply reference release while another envelope proxy remains. Acceptance SHALL NOT be represented as a guarantee of execution or delivery.

#### Scenario: Extra proxy survives dispatch

- **WHEN** a retained job finishes dispatch but a caller still holds its envelope
- **THEN** the envelope SHALL continue retaining its owner until final envelope destruction

#### Scenario: Envelope never dispatches

- **WHEN** a retained envelope is destroyed without dispatch
- **THEN** its retained owner reference SHALL be released without invoking the callback

### Requirement: Registration activation isolation

Pending registration work from a prior activation SHALL NOT access a replacement delegate after the same plugin instance is deinitialized and reinitialized. Work already admitted against the previous activation SHALL use retained previous-activation state rather than resolving the new delegate.

#### Scenario: Old registration dispatches after reinitialization

- **WHEN** registration is queued in one activation and dispatches only after same-instance reinitialization
- **THEN** it SHALL NOT register with the replacement activation's delegate

### Requirement: Synchronized delegate detachment and emitter ownership

Child delegate lookup and logical detachment SHALL use a common synchronization boundary. Destructive release SHALL occur outside outer lifecycle locks that callbacks may require. Event-emitter registrations and dispatch snapshots SHALL own balanced references on supported normal-return paths, and emission SHALL occur outside the registration-map lock.

#### Scenario: Cleanup races with a child snapshot

- **WHEN** registration obtains a child snapshot while cleanup detaches the parent's child references
- **THEN** the snapshot SHALL retain the child for its invocation without requiring destruction under the parent lock

#### Scenario: Emitter is removed after snapshot

- **WHEN** an emitter registration is removed after a dispatch snapshot acquires its reference
- **THEN** the snapshot reference SHALL preserve emitter lifetime through normal emission and SHALL be released afterward

### Requirement: Deferred state re-query

External callbacks requiring refreshed device state SHALL defer downstream getter work to the existing worker queue before emitting the corresponding events. The re-query SHALL use the owning delegate's channel contract rather than raw-owner capture or active-work draining. Discarding or disposing the envelope SHALL release its held state without invoking the business callback.

#### Scenario: Display state change requires refreshed values

- **WHEN** a display-state notification requires updated screen or video values while its channel accepts work
- **THEN** refreshed values SHALL be queried through deferred owner-safe work before the corresponding event is dispatched

#### Scenario: Re-query envelope is disposed before dispatch

- **WHEN** the deferred envelope is destroyed without invoking its callback
- **THEN** its held state SHALL be released without a downstream re-query or event emission

### Requirement: Business behavior and telemetry preservation

Discarded channel work SHALL NOT emit business events or responses. Executed work SHALL preserve existing payloads, existing ordering guarantees, hibernation checks, and timing identities. Timing scopes SHALL remain valid even when a timed envelope discards its business callback; cancellation SHALL NOT be equated with absence of telemetry.

#### Scenario: Response becomes paused while queued

- **WHEN** a retained response job reaches dispatch after its connection's registered application becomes paused
- **THEN** the dispatch-time hibernation check SHALL suppress the socket response

#### Scenario: Timed channel work is discarded

- **WHEN** timed dispatch observes closure
- **THEN** the business callback SHALL be skipped while existing timing construction remains valid

### Requirement: Ownership enforcement and verification

Production work SHALL use approved ownership policies or documented framework-owned/synchronously waited exceptions. Compile/configure enforcement SHALL reject direct unapproved submissions, unprotected job-owner fields, and unsafe asynchronous owner captures across affected production paths. Tests SHALL exercise approved forms, prohibited raw-reference/capture forms, closure races, envelope disposal, and activation reuse. An explicit source list or a signature-compatible callback alone SHALL NOT be reported as comprehensive enforcement.

#### Scenario: Unsafe asynchronous capture bypasses a helper

- **WHEN** an affected production job captures an unprotected owner through a raw pointer, raw reference, or implicit/mixed capture
- **THEN** compile/configure enforcement SHALL reject it rather than treating typed callback compatibility as proof of safety

#### Scenario: Lifecycle result is reported

- **WHEN** verification is reported for a code revision
- **THEN** the report SHALL identify actual plugin builds and applicable L0/L1 executions, counts, and blockers without treating skipped or no-op invocations as passes

### Requirement: Repository-local compatibility

The helpers SHALL use the pinned Thunder WorkerPool and supported C++ standard without requiring changes to Thunder, dependencies, or public wire APIs. The implementation SHALL NOT require a workspace-only design document at build or runtime.

#### Scenario: Standalone source checkout

- **WHEN** this repository is built with its supported dependencies without sibling workspace documentation
- **THEN** its job framework SHALL remain buildable without importing workspace-only artifacts
