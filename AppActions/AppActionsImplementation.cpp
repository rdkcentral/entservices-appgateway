#include "UtilsLogging.h"
#include "AppActionsImplementation.h"
#include <plugins/IShell.h>

#define API_VERSION_NUMBER_MAJOR    APPACTIONS_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    APPACTIONS_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    APPACTIONS_PATCH_VERSION

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(AppActionsImplementation, 1, 0, API_VERSION_NUMBER_PATCH);

    AppActionsImplementation::AppActionsImplementation() {
        // Constructor implementation
        //SYSLOG(Logging::Startup, (_T("AppActionsImplementation Constructor")));
        LOGINFO("AppActionsImplementation Constructor");
    }

    AppActionsImplementation::~AppActionsImplementation() {
        // Destructor implementation
        //SYSLOG(Logging::Shutdown, (_T("AppActionsImplementation Destructor")));
        LOGINFO("AppActionsImplementation Destructor");
    }

    Core::hresult AppActionsImplementation::ActionStart(const string& initiator, const string& intent, const string& handlerAppId)
    {
        LOGINFO("ActionStart called with initiator: %s, intent: %s, handlerAppId: %s", initiator.c_str(), intent.c_str(), handlerAppId.c_str());
        // Implementation of ActionStart method
        DispatchActionStartRequest(initiator, intent, handlerAppId);
        // Return success or appropriate error code
        return Core::ERROR_NONE;
    }
    /**
     * Register a notification callback
     */
    Core::hresult AppActionsImplementation::Register(Exchange::IAppActions::INotification *notification)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        ASSERT(nullptr != notification);
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);

        if (std::find(mAppActionsNotifications.begin(), mAppActionsNotifications.end(), notification) == mAppActionsNotifications.end())
        {
            LOGINFO("Register notification");
            mAppActionsNotifications.push_back(notification);
            notification->AddRef();
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("notification already registered");
        }
        return status;
    }
    /**
     * Unregister a notification callback
     */
    Core::hresult AppActionsImplementation::Unregister(Exchange::IAppActions::INotification *notification)
    {
        Core::hresult status = Core::ERROR_GENERAL;

        ASSERT(nullptr != notification);
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);

        auto itr = std::find(mAppActionsNotifications.begin(), mAppActionsNotifications.end(), notification);
        if (itr != mAppActionsNotifications.end())
        {
            (*itr)->Release();
            LOGINFO("Unregister notification");
            mAppActionsNotifications.erase(itr);
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("notification not found");
        }
        return status;
    }

    Core::hresult AppActionsImplementation::Configure(PluginHost::IShell* service)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        if (service != nullptr)
        {
            mService = service;
            mService->AddRef();
            LOGDBG("AppActionsImplementation service configured successfully");
        }
        else
        {
            LOGERR("AppActionsImplementation service configuration failed: service is null");
        }
        return status;
    }

    const string AppActionsImplementation::Initialize(PluginHost::IShell* service)
    {
        return Configure(service) == Core::ERROR_NONE ? string() : _T("Failed to configure AppActionsImplementation plugin");
    }

    void AppActionsImplementation::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(mService == service);
        if (mService != nullptr)
        {
            mService->Release();
            mService = nullptr;
        }
    }

    string AppActionsImplementation::Information() const {
        return string();
    }

    void AppActionsImplementation::DispatchActionStartRequest(
        const string& initiator, const string& intent, const string& handlerAppId)
    {
        Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
        LOGINFO("Dispatching ActionStartRequest to notifications: initiator=%s, intent=%s, handlerAppId=%s", initiator.c_str(), intent.c_str(), handlerAppId.c_str());
        for (auto* notification : mAppActionsNotifications) {
            notification->OnActionStartRequest(initiator, intent, handlerAppId);
        }
    }
} // namespace Plugin
} // namespace WPEFramework
