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
#include <interfaces/IAppNotifications.h>

using ::WPEFramework::Exchange::IAppNotifications;
using ::WPEFramework::Exchange::IAppNotificationHandler;

// ----------------------------------------------------------------------------
// Mock for Exchange::IAppNotifications
//
// Used to mock the primary AppNotifications COM interface in L1 unit tests.
// Covers: Subscribe, Emit, Cleanup + COM lifecycle methods.
// ----------------------------------------------------------------------------
class IAppNotificationsMock : public IAppNotifications {
public:
    IAppNotificationsMock() = default;
    virtual ~IAppNotificationsMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, Subscribe,
        (const IAppNotifications::AppNotificationContext& context,
         bool listen,
         const string& module,
         const string& event),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Emit,
        (const string& event,
         const string& payload,
         const string& appId),
        (override));

    MOCK_METHOD(WPEFramework::Core::hresult, Cleanup,
        (const uint32_t connectionId,
         const string& origin),
        (override));

    // COM interface methods
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

// ----------------------------------------------------------------------------
// Mock for Exchange::IAppNotificationHandler::IEmitter
//
// Used to mock the emitter callback interface passed into HandleAppEventNotifier.
// Per repository pattern: notification/callback mocks do NOT include COM methods.
// ----------------------------------------------------------------------------
class IAppNotificationHandlerEmitterMock : public IAppNotificationHandler::IEmitter {
public:
    IAppNotificationHandlerEmitterMock() = default;
    virtual ~IAppNotificationHandlerEmitterMock() = default;

    MOCK_METHOD(void, Emit,
        (const string& event,
         const string& payload,
         const string& appId),
        (override));

    // COM interface methods (IEmitter inherits Core::IUnknown)
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

// ----------------------------------------------------------------------------
// Mock for Exchange::IAppNotificationHandler
//
// Used to mock the handler interface acquired via QueryInterfaceByCallsign
// inside ThunderSubscriptionManager::HandleNotifier. The implementation calls
// HandleAppEventNotifier to subscribe/unsubscribe to events from other plugins.
// ----------------------------------------------------------------------------
class IAppNotificationHandlerMock : public IAppNotificationHandler {
public:
    IAppNotificationHandlerMock() = default;
    virtual ~IAppNotificationHandlerMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, HandleAppEventNotifier,
        (IAppNotificationHandler::IEmitter* emitCb,
         const string& event,
         bool listen,
         bool& status),
        (override));

    // COM interface methods
    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};
