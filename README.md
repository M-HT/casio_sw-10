# Casio SW-10

CASIO SW-10 is a [software synthesizer](https://en.wikipedia.org/wiki/Software_synthesizer) that was included in [Casio](https://en.wikipedia.org/wiki/Casio) LANA Lite MIDI program ([version 1.03](http://web.archive.org/web/20011122112757/www.casio.co.jp/lanalite/LanaSw10.exe "LanaSw10.exe") for Win9x only).

This project uses a decompiled version of the synthesizer to build some tools (like Linux ALSA driver).


The source code is released with [MIT license](https://spdx.org/licenses/MIT.html) (except decompression in *zextractfile* which uses [zlib license](https://spdx.org/licenses/Zlib.html)).

<hr/>

The projects consists of following parts:

* **conv2dll**
  * Program, which converts *VLSG.VXD* (the software synthesizer itself) to *VLSG.DLL* by only modifying the file headers, without changing the software synthesizer code.
  * The result is a 32-bit (x86) *DLL* usable on modern systems.
  * It's mainly useful to compare output with the decompiled version (*VLSG*).
* **VLSG**
  * Decompiled version of *VLSG.VXD* with the same interface as *VLSG.DLL*.
  * This allows using the software synthesizer on other *CPU* architectures (32-bit, 64-bit).
* **sw10_alsadrv**
  * Linux daemon which provides [ALSA](https://en.wikipedia.org/wiki/Advanced_Linux_Sound_Architecture) MIDI sequencer interface using *VLSG*.
  * It requires the Casio SW-10 ROM file *ROMSXGM.BIN*.
* **sw10_pcmtools**
  * Tools to convert [Standard MIDI File](https://www.midi.org/specifications-old/item/standard-midi-files-smf) to *PCM* (*WAV* or *RAW*) using *VLSG* or *VLSG.DLL*.
  * Also tools to compare the output of conversion tools - useful to verify the output of decompiled version against the original.
  * Tools require the Casio SW-10 ROM file *ROMSXGM.BIN*.
* **zextractfile**
  * Program to extract a file from PKWare Data Compression Library.
  * The installer for Casio LANA Lite MIDI program (*LanaSw10.exe*) is a [LHA](https://en.wikipedia.org/wiki/LHA_%28file_format%29) archive which can be unpacked using LHA or [7-Zip](https://www.7-zip.org/).
  * The files for Casio SW-10 are located in file *LANA3.Z* (in *LanaSw10.exe*) which is a PKWare Data Compression Library.
  * Among other files, *LANA3.Z* contains *ROMSXGM.BIN* and *VLSG.VXD*.
  * To extract the ROM file from *LANA3.Z* use command: ```./zextractfile LANA3.Z ROMSXGM.BIN```
