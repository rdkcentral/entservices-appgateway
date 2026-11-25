# Proposal

## Why

Focus-sensitive operations are currently forwarded without enforcing requesting-app focus/state in AppGatewayCommon. This allows non-focused apps to invoke APIs that Firebolt classifies as requiring an appropriate application state.

## What Changes

- Add focus/state validation in AppGatewayCommon before forwarding:
  - `TextToSpeech.speak`
  - `TextToSpeech.resume`
  - `SpeechSynthesis.speak`
  - `SpeechSynthesis.resume`
  - `Device.timeInActiveState`
- Use `lifecycleDelegate` to resolve requesting app focus, with internal alignment to active lifecycle state for these operations.
- Block forwarding when required focus/state is not satisfied and return Firebolt app-state-invalid error:
  - `code: -50110`
  - `message: "App state invalid"`
- Preserve current behavior for valid focused/active requests.

## Capabilities

### New Capabilities
- None

### Modified Capabilities
- `AppGatewayCommon`: add normative focus/state gate behavior for selected focus-sensitive APIs, including explicit non-forwarding and Firebolt app-state-invalid error responses.

## Impact

- Affected area: `AppGatewayCommon` request routing/validation path.
- API behavior impact: conditional error response for unfocused/non-active callers on the five listed APIs.
- No new external dependencies expected.
