# Tasks

## 1. Add AppGatewayCommon focus-policy interface and canonical method map

- [x] 1.1 Add a dedicated AppGatewayCommon policy-check entrypoint that accepts request context and method name, and verify AppGatewayCommon target compiles with the new interface wiring.
- [x] 1.2 Add canonical lower-case focus-sensitive method map in AppGatewayCommon (`texttospeech.speak`, `texttospeech.resume`, `speechsynthesis.speak`, `speechsynthesis.resume`, `device.timeinactivestate`) and verify non-mapped methods return allow.
- [x] 1.3 Reuse LifecycleDelegate-backed focus+active validation in the new policy-check path and verify deny response is exactly `code: -50110` and `message: App state invalid`.

## 2. Enforce policy at resolver dispatch boundary

- [x] 2.1 Update `AppGatewayImplementation::FetchResolvedData` to invoke AppGatewayCommon policy-check for mapped focus-sensitive methods before route-type split, and verify denied requests are not forwarded to COM-RPC or direct JSON-RPC targets (including `TextToSpeech.resume` direct alias route).
- [x] 2.2 Implement fail-closed handling for mapped methods when policy-check call is unavailable/fails, and verify resolver returns app-state-invalid contract without downstream dispatch.
- [x] 2.3 Preserve existing permission-group, event flow, and non-mapped route behavior, and verify existing unaffected resolver routing tests continue to pass.

## 3. Validate behavior, compatibility, and docs

- [x] 3.1 Add/extend L0 and L1 tests for resolver early gate (allow/deny/unavailable-policy paths) and verify expected response payload and no-forwarding assertions for denied calls.
- [x] 3.2 Add/extend tests for malformed payload cases to confirm prior validation-error behavior remains unchanged where required, and verify no regressions in existing TTS/SpeechSynthesis validation tests.
- [x] 3.3 Update AppGateway and AppGatewayCommon documentation to describe resolver-level enforcement with AppGatewayCommon policy ownership, and verify OpenSpec validation passes for this change.

## Workflow follow-up

- Archive the change after implementation and review are complete.
- Verify archived capability specs in `openspec/specs/AppGateway/spec.md` and `openspec/specs/AppGatewayCommon/spec.md`.
