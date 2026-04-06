/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 **/

#pragma once

#ifndef __APPDELEGATE_H__
#define __APPDELEGATE_H__
#include <mutex>
#include <interfaces/ISharedStorage.h>
#include <interfaces/IAppGateway.h>
#include <interfaces/IOCIContainer.h>
#include "UtilsUUID.h"
#include "UtilsLogging.h"
#include "StringUtils.h"
#include "UtilsFirebolt.h"

#define ADVERTISING_ID_KEY "fireboltAdvertisingId"
#define ADVERTISING_TYPE "sessionid"
#define ADVERTISING_ID_LIMIT "1"
#define DEVICE_UID_KEY "fireboltDeviceUid"


using namespace WPEFramework;

class AppDelegate {
    public:
        AppDelegate(PluginHost::IShell *shell) : mShell(shell), mSharedStorage(nullptr) {}
        virtual ~AppDelegate() {
            {
                std::lock_guard<std::mutex> lock(mSharedStorageMutex);
                if (nullptr != mSharedStorage) {
                    mSharedStorage->Release();
                    mSharedStorage = nullptr;
                }
            }
        };

        Core::hresult GetDeviceUID(const std::string &appId, string &result /* @out */) {
            Exchange::ISharedStorage* sharedStorage = GetSharedStorage();
            if (nullptr != sharedStorage) {
                uint32_t ttl;
                bool success;
                if (Core::ERROR_NONE != sharedStorage->GetValue(Exchange::ISharedStorage::DEVICE, appId, DEVICE_UID_KEY, result, ttl, success)) {
                    // there is no existing value create a new one
                    result = UtilsUUID::GenerateUUID();
                    Exchange::ISharedStorage::Success successResult;
                    // TTL of 0 means its stored forever
                    ttl = 0;
                    sharedStorage->SetValue(Exchange::ISharedStorage::DEVICE, appId, DEVICE_UID_KEY, result, ttl, successResult);
                    if (successResult.success) {
                        return Core::ERROR_NONE;
                    } else {
                        LOGERR("Failed to set new Device UID in SharedStorage");
                        ErrorUtils::CustomInternal("Failed to set new Device UID in SharedStorage", result);
                        return Core::ERROR_GENERAL;
                    }
                }
                return Core::ERROR_NONE;
            } else {
                LOGERR("Unable to get SharedStorage interface");
                ErrorUtils::CustomInternal("Unable to get SharedStorage interface", result);
                return Core::ERROR_UNAVAILABLE;
             }
         }

        Core::hresult GetAdvertisingId(const std::string &appId, string &result /* @out */) {
            Exchange::ISharedStorage* sharedStorage = GetSharedStorage();
            string ifa;
            if (nullptr != sharedStorage) {
                uint32_t ttl;
                bool success;
                if (Core::ERROR_NONE != sharedStorage->GetValue(Exchange::ISharedStorage::DEVICE, appId, ADVERTISING_ID_KEY, ifa, ttl, success)) {
                    // there is no existing value create a new one
                    ifa = UtilsUUID::GenerateUUID();
                    Exchange::ISharedStorage::Success successResult;
                    // TTL of 0 means its stored forever
                    ttl = 0;

                    sharedStorage->SetValue(Exchange::ISharedStorage::DEVICE, appId, ADVERTISING_ID_KEY, ifa, ttl, successResult);
                    if (!successResult.success) {
                        LOGERR("Failed to set new Advertising ID in SharedStorage");
                        ErrorUtils::CustomInternal("Failed to set new Advertising ID in SharedStorage", result);
                        return Core::ERROR_GENERAL;
                    }
                }

                JsonObject response;
                response["ifa"] = ifa;
                response["ifa_type"] = ADVERTISING_TYPE;
                response["limit"] = ADVERTISING_ID_LIMIT;
                response.ToString(result);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Unable to get SharedStorage interface");
                 ErrorUtils::CustomInternal("Unable to get SharedStorage interface", result);
                return Core::ERROR_UNAVAILABLE;
            }
        }

       Core::hresult HandleAppGatewayRequest(const Exchange::GatewayContext& context ,
                                          const string& method ,
                                          const string& payload /*@opaque */,
                                          string& result /*@out @opaque */) {
            string lowerMethod = StringUtils::toLower(method);
            if ("advertising.advertisingid" == lowerMethod) {
                return GetAdvertisingId(context.appId, result);
            } else if ("device.uid" == lowerMethod) {
                return GetDeviceUID(context.appId, result);
            } else if ("stats.memoryusage" == lowerMethod) {
                return GetStatsMemoryUsage(result);
            }
            
            ErrorUtils::CustomInternal("Not Supported", result);
            return Core::ERROR_UNAVAILABLE;
        }

        Core::hresult GetStatsMemoryUsage(string &result /* @out */) {
            // TODO: Replace with the correct container ID once the platform-specific
            // format is confirmed (e.g. "com.sky.as.apps_<appId>").
            static const std::string containerId = "com.bskyb.epgui";

            Exchange::IOCIContainer* ociContainer = mShell->QueryInterfaceByCallsign<Exchange::IOCIContainer>("org.rdk.OCIContainer");
            if (ociContainer == nullptr)
            {
                LOGERR("AppDelegate: OCIContainer interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            string infoJson;
            bool success = false;
            string errorReason;
            const Core::hresult rc = ociContainer->GetContainerInfo(containerId, infoJson, success, errorReason);
            ociContainer->Release();
            ociContainer = nullptr;
            if (rc != Core::ERROR_NONE || !success)
            {
                LOGERR("AppDelegate: OCIContainer.getContainerInfo failed rc=%u reason=%s", rc, errorReason.c_str());
                return Core::ERROR_GENERAL;
            }

            // GetContainerInfo returns the info block as an opaque JSON string.
            // Parse it into a VariantContainer then navigate the nested structure.
            WPEFramework::Core::JSON::VariantContainer infoObj(infoJson);
            if (!infoObj.HasLabel(_T("memory")) || !infoObj.HasLabel(_T("gpu")))
            {
                LOGERR("AppDelegate: getContainerInfo missing info.memory or info.gpu");
                return Core::ERROR_GENERAL;
            }

            WPEFramework::Core::JSON::VariantContainer memoryObj = infoObj[_T("memory")].Object();
            WPEFramework::Core::JSON::VariantContainer gpuObj    = infoObj[_T("gpu")].Object();
            if (!memoryObj.HasLabel(_T("user")) || !gpuObj.HasLabel(_T("memory")))
            {
                LOGERR("AppDelegate: getContainerInfo missing info.memory.user or info.gpu.memory");
                return Core::ERROR_GENERAL;
            }

            WPEFramework::Core::JSON::VariantContainer userObj   = memoryObj[_T("user")].Object();
            WPEFramework::Core::JSON::VariantContainer gpuMemObj = gpuObj[_T("memory")].Object();

            // Spec requires KiB; OCIContainer returns bytes
            const uint64_t userUsedKiB  = static_cast<uint64_t>(userObj[_T("usage")].Number())   / 1024;
            const uint64_t userLimitKiB = static_cast<uint64_t>(userObj[_T("limit")].Number())   / 1024;
            const uint64_t gpuUsedKiB   = static_cast<uint64_t>(gpuMemObj[_T("usage")].Number()) / 1024;
            const uint64_t gpuLimitKiB  = static_cast<uint64_t>(gpuMemObj[_T("limit")].Number()) / 1024;

            JsonObject obj;
            obj["userMemoryUsedKiB"]  = static_cast<double>(userUsedKiB);
            obj["userMemoryLimitKiB"] = static_cast<double>(userLimitKiB);
            obj["gpuMemoryUsedKiB"]   = static_cast<double>(gpuUsedKiB);
            obj["gpuMemoryLimitKiB"]  = static_cast<double>(gpuLimitKiB);
            obj.ToString(result);
            return Core::ERROR_NONE;
        }

        Exchange::ISharedStorage* GetSharedStorage() {
            std::lock_guard<std::mutex> lock(mSharedStorageMutex);
            if (nullptr == mSharedStorage && nullptr != mShell) {
                mSharedStorage = mShell->QueryInterfaceByCallsign<Exchange::ISharedStorage>("org.rdk.SharedStorage");
                if (nullptr == mSharedStorage) {
                    LOGERR("Failed to get SharedStorage COM interface");
                }
            }
            return mSharedStorage;
        }
    private:
        mutable std::mutex mSharedStorageMutex;
        PluginHost::IShell *mShell;
        Exchange::ISharedStorage *mSharedStorage;
};

#endif