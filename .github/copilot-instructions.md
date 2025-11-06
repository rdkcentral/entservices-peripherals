# WPEFramework Plugin L2 Integration Tes        - Use EXPECT_CALL for mocks with ::testing::_ for arguments unless a specific value is required for the test logic.
    - Use only error codes defined in the implementation (e.g., Core::ERROR_NONE, Core::ERROR_GENERAL, Core::ERROR_INVALID_PARAMETER). Do not invent error codes.
    - Ensure mock objects are properly initialized and cleaned up in the test fixture setup and teardown.
    - Use EXPECT_TRUE, EXPECT_FALSE, and EXPECT_EQ to check returned, updated, or tested event values using the test fixture's initialization, notification flags, handlers, and helper methods.
    - Test both successful integration scenarios and failure/error conditions in multi-plugin environments- Use real Thunder framework RPC stack for communication testing  - Every test must verify end-to-end integration behavior, state changes across plugins, and event propagationing Guide

### L2 Testing Objectives

For specified APIs, ensure the generated L2 tests meet the following criteria:
- **Integration Testing**: Test complete plugin lifecycle and inter-component communication in realistic environments
- **Cross-Plugin Communication**: Validate interactions between multiple plugins and services
- **Asynchronous Event Testing**: Test notification systems, event propagation, and timing between plugins
- **Real System Integration**: Use actual plugin instances and system APIs rather than mocks

## Testing
When generating L2 tests, please follow the below step-by-step approach:

1. **Checking for Adequate Context and Information**
    - Ensure that the test fixture, interface API header file, plugin implementation files, and related plugin dependencies are attached by the user for your reference.
    - Verify understanding of cross-plugin communication patterns and dependencies.

2. **Understanding the Plugin Integration**
    - For each API/method, step through the complete integration flow:
        - Map all plugin dependencies and inter-plugin communication paths
        - Identify Thunder framework integration points (JSON-RPC and COM-RPC)
        - Document asynchronous event flows and notification systems
        - Note real system resources, IARM bus communications, and hardware interactions
    - Before creating the tests, fully understand the complete plugin lifecycle and cross-plugin event propagation.

3. **Test Coverage**
    - For every cross-plugin interaction and integration scenario specified by the user, generate comprehensive test cases
    - Test both JSON-RPC and COM-RPC communication protocols to ensure identical behavior
    - Include full plugin activation, configuration, and cleanup testing
    - Test asynchronous event handling with proper timing and condition variable usage
    - Validate real system integration points and hardware abstraction layer interactions
    - Do not summarize, skip, or instruct the user to extrapolate patterns. Every specified integration scenario must be tested explicitly.

4. **Test Design**
    - Test complete plugin integration rather than isolated functionality
    - Use actual plugin instances instead of mocks for realistic testing
    - Implement asynchronous event testing with WaitForRequestStatus and condition variables
    - Test cross-plugin communication scenarios (e.g., UserPreferences ↔ UserSettings, SystemMode ↔ DisplaySettings)
    - Use GTest syntax for test structure 
    - Write all tests as modifications to the existing test fixture file
    - Use EXPECT_CALL for mocks with ::testing::_ for arguments unless a specific value is required for the test logic.
    - Use only error codes defined in the implementation (e.g., Core::ERROR_NONE, Core::ERROR_GENERAL, Core::ERROR_INVALID_PARAMETER). Do not invent error codes.
    - Ensure tests activate multiple services when testing plugin interactions
    - Use actual IARM bus communication and device settings APIs where applicable
    - Use EXPECT_TRUE, EXPECT_FALSE, and EXPECT_EQ to check returned, updated, or tested event values using the test fixture’s initialization, notification flags, handlers, and helper methods.

5. **Format**
    - Output only code, as if modifying the existing test fixture file
    - Do not include explanations, summaries, or instructions
    - Do not include comments like "repeat for other integrations" or "similarly for X"
    - Do not include any code that is not a test for a specific integration scenario

## Testing Example
Here are examples from the PowerManager L2 integration tests showing proper L2 test structure:

**L2 Test Class Structure with Notification Handling:**
```cpp
class PowerManager_L2Test : public L2TestMocks {
public:
    PowerManager_L2Test();
    virtual ~PowerManager_L2Test() override;

    void OnPowerModeChanged(const PowerState currentState, const PowerState newState);
    void OnPowerModePreChange(const PowerState currentState, const PowerState newState, const int trxnId, const int stateChangeAfter);
    void OnThermalModeChanged(const ThermalTemperature currentThermalLevel, const ThermalTemperature newThermalLevel, const float currentTemperature);
    
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, PowerManagerL2test_async_events_t expected_status);
    
    Core::Sink<PwrMgr_Notification> mNotification;

private:
    std::mutex m_mutex;
    std::condition_variable m_condition_variable;
    uint32_t m_event_signalled;
};
```

**Notification Interface Implementation:**
```cpp
class PwrMgr_Notification : public Exchange::IPowerManager::IRebootNotification,
                            public Exchange::IPowerManager::IModeChangedNotification,
                            public Exchange::IPowerManager::IThermalModeChangedNotification {
    BEGIN_INTERFACE_MAP(PwrMgr_Notification)
    INTERFACE_ENTRY(Exchange::IPowerManager::IRebootNotification)
    INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
    INTERFACE_ENTRY(Exchange::IPowerManager::IThermalModeChangedNotification)
    END_INTERFACE_MAP

public:
    void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_event_signalled |= POWERMANAGERL2TEST_SYSTEMSTATE_CHANGED;
        m_condition_variable.notify_one();
    }
    
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, PowerManagerL2test_async_events_t expected_status) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        
        while (!(expected_status & m_event_signalled)) {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                return POWERMANAGERL2TEST_STATE_INVALID;
            }
        }
        
        uint32_t signalled = m_event_signalled;
        m_event_signalled = POWERMANAGERL2TEST_STATE_INVALID;
        return signalled;
    }
};
```

**Constructor with Plugin Activation and Mock Setup:**
```cpp
PowerManager_L2Test::PowerManager_L2Test() : L2TestMocks() {
    m_event_signalled = POWERMANAGERL2TEST_STATE_INVALID;

    EXPECT_CALL(POWERMANAGER_MOCK, PLAT_DS_INIT())
        .WillOnce(::testing::Return(DEEPSLEEPMGR_SUCCESS));

    EXPECT_CALL(POWERMANAGER_MOCK, PLAT_INIT())
        .WillRepeatedly(::testing::Return(PWRMGR_SUCCESS));

    EXPECT_CALL(*p_mfrMock, mfrSetTempThresholds(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](int high, int critical) {
            EXPECT_EQ(high, 100);
            EXPECT_EQ(critical, 110);
            return mfrERR_NONE;
        }));

    // Activate the actual plugin service
    uint32_t status = ActivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}
```

**Destructor with Plugin Cleanup:**
```cpp
PowerManager_L2Test::~PowerManager_L2Test() {
    EXPECT_CALL(POWERMANAGER_MOCK, PLAT_TERM())
        .WillOnce(::testing::Return(PWRMGR_SUCCESS));

    EXPECT_CALL(POWERMANAGER_MOCK, PLAT_DS_TERM())
        .WillOnce(::testing::Return(DEEPSLEEPMGR_SUCCESS));

    uint32_t status = DeactivateService("org.rdk.PowerManager");
    EXPECT_EQ(Core::ERROR_NONE, status);
}
```

**COM-RPC Integration Test with Real Plugin Communication:**
```cpp
TEST_F(PowerManager_L2Test, PowerManagerComRpc) {
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngine_PowerManager;
    Core::ProxyType<RPC::CommunicatorClient> mClient_PowerManager;
    PluginHost::IShell *mController_PowerManager;

    mEngine_PowerManager = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClient_PowerManager = Core::ProxyType<RPC::CommunicatorClient>::Create(
        Core::NodeId("/tmp/communicator"), 
        Core::ProxyType<Core::IIPCServer>(mEngine_PowerManager));

    mEngine_PowerManager->Announcements(mClient_PowerManager->Announcement());

    if (mClient_PowerManager.IsValid()) {
        mController_PowerManager = mClient_PowerManager->Open<PluginHost::IShell>("org.rdk.PowerManager");
        
        if (mController_PowerManager) {
            Exchange::IPowerManager* PowerManagerPlugin = mController_PowerManager->QueryInterface<Exchange::IPowerManager>();
            
            if (PowerManagerPlugin) {
                PowerManagerPlugin->Register(&mNotification);
                
                Test_PowerStateChange(PowerManagerPlugin);
                Test_TemperatureThresholds(PowerManagerPlugin);
                Test_WakeupSrcConfig(PowerManagerPlugin);
                Test_NetworkStandbyMode(PowerManagerPlugin);
                
                PowerManagerPlugin->Unregister(&mNotification);
                PowerManagerPlugin->Release();
            }
            mController_PowerManager->Release();
        }
        mClient_PowerManager.Release();
    }
}
```

**Asynchronous Event Testing with State Verification:**
```cpp
void PowerManager_L2Test::Test_PowerStateChange(Exchange::IPowerManager* PowerManagerPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    uint32_t signalled = POWERMANAGERL2TEST_STATE_INVALID;
    
    PowerState currentState = PowerState::POWER_STATE_STANDBY;
    const string standbyReason = "";
    int keyCode = KED_FP_POWER;

    // Set power state and wait for notification
    status = PowerManagerPlugin->SetPowerState(keyCode, currentState, standbyReason);
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Wait for asynchronous event notification
    signalled = mNotification.WaitForRequestStatus(JSON_TIMEOUT, POWERMANAGERL2TEST_SYSTEMSTATE_CHANGED);
    EXPECT_TRUE(signalled & POWERMANAGERL2TEST_SYSTEMSTATE_CHANGED);

    // Verify state change through plugin interface
    PowerState currentState1 = PowerState::POWER_STATE_UNKNOWN;
    PowerState prevState1 = PowerState::POWER_STATE_UNKNOWN;
    
    status = PowerManagerPlugin->GetPowerState(currentState1, prevState1);
    EXPECT_EQ(currentState1, currentState);
    EXPECT_EQ(status, Core::ERROR_NONE);
}
```

**JSON-RPC Integration Test:**
```cpp
TEST_F(PowerManager_L2Test, JsonRpcWakeupSourceChange) {
    uint32_t status = Core::ERROR_GENERAL;
    JsonObject params;
    JsonObject result;
    JsonArray configs;

    JsonObject source;
    source["wakeupSource"] = "WIFI";
    source["enabled"] = true;
    configs.Add(source);
    
    params["wakeupSources"] = configs;

    EXPECT_CALL(POWERMANAGER_MOCK, PLAT_API_SetWakeupSrc(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([](PWRMGR_WakeupSrcType_t wakeupSrc, bool enabled) {
            EXPECT_EQ(wakeupSrc, PWRMGR_WAKEUPSRC_WIFI);
            EXPECT_EQ(enabled, true);
            return PWRMGR_SUCCESS;
        }));

    status = InvokeServiceMethod("org.rdk.PowerManager.1.", "setWakeupSourceConfig", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}
```

**Cross-Plugin Integration Test with Multiple Services:**
```cpp
void PowerManager_L2Test::Test_NetworkStandbyMode(Exchange::IPowerManager* PowerManagerPlugin) {
    uint32_t status = Core::ERROR_GENERAL;
    uint32_t signalled = POWERMANAGERL2TEST_STATE_INVALID;
    bool standbyMode = true;

    // Test setting network standby mode
    status = PowerManagerPlugin->SetNetworkStandbyMode(standbyMode);
    EXPECT_EQ(status, Core::ERROR_NONE);

    // Wait for notification event
    signalled = mNotification.WaitForRequestStatus(JSON_TIMEOUT, POWERMANAGERL2TEST_NETWORK_STANDBYMODECHANGED);
    EXPECT_TRUE(signalled & POWERMANAGERL2TEST_NETWORK_STANDBYMODECHANGED);

    // Verify state persistence
    bool standbyMode1;
    status = PowerManagerPlugin->GetNetworkStandbyMode(standbyMode1);
    EXPECT_EQ(standbyMode, standbyMode1);
    EXPECT_EQ(status, Core::ERROR_NONE);
}
```

