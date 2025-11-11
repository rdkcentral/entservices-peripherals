#!/bin/bash

# Remove the 'sample' directory if it exists
rm -rf rdkservices

# Clone the specific branch from the GitHub repository
git clone -b topic/RDK-57171-Latest https://github.com/rdkcentral/entservices-inputoutput.git

# Change directory to 'demo/mock'
cd entservices-inputoutput/L2HalMock

# Remove the 'build.sh' file if it exists
rm -rf build.sh
