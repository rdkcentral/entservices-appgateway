## Purpose

Defines safe, nonblocking shutdown behavior for owner-dependent asynchronous jobs without requiring changes to the underlying WorkerPool implementation.

## ADDED Requirements

### Requirement: Closed job channel

Every owner-dependent asynchronous job SHALL be associated with a lifecycle channel that can be closed before owner teardown. Closing the channel SHALL prevent new owner-dependent work from being accepted and SHALL not wait for queued WorkerPool work to dispatch or finish.

#### Scenario: Shutdown closes the channel

- **WHEN** an owner begins shutdown
- **THEN** its lifecycle channel SHALL close and the shutdown caller SHALL continue without waiting for queued jobs

#### Scenario: Submission follows closure

- **WHEN** owner-dependent work is submitted after its channel is closed
- **THEN** submission SHALL be rejected without retaining or accessing the owner

### Requirement: Pending job discard

A queued owner-dependent job SHALL retain only a weak owner capability before dispatch. On dispatch it SHALL discard itself without invoking owner code when the channel is closed or the owner capability cannot be upgraded.

#### Scenario: Closed-channel job arrives from WorkerPool

- **WHEN** WorkerPool dispatches a queued job after its lifecycle channel has closed
- **THEN** the job SHALL return without accessing owner state or producing its event or response

#### Scenario: Owner expires before dispatch

- **WHEN** WorkerPool dispatches a job whose weak owner capability can no longer be upgraded
- **THEN** the job SHALL return without dereferencing an owner

### Requirement: Running job ownership

A job that successfully upgrades its owner capability while the channel is open SHALL retain that owner only for the duration of callback execution. Channel closure SHALL not block waiting for that callback, and final object destruction MAY be deferred until the retained callback ownership is released.

#### Scenario: Dispatch wins the closure race

- **WHEN** dispatch upgrades the owner before channel closure wins synchronization
- **THEN** the callback SHALL execute with retained ownership and release that ownership on every exit path

#### Scenario: Closure wins the dispatch race

- **WHEN** channel closure wins synchronization before dispatch upgrades the owner
- **THEN** dispatch SHALL be discarded

### Requirement: Typed ownership enforcement

Repository production code SHALL submit owner-dependent jobs only through approved typed ownership policies. Raw owner pointers, raw owner references stored in job envelopes, and asynchronous raw-`this` captures SHALL be rejected by compile/configure-time enforcement. Reviewed detached framework jobs MAY be explicitly allowlisted.

#### Scenario: Approved weak-owner callback

- **WHEN** a callback uses the approved owner capability and callable signature
- **THEN** repository configuration and compilation SHALL accept it

#### Scenario: Raw owner submission

- **WHEN** production code submits owner-dependent work using a raw owner or asynchronous raw-`this` capture
- **THEN** repository configuration or compilation SHALL fail

### Requirement: Repository-only implementation

The cancellation framework SHALL be implemented and verified entirely within this repository and SHALL use the pinned Thunder/WPEFramework WorkerPool without modifying Thunder, dependency sources, or platform metadata.

#### Scenario: Framework integration

- **WHEN** repository plugins submit approved asynchronous work
- **THEN** they SHALL continue to use the existing Thunder WorkerPool through the repository-owned job envelope
