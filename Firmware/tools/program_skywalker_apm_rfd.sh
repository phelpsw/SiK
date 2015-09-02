#!/bin/bash

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
./set_sreg.py --write --reset --cmd=ATS14=0 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS18=4 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS15=0 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS16=65535 /dev/ttyUSB0
./set_sreg.py --write --reset --cmd=ATS17=0 /dev/ttyUSB0

