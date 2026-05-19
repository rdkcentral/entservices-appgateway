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

    AppActions::AppActions() : PluginHost::IPlugin(), PluginHost::JSONRPC(), mService(nullptr), mConnectionId(0), mAppActions(nullptr), mAppActionsConfigure(nullptr) {
        LOGINFO("AppActions Constructor");

        // Register JSONRPC methods here
        if (nullptr == AppActions::_instance)
        {
            AppActions::_instance = this;
        }
    }

    AppActions::~AppActions() {
        LOGINFO("AppActions Destructor");
        AppActions::_instance = nullptr;
    }

    const string AppActions::Initialize(PluginHost::IShell* service) {
        // Initialization logic
        string message = "";
        ASSERT(nullptr != service);
        ASSERT(nullptr == mService);
        ASSERT(nullptr == mAppActions);
        ASSERT(0 == mConnectionId);

        LOGINFO("AppActions::Initialize: PID=%u", getpid());
        SYSLOG(Logging::Startup, (_T("AppActions Initialize")));
        // On success return empty, to indicate there is no error text.
        mService = service;
        mService->AddRef();
        mAppActions = mService->Root<Exchange::IAppActions>(mConnectionId, 5000, _T("AppActionsImplementation"));
        if (nullptr == mAppActions)
        {
            LOGERR("Failed to initialise AppActions plugin!");
            SYSLOG(Logging::Startup, (_T("AppActions::Initialize: object creation failed")));
            message = _T("AppActions plugin could not be initialised");
        } else {
            auto configConnection = mAppActions->QueryInterface<Exchange::IConfiguration>();
            if (configConnection != nullptr) {
                configConnection->Configure(service);
                configConnection->Release();
            }

            //Invoking Plugin API register to wpeframework
            //Exchange::JAppActionsResolver::Resolver(*this, mAppActions);
        }

        return (service != nullptr)
            ? EMPTY_STRING
            : _T("Could not retrieve the AppActions interface.");
    }

    void AppActions::Deinitialize(PluginHost::IShell* service) {
        // Deinitialization logic
        ASSERT(mService == service);
        LOGINFO("AppActions Deinitialize");
        SYSLOG(Logging::Shutdown, (_T("AppActions Deinitialize")));

        RPC::IRemoteConnection *connection = nullptr;
        VARIABLE_IS_NOT_USED uint32_t result = Core::ERROR_NONE;

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        if (nullptr != mAppActionsConfigure)
        {
            mAppActionsConfigure->Release();
            mAppActionsConfigure = nullptr;
        }
        if ((mAppActions != nullptr) && (mConnectionId != 0)) {
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
            //Exchange::JAppActionsResolver::Unregister(*this);
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

} // namespace Plugin
} // namespace WPEFramework
