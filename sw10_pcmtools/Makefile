all: sw10_pcmconvert

all_dll: sw10_pcmconvert_dll sw10_pcmcompare_dll sw10_pcmcompare_dll_external

sw10_pcmconvert: sw10_pcmtools.c midi_loader.c midi_loader.h ../VLSG/VLSG.c ../VLSG/VLSG.h
	gcc -O2 -Wall -DPCM_TOOL=PCM_CONVERT_INTERNAL -o sw10_pcmconvert sw10_pcmtools.c midi_loader.c ../VLSG/VLSG.c -I../VLSG

sw10_pcmconvert_dll: sw10_pcmtools.c pe_helper.c pe_helper.h pe_loader.c pe_loader.h midi_loader.c midi_loader.h dll_loader.c dll_loader.h
	gcc -m32 -O2 -Wall -DPCM_TOOL=PCM_CONVERT_DLL -o sw10_pcmconvert_dll sw10_pcmtools.c pe_helper.c pe_loader.c midi_loader.c dll_loader.c

sw10_pcmcompare_dll: sw10_pcmtools.c pe_helper.c pe_helper.h pe_loader.c pe_loader.h midi_loader.c midi_loader.h dll_loader.c dll_loader.h ../VLSG/VLSG.c ../VLSG/VLSG.h
	gcc -m32 -O2 -Wall -DPCM_TOOL=PCM_COMPARE_DLL_INTERNAL -o sw10_pcmcompare_dll sw10_pcmtools.c pe_helper.c pe_loader.c midi_loader.c dll_loader.c ../VLSG/VLSG.c -I../VLSG

sw10_pcmcompare_dll_external: sw10_pcmtools.c pe_helper.c pe_helper.h pe_loader.c pe_loader.h midi_loader.c midi_loader.h dll_loader.c dll_loader.h
	gcc -m32 -O2 -Wall -DPCM_TOOL=PCM_COMPARE_DLL_EXTERNAL -o sw10_pcmcompare_dll_external sw10_pcmtools.c pe_helper.c pe_loader.c midi_loader.c dll_loader.c

.PHONY: clean
clean:
	rm -f sw10_pcmconvert sw10_pcmconvert_dll sw10_pcmcompare_dll sw10_pcmcompare_dll_external
