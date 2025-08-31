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

#include <core/Portability.h>
#include <interfaces/ILEDControl.h>

#include "dsFPD.h"
#include "dsError.h"
#include "dsFPDTypes.h"
#include "UtilsLogging.h"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(LEDControlImplementation, 1, 0);

        LEDControlImplementation::LEDControlImplementation(): m_isPlatInitialized (false)
        {
            LOGINFO("LEDControlImplementation Constructor called\n");
            if (!m_isPlatInitialized) {
                LOGINFO("Doing plat init; dsFPInit\n");
                if (dsERR_NONE != dsFPInit()){
                    LOGERR("dsFPInit failed\n");
                }
                m_isPlatInitialized = true;
            }
        }

        LEDControlImplementation::~LEDControlImplementation()
        {
            LOGINFO("LEDControlImplementation Destructor called\n");
            if (m_isPlatInitialized) {
                LOGINFO("Doing plat uninit; dsFPTerm\n");
                if (dsERR_NONE != dsFPTerm()) {
                    LOGERR("dsFPTerm failed\n");
                }
                m_isPlatInitialized = false;
            }
        }

        /************************ Helper Functions *************************/
        /***
         * @brief: Map ILEDControl::LEDControlState to dsFPDLedState_t
         * @param[in] state The LED control state
         * @return Corresponding dsFPDLedState_t
         */
        static dsFPDLedState_t mapFromLEDControlStateToDsFPDLedState(WPEFramework::Exchange::ILEDControl::LEDControlState state)
        {
            using LEDControlState = WPEFramework::Exchange::ILEDControl::LEDControlState;
            switch (state) {
                case LEDControlState::LEDSTATE_NONE: return dsFPD_LED_DEVICE_NONE;
                case LEDControlState::LEDSTATE_ACTIVE: return dsFPD_LED_DEVICE_ACTIVE;
                case LEDControlState::LEDSTATE_STANDBY: return dsFPD_LED_DEVICE_STANDBY;
                case LEDControlState::LEDSTATE_WPS_CONNECTING: return dsFPD_LED_DEVICE_WPS_CONNECTING;
                case LEDControlState::LEDSTATE_WPS_CONNECTED: return dsFPD_LED_DEVICE_WPS_CONNECTED;
                case LEDControlState::LEDSTATE_WPS_ERROR: return dsFPD_LED_DEVICE_WPS_ERROR;
                case LEDControlState::LEDSTATE_FACTORY_RESET: return dsFPD_LED_DEVICE_FACTORY_RESET;
                case LEDControlState::LEDSTATE_USB_UPGRADE: return dsFPD_LED_DEVICE_USB_UPGRADE;
                case LEDControlState::LEDSTATE_DOWNLOAD_ERROR: return dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR;
                /* The ILEDControl.h's LEDControlState has LEDSTATE_MAX defined but do not use it */
                default:
                    throw std::invalid_argument("Invalid LEDControlState for mapping to dsFPDLedState_t");
            }
        }

        /***
         * @brief: Map dsFPDLedState_t to ILEDControl::LEDControlState
         * @param[in] state The dsFPDLedState_t state
         * @return Corresponding ILEDControl::LEDControlState
         */
        static WPEFramework::Exchange::ILEDControl::LEDControlState mapFromDsFPDLedStateToLEDControlState(dsFPDLedState_t state)
        {
            using LEDControlState = WPEFramework::Exchange::ILEDControl::LEDControlState;
            switch (state) {
                case dsFPD_LED_DEVICE_NONE: return LEDControlState::LEDSTATE_NONE;
                case dsFPD_LED_DEVICE_ACTIVE: return LEDControlState::LEDSTATE_ACTIVE;
                case dsFPD_LED_DEVICE_STANDBY: return LEDControlState::LEDSTATE_STANDBY;
                case dsFPD_LED_DEVICE_WPS_CONNECTING: return LEDControlState::LEDSTATE_WPS_CONNECTING;
                case dsFPD_LED_DEVICE_WPS_CONNECTED: return LEDControlState::LEDSTATE_WPS_CONNECTED;
                case dsFPD_LED_DEVICE_WPS_ERROR: return LEDControlState::LEDSTATE_WPS_ERROR;
                case dsFPD_LED_DEVICE_FACTORY_RESET: return LEDControlState::LEDSTATE_FACTORY_RESET;
                case dsFPD_LED_DEVICE_USB_UPGRADE: return LEDControlState::LEDSTATE_USB_UPGRADE;
                case dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR: return LEDControlState::LEDSTATE_DOWNLOAD_ERROR;
                case dsFPD_LED_DEVICE_MAX:
                default:
                    throw std::invalid_argument("Invalid dsFPDLedState_t for mapping to LEDControlState");
            }
        }

        /***
         * @brief: Map LEDControlState to string
         * @param[in] state The LEDControlState
         * @return Corresponding string representation if valid, otherwise nullptr
         */
        static const char* LEDControlStateToString(WPEFramework::Exchange::ILEDControl::LEDControlState state)
        {
            using LEDControlState = WPEFramework::Exchange::ILEDControl::LEDControlState;
            switch (state) {
                case LEDControlState::LEDSTATE_NONE: return "NONE";
                case LEDControlState::LEDSTATE_ACTIVE: return "ACTIVE";
                case LEDControlState::LEDSTATE_STANDBY: return "STANDBY";
                case LEDControlState::LEDSTATE_WPS_CONNECTING: return "WPS_CONNECTING";
                case LEDControlState::LEDSTATE_WPS_CONNECTED: return "WPS_CONNECTED";
                case LEDControlState::LEDSTATE_WPS_ERROR: return "WPS_ERROR";
                case LEDControlState::LEDSTATE_FACTORY_RESET: return "FACTORY_RESET";
                case LEDControlState::LEDSTATE_USB_UPGRADE: return "USB_UPGRADE";
                case LEDControlState::LEDSTATE_DOWNLOAD_ERROR: return "DOWNLOAD_ERROR";
                // Treat LEDSTATE_MAX and greater as invalid
                case LEDControlState::LEDSTATE_MAX:
                default: return nullptr;
            }
        }

        /***
         * @brief: Map dsError_t to Core::hresult
         * @param[in] err The dsError_t error code
         * @return Corresponding Core::hresult, defaulting to Core::ERROR_GENERAL
         */
        Core::hresult getCoreErrorFromDSError(dsError_t err)
        {
            switch (err) {
                case dsERR_NONE:
                    return Core::ERROR_NONE;
                case dsERR_GENERAL:
                case dsERR_OPERATION_FAILED:
                    return Core::ERROR_GENERAL;
                case dsERR_INVALID_PARAM:
                    return Core::ERROR_INVALID_PARAMETER;
                case dsERR_INVALID_STATE:
                case dsERR_ALREADY_INITIALIZED:
                case dsERR_NOT_INITIALIZED:
                    return Core::ERROR_ILLEGAL_STATE;
                case dsERR_OPERATION_NOT_SUPPORTED:
                    return Core::ERROR_NOT_SUPPORTED;
                case dsERR_RESOURCE_NOT_AVAILABLE:
                    return Core::ERROR_UNAVAILABLE;
                default:
                    return Core::ERROR_GENERAL;
            }
        }

        /************************ Plugin Methods ************************/

        Core::hresult LEDControlImplementation::GetSupportedLEDStates(IStringIterator*& supportedLEDStates, bool& success)
        {
            LOGINFO("");
            unsigned int halSupportedLEDStates = (unsigned int)dsFPD_LED_DEVICE_MAX;
            {
                Core::SafeSyncType<Core::CriticalSection> lock(_adminLock);
                try {
                    dsError_t err = dsFPGetSupportedLEDStates(&halSupportedLEDStates);
                    if (dsERR_NONE != err) {
                        LOGERR("dsFPGetSupportedLEDStates error %d\n", err);
                        return getCoreErrorFromDSError(err);
                    }
                } catch (...) {
                    LOGERR("Exception in dsFPGetSupportedLEDStates.\n");
                    return Core::ERROR_GENERAL;
                }
            }
            std::list<std::string> stateNames;
            using State = WPEFramework::Exchange::ILEDControl::LEDControlState;
            for (int i = static_cast<int>(State::LEDSTATE_NONE); i < static_cast<int>(State::LEDSTATE_MAX); ++i) {
                const char* stateStr = LEDControlStateToString(static_cast<State>(i));
                if (stateStr != nullptr) {
                    // LEDControlState should be exact match with HAL supported states
                    if (halSupportedLEDStates & (1 << i)) {
                        stateNames.emplace_back(stateStr);
                    } else {
                        LOGWARN("LED state %d is not supported by HAL.\n", i);
                    }
                }
            }
            supportedLEDStates = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(stateNames);
            success = true;
            return Core::ERROR_NONE;
        }

        Core::hresult LEDControlImplementation::GetLEDState(WPEFramework::Exchange::ILEDControl::LEDControlState& ledState)
        {
            LOGINFO("");
            dsFPDLedState_t dsLEDState = dsFPD_LED_DEVICE_MAX;

            {
                Core::SafeSyncType<Core::CriticalSection> lock(_adminLock);
                try {
                    dsError_t err = dsFPGetLEDState(&dsLEDState);
                    if (err != dsERR_NONE) {
                        LOGERR("dsFPGetLEDState returned error: %d\n", err);
                        return getCoreErrorFromDSError(err);
                    }
                } catch (...) {
                    LOGERR("Exception in dsFPGetLEDState.\n");
                    return Core::ERROR_GENERAL;
                }
            }

            try {
                ledState = mapFromDsFPDLedStateToLEDControlState(dsLEDState);
            } catch (const std::invalid_argument& e) {
                LOGERR("Exception in mapFromDsFPDLedStateToLEDControlState dsFPDLedState_t value: %s\n", e.what());
                return Core::ERROR_READ_ERROR;
            }
            return Core::ERROR_NONE;
        }

        Core::hresult LEDControlImplementation::SetLEDState(const WPEFramework::Exchange::ILEDControl::LEDControlState& state, bool& success)
        {
            LOGINFO("");
            dsFPDLedState_t dsLEDState = dsFPD_LED_DEVICE_MAX;
            try {
                dsLEDState = mapFromLEDControlStateToDsFPDLedState(state);
            } catch (const std::invalid_argument& e) {
                LOGERR("Invalid dsFPDLedState_t value: %s\n", e.what());
                return Core::ERROR_READ_ERROR;
            }

            dsError_t err = dsERR_NONE;
            {
                Core::SafeSyncType<Core::CriticalSection> lock(_adminLock);
                try {
                    err = dsFPSetLEDState(dsLEDState);
                } catch (...) {
                    LOGERR("Exception in dsFPSetLEDState\n");
                    return Core::ERROR_GENERAL;
                }
            }
            if (err != dsERR_NONE) {
                Core::hresult rc = getCoreErrorFromDSError(err);
                LOGERR("Failed to set LED state to %d,dsFPGetLEDState returned error: %d\n", static_cast<int>(state),err);
                return rc;
            }
            success = true;
            return Core::ERROR_NONE;
        }
    } // namespace Plugin
} // namespace WPEFramework
