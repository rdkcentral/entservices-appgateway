# Design

## Context

See [proposal.md](/Users/venkatgoli/Downloads/AppInfra/AppGW/RDKEMW-28029_Focus/entservices-appgateway/openspec/changes/focus-gate-sensitive-apis/proposal.md) for motivation.

`AppGatewayCommon` dispatches API methods from a centralized handler map in [AppGatewayCommon.cpp](/Users/venkatgoli/Downloads/AppInfra/AppGW/RDKEMW-28029_Focus/entservices-appgateway/AppGatewayCommon/AppGatewayCommon.cpp). Focus visibility already exists via `LifecycleDelegate::GetPresentationFocused` and internal focus/lifecycle registries in [LifecycleDelegate.h](/Users/venkatgoli/Downloads/AppInfra/AppGW/RDKEMW-28029_Focus/entservices-appgateway/AppGatewayCommon/delegate/LifecycleDelegate.h), but the listed focus-sensitive APIs currently forward without a shared pre-forward gate.

## Goals / Non-Goals

**Goals:**
- Add a single, reusable focus/state validation path for the five targeted APIs before delegate forwarding.
- Ensure blocked calls return the same Firebolt error contract (`-50110`, `App state invalid`) for all targeted APIs.
- Preserve existing request/response behavior for callers that satisfy focus/state requirements.

**Non-Goals:**
- Changing behavior of any APIs outside the five listed in the spec delta.
- Redefining lifecycle/focus semantics beyond existing `LifecycleDelegate` authority.
- Altering TTS/System delegate business logic for successful request execution.

## Decisions

### 1) Centralize gate logic in AppGatewayCommon
- Decision: add one shared precondition helper in AppGatewayCommon request handling path and invoke it from each targeted API handler.
- Why: keeps policy consistent and avoids drift across TTS/System method-specific wrappers.
- Alternative considered: duplicate checks inside each method wrapper. Rejected due to higher maintenance risk and inconsistent error formatting.

### 2) LifecycleDelegate as source of truth for focus/state
- Decision: derive allow/deny from LifecycleDelegate-managed focus and lifecycle state (focused + active-aligned).
- Why: existing runtime contract already tracks presentation focus and lifecycle transitions there.
- Alternative considered: querying downstream delegates directly for state. Rejected because it fragments state authority and adds cross-component coupling.

### 3) Uniform blocked-response mapping
- Decision: on failed precondition, do not call downstream delegate and return a normalized error payload containing `code: -50110` and `message: "App state invalid"`.
- Why: guarantees predictable Firebolt-aligned behavior and testability across all five APIs.
- Alternative considered: returning current delegate-specific transport errors. Rejected because those represent backend availability/issues, not app-state policy violations.

## Risks / Trade-offs

- [Risk] Focus/state can be temporarily stale during lifecycle/focus transitions.  
  [Mitigation] Use LifecycleDelegate’s existing synchronized registries and cover transition cases in L0/L1 tests.

- [Risk] New gate may unintentionally block calls where app identity mapping is missing.  
  [Mitigation] Define explicit fallback behavior and assert expected response shape in tests for unmapped app/session contexts.

- [Risk] Error payload shape mismatch with existing caller expectations.  
  [Mitigation] Validate exact code/message output in targeted tests for each gated API.

## Migration Plan

1. Introduce shared gating helper and integrate it with the five targeted handler paths.
2. Add/adjust unit/integration coverage for allow and deny paths per API.
3. Run existing AppGatewayCommon test suites and OpenSpec validation.
4. If regressions appear, rollback by removing gate invocations while preserving non-policy code paths.

## Open Questions

None.
