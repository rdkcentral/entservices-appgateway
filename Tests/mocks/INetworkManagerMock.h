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

namespace WPEFramework {
namespace Exchange {

class MockINetworkManager : public INetworkManager {
public:
    MockINetworkManager() = default;
    virtual ~MockINetworkManager() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    // Registration methods
    MOCK_METHOD(uint32_t, Register, (INetworkManager::INotification* notification), (override));
    MOCK_METHOD(uint32_t, Unregister, (INetworkManager::INotification* notification), (override));

    // Interface methods
    MOCK_METHOD(uint32_t, GetAvailableInterfaces, (IInterfaceDetailsIterator*& interfaces), (override));
    MOCK_METHOD(uint32_t, GetPrimaryInterface, (string& interface), (override));
    MOCK_METHOD(uint32_t, SetPrimaryInterface, (const string& interface), (override));
    MOCK_METHOD(uint32_t, SetInterfaceState, (const string& interface, const bool enabled), (override));
    MOCK_METHOD(uint32_t, GetInterfaceState, (const string& interface, bool& enabled), (override));
    MOCK_METHOD(uint32_t, GetIPSettings, (string& interface, IPAddress& address), (override));
    MOCK_METHOD(uint32_t, SetIPSettings, (const string& interface, const IPAddress& address), (override));
    MOCK_METHOD(uint32_t, GetStunEndpoint, (string& endpoint, uint32_t& port, uint32_t& bindTimeout, uint32_t& cacheTimeout), (override));
    MOCK_METHOD(uint32_t, SetStunEndpoint, (const string endpoint, const uint32_t port, const uint32_t bindTimeout, const uint32_t cacheTimeout), (override));
    MOCK_METHOD(uint32_t, GetConnectivityTestEndpoints, (IStringIterator*& endpoints), (override));
    MOCK_METHOD(uint32_t, SetConnectivityTestEndpoints, (IStringIterator* const endpoints), (override));
    MOCK_METHOD(uint32_t, IsConnectedToInternet, (const string& ipversion, InternetStatus& result), (override));
    MOCK_METHOD(uint32_t, GetCaptivePortalURI, (string& uri), (override));
    MOCK_METHOD(uint32_t, StartConnectivityMonitoring, (const uint32_t interval), (override));
    MOCK_METHOD(uint32_t, StopConnectivityMonitoring, (), (override));
    MOCK_METHOD(uint32_t, GetPublicIP, (const string& ipversion, string& ipaddress), (override));
    MOCK_METHOD(uint32_t, Ping, (const string& ipversion, const string& endpoint, const uint32_t count, const uint16_t timeout, const string& guid, string& response), (override));
    MOCK_METHOD(uint32_t, Trace, (const string& ipversion, const string& endpoint, const uint32_t count, const string& guid, string& response), (override));
    MOCK_METHOD(uint32_t, StartWiFiScan, (const string& frequency), (override));
    MOCK_METHOD(uint32_t, StopWiFiScan, (), (override));
    MOCK_METHOD(uint32_t, GetKnownSSIDs, (IStringIterator*& ssids), (override));
    MOCK_METHOD(uint32_t, AddToKnownSSIDs, (const WiFiConnectTo& ssid), (override));
    MOCK_METHOD(uint32_t, RemoveKnownSSID, (const string& ssid), (override));
    MOCK_METHOD(uint32_t, WiFiConnect, (const WiFiConnectTo& ssid), (override));
    MOCK_METHOD(uint32_t, WiFiDisconnect, (), (override));
    MOCK_METHOD(uint32_t, GetConnectedSSID, (WiFiSSIDInfo& ssidinfo), (override));
    MOCK_METHOD(uint32_t, StartWPS, (const WiFiWPS& method, const string& pin), (override));
    MOCK_METHOD(uint32_t, StopWPS, (), (override));
    MOCK_METHOD(uint32_t, GetWifiState, (WiFiState& state), (override));
    MOCK_METHOD(uint32_t, GetWiFiSignalQuality, (string& ssid, string& strength, string& noise, string& snr, WiFiSignalQuality& quality), (override));
    MOCK_METHOD(uint32_t, GetSupportedSecurityModes, (ISecurityModeIterator*& modes), (override));
    MOCK_METHOD(uint32_t, SetLogLevel, (const Logging& level), (override));
    MOCK_METHOD(uint32_t, GetLogLevel, (Logging& level), (override));

    BEGIN_INTERFACE_MAP(MockINetworkManager)
    INTERFACE_ENTRY(INetworkManager)
    END_INTERFACE_MAP
};

// Mock iterator for interface details
class MockInterfaceDetailsIterator : public INetworkManager::IInterfaceDetailsIterator {
public:
    MockInterfaceDetailsIterator() = default;
    virtual ~MockInterfaceDetailsIterator() = default;

    // IReferenceCounted methods (required by Thunder interfaces)
    void AddRef() const override {}
    uint32_t Release() const override { return 0; }

    MOCK_METHOD(bool, Next, (INetworkManager::InterfaceDetails& iface), (override));
    MOCK_METHOD(bool, Previous, (INetworkManager::InterfaceDetails& iface), (override));
    MOCK_METHOD(void, Reset, (const uint32_t position), (override));
    MOCK_METHOD(bool, IsValid, (), (const, override));
    MOCK_METHOD(uint32_t, Count, (), (const, override));
    MOCK_METHOD(uint32_t, Current, (INetworkManager::InterfaceDetails& iface), (const, override));

    BEGIN_INTERFACE_MAP(MockInterfaceDetailsIterator)
    INTERFACE_ENTRY(INetworkManager::IInterfaceDetailsIterator)
    END_INTERFACE_MAP
};

} // namespace Exchange
} // namespace WPEFramework
