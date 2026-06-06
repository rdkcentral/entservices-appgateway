#include "UtilsLogging.h"
#include "AppActionsImplementation.h"
#include <plugins/IShell.h>

#include <vector>

#define API_VERSION_NUMBER_MAJOR    APPACTIONS_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    APPACTIONS_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    APPACTIONS_PATCH_VERSION

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(AppActionsImplementation, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    AppActionsImplementation::AppActionsImplementation(): mService(nullptr), mAppActionsNotifications() {
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
        LOGDBG("ActionStart called with initiator: %s, intent: %s, handlerAppId: %s", initiator.c_str(), intent.c_str(), handlerAppId.c_str());
        // Dispatch notification asynchronously to avoid blocking the caller
        Core::IWorkerPool::Instance().Submit(NotifyJob::Create(this, initiator, intent, handlerAppId));
        return Core::ERROR_NONE;
    }
    /**
     * Register a notification callback
     */
    Core::hresult AppActionsImplementation::Register(Exchange::IAppActions::INotification *notification)
    {
        if (nullptr == notification) {
            LOGERR("Register called with null notification");
            return Core::ERROR_BAD_REQUEST;
        }
        Core::hresult status = Core::ERROR_GENERAL;
        std::lock_guard<std::mutex> lock(mAdminLock);

        if (std::find(mAppActionsNotifications.begin(), mAppActionsNotifications.end(), notification) == mAppActionsNotifications.end()) {
            LOGINFO("Register notification");
            mAppActionsNotifications.push_back(notification);
            notification->AddRef();
            status = Core::ERROR_NONE;
        } else {
            LOGWARN("notification already registered");
        }
        return status;
    }
    /**
     * Unregister a notification callback
     */
    Core::hresult AppActionsImplementation::Unregister(Exchange::IAppActions::INotification *notification)
    {
        if (nullptr == notification) {
            LOGERR("Unregister called with null notification");
            return Core::ERROR_BAD_REQUEST;
        }
        Core::hresult status = Core::ERROR_GENERAL;
        std::lock_guard<std::mutex> lock(mAdminLock);

        auto itr = std::find(mAppActionsNotifications.begin(), mAppActionsNotifications.end(), notification);
        if (itr != mAppActionsNotifications.end())
        {
            (*itr)->Release();
            LOGINFO("Unregister notification");
            mAppActionsNotifications.erase(itr);
            status = Core::ERROR_NONE;
        } else {
            LOGWARN("notification not found");
        }
        return status;
    }

    Core::hresult AppActionsImplementation::Configure(PluginHost::IShell* service)
    {
        Core::hresult status = Core::ERROR_GENERAL;
        SYSLOG(Logging::Startup, (_T("AppActionsImplementation Configure entry")));
        if (nullptr != service) {
            std::lock_guard<std::mutex> lock(mAdminLock);
            if (nullptr != mService) {
                mService->Release();
                mService = nullptr;
            }
            mService = service;
            mService->AddRef();
            status = Core::ERROR_NONE;
            SYSLOG(Logging::Startup, (_T("AppActionsImplementation service configured successfully")));
        } else {
            SYSLOG(Logging::Startup, (_T("AppActionsImplementation service configuration failed: service is null")));
        }
        SYSLOG(Logging::Startup, (_T("AppActionsImplementation Configure exit status=%s"), status == Core::ERROR_NONE ? "success" : "failed"));
        return status;
    }

    const string AppActionsImplementation::Initialize(PluginHost::IShell* service)
    {
         SYSLOG(Logging::Notification, (_T("[%s] Initialize entry"), __FUNCTION__));
         const string result = (Configure(service) == Core::ERROR_NONE) ? string() : _T("Failed to configure AppActionsImplementation plugin");
         SYSLOG(Logging::Notification, (_T("[%s] Initialize exit"), __FUNCTION__));
         return result;
    }

    void AppActionsImplementation::Deinitialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Shutdown, (_T("AppActionsImplementation Deinitialize entry")));
        std::lock_guard<std::mutex> lock(mAdminLock);
        if (nullptr != mService) {
            // Only assert when mService is non-null; asserting against nullptr is
            // invalid and aborts when Deinitialize is called without prior Initialize.
            ASSERT(mService == service);
            SYSLOG(Logging::Shutdown, (_T("AppActions Deinitialize Unregistering notifications and releasing service")));
            for (auto* notification : mAppActionsNotifications) {
                notification->Release();
            }
            mAppActionsNotifications.clear();
            mService->Release();
            mService = nullptr;
        }
        SYSLOG(Logging::Shutdown, (_T("AppActionsImplementation Deinitialize exit")));
    }

    string AppActionsImplementation::Information() const {
        return string();
    }

    void AppActionsImplementation::DispatchActionStartRequest(
        const string& initiator, const string& intent, const string& handlerAppId)
    {
        LOGDBG("Dispatching ActionStartRequest to notifications: initiator=%s, intent=%s, handlerAppId=%s",
            initiator.c_str(), intent.c_str(), handlerAppId.c_str());

        std::vector<Exchange::IAppActions::INotification*> notifications;

        {
            std::lock_guard<std::mutex> lock(mAdminLock);
            notifications.assign(mAppActionsNotifications.begin(), mAppActionsNotifications.end());

            for (auto* notification : notifications) {
                if (nullptr != notification) {
                    notification->AddRef();
                }
            }
        }

        for (auto* notification : notifications) {
            if (nullptr != notification) {
                notification->OnActionStartRequest(initiator, intent, handlerAppId);
                notification->Release();
            }
        }
    }
} // namespace Plugin
} // namespace WPEFramework
