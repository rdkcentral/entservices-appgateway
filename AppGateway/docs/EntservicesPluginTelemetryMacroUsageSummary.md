# Entservices Plugin Telemetry Macro Usage Summary

## Scope

This file summarizes telemetry macro usage for these three plugins only:

- AppGateway
- AppGatewayCommon
- AppNotifications

Validation date: 2026-04-06

## Macro Coverage

| Plugin | Define Client | Init | Bootstrap | Deinit | Track API | Track Service | Track Response | Report Metric | Report API Error | Report External Error | Report API Latency | Report Service Latency | Report Event |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| AppGateway | No (core telemetry provider) | No | No | No | No | No | No | No | No | No | No | No | No |
| AppGatewayCommon | Yes | Yes | Yes | Yes | No | No | No | No | No | No | No | No | No |
| AppNotifications | Yes | Yes | Yes | Yes | No | No | No | No | No | No | No | No | No |

## Source References

- AppGateway: `entservices-appgateway/AppGateway/AppGateway.cpp`
- AppGatewayCommon: `entservices-appgateway/AppGatewayCommon/AppGatewayCommon.cpp`
- AppNotifications: `entservices-appgateway/AppNotifications/AppNotifications.cpp`

## Notes

- AppGateway acts as the telemetry provider/aggregator and does not use plugin-side helper lifecycle macros in its plugin entry source.
- AppGatewayCommon and AppNotifications follow the standard plugin lifecycle telemetry pattern.
- No direct usage was observed for `AGW_TRACK_API_CALL`, `AGW_TRACK_SERVICE_CALL`, `AGW_TRACK_RESPONSE_PAYLOAD`, `AGW_REPORT_METRIC`, `AGW_REPORT_API_ERROR`, `AGW_REPORT_EXTERNAL_SERVICE_ERROR`, `AGW_REPORT_API_LATENCY`, `AGW_REPORT_SERVICE_LATENCY`, or `AGW_REPORT_EVENT` in these three plugin source files.
