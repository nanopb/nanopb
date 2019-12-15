#!/bin/bash

BINARY=$1
shift
ARGS=$*

test X$OPENOCD_BOARD == X && export OPENOCD_BOARD=board/stm32vldiscovery.cfg

openocd -f $OPENOCD_BOARD \
    -c "init" -c "arm semihosting enable" \
    -c "arm semihosting_cmdline $ARGS" \
    -c "program $BINARY verify reset" \
    -c "reset run" 2>openocd.log

