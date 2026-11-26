# Design

## Context

See [proposal.md](/Users/venkatgoli/Downloads/AppInfra/AppGW/RDKEMW-28029_Focus/entservices-appgateway/openspec/changes/resolver-focus-gate-via-common-policy/proposal.md) for motivation.

Current routing in `AppGatewayImplementation::FetchResolvedData` resolves alias, checks permission group, then dispatches by route type (event, COM-RPC, direct JSON-RPC). Focus/state policy currently lives inside selected AppGatewayCommon handlers, so policy checks happen after entry into downstream method handlers instead of at resolver dispatch boundary.

## Goals / Non-Goals

**Goals:**
- Enforce focus-sensitive policy in resolver flow before COM-RPC forwarding.
- Keep policy ownership and lifecycle-state authority in AppGatewayCommon/LifecycleDelegate.
- Keep denied response contract consistent: `-50110`, `App state invalid`.
- Avoid duplicating focus-state logic in AppGateway implementation.

**Non-Goals:**
- Changing lifecycle state ownership away from LifecycleDelegate.
- Expanding focus-state enforcement to methods outside the defined focus-sensitive set.
- Refactoring unrelated resolver/event/permission paths.

## Decisions

### 1) Resolver-level enforcement via dedicated AppGatewayCommon policy check
- Decision: Add a dedicated AppGatewayCommon policy-check method and invoke it from `FetchResolvedData` before route-type split, so mapped methods are denied before either COM-RPC or direct JSON-RPC forwarding.
- Why: gives earlier deny behavior (no downstream invocation on any route) while preserving single policy source.
- Alternative considered: enforce in `AppGatewayResponderImplementation` before acquiring resolver. Rejected because responder layer lacks routing metadata and would couple transport layer to policy scope.

### 2) Canonical focus-sensitive method map in AppGatewayCommon
- Decision: Keep one canonical method set in AppGatewayCommon for methods requiring focus+active validation.
- Why: policy is domain behavior, not routing metadata; centralization prevents drift between resolver and request-handler checks.
- Alternative considered: add method list in resolver config (`resolution.base.json`). Rejected because policy ownership would split across config and delegate logic.

### 3) Keep LifecycleDelegate as state source
- Decision: AppGatewayCommon policy check continues using LifecycleDelegate focus/active-state resolution.
- Why: existing lifecycle/focus registries and synchronization already live there.
- Alternative considered: resolver querying lifecycle services directly. Rejected due to duplicate state plumbing and increased coupling.

### 4) Fail-closed on policy-check service failures for mapped methods
- Decision: For methods in the focus-sensitive map, if policy-check invocation fails/unavailable, return deny contract and do not forward.
- Why: avoids bypassing policy due to service-path failures.
- Alternative considered: fail-open when policy-check service is unavailable. Rejected due to security/policy bypass risk.

## Risks / Trade-offs

- [Risk] Additional COM-RPC hop in resolver path can add latency.  
  Mitigation: limit check to mapped focus-sensitive methods and keep payload minimal.

- [Risk] Temporary mismatch if gate remains in both resolver and selected handlers during migration.  
  Mitigation: define staged rollout and shared helper usage to ensure same deny contract.

- [Risk] Inconsistent method matching due to case normalization differences.  
  Mitigation: normalize method keys once and use lower-case canonical map entries.

- [Risk] Behavior changes for malformed payload cases if policy-check runs before method-level validation.  
  Mitigation: preserve existing validation ordering expectations in AppGatewayCommon handler tests and explicitly cover ordering in L0/L1.

## Migration Plan

1. Add AppGatewayCommon policy-check entrypoint and canonical focus-sensitive map.
2. Wire resolver `FetchResolvedData` to call policy check for mapped methods before route-type split (event / COM-RPC / direct), and bypass gate for non-mapped methods.
3. Keep existing handler-level checks temporarily, then converge to single enforcement path once tests prove parity (or retain as defensive fallback by decision).
4. Update L0/L1 tests for resolver early-deny, no-forwarding behavior, and deny payload parity.
5. Validate OpenSpec change and targeted tests; rollback by removing resolver hook while keeping AppGatewayCommon behavior intact.

## Open Questions

None.
