#!/bin/bash
dmesg -c > /dev/null
make
./reload.sh
aplay -D plughw:2,0 test_s32.wav &
APLAY_PID=$!
sleep 2
kill -9 $APLAY_PID
dmesg | tail -n 50
