# Network Call Inventory (HTTP REST and gRPC)

Scope: plugins in entservices-appgateway (AppGateway, AppGatewayCommon, AppNotifications).

## Executive Summary

- Plugins with direct or indirect HTTP/gRPC call paths:
  - AppGatewayCommon: HTTP (indirect via UtilsController helper)
- Plugins with no direct/indirect HTTP/gRPC call evidence in current source:
  - AppGateway
  - AppNotifications
- gRPC usage in this subtree: none found

## Plugin-by-Plugin Details

### AppGateway
- Direct HTTP REST: none found
- Direct gRPC: none found
- Indirect HTTP/gRPC: none found
- Notes:
  - Uses local Thunder JSON-RPC direct dispatcher link, not HTTP/gRPC transport:
    - AppGateway/Resolver.cpp:239
    - helpers/UtilsJsonrpcDirectLink.h:169

### AppGatewayCommon
- Direct HTTP REST in plugin files: none found
- Direct gRPC: none found
- Indirect HTTP REST: yes
  - AppGatewayCommon delegate obtains Thunder controller clients using UtilsController path:
    - AppGatewayCommon/delegate/SystemDelegate.h:978
    - AppGatewayCommon/delegate/SystemDelegate.h:1000
    - AppGatewayCommon/delegate/SystemDelegate.h:1022
    - AppGatewayCommon/delegate/SystemDelegate.h:1044
  - UtilsController helper performs direct local HTTP call using curl:
    - helpers/UtilsController.h:23
    - helpers/UtilsController.h:93
    - helpers/UtilsController.h:111

### AppNotifications
- Direct HTTP REST: none found
- Direct gRPC: none found
- Indirect HTTP/gRPC: none found
- Notes:
  - Includes UtilsController header but no HTTP/gRPC call path found in AppNotifications sources.

## Shared Helper

- helpers/UtilsController.h contains a direct local HTTP GET to Thunder Controller configuration:
  - helpers/UtilsController.h:93
  - helpers/UtilsController.h:111
- Endpoint used:
  - http://127.0.0.1:9998/Service/Controller/Configuration/Controller

## Conclusion

Direct transport-level network calls in entservices-appgateway are limited to libcurl usage in helpers/UtilsController.h (local Thunder controller configuration check). No gRPC call sites were found in this subtree.
