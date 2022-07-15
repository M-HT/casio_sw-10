/**
 *
 *  Copyright (C) 2022 Roman Pauer
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of
 *  this software and associated documentation files (the "Software"), to deal in
 *  the Software without restriction, including without limitation the rights to
 *  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is furnished to do
 *  so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 */

#define PCM_CONVERT_INTERNAL 1
#define PCM_CONVERT_DLL 2
#define PCM_COMPARE_DLL_INTERNAL 3
#define PCM_COMPARE_DLL_EXTERNAL 4

#ifdef PCM_TOOL
#if PCM_TOOL != PCM_CONVERT_INTERNAL && PCM_TOOL != PCM_CONVERT_DLL && PCM_TOOL != PCM_COMPARE_DLL_INTERNAL && PCM_TOOL != PCM_COMPARE_DLL_EXTERNAL
#error Wrong PCM tool selected
#endif
#else
#error No PCM tool selected
#endif

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "midi_loader.h"

#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
#include "dll_loader.h"
#endif

#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
#include "VLSG.h"
#else
enum ParameterType
{
    PARAMETER_OutputBuffer  = 1,
    PARAMETER_ROMAddress    = 2,
    PARAMETER_Frequency     = 3,
    PARAMETER_Polyphony     = 4,
    PARAMETER_Effect        = 5,
};
#endif

#if (defined(__WIN32__) || defined(__WINDOWS__)) && !defined(_WIN32)
#define _WIN32
#endif

#ifndef _WIN32
    #include <sys/types.h>
    #include <dirent.h>
#endif

#ifdef __BYTE_ORDER

#if (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define BIG_ENDIAN_BYTE_ORDER
#else
#undef BIG_ENDIAN_BYTE_ORDER
#endif

#elif !defined(_WIN32)

#include <endian.h>
#if (__BYTE_ORDER == __BIG_ENDIAN)
#define BIG_ENDIAN_BYTE_ORDER
#else
#undef BIG_ENDIAN_BYTE_ORDER
#endif

#endif


static const char *arg_input = NULL;
static const char *arg_output = NULL;
static const char *arg_dll = "VLSG.DLL";
static const char *arg_rom = "ROMSXGM.BIN";
static const char *arg_exttool = NULL;
static int wav_to_file = 1;

#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
static void *hVLSG;
static VLSG_Functions dll_functions;
#endif
static void *rom_address;
static uint32_t outbuf_counter;
static uint32_t current_time;

static unsigned int timediv;
static midi_event_info *midi_events;

static int frequency, polyphony, reverb_effect;

static uint8_t midi_buffer[65536];
static uint8_t *midi_buf[16];
#if PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
static uint8_t midi_buffer2[65536];
static uint8_t *midi_buf2[16];
#endif
static unsigned int bytes_per_call, samples_per_call;


static inline void WRITE_LE_UINT16(uint8_t *ptr, uint16_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
}

static inline void WRITE_LE_UINT32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
    ptr[2] = (value >> 16) & 0xff;
    ptr[3] = (value >> 24) & 0xff;
}


static uint32_t VLSG_GetTime(void)
{
    return current_time;
}

static void *load_rom_file(const char *romname)
{
    FILE *f;
    void *mem;

    f = fopen(romname, "rb");
    if (f == NULL)
    {
#ifndef _WIN32
        char *romnamecopy, *slash, *filename;
        DIR *dir;
        struct dirent *entry;

        romnamecopy = strdup(romname);
        if (romnamecopy == NULL) return NULL;

        slash = strrchr(romnamecopy, '/');
        if (slash != NULL)
        {
            filename = slash + 1;
            if (slash != romnamecopy)
            {
                *slash = 0;
                dir = opendir(romnamecopy);
                *slash = '/';
            }
            else
            {
                dir = opendir("/");
            }
        }
        else
        {
            filename = romnamecopy;
            dir = opendir(".");
        }

        if (dir == NULL)
        {
            free(romnamecopy);
            return NULL;
        }

        while (1)
        {
            entry = readdir(dir);
            if (entry == NULL) break;

            if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_REG && entry->d_type != DT_LNK) continue;

            if (0 != strcasecmp(filename, entry->d_name)) continue;

            strcpy(filename, entry->d_name);
            break;
        };

        closedir(dir);

        if (entry == NULL)
        {
            free(romnamecopy);
            return NULL;
        }

        f = fopen(romnamecopy, "rb");

        free(romnamecopy);

        if (f == NULL)
        {
            return NULL;
        }
#else
        return NULL;
#endif
    }

    mem = malloc(2 * 1024 * 1024);
    if (mem == NULL)
    {
        fclose(f);
        return NULL;
    }

    if (fread(mem, 1, 2 * 1024 * 1024, f) != 2 * 1024 * 1024)
    {
        free(mem);
        fclose(f);
        return NULL;
    }

    return mem;
}

static void lsgWrite(uint8_t *event, unsigned int length)
{
    uint8_t event_time[4];

    WRITE_LE_UINT32(event_time, VLSG_GetTime());

    for (; length != 0; length--,event++)
    {
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        dll_functions.VLSG_AddMidiData(event_time, 4);
        dll_functions.VLSG_AddMidiData(event, 1);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
        VLSG_AddMidiData(event_time, 4);
        VLSG_AddMidiData(event, 1);
#endif
    }
}


static void usage(const char *progname)
{
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_CONVERT_INTERNAL
    static const char basename[] = "sw10_pcmconvert";
#endif
#if PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    static const char basename[] = "sw10_pcmcompare";
#endif

    if (progname == NULL)
    {
        progname = basename;
    }
    else
    {
        const char *slash;

        slash = strrchr(progname, '/');
        if (slash != NULL)
        {
            progname = slash + 1;
        }

#ifdef _WIN32
        slash = strrchr(progname, '\\');
        if (slash != NULL)
        {
            progname = slash + 1;
        }
#endif
    }

    printf(
        "%s - CASIO Software Sound Generator SW-10 tool\n"
        "Usage: %s [OPTIONS]...\n"
        "  -i PATH  Input path (path to .mid)\n"
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_CONVERT_INTERNAL
        "  -s       Output raw data do stdout\n"
        "  -o PATH  Output path (path to .wav)\n"
#endif
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        "  -d PATH  Dll path (path to VLSG.DLL)\n"
#endif
#if PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        "  -t PATH  External compare tool path\n"
#endif
        "  -r PATH  Rom path (path to ROMSXGM.BIN)\n"
        "  -f NUM   Frequency (0 = 11025 Hz, 1 = 22050 Hz, 2 = 44100 Hz)\n"
        "  -p NUM   Polyphony (0 = 24 voices, 1 = 32 voices, 2 = 48 voices, 3 = 64 voices)\n"
        "  -e NUM   Reverb effect (0 = off, 1 = reverb 1, 2 = reverb 2)\n"
        "  -h       Help\n",
        basename,
        progname
    );
    exit(1);
}

int main(int argc, char *argv[])
{
    int return_value;

    // frequency: 0 = 11025 Hz, 1 = 22050 Hz, 2 = 44100 Hz
    frequency = 2;

    // polyphony: 0 = 24 voices, 1 = 32 voices, 2 = 48 voices, 3 = 64 voices
    polyphony = 3;

    // reverb effect: 0 = off, 1 = reverb 1, 2 = reverb 2
    reverb_effect = 0;

    // parse arguments
    if (argc > 1)
    {
        int i, j;
        for (i = 1; i < argc; i++)
        {
            if (argv[i][0] == '-' && argv[i][1] != 0 && argv[i][2] == 0)
            {
                switch (argv[i][1])
                {
                    case 'i': // input
                        if ((i + 1) < argc)
                        {
                            i++;
                            arg_input = argv[i];
                        }
                        break;
                    case 'o': // output
                        if ((i + 1) < argc)
                        {
                            i++;
                            arg_output = argv[i];
                        }
                        break;
                    case 'd': // dll
                        if ((i + 1) < argc)
                        {
                            i++;
                            arg_dll = argv[i];
                        }
                        break;
                    case 'r': // rom
                        if ((i + 1) < argc)
                        {
                            i++;
                            arg_rom = argv[i];
                        }
                        break;
                    case 's': // stdout
                        wav_to_file = 0;
                        break;
                    case 't': // tool
                        if ((i + 1) < argc)
                        {
                            i++;
                            arg_exttool = argv[i];
                        }
                        break;
                    case 'f': // frequency
                        if ((i + 1) < argc)
                        {
                            i++;
                            j = atoi(argv[i]);
                            if (j >= 0 && j <= 2)
                            {
                                frequency = j;
                            }
                        }
                        break;
                    case 'p': // polyphony
                        if ((i + 1) < argc)
                        {
                            i++;
                            j = atoi(argv[i]);
                            if (j >= 0 && j <= 3)
                            {
                                polyphony = j;
                            }
                        }
                        break;
                    case 'e': // reverb effect
                        if ((i + 1) < argc)
                        {
                            i++;
                            j = atoi(argv[i]);
                            if (j >= 0 && j <= 2)
                            {
                                reverb_effect = j;
                            }
                        }
                        break;
                    case 'h': // help
                        usage(argv[0]);
                    default:
                        break;
                }
            }
            else if (strcmp(argv[i], "--help") == 0)
            {
                usage(argv[0]);
            }
        }
    }

    if (arg_input == NULL)
    {
        fprintf(stderr, "no input file\n");
        usage(argv[0]);
    }
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_CONVERT_INTERNAL
    if (wav_to_file && arg_output == NULL)
    {
        fprintf(stderr, "no output file\n");
        usage(argv[0]);
    }
#endif
#if PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    if (arg_exttool == NULL)
    {
        fprintf(stderr, "no external compare tool\n");
        usage(argv[0]);
    }
#endif

#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    // load dll
    hVLSG = load_vlsg_dll(arg_dll, &dll_functions);
    if (hVLSG == NULL)
    {
        fprintf(stderr, "error loading dll\n");
        return 2;
    }
#endif

    // load ROM file
    rom_address = load_rom_file(arg_rom);
    if (rom_address == NULL)
    {
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        free_vlsg_dll(hVLSG);
#endif
        fprintf(stderr, "error loading ROM file\n");
        return 3;
    }

    // load MIDI file
    if (load_midi_file(arg_input, &timediv, &midi_events))
    {
        free(rom_address);
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        free_vlsg_dll(hVLSG);
#endif
        fprintf(stderr, "error loading MIDI file\n");
        return 4;
    }


    return_value = 0;


    // set frequency
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_SetParameter(PARAMETER_Frequency, frequency);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_SetParameter(PARAMETER_Frequency, frequency);
#endif

    // set polyphony
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);
#endif

    // set reverb effect
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);
#endif

    // set address of ROM file
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_SetParameter(PARAMETER_ROMAddress, (uintptr_t)rom_address);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_SetParameter(PARAMETER_ROMAddress, (uintptr_t)rom_address);
#endif

    // set output buffer
    outbuf_counter = 0;
    memset(midi_buffer, 0, 65536);
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)midi_buffer);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL
    VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)midi_buffer);
#endif
#if PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    memset(midi_buffer2, 0, 65536);
    VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)midi_buffer2);
#endif

    // split output buffer to 16 subbuffers
    samples_per_call = 256 << frequency;
    bytes_per_call = 4 * samples_per_call;
    {
        int i;
        for (i = 0; i < 16; i++)
        {
            midi_buf[i] = &(midi_buffer[i * bytes_per_call]);
#if PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
            midi_buf2[i] = &(midi_buffer2[i * bytes_per_call]);
#endif
        }
    }

    // set function GetTime
    current_time = 0;
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_SetFunc_GetTime(&VLSG_GetTime);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_SetFunc_GetTime(&VLSG_GetTime);
#endif


    // start playback
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_PlaybackStart();
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_PlaybackStart();
#endif


    // play midi
    {
        unsigned int num_calls, remaining_events;
        midi_event_info *cur_event;

#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_CONVERT_INTERNAL
        FILE *fout;

        if (wav_to_file)
        {
            uint8_t wav_header[44];
            uint8_t *header_ptr;

            fout = fopen(arg_output, "wb");
            if (fout == NULL)
            {
                free_midi_data(midi_events);
                free(rom_address);
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
                free_vlsg_dll(hVLSG);
#endif
                fprintf(stderr, "error opening output file\n");
                return 5;
            }

            // wav header
            header_ptr = wav_header;
            WRITE_LE_UINT32(header_ptr, 0x46464952);        // "RIFF" tag
            WRITE_LE_UINT32(header_ptr + 4, 36);            // RIFF length - filled later
            WRITE_LE_UINT32(header_ptr + 8, 0x45564157);    // "WAVE" tag
            header_ptr += 12;

            // fmt chunk
            WRITE_LE_UINT32(header_ptr, 0x20746D66);    // "fmt " tag
            WRITE_LE_UINT32(header_ptr + 4, 16);        // chunk length
            header_ptr += 8;

            // PCMWAVEFORMAT structure
            WRITE_LE_UINT16(header_ptr, 1);                             // wFormatTag - 1 = PCM
            WRITE_LE_UINT16(header_ptr + 2, 2);                         // nChannels - 2 = stereo
            WRITE_LE_UINT32(header_ptr + 4, 11025 << frequency);        // nSamplesPerSec
            WRITE_LE_UINT32(header_ptr + 8, 4 * (11025 << frequency));  // nAvgBytesPerSec
            WRITE_LE_UINT16(header_ptr + 12, 4);                        // nBlockAlign
            WRITE_LE_UINT16(header_ptr + 14, 16);                       // wBitsPerSample
            header_ptr += 16;

            // data chunk
            WRITE_LE_UINT32(header_ptr, 0x61746164);    // "data" tag
            WRITE_LE_UINT32(header_ptr + 4, 0);         // chunk length - filled later
            header_ptr += 8;

            if (fwrite(wav_header, 1, 44, fout) != 44)
            {
                fprintf(stderr, "error writing to output file\n");
                return 6;
            }
        }
        else
        {
            fout = stdout;
        }
#endif

#if PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        FILE *fpipe;

        char command[8192];

        sprintf(command, "%s -i \"%s\" -d \"%s\" -r \"%s\" -s -f %i -p %i -e %i", arg_exttool, arg_input, arg_dll, arg_rom, frequency, polyphony, reverb_effect);

#ifdef _WIN32
        fpipe = _popen(command, "rb");
#else
        fpipe = popen(command, "r");
#endif
        if (fpipe == NULL)
        {
            fprintf(stderr, "error running external convert tool\n");
            return 7;
        }
#endif

        num_calls = 0;
        remaining_events = midi_events[0].len;
        cur_event = midi_events + 1;
        while (current_time < midi_events[0].time + 112)
        {
            uint32_t next_time;
            num_calls++;

            next_time = ((num_calls * 256 + 128) * (uint64_t)1000) / 11025;
            while ((remaining_events > 0) && (cur_event->time <= next_time))
            {
                current_time = cur_event->time;
                if (current_time == 0) current_time = 1; // !!! events with zero timestamp are ignored

                if (cur_event->len <= 8)
                {
                    if (cur_event->data[0] != 0xff) // skip meta events
                    {
                        lsgWrite(cur_event->data, cur_event->len);
                    }
                }
                else
                {
                    if (cur_event->sysex[0] != 0xff) // skip meta events
                    {
                        lsgWrite(cur_event->sysex, cur_event->len);
                    }
                }

                cur_event++;
                remaining_events--;
            }

            current_time = next_time;

#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
            dll_functions.VLSG_FillOutputBuffer(outbuf_counter);
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
            VLSG_FillOutputBuffer(outbuf_counter);
#endif

#if PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
            if (fread(midi_buf2[outbuf_counter & 0x0f], 1, bytes_per_call, fpipe) != bytes_per_call)
            {
                fprintf(stderr, "error reading from pipe\n");
                return_value = 8;
                break;
            }
#endif


#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_CONVERT_INTERNAL
#ifdef BIG_ENDIAN_BYTE_ORDER
            // swap values to little-endian
            {
                int i;
                for (i = 0; i < bytes_per_call; i += 2)
                {
                    uint8_t value;
                    value = midi_buf[outbuf_counter & 0x0f][i];
                    midi_buf[outbuf_counter & 0x0f][i] = midi_buf[outbuf_counter & 0x0f][i + 1];
                    midi_buf[outbuf_counter & 0x0f][i + 1] = value;
                }
            }
#endif

            if (fwrite(midi_buf[outbuf_counter & 0x0f], 1, bytes_per_call, fout) != bytes_per_call)
            {
                fprintf(stderr, "error writing to output file\n");
                return_value = 6;
                break;
            }
#endif

#if PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
            if (0 != memcmp(midi_buf[outbuf_counter & 0x0f], midi_buf2[outbuf_counter & 0x0f], bytes_per_call))
            {
                fprintf(stderr, "Output different: %i (%i)\n", outbuf_counter, bytes_per_call);

#if 0
                int i;
                for (i = 0; i < bytes_per_call; i++)
                {
                    fprintf(stderr, "%i - 0x%x - 0x%x\n", i, midi_buf[outbuf_counter & 0x0f][i], midi_buf2[outbuf_counter & 0x0f][i]);
                }
#endif

                return_value = -1;
                break;
            }
#endif

            outbuf_counter++;
        }

#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_CONVERT_INTERNAL
        if (wav_to_file)
        {
            uint8_t chunk_length[4];

            // RIFF length
            WRITE_LE_UINT32(chunk_length, 36 + num_calls * bytes_per_call);
            fseek(fout, 4, SEEK_SET);
            if (fwrite(chunk_length, 4, 1, fout) != 1)
            {
                fprintf(stderr, "error writing to output file\n");
                return 6;
            }

            // data chunk length
            WRITE_LE_UINT32(chunk_length, num_calls * bytes_per_call);
            fseek(fout, 40, SEEK_SET);
            if (fwrite(chunk_length, 4, 1, fout) != 1)
            {
                fprintf(stderr, "error writing to output file\n");
                return 6;
            }

            fclose(fout);
        }
        else
        {
            fflush(fout);
            fout = NULL;
        }
#endif

#if PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
        pclose(fpipe);
#endif
    }


    // stop playback
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    dll_functions.VLSG_PlaybackStop();
#endif
#if PCM_TOOL == PCM_CONVERT_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL
    VLSG_PlaybackStop();
#endif

    // free MIDI file, ROM file, dll
    free_midi_data(midi_events);
    free(rom_address);
#if PCM_TOOL == PCM_CONVERT_DLL || PCM_TOOL == PCM_COMPARE_DLL_INTERNAL || PCM_TOOL == PCM_COMPARE_DLL_EXTERNAL
    free_vlsg_dll(hVLSG);
#endif

    return return_value;
}

