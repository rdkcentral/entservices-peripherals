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
#include <interfaces/IFrontPanel.h>

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
        class FrontPanelImplementation : public Exchange::IFrontPanel {
        private:
            class PowerManagerNotification : public Exchange::IPowerManager::IModeChangedNotification {
            private:
                PowerManagerNotification(const PowerManagerNotification&) = delete;
                PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;
            
            public:
                explicit PowerManagerNotification(FrontPanelImplementation& parent)
                    : _parent(parent)
                {
                }
                ~PowerManagerNotification() override = default;
            
            public:
                void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override
                {
                    _parent.onPowerModeChanged(currentState, newState);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, PowerManagerNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(PowerManagerNotification)
                INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
                END_INTERFACE_MAP
            
            private:
                FrontPanelImplementation& _parent;
            };

            // We do not allow this plugin to be copied !!
            FrontPanelImplementation(const FrontPanelImplementation&) = delete;
            FrontPanelImplementation& operator=(const FrontPanelImplementation&) = delete;

            bool setClockBrightness(int brightness);
            int getClockBrightness();
            std::vector<string> getFrontPanelLights();
            JsonObject getFrontPanelLightsInfo();
            void setBlink(const JsonObject& blinkInfo);
            void set24HourClock(bool is24Hour);
            bool is24HourClock();
            void setClockTestPattern(bool show);

            void loadPreferences();
            void InitializePowerManager(PluginHost::IShell *service);


            //Begin methods
            Core::hresult SetBrightness(const string& index, const int32_t& brightness, FrontPanelSuccess& success);
            Core::hresult GetBrightness(const string& index, int32_t& brightness, bool& success);
            Core::hresult PowerLedOn(const string& index, FrontPanelSuccess& success);
            Core::hresult PowerLedOff(const string& index, FrontPanelSuccess& success);
            Core::hresult SetClockBrightness(const uint32_t& brightness, FrontPanelSuccess& success);
            Core::hresult GetClockBrightness(uint32_t& brightness, bool& success);
            Core::hresult GetFrontPanelLights(IFrontPanelLightsListIterator& supportedLights, string& supportedLightsInfo, bool& success);
            Core::hresult GetPreferences(string& preferences, bool& success);
            Core::hresult SetPreferences(const string& preferences, FrontPanelSuccess& success);
            Core::hresult SetLED(const string& ledIndiciator, const uint32_t& brightness, const uint32_t& red, const uint32_t& green, const uint32_t& blue, FrontPanelSuccess& success);
            Core::hresult SetBlink(const FrontPanelBlinkInfo& blinkInfo, FrontPanelSuccess& success);
            Core::hresult Set24HourClock(const bool& is24Hour, FrontPanelSuccess& success);
            Core::hresult Is24HourClock(bool& is24Hour, bool& success);
            Core::hresult SetClockTestPattern(const bool& show, const uint32_t& timeInterval, FrontPanelSuccess& success);
            Core::hresult Configure(PluginHost::IShell* service) override;
            Core::hresult Register() override;
            Core::hresult Unregister() override;
            //End methods

        public:
            FrontPanelImplementation();
            virtual ~FrontPanelImplementation();
            virtual const string Initialize(PluginHost::IShell* shell) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override { return {}; }
            void onPowerModeChanged(const PowerState currentState, const PowerState newState);
            void updateLedTextPattern();
            void registerEventHandlers();

            BEGIN_INTERFACE_MAP(FrontPanelImplementation)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            END_INTERFACE_MAP
        public:
            static FrontPanelImplementation* _instance;
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

        class TestPatternInfo
        {
        private:
            TestPatternInfo() = delete;
            TestPatternInfo& operator=(const TestPatternInfo& RHS) = delete;

        public:
            TestPatternInfo(FrontPanelImplementation* fp)
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
            FrontPanelImplementation* m_frontPanel;
        };
	} // namespace Plugin
} // namespace WPEFramework
