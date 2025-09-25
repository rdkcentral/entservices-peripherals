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

TEST_F(FrontPanelInitializedTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setBrightness")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBrightness")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("powerLedOn")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("powerLedOff")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getFrontPanelLights")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setLED")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setBlink")));
}

TEST_F(FrontPanelInitializedEventDsTest, setBrightnessWIndex)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("Power"));


    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](int brightness, bool toPersist) {
                EXPECT_EQ(brightness, 1);
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBrightness"), _T("{\"brightness\": 1,\"index\": \"power_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBrightness)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
            .WillByDefault(::testing::Return(device::List<device::FrontPanelIndicator>({device::FrontPanelIndicator::getInstance()})));

    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("Power"));

    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](int brightness, bool toPersist) {
                EXPECT_EQ(brightness, 1);
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBrightness"), _T("{\"brightness\": 1}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(FrontPanelInitializedEventDsTest, getBrightnessWIndex)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));
    ON_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillByDefault(::testing::Return(50));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBrightness"), _T("{\"index\": \"power_led\"}"), response));
    EXPECT_EQ(response, string("{\"brightness\":50,\"success\":true}"));
}

TEST_F(FrontPanelInitializedEventDsTest, getBrightnessOtherName)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("other", name);
                return device::FrontPanelIndicator::getInstance();
            }));
    ON_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillByDefault(::testing::Return(50));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBrightness"), _T("{\"index\": \"other\"}"), response));
    EXPECT_EQ(response, string("{\"brightness\":50,\"success\":true}"));
}


TEST_F(FrontPanelInitializedEventDsTest, getBrightness)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));
    ON_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillByDefault(::testing::Return(50));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getBrightness"), _T(""), response));
    EXPECT_EQ(response, string("{\"brightness\":50,\"success\":true}"));
}


TEST_F(FrontPanelInitializedEventDsTest, getFrontPanelLights)
{
    ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillByDefault(::testing::Return(device::List<device::FrontPanelIndicator>({ device::FrontPanelIndicator::getInstance() })));

    ON_CALL(frontPanelIndicatorMock, getBrightnessLevels(::testing::_,::testing::_,::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](int &levels,int &min,int &max) {
                levels=1;
                min=0;
                max=2;
            }));
    ON_CALL(frontPanelTextDisplayIndicatorMock, getBrightnessLevels(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](int& levels, int& min, int& max) {
                levels = 1;
                min = 0;
                max = 2;
            }));

    ON_CALL(*p_frontPanelConfigImplMock, getTextDisplays())
        .WillByDefault(::testing::Return(device::List<device::FrontPanelTextDisplay>({ device::FrontPanelTextDisplay::getInstance() })));
    ON_CALL(frontPanelTextDisplayIndicatorMock, getName())
        .WillByDefault(::testing::Return("Text"));
    ON_CALL(*p_colorImplMock, getName())
        .WillByDefault(::testing::Return("white"));

        int test = 0;

    ON_CALL(*p_frontPanelConfigImplMock, getTextDisplay(test))
        .WillByDefault(::testing::ReturnRef(device::FrontPanelTextDisplay::getInstance()));

    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("Power"));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getFrontPanelLights"), _T(""), response));
    EXPECT_EQ(response, string("{\"supportedLights\":[\"power_led\"],\"supportedLightsInfo\":{\"power_led\":{\"range\":\"boolean\",\"min\":0,\"max\":2,\"colorMode\":0}},\"success\":true}"));
}

TEST_F(FrontPanelInitializedEventDsTest, powerLedOffPower)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));


    EXPECT_CALL(frontPanelIndicatorMock, setState(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](bool state) {
                EXPECT_EQ(state, false);
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("powerLedOff"), _T("{\"index\": \"power_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}
TEST_F(FrontPanelInitializedEventDsTest, powerLedOffData)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Message", name);
                return device::FrontPanelIndicator::getInstance();
            }));


    EXPECT_CALL(frontPanelIndicatorMock, setState(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](bool state) {
                EXPECT_EQ(state, false);
            }));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("powerLedOff"), _T("{\"index\": \"data_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}
TEST_F(FrontPanelInitializedEventDsTest, powerLedOffRecord)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Record", name);
                return device::FrontPanelIndicator::getInstance();
            }));


    EXPECT_CALL(frontPanelIndicatorMock, setState(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](bool state) {
                EXPECT_EQ(state, false);
            }));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("powerLedOff"), _T("{\"index\": \"record_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(FrontPanelInitializedEventDsTest, powerLedOnPower)
{

    ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillByDefault(::testing::Return(device::List<device::FrontPanelIndicator>({device::FrontPanelIndicator::getInstance()})));
    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("red"));

 ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("powerLedOn"), _T("{\"index\": \"power_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Record", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("powerLedOn"), _T("{\"index\": \"record_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Message", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("powerLedOn"), _T("{\"index\": \"data_led\"}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));


}

TEST_F(FrontPanelInitializedEventDsTest, setBlink)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    ON_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .WillByDefault(::testing::Return(50));
    ON_CALL(*p_frontPanelTextDisplayMock, getTextBrightness())
        .WillByDefault(::testing::Return(50));

    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("Power"));

    EXPECT_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](uint32_t color, bool persist) {
                EXPECT_EQ(color, 131586);
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setBlink"), _T("{\"blinkInfo\": {\"ledIndicator\": \"power_led\", \"iterations\": 10, \"pattern\": [{\"brightness\": 50, \"duration\": 1000, \"red\": 2, \"green\":2, \"blue\":2}]}}"), response));
    EXPECT_EQ(response, string("{\"success\":true}"));
        
}

TEST_F(FrontPanelInitializedEventDsTest, setLEDMode1)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    ON_CALL(frontPanelIndicatorMock, getName())
        .WillByDefault(::testing::Return("Power"));

    EXPECT_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](uint32_t color, bool toPersist) {
                EXPECT_EQ(color, 0);
            }));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setLED"), _T("{\"ledIndicator\": \"power_led\", \"brightness\": 50, \"red\": 0, \"green\": 0, \"blue\":0}"), response));

        EXPECT_EQ(response, string("{\"success\":true}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setLEDMode2)
{

    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    ON_CALL(*p_colorImplMock, getInstanceByName(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator::Color& {
                return device::FrontPanelIndicator::Color::getInstance();
            }));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setLED"), _T("{\"ledIndicator\": \"power_led\", \"brightness\": 50, \"color\": \"red\", \"red\": 1, \"green\": 2, \"blue\":3}"), response));

        EXPECT_EQ(response, string("{\"success\":true}"));
}

// --- Negative Test Cases ---

TEST_F(FrontPanelInitializedEventDsTest, setBrightnessException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](int brightness, bool toPersist) {
                throw std::runtime_error("mocked hardware exception");
            }));

    uint32_t result = handler.Invoke(connection, _T("setBrightness"), _T("{\"brightness\": 50,\"index\": \"power_led\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, getBrightnessException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, getBrightness(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](bool fromCache) -> int {
                throw std::runtime_error("mocked hardware exception");
                return 0;
            }));

    uint32_t result = handler.Invoke(connection, _T("getBrightness"), _T("{\"index\":\"power_led\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"brightness\":4294967295,\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, powerOnLedException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, setState(true))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](bool state) {
                throw std::runtime_error("mocked hardware exception on power on");
            }));

    uint32_t result = handler.Invoke(connection, _T("powerLedOn"), _T("{\"index\":\"power_led\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, powerOffLedException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, setState(false))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](bool state) {
                throw std::runtime_error("mocked hardware exception on power off");
            }));

    uint32_t result = handler.Invoke(connection, _T("powerLedOff"), _T("{\"index\":\"power_led\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setLEDException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, setBrightness(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](int brightness, bool toPersist) {
                throw std::runtime_error("mocked hardware exception on setBrightness");
            }));

    uint32_t result = handler.Invoke(connection, _T("setLED"), _T("{\"ledIndicator\": \"power_led\", \"brightness\": 50, \"color\": \"red\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setLEDColorModeException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    ON_CALL(*p_colorImplMock, getInstanceByName(::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator::Color& {
                throw std::runtime_error("color not supported");
                return *p_colorImplMock;
            }));

    uint32_t result = handler.Invoke(connection, _T("setLED"), _T("{\"ledIndicator\": \"power_led\", \"brightness\": 50, \"color\": \"invalid_color\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setLEDColorIntException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](uint32_t color, bool toPersist) {
                throw std::runtime_error("hardware color setting failed");
            }));

    uint32_t result = handler.Invoke(connection, _T("setLED"), _T("{\"ledIndicator\": \"power_led\", \"brightness\": 50, \"red\": 255, \"green\": 0, \"blue\": 0}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBrightnessInvalidRange)
{
    uint32_t result = handler.Invoke(connection, _T("setBrightness"), _T("{\"brightness\": 150}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBrightnessNegativeValue)
{
    uint32_t result = handler.Invoke(connection, _T("setBrightness"), _T("{\"brightness\": -10}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBrightnessMissingParameter)
{
    uint32_t result = handler.Invoke(connection, _T("setBrightness"), _T("{}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, powerLedOnInvalidIndex)
{
    uint32_t result = handler.Invoke(connection, _T("powerLedOn"), _T("{\"index\": \"invalid_led\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, powerLedOffInvalidIndex)
{
    uint32_t result = handler.Invoke(connection, _T("powerLedOff"), _T("{\"index\": \"invalid_led\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setLEDMissingIndicator)
{
    uint32_t result = handler.Invoke(connection, _T("setLED"), _T("{\"brightness\": 50, \"color\": \"red\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBlinkInvalidJSON)
{
    uint32_t result = handler.Invoke(connection, _T("setBlink"), _T("{\"blinkInfo\": \"malformed_json\"}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBlinkMissingPattern)
{
    uint32_t result = handler.Invoke(connection, _T("setBlink"), _T("{\"blinkInfo\": {\"ledIndicator\": \"power_led\", \"iterations\": 10}}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, setBlinkException)
{
    ON_CALL(frontPanelIndicatorMock, getInstanceString)
        .WillByDefault(::testing::Invoke(
            [&](const std::string& name) -> device::FrontPanelIndicator& {
                EXPECT_EQ("Power", name);
                return device::FrontPanelIndicator::getInstance();
            }));

    EXPECT_CALL(frontPanelIndicatorMock, setColorInt(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](uint32_t color, bool persist) {
                throw std::runtime_error("blink hardware exception");
            }));

    uint32_t result = handler.Invoke(connection, _T("setBlink"), _T("{\"blinkInfo\": {\"ledIndicator\": \"power_led\", \"iterations\": 5, \"pattern\": [{\"brightness\": 50, \"duration\": 1000, \"red\": 255, \"green\": 0, \"blue\": 0}]}}"), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"success\":false}"));
}

TEST_F(FrontPanelInitializedEventDsTest, getFrontPanelLightsException)
{
    ON_CALL(*p_frontPanelConfigImplMock, getIndicators())
        .WillByDefault(::testing::Invoke(
            []() -> device::List<device::FrontPanelIndicator> {
                throw std::runtime_error("hardware configuration error");
                return device::List<device::FrontPanelIndicator>();
            }));

    uint32_t result = handler.Invoke(connection, _T("getFrontPanelLights"), _T(""), response);
    EXPECT_EQ(result, Core::ERROR_GENERAL);
    EXPECT_EQ(response, string("{\"supportedLights\":[],\"supportedLightsInfo\":{},\"success\":false}"));
}
