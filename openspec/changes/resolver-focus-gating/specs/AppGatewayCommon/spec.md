# Spec Delta

## ADDED Requirements

### Requirement: Focus-policy state evaluation contract
AppGatewayCommon SHALL provide resolver-consumable focus-policy state evaluation for a caller context using LifecycleDelegate-derived state. This contract SHALL evaluate state only (focused/active eligibility) and SHALL NOT own or maintain the resolver’s gated-method inventory.

#### Scenario: Resolver asks for policy state
- **WHEN** resolver invokes AppGatewayCommon policy-state evaluation for a request context
- **THEN** AppGatewayCommon SHALL return an allow/deny outcome derived from lifecycle focus and active state

#### Scenario: Method-scope ownership remains in resolver
- **WHEN** AppGatewayCommon evaluates policy state
- **THEN** AppGatewayCommon SHALL NOT define, duplicate, or expand the set of focus-gated methods
