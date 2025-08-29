/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/ILEDControl.h>

#include <mutex>
#include "libIARM.h"

#include <com/com.h>
#include <core/core.h>

// debug
#include "dsFPDTypes.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class LEDControlImplementation : public Exchange::ILEDControl
        {
            public:
                // We do not allow this plugin to be copied !!
                LEDControlImplementation();
                ~LEDControlImplementation() override;

                // We do not allow this plugin to be copied !!
                LEDControlImplementation(const LEDControlImplementation&) = delete;
                LEDControlImplementation& operator=(const LEDControlImplementation&) = delete;

                BEGIN_INTERFACE_MAP(LEDControlImplementation)
                INTERFACE_ENTRY(Exchange::ILEDControl)
                END_INTERFACE_MAP

            private:
                bool m_isPlatInitialized;
				dsFPDLedState_t m_dsLEDStateDebug; // Test
				int m_dsLEDStateBitMaskDebug; // Test

            public:
                Core::hresult GetSupportedLEDStates(IStringIterator*& supportedLEDStates, bool& success) override;
                Core::hresult GetLEDState(LEDControlState& ledState) override;
                Core::hresult SetLEDState(const LEDControlState& state, bool& success) override;
        };
    } // namespace Plugin
} // namespace WPEFramework
