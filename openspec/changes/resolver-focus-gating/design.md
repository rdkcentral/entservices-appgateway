# Design

## Context

See [proposal.md](./proposal.md) for motivation.  
AppGateway resolver is the single routing chokepoint for this method set, while AppGatewayCommon (LifecycleDelegate-backed) provides state evaluation.

## Goals / Non-Goals

**Goals:**
- Enforce focus-policy at one resolver point for the fixed four-method set.
- Keep policy scope ownership in AppGateway.
- Keep AppGatewayCommon as state evaluator only.
- Preserve focus-first policy behavior for gated methods.

**Non-Goals:**
- Moving focus-sensitive method inventory into AppGatewayCommon.
- Expanding this change to unrelated routing or lifecycle refactors.
- Generalizing policy to all methods in this change.

## Decisions

### 1) Single resolver-owned gated-method list
- Decision: AppGateway resolver owns the canonical list:
  - `TextToSpeech.resume`
  - `SpeechSynthesis.speak`
  - `SpeechSynthesis.resume`
  - `Device.timeInActiveState`
- Why: one enforcement location across both COM-RPC-routed and direct-alias-routed methods.
- Alternative considered: list ownership in AppGatewayCommon. Rejected to avoid duplicate policy ownership.

### 2) AppGatewayCommon provides state-only policy evaluation
- Decision: AppGatewayCommon evaluates focus/active state via LifecycleDelegate and returns allow/deny contract.
- Why: lifecycle-derived truth remains centralized while method-scope ownership remains in resolver.
- Alternative considered: AppGatewayCommon owning both scope and state. Deferred.

### 3) Fail-closed deny/no-forward behavior
- Decision: for gated methods, resolver returns app-state-invalid (`-50110`) and does not forward when policy denies or policy evaluation is unavailable.
- Why: prevent bypass under degraded policy-path conditions.
- Alternative considered: fail-open forwarding. Rejected as unsafe.

### 4) Focus-policy precedence
- Decision: focus-policy evaluation runs first for gated methods; deny/unavailable policy returns app-state-invalid before downstream validation.
- Why: enforces a strict fail-closed app-state gate for the selected sensitive methods.
- Alternative considered: payload-validation-first precedence. Rejected based on finalized requirement update.

## Risks / Trade-offs

- [Risk] Future entry paths may bypass resolver-owned gate if added outside current routing model.  
  Mitigation: keep tests/docs asserting resolver chokepoint invariant for gated methods.

- [Risk] Method-list drift when adding related APIs.  
  Mitigation: keep fixed inventory explicit and covered by focused resolver tests.

- [Risk] Focus-first precedence can mask malformed payload diagnostics when app state is denied.  
  Mitigation: document this contract explicitly and cover it in focused tests.

## Migration Plan

1. Add resolver focus-gate logic for the fixed method list.
2. Use AppGatewayCommon state-evaluation contract for allow/deny decision.
3. Enforce fail-closed deny/no-forward behavior for denied/unavailable policy checks.
4. Preserve focus-first precedence for gated methods.
5. Add focused L0/L1 coverage for allow, deny, unavailable-policy, and focus-first precedence cases.

Rollback: revert resolver gating hook and AppGatewayCommon policy-evaluation integration together.
