/**
 *
 *  Copyright (C) 2022-2024 Roman Pauer
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

#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <pwd.h>
#include <alsa/asoundlib.h>
#include "VLSG.h"

#ifdef PANDORA
#define secure_getenv __secure_getenv
#endif


#define ROMSIZE (2 * 1024 * 1024)


static const char midi_name[] = "CASIO SW-10";
static const char port_name[] = "CASIO SW-10 port";

static snd_seq_t *midi_seq;
static int midi_port_id;
static pthread_t midi_thread;
static snd_pcm_t *midi_pcm;
static volatile int midi_init_state;
static volatile int midi_event_written;

static int frequency, polyphony, reverb_effect, daemonize;
static const char *rom_filepath = "ROMSXGM.BIN";

static uint8_t *rom_address;
static uint32_t outbuf_counter;
static unsigned int bytes_per_call, samples_per_call;
static uint8_t midi_buffer[65536];
static uint8_t *midi_buf[16];

static struct timespec start_time;

#if defined(CLOCK_MONOTONIC_RAW)
    static clockid_t monotonic_clock_id;

    #define MONOTONIC_CLOCK_TYPE monotonic_clock_id
#else
    #define MONOTONIC_CLOCK_TYPE CLOCK_MONOTONIC
#endif


static uint32_t VLSG_GetTime(void)
{
    struct timespec _tp;

    clock_gettime(MONOTONIC_CLOCK_TYPE, &_tp);

    return ((_tp.tv_sec - start_time.tv_sec) * 1000) + ((_tp.tv_nsec - start_time.tv_nsec) / 1000000);
}


static void set_thread_scheduler(void) __attribute__((noinline));
static void set_thread_scheduler(void)
{
    struct sched_param param;

    memset(&param, 0, sizeof(struct sched_param));
    param.sched_priority = sched_get_priority_min(SCHED_FIFO);
    if (param.sched_priority > 0)
    {
        sched_setscheduler(0, SCHED_FIFO, &param);
    }
}

static void wait_for_midi_initialization(void) __attribute__((noinline));
static void wait_for_midi_initialization(void)
{
    while (midi_init_state == 0)
    {
        struct timespec req;

        req.tv_sec = 0;
        req.tv_nsec = 10000000;
        nanosleep(&req, NULL);
    };
}

static void subscription_event(snd_seq_event_t *event) __attribute__((noinline));
static void subscription_event(snd_seq_event_t *event)
{
    snd_seq_client_info_t *cinfo;
    int err;

    snd_seq_client_info_alloca(&cinfo);
    err = snd_seq_get_any_client_info(midi_seq, event->data.connect.sender.client, cinfo);
    if (err >= 0)
    {
        if (event->type == SND_SEQ_EVENT_PORT_SUBSCRIBED)
        {
            printf("Client subscribed: %s\n", snd_seq_client_info_get_name(cinfo));
        }
        else
        {
            printf("Client unsubscribed: %s\n", snd_seq_client_info_get_name(cinfo));
        }
    }
    else
    {
        printf("Client unsubscribed\n");
    }
}

static inline void WRITE_LE_UINT32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
    ptr[2] = (value >> 16) & 0xff;
    ptr[3] = (value >> 24) & 0xff;
}

static void write_event(const uint8_t *event, unsigned int length)
{
    uint8_t event_time[4];

    WRITE_LE_UINT32(event_time, VLSG_GetTime());

    for (; length != 0; length--,event++)
    {
        VLSG_AddMidiData(event_time, 4);
        VLSG_AddMidiData(event, 1);
    }

    midi_event_written = 1;
}

static void process_event(snd_seq_event_t *event, uint8_t *running_status)
{
    uint8_t data[12];
    int length;

    switch (event->type)
    {
        case SND_SEQ_EVENT_NOTEON:
            data[0] = 0x90 | event->data.note.channel;
            data[1] = event->data.note.note;
            data[2] = event->data.note.velocity;
            length = 3;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("Note ON, channel:%d note:%d velocity:%d\n", event->data.note.channel, event->data.note.note, event->data.note.velocity);
#endif

            break;

        case SND_SEQ_EVENT_NOTEOFF:
            // send note off event as note on with zero velocity to increase the chance of using running status
            data[0] = 0x90 | event->data.note.channel;
            data[1] = event->data.note.note;
            data[2] = 0;
            length = 3;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("Note OFF, channel:%d note:%d velocity:%d\n", event->data.note.channel, event->data.note.note, event->data.note.velocity);
#endif

            break;

        case SND_SEQ_EVENT_KEYPRESS:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xA0 | event->data.note.channel;
            data[1] = event->data.note.note;
            data[2] = event->data.note.velocity;
            length = 3;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }
#endif

#ifdef PRINT_EVENTS
            printf("Keypress, channel:%d note:%d velocity:%d\n", event->data.note.channel, event->data.note.note, event->data.note.velocity);
#endif

            break;

        case SND_SEQ_EVENT_CONTROLLER:
            data[0] = 0xB0 | event->data.control.channel;
            data[1] = event->data.control.param;
            data[2] = event->data.control.value;
            length = 3;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("Controller, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_PGMCHANGE:
            data[0] = 0xC0 | event->data.control.channel;
            data[1] = event->data.control.value;
            length = 2;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("Program change, channel:%d value:%d\n", event->data.control.channel, event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_CHANPRESS:
            data[0] = 0xD0 | event->data.control.channel;
            data[1] = event->data.control.value;
            length = 2;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("Channel pressure, channel:%d value:%d\n", event->data.control.channel, event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_PITCHBEND:
            data[0] = 0xE0 | event->data.control.channel;
            data[1] = (event->data.control.value + 0x2000) & 0x7f;
            data[2] = ((event->data.control.value + 0x2000) >> 7) & 0x7f;
            length = 3;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("Pitch bend, channel:%d value:%d\n", event->data.control.channel, event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_CONTROL14:
            if (event->data.control.param >= 0 && event->data.control.param < 32)
            {
                data[0] = 0xB0 | event->data.control.channel;
                data[1] = event->data.control.param;
                data[2] = (event->data.control.value >> 7) & 0x7f;
                data[3] = event->data.control.param + 32;
                data[4] = event->data.control.value & 0x7f;
                length = 5;

                if (data[0] != *running_status)
                {
                    *running_status = data[0];
                    write_event(data, length);
                }
                else
                {
                    write_event(data + 1, length - 1);
                }

#ifdef PRINT_EVENTS
                printf("Controller 14-bit, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
#endif
            }
            else
            {
#ifdef PRINT_EVENTS
                printf("Unknown controller, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
#endif
            }

            break;

        case SND_SEQ_EVENT_NONREGPARAM:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xB0 | event->data.control.channel;
            data[1] = 0x63; // NRPN MSB
            data[2] = (event->data.control.param >> 7) & 0x7f;
            data[3] = 0x62; // NRPN LSB
            data[4] = event->data.control.param & 0x7f;
            data[5] = 0x06; // data entry MSB
            data[6] = (event->data.control.value >> 7) & 0x7f;
            data[7] = 0x26; // data entry LSB
            data[8] = event->data.control.value & 0x7f;
            length = 9;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }
#endif

#ifdef PRINT_EVENTS
            printf("NRPN, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_REGPARAM:
            data[0] = 0xB0 | event->data.control.channel;
            data[1] = 0x65; // RPN MSB
            data[2] = (event->data.control.param >> 7) & 0x7f;
            data[3] = 0x64; // RPN LSB
            data[4] = event->data.control.param & 0x7f;
            data[5] = 0x06; // data entry MSB
            data[6] = (event->data.control.value >> 7) & 0x7f;
            data[7] = 0x26; // data entry LSB
            data[8] = event->data.control.value & 0x7f;
            length = 9;

            if (data[0] != *running_status)
            {
                *running_status = data[0];
                write_event(data, length);
            }
            else
            {
                write_event(data + 1, length - 1);
            }

#ifdef PRINT_EVENTS
            printf("RPN, channel:%d param:%d value:%d\n", event->data.control.channel, event->data.control.param, event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_SYSEX:
            length = event->data.ext.len;

            *running_status = 0;
            write_event(event->data.ext.ptr, length);

#ifdef PRINT_EVENTS
            printf("SysEx (fragment) of size %d\n", event->data.ext.len);
#endif

            break;

        case SND_SEQ_EVENT_QFRAME:
            // Not handled by CASIO SW-10
#if 0
            data[0] = 0xF1;
            data[1] = ev->data.control.value;
            length = 2;

            *running_status = 0;
            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("MTC Quarter Frame, value:%d\n", event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_SONGPOS:
            // Not handled by CASIO SW-10
#if 0
            data[0] = 0xF2;
            data[1] = (event->data.control.value + 0x2000) & 0x7f;
            data[2] = ((event->data.control.value + 0x2000) >> 7) & 0x7f;
            length = 3;

            *running_status = 0;
            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Song Position, value:%d\n", event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_SONGSEL:
            // Not handled by CASIO SW-10
#if 0
            data[0] = 0xF3;
            data[1] = ev->data.control.value;
            length = 2;

            *running_status = 0;
            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Song Select, value:%d\n", event->data.control.value);
#endif

            break;

        case SND_SEQ_EVENT_TUNE_REQUEST:
            // Not handled by CASIO SW-10
#if 0
            data[0] = 0xF6;
            length = 1;

            *running_status = 0;
            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Tune Request\n");
#endif

            break;

        case SND_SEQ_EVENT_CLOCK:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xF8;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Clock\n");
#endif

            break;

        case SND_SEQ_EVENT_TICK:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xF9;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Tick\n");
#endif

            break;

        case SND_SEQ_EVENT_START:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xFA;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Start\n");
#endif

            break;

        case SND_SEQ_EVENT_CONTINUE:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xFB;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Continue\n");
#endif

            break;

        case SND_SEQ_EVENT_STOP:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xFC;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Stop\n");
#endif

            break;

        case SND_SEQ_EVENT_SENSING:
            // Not used by CASIO SW-10
#if 0
            data[0] = 0xFE;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Active Sense\n");
#endif

            break;

        case SND_SEQ_EVENT_RESET:
            // Not handled correctly by CASIO SW-10
#if 0
            data[0] = 0xFF;
            length = 1;

            write_event(data, length);
#endif

#ifdef PRINT_EVENTS
            printf("Reset\n");
#endif

            break;

        case SND_SEQ_EVENT_PORT_SUBSCRIBED:
            subscription_event(event);
            break;

        case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
            subscription_event(event);
            break;

        default:
            fprintf(stderr, "Unhandled event type: %i\n", event->type);
            break;
    }
}

static void *midi_thread_proc(void *arg)
{
    snd_seq_event_t *event;
    uint8_t running_status;

    // try setting thread scheduler (only root)
    set_thread_scheduler();

    // set thread as initialized
    *(int *)arg = 1;

    wait_for_midi_initialization();

    running_status = 0;

    while (midi_init_state > 0)
    {
        if (snd_seq_event_input(midi_seq, &event) < 0)
        {
            continue;
        }

        process_event(event, &running_status);
    }

    return NULL;
}

static void usage(const char *progname)
{
    static const char basename[] = "sw10_alsadrv";

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
    }

    printf(
        "%s - CASIO Software Sound Generator SW-10\n"
        "Usage: %s [OPTIONS]...\n"
        "  -f NUM   Frequency (0 = 11025 Hz, 1 = 22050 Hz, 2 = 44100 Hz)\n"
        "  -p NUM   Polyphony (0 = 24 voices, 1 = 32 voices, 2 = 48 voices, 3 = 64 voices)\n"
        "  -e NUM   Reverb effect (0 = off, 1 = reverb 1, 2 = reverb 2)\n"
        "  -r PATH  Rom path (path to ROMSXGM.BIN)\n"
        "  -d       Daemonize\n"
        "  -h       Help\n",
        basename,
        progname
    );
    exit(1);
}

static void read_arguments(int argc, char *argv[]) __attribute__((noinline));
static void read_arguments(int argc, char *argv[])
{
    int i, j;

    // frequency: 0 = 11025 Hz, 1 = 22050 Hz, 2 = 44100 Hz
    frequency = 2;

    // polyphony: 0 = 24 voices, 1 = 32 voices, 2 = 48 voices, 3 = 64 voices
    polyphony = 3;

    // reverb effect: 0 = off, 1 = reverb 1, 2 = reverb 2
    reverb_effect = 0;

    daemonize = 0;

    if (argc <= 1)
    {
        return;
    }

    for (i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-' && argv[i][1] != 0 && argv[i][2] == 0)
        {
            switch (argv[i][1])
            {
                case 'r': // rom
                    if ((i + 1) < argc)
                    {
                        i++;
                        rom_filepath = argv[i];
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
                case 'd': // daemonize
                    daemonize = 1;
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


static int load_rom_file(void)
{
    int rom_fd;

    rom_fd = open(rom_filepath, O_RDONLY);
    if (rom_fd < 0)
    {
        char *rompathcopy, *slash, *filename;
        DIR *dir;
        struct dirent *entry;

        rompathcopy = strdup(rom_filepath);
        if (rompathcopy == NULL) return -1;

        slash = strrchr(rompathcopy, '/');
        if (slash != NULL)
        {
            filename = slash + 1;
            if (slash != rompathcopy)
            {
                *slash = 0;
                dir = opendir(rompathcopy);
                *slash = '/';
            }
            else
            {
                dir = opendir("/");
            }
        }
        else
        {
            filename = rompathcopy;
            dir = opendir(".");
        }

        if (dir == NULL)
        {
            free(rompathcopy);
            return -2;
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
            free(rompathcopy);
            return -3;
        }

        rom_fd = open(rompathcopy, O_RDONLY);

        free(rompathcopy);

        if (rom_fd < 0)
        {
            return -4;
        }
    }

    rom_address = mmap(NULL, ROMSIZE, PROT_READ, MAP_PRIVATE | MAP_LOCKED, rom_fd, 0);
    if (rom_address == MAP_FAILED)
    {
        rom_address = mmap(NULL, ROMSIZE, PROT_READ, MAP_PRIVATE, rom_fd, 0);
    }

    close(rom_fd);

    if (rom_address == MAP_FAILED)
    {
        return -5;
    }

    return 0;
}


static int start_synth(void) __attribute__((noinline));
static int start_synth(void)
{
    if (load_rom_file() < 0)
    {
        fprintf(stderr, "Error opening ROM file: %s\n", rom_filepath);
        return -1;
    }

    // set frequency
    VLSG_SetParameter(PARAMETER_Frequency, frequency);

    // set polyphony
    VLSG_SetParameter(PARAMETER_Polyphony, 0x10 + polyphony);

    // set reverb effect
    VLSG_SetParameter(PARAMETER_Effect, 0x20 + reverb_effect);

    // set address of ROM file
    VLSG_SetParameter(PARAMETER_ROMAddress, (uintptr_t)rom_address);

    // set output buffer
    outbuf_counter = 0;
    memset(midi_buffer, 0, 65536);
    VLSG_SetParameter(PARAMETER_OutputBuffer, (uintptr_t)midi_buffer);

    // split output buffer to 16 subbuffers
    samples_per_call = 256 << frequency;
    bytes_per_call = 4 * samples_per_call;

    int i;
    for (i = 0; i < 16; i++)
    {
        midi_buf[i] = &(midi_buffer[i * bytes_per_call]);
    }


    // initialize time
#if defined(CLOCK_MONOTONIC_RAW)
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &start_time))
    {
        monotonic_clock_id = CLOCK_MONOTONIC;
        clock_gettime(CLOCK_MONOTONIC, &start_time);
    }
    else
    {
        monotonic_clock_id = CLOCK_MONOTONIC_RAW;
    }
#else
    clock_gettime(CLOCK_MONOTONIC, &start_time);
#endif

    if (start_time.tv_sec != 0)
    {
        start_time.tv_sec--;
    }
    else
    {
        start_time.tv_nsec = 0;
    }


    // set function GetTime
    VLSG_SetFunc_GetTime(&VLSG_GetTime);

    // start playback
    VLSG_PlaybackStart();

    return 0;
}

static void stop_synth(void)
{
    VLSG_PlaybackStop();
    munmap(rom_address, ROMSIZE);
}

static int run_as_daemon(void) __attribute__((noinline));
static int run_as_daemon(void)
{
    int err;

    printf("Running as daemon...\n");

    err = daemon(0, 0);
    if (err < 0)
    {
        fprintf(stderr, "Error running as daemon: %i\n", err);
        return -1;
    }

    return 0;
}

static int open_midi_port(void) __attribute__((noinline));
static int open_midi_port(void)
{
    int err;
    unsigned int caps, type;

    err = snd_seq_open(&midi_seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
    if (err < 0)
    {
        fprintf(stderr, "Error opening ALSA sequencer: %i\n%s\n", err, snd_strerror(err));
        return -1;
    }

    err = snd_seq_set_client_name(midi_seq, midi_name);
    if (err < 0)
    {
        snd_seq_close(midi_seq);
        fprintf(stderr, "Error setting sequencer client name: %i\n%s\n", err, snd_strerror(err));
        return -2;
    }

    caps = SND_SEQ_PORT_CAP_SUBS_WRITE | SND_SEQ_PORT_CAP_WRITE;
    type = SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_MIDI_GM | SND_SEQ_PORT_TYPE_SYNTHESIZER;
    err = snd_seq_create_simple_port(midi_seq, port_name, caps, type);
    if (err < 0)
    {
        snd_seq_close(midi_seq);
        fprintf(stderr, "Error creating sequencer port: %i\n%s\n", err, snd_strerror(err));
        return -3;
    }
    midi_port_id = err;

    printf("%s ALSA address is %i:0\n", midi_name, snd_seq_client_id(midi_seq));

    return 0;
}

static void close_midi_port(void)
{
    snd_seq_delete_port(midi_seq, midi_port_id);
    snd_seq_close(midi_seq);
}


static int set_hw_params(void)
{
    int err, dir;
    unsigned int rate;
    snd_pcm_uframes_t buffer_size, period_size;
    snd_pcm_hw_params_t *pcm_hwparams;

    snd_pcm_hw_params_alloca(&pcm_hwparams);

    err = snd_pcm_hw_params_any(midi_pcm, pcm_hwparams);
    if (err < 0)
    {
        fprintf(stderr, "Error getting hwparams: %i\n%s\n", err, snd_strerror(err));
        return -1;
    }

    err = snd_pcm_hw_params_set_access(midi_pcm, pcm_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        fprintf(stderr, "Error setting access: %i\n%s\n", err, snd_strerror(err));
        return -2;
    }

    err = snd_pcm_hw_params_set_format(midi_pcm, pcm_hwparams, SND_PCM_FORMAT_S16);
    if (err < 0)
    {
        fprintf(stderr, "Error setting format: %i\n%s\n", err, snd_strerror(err));
        return -3;
    }

    err = snd_pcm_hw_params_set_channels(midi_pcm, pcm_hwparams, 2);
    if (err < 0)
    {
        fprintf(stderr, "Error setting channels: %i\n%s\n", err, snd_strerror(err));
        return -4;
    }

    rate = 11025 << frequency;
    dir = 0;
    err = snd_pcm_hw_params_set_rate_near(midi_pcm, pcm_hwparams, &rate, &dir);
    if (err < 0)
    {
        fprintf(stderr, "Error setting rate: %i\n%s\n", err, snd_strerror(err));
        return -5;
    }

    buffer_size = samples_per_call * 16;
    err = snd_pcm_hw_params_set_buffer_size_near(midi_pcm, pcm_hwparams, &buffer_size);
    if (err < 0)
    {
        fprintf(stderr, "Error setting buffer size: %i\n%s\n", err, snd_strerror(err));
        return -6;
    }

    period_size = samples_per_call;
    dir = 0;
    err = snd_pcm_hw_params_set_period_size_near(midi_pcm, pcm_hwparams, &period_size, &dir);
    if (err < 0)
    {
        fprintf(stderr, "Error setting period size: %i\n%s\n", err, snd_strerror(err));
        return -7;
    }

    err = snd_pcm_hw_params(midi_pcm, pcm_hwparams);
    if (err < 0)
    {
        fprintf(stderr, "Error setting hwparams: %i\n%s\n", err, snd_strerror(err));
        return -8;
    }

    return 0;
}

static int set_sw_params(void)
{
    snd_pcm_sw_params_t *swparams;
    int err;

    snd_pcm_sw_params_alloca(&swparams);

    err = snd_pcm_sw_params_current(midi_pcm, swparams);
    if (err < 0)
    {
        fprintf(stderr, "Error getting swparams: %i\n%s\n", err, snd_strerror(err));
        return -1;
    }

    err = snd_pcm_sw_params_set_avail_min(midi_pcm, swparams, samples_per_call);
    if (err < 0)
    {
        fprintf(stderr, "Error setting avail min: %i\n%s\n", err, snd_strerror(err));
        return -2;
    }

    err = snd_pcm_sw_params(midi_pcm, swparams);
    if (err < 0)
    {
        fprintf(stderr, "Error setting sw params: %i\n%s\n", err, snd_strerror(err));
        return -3;
    }

    return 0;
}

static int open_pcm_output(void) __attribute__((noinline));
static int open_pcm_output(void)
{
    int err;

    err = snd_pcm_open(&midi_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0)
    {
        fprintf(stderr, "Error opening PCM device: %i\n%s\n", err, snd_strerror(err));
        return -1;
    }

    if (set_hw_params() < 0)
    {
        return -2;
    }

    if (set_sw_params() < 0)
    {
        return -3;
    }

    // set nonblock mode
    snd_pcm_nonblock(midi_pcm, 1);

    snd_pcm_prepare(midi_pcm);

    return 0;
}

static void close_pcm_output(void)
{
    snd_pcm_close(midi_pcm);
}


static int drop_privileges(void)
{
    uid_t uid;
    gid_t gid;
    const char *sudo_id;
    long long int llid;
    const char *xdg_dir;
    char buf[32];
    struct stat statbuf;
    struct passwd *passwdbuf;

    if (getuid() != 0)
    {
        return 0;
    }

    sudo_id = secure_getenv("SUDO_UID");
    if (sudo_id == NULL)
    {
        sudo_id = secure_getenv("PKEXEC_UID");
        if (sudo_id == NULL)
        {
            return -1;
        }
    }

    errno = 0;
    llid = strtoll(sudo_id, NULL, 10);
    uid = (uid_t) llid;
    if (errno != 0 || uid == 0 || llid != (long long int)uid)
    {
        return -2;
    }

    gid = getgid();
    if (gid == 0)
    {
        sudo_id = secure_getenv("SUDO_GID");
        if (sudo_id == NULL)
        {
            passwdbuf = getpwuid(uid);
            if (passwdbuf != NULL)
            {
                gid = passwdbuf->pw_gid;
            }

            if (gid == 0)
            {
                return -3;
            }
        }
        else
        {
            errno = 0;
            llid = strtoll(sudo_id, NULL, 10);
            gid = (gid_t) llid;
            if (errno != 0 || gid == 0 || llid != (long long int)gid)
            {
                return -4;
            }
        }
    }

    if (setgid(gid) != 0)
    {
        return -5;
    }
    if (setuid(uid) != 0)
    {
        return -6;
    }

    printf("Dropped root privileges\n");

    chdir("/");

    // define some environment variables

    xdg_dir = getenv("XDG_RUNTIME_DIR");
    if ((xdg_dir == NULL) || (*xdg_dir == 0))
    {
        snprintf(buf, 32, "/run/user/%lli", (long long int)uid);

        if ((stat(buf, &statbuf) == 0) && ((statbuf.st_mode & S_IFMT) == S_IFDIR) && (statbuf.st_uid == uid))
        {
            // if XDG_RUNTIME_DIR is not defined and directory /run/user/$USER exists then use it for XDG_RUNTIME_DIR
            setenv("XDG_RUNTIME_DIR", buf, 1);

            xdg_dir = getenv("XDG_CONFIG_HOME");
            if ((xdg_dir == NULL) || (*xdg_dir == 0))
            {
                passwdbuf = getpwuid(uid);
                if (passwdbuf != NULL)
                {
                    // also if XDG_CONFIG_HOME is not defined then define it as user's home directory
                    setenv("XDG_CONFIG_HOME", passwdbuf->pw_dir, 1);
                }
            }
        }
    }

    return 0;
}

static int start_thread(void) __attribute__((noinline));
static int start_thread(void)
{
    pthread_attr_t attr;
    int err;
    volatile int initialized;

    // try to increase priority (only root)
    nice(-20);

    err = pthread_attr_init(&attr);
    if (err != 0)
    {
        fprintf(stderr, "Error creating thread attribute: %i\n", err);
        return -1;
    }

    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    midi_init_state = 0;
    initialized = 0;
    err = pthread_create(&midi_thread, &attr, &midi_thread_proc, (void *)&initialized);
    pthread_attr_destroy(&attr);

    if (err != 0)
    {
        fprintf(stderr, "Error creating thread: %i\n", err);
        return -2;
    }

    // wait for thread initialization
    while (initialized == 0)
    {
        struct timespec req;

        req.tv_sec = 0;
        req.tv_nsec = 10000000;
        nanosleep(&req, NULL);
    };

    if (drop_privileges() < 0)
    {
        fprintf(stderr, "Error dropping root privileges\n");
    }

    return 0;
}


static int output_subbuffer(int num)
{
    snd_pcm_uframes_t remaining;
    snd_pcm_sframes_t written;
    uint8_t *buf_ptr;

    remaining = samples_per_call;
    buf_ptr = midi_buf[num];

    while (remaining)
    {
        written = snd_pcm_writei(midi_pcm, buf_ptr, remaining);
        if (written < 0)
        {
            return -1;
        }

        remaining -= written;
        buf_ptr += written << 2;
    };

    return 0;
}

static void main_loop(void) __attribute__((noinline));
static void main_loop(void)
{
    int is_paused;
    struct timespec last_written_time, current_time;

    for (int i = 2; i < 16; i++)
    {
        output_subbuffer(i);
    }

    is_paused = 0;
    // pause pcm playback at the beginning
    if (0 == snd_pcm_pause(midi_pcm, 1))
    {
        is_paused = 1;
        printf("PCM playback paused\n");
    }
    else
    {
        // if pausing doesn't work then set time of last written event as current time, so the next attempt to pause will be in 60 seconds
        clock_gettime(MONOTONIC_CLOCK_TYPE, &last_written_time);
    }

    midi_event_written = 0;
    midi_init_state = 1;

    while (1)
    {
        struct timespec req;
        snd_pcm_state_t pcmstate;
        snd_pcm_sframes_t available_frames;

        req.tv_sec = 0;
        req.tv_nsec = 10000000;
        nanosleep(&req, NULL);

        if (midi_event_written)
        {
            midi_event_written = 0;

            // remember time of last written event
            clock_gettime(MONOTONIC_CLOCK_TYPE, &last_written_time);

            if (is_paused)
            {
                is_paused = 0;
                snd_pcm_pause(midi_pcm, 0);
                printf("PCM playback unpaused\n");
            }
        }
        else
        {
            if (is_paused)
            {
                continue;
            }

            clock_gettime(MONOTONIC_CLOCK_TYPE, &current_time);
            // if more than 60 seconds elapsed from last written event, then pause pcm playback
            if (current_time.tv_sec - last_written_time.tv_sec > 60)
            {
                if (0 == snd_pcm_pause(midi_pcm, 1))
                {
                    is_paused = 1;
                    printf("PCM playback paused\n");
                    continue;
                }
                else
                {
                    // if pausing doesn't work then set time of last written event as current time, so the next attempt to pause will be in 60 seconds
                    last_written_time = current_time;
                }
            }
        }

        pcmstate = snd_pcm_state(midi_pcm);
        if (pcmstate == SND_PCM_STATE_XRUN)
        {
            fprintf(stderr, "Buffer underrun\n");
            snd_pcm_prepare(midi_pcm);
        }

        available_frames = snd_pcm_avail_update(midi_pcm);
        while (available_frames >= (3 * samples_per_call))
        {
            VLSG_FillOutputBuffer(outbuf_counter);

            if (output_subbuffer(outbuf_counter & 15) < 0)
            {
                fprintf(stderr, "Error writing audio data\n");
                available_frames = 0;
                break;
            }
            else
            {
                available_frames -= samples_per_call;
            }

            outbuf_counter++;
        };
    };
}

int main(int argc, char *argv[])
{
    read_arguments(argc, argv);

    if (start_synth() < 0)
    {
        return 2;
    }

    if (daemonize)
    {
        if (run_as_daemon() < 0)
        {
            stop_synth();
            return 3;
        }
    }

    if (start_thread() < 0)
    {
        stop_synth();
        return 4;
    }

    if (open_pcm_output() < 0)
    {
        midi_init_state = -1;
        stop_synth();
        return 5;
    }

    if (open_midi_port() < 0)
    {
        midi_init_state = -1;
        close_pcm_output();
        stop_synth();
        return 6;
    }

    main_loop();

    midi_init_state = -1;
    close_midi_port();
    close_pcm_output();
    stop_synth();
    return 0;
}

