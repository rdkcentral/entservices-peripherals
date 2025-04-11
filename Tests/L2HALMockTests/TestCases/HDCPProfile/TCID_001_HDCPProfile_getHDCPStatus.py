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

# Testcase ID : TCID001_DS_HDCPProfile_getHdcpStatus
# Testcase Description :Returns HDCP-related data.
# hdcpReason Argument Values
#
# 0: HDMI cable is not connected or rx sense status is off
# 1: Rx device is connected with power ON state, and HDCP authentication is not initiated
# 2: HDCP success
# 3: HDCP authentication failed after multiple retries
# 4: HDCP authentication in progress
# 5: HDMI video port is disabled.

from Utilities import Utils, ReportGenerator
from HDCPProfile import HDCPProfileApis

print("TC Description - Returns HDCP-related data hdcpReason Argument Values--- 0: HDMI cable is not connected or rx sense status is off, 1: Rx device is connected with power ON state, and HDCP authentication is not initiated, 2: HDCP success, 3: HDCP authentication failed after multiple retries, 4: HDCP authentication in progress , 5: HDMI video port is disabled.")
print("---------------------------------------------------------------------------------------------------------------------------")
# store the expected output response
expected_output_response = '{"jsonrpc":"2.0","id":42,"result":{"HDCPStatus":{"isConnected":false,"isHDCPCompliant":false,"isHDCPEnabled":true,"hdcpReason":0,"supportedHDCPVersion":"1.4","receiverHDCPVersion":"1.4","currentHDCPVersion":"1.4"},"success":true}}'

# send the curl command and fetch the output json response
curl_response = Utils.send_curl_command(HDCPProfileApis.get_hdcpstatus)
if curl_response:
     Utils.info_log("curl command to get hdcp status is sent from the test runner")
else:
     Utils.error_log("curl command invoke failed for {}" .format(HDCPProfileApis.get_hdcpstatus))

print("---------------------------------------------------------------------------------------------------------------------------")
# compare both expected and received output responses
if str(curl_response) == str(expected_output_response):
    status = 'Pass'
    message = 'Output response is matching with expected one. The HDCP Status ' \
              'is obtained in output response'
else:
    status = 'Fail'
    message = 'Output response is different from expected one'

# generate logs in terminal
tc_id = 'TCID001_DS_HDCPProfile_getHdcpStatus'
print("Testcase ID : " + tc_id)
print("Testcase Output Response : " + curl_response)
print("Testcase Status : " + status)
print("Testcase Message : " + message)
print("")

if status == 'Pass':
    ReportGenerator.passed_tc_list.append(tc_id)
else:
    ReportGenerator.failed_tc_list.append(tc_id)
# push the testcase execution details to report file
ReportGenerator.append_test_results_to_csv(tc_id, curl_response, status, message)
