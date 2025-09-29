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

#include "FrontPanel.h"
#include "FrontPanelImplementation.h"
#include "frontpanel.h"
#include "frontpanel.cpp"
#include "FrontPanelMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "COMLinkMock.h"

#include "FactoriesImplementation.h"

#include "FrontPanelIndicatorMock.h"
#include "FrontPanelTextDisplayMock.h"
#include "FrontPanelConfigMock.h"
#include "IarmBusMock.h"
#include "ServiceMock.h"
#include "ColorMock.h"
#include "PowerManagerMock.h"
#include "ThunderPortability.h"

using namespace WPEFramework;
using IPowerManager = Exchange::IPowerManager;

using testing::Eq;

class FrontPanelTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::FrontPanel> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    NiceMock<ServiceMock> service;
    NiceMock<COMLinkMock> comLinkMock;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::ProxyType<Plugin::FrontPanelImplementation> FrontPanelImplem;
    NiceMock<FactoriesImplementation> factoriesImplementation;
    PLUGINHOST_DISPATCHER *dispatcher;
    string response;
    Core::JSONRPC::Message message;
    ServiceMock  *p_serviceMock  = nullptr;
    WrapsImplMock* p_wrapsImplMock = nullptr;
    FrontPanelMock* p_frontPanelMock = nullptr;
    
    FrontPanelTest()
        : plugin(Core::ProxyType<Plugin::FrontPanel>::Create())
        , handler(*plugin)
        , connection(0,1,"")
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        
        p_serviceMock = new NiceMock <ServiceMock>;

        p_frontPanelMock  = new NiceMock <FrontPanelMock>;

        p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

        PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
        plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);


        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
                .WillByDefault(::testing::Invoke(
                    [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        FrontPanelImplem = Core::ProxyType<Plugin::FrontPanelImplementation>::Create();
                        return &FrontPanelImplem;
                    }));

        Core::IWorkerPool::Assign(&(*workerPool));
            workerPool->Run();

    }

    virtual ~FrontPanelTest()
    {
        // Ensure we deactivate dispatcher before releasing it
        if (dispatcher != nullptr) {
            // Deactivate if it was activated (safe even if not)
            dispatcher->Deactivate();
            dispatcher->Release();
            dispatcher = nullptr;
        }

        // Restore global factory hooks
        PluginHost::IFactories::Assign(nullptr);

        // Clear Wraps implementation and delete allocated mocks
        Wraps::setImpl(nullptr);
        delete p_wrapsImplMock;
        p_wrapsImplMock = nullptr;

        delete p_frontPanelMock;
        p_frontPanelMock = nullptr;

        delete p_serviceMock;
        p_serviceMock = nullptr;

    }
};

class FrontPanelInitializedTest : public FrontPanelTest {
protected:
    IarmBusImplMock   *p_iarmBusImplMock = nullptr ;
    FrontPanelConfigMock   *p_frontPanelConfigImplMock = nullptr;
    FrontPanelTextDisplayMock   *p_frontPanelTextDisplayMock = nullptr;
    IPowerManager::IModeChangedNotification* _notification = nullptr;

    IARM_EventHandler_t dsPanelEventHandler;
    testing::NiceMock<FrontPanelIndicatorMock> frontPanelIndicatorMock;
    testing::NiceMock<FrontPanelIndicatorMock> frontPanelTextDisplayIndicatorMock;


    FrontPanelInitializedTest()
        : FrontPanelTest()
    {

        p_iarmBusImplMock  = new testing::NiceMock <IarmBusImplMock>;
        IarmBus::setImpl(p_iarmBusImplMock);
        p_frontPanelConfigImplMock  = new testing::NiceMock <FrontPanelConfigMock>;
        device::FrontPanelConfig::setImpl(p_frontPanelConfigImplMock);

        device::FrontPanelIndicator::getInstance().impl = &frontPanelIndicatorMock;
        device::FrontPanelTextDisplay::getInstance().FrontPanelIndicator::impl = &frontPanelTextDisplayIndicatorMock;

        p_frontPanelTextDisplayMock  = new testing::NiceMock <FrontPanelTextDisplayMock>;
        device::FrontPanelTextDisplay::setImpl(p_frontPanelTextDisplayMock);

        //Needs to be set at initiative time, as the function gets called when FrontPanel is intialized.
        ON_CALL(frontPanelIndicatorMock, getInstanceString)
            .WillByDefault(::testing::Invoke(
                [&](const std::string& name) -> device::FrontPanelIndicator& {
                    //EXPECT_EQ("Power", name);
                    return device::FrontPanelIndicator::getInstance();
                }));

        ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
            .WillByDefault(::testing::Return(device::List<device::FrontPanelIndicator>({ device::FrontPanelIndicator::getInstance() })));

        EXPECT_CALL(PowerManagerMock::Mock(), Register(::testing::Matcher<Exchange::IPowerManager::IModeChangedNotification*>(::testing::_)))
            .WillOnce(
                [this](IPowerManager::IModeChangedNotification* notification) -> uint32_t {
                    _notification = notification;
                    return Core::ERROR_NONE;
                });

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }
    virtual ~FrontPanelInitializedTest() override
    {
        device::FrontPanelIndicator::getInstance().impl = nullptr;
        device::FrontPanelTextDisplay::getInstance().FrontPanelIndicator::impl = nullptr;

        plugin->Deinitialize(&service);

        //delete Plugin::CFrontPanel::instance(&service);
        
        _notification = nullptr;
        PowerManagerMock::Delete();

        //Clearing out out of scope variables, and setting initDone to 0.
        Plugin::CFrontPanel::initDone = 0;
        IarmBus::setImpl(nullptr);
        if (p_iarmBusImplMock != nullptr)
        {
            delete p_iarmBusImplMock;
            p_iarmBusImplMock = nullptr;
        }
        device::FrontPanelTextDisplay::setImpl(nullptr);
        if (p_frontPanelTextDisplayMock != nullptr)
        {
            delete p_frontPanelTextDisplayMock;
            p_frontPanelTextDisplayMock = nullptr;
        }
        device::FrontPanelConfig::setImpl(nullptr);
        if (p_frontPanelConfigImplMock != nullptr)
        {
            delete p_frontPanelConfigImplMock;
            p_frontPanelConfigImplMock = nullptr;
        }
    }
};

class FrontPanelInitializedEventTest : public FrontPanelInitializedTest {
protected:
    testing::NiceMock<ServiceMock> service;
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER* dispatcher;
    Core::JSONRPC::Message message;

    FrontPanelInitializedEventTest()
        : FrontPanelInitializedTest()
    {
        PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
            plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));

        dispatcher->Activate(&service);
    }

    virtual ~FrontPanelInitializedEventTest() override
    {
        dispatcher->Deactivate();
        dispatcher->Release();

        PluginHost::IFactories::Assign(nullptr);
    }
};

class FrontPanelInitializedEventDsTest : public FrontPanelInitializedEventTest {
protected:

    ColorMock      *p_colorImplMock = nullptr ;

    FrontPanelInitializedEventDsTest()
        : FrontPanelInitializedEventTest()
    {
        p_colorImplMock  = new testing::NiceMock <ColorMock>;
        device::FrontPanelIndicator::Color::setImpl(p_colorImplMock);

        EXPECT_NE(_notification, nullptr);
        _notification->OnPowerModeChanged(IPowerManager::POWER_STATE_STANDBY, IPowerManager::POWER_STATE_ON);
    }

    virtual ~FrontPanelInitializedEventDsTest() override
    {
        device::FrontPanelIndicator::Color::setImpl(nullptr);
        if (p_colorImplMock != nullptr)
        {
            delete p_colorImplMock;
            p_colorImplMock = nullptr;
        }
    }
};

using namespace WPEFramework;
using ::testing::_;
using ::testing::Return;
using ::testing::NiceMock;

class FrontPanel_L1_Test : public FrontPanelInitializedTest {
protected:
    FrontPanel_L1_Test() = default;
    ~FrontPanel_L1_Test() override = default;
};

// Test initialization of the FrontPanel plugin
TEST_F(FrontPanel_L1_Test, L1_Initialize) {
    // Verify that plugin is initialized correctly
    EXPECT_NE(plugin, nullptr);
    EXPECT_NE(FrontPanelImplem, nullptr);
    
    // Verify that the IFrontPanel interface is correctly set up
    EXPECT_TRUE(plugin->IsActive());
    EXPECT_NE(_notification, nullptr);
}

// Test deinitialize behavior
TEST_F(FrontPanel_L1_Test, L1_Deinitialize) {
    // First verify the plugin is active
    EXPECT_TRUE(plugin->IsActive());
    
    // Set up expectations for deinitialization
    EXPECT_CALL(PowerManagerMock::Mock(), Unregister(::testing::_))
        .WillOnce(Return(Core::ERROR_NONE));
    
    // Deinitialize the plugin
    plugin->Deinitialize(&service);
    
    // Verify the plugin is no longer active
    EXPECT_FALSE(plugin->IsActive());
}

// Test Information method
TEST_F(FrontPanel_L1_Test, L1_Information) {
    std::string info = plugin->Information();
    EXPECT_FALSE(info.empty());
    EXPECT_NE(info.find("FrontPanel"), std::string::npos);
}

// Test SetBrightness method
TEST_F(FrontPanel_L1_Test, L1_SetBrightness) {
    // Arrange
    ON_CALL(frontPanelIndicatorMock, getInstanceString(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                return device::FrontPanelIndicator::getInstance();
            }));
            
    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(50, false))
        .WillOnce(::testing::Return());
    
    // Act
    Exchange::IFrontPanel::FrontPanelSuccess success;
    Core::hresult result = FrontPanelImplem->SetBrightness("power_led", 50, success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success.success);
}

// Test GetBrightness method
TEST_F(FrontPanel_L1_Test, L1_GetBrightness) {
    // Arrange
    ON_CALL(frontPanelIndicatorMock, getInstanceString(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                return device::FrontPanelIndicator::getInstance();
            }));
            
    EXPECT_CALL(frontPanelIndicatorMock, getBrightness(false))
        .WillOnce(Return(75));
    
    // Act
    uint32_t brightness = 0;
    bool success = false;
    Core::hresult result = FrontPanelImplem->GetBrightness("power_led", brightness, success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_EQ(brightness, 75);
}

// Test GetBrightness failure case
TEST_F(FrontPanel_L1_Test, L1_GetBrightness_Failure) {
    // Arrange
    ON_CALL(frontPanelIndicatorMock, getInstanceString(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                return device::FrontPanelIndicator::getInstance();
            }));
            
    EXPECT_CALL(frontPanelIndicatorMock, getBrightness(false))
        .WillOnce(Return(-1));
    
    // Act
    uint32_t brightness = 0;
    bool success = true;
    Core::hresult result = FrontPanelImplem->GetBrightness("power_led", brightness, success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_FALSE(success);
}

// Test PowerLedOn method
TEST_F(FrontPanel_L1_Test, L1_PowerLedOn) {
    // Arrange
    EXPECT_CALL(*p_frontPanelMock, powerOnLed(FRONT_PANEL_INDICATOR_POWER))
        .WillOnce(Return(true));
        
    // Mock CFrontPanel::instance to return our mock
    Plugin::CFrontPanel::_instance = p_frontPanelMock;
    
    // Act
    Exchange::IFrontPanel::FrontPanelSuccess success;
    Core::hresult result = FrontPanelImplem->PowerLedOn("power_led", success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success.success);
    
    // Cleanup
    Plugin::CFrontPanel::_instance = nullptr;
}

// Test PowerLedOff method
TEST_F(FrontPanel_L1_Test, L1_PowerLedOff) {
    // Arrange
    EXPECT_CALL(*p_frontPanelMock, powerOffLed(FRONT_PANEL_INDICATOR_POWER))
        .WillOnce(Return(true));
        
    // Mock CFrontPanel::instance to return our mock
    Plugin::CFrontPanel::_instance = p_frontPanelMock;
    
    // Act
    Exchange::IFrontPanel::FrontPanelSuccess success;
    Core::hresult result = FrontPanelImplem->PowerLedOff("power_led", success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success.success);
    
    // Cleanup
    Plugin::CFrontPanel::_instance = nullptr;
}

// Test GetFrontPanelLights method
TEST_F(FrontPanel_L1_Test, L1_GetFrontPanelLights) {
    // Arrange
    device::List<device::FrontPanelIndicator> indicators;
    indicators.push_back(device::FrontPanelIndicator::getInstance());
    
    ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillByDefault(Return(indicators));
        
    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(Return("Power"));
        
    // Setup for indicator info
    ON_CALL(frontPanelIndicatorMock, getBrightnessLevels(_, _, _))
        .WillByDefault(::testing::Invoke(
            [](int& levels, int& min, int& max) {
                levels = 10;
                min = 0;
                max = 100;
            }));
            
    ON_CALL(frontPanelIndicatorMock, getColorMode())
        .WillByDefault(Return(1));
        
    device::List<device::FrontPanelIndicator::Color> colors;
    device::FrontPanelIndicator::Color color;
    colors.push_back(color);
    
    ON_CALL(frontPanelIndicatorMock, getSupportedColors())
        .WillByDefault(Return(colors));
    
    // Act
    Exchange::IFrontPanel::IFrontPanelLightsListIterator* supportedLights = nullptr;
    std::string supportedLightsInfo;
    bool success = false;
    Core::hresult result = FrontPanelImplem->GetFrontPanelLights(supportedLights, supportedLightsInfo, success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success);
    EXPECT_NE(supportedLights, nullptr);
    EXPECT_FALSE(supportedLightsInfo.empty());
    
    // Cleanup
    if (supportedLights) {
        supportedLights->Release();
    }
}

// Test SetLED method
TEST_F(FrontPanel_L1_Test, L1_SetLED) {
    // Arrange
    EXPECT_CALL(*p_frontPanelMock, setLED(_))
        .WillOnce(Return(true));
        
    // Mock CFrontPanel::instance to return our mock
    Plugin::CFrontPanel::_instance = p_frontPanelMock;
    
    // Act
    Exchange::IFrontPanel::FrontPanelSuccess success;
    Core::hresult result = FrontPanelImplem->SetLED("power_led", 75, "red", 255, 0, 0, success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success.success);
    
    // Cleanup
    Plugin::CFrontPanel::_instance = nullptr;
}

// Test SetBlink method
TEST_F(FrontPanel_L1_Test, L1_SetBlink) {
    // Arrange
    JsonObject blinkInfoObj;
    blinkInfoObj["ledIndicator"] = "power_led";
    blinkInfoObj["iterations"] = 5;
    blinkInfoObj["pattern"] = JsonArray();
    std::string blinkInfo;
    blinkInfoObj.ToString(blinkInfo);
    
    EXPECT_CALL(*p_frontPanelMock, setBlink(_))
        .WillOnce(::testing::Return());
        
    // Mock CFrontPanel::instance to return our mock
    Plugin::CFrontPanel::_instance = p_frontPanelMock;
    
    // Act
    Exchange::IFrontPanel::FrontPanelSuccess success;
    Core::hresult result = FrontPanelImplem->SetBlink(blinkInfo, success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_NONE);
    EXPECT_TRUE(success.success);
    
    // Cleanup
    Plugin::CFrontPanel::_instance = nullptr;
}

// Test SetBlink failure case with invalid JSON
TEST_F(FrontPanel_L1_Test, L1_SetBlink_InvalidJson) {
    // Act
    Exchange::IFrontPanel::FrontPanelSuccess success;
    Core::hresult result = FrontPanelImplem->SetBlink("invalid json", success);
    
    // Assert
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_FALSE(success.success);
}

// Test power mode changed behavior
TEST_F(FrontPanel_L1_Test, L1_PowerModeChanged) {
    // Arrange
    EXPECT_CALL(*p_frontPanelMock, setPowerStatus(true))
        .WillOnce(Return(true));
        
    // Mock CFrontPanel::instance to return our mock
    Plugin::CFrontPanel::_instance = p_frontPanelMock;
    
    // Act - trigger the power mode changed notification
    ASSERT_NE(_notification, nullptr);
    _notification->OnPowerModeChanged(Exchange::IPowerManager::POWER_STATE_STANDBY, Exchange::IPowerManager::POWER_STATE_ON);
    
    // Cleanup
    Plugin::CFrontPanel::_instance = nullptr;
}
