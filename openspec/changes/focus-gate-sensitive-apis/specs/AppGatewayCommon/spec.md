# Spec Delta

## ADDED Requirements

### Requirement: Focus-sensitive API state gate
AppGatewayCommon SHALL validate requesting application focus/state using LifecycleDelegate before forwarding `TextToSpeech.speak`, `TextToSpeech.resume`, `SpeechSynthesis.speak`, `SpeechSynthesis.resume`, and `Device.timeInActiveState`. These operations SHALL be forwarded only when the requesting app satisfies required focus with corresponding active lifecycle state alignment.

#### Scenario: Focused active app invokes focus-sensitive API
- **WHEN** one of the listed focus-sensitive APIs is requested by an application that LifecycleDelegate resolves as focused with matching active state
- **THEN** AppGatewayCommon SHALL forward the request to the existing underlying delegate/implementation and preserve existing successful behavior

#### Scenario: Unfocused or non-active app invokes focus-sensitive API
- **WHEN** one of the listed focus-sensitive APIs is requested and LifecycleDelegate resolves that the requesting application does not satisfy required focus/state
- **THEN** AppGatewayCommon SHALL NOT forward the request and SHALL return an error payload with code `-50110` and message `App state invalid`
