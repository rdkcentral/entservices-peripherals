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

#include "rdk/iarmmgrs-hal/pwrMgr.h"

#include "UtilsJsonRpc.h"
#include "UtilsIarm.h"
#include "dsFPD.h"

#define FPD_LED_DEVICE_NONE "NONE"
#define FPD_LED_DEVICE_ACTIVE "ACTIVE"
#define FPD_LED_DEVICE_STANDBY "STANDBY"
#define FPD_LED_DEVICE_WPS_CONNECTING "WPS_CONNECTING"
#define FPD_LED_DEVICE_WPS_CONNECTED "WPS_CONNECTED"
#define FPD_LED_DEVICE_WPS_ERROR "WPS_ERROR"
#define FPD_LED_DEVICE_FACTORY_RESET "FACTORY_RESET"
#define FPD_LED_DEVICE_USB_UPGRADE "USB_UPGRADE"
#define FPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR "DOWNLOAD_ERROR"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(LEDControlImplementation, 1, 0);
    
        LEDControlImplementation::LEDControlImplementation(): m_isPlatInitialized (false)
        {
            LOGINFO("LEDControlImplementation Constructor called");
            if (!m_isPlatInitialized){
                LOGINFO("Doing plat init");
                if (dsERR_NONE != dsFPInit()){
		    LOGERR("dsFPInit failed");
		}
                m_isPlatInitialized = true;
            }
        }

        LEDControlImplementation::~LEDControlImplementation()
        {
	    LOGINFO("LEDControlImplementation Destructor called");
	    if (m_isPlatInitialized){
                LOGINFO("Doing plat uninit");
                dsFPTerm();
                m_isPlatInitialized = false;
            }
        }

        Core::hresult LEDControlImplementation::GetSupportedLEDStates(IStringIterator*& supportedLEDStates)
        {
	    LOGINFO("");
 
            std::list<string> supportedLEDStatesInfo;

            try {
                unsigned int states = dsFPD_LED_DEVICE_NONE;
                dsError_t err = dsFPGetSupportedLEDStates (&states);
                if (!err) {
                    if(!states)supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_NONE);
                    if(states & (1<<dsFPD_LED_DEVICE_ACTIVE))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_ACTIVE);
                    if(states & (1<<dsFPD_LED_DEVICE_STANDBY))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_STANDBY);
                    if(states & (1<<dsFPD_LED_DEVICE_WPS_CONNECTING))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_WPS_CONNECTING);
                    if(states & (1<<dsFPD_LED_DEVICE_WPS_CONNECTED))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_WPS_CONNECTED);
                    if(states & (1<<dsFPD_LED_DEVICE_WPS_ERROR))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_WPS_ERROR);
                    if(states & (1<<dsFPD_LED_DEVICE_FACTORY_RESET))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_FACTORY_RESET);
                    if(states & (1<<dsFPD_LED_DEVICE_USB_UPGRADE))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_USB_UPGRADE);
                    if(states & (1<<dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR))supportedLEDStatesInfo.emplace_back(FPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR);
                    
                } else {
                        LOGERR("dsFPGetSupportedLEDStates returned error %d", err);
                }

            } catch (...){
                LOGERR("Exception in supportedLEDStates");
            }

            supportedLEDStates = (Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(supportedLEDStatesInfo));
            return Core::ERROR_NONE;
        }

        Core::hresult LEDControlImplementation::GetLEDState(LEDControlState& state)
        {
	    LOGINFO("");

            try
            {
                dsFPDLedState_t Ledstate;
                dsError_t err = dsFPGetLEDState (&Ledstate);
                if (!err) {
                    switch (Ledstate) {
                    case dsFPD_LED_DEVICE_NONE:
		        state.state = FPD_LED_DEVICE_NONE;
                        break;
                    case dsFPD_LED_DEVICE_ACTIVE:
                        state.state = FPD_LED_DEVICE_ACTIVE;
                        break;
                    case dsFPD_LED_DEVICE_STANDBY:
                        state.state = FPD_LED_DEVICE_STANDBY;
                        break;
                    case dsFPD_LED_DEVICE_WPS_CONNECTING:
                        state.state = FPD_LED_DEVICE_WPS_CONNECTING;
                        break;
                    case dsFPD_LED_DEVICE_WPS_CONNECTED:
                        state.state = FPD_LED_DEVICE_WPS_CONNECTED;
                        break;
                    case dsFPD_LED_DEVICE_WPS_ERROR:
                        state.state = FPD_LED_DEVICE_WPS_ERROR;
                        break;
                    case dsFPD_LED_DEVICE_FACTORY_RESET:
                        state.state = FPD_LED_DEVICE_FACTORY_RESET;
                        break;
                    case dsFPD_LED_DEVICE_USB_UPGRADE:
                        state.state = FPD_LED_DEVICE_USB_UPGRADE;
                        break;
                    case dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR:
                        state.state = FPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR;
                        break;

                    default :
                        LOGERR("Unsupported LEDState %d", Ledstate);
                        return WPEFramework::Core::ERROR_BAD_REQUEST;
                    }
                } else {
                    LOGERR("dsFPGetLEDState returned error %d", err);
                    return Core::ERROR_GENERAL;
                }
            }
            catch(...)
            {
                LOGERR("Exception in dsFPGetLEDState");
                return Core::ERROR_GENERAL;
            }

            return Core::ERROR_NONE;
        }

        Core::hresult LEDControlImplementation::SetLEDState(const string& state)
        {
	    LOGINFO("");
            
            if (state.empty())
            {
                LOGERR("state is empty");
                return WPEFramework::Core::ERROR_BAD_REQUEST;
            }
            
            try
            {
                dsFPDLedState_t LEDstate = dsFPD_LED_DEVICE_NONE;
                if (0==strncmp(state.c_str(), FPD_LED_DEVICE_ACTIVE, strlen(FPD_LED_DEVICE_ACTIVE)) &&
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_ACTIVE)) ){
                    LEDstate = dsFPD_LED_DEVICE_ACTIVE;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_STANDBY, strlen(FPD_LED_DEVICE_STANDBY)) &&
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_STANDBY)) ){
                    LEDstate = dsFPD_LED_DEVICE_STANDBY;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_WPS_CONNECTING, strlen(FPD_LED_DEVICE_WPS_CONNECTING)) && 
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_WPS_CONNECTING))){
                    LEDstate = dsFPD_LED_DEVICE_WPS_CONNECTING;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_WPS_CONNECTED, strlen(FPD_LED_DEVICE_WPS_CONNECTED)) &&
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_WPS_CONNECTED)) ){
                    LEDstate = dsFPD_LED_DEVICE_WPS_CONNECTED;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_WPS_ERROR, strlen(FPD_LED_DEVICE_WPS_ERROR)) &&
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_WPS_ERROR)) ){
                    LEDstate = dsFPD_LED_DEVICE_WPS_ERROR;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_FACTORY_RESET, strlen(FPD_LED_DEVICE_FACTORY_RESET)) &&
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_FACTORY_RESET)) ){
                    LEDstate = dsFPD_LED_DEVICE_FACTORY_RESET;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_USB_UPGRADE, strlen(FPD_LED_DEVICE_USB_UPGRADE)) &&
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_USB_UPGRADE)) ){
                    LEDstate = dsFPD_LED_DEVICE_USB_UPGRADE;
                } else if (0==strncmp(state.c_str(), FPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR, strlen(FPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR)) && 
				(strlen(state.c_str()) == strlen(FPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR)) ){
                    LEDstate = dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR;
                } else {
                    //Invalid parameter
                    LOGERR("UNKNOWN state : %s", state.c_str());
                    return WPEFramework::Core::ERROR_BAD_REQUEST;
                }
                if (dsFPD_LED_DEVICE_NONE!=LEDstate) {
		    LOGINFO("dsFPSetLEDState state:%s state:%d", state.c_str(), LEDstate);
                    dsError_t err = dsFPSetLEDState (LEDstate);
                    if (err) {
                        LOGERR("dsFPSetLEDState returned error %d", err);
                        return Core::ERROR_GENERAL;
                    }
                }
            }
            catch (...)
            {
                LOGERR("Exception in dsFPSetLEDState");
                return Core::ERROR_GENERAL;
            }

            return Core::ERROR_NONE;
        }

    } // namespace Plugin
} // namespace WPEFramework
