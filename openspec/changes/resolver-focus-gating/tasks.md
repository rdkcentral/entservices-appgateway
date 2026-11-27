# Tasks

## 1. Resolver focus-gate integration

- [x] 1.1 Add resolver-owned gated-method inventory for `TextToSpeech.resume`, `SpeechSynthesis.speak`, `SpeechSynthesis.resume`, and `Device.timeInActiveState`, and verify method matching (including case-insensitive input) with focused L0 coverage.
- [x] 1.2 Add resolver policy-check hook before downstream dispatch for gated methods and verify denied/unavailable-policy paths return app-state-invalid (`-50110`) with no downstream forward in L0/L1 tests.
- [x] 1.3 Preserve focus-first behavior for gated methods and verify malformed payload cases still return app-state-invalid when policy denies in focused tests.

## 2. AppGatewayCommon policy-state contract

- [x] 2.1 Add or align AppGatewayCommon policy-state evaluation endpoint (LifecycleDelegate-backed) for resolver consumption and verify allow/deny outputs in AppGatewayCommon focused unit tests.
- [x] 2.2 Keep method-scope ownership out of AppGatewayCommon and verify no duplicate focus-sensitive method list is introduced in Common code/docs during review.

## 3. Documentation and integration verification

- [x] 3.1 Update relevant subsystem docs/spec text to reflect resolver-owned scope plus AppGatewayCommon state-only role, and verify consistency against the new change specs.
- [ ] 3.2 Run focused AppGateway and AppGatewayCommon L0/L1 suites for gating allow/deny/no-forward and focus-first precedence behavior, and verify all updated tests pass.
