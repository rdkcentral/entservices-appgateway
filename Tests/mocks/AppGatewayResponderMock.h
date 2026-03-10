/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <gmock/gmock.h>
#include <interfaces/IAppGateway.h>

using ::WPEFramework::Exchange::IAppGatewayResponder;

// ----------------------------------------------------------------------------
// Mock for Exchange::IAppGatewayResponder::INotification
//
// Per repository pattern: notification/callback mocks do NOT include COM
// lifecycle methods (AddRef, Release, QueryInterface).
// Used to verify Register/Unregister call behaviour on the responder.
// ----------------------------------------------------------------------------
class IAppGatewayResponderNotificationMock : public IAppGatewayResponder::INotification {
public:
    IAppGatewayResponderNotificationMock() = default;
    virtual ~IAppGatewayResponderNotificationMock() = default;

    MOCK_METHOD(void, OnAppConnectionChanged,
        (const string& appId,
         const uint32_t connectionId,
         const bool connected),
        (override));

    // COM interface methods
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

// ----------------------------------------------------------------------------
// Mock for Exchange::IAppGatewayResponder
//
// Used to mock the AppGateway responder interface acquired via
// QueryInterfaceByCallsign inside SubscriberMap::DispatchToGateway (callsign:
// APP_GATEWAY_CALLSIGN) and SubscriberMap::DispatchToLaunchDelegate (callsign:
// INTERNAL_GATEWAY_CALLSIGN / "org.rdk.LaunchDelegate").
//
// The implementation calls Emit() to dispatch notifications to connected clients.
// ----------------------------------------------------------------------------
class IAppGatewayResponderMock : public IAppGatewayResponder {
public:
    IAppGatewayResponderMock() = default;
    virtual ~IAppGatewayResponderMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, Respond,
        (const WPEFramework::Exchange::GatewayContext& context,
         const string& payload),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Emit,
        (const WPEFramework::Exchange::GatewayContext& context,
         const string& method,
         const string& payload),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Request,
        (const uint32_t connectionId,
         const uint32_t id,
         const string& method,
         const string& params),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, GetGatewayConnectionContext,
        (const uint32_t connectionId,
         const string& contextKey,
         string& contextValue),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Register,
        (IAppGatewayResponder::INotification* notification),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Unregister,
        (IAppGatewayResponder::INotification* notification),
        (override));

    // COM interface methods
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};
