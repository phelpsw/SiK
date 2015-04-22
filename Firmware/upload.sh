#!/bin/bash

#BANK_REQ="rfd900u"

if [ $# -eq 0 ]; then
	BOARD=rfd900a
else
	BOARD=$1
fi
echo "Making binary for $BOARD"

make BOARDS=$BOARD
if [ "$?" -ne "0" ]; then
	exit 1
fi

make BOARDS=$BOARD install
cat dst/bootloader~${BOARD}~915.hex dst/radio~${BOARD}.ihx > dst/${BOARD}Combined.hex
echo Created dst/${BOARD}Combined.hex
if [[ "$?" -eq "0" && $# -eq 2 ]]; then
	for PORT in $(ls /dev/tty.usbserial-*); do
        tools/uploader.py --port $PORT dst/radio~$BOARD.ihx &
	done
fi
