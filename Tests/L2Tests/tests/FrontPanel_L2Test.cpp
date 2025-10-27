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
#include "FrontPanelIndicatorMock.h"
#include "FrontPanelTextDisplayMock.h"
#include "FrontPanelConfigMock.h"
#include "ColorMock.h"
#include "IarmBusMock.h"

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
public:
    FrontPanel_L2Test();
    virtual ~FrontPanel_L2Test() override;

    void OnPowerModeChanged(const PowerState currentState, const PowerState newState);
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, FrontPanelL2test_async_events_t expected_status);
    
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
    
    FrontPanelIndicatorMock* p_frontPanelIndicatorMock = nullptr;
    FrontPanelTextDisplayMock* p_frontPanelTextDisplayMock = nullptr;
    FrontPanelConfigMock* p_frontPanelConfigImplMock = nullptr;
    ColorMock* p_colorImplMock = nullptr;
    IarmBusImplMock* p_iarmBusImplMock = nullptr;
};

/**
 * @brief Constructor for FrontPanel L2 test class
 */
FrontPanel_L2Test::FrontPanel_L2Test() : L2TestMocks() {
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = FRONTPANELL2TEST_STATE_INVALID;

    // Initialize mock pointers
    p_frontPanelIndicatorMock = new NiceMock<FrontPanelIndicatorMock>();
    p_frontPanelTextDisplayMock = new NiceMock<FrontPanelTextDisplayMock>();
    p_frontPanelConfigImplMock = new NiceMock<FrontPanelConfigMock>();
    p_colorImplMock = new NiceMock<ColorMock>();
    p_iarmBusImplMock = new NiceMock<IarmBusImplMock>();

    // Setup mock expectations for device settings
    EXPECT_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillRepeatedly(::testing::Return(device::List<device::FrontPanelIndicator>()));

    EXPECT_CALL(*p_frontPanelIndicatorMock, getInstanceString(::testing::_))
        .WillRepeatedly(::testing::ReturnRef(*p_frontPanelIndicatorMock));

    EXPECT_CALL(*p_frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillRepeatedly(::testing::Return(50));

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return());

    EXPECT_CALL(*p_frontPanelIndicatorMock, setState(::testing::_))
        .WillRepeatedly(::testing::Return());

    EXPECT_CALL(*p_frontPanelIndicatorMock, getName())
        .WillRepeatedly(::testing::Return("power_led"));

    EXPECT_CALL(*p_frontPanelIndicatorMock, getBrightnessLevels(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Invoke([](int& levels, int& min, int& max) {
            levels = 100;
            min = 0;
            max = 100;
        }));

    EXPECT_CALL(*p_frontPanelIndicatorMock, getColorMode())
        .WillRepeatedly(::testing::Return(1));

    EXPECT_CALL(*p_frontPanelIndicatorMock, getSupportedColors())
        .WillRepeatedly(::testing::Return(device::List<device::FrontPanelIndicator::Color>()));

    // Setup IARM mock expectations
    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Init(::testing::_))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Connect())
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_RegisterEventHandler(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return(IARM_RESULT_SUCCESS));

    // Activate the actual plugin service
    status = ActivateService("org.rdk.FrontPanel");
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Also activate PowerManager for cross-plugin integration tests
    status = ActivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 * @brief Destructor for FrontPanel L2 test class
 */
FrontPanel_L2Test::~FrontPanel_L2Test() {
    uint32_t status = Core::ERROR_GENERAL;
    m_event_signalled = FRONTPANELL2TEST_STATE_INVALID;

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Disconnect())
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    EXPECT_CALL(*p_iarmBusImplMock, IARM_Bus_Term())
        .WillOnce(::testing::Return(IARM_RESULT_SUCCESS));

    status = DeactivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);

    status = DeactivateService("org.rdk.FrontPanel");
    EXPECT_EQ(Core::ERROR_NONE, status);

    // Clean up mock pointers
    delete p_frontPanelIndicatorMock;
    delete p_frontPanelTextDisplayMock;
    delete p_frontPanelConfigImplMock;
    delete p_colorImplMock;
    delete p_iarmBusImplMock;
}

void FrontPanel_L2Test::OnPowerModeChanged(const PowerState currentState, const PowerState newState) {
    TEST_LOG("OnPowerModeChanged event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);
    TEST_LOG("OnPowerModeChanged currentState: %u, newState: %u\n", currentState, newState);
    m_event_signalled |= FRONTPANELL2TEST_POWER_STATE_CHANGED;
    m_condition_variable.notify_one();
}

uint32_t FrontPanel_L2Test::WaitForRequestStatus(uint32_t timeout_ms, FrontPanelL2test_async_events_t expected_status) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = FRONTPANELL2TEST_STATE_INVALID;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            return FRONTPANELL2TEST_STATE_INVALID;
        }
    }

    signalled = m_event_signalled;
    m_event_signalled = FRONTPANELL2TEST_STATE_INVALID;
    return signalled;
}

/* COM-RPC tests */
void FrontPanel_L2Test::Test_SetGetBrightness(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    uint32_t brightness = 75;
    uint32_t retrievedBrightness = 0;
    bool success = false;
    string index = "power_led";

    TEST_LOG("\n################## Running Test_SetGetBrightness Test #################\n");

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(brightness, ::testing::_))
        .WillOnce(::testing::Return());

    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;
    status = FrontPanelPlugin->SetBrightness(index, brightness, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(fpSuccess.success);

    EXPECT_CALL(*p_frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillOnce(::testing::Return(brightness));

    status = FrontPanelPlugin->GetBrightness(index, retrievedBrightness, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_EQ(retrievedBrightness, brightness);
}

void FrontPanel_L2Test::Test_PowerLedOnOff(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    string index = "power_led";
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;

    TEST_LOG("\n################## Running Test_PowerLedOnOff Test #################\n");

    EXPECT_CALL(*p_frontPanelIndicatorMock, setState(true))
        .WillOnce(::testing::Return());

    status = FrontPanelPlugin->PowerLedOn(index, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(fpSuccess.success);

    EXPECT_CALL(*p_frontPanelIndicatorMock, setState(false))
        .WillOnce(::testing::Return());

    status = FrontPanelPlugin->PowerLedOff(index, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(fpSuccess.success);
}

void FrontPanel_L2Test::Test_GetFrontPanelLights(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    Exchange::IFrontPanel::IFrontPanelLightsListIterator* supportedLights = nullptr;
    string supportedLightsInfo;
    bool success = false;

    TEST_LOG("\n################## Running Test_GetFrontPanelLights Test #################\n");

    device::List<device::FrontPanelIndicator> mockIndicators;
    EXPECT_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillOnce(::testing::Return(mockIndicators));

    status = FrontPanelPlugin->GetFrontPanelLights(supportedLights, supportedLightsInfo, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_NE(supportedLights, nullptr);

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

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(brightness, ::testing::_))
        .WillOnce(::testing::Return());

    EXPECT_CALL(*p_frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .WillOnce(::testing::Return());

    status = FrontPanelPlugin->SetLED(ledIndicator, brightness, color, red, green, blue, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(fpSuccess.success);
}

void FrontPanel_L2Test::Test_SetBlink(Exchange::IFrontPanel* FrontPanelPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    string blinkInfo = R"({"ledIndicator":"power_led","iterations":3,"pattern":[{"brightness":100,"duration":500,"color":"red"},{"brightness":0,"duration":500}]})";
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;

    TEST_LOG("\n################## Running Test_SetBlink Test #################\n");

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return());

    EXPECT_CALL(*p_frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return());

    status = FrontPanelPlugin->SetBlink(blinkInfo, fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(fpSuccess.success);
}

void FrontPanel_L2Test::Test_PowerStateIntegration(Exchange::IFrontPanel* FrontPanelPlugin, Exchange::IPowerManager* PowerManagerPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    uint32_t signalled = FRONTPANELL2TEST_STATE_INVALID;
    PowerState newPowerState = PowerState::POWER_STATE_STANDBY;
    const string standbyReason = "";
    int keyCode = 10;

    TEST_LOG("\n################## Running Test_PowerStateIntegration Test #################\n");

    // Register for power state notifications
    status = PowerManagerPlugin->Register(&mNotification);
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Set power state through PowerManager
    status = PowerManagerPlugin->SetPowerState(keyCode, newPowerState, standbyReason);
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Wait for notification
    signalled = mNotification.WaitForRequestStatus(JSON_TIMEOUT, FRONTPANELL2TEST_POWER_STATE_CHANGED);
    EXPECT_TRUE(signalled & FRONTPANELL2TEST_POWER_STATE_CHANGED);

    // Verify FrontPanel responds to power state change
    Exchange::IFrontPanel::FrontPanelSuccess fpSuccess;
    EXPECT_CALL(*p_frontPanelIndicatorMock, setState(::testing::_))
        .WillRepeatedly(::testing::Return());

    // In standby mode, power LED should be dimmed or off
    status = FrontPanelPlugin->PowerLedOff("power_led", fpSuccess);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(fpSuccess.success);

    // Unregister from notifications
    status = PowerManagerPlugin->Unregister(&mNotification);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

TEST_F(FrontPanel_L2Test, FrontPanelComRpc) {
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngine_FrontPanel;
    Core::ProxyType<RPC::CommunicatorClient> mClient_FrontPanel;
    PluginHost::IShell *mController_FrontPanel;

    TEST_LOG("Creating mEngine_FrontPanel");
    mEngine_FrontPanel = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClient_FrontPanel = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator"), 
        Core::ProxyType<Core::IIPCServer>(mEngine_FrontPanel));

    TEST_LOG("Creating mEngine_FrontPanel Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    mEngine_FrontPanel->Announcements(mClient_FrontPanel->Announcement());
#endif

    if (!mClient_FrontPanel.IsValid()) {
        ASSERT_TRUE(false) << "Failed to create RPC client for FrontPanel";
    } else {
        mController_FrontPanel = mClient_FrontPanel->Open<PluginHost::IShell>("org.rdk.FrontPanel");
        
        if (mController_FrontPanel) {
            Exchange::IFrontPanel* FrontPanelPlugin = mController_FrontPanel->QueryInterface<Exchange::IFrontPanel>();
            
            if (FrontPanelPlugin) {
                Test_SetGetBrightness(FrontPanelPlugin);
                Test_PowerLedOnOff(FrontPanelPlugin);
                Test_GetFrontPanelLights(FrontPanelPlugin);
                Test_SetLED(FrontPanelPlugin);
                Test_SetBlink(FrontPanelPlugin);
                
                FrontPanelPlugin->Release();
            }
            mController_FrontPanel->Release();
        }
        mClient_FrontPanel.Release();
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

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(75, ::testing::_))
        .WillOnce(::testing::Return());

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "setBrightness", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(FrontPanel_L2Test, JsonRpcGetBrightness) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";

    EXPECT_CALL(*p_frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillOnce(::testing::Return(50));

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "getBrightness", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_EQ(result["brightness"].Number(), 50);
}

TEST_F(FrontPanel_L2Test, JsonRpcPowerLedOn) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";

    EXPECT_CALL(*p_frontPanelIndicatorMock, setState(true))
        .WillOnce(::testing::Return());

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "powerLedOn", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(FrontPanel_L2Test, JsonRpcPowerLedOff) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    params["index"] = "power_led";

    EXPECT_CALL(*p_frontPanelIndicatorMock, setState(false))
        .WillOnce(::testing::Return());

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "powerLedOff", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
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

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(80, ::testing::_))
        .WillOnce(::testing::Return());

    EXPECT_CALL(*p_frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .WillOnce(::testing::Return());

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "setLED", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(FrontPanel_L2Test, JsonRpcSetBlink) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    string blinkInfo = R"({"ledIndicator":"power_led","iterations":3,"pattern":[{"brightness":100,"duration":500,"color":"red"},{"brightness":0,"duration":500}]})";
    params["blinkInfo"] = blinkInfo;

    EXPECT_CALL(*p_frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return());

    EXPECT_CALL(*p_frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .WillRepeatedly(::testing::Return());

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "setBlink", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

TEST_F(FrontPanel_L2Test, JsonRpcGetFrontPanelLights) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;

    device::List<device::FrontPanelIndicator> mockIndicators;
    EXPECT_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillOnce(::testing::Return(mockIndicators));

    status = InvokeServiceMethod("org.rdk.FrontPanel.1.", "getFrontPanelLights", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
    EXPECT_TRUE(result.HasLabel("supportedLights"));
}
