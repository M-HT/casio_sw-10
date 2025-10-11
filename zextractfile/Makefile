all: zextractfile

zextractfile: zextractfile.c blast/blast.c blast/blast.h
	$(CC) -O2 -Wall -o zextractfile zextractfile.c blast/blast.c

.PHONY: clean
clean:
	rm -f zextractfile
