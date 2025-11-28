/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
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

/**
* @defgroup servicemanager
* @{
* @defgroup src
* @{
**/

//#define USE_DS //TODO - this was defined in servicemanager.pro for all STB builds.  Not sure where to put it except here for now
//#define HAS_API_POWERSTATE

#include "frontpanel.h"
#ifdef USE_DS
    #include "frontPanelConfig.hpp"
    #include "frontPanelTextDisplay.hpp"
    #include "manager.hpp"
#endif

#include <time.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <atomic>

#if defined(HAS_API_POWERSTATE)
#include "libIBus.h"
#include <interfaces/IPowerManager.h>

using namespace WPEFramework;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
#endif

#include "UtilsJsonRpc.h"
#include "UtilsLogging.h"
#include "UtilssyncPersistFile.h"
#include "PowerManagerInterface.h"
#include "UtilsSearchRDKProfile.h"

#define FP_SETTINGS_FILE_JSON "/opt/fp_service_preferences.json"

/*
Requirement now
    Ability to get/set Led brightness
    Ability to power off/on a led

*/

namespace WPEFramework
{

    namespace Plugin
    {
        CFrontPanel* CFrontPanel::s_instance = NULL;
        static int globalLedBrightness = 100;

        int CFrontPanel::initDone = 0;
        static std::atomic<bool> isMessageLedOn{false};
        static std::atomic<bool> isRecordLedOn{false};

        static bool powerStatus = false;     //Check how this works on xi3 and rng's
        static bool started = false;
        static int m_numberOfBlinks = 0;
        static int m_maxNumberOfBlinkRepeats = 0;
        static int m_currentBlinkListIndex = 0;
        static std::vector<std::string> m_lights;
        static device::List <device::FrontPanelIndicator> fpIndicators;
        static PowerManagerInterfaceRef _powerManagerPlugin;

        static Core::TimerType<BlinkInfo> blinkTimer(64 * 1024, "BlinkTimer");

        namespace
        {

            struct Mapping
            {
                const char *IArmBusName;
                const char *SvcManagerName;
            };

            static struct Mapping name_mappings[] = {
                { "Record" , "record_led"},
                { "Message" , "data_led"},
                { "Power" , "power_led"},
                // TODO: add your mappings here
                // { <IARM_NAME>, <SVC_MANAGER_API_NAME> },
                { 0,  0}
            };

            std::string svc2iarm(const std::string &name)
            {
                const char *s = name.c_str();
                static constexpr size_t NAME_MAPPINGS_SIZE = sizeof(name_mappings) / sizeof(name_mappings[0]);

                for (size_t i = 0; i < NAME_MAPPINGS_SIZE && name_mappings[i].SvcManagerName; i++)
                {
                    if (strcmp(s, name_mappings[i].SvcManagerName) == 0)
                        return name_mappings[i].IArmBusName;
                }
                return name;
            }
        }

        CFrontPanel::CFrontPanel()
            : m_blinkTimer(this)
            , m_isBlinking(false)
        {
        }

        CFrontPanel* CFrontPanel::instance(PluginHost::IShell *service)
        {
            if (!initDone)
            {
                if (nullptr != service)
                {
                    try {
                        _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                                          .withIShell(service)
                                          .withRetryIntervalMS(200)
                                          .withRetryCount(25)
                                          .createInterface();
                    } catch (const std::exception& e) {
                        LOGERR("Exception during PowerManager initialization: %s", e.what());
                    } catch (...) {
                        LOGERR("Unknown exception during PowerManager initialization");
                    }
                }
                if (!s_instance)
                    s_instance = new CFrontPanel;
#ifdef USE_DS
                try
                {
                    LOGINFO("Front panel init");
                    fpIndicators = device::FrontPanelConfig::getInstance().getIndicators();

                    for (uint i = 0; i < fpIndicators.size(); i++)
                    {
                        std::string IndicatorNameIarm = fpIndicators.at(i).getName();

                        auto it = std::find(m_lights.begin(), m_lights.end(), IndicatorNameIarm);
                        if (m_lights.end() == it)
                        {
                            m_lights.push_back(IndicatorNameIarm);
                        }
                    }

#if defined(HAS_API_POWERSTATE)
                    {
                        Core::hresult res = Core::ERROR_GENERAL;
                        PowerState pwrStateCur = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
                        PowerState pwrStatePrev = WPEFramework::Exchange::IPowerManager::POWER_STATE_UNKNOWN;
                        ASSERT (_powerManagerPlugin);
                        if (_powerManagerPlugin) {
                            res = _powerManagerPlugin->GetPowerState(pwrStateCur, pwrStatePrev);
                            if (Core::ERROR_NONE == res)
                            {
                                if (pwrStateCur == WPEFramework::Exchange::IPowerManager::POWER_STATE_ON)
                                    powerStatus = true;
                            } else {
                                LOGERR("GetPowerState failed with error: %d, powerStatus remains: %d", res, powerStatus);
                            }
                            LOGINFO("pwrStateCur[%d] pwrStatePrev[%d] powerStatus[%d]", pwrStateCur, pwrStatePrev, powerStatus);
                        }
                    }
#endif

                    try {
                        globalLedBrightness = device::FrontPanelIndicator::getInstance("Power").getBrightness();
                        LOGINFO("Power light brightness, %d, power status %d", globalLedBrightness, powerStatus);
                    } catch (const std::exception& e) {
                        LOGERR("Exception getting brightness: %s", e.what());
                    } catch (...) {
                        LOGERR("Unknown exception getting brightness");
                    }

		    profileType = searchRdkProfile();
		    if (TV != profileType)
		    {
                        for (uint i = 0; i < fpIndicators.size(); i++)
			{
                            LOGWARN("Initializing light %s", fpIndicators.at(i).getName().c_str());
			    if (powerStatus)
                                device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setBrightness(globalLedBrightness, false);

			    device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setState(false);
			}
		    }
		    else
		    {
                        LOGWARN("Power LED Initializing is not set since we continue with bootloader patern");
		    }

		    if (powerStatus)
                        device::FrontPanelIndicator::getInstance("Power").setState(true);

                }
                catch (...)
                {
                    LOGERR("Exception Caught during [CFrontPanel::instance]\r\n");
                }
                initDone=1;
#endif
            }

            return s_instance;
        }


        void CFrontPanel::deinitialize()
        {
            // Clear observers before deleting s_instance to prevent dangling pointers
            if (s_instance) {
                s_instance->observers_.clear();
                s_instance->stop();
            }
            
            if (_powerManagerPlugin) {
                _powerManagerPlugin.Reset();
            }
            if (s_instance) {
                delete s_instance;
                s_instance = nullptr;
            }
            initDone = 0;
        }

        bool CFrontPanel::start()
        {
            LOGWARN("Front panel start");
            try
            {
                if (powerStatus)
                    device::FrontPanelIndicator::getInstance("Power").setState(true);

                device::List <device::FrontPanelIndicator> fpIndicators = device::FrontPanelConfig::getInstance().getIndicators();
                for (uint i = 0; i < fpIndicators.size(); i++)
                {
                    std::string IndicatorNameIarm = fpIndicators.at(i).getName();

                    auto it = std::find(m_lights.begin(), m_lights.end(), IndicatorNameIarm);
                    if (m_lights.end() == it)
                        m_lights.push_back(IndicatorNameIarm);
                }
            }
            catch (...)
            {
                LOGERR("Frontpanel Exception Caught during [%s]\r\n", __func__);
            }
            if (!started)
            {
                m_numberOfBlinks = 0;
                m_maxNumberOfBlinkRepeats = 0;
                m_currentBlinkListIndex = 0;
                started = true;
            }
            return true;
        }

        bool CFrontPanel::stop()
        {
            stopBlinkTimer();
            return true;
        }

        void CFrontPanel::setPowerStatus(bool bPowerStatus)
        {
            powerStatus = bPowerStatus;
        }

        std::string CFrontPanel::getLastError()
        {
            return lastError_;
        }

        void CFrontPanel::addEventObserver(FrontPanelImplementation* o)
        {

            auto it = std::find(observers_.begin(), observers_.end(), o);

            if (observers_.end() == it)
                observers_.push_back(o);
        }

        void CFrontPanel::removeEventObserver(FrontPanelImplementation* o)
        {
            observers_.remove(o);
        }

        bool CFrontPanel::setBrightness(int fp_brightness)
        {
            stopBlinkTimer();
            globalLedBrightness = fp_brightness;

            try
            {
                for (uint i = 0; i < fpIndicators.size(); i++)
                {
                    device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setBrightness(globalLedBrightness);
                }
            }
            catch (...)
            {
                LOGERR("Frontpanel Exception Caught during [%s]\r\n",__func__);
            }

            powerOnLed(FRONT_PANEL_INDICATOR_ALL);
            return true;
        }

        int CFrontPanel::getBrightness()
        {
            try
            {
                globalLedBrightness = device::FrontPanelIndicator::getInstance("Power").getBrightness();
                LOGWARN("Power light brightness, %d\n", globalLedBrightness);
            }
            catch (...)
            {
                LOGERR("Frontpanel Exception Caught during [%s]\r\n", __func__);
            }

            return globalLedBrightness;
        }

        bool CFrontPanel::powerOnLed(frontPanelIndicator fp_indicator)
        {
            stopBlinkTimer();
            try
            {
                if (powerStatus)
                {
                    switch (fp_indicator)
                    {
                    case FRONT_PANEL_INDICATOR_MESSAGE:
                        try {
                            device::FrontPanelIndicator::getInstance("Message").setState(true);
                            isMessageLedOn = true;
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power on Message LED: %s", e.what());
                            throw;
                        }
                        break;
                    case FRONT_PANEL_INDICATOR_RECORD:
                        try {
                            device::FrontPanelIndicator::getInstance("Record").setState(true);
                            isRecordLedOn = true;
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power on Record LED: %s", e.what());
                            throw;
                        }
                        break;
                    case FRONT_PANEL_INDICATOR_REMOTE:
                        try {
                            device::FrontPanelIndicator::getInstance("Remote").setState(true);
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power on Remote LED: %s", e.what());
                            throw;
                        }
                        break;
                    case FRONT_PANEL_INDICATOR_RFBYPASS:
                        try {
                            device::FrontPanelIndicator::getInstance("RfByPass").setState(true);
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power on RfByPass LED: %s", e.what());
                            throw;
                        }
                        break;
                    case FRONT_PANEL_INDICATOR_ALL:
                        // Turn on LEDs one by one, logging failures but continuing
                        if (isMessageLedOn) {
                            try {
                                device::FrontPanelIndicator::getInstance("Message").setState(true);
                            } catch (const std::exception& e) {
                                LOGERR("Failed to power on Message LED in ALL: %s", e.what());
                            }
                        }
                        if (isRecordLedOn) {
                            try {
                                device::FrontPanelIndicator::getInstance("Record").setState(true);
                            } catch (const std::exception& e) {
                                LOGERR("Failed to power on Record LED in ALL: %s", e.what());
                            }
                        }
                        try {
                            device::FrontPanelIndicator::getInstance("Power").setState(true);
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power on Power LED in ALL: %s", e.what());
                        }
                        break;
                    case FRONT_PANEL_INDICATOR_POWER:
                        try {
                            device::FrontPanelIndicator::getInstance("Power").setState(true);
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power on Power LED: %s", e.what());
                            throw;
                        }
                        break;
                    default:
                        LOGERR("Invalid Indicator %d", fp_indicator);
                    }
                }
            }
            catch (const std::exception& e)
            {
                LOGERR("FrontPanel Exception Caught during [%s]: %s\r\n", __func__, e.what());
                return false;
            }
            catch (...)
            {
                LOGERR("FrontPanel Unknown Exception Caught during [%s]\r\n", __func__);
                return false;
            }
            return true;
        }

        bool CFrontPanel::powerOffLed(frontPanelIndicator fp_indicator)
        {
            stopBlinkTimer();
            try
            {
                switch (fp_indicator)
                {
                case FRONT_PANEL_INDICATOR_MESSAGE:
                    try {
                        device::FrontPanelIndicator::getInstance("Message").setState(false);
                        isMessageLedOn = false;
                    } catch (const std::exception& e) {
                        LOGERR("Failed to power off Message LED: %s", e.what());
                        throw;
                    }
                    break;
                case FRONT_PANEL_INDICATOR_RECORD:
                    try {
                        device::FrontPanelIndicator::getInstance("Record").setState(false);
                        isRecordLedOn = false;
                    } catch (const std::exception& e) {
                        LOGERR("Failed to power off Record LED: %s", e.what());
                        throw;
                    }
                    break;
                case FRONT_PANEL_INDICATOR_REMOTE:
                    try {
                        device::FrontPanelIndicator::getInstance("Remote").setState(false);
                    } catch (const std::exception& e) {
                        LOGERR("Failed to power off Remote LED: %s", e.what());
                        throw;
                    }
                    break;
                case FRONT_PANEL_INDICATOR_RFBYPASS:
                    try {
                        device::FrontPanelIndicator::getInstance("RfByPass").setState(false);
                    } catch (const std::exception& e) {
                        LOGERR("Failed to power off RfByPass LED: %s", e.what());
                        throw;
                    }
                    break;
                case FRONT_PANEL_INDICATOR_ALL:
                    // Turn off LEDs one by one, logging failures but continuing
                    for (uint i = 0; i < fpIndicators.size(); i++)
                    {
                        try {
                            LOGWARN("powerOffLed for Indicator %s", fpIndicators.at(i).getName().c_str());
                            device::FrontPanelIndicator::getInstance(fpIndicators.at(i).getName()).setState(false);
                        } catch (const std::exception& e) {
                            LOGERR("Failed to power off LED %s in ALL: %s", fpIndicators.at(i).getName().c_str(), e.what());
                        }
                    }
                    break;
                case FRONT_PANEL_INDICATOR_POWER:
                    try {
                        device::FrontPanelIndicator::getInstance("Power").setState(false);
                    } catch (const std::exception& e) {
                        LOGERR("Failed to power off Power LED: %s", e.what());
                        throw;
                    }
                    break;
                default:
                    LOGERR("Invalid Indicator %d", fp_indicator);
                }
            }
            catch (const std::exception& e)
            {
                LOGERR("FrontPanel Exception Caught during [%s]: %s\r\n", __func__, e.what());
                return false;
            }
            catch (...)
            {
                LOGERR("FrontPanel Unknown Exception Caught during [%s]\r\n", __func__);
                return false;
            }
            return true;
        }


        bool CFrontPanel::powerOffAllLed()
        {
            powerOffLed(FRONT_PANEL_INDICATOR_ALL);
            return true;
        }

        bool CFrontPanel::powerOnAllLed()
        {
            powerOnLed(FRONT_PANEL_INDICATOR_ALL);
            return true;
        }

        bool CFrontPanel::setLED(const JsonObject& parameters)
        {
            stopBlinkTimer();
            bool success = false;
            string ledIndicator = svc2iarm(parameters["ledIndicator"].String());
            int brightness = -1;

            if (parameters.HasLabel("brightness"))
                //brightness = properties["brightness"].Number();
                getNumberParameter("brightness", brightness);

            unsigned int color = 0;
            if (parameters.HasLabel("color") && !parameters["color"].String().empty()) //color mode 2
            {
                string colorString = parameters["color"].String();
                try
                {
                    device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).setColor(device::FrontPanelIndicator::Color::getInstance(colorString.c_str()), false);
                    success = true;
                }
                catch (const std::exception& e)
                {
                    LOGERR("Failed to set color for LED %s: %s", ledIndicator.c_str(), e.what());
                    success = false;
                }
                catch (...)
                {
                    LOGERR("Unknown exception while setting color for LED %s", ledIndicator.c_str());
                    success = false;
                }
            }
            else if (parameters.HasLabel("red")) //color mode 1
            {
                unsigned int red = 0, green = 0, blue = 0;

                getNumberParameter("red", red);
                getNumberParameter("green", green);
                getNumberParameter("blue", blue);

                color = (red << 16) | (green << 8) | blue;
                try
                {
                    device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).setColor(color);
                    success = true;
                }
                catch (const std::exception& e)
                {
                    LOGERR("Failed to set RGB color for LED %s: %s", ledIndicator.c_str(), e.what());
                    success = false;
                }
                catch (...)
                {
                    LOGERR("Unknown exception while setting RGB color for LED %s", ledIndicator.c_str());
                    success = false;
                }
            }

            LOGWARN("setLed ledIndicator: %s brightness: %d", parameters["ledIndicator"].String().c_str(), brightness);
            try
            {
                if (brightness == -1)
                    brightness = device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).getBrightness(true);

                device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).setBrightness(brightness, false);
                success = true;
            }
            catch (const std::exception& e)
            {
                LOGERR("Failed to set brightness for LED %s: %s", ledIndicator.c_str(), e.what());
                success = false;
            }
            catch (...)
            {
                LOGERR("Unknown exception while setting brightness for LED %s", ledIndicator.c_str());
                success = false;
            }
            return success;
        }

        void CFrontPanel::setBlink(const JsonObject& blinkInfo)
        {
            stopBlinkTimer();
            m_blinkList.clear();
            string ledIndicator = svc2iarm(blinkInfo["ledIndicator"].String());
            int iterations = 0;
            getNumberParameterObject(blinkInfo, "iterations", iterations);
            JsonArray patternList = blinkInfo["pattern"].Array();
            for (int i = 0; i < patternList.Length(); i++)
            {
                JsonObject frontPanelBlinkHash = patternList[i].Object();
                FrontPanelBlinkInfo frontPanelBlinkInfo;
                frontPanelBlinkInfo.ledIndicator = ledIndicator;
                int brightness = -1;
                if (frontPanelBlinkHash.HasLabel("brightness"))
                    getNumberParameterObject(frontPanelBlinkHash, "brightness", brightness);

                int duration = 0;
                getNumberParameterObject(frontPanelBlinkHash, "duration", duration);
                
                // Validate duration to prevent overflow and ensure reasonable values
                // Duration must be non-negative and within acceptable range (max 1 hour = 3600000ms)
                if (duration < 0 || duration > 3600000) {
                    LOGERR("Invalid duration value: %d (must be 0-3600000ms). Using 0.", duration);
                    duration = 0;
                }
                
                LOGWARN("setBlink ledIndicator: %s iterations: %d brightness: %d duration: %d", ledIndicator.c_str(), iterations, brightness, duration);
                frontPanelBlinkInfo.brightness = brightness;
                frontPanelBlinkInfo.durationInMs = duration;
                frontPanelBlinkInfo.colorValue = 0;
                if (frontPanelBlinkHash.HasLabel("color")) //color mode 2
                {
                    string color = frontPanelBlinkHash["color"].String();
                    frontPanelBlinkInfo.colorName = color;
                    frontPanelBlinkInfo.colorMode = 2;
                }
                else if (frontPanelBlinkHash.HasLabel("red")) //color mode 1
                {
                    unsigned int red = 0, green = 0, blue = 0;

                    getNumberParameterObject(frontPanelBlinkHash, "red", red);
                    getNumberParameterObject(frontPanelBlinkHash, "green", green);
                    getNumberParameterObject(frontPanelBlinkHash, "blue", blue);

                    frontPanelBlinkInfo.colorValue = (red << 16) | (green << 8) | blue;
                    frontPanelBlinkInfo.colorMode = 1;
                }
                else
                {
                    frontPanelBlinkInfo.colorMode = 0;
                }
                m_blinkList.push_back(frontPanelBlinkInfo);
            }
            startBlinkTimer(iterations);
        }

        void CFrontPanel::startBlinkTimer(int numberOfBlinkRepeats)
        {
            LOGWARN("startBlinkTimer numberOfBlinkRepeats: %d m_blinkList.length : %zu", numberOfBlinkRepeats, m_blinkList.size());
            stopBlinkTimer();
            m_numberOfBlinks = 0;
            m_isBlinking = true;
            m_maxNumberOfBlinkRepeats = numberOfBlinkRepeats;
            m_currentBlinkListIndex = 0;
            if (m_blinkList.size() > 0)
            {
                FrontPanelBlinkInfo blinkInfo = m_blinkList.at(0);
                setBlinkLed(blinkInfo);
                if (m_isBlinking)
                    blinkTimer.Schedule(Core::Time::Now().Add(blinkInfo.durationInMs), m_blinkTimer);
            }
        }

        void CFrontPanel::stopBlinkTimer()
        {
            m_isBlinking = false;
            blinkTimer.Revoke(m_blinkTimer);
        }

        void CFrontPanel::setBlinkLed(FrontPanelBlinkInfo blinkInfo)
        {
            std::string ledIndicator = blinkInfo.ledIndicator;
            int brightness = blinkInfo.brightness;
            try
            {
                if (blinkInfo.colorMode == 1)
                {
                    device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).setColor(blinkInfo.colorValue, false);
                }
                else if (blinkInfo.colorMode == 2)
                {
                    device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).setColor(device::FrontPanelIndicator::Color::getInstance(blinkInfo.colorName.c_str()), false);
                }

            }
            catch (const std::exception& e)
            {
                LOGERR("Exception caught in setBlinkLed for setColor: %s", e.what());
            }
            catch (...)
            {
                LOGERR("Unknown exception caught in setBlinkLed for setColor");
            }
            try
            {
                if (brightness == -1)
                    brightness = device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).getBrightness(true);

                device::FrontPanelIndicator::getInstance(ledIndicator.c_str()).setBrightness(brightness, false);
            }
            catch (const std::exception& e)
            {
                LOGERR("Exception caught in setBlinkLed for setBrightness: %s", e.what());
            }
            catch (...)
            {
                LOGWARN("Exception caught in setBlinkLed for setBrightness ");
            }
        }

        void CFrontPanel::onBlinkTimer()
        {
            m_currentBlinkListIndex++;
            bool blinkAgain = true;
            
            // Blink logic state machine:
            // 1. Check if we've reached the end of the pattern list
            // 2. If at end, reset index and increment blink count
            // 3. Continue if max repeats not reached (negative = infinite)
            // 4. Apply next blink pattern and schedule next timer
            
            if ((size_t)m_currentBlinkListIndex >= m_blinkList.size())
            {
                // Reached end of pattern list - reset to beginning
                blinkAgain = false;
                m_currentBlinkListIndex = 0;
                m_numberOfBlinks++;
                
                // Check if we should continue blinking
                // m_maxNumberOfBlinkRepeats < 0 means infinite loop
                if (m_maxNumberOfBlinkRepeats < 0 || m_numberOfBlinks <= m_maxNumberOfBlinkRepeats)
                {
                    blinkAgain = true;
                }
            }
            
            if (blinkAgain)
            {
                // Apply the current pattern from the blink list
                FrontPanelBlinkInfo blinkInfo = m_blinkList.at(m_currentBlinkListIndex);
                setBlinkLed(blinkInfo);
                
                // Schedule next timer event if still blinking
                if (m_isBlinking)
                    blinkTimer.Schedule(Core::Time::Now().Add(blinkInfo.durationInMs), m_blinkTimer);
            }

            // Spec requirement: LED color should stay on the LAST element in the array when blinking stops
        }

        uint64_t BlinkInfo::Timed(const uint64_t scheduledTime)
        {

            uint64_t result = 0;
            m_frontPanel->onBlinkTimer();
            return(result);
        }

    }
}

/** @} */
/** @} */
