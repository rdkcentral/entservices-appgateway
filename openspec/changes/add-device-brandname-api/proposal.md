## Why

Firebolt clients currently cannot read the device display brand name through AppGateway, even though the underlying Thunder DeviceInfo interface already exposes it. Adding `Device.brandName` now closes this gap with a low-risk, additive API that reuses an existing COM-RPC source.

## What Changes

- Add Firebolt GET method support for `Device.brandName` in AppGateway resolution routing.
- Add `AppGatewayCommon` request-handler mapping for `device.brandname`.
- Add `AppGatewayCommon` facade and `SystemDelegate` retrieval path that calls `Exchange::IDeviceInfo::DisplayBrandName(DeviceDisplayBrandName&)`.
- Return the Thunder `displayBrandName` string as a Firebolt JSON string (no value transformation).
- Extend L1 branding tests to cover success, empty value, interface failure, unavailable service, null delegate, and case-insensitive routing for `Device.brandName`.
- Update API reference/docs where Device API methods are enumerated.

## Capabilities

### New Capabilities
- None.

### Modified Capabilities
- `AppGatewayCommon`: Extend supported Device request family to include `Device.brandName` (GET only), backed by DeviceInfo `displaybrandname` via COM-RPC.

## Impact

### Needed Interface Files
- `public/entservices-apis/apis/DeviceInfo/IDeviceInfo.h`
  - Required to validate owning interface contract and method signatures for:
  - `DeviceDisplayBrandName` struct
  - `DisplayBrandName(DeviceDisplayBrandName&) const`
  - `DisplayBrandName(const string&)`
- `public/entservices-appgateway/AppGatewayCommon/AppGatewayCommon.h`
  - Required to verify AppGatewayCommon private helper contract additions.
- `public/entservices-appgateway/AppGatewayCommon/delegate/SystemDelegate.h`
  - Required to verify Thunder client integration point and error normalization conventions.

### Affected Components and Callers
- `AppGateway/resolutions/resolution.base.json`
  - Adds routing entry for `device.brandName` to `org.rdk.AppGatewayCommon` with `useComRpc: true`.
- `AppGatewayCommon` plugin
  - Request dispatch table adds `device.brandname` key.
  - Internal helper and delegate call path for brand-name retrieval.
- `SystemDelegate` (within AppGatewayCommon)
  - New getter path consuming Thunder `DeviceInfo` via `QueryInterfaceByCallsign<Exchange::IDeviceInfo>("DeviceInfo")`.
- Test and mocks
  - `Tests/L1Tests/AppGatewayCommon/AppGatewayCommon_branding_test.cpp` extends existing branding test suite.
  - Existing `MockDeviceInfo` already includes the relevant vtable surface and can be expanded for the new method.
- Documentation
  - `docs/API_REFERENCE.md` and any Device API lists updated to include `device.brandName`.

### Repository-Wide Cross-Check Summary
- Production code: no existing `device.brandName`/`displaybrandname` handling in AppGateway routing or handler map.
- Interface boundary: Thunder contract exists and is stable in `IDeviceInfo.h`; this change is additive and consumes existing interface symbols only.
- Gateway method strings and registration wiring: no removals/renames; only one new method key is added.
- Tests/mocks/docs: branding L1 suite and API docs require updates.

### Compatibility and ABI Risk
- No symbol removal or rename.
- No interface deletion or signature changes in `IDeviceInfo` or AppGateway interfaces.
- Externally observable behavior change is additive only (`Device.brandName` becomes available).
- Error behavior aligns with existing AppGatewayCommon DeviceInfo patterns:
  - `Core::ERROR_NONE` on success
  - `Core::ERROR_GENERAL` when `DisplayBrandName(...)` returns non-success
  - `Core::ERROR_UNAVAILABLE` when DeviceInfo/delegate is unavailable (existing platform convention)

### Dependencies
- No new build-time dependencies.
- Runtime dependency remains existing Thunder plugin callsign `DeviceInfo`.
