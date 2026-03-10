/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
**/

/**
 * Mock version of UtilsController.h for unit tests.
 * 
 * This file is placed in Tests/mocks and included before the real helpers/UtilsController.h
 * via CMake include directory ordering. It provides a mock implementation of
 * getThunderControllerClient() that returns nullptr to prevent subscription
 * errors during unit tests.
 * 
 * Note: This causes the subscription code paths in SystemDelegate to be skipped,
 * which may result in slightly lower coverage for those functions. However, 
 * this trade-off is acceptable because:
 * 1. It eliminates noisy ERROR messages during tests
 * 2. Tests run much faster (~55s vs ~15min) without network timeouts
 * 3. Overall coverage still exceeds 80% target
 */

#pragma once

#include <mutex>
// Note: curl is not needed in mock version

// std
#include <string>
#include <functional>
#include <memory>

#define MAX_STRING_LENGTH 2048

#define SERVER_DETAILS  "127.0.0.1:9998"

using namespace WPEFramework;
using namespace std;

namespace Utils
{
    struct SecurityToken
    {
        static void getSecurityToken(std::string &token)
        {
            static std::string sToken = "";
            static bool sThunderSecurityChecked = false;

            static std::mutex mtx;
            std::unique_lock<std::mutex> lock(mtx);

            if (sThunderSecurityChecked)
            {
                token = sToken;
                return;
            }

            sThunderSecurityChecked = true;

            // In test mode, always return empty token
            token = sToken;
        }
    };

    /**
     * Mock implementation of getThunderControllerClient.
     * 
     * Returns nullptr to prevent subscription attempts in SystemDelegate.
     * The SystemDelegate checks `if (_displayRpc)` before attempting to subscribe,
     * so returning nullptr here will cause subscriptions to be skipped silently.
     */
    inline std::shared_ptr<WPEFramework::JSONRPC::LinkType<WPEFramework::Core::JSON::IElement>> getThunderControllerClient(std::string callsign="")
    {
        (void)callsign;
        // Return nullptr to skip subscriptions in unit tests
        return nullptr;
    }

#ifndef USE_THUNDER_R4
    class Job : public Core::IDispatchType<void>
#else
    class Job : public Core::IDispatch
#endif /* USE_THUNDER_R4 */
    {
    public:
        Job(std::function<void()> work)
            : _work(work)
        {
        }
        void Dispatch() override
        {
            _work();
        }

    private:
        std::function<void()> _work;
    };

    inline uint32_t getServiceState(PluginHost::IShell *shell, const string &callsign, PluginHost::IShell::state &state)
    {
        uint32_t result;
        auto interface = shell->QueryInterfaceByCallsign<PluginHost::IShell>(callsign);
        if (interface == nullptr)
        {
            result = Core::ERROR_UNAVAILABLE;
            std::cout << "no IShell for " << callsign << std::endl;
        }
        else
        {
            result = Core::ERROR_NONE;
            state = interface->State();
            std::cout << "IShell state " << state << " for " << callsign << std::endl;
            interface->Release();
        }
        return result;
    }

    inline uint32_t activatePlugin(PluginHost::IShell *shell, const string &callsign)
    {
        uint32_t result = Core::ERROR_ASYNC_FAILED;
        Core::Event event(false, true);

#ifndef USE_THUNDER_R4
        Core::IWorkerPool::Instance().Submit(Core::ProxyType<Core::IDispatchType<void>>(Core::ProxyType<Job>::Create([&]()
                                                                                                                     {
#else
        Core::IWorkerPool::Instance().Submit(Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create([&]()
                                                                                                           {
#endif /* USE_THUNDER_R4 */
                    auto interface = shell->QueryInterfaceByCallsign<PluginHost::IShell>(callsign);
                    if (interface == nullptr) {
                        result = Core::ERROR_UNAVAILABLE;
                        std::cout << "no IShell for " << callsign << std::endl;
                    } else {
                        result = interface->Activate(PluginHost::IShell::reason::REQUESTED);
                        std::cout << "IShell activate status " << result << " for " << callsign << std::endl;
                        interface->Release();
                    }
                    event.SetEvent(); })));

        event.Lock();
        return result;
    }

}
