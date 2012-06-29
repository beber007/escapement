#!/bin/sh
# Fichier "flash.sh"

if [ $# -ne 1 ]
then
    echo "Error in $0 - Invalid Argument Count"
    echo "Syntax: $0 input_file"
    exit
fi

sudo openocd -d0 -f openocd.cfg -c init -c targets -c "halt" \
-c "stm32f1x mass_erase 0" \
-c "flash write_image $1 0x08000000" \
-c "verify_image $1" -c "soft_reset_halt" 
