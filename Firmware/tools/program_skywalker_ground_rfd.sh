#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Requires node id as argument"
    exit -1
fi

node_id=$1

if [[ $node_id -eq 0 || $node_id -ge 65535 ]]; then
    echo "Node id must be between 1 and 65534"
    exit -1
fi

./set_sreg.py --write --reset --cmd=ATS1=57 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS2=64 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS3=42 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS4=20 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS6=1 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS7=1 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS8=902000 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS9=928000 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS10=50 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS11=100 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS12=0 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS13=0 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS14=1 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS18=4 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS15=$node_id /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS16=0 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS17=0 /dev/ttyUSB0

