#pragma once

//#include <interfaces/IApp2AppProvider.h>
#include <interfaces/IAppGateway.h>

// // PUBLIC_INTERFACE
// inline WPEFramework::Exchange::IApp2AppProvider::Context ConvertAppGatewayToProviderContext(
//     const WPEFramework::Exchange::GatewayContext& src,
//     const string& origin)
// {
//     /** Convert AppGateway's GatewayContext + origin string into an App2AppProvider::Context (tests-only helper). */
//     WPEFramework::Exchange::IApp2AppProvider::Context result;
//     result.requestId = src.requestId;
//     result.connectionId = src.connectionId;
//     result.appId = src.appId;
//     result.origin = origin;
//     return result;
// }

// // PUBLIC_INTERFACE
// inline WPEFramework::Exchange::GatewayContext ConvertProviderToAppGatewayContext(
//     const WPEFramework::Exchange::IApp2AppProvider::Context& src)
// {
//     /** Convert App2AppProvider::Context into AppGateway's GatewayContext (tests-only helper). */
//     WPEFramework::Exchange::GatewayContext result;
//     result.requestId = src.requestId;
//     result.connectionId = src.connectionId;
//     result.appId = src.appId;
//     return result;
// }
