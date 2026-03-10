/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 RDK Management
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
#include <interfaces/ILifecycleManagerState.h>

namespace WPEFramework {
namespace Exchange {

class MockILifecycleManagerState : public ILifecycleManagerState {
public:
    MockILifecycleManagerState() = default;
    virtual ~MockILifecycleManagerState() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    // Registration methods
    MOCK_METHOD(Core::hresult, Register, (ILifecycleManagerState::INotification* notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (ILifecycleManagerState::INotification* notification), (override));

    // AppReady - Response api call to appInitializing API
    MOCK_METHOD(Core::hresult, AppReady, (const string& appId), (override));

    // StateChangeComplete - Response api call to appLifecycleStateChanged API
    MOCK_METHOD(Core::hresult, StateChangeComplete, (const string& appId, const uint32_t stateChangedId, const bool success), (override));

    // CloseApp - close the app
    MOCK_METHOD(Core::hresult, CloseApp, (const string& appId, const AppCloseReason closeReason), (override));

    BEGIN_INTERFACE_MAP(MockILifecycleManagerState)
    INTERFACE_ENTRY(ILifecycleManagerState)
    END_INTERFACE_MAP
};

} // namespace Exchange
} // namespace WPEFramework
