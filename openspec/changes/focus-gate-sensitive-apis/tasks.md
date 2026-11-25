# Tasks

## 1. Add shared focus/state gate for targeted APIs

- [ ] 1.1 Add a reusable AppGatewayCommon helper that validates requesting app focus/state through LifecycleDelegate and verify compilation succeeds for AppGatewayCommon target.
- [x] 1.2 Wire gate checks into `TextToSpeech.speak`, `TextToSpeech.resume`, `SpeechSynthesis.speak`, `SpeechSynthesis.resume`, and `Device.timeInActiveState` request paths and verify each path short-circuits before delegate forwarding when gate fails.
- [x] 1.3 Implement a single blocked-response builder returning `code: -50110` and `message: "App state invalid"` and verify response payload shape is identical across all five gated APIs.

## 2. Verify behavior with focused and unfocused callers

- [x] 2.1 Add/extend AppGatewayCommon tests for focused+active callers and verify each gated API preserves existing successful forwarding behavior.
- [x] 2.2 Add/extend AppGatewayCommon tests for unfocused/non-active callers and verify each gated API returns `-50110` with `App state invalid` and does not invoke downstream delegate.
- [x] 2.3 Add/extend tests for edge contexts (unknown/unmapped app identity and transition-adjacent state) and verify deterministic deny behavior without crashes.

## 3. Documentation and validation

- [x] 3.1 Update AppGatewayCommon documentation to describe focus-state enforcement for the five APIs and verify wording matches spec delta semantics.
- [ ] 3.2 Run OpenSpec validation and targeted AppGatewayCommon test commands and verify all updated/added tests pass.

## Workflow follow-up

- Archive the change after implementation and review are complete.
- Verify archived spec content in `openspec/specs/AppGatewayCommon/spec.md`.
