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
#include <interfaces/IRDKWindowManager.h>

namespace WPEFramework {
namespace Exchange {

class MockIRDKWindowManager : public IRDKWindowManager {
public:
    MockIRDKWindowManager() = default;
    virtual ~MockIRDKWindowManager() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    // Registration methods
    MOCK_METHOD(Core::hresult, Register, (IRDKWindowManager::INotification* notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (IRDKWindowManager::INotification* notification), (override));

    // Initialize/Deinitialize
    MOCK_METHOD(Core::hresult, Initialize, (PluginHost::IShell* service), (override));
    MOCK_METHOD(Core::hresult, Deinitialize, (PluginHost::IShell* service), (override));

    // Display management
    MOCK_METHOD(Core::hresult, CreateDisplay, (const string& displayParams), (override));
    MOCK_METHOD(Core::hresult, GetApps, (string& appsIds), (const, override));

    // Key intercept methods
    MOCK_METHOD(Core::hresult, AddKeyIntercept, (const string& intercept), (override));
    MOCK_METHOD(Core::hresult, AddKeyIntercepts, (const string& intercepts), (override));
    MOCK_METHOD(Core::hresult, RemoveKeyIntercept, (const string& intercept), (override));
    MOCK_METHOD(Core::hresult, AddKeyListener, (const string& keyListeners), (override));
    MOCK_METHOD(Core::hresult, RemoveKeyListener, (const string& keyListeners), (override));

    // Key injection
    MOCK_METHOD(Core::hresult, InjectKey, (uint32_t keyCode, const string& modifiers), (override));
    MOCK_METHOD(Core::hresult, GenerateKey, (const string& keys, const string& client), (override));

    // Inactivity
    MOCK_METHOD(Core::hresult, EnableInactivityReporting, (const bool enable), (override));
    MOCK_METHOD(Core::hresult, SetInactivityInterval, (const uint32_t interval), (override));
    MOCK_METHOD(Core::hresult, ResetInactivityTime, (), (override));

    // Key repeats
    MOCK_METHOD(Core::hresult, EnableKeyRepeats, (bool enable), (override));
    MOCK_METHOD(Core::hresult, GetKeyRepeatsEnabled, (bool& keyRepeat), (const, override));
    MOCK_METHOD(Core::hresult, IgnoreKeyInputs, (bool ignore), (override));
    MOCK_METHOD(Core::hresult, EnableInputEvents, (const string& clients, bool enable), (override));
    MOCK_METHOD(Core::hresult, KeyRepeatConfig, (const string& input, const string& keyConfig), (override));

    // Focus/Visibility
    MOCK_METHOD(Core::hresult, SetFocus, (const string& client), (override));
    MOCK_METHOD(Core::hresult, SetVisible, (const std::string& client, bool visible), (override));

    // Render ready
    MOCK_METHOD(Core::hresult, RenderReady, (const string& client, bool& status), (const, override));
    MOCK_METHOD(Core::hresult, EnableDisplayRender, (const string& client, bool enable), (override));

    // Last key info
    MOCK_METHOD(Core::hresult, GetLastKeyInfo, (uint32_t& keyCode, uint32_t& modifiers, uint64_t& timestampInSeconds), (const, override));

    // ZOrder
    MOCK_METHOD(Core::hresult, SetZOrder, (const string& appInstanceId, const int32_t zOrder), (override));
    MOCK_METHOD(Core::hresult, GetZOrder, (const string& appInstanceId, int32_t& zOrder), (override));

    // VNC server
    MOCK_METHOD(Core::hresult, StartVncServer, (), (override));
    MOCK_METHOD(Core::hresult, StopVncServer, (), (override));

    BEGIN_INTERFACE_MAP(MockIRDKWindowManager)
    INTERFACE_ENTRY(IRDKWindowManager)
    END_INTERFACE_MAP
};

} // namespace Exchange
} // namespace WPEFramework
