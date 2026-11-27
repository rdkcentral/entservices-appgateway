# Spec Delta

## ADDED Requirements

### Requirement: Resolver focus-policy gating for selected methods
Resolver flow SHALL enforce focus-policy gating before downstream dispatch for exactly these method names (case-insensitive): `TextToSpeech.resume`, `SpeechSynthesis.speak`, `SpeechSynthesis.resume`, and `Device.timeInActiveState`. The method-scope inventory SHALL be owned in AppGateway resolver flow and SHALL NOT be duplicated in AppGatewayCommon.

#### Scenario: Gated method allowed by policy
- **WHEN** a request targets one of the gated methods and focus-policy evaluation returns allow
- **THEN** resolver SHALL continue normal dispatch using existing COM-RPC or direct JSON-RPC routing

#### Scenario: Gated method denied by policy
- **WHEN** a request targets one of the gated methods and focus-policy evaluation returns deny
- **THEN** resolver SHALL return app-state-invalid (`-50110`) and SHALL NOT forward the request downstream

#### Scenario: Policy evaluation unavailable for gated method
- **WHEN** a request targets one of the gated methods and focus-policy evaluation cannot be obtained
- **THEN** resolver SHALL fail closed with app-state-invalid (`-50110`) and SHALL NOT forward the request downstream

### Requirement: Focus-policy precedence for gated methods
For gated methods, resolver focus-policy evaluation SHALL run before downstream payload validation.

#### Scenario: Invalid payload for a gated method
- **WHEN** a gated method request payload is malformed or missing required fields according to the target method contract
- **THEN** a deny or unavailable focus-policy result SHALL return app-state-invalid (`-50110`) without downstream forward
