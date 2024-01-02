#!/bin/sh

picocom --init "$(date +'time %Y-%m-%d %H:%M:%S'; echo '\r' )" -b 115200 /dev/tty.usbserial-A506P9FH
