/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#include "L2Tests.h"
#include "L2TestsMock.h"
#include <condition_variable>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <interfaces/ILEDControl.h>
#include <mutex>

#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);

#define LED_CALLSIGN _T("org.rdk.LEDControl.1")
#define LEDL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::ILEDControl;

class LEDControl_L2test : public L2TestMocks {
protected:
    virtual ~LEDControl_L2test() override;

public:
    LEDControl_L2test();
    uint32_t CreateDeviceLEDControlInterfaceObject();

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell* m_controller_LED;

    /** @brief Pointer to the ILEDControl interface */
    Exchange::ILEDControl* m_LEDplugin;
};

LEDControl_L2test::LEDControl_L2test()
    : L2TestMocks()
{
    uint32_t status = Core::ERROR_GENERAL;

    ON_CALL(*p_dsFPDMock, dsFPInit())
        .WillByDefault(testing::Return(dsERR_NONE));

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.LEDControl");
    EXPECT_EQ(Core::ERROR_NONE, status);

    if (CreateDeviceLEDControlInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid LEDControl_Client");
    } else {
        EXPECT_TRUE(m_controller_LED != nullptr);
        if (m_controller_LED) {
            EXPECT_TRUE(m_LEDplugin != nullptr);
            if (m_LEDplugin) {
                m_LEDplugin->AddRef();
            } else {
                TEST_LOG("m_LEDplugin is NULL");
            }
        } else {
            TEST_LOG("m_controller_LED is NULL");
        }
    }
}

LEDControl_L2test::~LEDControl_L2test()
{
    TEST_LOG("Inside LEDControl_L2test destructor");

    ON_CALL(*p_dsFPDMock, dsFPTerm())
        .WillByDefault(testing::Return(dsERR_NONE));

    if (m_LEDplugin) {
        m_LEDplugin->Release();
        m_LEDplugin = nullptr;
    }
    if (m_controller_LED) {
        m_controller_LED->Release();
        m_controller_LED = nullptr;
    }

    uint32_t status = Core::ERROR_GENERAL;

    /* Deactivate plugin in destructor */
    status = DeactivateService("org.rdk.LEDControl");
    sleep(3);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

uint32_t LEDControl_L2test::CreateDeviceLEDControlInterfaceObject()
{
    uint32_t return_value = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> LEDControl_Engine;
    Core::ProxyType<RPC::CommunicatorClient> LEDControl_Client;

    TEST_LOG("Creating LEDControl_Engine");
    LEDControl_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    LEDControl_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(LEDControl_Engine));

    TEST_LOG("Creating LEDControl_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    LEDControl_Engine->Announcements(mLEDControl_Client->Announcement());
#endif
    if (!LEDControl_Client.IsValid()) {
        TEST_LOG("Invalid LEDControl_Client");
    } else {
        m_controller_LED = LEDControl_Client->Open<PluginHost::IShell>(_T("org.rdk.LEDControl"), ~0, 3000);
        if (m_controller_LED) {
            m_LEDplugin = m_controller_LED->QueryInterface<Exchange::ILEDControl>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

/************Test case Details **************************
** 1.getSupportedLEDStates with states set as "ACTIVE,STANDBY" using jsonrpc
*******************************************************/

TEST_F(LEDControl_L2test, JSONRPC_GetSupportedLEDStates_ACTIVE)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(LED_CALLSIGN, LEDL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject param, result;

    //mock dsFPGetSupportedLEDStates to respond with ACTIVE and STANDBY states
    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_ACTIVE | 1 << dsFPD_LED_DEVICE_STANDBY),
            ::testing::Return(dsERR_NONE)));

    status = InvokeServiceMethod("org.rdk.LEDControl.1", "getSupportedLEDStates", param, result);
    EXPECT_EQ(Core::ERROR_NONE, status);
    EXPECT_TRUE(result["success"].Boolean());
}

/************Test case Details **************************
** 1.setLEDState with states set as "ACTIVE" using jsonrpc
*******************************************************/

TEST_F(LEDControl_L2test, JSONRPC_Set_LEDState_ACTIVE)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(LED_CALLSIGN, LEDL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject param, response;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    param["state"] = "ACTIVE";
    status = InvokeServiceMethod("org.rdk.LEDControl.1", "setLEDState", param, response);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

/************Test case Details **************************
** 1.getLEDState with states set as "ACTIVE" using jsonrpc
*******************************************************/

TEST_F(LEDControl_L2test, JSONRPC_Get_LEDState_ACTIVE)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(LED_CALLSIGN, LEDL2TEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject param, result;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_ACTIVE),
            ::testing::Return(dsERR_NONE)));

    status = InvokeServiceMethod("org.rdk.LEDControl.1", "getLEDState", param, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "ACTIVE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_ACTIVE)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_ACTIVE),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "STANDBY" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_STANDBY)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_STANDBY),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "WPSCONNECTING" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_WPSCONNECTING)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_WPS_CONNECTING),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "WPSCONNECTED" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_WPSCONNECTED)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_WPS_CONNECTED),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "WPSERROR" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_WPSERROR)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_WPS_ERROR),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "FACTORY_RESET" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_RESET)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_FACTORY_RESET),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "USB_UPGRADE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_USBUPGRADE)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_USB_UPGRADE),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with states set as "SOFTWARE_DOWNLOAD_ERROR" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_DOWNLOADERROR)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(1 << dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);

    if (supportedLEDStates != nullptr) {
        string value;
        while (supportedLEDStates->Next(value) == true) {
            TEST_LOG("supportedLEDState: %s", value.c_str());
        }
    } else {
        TEST_LOG("supportedLEDStates is empty!");
    }
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with mock returning error using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, GetSupportedLEDStates_ErrorCase)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    //return dsERR_GENERAL for failure case
    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::Return(dsERR_GENERAL));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(success);
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "ACTIVE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_ACTIVE)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "ACTIVE";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state set to be "ACTIVE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_ACTIVE)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_ACTIVE),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "STANDBY" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_STANDBY)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "STANDBY";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state set to be "STANDBY" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_STANDBY)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_STANDBY),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "WPS_CONNECTING" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_WPSCONNECTING)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "WPS_CONNECTING";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state set to be "WPS_CONNECTING" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_WPSCONNECTING)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_WPS_CONNECTING),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "WPS_CONNECTED" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_CONNECTED)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "WPS_CONNECTED";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state returned as "WPS_CONNECTED" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_WPSCONNECTED)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_WPS_CONNECTED),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "WPS_ERROR" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_ERROR)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "WPS_ERROR";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state returned as "WPS_ERROR" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_ERROR)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_WPS_ERROR),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "FACTORY_RESET" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_FACTORYRESET)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "FACTORY_RESET";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state returned as "DEVICE_FACTORY_RESET" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_FACTORYRESET)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_FACTORY_RESET),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "USB_UPGRADE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_USBUPGRADE)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "USB_UPGRADE";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state returned as "DEVICE_USB_UPGRADE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_USBUPGRADE)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_USB_UPGRADE),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "DOWNLOAD_ERROR" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_DOWNLOADERROR)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "DOWNLOAD_ERROR";
    bool success = false;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_NONE));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(success);
}

/************Test case Details **************************
** 1.GetLEDState with state returned as "SOFTWARE_DOWNLOAD_ERROR" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_DOWNLOADERROR)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_SOFTWARE_DOWNLOAD_ERROR),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.GetLEDState with state returned as "NONE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_FPD_LED_DEVICE_NONE)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_NONE),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_NONE);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with state set to be "NONE" using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_NONE)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "NONE";
    bool success = true;

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_BAD_REQUEST);
    EXPECT_FALSE(success);
}

/************Test case Details **************************
** 1.GetLEDState with dsFPGetLEDState mock returning dsFPD_LED_DEVICE_MAX for Unsupported LED state using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_defaultCase)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(dsFPD_LED_DEVICE_MAX),
            ::testing::Return(dsERR_NONE)));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_BAD_REQUEST);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.GetLEDState with dsFPGetLEDState mock returning error using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Get_LEDState_Errorcase)
{
    Exchange::ILEDControl::LEDControlState LEDState;
    uint32_t status = Core::ERROR_NONE;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_GENERAL));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_GENERAL);

    TEST_LOG("GetLEDState returned: %s", LEDState.state.c_str());
}

/************Test case Details **************************
** 1.SetLEDState with invalid parameter using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_InvalidParameter)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "INVALID";
    bool success = true;

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_BAD_REQUEST);
    EXPECT_FALSE(success);
}

/************Test case Details **************************
** 1.SetLEDState with empty parameter using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_emptyParameter)
{
    uint32_t status = Core::ERROR_NONE;
    string State;
    bool success = true;

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_BAD_REQUEST);
    EXPECT_FALSE(success);
}

/************Test case Details **************************
** 1.SetLEDState with dsFPSetLEDState mock returning error using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, Set_LEDState_dsFPSetLEDState_Error)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "DOWNLOAD_ERROR";
    bool success = true;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Return(dsERR_GENERAL));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(success);
}

/************Test case Details **************************
** 1.GetSupportedLEDStates with dsFPGetSupportedLEDStates mock raising exception using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, dsFPGetSupportedLEDStates_RaiseException)
{
    uint32_t status = Core::ERROR_NONE;
    bool success = false;
    WPEFramework::RPC::IStringIterator* supportedLEDStates;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetSupportedLEDStates(::testing::_))
        .WillOnce(::testing::Invoke([](unsigned int* states) {
            throw std::runtime_error("Simulated Exception");
            return dsERR_NONE;
        }));

    status = m_LEDplugin->GetSupportedLEDStates(supportedLEDStates, success);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(success);
}

/************Test case Details **************************
** 1.GetLEDState with dsFPGetLEDState mock raising exception using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, dsFPGetLEDState_RaiseException)
{
    uint32_t status = Core::ERROR_NONE;
    Exchange::ILEDControl::LEDControlState LEDState;
    ;

    EXPECT_CALL(*p_dsFPDMock, dsFPGetLEDState(::testing::_))
        .WillOnce(::testing::Invoke([](dsFPDLedState_t* states) {
            throw std::runtime_error("Simulated Exception");
            return dsERR_NONE;
        }));

    status = m_LEDplugin->GetLEDState(LEDState);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
}

/************Test case Details **************************
** 1.SetLEDState with dsFPSetLEDState mock raising exception using comrpc
*******************************************************/

TEST_F(LEDControl_L2test, dsFPSetLEDState_RaiseException)
{
    uint32_t status = Core::ERROR_NONE;
    string State = "DOWNLOAD_ERROR";
    bool success = true;

    EXPECT_CALL(*p_dsFPDMock, dsFPSetLEDState(::testing::_))
        .WillOnce(::testing::Invoke([](dsFPDLedState_t states) {
            throw std::runtime_error("Simulated Exception");
            return dsERR_NONE;
        }));

    status = m_LEDplugin->SetLEDState(State, success);
    EXPECT_EQ(status, Core::ERROR_GENERAL);
    EXPECT_FALSE(success);
}
