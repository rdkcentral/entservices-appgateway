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
#include <interfaces/ISystemServices.h>

using ::WPEFramework::Exchange::ISystemServices;

class SystemServicesMock : public ISystemServices {
public:
    SystemServicesMock() = default;
    virtual ~SystemServicesMock() = default;

    MOCK_METHOD(WPEFramework::Core::hresult, Register, (INotification* notification), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Unregister, (INotification* notification), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetDeviceInfo, (IStringIterator* const& params, DeviceInfo& deviceInfo), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetDownloadedFirmwareInfo, (DownloadedFirmwareInfo& downloadedFirmwareInfo), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFirmwareDownloadPercent, (int32_t& downloadPercent, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFirmwareUpdateInfo, (const string& GUID, bool& asyncResponse, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFirmwareUpdateState, (int& firmwareUpdateState, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetLastFirmwareFailureReason, (string& failReason, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetLastWakeupKeyCode, (int& wakeupKeyCode, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetMfgSerialNumber, (string& mfgSerialNumber, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetNetworkStandbyMode, (bool& nwStandby, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPowerState, (string& powerState, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPowerStateBeforeReboot, (string& state, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetRFCConfig, (IStringIterator* const& rfcList, string& RFCConfig, uint32_t& SysSrv_Status, string& errorMessage, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetSerialNumber, (string& serialNumber, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFriendlyName, (string& friendlyName, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetTerritory, (string& territory, string& region, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetTimeZones, (IStringIterator* const& timeZones, string& zoneinfo, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetTimeZoneDST, (string& timeZone, string& accuracy, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetWakeupReason, (string& wakeupReason, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, IsOptOutTelemetry, (bool& OptOut, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, Reboot, (const string& rebootReason, int& IARM_Bus_Call_STATUS, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetDeepSleepTimer, (const int seconds, uint32_t& SysSrv_Status, string& errorMessage, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFirmwareAutoReboot, (const bool enable, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetNetworkStandbyMode, (const bool nwStandby, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetOptOutTelemetry, (const bool OptOut, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetPowerState, (const string& powerState, const string& standbyReason, uint32_t& SysSrv_Status, string& errorMessage, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFriendlyName, (const string& friendlyName, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetBootLoaderSplashScreen, (const string& path, ErrorInfo& error, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetTerritory, (const string& territory, const string& region, SystemError& error, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetTimeZoneDST, (const string& timeZone, const string& accuracy, uint32_t& SysSrv_Status, string& errorMessage, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, UpdateFirmware, (SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetBootTypeInfo, (BootType& bootType), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetMigrationStatus, (const string& status, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetMigrationStatus, (MigrationStatus& migrationStatus), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetMacAddresses, (const string& GUID, bool& asyncResponse, uint32_t& SysSrv_Status, string& errorMessage, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetPlatformConfiguration, (const string& query, PlatformConfig& platformConfig), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetWakeupSrcConfiguration, (const string& powerState, ISystemServicesWakeupSourcesIterator* const& wakeupSources, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetSystemVersions, (SystemVersionsInfo& systemVersionsInfo), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, RequestSystemUptime, (string& systemUptime, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetMode, (const ModeInfo& modeInfo, uint32_t& SysSrv_Status, string& errorMessage, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, UploadLogsAsync, (SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, AbortLogUpload, (SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetFSRFlag, (const bool fsrFlag, SystemResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetFSRFlag, (bool& fsrFlag, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, SetBlocklistFlag, (const bool blocklist, SetBlocklistResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetBlocklistFlag, (BlocklistResult& result), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetBuildType, (string& buildType, bool& success), (override));
    MOCK_METHOD(WPEFramework::Core::hresult, GetTimeStatus, (string& TimeQuality, string& TimeSrc, string& Time, bool& success), (override));

    MOCK_METHOD(void, AddRef, (), (const, override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void*, QueryInterface, (const uint32_t interfaceNumber), (override));
};
