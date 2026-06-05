#include "AppActions.h"
#include "AppActionsImplementation.h"
#include <plugins/IShell.h>

#define API_VERSION_NUMBER_MAJOR    APPACTIONS_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    APPACTIONS_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    APPACTIONS_PATCH_VERSION

namespace WPEFramework {
namespace Plugin {
    SERVICE_REGISTRATION(AppActions, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    AppActions *AppActions::_instance = nullptr;

    AppActions::AppActions() : PluginHost::IPlugin(), PluginHost::JSONRPC(), mService(nullptr), mConnectionId(0), mAppActions(nullptr), mAppActionsConfigure(nullptr), mAppActionsNotification(this) {

        // Register JSONRPC methods here
        if (nullptr == AppActions::_instance)
        {
            AppActions::_instance = this;
        }
    }

    AppActions::~AppActions() {
        AppActions::_instance = nullptr;
    }

    const string AppActions::Initialize(PluginHost::IShell* service) {
        // Initialization logic
        string message = "";
        ASSERT(nullptr != service);
        ASSERT(nullptr == mService);
        ASSERT(nullptr == mAppActions);
        ASSERT(0 == mConnectionId);

        SYSLOG(Logging::Startup, (_T("[%s] Initialize entry PID=%u"), __FUNCTION__, getpid()));
        mService = service;
        mService->AddRef();
        mService->Register(&mAppActionsNotification);
        mAppActions = mService->Root<Exchange::IAppActions>(mConnectionId, 5000, _T("AppActionsImplementation"));
        if (nullptr == mAppActions)
        {
            LOGERR("Failed to initialise AppActions plugin!");
            SYSLOG(Logging::Startup, (_T("AppActions::Initialize: object creation failed")));
            message = _T("AppActions plugin could not be initialised");
        } else {
            mAppActionsConfigure = mAppActions->QueryInterface<Exchange::IConfiguration>();
            if (nullptr == mAppActionsConfigure)
            {
                LOGERR("Failed to get IConfiguration interface from AppActions plugin!");
                SYSLOG(Logging::Startup, (_T("AppActions::Initialize: IConfiguration interface not found")));
                message = _T("AppActions plugin could not be initialised due to missing IConfiguration interface");
            } else {
                if (Core::ERROR_NONE == mAppActionsConfigure->Configure(mService))
                {
                    mAppActions->Register(&mAppActionsNotification);
                    //Invoking Plugin API register to wpeframework
                    Exchange::JAppActions::Register(*this, mAppActions);
                } else {
                    SYSLOG(Logging::Startup, (_T("AppActions::Initialize: could not be configured")));
                    message = _T("AppActions could not be configured");
                }
            }
        }

        SYSLOG(Logging::Startup, (_T("[%s] Initialize exit status=%s"), __FUNCTION__, message.empty() ? "success" : "failed"));
        // On success return empty, to indicate there is no error text.
        return message;
    }

    void AppActions::Deinitialize(PluginHost::IShell* service) {
        // Deinitialization logic
        ASSERT(mService == service);
        SYSLOG(Logging::Shutdown, (_T("AppActions Deinitialize")));

        RPC::IRemoteConnection *connection = nullptr;
        VARIABLE_IS_NOT_USED uint32_t result = Core::ERROR_NONE;

        mService->Unregister(&mAppActionsNotification);

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        if (nullptr != mAppActionsConfigure)
        {
            mAppActionsConfigure->Release();
            mAppActionsConfigure = nullptr;
        }
        if (0 != mConnectionId) {
            connection = service->RemoteConnection(mConnectionId);
            // If this was running in a (container) process...
            if (nullptr != connection)
            {
                // Lets trigger the cleanup sequence for
                // out-of-process code. Which will guard
                // that unwilling processes, get shot if
                // not stopped friendly :-)
                connection->Terminate();
                connection->Release();
            }
        }
        if (nullptr != mAppActions)
        {
            mAppActions->Unregister(&mAppActionsNotification);
            Exchange::JAppActions::Unregister(*this);
            result = mAppActions->Release();
            mAppActions = nullptr;

            // It should have been the last reference we are releasing,
            // so it should end up in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);
        }
        mConnectionId = 0;
        mService->Release();
        mService = nullptr;
    }

    string AppActions::Information() const {
        return string();
    }

    void AppActions::Deactivated(RPC::IRemoteConnection *connection)
    {
        if (connection->Id() == mConnectionId)
        {
            ASSERT(nullptr != mService);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework
