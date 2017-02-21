#!/bin/bash

# directory of script
script=$(readlink -f "$0")
scriptDir=$(dirname "$script")

# default mode and user options
MODE=flash
USEROPTIONS=$@

# list available modes
if [ "$1" == "--listMode" ]; then
	echo 
	echo "Available Modes for UniFlash CLI:"
	echo "  * flash [default] - on-chip flash programming"
	echo "  * memory          - export memory to a file"
	echo "  * load            - simple loader [use default options]"
	echo
	
	exit 0
fi

# custom mode
if [ "$#" -ne 0 ]; then
	if [ $1 = "--mode" ]; then
		MODE=$2;
		USEROPTIONS=${@:3}
	fi
fi

# run the command line batch file
if [ "$#" -eq 0 ]; then
	eval "$scriptDir"/ccs_base/DebugServer/bin/DSLite $MODE -c user_files/configs/cc2650f128.ccxml -l user_files/settings/generated.ufsettings
else
	eval "$scriptDir"/ccs_base/DebugServer/bin/DSLite $MODE $USEROPTIONS
fi