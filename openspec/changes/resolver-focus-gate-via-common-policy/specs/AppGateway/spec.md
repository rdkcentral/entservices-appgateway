# Spec Delta

## MODIFIED Requirements

### Requirement: COM-RPC and direct JSON-RPC routing
Methods with `useComRpc=true` SHALL call `IAppGatewayRequestHandler::HandleAppGatewayRequest`. Other non-event methods SHALL split the alias at its final dot and synchronously invoke the target through `Utils::GetThunderControllerClient`. For focus-sensitive methods configured by AppGatewayCommon policy, `FetchResolvedData` SHALL call the AppGatewayCommon focus-policy check before route-type dispatch and SHALL continue to COM-RPC or direct JSON-RPC forwarding only when policy allows.

#### Scenario: Focus-sensitive method allowed by policy
- **WHEN** a focus-sensitive method reaches resolver dispatch and AppGatewayCommon policy check returns allowed
- **THEN** AppGateway SHALL preserve normal route behavior for that method and forward it to its configured COM-RPC or direct JSON-RPC target

#### Scenario: Focus-sensitive method denied by policy
- **WHEN** a focus-sensitive method reaches resolver dispatch and AppGatewayCommon policy check returns denied
- **THEN** AppGateway SHALL NOT forward that method to either COM-RPC or direct JSON-RPC targets and SHALL return policy-denied payload from AppGatewayCommon with code `-50110` and message `App state invalid`

#### Scenario: COM-RPC context inclusion is enabled
- **WHEN** `additionalContext` is configured
- **THEN** parameters SHALL be wrapped as `params` plus `_additionalContext`, and `_additionalContext.origin` SHALL contain the request origin

#### Scenario: Direct JSON-RPC context inclusion is enabled
- **WHEN** `includeContext=true` on a direct route
- **THEN** parameters SHALL receive `context.appId`, `context.connectionId`, and `context.requestId`
