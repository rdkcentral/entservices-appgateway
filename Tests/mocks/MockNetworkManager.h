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
#include <interfaces/INetworkManager.h>

class MockNetworkManager : public WPEFramework::Exchange::INetworkManager {
public:
    ~MockNetworkManager() override = default;

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    MOCK_METHOD(uint32_t, Register, (INotification * notification), (override));
    MOCK_METHOD(uint32_t, Unregister, (INotification * notification), (override));

    MOCK_METHOD(uint32_t, GetAvailableInterfaces, (IInterfaceDetailsIterator * &interfaces), (override));
    MOCK_METHOD(uint32_t, GetPrimaryInterface, (string & interface), (override));
    MOCK_METHOD(uint32_t, SetInterfaceState, (const string& interface, const bool enabled), (override));
    MOCK_METHOD(uint32_t, GetInterfaceState, (const string& interface, bool& enabled), (override));
    MOCK_METHOD(uint32_t, GetIPSettings, (string & interface, const string& ipversion, IPAddress& address), (override));
    MOCK_METHOD(uint32_t, SetIPSettings, (const string& interface, const IPAddress& address), (override));
    MOCK_METHOD(uint32_t, GetStunEndpoint, (string & endpoint, uint32_t& port, uint32_t& timeout, uint32_t& cacheLifetime), (const, override));
    MOCK_METHOD(uint32_t, SetStunEndpoint, (string const endpoint, const uint32_t port, const uint32_t timeout, const uint32_t cacheLifetime), (override));
    MOCK_METHOD(uint32_t, GetConnectivityTestEndpoints, (IStringIterator * &endpoints), (const, override));
    MOCK_METHOD(uint32_t, SetConnectivityTestEndpoints, (IStringIterator * const endpoints), (override));
    MOCK_METHOD(uint32_t, IsConnectedToInternet, (string & ipversion, string& interface, InternetStatus& status), (override));
    MOCK_METHOD(uint32_t, GetCaptivePortalURI, (string & uri), (const, override));
    MOCK_METHOD(uint32_t, GetPublicIP, (string & interface, string& ipversion, string& ipaddress), (override));
    MOCK_METHOD(uint32_t, Ping, (const string ipversion, const string endpoint, const uint32_t count, const uint16_t timeout, const string guid, string& response), (override));
    MOCK_METHOD(uint32_t, Trace, (const string ipversion, const string endpoint, const uint32_t nqueries, const string guid, string& response), (override));
    MOCK_METHOD(uint32_t, SetHostname, (const string& hostname), (override));
    MOCK_METHOD(uint32_t, StartWiFiScan, (const string& frequency, IStringIterator* const ssids), (override));
    MOCK_METHOD(uint32_t, StopWiFiScan, (), (override));
    MOCK_METHOD(uint32_t, GetKnownSSIDs, (IStringIterator * &ssids), (override));
    MOCK_METHOD(uint32_t, AddToKnownSSIDs, (const WiFiConnectTo& ssid), (override));
    MOCK_METHOD(uint32_t, RemoveKnownSSID, (const string& ssid), (override));
    MOCK_METHOD(uint32_t, WiFiConnect, (const WiFiConnectTo& ssid), (override));
    MOCK_METHOD(uint32_t, WiFiDisconnect, (), (override));
    MOCK_METHOD(uint32_t, GetConnectedSSID, (WiFiSSIDInfo & ssidInfo), (override));
    MOCK_METHOD(uint32_t, StartWPS, (const WiFiWPS& method, const string& pin), (override));
    MOCK_METHOD(uint32_t, StopWPS, (), (override));
    MOCK_METHOD(uint32_t, GetWifiState, (WiFiState & state), (override));
    MOCK_METHOD(uint32_t, GetWiFiSignalQuality, (string & ssid, int& strength, int& noise, int& snr, WiFiSignalQuality& quality), (override));
    MOCK_METHOD(uint32_t, GetSupportedSecurityModes, (ISecurityModeIterator * &modes), (const, override));
    MOCK_METHOD(uint32_t, SetLogLevel, (const Logging& level), (override));
    MOCK_METHOD(uint32_t, GetLogLevel, (Logging & level), (override));
    MOCK_METHOD(uint32_t, Configure, (const string configLine), (override));

    BEGIN_INTERFACE_MAP(MockNetworkManager)
    INTERFACE_ENTRY(WPEFramework::Exchange::INetworkManager)
    END_INTERFACE_MAP
};

class MockInterfaceDetailsIterator : public WPEFramework::Exchange::INetworkManager::IInterfaceDetailsIterator {
public:
    ~MockInterfaceDetailsIterator() override = default;

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));

    MOCK_METHOD(bool, Next, (WPEFramework::Exchange::INetworkManager::InterfaceDetails & details), (override));
    MOCK_METHOD(bool, Previous, (WPEFramework::Exchange::INetworkManager::InterfaceDetails & details), (override));
    MOCK_METHOD(void, Reset, (const uint32_t position), (override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(uint32_t, Count, (), (const, override));
    MOCK_METHOD(WPEFramework::Exchange::INetworkManager::InterfaceDetails, Current, (), (const, override));

    BEGIN_INTERFACE_MAP(MockInterfaceDetailsIterator)
    INTERFACE_ENTRY(WPEFramework::Exchange::INetworkManager::IInterfaceDetailsIterator)
    END_INTERFACE_MAP
};
