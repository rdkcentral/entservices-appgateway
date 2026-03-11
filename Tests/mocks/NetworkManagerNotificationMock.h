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
#include <interfaces/INetworkManager.h>

using ::WPEFramework::Exchange::INetworkManager;

class NetworkManagerNotificationMock : public INetworkManager::INotification {
public:
    NetworkManagerNotificationMock() = default;
    virtual ~NetworkManagerNotificationMock() = default;

    MOCK_METHOD(void, onInterfaceStateChange, (const INetworkManager::InterfaceState state, const string interface), (override));
    MOCK_METHOD(void, onActiveInterfaceChange, (const string prevActiveInterface, const string currentActiveInterface), (override));
    MOCK_METHOD(void, onIPAddressChange, (const string interface, const string ipversion, const string ipaddress, const INetworkManager::IPStatus status), (override));
    MOCK_METHOD(void, onInternetStatusChange, (const INetworkManager::InternetStatus prevState, const INetworkManager::InternetStatus currState, const string interface), (override));
    MOCK_METHOD(void, onAvailableSSIDs, (const string jsonOfScanResults), (override));
    MOCK_METHOD(void, onWiFiStateChange, (const INetworkManager::WiFiState state), (override));
    MOCK_METHOD(void, onWiFiSignalQualityChange, (const string ssid, const string strength, const string noise, const string snr, const INetworkManager::WiFiSignalQuality quality), (override));
};

class InterfaceDetailsIteratorMock : public INetworkManager::IInterfaceDetailsIterator {
public:
    InterfaceDetailsIteratorMock() = default;
    virtual ~InterfaceDetailsIteratorMock() = default;

    MOCK_METHOD(bool, Next, (INetworkManager::InterfaceDetails& info), (override));
    MOCK_METHOD(bool, Previous, (INetworkManager::InterfaceDetails& info), (override));
    MOCK_METHOD(void, Reset, (const uint32_t position), (override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(uint32_t, Count, (), (const, override));
    MOCK_METHOD(INetworkManager::InterfaceDetails, Current, (), (const, override));

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};
