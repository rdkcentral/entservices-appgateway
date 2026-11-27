## MODIFIED Requirements

### Requirement: Supported request families
The handler map SHALL implement the source-defined families below while `AppGateway/resolutions/resolution.base.json` remains the source-tree routing catalog.

#### Scenario: Supported family is invoked
- **WHEN** a mapped device, localization, accessibility, output, lifecycle, action, storage, TTS, or metrics method arrives, including `Device.brandName`
- **THEN** AppGatewayCommon SHALL invoke the corresponding delegate or source-defined stub behavior

### Requirement: Synchronous request dispatch
`HandleAppGatewayRequest` SHALL lowercase the incoming method and synchronously invoke the corresponding static handler-map function. If `mDelegate` is null it SHALL return `Core::ERROR_UNAVAILABLE` with `{"error":"Service unavailable"}`. Advertising ID and device UID SHALL delegate through their explicit handler-map entries; if no mapped or source-defined special-case handler exists, it SHALL call `ErrorUtils::NotSupported` and return `Core::ERROR_UNKNOWN_KEY`.

#### Scenario: Method has a mapped handler
- **WHEN** the lowercased method exists in `handlers`
- **THEN** its member handler SHALL execute on the incoming COM-RPC service thread and return its `Core::hresult` and payload

#### Scenario: Method is unsupported
- **WHEN** the lowercased method matches neither a handler-map entry nor a source-defined special case
- **THEN** the component SHALL return `Core::ERROR_UNKNOWN_KEY` with the payload produced by `ErrorUtils::NotSupported`

#### Scenario: Device.brandName delegates to DeviceInfo displaybrandname
- **WHEN** `Device.brandName` is requested through AppGateway
- **THEN** the handler SHALL retrieve the value through the System delegate using Thunder `DeviceInfo.displaybrandname`
- **AND** the returned payload SHALL be a JSON string containing the Thunder `displayBrandName` value without value conversion
- **AND** `Core::ERROR_NONE` from Thunder SHALL map to success while non-success property-read failures SHALL map to Firebolt error semantics
