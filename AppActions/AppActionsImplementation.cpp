#include "UtilsLogging.h"
#include "AppActionsImplementation.h"

namespace WPEFramework {
namespace Plugin {


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
        // Implementation of ActionStart method
        LOGINFO("ActionStart called with initiator: %s, intent: %s, handlerAppId: %s", initiator.c_str(), intent.c_str(), handlerAppId.c_str());
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

} // namespace Plugin
} // namespace WPEFramework
