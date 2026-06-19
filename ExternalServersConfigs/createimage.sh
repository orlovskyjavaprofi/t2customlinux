#!/bin/bash
echo "Creating image"
./t2 build-target --cfg system 2>&1 | tee build.log
