## Context

See proposal.md for motivation and scope. The current AppGatewayCommon implementation already routes multiple Device GET methods through a lowercase handler map and forwards Thunder-backed lookups through `SystemDelegate`. Existing Device branding methods (`Device.osName`, `Device.osVersion`, `Device.firmware`) already use `Exchange::IDeviceInfo` and normalize string output as JSON strings. `Device.brandName` is currently absent from resolution routing, handler registration, delegate facade, and L1 branding tests.

## Goals / Non-Goals

**Goals:**
- Add a GET-only Firebolt method path for `Device.brandName` in AppGateway.
- Reuse existing COM-RPC integration to Thunder `DeviceInfo` without introducing new dependencies.
- Preserve established AppGatewayCommon error and response conventions for DeviceInfo-backed getters.
- Add L1 coverage for success and failure behavior specific to `Device.brandName`.

**Non-Goals:**
- No SET support for brand name in Firebolt scope for this change.
- No changes to Thunder `IDeviceInfo` interface definitions or ownership.
- No refactor of existing AppGateway method-dispatch architecture.
- No behavior changes for other Device methods.

## Decisions

1. Route through existing AppGatewayCommon + SystemDelegate layering
- Decision: Implement `Device.brandName` using the same flow as other DeviceInfo-backed getters (`resolution.base.json` -> handler map in AppGatewayCommon -> SystemDelegate -> `IDeviceInfo`).
- Rationale: Minimizes risk by following proven code paths and test patterns already used for branding APIs.
- Alternative considered: Direct COM-RPC call from AppGateway routing layer. Rejected because it bypasses existing plugin boundaries and would fragment method handling conventions.

2. Use Thunder `DisplayBrandName(DeviceDisplayBrandName&)` as source of truth
- Decision: Bind Firebolt `Device.brandName` to Thunder property `displaybrandname` via `Exchange::IDeviceInfo::DisplayBrandName(...)` getter.
- Rationale: Matches requested mapping and avoids introducing value translation.
- Alternative considered: Use `Brand(DeviceBrand&)`. Rejected because requirement explicitly targets display brand name and not generic brand metadata.

3. Preserve existing output and error normalization
- Decision: Return a JSON string payload using existing `Core::JSON::String` conversion conventions; map getter success/failure consistent with current DeviceInfo-based methods.
- Rationale: Keeps external behavior predictable and aligned with AppGatewayCommon contracts.
- Alternative considered: Introduce new response object shape or custom error envelope. Rejected to avoid incompatible response changes.

4. Extend existing branding L1 suite
- Decision: Add `Device.brandName` tests inside existing `AppGatewayCommon_branding_test.cpp`.
- Rationale: Reuses established fixture/mocks and keeps branding API verification centralized.
- Alternative considered: New standalone test file. Rejected because it duplicates setup and increases maintenance overhead.

## Risks / Trade-offs

- [Risk] Thunder `DeviceInfo` service unavailable at runtime -> Mitigation: Preserve existing unavailable-path handling and add explicit L1 test case.
- [Risk] Method-name mismatch (`device.brandName` vs lowercase dispatch key) -> Mitigation: Add resolution entry and case-insensitivity tests through `HandleAppGatewayRequest`.
- [Risk] Divergence from requested error semantics for service-unavailable vs getter-failure paths -> Mitigation: Document expected mapping and validate in L1 tests for both `ERROR_GENERAL` and `ERROR_UNAVAILABLE` paths.
- [Trade-off] Additive API increases surface area slightly -> Mitigation: Scoped to one GET method with no ABI/interface signature changes.

## Migration Plan

1. Add routing entry for `device.brandName` in `resolution.base.json`.
2. Add AppGatewayCommon handler-map route and helper method declaration/definition.
3. Add SystemDelegate getter implementation that calls `IDeviceInfo::DisplayBrandName(...)`.
4. Extend L1 branding tests and mocks for new method behavior.
5. Update API reference docs for new Device method.
6. Validate build/tests in existing AppGatewayCommon test targets.

Rollback strategy:
- Revert the resolution entry and corresponding AppGatewayCommon/SystemDelegate/test/doc updates as a single change set, restoring pre-change behavior with no persistent data migration.
