# Proposal

## Why

Focus-sensitive APIs are currently gated inside `AppGatewayCommon` request handlers, which means policy evaluation happens after resolver dispatch has already selected and entered downstream request paths. We want an earlier enforcement point in resolver flow while keeping a single source of truth for focus/state policy.

## What Changes

- Add a dedicated AppGatewayCommon policy-check interface that evaluates focus/state eligibility for incoming app context and method name, reusing existing lifecycle-driven focus/active-state logic.
- Add a focus-sensitive method map in AppGatewayCommon that defines which methods require focus+active validation.
- Enforce this policy in AppGateway resolver flow (`FetchResolvedData`) before route-type split (event / COM-RPC / direct JSON-RPC), so mapped methods are gated regardless of backend route.
- Preserve deny contract for policy failures:
  - `code: -50110`
  - `message: "App state invalid"`
- Do not forward focus-sensitive requests when policy check denies.
- Preserve successful forwarding behavior when policy check allows.

## Capabilities

### New Capabilities
- None

### Modified Capabilities
- `AppGateway`: add resolver-level pre-forward policy enforcement for focus-sensitive methods by querying AppGatewayCommon policy check.
- `AppGatewayCommon`: add dedicated focus-policy check entrypoint and canonical focus-sensitive method map used to make allow/deny decisions.

## Impact

- Affected code:
  - `AppGateway/AppGatewayImplementation.*` (resolver enforcement hook)
  - `AppGatewayCommon/AppGatewayCommon.*` (policy-check API + method map)
  - related L0/L1 tests for both resolver path and policy behavior
- API behavior impact:
  - Focus-sensitive methods are denied earlier in resolver flow across both COM-RPC and direct JSON-RPC routes, while keeping the same app-state-invalid response contract.
- Dependencies:
  - No new external dependencies expected; uses existing lifecycle/focus data ownership in AppGatewayCommon.
