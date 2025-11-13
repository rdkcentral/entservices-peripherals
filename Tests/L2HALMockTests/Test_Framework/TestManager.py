#** *****************************************************************************
# *
# * If not stated otherwise in this file or this component's LICENSE file the
# * following copyright and licenses apply:
# *
# * Copyright 2024 RDK Management
# *
# * Licensed under the Apache License, Version 2.0 (the "License");
# * you may not use this file except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *
# http://www.apache.org/licenses/LICENSE-2.0
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *
#* ******************************************************************************

import sys
import datetime
import time
sys.path.append("../TestCases")
from FrontPanel import FPUtils
from Utilities import Utils, ReportGenerator

sys.path.append("Utilities")
sys.path.append("../TestCases/FrontPanel")

# Define the build name of current build being tested
build_name = "23Q4-HAL-MOCK-TEST"

# Get the date and time of execution
now = datetime.datetime.now()

# clear the previous test results
ReportGenerator.passed_tc_list.clear()
ReportGenerator.failed_tc_list.clear()

print("")
print("Inside TestManager.py : Initializing Python Test Framework......................!")
print("")
argument = sys.argv[1:3]
result = ' '.join(argument)
if len(sys.argv) >= 2:
    second_argument = sys.argv[1]
    print(f"Two plugins are given: {second_argument}")
elif len(sys.argv) >= 3:
    third_argument = sys.argv[2]
    print(f"three plugins are given:  {third_argument}")
elif len(sys.argv) >= 4:
    forth_argument = sys.argv[3]
    print(f"4 plugins are given: {argument}")
else:
    print("No second argument provided.")

print("GIVEN PLUGIN NAMES/TC NAME {}" .format(argument))
# Pushing the initial configuration (cec network data &
# Pushing the initial configuration (cec network data & api override data) to Flask
#Utils.initialize_flask()
print("")

# Activating the HdmiCecSource plugin using controller1.activate curl command
#CecUtils.activate_cec()
#time.sleep(5)

print("")
print("***** Test Execution Starts *****")
print("")

tc_name = result
flag = 0
track = 0

list_of_plugins = ['FrontPanel']

plugin_name = []

for each in list_of_plugins:
    if each in argument or each in result:
        plugin_name.append(each)
        track = track + 1

if track == 2 or track > 2:
    Utils.info_log("Executing Testcases for the given plugins")
    print("Plugins changed does not require FLASK HTTP Server for execution and build")
  

    if "FrontPanel" in plugin_name:
            #Utils.initialize_flask()
        print("Executing Test Framework without HTTP server and Websocket")
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_001_DS_FrontPanel_getBrightness
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_011_DS_FrontPanel_setPreferences  #commented as its descoped in source code
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_002_DS_FrontPanel_getPreferences
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_003_DS_FrontPanel_is24HourClock
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_014_DS_FrontPanel_setClockBrightness #commented as its descoped in source code
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_004_DS_FrontPanel_getClockBrightness #commented as its descoped in source code
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_005_DS_FrontPanel_powerLedOff
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_006_DS_FrontPanel_powerLedOn
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_007_DS_FrontPanel_set24HourClock
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_008_DS_FrontPanel_setBlink
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_009_DS_FrontPanel_setClockTestPattern
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_010_DS_FrontPanel_setLED
        print("\033[32m------########################################################################################################################---------.\033[0m")    
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_013_DS_FrontPanel_setBrightness
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_015_DS_FrontPanel_getSetbrightnesscombination
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_012_DS_FrontPanel_getFrontPanelLights
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_017_DS_FrontPanel_sendEventsSimulation
        import TCID_016_DS_FrontPanel_Deactivate
    else:
        print("Skipping FrontPanel as no changes are added")
else:
    track = 1

if "FrontPanel" in argument or "FrontPanel" in result:
    if track < 2:
        flag = 3
        Utils.info_log("Executing FrontPanel Test suite")
        print("\033[32m------########################################################################################################################---------.\033[0m")

        #Utils.initialize_flask()
        print("Executing Test Framework without HTTP server and Websocket")
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_001_DS_FrontPanel_getBrightness
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_011_DS_FrontPanel_setPreferences  #commented as its descoped in source code
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_002_DS_FrontPanel_getPreferences
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_003_DS_FrontPanel_is24HourClock
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_014_DS_FrontPanel_setClockBrightness #commented as its descoped in source code
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_004_DS_FrontPanel_getClockBrightness #commented as its descoped in source code
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_005_DS_FrontPanel_powerLedOff
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_006_DS_FrontPanel_powerLedOn
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_007_DS_FrontPanel_set24HourClock
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_008_DS_FrontPanel_setBlink
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_009_DS_FrontPanel_setClockTestPattern
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_010_DS_FrontPanel_setLED
        print("\033[32m------########################################################################################################################---------.\033[0m")    
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_013_DS_FrontPanel_setBrightness
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_015_DS_FrontPanel_getSetbrightnesscombination
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_012_DS_FrontPanel_getFrontPanelLights
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_017_DS_FrontPanel_sendEventsSimulation
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_018_DS_FrontPanel_negative
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_020_DS_FrontPanel_powerLed_invalid
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_021_DS_FrontPanel_LEDBrightnessNegative
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_022_DS_FrontPanel_24HrClock_invalid
        print("\033[32m------########################################################################################################################---------.\033[0m")
        import TCID_016_DS_FrontPanel_Deactivate
        
if argument == "DeviceSettings":
    flag = 3
    Utils.highlight_log("Executing DeviceSettings Test suite")
if argument == "Bluetooth":
    flag = 4
    Utils.highlight_log("Executing Bluetooth Test Suite")
if argument == "Wifi":
    flag = 5
    Utils.highlight_log("Executing Wifi Test Suite")
if argument == "HdmiInput":
    flag = 6
if argument == "all":
    flag = 15
    Utils.highlight_log("Executing Complete Test Suite for all plugins")
if "TCID" in result:
    flag = 0
    print(
        "\033[32m------########################################################################################################################---------.\033[0m")
    print("Execution of testcase {}".format(argument))
    __import__(tc_name)

if argument != "HdmiCecSource" or argument != "HdmiCecSink" or argument != "all":
    print("Execution of testcase {}" .format(argument))
if flag == 2 or flag == 15:
    Utils.info_log("Executed HdmiCecSinkTestcases")
if flag == 15:
    Utils.info_log("Executed Complete Test suite")
else:
    print("Executed TestManager")

print("***** Test Execution Ends *****")

# Generate a html report file with all testcase execution details
ReportGenerator.generate_html_report(build_name, now)
