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

#pragma once

#include <mutex>
#include "Module.h"
#include "libIARM.h"
#include "frontpanel.h"
#include <interfaces/IPowerManager.h>
#include "PowerManagerInterface.h"

using namespace WPEFramework;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

#define DATA_LED  "data_led"
#define RECORD_LED "record_led"
#ifdef CLOCK_BRIGHTNESS_ENABLED
#define CLOCK_LED "clock_led"
#define TEXT_LED "Text"
#endif

namespace WPEFramework {

    namespace Plugin {

        class TestPatternInfo
        {
        private:
            TestPatternInfo() = delete;
            TestPatternInfo& operator=(const TestPatternInfo& RHS) = delete;

        public:
            TestPatternInfo(FrontPanel* fp)
            : m_frontPanel(fp)
            {
            }
            TestPatternInfo(const TestPatternInfo& copy)
            : m_frontPanel(copy.m_frontPanel)
            {
            }
            ~TestPatternInfo() {}

            inline bool operator==(const TestPatternInfo& RHS) const
            {
                return(m_frontPanel == RHS.m_frontPanel);
            }

        public:
            uint64_t Timed(const uint64_t scheduledTime);

        private:
            FrontPanel* m_frontPanel;
        };


		// This is a server for a JSONRPC communication channel.
		// For a plugin to be capable to handle JSONRPC, inherit from PluginHost::JSONRPC.
		// By inheriting from this class, the plugin realizes the interface PluginHost::IDispatcher.
		// This realization of this interface implements, by default, the following methods on this plugin
		// - exists
		// - register
		// - unregister
		// Any other methood to be handled by this plugin  can be added can be added by using the
		// templated methods Register on the PluginHost::JSONRPC class.
		// As the registration/unregistration of notifications is realized by the class PluginHost::JSONRPC,
		// this class exposes a public method called, Notify(), using this methods, all subscribed clients
		// will receive a JSONRPC message as a notification, in case this method is called.
        class FrontPanel : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            class PowerManagerNotification : public Exchange::IPowerManager::INotification {
            private:
                PowerManagerNotification(const PowerManagerNotification&) = delete;
                PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;
            
            public:
                explicit PowerManagerNotification(FrontPanel& parent)
                    : _parent(parent)
                {
                }
                ~PowerManagerNotification() override = default;
            
            public:
                void OnPowerModeChanged(const PowerState &currentState, const PowerState &newState) override
                {
                    _parent.onPowerModeChanged(currentState, newState);
                }
                void OnPowerModePreChange(const PowerState &currentState, const PowerState &newState) override {}
                void OnDeepSleepTimeout(const int &wakeupTimeout) override {}
                void OnNetworkStandbyModeChanged(const bool &enabled) override {}
                void OnThermalModeChanged(const ThermalTemperature &currentThermalLevel, const ThermalTemperature &newThermalLevel, const float &currentTemperature) override {}
                void OnRebootBegin(const string &rebootReasonCustom, const string &rebootReasonOther, const string &rebootRequestor) override {}

                BEGIN_INTERFACE_MAP(PowerManagerNotification)
                INTERFACE_ENTRY(Exchange::IPowerManager::INotification)
                END_INTERFACE_MAP
            
            private:
                FrontPanel& _parent;
            };

            // We do not allow this plugin to be copied !!
            FrontPanel(const FrontPanel&) = delete;
            FrontPanel& operator=(const FrontPanel&) = delete;

            bool setBrightness(int brightness);
            int getBrightness();
            bool powerLedOn(frontPanelIndicator fp_indicator);
            bool powerLedOff(frontPanelIndicator fp_indicator);
            bool setClockBrightness(int brightness);
            int getClockBrightness();
            std::vector<string> getFrontPanelLights();
            JsonObject getFrontPanelLightsInfo();
            JsonObject getPreferences();
            void setPreferences(const JsonObject& preferences);
            bool setLED(const JsonObject& properties);
            void setBlink(const JsonObject& blinkInfo);
            void set24HourClock(bool is24Hour);
            bool is24HourClock();
            void setClockTestPattern(bool show);

            void loadPreferences();
            void InitializePowerManager(PluginHost::IShell *service);

            //Begin methods
            uint32_t setBrightnessWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t getBrightnessWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t powerLedOnWrapper(const JsonObject& parameters, JsonObject& response );
            uint32_t powerLedOffWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t setClockBrightnessWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t getClockBrightnessWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t getFrontPanelLightsWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t getPreferencesWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t setPreferencesWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t setLEDWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t setBlinkWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t set24HourClockWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t is24HourClockWrapper(const JsonObject& parameters, JsonObject& response);
            uint32_t setClockTestPatternWrapper(const JsonObject& parameters, JsonObject& response);
            //End methods

        public:
            FrontPanel();
            virtual ~FrontPanel();
            virtual const string Initialize(PluginHost::IShell* shell) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override { return {}; }
            void onPowerModeChanged(const PowerState &currentState, const PowerState &newState);
            void updateLedTextPattern();
            void registerEventHandlers();

            BEGIN_INTERFACE_MAP(FrontPanel)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP
        public:
            static FrontPanel* _instance;
        private:
            static int m_savedClockBrightness;
            static int m_LedDisplayPatternUpdateTimerInterval;

            TestPatternInfo m_updateTimer;
            bool           m_runUpdateTimer;
            std::mutex      m_updateTimerMutex;
            PowerManagerInterfaceRef _powerManagerPlugin;
            Core::Sink<PowerManagerNotification> _pwrMgrNotification;
            bool _registeredEventHandlers;

        };
	} // namespace Plugin
} // namespace WPEFramework
