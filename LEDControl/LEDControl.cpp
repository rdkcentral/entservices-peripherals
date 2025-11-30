/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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

#include "LEDControl.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 2


namespace WPEFramework
{
    namespace {
        static Plugin::Metadata<Plugin::LEDControl> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin
    {
        /*
         *Register Ledcontrol module as wpeframework plugin
         **/
        SERVICE_REGISTRATION(LEDControl, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        LEDControl::LEDControl() : _service(nullptr), _connectionId(0), _ledcontrol(nullptr)
        {
            SYSLOG(Logging::Startup, (_T("LEDControl Constructor")));
        }

        LEDControl::~LEDControl()
        {
            SYSLOG(Logging::Shutdown, (string(_T("LEDControl Destructor"))));
        }

        const string LEDControl::Initialize(PluginHost::IShell* service)
        {
            string message="";

            ASSERT(nullptr != service);
            ASSERT(nullptr == _service);
            ASSERT(nullptr == _ledcontrol);
            ASSERT(0 == _connectionId);

            SYSLOG(Logging::Startup, (_T("LEDControl::Initialize: PID=%u"), getpid()));

            _service = service;
            _service->AddRef();
            _ledcontrol = _service->Root<Exchange::ILEDControl>(_connectionId, 5000, _T("LEDControlImplementation"));


            if(nullptr != _ledcontrol)
            {
                // Invoking Plugin API register to wpeframework
                Exchange::JLEDControl::Register(*this, _ledcontrol);
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("Ledcontrol::Initialize: Failed to initialise LEDControl plugin")));
                message = _T("LEDControl plugin could not be initialised");
            }

            return message;
        }

        void LEDControl::Deinitialize(PluginHost::IShell* service)
        {
            ASSERT(_service == service);

            SYSLOG(Logging::Shutdown, (string(_T("LEDControl::Deinitialize"))));

            if (nullptr != _ledcontrol)
            {

                Exchange::JLEDControl::Unregister(*this);

                // Stop processing:
                RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
                
                // Release the LED control interface and verify proper destruction
                // In DEBUG mode: Assert that destruction succeeded to catch reference leaks
                // In RELEASE mode: Log error if destruction didn't succeed as expected
                uint32_t result = _ledcontrol->Release();
#ifdef __DEBUG__
                ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);
#else
                if (result != Core::ERROR_DESTRUCTION_SUCCEEDED) {
                    LOGERR("LEDControl interface Release() failed with code: %u (expected DESTRUCTION_SUCCEEDED)", result);
                }
#endif

                _ledcontrol = nullptr;

                // It should have been the last reference we are releasing,
                // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
                // are leaking...

                // If this was running in a (container) process...
                if (nullptr != connection)
                {
                    // Lets trigger the cleanup sequence for
                    // out-of-process code. Which will guard
                    // that unwilling processes, get shot if
                    // not stopped friendly :-)
                    try
                    {
                        connection->Terminate();
                        // Log success if needed
                        LOGWARN("Connection terminated successfully.");
                    }
                    catch (const std::exception& e)
                    {
                        std::string errorMessage = "Failed to terminate connection: ";
                        errorMessage += e.what();
                        LOGWARN("%s",errorMessage.c_str());
                    }

                    connection->Release();
                }
            }

            _connectionId = 0;
            _service->Release();
            _service = nullptr;
            SYSLOG(Logging::Shutdown, (string(_T("LEDControl de-initialised"))));
        }

        string LEDControl::Information() const
        {
            return string(_T("Plugin which helps to control LED indicator"));
        }

        void LEDControl::Deactivated(RPC::IRemoteConnection* connection)
        {
            if (connection->Id() == _connectionId) {
                ASSERT(nullptr != _service);
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }
    } // namespace Plugin
} // namespace WPEFramework
