#!/bin/bash

# Change directory to 'Mock'
 cd FLASK-for-Hal-Mock
 
# List files in the directory
ls

# Move specified files one level up
mv dependencies_auto.py ..
mv build.sh ..
mv peru.yaml ..

# Return to the parent directory
cd ..
