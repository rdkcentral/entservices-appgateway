# Spec Delta

## ADDED Requirements

### Requirement: Canonical focus-policy check for focus-sensitive methods
AppGatewayCommon SHALL expose a dedicated request-path policy check that evaluates whether a requesting app can execute focus-sensitive methods, using LifecycleDelegate-owned focus and active-state data. AppGatewayCommon SHALL own the canonical method map for focus-sensitive methods and SHALL apply focus-state enforcement only for methods in that map.

#### Scenario: Focus-sensitive method map lookup applies
- **WHEN** AppGateway sends a policy-check request with method name matching the configured focus-sensitive method map
- **THEN** AppGatewayCommon SHALL evaluate focus plus active-state alignment for the request context and return allow or deny

#### Scenario: Method not in focus-sensitive map
- **WHEN** AppGateway sends a policy-check request with method name not present in the focus-sensitive method map
- **THEN** AppGatewayCommon SHALL return allow without applying focus-state deny behavior

### Requirement: Standardized denied response for focus policy
For mapped focus-sensitive methods, if requesting app focus/state requirements are not satisfied, AppGatewayCommon SHALL return a denied policy result that uses Firebolt app-state-invalid contract: code `-50110` and message `App state invalid`.

#### Scenario: Focus or active-state requirement fails
- **WHEN** policy check runs for a mapped focus-sensitive method and LifecycleDelegate indicates caller is unfocused or not active
- **THEN** AppGatewayCommon SHALL return deny result with code `-50110` and message `App state invalid`
