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

#include "FrontPanelImplementation.h"
#include "frontpanel.h"
#include <algorithm>

#include "frontPanelIndicator.hpp"
#include "frontPanelConfig.hpp"
#include "frontPanelTextDisplay.hpp"

#include "libIBus.h"

#include "UtilsJsonRpc.h"
#include "UtilsIarm.h"

#define SERVICE_NAME "FrontPanelService"
#define METHOD_FP_SET_BRIGHTNESS "setBrightness"
#define METHOD_FP_GET_BRIGHTNESS "getBrightness"
#define METHOD_FP_POWER_LED_ON "powerLedOn"
#define METHOD_FP_POWER_LED_OFF "powerLedOff"
#define METHOD_CLOCK_SET_BRIGHTNESS "setClockBrightness"
#define METHOD_CLOCK_GET_BRIGHTNESS "getClockBrightness"
#define METHOD_GET_FRONT_PANEL_LIGHTS "getFrontPanelLights"
#define METHOD_FP_GET_PREFERENCES "getPreferences"
#define METHOD_FP_SET_PREFERENCES "setPreferences"
#define METHOD_FP_SET_LED "setLED"
#define METHOD_FP_SET_BLINK "setBlink"
#define METHOD_FP_SET_24_HOUR_CLOCK "set24HourClock"
#define METHOD_FP_IS_24_HOUR_CLOCK "is24HourClock"
#define METHOD_FP_SET_CLOCKTESTPATTERN "setClockTestPattern"

#define DATA_LED "data_led"
#define RECORD_LED "record_led"
#define POWER_LED "power_led"
#ifdef CLOCK_BRIGHTNESS_ENABLED
#define CLOCK_LED "clock_led"
#define TEXT_LED "Text"
#endif

#ifdef USE_EXTENDED_ALL_SEGMENTS_TEXT_PATTERN
#define ALL_SEGMENTS_TEXT_PATTERN "88:88"
#else
#define ALL_SEGMENTS_TEXT_PATTERN "8888"
#endif

#define DEFAULT_TEXT_PATTERN_UPDATE_INTERVAL 5

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 6

using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;


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
        { "Text" , "clock_led"},
        // TODO: add your mappings here
        // { <IARM_NAME>, <SVC_MANAGER_API_NAME> },
        { 0,  0}
    };

    string svc2iarm(const string &name)
    {
        const char *s = name.c_str();

        int i = 0;
        while (name_mappings[i].SvcManagerName)
        {
            if (strcmp(s, name_mappings[i].SvcManagerName) == 0)
                return name_mappings[i].IArmBusName;
            i++;
        }
        return name;
    }

    string iarm2svc(const string &name)
    {
        const char *s = name.c_str();

        int i = 0;
        while (name_mappings[i].IArmBusName)
        {
            if (strcmp(s, name_mappings[i].IArmBusName) == 0)
                return name_mappings[i].SvcManagerName;
            i++;
        }
        return name;
    }

    JsonObject getFrontPanelIndicatorInfo(device::FrontPanelIndicator &indicator,JsonObject &indicatorInfo)
    {
        JsonObject returnResult;
        int levels=0, min=0, max=0;
        string range;

        indicator.getBrightnessLevels(levels, min, max);
        range = (levels <=2)?"boolean":"int";
        indicatorInfo["range"] = range;

        indicatorInfo["min"] = JsonValue(min);
        indicatorInfo["max"] = JsonValue(max);

        if (range == ("int"))
        {
            indicatorInfo["step"] = JsonValue((max-min)/levels);
        }
        JsonArray availableColors;
        const device::List <device::FrontPanelIndicator::Color> colorsList = indicator.getSupportedColors();
        for (uint j = 0; j < colorsList.size(); j++)
        {
            availableColors.Add(colorsList.at(j).getName());
        }
        if (availableColors.Length() > 0)
        {
            indicatorInfo["colors"] = availableColors;
        }

        indicatorInfo["colorMode"] = indicator.getColorMode();
        return indicatorInfo;
    }
}

namespace WPEFramework
{

    namespace Plugin
    {
        SERVICE_REGISTRATION(FrontPanelImplementation, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

        FrontPanelImplementation* FrontPanelImplementation::_instance = nullptr;

        static Core::TimerType<TestPatternInfo> patternUpdateTimer(64 * 1024, "PatternUpdateTimer");
        int FrontPanelImplementation::m_savedClockBrightness = -1;
        int FrontPanelImplementation::m_LedDisplayPatternUpdateTimerInterval = DEFAULT_TEXT_PATTERN_UPDATE_INTERVAL;

        FrontPanelImplementation::FrontPanelImplementation()
        : m_updateTimer(this)
        , m_runUpdateTimer(false)
        , _pwrMgrNotification(*this)
        , _registeredEventHandlers(false)
        {
            FrontPanelImplementation::_instance = this;
            m_runUpdateTimer = false;

        }

        FrontPanelImplementation::~FrontPanelImplementation()
        {
            if (_powerManagerPlugin) {
                _powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
                _powerManagerPlugin.Reset();
            }

            _registeredEventHandlers = false;
        }

        Core::hresult FrontPanelImplementation::Configure(PluginHost::IShell* service)
        {
            InitializePowerManager(service);
            FrontPanelImplementation::_instance = this;
            CFrontPanel::instance(service);
            CFrontPanel::instance()->start();
            CFrontPanel::instance()->addEventObserver(this);
            loadPreferences();

            return (string());
        }

        void FrontPanelImplementation::Deinitialize(PluginHost::IShell* /* service */)
        {
            FrontPanelImplementation::_instance = nullptr;

            {
                std::lock_guard<std::mutex> lock(m_updateTimerMutex);
                m_runUpdateTimer = false;
            }
            patternUpdateTimer.Revoke(m_updateTimer);
        }

        void FrontPanelImplementation::InitializePowerManager(PluginHost::IShell *service)
        {
            _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                                        .withIShell(service)
                                        .withRetryIntervalMS(200)
                                        .withRetryCount(25)
                                        .createInterface();
            registerEventHandlers();
        }

        void FrontPanelImplementation::onPowerModeChanged(const PowerState currentState, const PowerState newState)
        {
            if(newState == WPEFramework::Exchange::IPowerManager::POWER_STATE_ON)
            {
                LOGINFO("setPowerStatus true");
                CFrontPanel::instance()->setPowerStatus(true);
            }
            else
            {
                LOGINFO("setPowerStatus false");
                CFrontPanel::instance()->setPowerStatus(false);
            }
            return;
        }

        void FrontPanelImplementation::registerEventHandlers()
        {
            ASSERT (_powerManagerPlugin);

            if(!_registeredEventHandlers && _powerManagerPlugin) {
                _registeredEventHandlers = true;
                _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
            }
        }

        void setResponseArray(JsonObject& response, const char* key, const std::vector<std::string>& items)
        {
            JsonArray arr;
            for (auto& i : items) arr.Add(JsonValue(i));

            response[key] = arr;

            string json;
            response.ToString(json);
            LOGINFO("%s: result json %s\n", __FUNCTION__, json.c_str());
        }

        Core::hresult FrontPanelImplementation::SetBrightness(const string& index, const int32_t& brightness, FrontPanelSuccess& success);
        {
            CFrontPanel::instance()->stopBlinkTimer();
            bool ok = false;

            string fp_ind = svc2iarm(index);
            if (!fp_ind.empty())
                {
                
    #ifdef CLOCK_BRIGHTNESS_ENABLED
                if (TEXT_LED == fp_ind)
                {
                ok = setClockBrightness(int(brightness));
                }
                else
    #endif
                {
                    try
                    {
                        device::FrontPanelIndicator::getInstance(fp_ind.c_str()).setBrightness(int(brightness));
                        ok = true;
                    }
                    catch (...)
                    {
                        ok = false;
                    }
                }
            }
            else if (brightness >= 0 && brightness <= 100)
            {
                LOGWARN("calling setBrightness");
                ok = CFrontPanel::instance()->setBrightness(brightness);
                ok = setBrightness(brightness);
            }
            else
            {
                LOGWARN("Invalid brightnessLevel passed to method setBrightness CallMethod");
                ok = false;
            }

            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        /**
         * @brief Gets the brightness of the specified LED.
         *
         * @param[in] argList List of arguments (Not used).
         *
         * @return Returns a ServiceParams object containing brightness value and function result.
         * @ingroup SERVMGR_FRONTPANEL_API
         */
        Core::hresult FrontPanelImplementation::GetBrightness(const string& index, int32_t& brightness, bool& success)
        {
            bool ok = false;
            int value = -1;
            

            string fp_ind = svc2iarm(index);

            if (!fp_ind.empty())
            {
        #ifdef CLOCK_BRIGHTNESS_ENABLED
                if (TEXT_LED == fp_ind)
                {
                    value = getClockBrightness();
                }
                else
        #endif
                {
                    try
                    {
                        value = device::FrontPanelIndicator::getInstance(fp_ind.c_str()).getBrightness();
                    }
                    catch (...)
                    {
                        LOGWARN("Exception thrown from ds while calling getBrightness");
                    }
                }
            }
            else
            {
                value = CFrontPanel::instance()->getBrightness();
            }

            if (value >= 0)
            {
                brightness = value;
                ok = true;
            }
            else
            {
                brightness = -1;
                ok = false;
            }

            success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        Core::hresult FrontPanelImplementation::PowerLedOn(const string& index, FrontPanelSuccess& success)
        {
            bool ok = false;
            if (index == DATA_LED) {
                ok = CFrontPanel::instance()->powerOnLed(FRONT_PANEL_INDICATOR_MESSAGE);
            } else if (index == RECORD_LED) {
                ok = CFrontPanel::instance()->powerOnLed(FRONT_PANEL_INDICATOR_RECORD);
            } else if (index == POWER_LED) {
                ok = CFrontPanel::instance()->powerOnLed(FRONT_PANEL_INDICATOR_POWER); 
            }
            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }


        Core::hresult FrontPanelImplementation::PowerLedOff(const string& index, FrontPanelSuccess& success)
        {
            bool ok = false;
            if (index == DATA_LED) {
                ok = CFrontPanel::instance()->powerOffLed(FRONT_PANEL_INDICATOR_MESSAGE); 
            } else if (index == RECORD_LED) {
                ok = CFrontPanel::instance()->powerOffLed(FRONT_PANEL_INDICATOR_RECORD); 
            } else if (index == POWER_LED) {
                ok = CFrontPanel::instance()->powerOffLed(FRONT_PANEL_INDICATOR_POWER); 
            }
            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }


        bool FrontPanelImplementation::setClockBrightness(int brightness )
        {
#ifdef CLOCK_BRIGHTNESS_ENABLED
            bool ok;
            ok = CFrontPanel::instance()->setClockBrightness(brightness);
            return ok;
#else
            return false;
#endif
        }

        Core::hresult FrontPanelImplementation::SetClockBrightness(const uint32_t& brightness, FrontPanelSuccess& success)
        {
        #ifdef CLOCK_BRIGHTNESS_ENABLED
            bool ok = false;
            if (brightness <= 100) {
                ok = setClockBrightness(static_cast<int>(brightness));
            }
            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        #else
            success.success = false;
            return Core::ERROR_UNAVAILABLE;
        #endif
        }


        /**
         * @brief get the clock brightness of the specified LED. Brightness must be a value support by the
         * LED and the value of the brightness for this led must be persisted.
         *
         * @return The brightness integer value if clock brightness enable macro is true else it will return -1.
         */
        int FrontPanelImplementation::getClockBrightness()
        {
#ifdef CLOCK_BRIGHTNESS_ENABLED
            int brightness = -1;
            brightness = CFrontPanel::instance()->getClockBrightness();
            return brightness;
#else
            return -1;
#endif
        }

        Core::hresult FrontPanelImplementation::getClockBrightness(uint32_t& brightness, bool& success)
        {
        #ifdef CLOCK_BRIGHTNESS_ENABLED
            int value = GetClockBrightness();
            if (value >= 0) {
                brightness = static_cast<uint32_t>(value);
                success = true;
                return Core::ERROR_NONE;
            } else {
                brightness = 0;
                success = false;
                return Core::ERROR_GENERAL;
            }
        #else
            brightness = 0;
            success = false;
            LOGERR("Clock brightness is not enabled");
            return Core::ERROR_GENERAL;
        #endif
        }


        /**
         * @brief getFrontPanelLights This returns an object containing attributes of front panel
         * light: success, supportedLights, and supportedLightsInfo.
         * supportedLights defines the LED lights that can be controlled through the Front Panel API.
         * supportedLightsInfo defines a hash of objects describing each LED light.
         * success - false if the supported lights info was unable to be determined.
         *
         * @return Returns a list of front panel lights parameter.
         * @ingroup SERVMGR_FRONTPANEL_API
         */
        std::vector<std::string> FrontPanelImplementation::getFrontPanelLights()
        {
            std::vector<std::string> lights;
            device::List <device::FrontPanelIndicator> fpIndicators = device::FrontPanelConfig::getInstance().getIndicators();
            for (uint i = 0; i < fpIndicators.size(); i++)
            {
                string IndicatorNameIarm = fpIndicators.at(i).getName();
                string MappedName = iarm2svc(IndicatorNameIarm);
                if (MappedName != IndicatorNameIarm) lights.push_back(MappedName);
            }
#ifdef CLOCK_BRIGHTNESS_ENABLED
            try
            {
                device::List <device::FrontPanelTextDisplay> fpTextDisplays = device::FrontPanelConfig::getInstance().getTextDisplays();
                for (uint i = 0; i < fpTextDisplays.size(); i++)
                {
                    string TextDisplayNameIarm = fpTextDisplays.at(i).getName();
                    string MappedName = iarm2svc(TextDisplayNameIarm);
                    if (MappedName != TextDisplayNameIarm)
                    {
                        lights.push_back(MappedName);
                    }
                }
            }
            catch (...)
            {
                LOGERR("Exception while getFrontPanelLights");
            }
#endif
            return lights;
        }

        /**
         * @brief getFrontPanelLightsInfo This returns an object containing attributes of front
         * panel light: success, supportedLights, and supportedLightsInfo.
         * supportedLightsInfo defines a hash of objects describing each LED light properties such as
         * -"range" Determines the types of values that can be expected in min and max value.
         * -"min" The minimum value is equivalent to off i.e "0".
         * -"max" The maximum value is when the LED is on i.e "1" and at its brightest.
         * -"step" The step or interval between the min and max values supported by the LED.
         * -"colorMode" Defines enum of "0" LED's color cannot be changed, "1"  LED can be set to any color
         * (using rgb-hex code),"2"  LED can be set to an enumeration of colors as specified by the
         * supportedColors property.
         *
         * @return Returns a serviceParams list of front panel lights info.
         */

        
        JsonObject FrontPanelImplementation::getFrontPanelLightsInfo()
        {
            JsonObject returnResult;
            JsonObject indicatorInfo;
            string IndicatorNameIarm, MappedName;

            device::List <device::FrontPanelIndicator> fpIndicators = device::FrontPanelConfig::getInstance().getIndicators();
            for (uint i = 0; i < fpIndicators.size(); i++)
            {
                IndicatorNameIarm = fpIndicators.at(i).getName();
                MappedName = iarm2svc(IndicatorNameIarm);
                getFrontPanelIndicatorInfo(fpIndicators.at(i),indicatorInfo);
                if (MappedName != IndicatorNameIarm)
                {
                    returnResult[MappedName.c_str()] = indicatorInfo;
                }
                else
                {
                    returnResult[IndicatorNameIarm.c_str()] = indicatorInfo;
                }		    
            }

#ifdef CLOCK_BRIGHTNESS_ENABLED
            try
            {
                getFrontPanelIndicatorInfo(device::FrontPanelConfig::getInstance().getTextDisplay(0),indicatorInfo);
                returnResult[CLOCK_LED] = indicatorInfo;
            }
            catch (...)
            {
                LOGERR("Exception while getFrontPanelLightsInfo");
            }
#endif
            return returnResult;
        }

        Core::hresult FrontPanelImplementation::GetFrontPanelLights(IFrontPanelLightsListIterator& supportedLights, string& supportedLightsInfo, bool& success)
        {
            std::vector<std::string> lights = getFrontPanelLights();
            for (const auto& light : lights) {
                supportedLights.Next(light);
            }
            JsonObject info = getFrontPanelLightsInfo();
            string infoStr;
            info.ToString(infoStr);
            supportedLightsInfo = infoStr;
            success = true;
            return Core::ERROR_NONE;
        }

        void FrontPanelImplementation::loadPreferences()
        {
            CFrontPanel::instance()->loadPreferences();
        }

        Core::hresult FrontPanelImplementation::GetPreferences(string& preferences, bool& success)
        {
            JsonObject prefs = CFrontPanel::instance()->getPreferences();;
            string prefsStr;
            prefs.ToString(prefsStr);
            preferences = prefsStr;
            success = true;
            return Core::ERROR_NONE;
        }

        /**
         * @brief This method stores the preferences into persistent storage.
         * It does not change the color of any LEDs. The preferences object is not validated.
         * It is up to the client of this API to ensure that preference values are valid.
         *
         * @param[in] argList List of preferences.
         * @return Returns the success code of underlying method.
         * @ingroup SERVMGR_FRONTPANEL_API
         */

        Core::hresult FrontPanelImplementation::SetPreferences(const string& preferences, FrontPanelSuccess& success)
        {
            bool ok = false;
            try {
                JsonObject prefs;
                if (prefs.FromString(preferences)) {
                    CFrontPanel::instance()->setPreferences(preferences);
                    ok = true;
                }
            } catch (...) {
                ok = false;
            }
            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        /**
         * @brief Sets the brightness and color properties of the specified LED.
         * The supported properties of the info object passed in will be determined by the color
         * mode of the LED. If the colorMode of an LED is 0 color values will be ignored. If the
         * brightness of the LED is unspecified or value = -1, then the persisted or default
         * value for the system is used.
         *
         * @param[in] properties Key value pair of properties data.
         *
         * @return Returns success value of the helper method, returns false in case of failure.
         */

        Core::hresult FrontPanelImplementation::SetLED(const string& ledIndicator, const uint32_t& brightness, const uint32_t& red, const uint32_t& green, const uint32_t& blue, FrontPanelSuccess& success)
        {
            JsonObject properties;
            properties["name"] = ledIndicator;
            properties["brightness"] = brightness;
            properties["red"] = red;
            properties["green"] = green;
            properties["blue"] = blue;

            bool ok = setLED(properties);
            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        /**
         * @brief Specifies a blinking pattern for an LED. This method returns immediately, but starts
         * a process of iterating through each element in the array and lighting the LED with the specified
         * brightness and color (if applicable) for the given duration (in milliseconds).
         *
         * @param[in] blinkInfo Object containing Indicator name, blink pattern and duration.
         * @ingroup SERVMGR_FRONTPANEL_API
         */
        void FrontPanelImplementation::setBlink(const JsonObject& blinkInfo)
        {
            CFrontPanel::instance()->setBlink(blinkInfo);
        }

        /**
         * @brief Specifies the 24 hour clock format.
         *
         * @param[in] is24Hour true if 24 hour clock format.
         * @ingroup SERVMGR_FRONTPANEL_API
         */
        void FrontPanelImplementation::set24HourClock(bool is24Hour)
        {
            CFrontPanel::instance()->set24HourClock(is24Hour);
        }

        /**
         * @brief Get the 24 hour clock format.
         *
         * @return true if 24 hour clock format is used.
         * @ingroup SERVMGR_FRONTPANEL_API
         */
        bool FrontPanelImplementation::is24HourClock()
        {
            bool is24Hour = false;
            is24Hour = CFrontPanel::instance()->is24HourClock();
            return is24Hour;
        }


        /**
         * @brief Enable or disable showing test pattern 88:88 on stbs with clock displays.
         *
         * @param[in] show true to show pattern, false to restore display to default behavior, usual it's clock.
         * @param[in] interval (optional) interval in seconds to check and update LED display with pattern, when it's overridden by external API.
         *            from 1 to 60 seconds. 0 and other outbound values mean that timer isn't used and isn't activated by this call.
         *            Optionally the timer is enabled for 5 seconds interval.
         * @return true if method succeeded.
         * @ingroup SERVMGR_FRONTPANEL_API
         */
        void FrontPanelImplementation::setClockTestPattern(bool show)
        {
        #ifdef CLOCK_BRIGHTNESS_ENABLED
            try{
                device::FrontPanelTextDisplay& display = device::FrontPanelConfig::getInstance().getTextDisplay("Text");

                if (show)
                {
                    if (m_LedDisplayPatternUpdateTimerInterval > 0 && m_LedDisplayPatternUpdateTimerInterval < 61)
                    {
                        {
                            std::lock_guard<std::mutex> lock(m_updateTimerMutex);
                            m_runUpdateTimer = true;
                        }
                        patternUpdateTimer.Schedule(Core::Time::Now().Add(m_LedDisplayPatternUpdateTimerInterval * 1000), m_updateTimer);

                        LOGWARN("%s: LED FP display update timer activated with interval %ds", __FUNCTION__, m_LedDisplayPatternUpdateTimerInterval);
                    }
                    else
                    {
                        LOGWARN("%s: LED FP display update timer didn't used for interval value %d. To activate it, interval should be in bound of values from 1 till 60"
                                , __FUNCTION__, m_LedDisplayPatternUpdateTimerInterval);

                        {
                            std::lock_guard<std::mutex> lock(m_updateTimerMutex);
                            m_runUpdateTimer = false;
                        }
                        patternUpdateTimer.Revoke(m_updateTimer);
                    }

                    if (-1 == m_savedClockBrightness)
                    {
                        m_savedClockBrightness = getClockBrightness();
                        LOGWARN("%s: brightness of LED FP display %d was saved", __FUNCTION__, m_savedClockBrightness);
                    }

                    display.setMode(1); //Set Front Panel Display to Text Mode
                    display.setText(ALL_SEGMENTS_TEXT_PATTERN);
                    setClockBrightness(100);
                    LOGWARN("%s: pattern " ALL_SEGMENTS_TEXT_PATTERN " activated on LED FP display with max brightness", __FUNCTION__);
                }
                else
                {
                    {
                        std::lock_guard<std::mutex> lock(m_updateTimerMutex);
                        m_runUpdateTimer = false;
                    }
                    patternUpdateTimer.Revoke(m_updateTimer);

                    display.setMode(0);//Set Front Panel Display to Default Mode
                    display.setText("    ");
                    LOGWARN("%s: pattern " ALL_SEGMENTS_TEXT_PATTERN " deactivated on LED FP display", __FUNCTION__);

                    if (-1 != m_savedClockBrightness)
                    {
                        setClockBrightness(m_savedClockBrightness);
                        LOGWARN("%s: brightness %d of LED FP display restored", __FUNCTION__, m_savedClockBrightness);
                        m_savedClockBrightness = -1;
                    }
                }
            }
            catch (...)
            {
                LOGERR("Exception while getTextDisplay");
            }
        #else
            LOGWARN("%s: disabled for this platform", __FUNCTION__);
        #endif
        }
        Core::hresult FrontPanelImplementation::SetBlink(const FrontPanelBlinkInfo& blinkInfo, FrontPanelSuccess& success)
        {
            bool ok = false;
            try {
                JsonObject blinkObj;
                blinkObj["ledIndicator"] = blinkInfo.ledIndiciator;
                blinkObj["iterations"] = blinkInfo.iterations;

                JsonArray patternArr;
                for (const auto& pat : blinkInfo.pattern) {
                    JsonObject patObj;
                    patObj["brightness"] = pat.brightness;
                    patObj["duration"] = pat.duration;
                    patObj["color"] = pat.color;
                    patObj["red"] = pat.red;
                    patObj["green"] = pat.green;
                    patObj["blue"] = pat.blue;
                    patternArr.Add(patObj);
                }
                blinkObj["pattern"] = patternArr;

                setBlink(blinkObj);
                ok = true;
            } catch (...) {
                ok = false;
            }
            success.success = ok;
            return ok ? Core::ERROR_NONE : Core::ERROR_GENERAL;
        }

        Core::hresult FrontPanelImplementation::Set24HourClock(const bool& is24Hour, FrontPanelSuccess& success)
        {
            try{
                set24HourClock(is24Hour);
                success.success = true;
                return Core::ERROR_NONE;
            }
            catch (...)
            {
                LOGERR("Exception while set24HourClock");
                success.success = false;
                return Core::ERROR_GENERAL;
            }
            
        }

        Core::hresult FrontPanelImplementation::Is24HourClock(bool& is24Hour, bool& success)
        {
            try {
                is24Hour = is24HourClock();
                success = true;
                return Core::ERROR_NONE;
            }
            catch (...) {
                LOGERR("Exception while checking is24HourClock");
                success = false;
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult FrontPanelImplementation::SetClockTestPattern(const bool& show, const uint32_t& timeInterval, FrontPanelSuccess& success)
        {
        #ifdef CLOCK_BRIGHTNESS_ENABLED
            try {
                if (timeInterval > 0 && timeInterval < 61) {
                    m_LedDisplayPatternUpdateTimerInterval = static_cast<int>(timeInterval);
                } else {
                    m_LedDisplayPatternUpdateTimerInterval = DEFAULT_TEXT_PATTERN_UPDATE_INTERVAL;
                }
                setClockTestPattern(show);
                success.success = true;
                return Core::ERROR_NONE;
            } catch (...) {
                LOGERR("Exception while SetClockTestPattern");
                success.success = false;
                return Core::ERROR_GENERAL;
            }
        #else
            LOGWARN("%s: disabled for this platform", __FUNCTION__);
            success.success = false;
            return Core::ERROR_UNAVAILABLE;
        #endif
        }

        void FrontPanelImplementation::updateLedTextPattern()
        {
            LOGWARN("%s: override FP LED display with text pattern " ALL_SEGMENTS_TEXT_PATTERN, __FUNCTION__);

            if (getClockBrightness() != 100)
            {
                setClockBrightness(100);
            }

            device::FrontPanelConfig::getInstance().getTextDisplay("Text").setText(ALL_SEGMENTS_TEXT_PATTERN);
            LOGWARN("%s: LED display updated by pattern " ALL_SEGMENTS_TEXT_PATTERN, __FUNCTION__);

            {
                std::lock_guard<std::mutex> lock(m_updateTimerMutex);
                if (m_runUpdateTimer)
                    patternUpdateTimer.Schedule(Core::Time::Now().Add(m_LedDisplayPatternUpdateTimerInterval * 1000), m_updateTimer);
            }
        }

        uint64_t TestPatternInfo::Timed(const uint64_t scheduledTime)
        {
            uint64_t result = 0;
            m_frontPanel->updateLedTextPattern();
            return(result);
        }


    } // namespace Plugin
} // namespace WPEFramework
