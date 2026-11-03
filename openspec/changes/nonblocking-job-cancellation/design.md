## Context

See `proposal.md`. The repository uses C++11 production targets, Thunder `Core::IDispatch`, shared-pointer delegates, intrusive COM objects, and several custom active-job drains. The current uncommitted `SharedOwnerChannel` implementation blocks delegate destruction and must be replaced. `BaseEventDelegate` already derives from `enable_shared_from_this`, and SettingsDelegate creates domain delegates with `make_shared`.

## Goals / Non-Goals

**Goals:**

- Make shutdown return without waiting for queued WorkerPool jobs.
- Give queued jobs only weak owner authority and discard them after channel closure.
- Let work already executing retain its owner without blocking the shutdown caller.
- Provide one repository-wide typed API for every owner-dependent job modified by RDKEMW-24834.
- Detect direct submissions and unsafe captures during repository configuration or compilation.

**Non-Goals:**

- Modify Thunder, WorkerPool, dependency checkouts, Yocto recipes, or public APIs.
- Forcefully interrupt callbacks already executing.
- Convert framework-owned detached shell jobs that do not reference repository owners.

## Decisions

### Model lifecycle as a closeable channel

A shared channel state contains an atomic closed flag and no owner pointer. Jobs keep the channel and `weak_ptr<Owner>`. Closing is idempotent and nonblocking. Submission checks closure before creating the envelope; dispatch checks closure, upgrades the weak owner, checks closure again, and invokes only when all checks succeed.

This mirrors Rust channel closure plus `Weak::upgrade()`. The second closure check prevents ordinary queued work from starting after shutdown. If closure races after the final check, the upgraded `shared_ptr` keeps the owner valid; shutdown still does not wait.

**Alternative considered:** active counters and condition-variable drains. Rejected because teardown can block indefinitely.

### Require shared ownership for shared-pointer policy

The typed API accepts `shared_ptr<Owner>` or an explicit weak capability; raw pointer/reference overloads are deleted. Callbacks are constrained to a signature receiving a retained owner, and the envelope stores no raw owner member. Delegates obtain a derived shared pointer from their existing shared ownership before submission.

**Alternative considered:** a shared cancellation flag plus raw owner. Rejected because checking the flag cannot prevent destruction between the check and dereference.

### Keep a policy-based API

Use a common channel/envelope shape with repository-local ownership policies. The entservices shared policy uses `weak_ptr`; intrusive policy support is available for COM-owned jobs that cannot use shared ownership. Intrusive channels must close from explicit logical shutdown while a known strong reference remains and before final `Release()`; destructor-only first closure is forbidden. This keeps call sites consistent without changing Thunder.

### Migrate owners with an existing logical shutdown hook

Inventory AppGateway, AppGatewayCommon delegates, BaseEventDelegate, AppNotifications, and AppActions. Migrate jobs whose owner can close a channel while a known strong reference remains. Remote implementation objects that expose no shutdown method through their existing Exchange interfaces SHALL NOT receive a new interface for this change; their self-retaining AddRef/Release jobs remain narrow reviewed exceptions. Keep reviewed `PluginHost::IShell::Job` and value-only jobs on the allowlist.

### Enforce at type and repository levels

C++11 SFINAE/type traits enforce approved owner policies and callable signatures. Repository configure/static checks reject direct owner-dependent WorkerPool calls, raw parent members in dispatch jobs, and `[this]`/implicit `[=]` asynchronous captures outside allowlisted framework jobs. No external compiler plugin is required for target builds; an AST check may replace the configure check when supported infrastructure exists.

### Test queue state deterministically

Tests control dispatch with mock jobs and barriers. Required states are queued-before-close, dispatched-after-close, expired owner, dispatch/close race, running retained owner, and shutdown return while queued work remains. Sleep-based proof is not accepted where a barrier or future can establish the state.

## Risks / Trade-offs

- **[Final destructor can run on a WorkerPool thread]** → Keep destructors thread-safe and separate logical channel closure from final reference release.
- **[External unsubscribe calls can still have their own timeout]** → This change removes job-drain waits only; audit subscription APIs separately and do not hold job-channel locks around unsubscribe.
- **[Callback starts immediately before closure]** → Retained ownership guarantees safety; cancellation is cooperative, not forced interruption.
- **[Weak upgrade unavailable for intrusive owner]** → Use the repository-owned intrusive policy with synchronized AddRef/detach rather than raw pointers.
- **[Static check false positives]** → Maintain narrow path/symbol allowlists reviewed with the production inventory.

## Migration Plan

1. Replace the blocking helper and tests with closeable-channel weak-owner semantics.
2. Migrate System, VideoOutput, Network, registration, event-dispatch, responder, notification, and action jobs identified by inventory.
3. Remove redundant active counters, condition variables, permits, and shutdown waits only after each path uses the new envelope.
4. Enable configure/compile enforcement and reviewed allowlists.
5. Run clean plugin, full L0, and every affected L1 executable locally in Docker.
6. Commit only repository production code, tests, enforcement configuration, and OpenSpec artifacts; exclude local scaffolding.
