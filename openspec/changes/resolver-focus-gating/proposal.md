# Proposal

## Why

Focus-sensitive calls currently rely on downstream behavior and do not have one canonical enforcement point in resolver flow. We need a single, reviewable gate for a fixed method set so requests are either denied before forward or dispatched consistently.

## What Changes

- Add resolver-owned focus-policy gating for this fixed method set:
  - `TextToSpeech.resume`
  - `SpeechSynthesis.speak`
  - `SpeechSynthesis.resume`
  - `Device.timeInActiveState`
- Evaluate focus/active state through AppGatewayCommon (LifecycleDelegate-backed) from resolver flow.
- Keep fail-closed deny behavior when policy denies or policy check is unavailable, with no downstream forward for gated methods.
- Apply focus-policy precedence for gated methods (app-state-invalid deny occurs before downstream payload validation).
- Keep policy ownership in AppGateway resolver (no duplicate scope list in AppGatewayCommon).

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `AppGateway`: add resolver focus-policy gating contract, no-forward deny behavior, and focus-first precedence for the gated method set.
- `AppGatewayCommon`: define focus/active policy-evaluation endpoint contract used by resolver without owning method-scope inventory.

## Impact

- Affected code:
  - `AppGateway/AppGatewayImplementation.cpp` (resolver flow enforcement)
  - `AppGatewayCommon/AppGatewayCommon.cpp` and lifecycle-backed evaluation path
  - `AppGateway/resolutions/resolution.base.json` remains the routing catalog; no duplicate policy ownership introduced
- Affected tests:
  - AppGateway L0/L1 focused tests for deny/no-forward and precedence
  - AppGatewayCommon L0/L1 focused tests for policy-evaluation endpoint behavior
