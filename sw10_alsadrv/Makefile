all: sw10_alsadrv

sw10_alsadrv: sw10_alsadrv.c ../VLSG/VLSG.c ../VLSG/VLSG.h
	$(CC) -O2 -Wall -o sw10_alsadrv sw10_alsadrv.c ../VLSG/VLSG.c -I../VLSG -lasound -lpthread

.PHONY: clean
clean:
	rm -f sw10_alsadrv
