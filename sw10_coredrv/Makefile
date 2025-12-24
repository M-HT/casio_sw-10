all: sw10_coredrv

sw10_coredrv: sw10_coredrv.c ../VLSG/VLSG.c ../VLSG/VLSG.h
	$(CC) -O2 -Wall -o sw10_coredrv sw10_coredrv.c ../VLSG/VLSG.c -I../VLSG -lpthread -framework CoreMIDI -framework CoreFoundation -framework AudioToolbox

.PHONY: clean
clean:
	rm -f sw10_coredrv
