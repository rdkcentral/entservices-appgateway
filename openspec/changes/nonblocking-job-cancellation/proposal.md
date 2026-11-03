## Why

Owner-dependent WorkerPool jobs can outlive plugin teardown, but waiting for every queued job makes deinitialization vulnerable to slow or stuck downstream calls. The repository needs a nonblocking, Rust-inspired weak-owner channel that discards pending work after closure without changing Thunder.

## What Changes

- Add a repository-local nonblocking job framework using closed-channel state, weak owner handles, and typed callbacks.
- Make queued jobs behave like writes delivered after a broken pipe: dispatch observes closure or an expired owner and returns without touching plugin state.
- Allow work that already upgraded its weak owner to finish under retained ownership without blocking the shutdown caller.
- Migrate owner-dependent asynchronous work with an existing close-before-release lifecycle hook, including AppGatewayCommon registration/event jobs and System/VideoOutput/Network delegate jobs; retain remote implementation jobs without such a hook as narrow self-retaining exceptions.
- Enforce approved owner policies and callback signatures through C++11 type constraints, and reject direct owner-dependent WorkerPool submissions and raw asynchronous owner captures through repository configure/static checks.
- Preserve detached Thunder-owned jobs through narrow reviewed allowlists.
- Add deterministic L0 and L1 coverage for closure, weak-owner expiry, queued discard, race outcomes, running-owner retention, and nonblocking teardown.

## Capabilities

### New Capabilities

- `async-job-cancellation`: Defines repository-wide nonblocking lifecycle and compile-time ownership rules for owner-dependent asynchronous jobs.

### Modified Capabilities

None.

## Impact

- Affected production areas include AppGateway, AppGatewayCommon and its delegates, AppNotifications, AppActions, shared event helpers, and their job classes.
- Affected tests include the corresponding L0 and L1 lifecycle suites.
- The implementation is repository-local and compatible with the pinned Thunder/WPEFramework and C++ standard; Thunder, dependency sources, Yocto metadata, and public wire APIs are unchanged.
- The completed `drain-lambda-worker-jobs` planning change is superseded by this broader nonblocking design; its uncommitted blocking implementation must be replaced rather than layered on top.