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
#include <interfaces/ISharedStorage.h>

using ::WPEFramework::Exchange::ISharedStorage;

class SharedStorageMock : public ISharedStorage {
public:
    SharedStorageMock() = default;
    virtual ~SharedStorageMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, Register, (ISharedStorage::INotification* notification), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unregister, (ISharedStorage::INotification* notification), (override));

    MOCK_METHOD(WPEFramework::Core::hresult, SetValue,
        (const ISharedStorage::ScopeType scope, const string& ns, const string& key, const string& value, const uint32_t ttl, ISharedStorage::Success& success),
        (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetValue,
        (const ISharedStorage::ScopeType scope, const string& ns, const string& key, string& value, uint32_t& ttl, bool& success),
        (override));
    MOCK_METHOD(WPEFramework::Core::hresult, DeleteKey,
        (const ISharedStorage::ScopeType scope, const string& ns, const string& key, ISharedStorage::Success& success),
        (override));
    MOCK_METHOD(WPEFramework::Core::hresult, DeleteNamespace,
        (const ISharedStorage::ScopeType scope, const string& ns, ISharedStorage::Success& success),
        (override));

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};

class SharedStorageNotificationMock : public ISharedStorage::INotification {
public:
    SharedStorageNotificationMock() = default;
    virtual ~SharedStorageNotificationMock() = default;

    MOCK_METHOD(void, OnValueChanged,
        (const ISharedStorage::ScopeType scope, const string& ns, const string& key, const string& value),
        (override));
};
