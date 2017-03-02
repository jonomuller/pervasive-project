#!/bin/bash

echo "lets clean"
make clean
echo "Lets build newprocess"
make rssi-test-send.bin
echo "Lets flash that nonsense"
../flasher/dslite.sh -c ../flasher/user_files/configs/cc2650f128.ccxml rssi-test-send.elf
