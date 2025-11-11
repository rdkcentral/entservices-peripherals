#!/bin/bash
##########################################################################
# If not stated otherwise in this file or this component's Licenses.txt
# file the following copyright and licenses apply:
#
# Copyright 2016 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
##########################################################################
#
set -x

source $PWD/../../../../env.sh

export LDFLAGS="`pkg-config --libs libsafec`"
export USE_DBUS=y
export CXX=g++
export RMFLAGS=""
export STANDALONE_BUILD_ENABLED=y
# export DSMGR_LOGGER_ENABLED=ON
export CFLAGS="${CFLAGS} -DDSMGR_LOGGER_ENABLED"
export CFLAGS="${CFLAGS} -DDS_AUDIO_SETTINGS_PERSISTENCE"
# export LDFLAGS="${LDFLAGS} -lrdkloggers -lds-hal"

# LIB_PATH="/home/kishore/PROJECT/Input_output/entservices-inputoutput/L2HalMock/workspace/deps/rdk/devicesettings/install/lib"
# export LDFLAGS="-L$LIB_PATH -lds-hal"

# Define macro for DSMGR logging
# export CFLAGS="${CFLAGS} -DDSMGR_LOGGER_ENABLED"

# Define macro with quoted string
# export CFLAGS="${CFLAGS} -DRDK_DSHAL_NAME=\\\"libds-hal.so\\\""

# Link rdkloggers library
# export LDFLAGS="${LDFLAGS} -lrdkloggers"

make
if [ $? -ne 0 ] ; then
  echo Build Failed
  exit 1
else
  echo Build Success
  exit 0
fi