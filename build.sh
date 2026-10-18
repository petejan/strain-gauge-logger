#!/bin/sh

arduino-cli --config-dir ~/embedded/sloeber-workspace/Arduino compile -v --libraries FlashStorage,zmodem -b adafruit:samd:adafruit_feather_m0:speed=8 . --build-path build --output-dir output
