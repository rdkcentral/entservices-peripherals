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
# export CXX=g++

# Set the path to the library
# LIB_PATH=$DS_LIB_PATH
# export LDFLAGS="-L$DS_LIB_PATH -lds -ldshalsrv -lds-hal"
# export LD_LIBRARY_PATH=$LIB_PATH:$LD_LIBRARY_PATH

# LIB_PATH="/__w/entservices-inputoutput/entservices-inputoutput/entservices-inputoutput/L2HalMock/install/lib"
# Export LDFLAGS with the path and the library
export LDFLAGS="-L$WPEFRAMEWORK_LIB -lWPEFrameworkPowerController -lWPEFrameworkCore -lWPEFrameworkCOM -lWPEFrameworkMessaging"
make 
if [ $? -ne 0 ] ; then
  echo iarmmgr Build Failed
  echo "Kishore print"
  cd $DS_LIB_PATH
  ls
  exit 1
else
  echo iarmmgr Build Success
  exit 0
fi