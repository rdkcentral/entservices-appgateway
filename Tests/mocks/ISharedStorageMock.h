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
#include <interfaces/ISharedStorage.h>

namespace WPEFramework {
namespace Exchange {

class MockISharedStorage : public ISharedStorage {
public:
    MockISharedStorage() = default;
    virtual ~MockISharedStorage() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    // Registration methods
    MOCK_METHOD(Core::hresult, Register, (ISharedStorage::INotification* notification), (override));
    MOCK_METHOD(Core::hresult, Unregister, (ISharedStorage::INotification* notification), (override));

    // SetValue - ns is namespace parameter
    MOCK_METHOD(Core::hresult, SetValue, (const ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl, Success& success), (override));
    
    // GetValue - ns is namespace parameter
    MOCK_METHOD(Core::hresult, GetValue, (const ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl, bool& success), (override));
    
    // DeleteKey - ns is namespace parameter
    MOCK_METHOD(Core::hresult, DeleteKey, (const ScopeType scope, const string& ns, const string& key, Success& success), (override));
    
    // DeleteNamespace - ns is namespace parameter
    MOCK_METHOD(Core::hresult, DeleteNamespace, (const ScopeType scope, const string& ns, Success& success), (override));

    BEGIN_INTERFACE_MAP(MockISharedStorage)
    INTERFACE_ENTRY(ISharedStorage)
    END_INTERFACE_MAP
};

} // namespace Exchange
} // namespace WPEFramework
