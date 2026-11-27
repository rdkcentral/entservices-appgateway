## 1. API Routing and Request Dispatch

- [x] 1.1 Add `device.brandName` resolution entry in `AppGateway/resolutions/resolution.base.json` mapped to `org.rdk.AppGatewayCommon` with `useComRpc: true`.
- [x] 1.2 Add `device.brandname` handler-map entry in `AppGatewayCommon/AppGatewayCommon.cpp` that routes to a new AppGatewayCommon getter.
- [x] 1.3 Declare and define AppGatewayCommon getter method for `Device.brandName` in `AppGatewayCommon/AppGatewayCommon.h` and `AppGatewayCommon/AppGatewayCommon.cpp`.

## 2. Thunder DeviceInfo Integration

- [x] 2.1 Implement `SystemDelegate` getter in `AppGatewayCommon/delegate/SystemDelegate.h` that calls `Exchange::IDeviceInfo::DisplayBrandName(DeviceDisplayBrandName&)`.
- [x] 2.2 Normalize the returned value to Firebolt JSON string format using existing DeviceInfo getter conventions.
- [x] 2.3 Preserve existing error handling conventions for unavailable delegate/service and map Thunder property-read failures to Firebolt error semantics.

## 3. Test Coverage

- [x] 3.1 Extend `Tests/L1Tests/AppGatewayCommon/AppGatewayCommon_branding_test.cpp` with `Device.brandName` success and empty-string cases.
- [x] 3.2 Add `Device.brandName` failure-path tests for interface failure (`Core::ERROR_GENERAL`), DeviceInfo unavailable (`Core::ERROR_UNAVAILABLE`), and null delegate (`Core::ERROR_UNAVAILABLE`).
- [x] 3.3 Add/extend routing case-insensitivity test coverage for `DEVICE.BRANDNAME` and `device.brandname`.

## 4. Documentation and Validation

- [x] 4.1 Update `docs/API_REFERENCE.md` (and any relevant method lists) to include `device.brandName` as GET-only Device API.
- [ ] 4.2 Run/verify AppGatewayCommon L1 tests covering branding APIs.
- [x] 4.3 Validate OpenSpec change completeness and consistency before handoff to apply workflow.
