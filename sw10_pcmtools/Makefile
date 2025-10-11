all: sw10_pcmconvert

sw10_pcmconvert: sw10_pcmtools.c midi_loader.c midi_loader.h ../VLSG/VLSG.c ../VLSG/VLSG.h
	$(CC) -O2 -Wall -DPCM_TOOL=PCM_CONVERT_INTERNAL -o sw10_pcmconvert sw10_pcmtools.c midi_loader.c ../VLSG/VLSG.c -I../VLSG

.PHONY: clean
clean:
	rm -f sw10_pcmconvert
