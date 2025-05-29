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

#pragma once

#include "Module.h"
#include <interfaces/ILEDControl.h>
#include <interfaces/json/JLEDControl.h>
#include <interfaces/json/JsonData_LEDControl.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework 
{
    namespace Plugin
    {
        class LEDControl : public PluginHost::IPlugin, public PluginHost::JSONRPC 
        {
            public:
                LEDControl(const LEDControl&) = delete;
                LEDControl& operator=(const LEDControl&) = delete;

                LEDControl();
                virtual ~LEDControl();

                BEGIN_INTERFACE_MAP(LEDControl)
                INTERFACE_ENTRY(PluginHost::IPlugin)
                INTERFACE_ENTRY(PluginHost::IDispatcher)
                INTERFACE_AGGREGATE(Exchange::ILEDControl, _ledcontrol)
                END_INTERFACE_MAP

                //  IPlugin methods
                // -------------------------------------------------------------------------------------------------------
                const string Initialize(PluginHost::IShell* service) override;
                void Deinitialize(PluginHost::IShell* service) override;
                string Information() const override;

            private:
                void Deactivated(RPC::IRemoteConnection* connection);

            private:
                PluginHost::IShell* _service{};
                uint32_t _connectionId{};
                Exchange::ILEDControl* _ledcontrol{};
       };
    } // namespace Plugin
} // namespace WPEFramework
