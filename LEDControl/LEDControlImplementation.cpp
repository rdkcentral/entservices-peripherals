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

#include "LEDControlImplementation.h"

#include <algorithm>

#include "UtilsJsonRpc.h"
#include "UtilsIarm.h"
#include "dsFPD.h"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(LEDControlImplementation, 1, 0);

        LEDControlImplementation::LEDControlImplementation(): m_isPlatInitialized (false)
        {
            LOGINFO("LEDControlImplementation Constructor called");
            if (!m_isPlatInitialized) {
                LOGINFO("Doing plat init; dsFPInit.");
                if (dsERR_NONE != dsFPInit()){
                    LOGERR("dsFPInit failed");
                }
                m_isPlatInitialized = true;
            }
        }

        LEDControlImplementation::~LEDControlImplementation()
        {
            LOGINFO("LEDControlImplementation Destructor called");
            if (m_isPlatInitialized) {
                LOGINFO("Doing plat uninit; dsFPTerm.");
                if (dsERR_NONE != dsFPTerm()) {
                    LOGERR("dsFPTerm failed.");
                }
                m_isPlatInitialized = false;
            }
        }

        /************************ Map Functions *************************/
        /***
         * @brief: Map ILEDControl::LEDControlState to dsFPDLedState_t
         * @param[in] state The LED control state
         * @return Corresponding dsFPDLedState_t
         */
        static dsFPDLedState_t mapFromLEDControlStateToDsFPDLedState(WPEFramework::Exchange::ILEDControl::LEDControlState state)
        {
            using LEDControlState = WPEFramework::Exchange::ILEDControl::LEDControlState;
            switch (state) {
                case LEDControlState::LEDSTATE_NONE:
                    return dsFPD_LED_DEVICE_NONE;
                case LEDControlState::LEDSTATE_ACTIVE:
                    return dsFPD_LED_DEVICE_ACTIVE;
                case LEDControlState::LEDSTATE_STANDBY:
                    return dsFPD_LED_DEVICE_STANDBY;
                case LEDControlState::LEDSTATE_WPS_CONNECTING:
                    return dsFPD_LED_DEVICE_WPS_CONNECTING;
                case LEDControlState::LEDSTATE_WPS_CONNECTED:
                    return dsFPD_LED_DEVICE_WPS_CONNECTED;
                case LEDControlState::LEDSTATE_WPS_ERROR:
                    return dsFPD_LED_DEVICE_WPS_ERROR;
                case LEDControlState::LEDSTATE_FACTORY_RESET:
                    return dsFPD_LED_DEVICE_FACTORY_RESET;
                case LEDControlState::LEDSTATE_USB_UPGRADE:
                    return dsFPD_LED_DEVICE_USB_UPGRADE;
                case LEDControlState::LEDSTATE_SOFTWARE_DOWNLOAD_ERROR:
                    return dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR;
                default:
                    throw std::invalid_argument("Invalid LEDControlState for mapping to dsFPDLedState_t");
            }
        }

        /***
         * @brief: Map dsFPDLedState_t to ILEDControl::LEDControlState
         * @param[in] state The dsFPDLedState_t state
         * @return Corresponding ILEDControl::LEDControlState, default LEDControlState::LEDSTATE_NONE
         */
        static WPEFramework::Exchange::ILEDControl::LEDControlState mapFromDsFPDLedStateToLEDControlState(dsFPDLedState_t state)
        {
            using LEDControlState = WPEFramework::Exchange::ILEDControl::LEDControlState;
            switch (state) {
                case dsFPD_LED_DEVICE_NONE:
                    return LEDControlState::LEDSTATE_NONE;
                case dsFPD_LED_DEVICE_ACTIVE:
                    return LEDControlState::LEDSTATE_ACTIVE;
                case dsFPD_LED_DEVICE_STANDBY:
                    return LEDControlState::LEDSTATE_STANDBY;
                case dsFPD_LED_DEVICE_WPS_CONNECTING:
                    return LEDControlState::LEDSTATE_WPS_CONNECTING;
                case dsFPD_LED_DEVICE_WPS_CONNECTED:
                    return LEDControlState::LEDSTATE_WPS_CONNECTED;
                case dsFPD_LED_DEVICE_WPS_ERROR:
                    return LEDControlState::LEDSTATE_WPS_ERROR;
                case dsFPD_LED_DEVICE_FACTORY_RESET:
                    return LEDControlState::LEDSTATE_FACTORY_RESET;
                case dsFPD_LED_DEVICE_USB_UPGRADE:
                    return LEDControlState::LEDSTATE_USB_UPGRADE;
                case dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR:
                    return LEDControlState::LEDSTATE_SOFTWARE_DOWNLOAD_ERROR;
                default:
                    throw std::invalid_argument("Invalid dsFPDLedState_t for mapping to LEDControlState");
            }
        }

        /************************ Plugin Methods ************************/

        Core::hresult LEDControlImplementation::GetSupportedLEDStates(IStringIterator*& supportedLEDStates, bool& success)
        {
            LOGINFO("");

            std::list<std::string> supportedLEDStatesInfo;
            using LEDControlState = WPEFramework::Exchange::ILEDControl::LEDControlState;
            using WPEFramework::Exchange::ILEDControl::LEDControlStateToString;

            constexpr int first = static_cast<int>(LEDControlState::LEDSTATE_NONE);
            // Exclude  UNKNOWN from supported. See ILEDControl.h enum class LEDControlState.
            constexpr int last = static_cast<int>(LEDControlState::LEDSTATE_UNKNOWN) - 1;
            for (int i = first; i <= last; ++i) {
                LEDControlState state = static_cast<LEDControlState>(i);
                const char* stateStr = LEDControlStateToString(state);
                // Exclude empty and UNKNOWN
                if (stateStr && *stateStr && std::string(stateStr) != "UNKNOWN") {
                    supportedLEDStatesInfo.emplace_back(stateStr);
                }
            }
            supportedLEDStates = (Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(supportedLEDStatesInfo));
            LOGINFO("Supported LED States: ");
            for (const auto& state : supportedLEDStatesInfo) {
                LOGINFO(" - %s", state.c_str());
            }
            success = true;
            return Core::ERROR_NONE;
        }

        Core::hresult LEDControlImplementation::GetLEDState(LEDControlState& ledState)
        {
            LOGINFO("");
            dsFPDLedState_t dsLEDState;
            dsError_t err = dsERR_NONE;
            try {
                err = dsFPGetLEDState(&dsLEDState);
            } catch (...) {
                LOGERR("Exception in dsFPGetLEDState");
                return Core::ERROR_GENERAL;
            }
            if (dsERR_NONE != err) {
                LOGERR("Failed to get LED state from dsFPGetLEDState.");
                return Core::ERROR_GENERAL;
            }
            try {
                ledState = mapFromDsFPDLedStateToLEDControlState(dsLEDState);
            } catch (const std::invalid_argument& e) {
                LOGERR("Invalid dsFPDLedState_t value: %s", e.what());
                return Core::ERROR_ILLEGAL_STATE;
            }
            LOGINFO("Current LED State: %s", LEDControlStateToString(ledState));
            return Core::ERROR_NONE;
        }

        Core::hresult LEDControlImplementation::SetLEDState(const LEDControlState& state, bool& success)
        {
            LOGINFO("");
            dsFPDLedState_t dsLEDState;
            try {
                dsLEDState = MapToDsFPDLedState(state);
            } catch (const std::invalid_argument& e) {
                LOGERR("Invalid dsFPDLedState_t value: %s", e.what());
                success = false;
                return Core::ERROR_INVALID_PARAMETER;
            }

            dsError_t err = dsERR_NONE;
            try {
                err = dsFPSetLEDState(dsLEDState);
            } catch (...) {
                LOGERR("Exception in dsFPSetLEDState");
                success = false;
                return Core::ERROR_GENERAL;
            }
            if (dsERR_NONE != err) {
                LOGERR("Failed to set LED state to %s", LEDControlStateToString(state));
                success = false;
                return Core::ERROR_GENERAL;
            }
            success = true;
            return Core::ERROR_NONE;
        }
    } // namespace Plugin
} // namespace WPEFramework
