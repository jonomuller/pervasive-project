#!/bin/bash

echo "lets clean"
make clean
echo "Lets build newprocess"
make watch.bin
echo "Lets flash that nonsense"
../flasher/dslite.sh -c ../flasher/user_files/configs/cc2650f128.ccxml watch.elf
