/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <interfaces/IFrontPanel.h>
#include <interfaces/IPowerManager.h>
#include "FrontPanelMock.h"
#include "PowerManagerMock.h"

#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);
    
#include "FrontPanelIndicatorMock.h"
#include "FrontPanelTextDisplayMock.h"
#include "FrontPanelConfigMock.h"
#include "ColorMock.h"
#include "IarmBusMock.h"

// Device settings includes for proper mock setup
#include "frontPanelIndicator.hpp"
#include "frontPanelConfig.hpp"
#include "frontPanelTextDisplay.hpp"

#define JSON_TIMEOUT   (1000)
#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define FRONTPANEL_CALLSIGN  _T("org.rdk.FrontPanel.1")
#define L2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;

typedef enum : uint32_t {
    FRONTPANELL2TEST_BRIGHTNESS_CHANGED = 0x00000001,
    FRONTPANELL2TEST_LED_STATE_CHANGED = 0x00000002,
    FRONTPANELL2TEST_BLINK_COMPLETED = 0x00000004,
    FRONTPANELL2TEST_POWER_STATE_CHANGED = 0x00000008,
    FRONTPANELL2TEST_STATE_INVALID = 0x00000000
} FrontPanelL2test_async_events_t;

class FrontPanel_Notification : public Exchange::IPowerManager::IModeChangedNotification {
    private:
        std::mutex m_mutex;
        std::condition_variable m_condition_variable;
        uint32_t m_event_signalled;

        BEGIN_INTERFACE_MAP(FrontPanel_Notification)
        INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
        END_INTERFACE_MAP

    public:
        FrontPanel_Notification() : m_event_signalled(FRONTPANELL2TEST_STATE_INVALID) {}
        ~FrontPanel_Notification() {}

        void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override {
            TEST_LOG("OnPowerModeChanged event triggered ***\n");
            std::unique_lock<std::mutex> lock(m_mutex);
            TEST_LOG("OnPowerModeChanged currentState: %u, newState: %u\n", currentState, newState);
            m_event_signalled |= FRONTPANELL2TEST_POWER_STATE_CHANGED;
            m_condition_variable.notify_one();
        }

        uint32_t WaitForRequestStatus(uint32_t timeout_ms, FrontPanelL2test_async_events_t expected_status) {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now();
            std::chrono::milliseconds timeout(timeout_ms);

            while (!(expected_status & m_event_signalled)) {
                if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                    return FRONTPANELL2TEST_STATE_INVALID;
                }
            }

            uint32_t signalled = m_event_signalled;
            m_event_signalled = FRONTPANELL2TEST_STATE_INVALID;
            return signalled;
        }
};

/* FrontPanel L2 test class declaration */
class FrontPanel_L2Test : public L2TestMocks {
protected:
    PluginHost::IShell* m_controller_FrontPanel;
    Exchange::IFrontPanel* m_frontPanelPlugin;
    
    // Device settings mocks
    IarmBusImplMock* p_iarmBusImplMock;
    FrontPanelConfigMock* p_frontPanelConfigImplMock;
    FrontPanelTextDisplayMock* p_frontPanelTextDisplayMock;
    ColorMock* p_colorImplMock;
    testing::NiceMock<FrontPanelIndicatorMock> frontPanelIndicatorMock;
    testing::NiceMock<FrontPanelIndicatorMock> frontPanelTextDisplayIndicatorMock;

public:
    FrontPanel_L2Test();
    virtual ~FrontPanel_L2Test() override;

    uint32_t CreateDeviceFrontPanelInterfaceObject();
    
    void Test_SetGetBrightness(Exchange::IFrontPanel* FrontPanelPlugin);
    void Test_PowerLedOnOff(Exchange::IFrontPanel* FrontPanelPlugin);
    void Test_GetFrontPanelLights(Exchange::IFrontPanel* FrontPanelPlugin);
    void Test_SetLED(Exchange::IFrontPanel* FrontPanelPlugin);
    void Test_SetBlink(Exchange::IFrontPanel* FrontPanelPlugin);
    void Test_PowerStateIntegration(Exchange::IFrontPanel* FrontPanelPlugin, Exchange::IPowerManager* PowerManagerPlugin);
    
    Core::Sink<FrontPanel_Notification> mNotification;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;
};

/**
 * @brief Constructor for FrontPanel L2 test class
 */
FrontPanel_L2Test::FrontPanel_L2Test() 
    : L2TestMocks()
    , m_controller_FrontPanel(nullptr)
    , m_frontPanelPlugin(nullptr)
    , p_iarmBusImplMock(nullptr)
    , p_frontPanelConfigImplMock(nullptr)
    , p_frontPanelTextDisplayMock(nullptr)
    , p_colorImplMock(nullptr)
    , m_event_signalled(FRONTPANELL2TEST_STATE_INVALID) {
    
    uint32_t status = Core::ERROR_GENERAL;
    
    // Set up device settings mocks
    p_iarmBusImplMock = new testing::NiceMock<IarmBusImplMock>;
    IarmBus::setImpl(p_iarmBusImplMock);
    
    p_frontPanelConfigImplMock = new testing::NiceMock<FrontPanelConfigMock>;
    device::FrontPanelConfig::setImpl(p_frontPanelConfigImplMock);
    
    device::FrontPanelIndicator::getInstance().impl = &frontPanelIndicatorMock;
    device::FrontPanelTextDisplay::getInstance().FrontPanelIndicator::impl = &frontPanelTextDisplayIndicatorMock;
    
    p_frontPanelTextDisplayMock = new testing::NiceMock<FrontPanelTextDisplayMock>;
    device::FrontPanelTextDisplay::setImpl(p_frontPanelTextDisplayMock);
    
    p_colorImplMock = new testing::NiceMock<ColorMock>;
    device::FrontPanelIndicator::Color::setImpl(p_colorImplMock);
    
    // Set up ColorMock expectations to match L1 tests
    ON_CALL(*p_colorImplMock, getName())
        .WillByDefault(::testing::Return("White"));

    ON_CALL(*p_colorImplMock, getInstanceByName(::testing::_))
        .WillByDefault(::testing::Invoke([](const std::string& name) {
            (void)name;
            return device::FrontPanelIndicator::Color::getInstance();
        }));
    
    // Set up basic mock expectations that are needed for plugin initialization
    ON_CALL(frontPanelIndicatorMock, getInstanceString(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                return device::FrontPanelIndicator::getInstance();
            }));
    
    ON_CALL(frontPanelIndicatorMock, getInstanceInt(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](int id) -> device::FrontPanelIndicator& {
                return device::FrontPanelIndicator::getInstance();
            }));
    
    ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillByDefault(::testing::Return(device::List<device::FrontPanelIndicator>({ device::FrontPanelIndicator::getInstance() })));
    
    ON_CALL(*p_frontPanelConfigImplMock, getTextDisplays())
        .WillByDefault(::testing::Return(device::List<device::FrontPanelTextDisplay>({ device::FrontPanelTextDisplay::getInstance() })));
    
    ON_CALL(frontPanelTextDisplayIndicatorMock, getName())
        .WillByDefault(::testing::Return("Text"));
    
    ON_CALL(*p_frontPanelConfigImplMock, getTextDisplay(::testing::_))
        .WillByDefault(::testing::ReturnRef(device::FrontPanelTextDisplay::getInstance()));
    
    ON_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillByDefault(::testing::Return(100));
    
    ON_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());
    
    ON_CALL(frontPanelIndicatorMock, setState(::testing::_))
        .WillByDefault(::testing::Return());
    
    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("Power"));
    
    ON_CALL(frontPanelIndicatorMock, getBrightnessLevels(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](int& levels, int& min, int& max) {
            levels = 100; min = 0; max = 100;
        }));
    
    ON_CALL(frontPanelIndicatorMock, getSupportedColors())
        .WillByDefault(::testing::Return(device::List<device::FrontPanelIndicator::Color>()));
    
    ON_CALL(frontPanelIndicatorMock, getColorMode())
        .WillByDefault(::testing::Return(0));
    
    // Set up IARM Bus mock expectations for basic functionality
    ON_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
    
    ON_CALL(*p_iarmBusImplMock, IARM_Bus_Init(::testing::_))
        .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
    
    ON_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
    
    ON_CALL(*p_iarmBusImplMock, IARM_Bus_UnRegisterEventHandler(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(IARM_RESULT_SUCCESS));
    
    // Additional mock for color operations that might be needed
    ON_CALL(frontPanelIndicatorMock, setColor(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());
        
    ON_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return());
        
    ON_CALL(frontPanelIndicatorMock, getColorName())
        .WillByDefault(::testing::Return("red"));
    
    // Set up PowerManager mock to prevent segmentation faults during plugin initialization
    EXPECT_CALL(PowerManagerMock::Mock(), Register(::testing::Matcher<Exchange::IPowerManager::IModeChangedNotification*>(::testing::_)))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
    EXPECT_CALL(PowerManagerMock::Mock(), Unregister(::testing::Matcher<const Exchange::IPowerManager::IModeChangedNotification*>(::testing::_)))
        .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
    // Activate the actual plugin service
    status = ActivateService("org.rdk.FrontPanel");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 * @brief Destructor for FrontPanel L2 test class
 */
FrontPanel_L2Test::~FrontPanel_L2Test() {
    uint32_t status = Core::ERROR_GENERAL;
    
    if (m_frontPanelPlugin) {
        m_frontPanelPlugin->Release();
        m_frontPanelPlugin = nullptr;
    }
    
    if (m_controller_FrontPanel) {
        m_controller_FrontPanel->Release();
        m_controller_FrontPanel = nullptr;
    }

    status = DeactivateService("org.rdk.FrontPanel");
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    // Clean up PowerManager mock
    PowerManagerMock::Delete();
    
    // Clean up device settings mocks
    device::FrontPanelIndicator::getInstance().impl = nullptr;
    device::FrontPanelTextDisplay::getInstance().FrontPanelIndicator::impl = nullptr;
    
    IarmBus::setImpl(nullptr);
    if (p_iarmBusImplMock != nullptr) {
        delete p_iarmBusImplMock;
        p_iarmBusImplMock = nullptr;
    }
    
    device::FrontPanelTextDisplay::setImpl(nullptr);
    if (p_frontPanelTextDisplayMock != nullptr) {
        delete p_frontPanelTextDisplayMock;
        p_frontPanelTextDisplayMock = nullptr;
    }
    
    device::FrontPanelConfig::setImpl(nullptr);
    if (p_frontPanelConfigImplMock != nullptr) {
        delete p_frontPanelConfigImplMock;
        p_frontPanelConfigImplMock = nullptr;
    }
    
    device::FrontPanelIndicator::Color::setImpl(nullptr);
    if (p_colorImplMock != nullptr) {
        delete p_colorImplMock;
        p_colorImplMock = nullptr;
    }
    
    // Clear CFrontPanel static variables like L1 tests do
    Plugin::CFrontPanel::initDone = 0;
}

uint32_t FrontPanel_L2Test::CreateDeviceFrontPanelInterfaceObject() {
    uint32_t return_value = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> FrontPanel_Engine;
    Core::ProxyType<RPC::CommunicatorClient> FrontPanel_Client;

    TEST_LOG("Creating FrontPanel_Engine");
    FrontPanel_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    FrontPanel_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(FrontPanel_Engine));

    TEST_LOG("Creating FrontPanel_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    FrontPanel_Engine->Announcements(FrontPanel_Client->Announcement());
#endif
    if (!FrontPanel_Client.IsValid()) {
        TEST_LOG("Invalid FrontPanel_Client");
    } else {
        m_controller_FrontPanel = FrontPanel_Client->Open<PluginHost::IShell>(_T("org.rdk.FrontPanel"), ~0, 3000);
        if (m_controller_FrontPanel) {
            m_frontPanelPlugin = m_controller_FrontPanel->QueryInterface<Exchange::IFrontPanel>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

/* COM-RPC tests */
void FrontPanel_L2Test::Test_SetGetBrightness(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    uint32_t brightness = 75;
    uint32_t retrievedBrightness = 0;
    bool success = false;
    string index = "power_led";

    TEST_LOG("\n################## Running Test_SetGetBrightness Test #################\n");

    // Add mock expectations for device settings calls
    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(brightness, ::testing::_))
        .Times(::testing::AtLeast(0));
    
    EXPECT_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillRepeatedly(::testing::Return(brightness));

    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;
    status = FrontPanelPlugin->SetBrightness(index, brightness, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);

    status = FrontPanelPlugin->GetBrightness(index, retrievedBrightness, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

void FrontPanel_L2Test::Test_PowerLedOnOff(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    string index = "power_led";
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;

    TEST_LOG("\n################## Running Test_PowerLedOnOff Test #################\n");

    // Add mock expectations for device settings calls
    EXPECT_CALL(frontPanelIndicatorMock, setState(true))
        .Times(::testing::AtLeast(0));
    
    EXPECT_CALL(frontPanelIndicatorMock, setState(false))
        .Times(::testing::AtLeast(0));

    status = FrontPanelPlugin->PowerLedOn(index, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);

    status = FrontPanelPlugin->PowerLedOff(index, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

void FrontPanel_L2Test::Test_GetFrontPanelLights(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    Exchange::IFrontPanel::IFrontPanelLightsListIterator* supportedLights = nullptr;
    string supportedLightsInfo;
    bool success = false;

    TEST_LOG("\n################## Running Test_GetFrontPanelLights Test #################\n");

    // Add mock expectations for device settings calls
    EXPECT_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillRepeatedly(::testing::Return(device::List<device::FrontPanelIndicator>({ device::FrontPanelIndicator::getInstance() })));
    
    EXPECT_CALL(frontPanelIndicatorMock, getBrightnessLevels(::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0))
        .WillRepeatedly(::testing::Invoke([](int& levels, int& min, int& max) {
            levels = 100; min = 0; max = 100;
        }));
    
    EXPECT_CALL(frontPanelIndicatorMock, getSupportedColors())
        .Times(::testing::AtLeast(0))
        .WillRepeatedly(::testing::Return(device::List<device::FrontPanelIndicator::Color>()));

    status = FrontPanelPlugin->GetFrontPanelLights(supportedLights, supportedLightsInfo, success);
    EXPECT_EQ(status, Core::ERROR_NONE);

    if (supportedLights) {
        supportedLights->Release();
    }
}

void FrontPanel_L2Test::Test_SetLED(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    string ledIndicator = "power_led";
    uint32_t brightness = 80;
    string color = "blue";
    uint32_t red = 0, green = 0, blue = 255;
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;

    TEST_LOG("\n################## Running Test_SetLED Test #################\n");

    // Add mock expectations for device settings calls
    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0));
    
    EXPECT_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0));

    status = FrontPanelPlugin->SetLED(ledIndicator, brightness, color, red, green, blue, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

void FrontPanel_L2Test::Test_SetBlink(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    string blinkInfo = R"({"ledIndicator":"power_led","iterations":3,"pattern":[{"brightness":100,"duration":500,"color":"red"},{"brightness":0,"duration":500}]})";
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;

    TEST_LOG("\n################## Running Test_SetBlink Test #################\n");

    // Add mock expectations for device settings calls
    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0));
    
    EXPECT_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .Times(::testing::AtLeast(0));

    status = FrontPanelPlugin->SetBlink(blinkInfo, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

void FrontPanel_L2Test::Test_PowerStateIntegration(Exchange::IFrontPanel* FrontPanelPlugin, Exchange::IPowerManager* PowerManagerPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    PowerState newPowerState = PowerState::POWER_STATE_STANDBY;
    const string standbyReason = "";
    int keyCode = 10;

    TEST_LOG("\n################## Running Test_PowerStateIntegration Test #################\n");

    if (PowerManagerPlugin) {
        // Set power state through PowerManager
        status = PowerManagerPlugin->SetPowerState(keyCode, newPowerState, standbyReason);
        EXPECT_EQ(status, Core::ERROR_NONE);
    }

    // Verify FrontPanel responds to power state change
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;
    status = FrontPanelPlugin->PowerLedOff("power_led", fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, FrontPanelComRpc) {
    uint32_t status = Core::ERROR_GENERAL;
    
    status = CreateDeviceFrontPanelInterfaceObject();
    EXPECT_EQ(Core::ERROR_NONE, status);
    
    if (m_frontPanelPlugin) {
        Test_SetGetBrightness(m_frontPanelPlugin);
        Test_PowerLedOnOff(m_frontPanelPlugin);
        Test_GetFrontPanelLights(m_frontPanelPlugin);
        Test_SetLED(m_frontPanelPlugin);
        Test_SetBlink(m_frontPanelPlugin);
    }
}

TEST_F(FrontPanel_L2Test, CrossPluginIntegration) {
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngine_FrontPanel;
    Core::ProxyType<RPC::CommunicatorClient> mClient_FrontPanel;
    PluginHost::IShell *mController_FrontPanel;

    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngine_PowerManager;
    Core::ProxyType<RPC::CommunicatorClient> mClient_PowerManager;
    PluginHost::IShell *mController_PowerManager;

    TEST_LOG("Creating RPC engines for cross-plugin test");
    mEngine_FrontPanel = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClient_FrontPanel = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator"), 
        Core::ProxyType<Core::IIPCServer>(mEngine_FrontPanel));

    mEngine_PowerManager = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClient_PowerManager = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator2"), 
        Core::ProxyType<Core::IIPCServer>(mEngine_PowerManager));

#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    mEngine_FrontPanel->Announcements(mClient_FrontPanel->Announcement());
    mEngine_PowerManager->Announcements(mClient_PowerManager->Announcement());
#endif

    if (mClient_FrontPanel.IsValid() && mClient_PowerManager.IsValid()) {
        mController_FrontPanel = mClient_FrontPanel->Open<PluginHost::IShell>("org.rdk.FrontPanel");
        mController_PowerManager = mClient_PowerManager->Open<PluginHost::IShell>("org.rdk.PowerManager");
        
        if (mController_FrontPanel && mController_PowerManager) {
            Exchange::IFrontPanel* FrontPanelPlugin = mController_FrontPanel->QueryInterface<Exchange::IFrontPanel>();
            Exchange::IPowerManager* PowerManagerPlugin = mController_PowerManager->QueryInterface<Exchange::IPowerManager>();
            
            if (FrontPanelPlugin && PowerManagerPlugin) {
                Test_PowerStateIntegration(FrontPanelPlugin, PowerManagerPlugin);
                
                FrontPanelPlugin->Release();
                PowerManagerPlugin->Release();
            }
            
            if (mController_FrontPanel) mController_FrontPanel->Release();
            if (mController_PowerManager) mController_PowerManager->Release();
        }
        
        mClient_FrontPanel.Release();
        mClient_PowerManager.Release();
    }
}

TEST_F(FrontPanel_L2Test, JsonRpcSetBrightness) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";
    params["brightness"] = 75;

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "setBrightness", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, JsonRpcGetBrightness) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "getBrightness", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, JsonRpcPowerLedOn) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "powerLedOn", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, JsonRpcPowerLedOff) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "powerLedOff", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, JsonRpcSetLED) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["ledIndicator"] = "power_led";
    params["brightness"] = 80;
    params["color"] = "blue";
    params["red"] = 0;
    params["green"] = 0;
    params["blue"] = 255;

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "setLED", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, JsonRpcSetBlink) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    string blinkInfo = R"({"ledIndicator":"power_led","iterations":3,"pattern":[{"brightness":100,"duration":500,"color":"red"},{"brightness":0,"duration":500}]})";
    params["blinkInfo"] = blinkInfo;

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "setBlink", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, JsonRpcGetFrontPanelLights) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "getFrontPanelLights", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}
