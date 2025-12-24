/**
 *
 *  Copyright (C) 2022-2025 Roman Pauer
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

#include <AvailabilityMacros.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreMIDI/CoreMIDI.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/mman.h>
#include <pwd.h>
#include <libproc.h>
#include <signal.h>
#include <spawn.h>
#include <crt_externs.h>
#include "VLSG.h"

#if defined(MAC_OS_VERSION_11_0) && MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_VERSION_11_0
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_11_0
#define MIDI_API 0
#else
#define MIDI_API 1
#endif
#else
#define MIDI_API -1
#endif


#define ROMSIZE (2 * 1024 * 1024)
#define MIDI_NAME "CASIO SW-10"

static const uint32_t midi_name_crc32 = 0x64cf66e2;

static MIDIClientRef midi_client;
static MIDIEndpointRef midi_endpoint;
static AudioQueueRef midi_pcm_queue;
static volatile int midi_event_written;

static int frequency, polyphony, reverb_effect, daemonize;
static const char *rom_filepath = "ROMSXGM.BIN";

static uint8_t *rom_address;
static uint32_t outbuf_counter;
static unsigned int bytes_per_call, samples_per_call;
static uint8_t midi_buffer[65536];
static uint8_t *midi_buf[16];
static struct AudioQueueBuffer *midi_queue_buffer[16];

static uint64_t start_time;

static uint8_t running_status;


static uint32_t VLSG_GetTime(void)
{
    return (clock_gettime_nsec_np(CLOCK_UPTIME_RAW) - start_time) / 1000000;
}


static inline void WRITE_LE_UINT32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = value & 0xff;
    ptr[1] = (value >> 8) & 0xff;
    ptr[2] = (value >> 16) & 0xff;
    ptr[3] = (value >> 24) & 0xff;
}

static void write_event(const uint8_t *event, unsigned int length, unsigned int time)
{
    uint8_t event_time[4];

    if (time == 0) time = VLSG_GetTime();
    WRITE_LE_UINT32(event_time, time);

    for (; length != 0; length--,event++)
    {
        VLSG_AddMidiData(event_time, 4);
        VLSG_AddMidiData(event, 1);
    }

    midi_event_written = 1;
}

#if (MIDI_API <= 0)
static void midi_read_proc(const MIDIPacketList *pktlist, void * __nullable readProcRefCon, void * __nullable srcConnRefCon)
{
    unsigned int index1, index2, length, time;
    uint8_t data[4];
    const MIDIPacket *packet;

    for (index1 = 0, packet = pktlist->packet; index1 < pktlist->numPackets; index1++, packet = MIDIPacketNext(packet))
    {
        time = VLSG_GetTime();

        for (index2 = 0; index2 < packet->length; index2++)
        {
            switch ((packet->data[index2] >> 4) & 0x0f) // status
            {
                case 0x08:
                    // send note off event as note on with zero velocity to increase the chance of using running status
                    data[0] = 0x90 | (packet->data[index2] & 0x0f);
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    data[2] = 0;
                    length = 3;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }

#ifdef PRINT_EVENTS
                    printf("Note OFF, channel:%d note:%d velocity:%d\n", packet->data[index2] & 0x0f, packet->data[index2 + 1] & 0x7f, packet->data[index2 + 2] & 0x7f);
#endif

                    index2 += 2;
                    break;

                case 0x09:
                    data[0] = packet->data[index2];
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    data[2] = packet->data[index2 + 2] & 0x7f;
                    length = 3;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }

#ifdef PRINT_EVENTS
                    printf("Note ON, channel:%d note:%d velocity:%d\n", packet->data[index2] & 0x0f, packet->data[index2 + 1] & 0x7f, packet->data[index2 + 2] & 0x7f);
#endif

                    index2 += 2;
                    break;

                case 0x0a:
                    // Not used by CASIO SW-10
#if 0
                    data[0] = packet->data[index2];
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    data[2] = packet->data[index2 + 2] & 0x7f;
                    length = 3;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }
#endif

#ifdef PRINT_EVENTS
                    printf("Keypress, channel:%d note:%d velocity:%d\n", packet->data[index2] & 0x0f, packet->data[index2 + 1] & 0x7f, packet->data[index2 + 2] & 0x7f);
#endif

                    index2 += 2;
                    break;

                case 0x0b:
                    data[0] = packet->data[index2];
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    data[2] = packet->data[index2 + 2] & 0x7f;
                    length = 3;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }

#ifdef PRINT_EVENTS
                    printf("Controller, channel:%d param:%d value:%d\n", packet->data[index2] & 0x0f, packet->data[index2 + 1] & 0x7f, packet->data[index2 + 2] & 0x7f);
#endif

                    index2 += 2;
                    break;

                case 0x0c:
                    data[0] = packet->data[index2];
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    length = 2;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }

#ifdef PRINT_EVENTS
                    printf("Program change, channel:%d value:%d\n", packet->data[index2] & 0x0f, packet->data[index2 + 1] & 0x7f);
#endif

                    index2++;
                    break;

                case 0x0d:
                    data[0] = packet->data[index2];
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    length = 2;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }

#ifdef PRINT_EVENTS
                    printf("Channel pressure, channel:%d value:%d\n", packet->data[index2] & 0x0f, packet->data[index2 + 1] & 0x7f);
#endif

                    index2++;
                    break;

                case 0x0e:
                    data[0] = packet->data[index2];
                    data[1] = packet->data[index2 + 1] & 0x7f;
                    data[2] = packet->data[index2 + 2] & 0x7f;
                    length = 3;

                    if (data[0] != running_status)
                    {
                        running_status = data[0];
                        write_event(data, length, time);
                    }
                    else
                    {
                        write_event(data + 1, length - 1, time);
                    }

#ifdef PRINT_EVENTS
                    printf("Pitch bend, channel:%d value:%d\n", packet->data[index2] & 0x0f, ((packet->data[index2 + 1] & 0x7f) | ((packet->data[index2 + 2] & 0x7f) << 7)) - 0x2000);
#endif

                    index2 += 2;
                    break;

                case 0x0f:
                    switch (packet->data[index2])
                    {
                        case 0xf0:
                            length = packet->length - index2;

                            running_status = 0;
                            write_event(packet->data + index2, length, time);

#ifdef PRINT_EVENTS
                            printf("SysEx (fragment), length:%d\n", length);
#endif

                            index2 = packet->length - 1;
                            break;

                        case 0xf1:
                            // Not handled by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            data[1] = packet->data[index2 + 1] & 0x7f;
                            length = 2;

                            running_status = 0;
                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("MTC Quarter Frame, value:%d\n", packet->data[index2 + 1] & 0x7f);
#endif

                            index2++;
                            break;

                        case 0xf2:
                            // Not handled by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            data[1] = packet->data[index2 + 1] & 0x7f;
                            data[2] = packet->data[index2 + 2] & 0x7f;
                            length = 3;

                            running_status = 0;
                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Song Position, value:%d\n", ((packet->data[index2 + 1] & 0x7f) | ((packet->data[index2 + 2] & 0x7f) << 7)) - 0x2000);
#endif

                            index2 += 2;
                            break;

                        case 0xf3:
                            // Not handled by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            data[1] = packet->data[index2 + 1] & 0x7f;
                            length = 2;

                            running_status = 0;
                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Song Select, value:%d\n", packet->data[index2 + 1] & 0x7f);
#endif

                            index2++;
                            break;

                        case 0xf6:
                            // Not handled by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            running_status = 0;
                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Tune Request\n");
#endif

                            break;

                        case 0xf8:
                            // Not used by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Clock\n");
#endif

                            break;

                        case 0xfa:
                            // Not used by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Start\n");
#endif

                            break;

                        case 0xfb:
                            // Not used by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Continue\n");
#endif

                            break;

                        case 0xfc:
                            // Not used by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Stop\n");
#endif

                            break;

                        case 0xfe:
                            // Not used by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Active Sense\n");
#endif

                            break;

                        case 0xff:
                            // Not handled correctly by CASIO SW-10
#if 0
                            data[0] = packet->data[index2];
                            length = 1;

                            write_event(data, length, time);
#endif

#ifdef PRINT_EVENTS
                            printf("Reset\n");
#endif

                            break;

                        default:
                            fprintf(stderr, "Unhandled system message: 0x%x\n", packet->data[index2]);
                            break;
                    }
                    break;
                default:
                    if (index2 == 0)
                    {
                        length = packet->length;

                        running_status = 0;
                        write_event(packet->data, length, time);

#ifdef PRINT_EVENTS
                        printf("SysEx (fragment) of size %d\n", length);
#endif

                        index2 = packet->length - 1;
                    }
                    else
                    {
                        fprintf(stderr, "Unhandled message: 0x%x\n", packet->data[index2]);
                    }
                    break;
            }
        }
    }
}
#endif

#if (MIDI_API >= 0)
static void midi_receive_proc(const MIDIEventList *evtlist, void * __nullable srcConnRefCon)
{
    unsigned int index1, index2, length, time;
    uint8_t data[8];
    const MIDIEventPacket *packet;

    for (index1 = 0, packet = evtlist->packet; index1 < evtlist->numPackets; index1++, packet = MIDIEventPacketNext(packet))
    {
        time = VLSG_GetTime();

        for (index2 = 0; index2 < packet->wordCount; index2++)
        {
            switch ((packet->words[index2] >> 28) & 0x0f) // message type
            {
                // Traditional MIDI 1.0 Functionality
                case kMIDIMessageTypeSystem:
                    switch ((packet->words[index2] >> 16) & 0xff) // status
                    {
                        case kMIDIStatusMTC:
                            // Not handled by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xF1;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                length = 2;

                                running_status = 0;
                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("MTC Quarter Frame, group:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 8) & 0x7f);
#endif

                            break;

                        case kMIDIStatusSongPosPointer:
                            // Not handled by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xF2;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                data[2] = packet->words[index2] & 0x7f;
                                length = 3;

                                running_status = 0;
                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Song Position, group:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (((packet->words[index2] >> 8) & 0x7f) | ((packet->words[index2] & 0x7f) << 7)) - 0x2000);
#endif

                            break;

                        case kMIDIStatusSongSelect:
                            // Not handled by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xF3;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                length = 2;

                                running_status = 0;
                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Song Select, group:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 8) & 0x7f);
#endif

                            break;

                        case kMIDIStatusTuneRequest:
                            // Not handled by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xF6;
                                length = 1;

                                running_status = 0;
                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Tune Request, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        case kMIDIStatusTimingClock:
                            // Not used by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xF8;
                                length = 1;

                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Clock, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        case kMIDIStatusStart:
                            // Not used by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xFA;
                                length = 1;

                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Start, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        case kMIDIStatusContinue:
                            // Not used by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xFB;
                                length = 1;

                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Continue, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        case kMIDIStatusStop:
                            // Not used by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xFC;
                                length = 1;

                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Stop, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        case kMIDIStatusActiveSending:
                            // Not used by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xFE;
                                length = 1;

                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Active Sense, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        case kMIDIStatusSystemReset:
                            // Not handled correctly by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = 0xFF;
                                length = 1;

                                write_event(data, length, time);
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Reset, group:%d\n", (packet->words[index2] >> 24) & 0x0f);
#endif

                            break;

                        default:
                            fprintf(stderr, "Unhandled system message: 0x%x, group:%d\n", (packet->words[index2] >> 16) & 0xff, (packet->words[index2] >> 24) & 0x0f);
                            break;
                    }
                    break;

                case kMIDIMessageTypeChannelVoice1:
                    switch ((packet->words[index2] >> 20) & 0x0f) // status
                    {
                        case kMIDICVStatusNoteOff:
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                // send note off event as note on with zero velocity to increase the chance of using running status
                                data[0] = 0x90 | ((packet->words[index2] >> 16) & 0x0f);
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                data[2] = 0;
                                length = 3;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }

#ifdef PRINT_EVENTS
                            printf("Note OFF, group:%d channel:%d note:%d velocity:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (packet->words[index2] >> 8) & 0x7f, packet->words[index2] & 0x7f);
#endif

                            break;

                        case kMIDICVStatusNoteOn:
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = packet->words[index2] >> 16;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                data[2] = packet->words[index2] & 0x7f;
                                length = 3;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }

#ifdef PRINT_EVENTS
                            printf("Note ON, group:%d channel:%d note:%d velocity:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (packet->words[index2] >> 8) & 0x7f, packet->words[index2] & 0x7f);
#endif

                            break;

                        case kMIDICVStatusPolyPressure:
                            // Not used by CASIO SW-10
#if 0
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = packet->words[index2] >> 16;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                data[2] = packet->words[index2] & 0x7f;
                                length = 3;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }
#endif

#ifdef PRINT_EVENTS
                            printf("Keypress, group:%d channel:%d note:%d velocity:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (packet->words[index2] >> 8) & 0x7f, packet->words[index2] & 0x7f);
#endif

                            break;

                        case kMIDICVStatusControlChange:
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = packet->words[index2] >> 16;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                data[2] = packet->words[index2] & 0x7f;
                                length = 3;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }

#ifdef PRINT_EVENTS
                            printf("Controller, group:%d channel:%d param:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (packet->words[index2] >> 8) & 0x7f, packet->words[index2] & 0x7f);
#endif

                            break;

                        case kMIDICVStatusProgramChange:
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = packet->words[index2] >> 16;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                length = 2;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }

#ifdef PRINT_EVENTS
                            printf("Program change, group:%d channel:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (packet->words[index2] >> 8) & 0x7f);
#endif

                            break;

                        case kMIDICVStatusChannelPressure:
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = packet->words[index2] >> 16;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                length = 2;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }

#ifdef PRINT_EVENTS
                            printf("Channel pressure, group:%d channel:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (packet->words[index2] >> 8) & 0x7f);
#endif

                            break;

                        case kMIDICVStatusPitchBend:
                            if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                            {
                                data[0] = packet->words[index2] >> 16;
                                data[1] = (packet->words[index2] >> 8) & 0x7f;
                                data[2] = packet->words[index2] & 0x7f;
                                length = 3;

                                if (data[0] != running_status)
                                {
                                    running_status = data[0];
                                    write_event(data, length, time);
                                }
                                else
                                {
                                    write_event(data + 1, length - 1, time);
                                }
                            }

#ifdef PRINT_EVENTS
                            printf("Pitch bend, group:%d channel:%d value:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f, (((packet->words[index2] >> 8) & 0x7f) | ((packet->words[index2] & 0x7f) << 7)) - 0x2000);
#endif

                            break;

                        default:
                            fprintf(stderr, "Unhandled channel voice message: 0x%x, group:%d\n", (packet->words[index2] >> 16) & 0xff, (packet->words[index2] >> 24) & 0x0f);
                            break;
                    }
                    break;

                case kMIDIMessageTypeSysEx:
                    if (((packet->words[index2] >> 24) & 0x0f) == 0) // group
                    {
                        length = (packet->words[index2] >> 16) & 0x0f;
                        data[1] = (packet->words[index2] >> 8) & 0x7f;
                        data[2] = packet->words[index2] & 0x7f;
                        data[3] = (packet->words[index2 + 1] >> 24) & 0x7f;
                        data[4] = (packet->words[index2 + 1] >> 16) & 0x7f;
                        data[5] = (packet->words[index2 + 1] >> 8) & 0x7f;
                        data[6] = packet->words[index2 + 1] & 0x7f;

                        running_status = 0;
                        if (((packet->words[index2] >> 20) & 0x0f) == kMIDISysExStatusComplete || ((packet->words[index2] >> 20) & 0x0f) == kMIDISysExStatusEnd)
                        {
                            length++;
                            data[length] = 0xf7;
                        }
                        if (((packet->words[index2] >> 20) & 0x0f) == kMIDISysExStatusComplete || ((packet->words[index2] >> 20) & 0x0f) == kMIDISysExStatusStart)
                        {
                            data[0] = 0xf0;
                            write_event(data, length + 1, time);
                        }
                        else
                        {
                            write_event(data + 1, length, time);
                        }
                    }

#ifdef PRINT_EVENTS
                    printf("SysEx (fragment), group:%d length:%d\n", (packet->words[index2] >> 24) & 0x0f, (packet->words[index2] >> 16) & 0x0f);
#endif

                    index2++;
                    break;

                // other types - 1 word
                case kMIDIMessageTypeUtility:
                case 6:
                case 7:
                default:
                    fprintf(stderr, "Unhandled message type: %d\n", (packet->words[index2] >> 28) & 0x0f);
                    break;

                // other types - 2 words
                case kMIDIMessageTypeChannelVoice2:
                case 8:
                case 9:
                case 10:
                    fprintf(stderr, "Unhandled message type: %d\n", (packet->words[index2] >> 28) & 0x0f);
                    index2++;
                    break;

                // other types - 3 words
                case 11:
                case 12:
                    fprintf(stderr, "Unhandled message type: %d\n", (packet->words[index2] >> 28) & 0x0f);
                    index2 += 2;
                    break;

                // other types - 4 words
                case kMIDIMessageTypeData128:
                case 13:
                case 14:
                case 15:
                    fprintf(stderr, "Unhandled message type: %d\n", (packet->words[index2] >> 28) & 0x0f);
                    index2 += 3;
                    break;
            }
        }
    }
}
#endif


static void usage(const char *progname)
{
    static const char basename[] = "sw10_coredrv";

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


static int run_as_daemon_start(char *argv[]) __attribute__((noinline));
static int run_as_daemon_start(char *argv[])
{
    pid_t pid;
    int res, err;
    struct sigaction chld_action;
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];

    // get current signal action for SIGCHLD
    sigaction(SIGCHLD, NULL, &chld_action);

    pid = getpid();
    if (chld_action.sa_handler != SIG_IGN || getpgrp() == pid)
    {
        // spawn a new process (current process is process group leader or signal handler for SIGCHLD is not SIG_IGN)

        signal(SIGCHLD, SIG_IGN);

        res = proc_pidpath(pid, pathbuf, sizeof(pathbuf));
        err = posix_spawn(NULL, (res > 0 && res < sizeof(pathbuf)) ? pathbuf : argv[0], NULL, NULL, argv, *_NSGetEnviron());
        if (err != 0)
        {
            fprintf(stderr, "Error spawning process: %i\n", err);
            return -1;
        }

        exit(0);
    }
    else
    {
        // create new session (current process is not process group leader and signal handler for SIGCHLD is SIG_IGN)

        if (setsid() < 0)
        {
            fprintf(stderr, "Error creating session\n");
            return -2;
        }
    }

    printf("Running as daemon...\n");

    return 0;
}

static void run_as_daemon_finish(void) __attribute__((noinline));
static void run_as_daemon_finish(void)
{
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
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

    rom_address = mmap(NULL, ROMSIZE, PROT_READ, MAP_PRIVATE, rom_fd, 0);

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

    for (int i = 0; i < 16; i++)
    {
        midi_buf[i] = &(midi_buffer[i * bytes_per_call]);
    }


    // initialize time
    start_time = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
    start_time -= 1000000000; // 1 sec


    // set function GetTime
    VLSG_SetFunc_GetTime(&VLSG_GetTime);

    // start playback
    VLSG_PlaybackStart();

    return 0;
}

static void stop_synth(void) __attribute__((noinline));
static void stop_synth(void)
{
    VLSG_PlaybackStop();
    munmap(rom_address, ROMSIZE);
}

static int drop_privileges(void)
{
    uid_t uid;
    gid_t gid;
    const char *sudo_id;
    long long int llid;
    struct passwd *passwdbuf;

    if (getuid() != 0)
    {
        return 0;
    }

    if (issetugid())
    {
        return -1;
    }

    sudo_id = getenv("SUDO_UID");
    if (sudo_id == NULL)
    {
        return -2;
    }

    errno = 0;
    llid = strtoll(sudo_id, NULL, 10);
    uid = (uid_t) llid;
    if (errno != 0 || uid == 0 || llid != (long long int)uid)
    {
        return -3;
    }

    gid = getgid();
    if (gid == 0)
    {
        sudo_id = getenv("SUDO_GID");
        if (sudo_id == NULL)
        {
            passwdbuf = getpwuid(uid);
            if (passwdbuf != NULL)
            {
                gid = passwdbuf->pw_gid;
            }

            if (gid == 0)
            {
                return -4;
            }
        }
        else
        {
            errno = 0;
            llid = strtoll(sudo_id, NULL, 10);
            gid = (gid_t) llid;
            if (errno != 0 || gid == 0 || llid != (long long int)gid)
            {
                return -5;
            }
        }
    }

    if (setgid(gid) != 0)
    {
        return -6;
    }
    if (setuid(uid) != 0)
    {
        return -7;
    }

    printf("Dropped root privileges\n");

    chdir("/");

    return 0;
}

static void handle_privileges(void) __attribute__((noinline));
static void handle_privileges(void)
{
    // try to increase priority (only root)
    setpriority(PRIO_PROCESS, 0, -20);

    if (drop_privileges() < 0)
    {
        fprintf(stderr, "Error dropping root privileges\n");
    }
}


static void audio_callback_proc(void *inUserData, AudioQueueRef inAQ, AudioQueueBufferRef inBuffer)
{
    VLSG_FillOutputBuffer(outbuf_counter);
    midi_queue_buffer[outbuf_counter & 15]->mAudioDataByteSize = bytes_per_call;
    memcpy(midi_queue_buffer[outbuf_counter & 15]->mAudioData, midi_buf[outbuf_counter & 15], bytes_per_call);
    AudioQueueEnqueueBuffer(midi_pcm_queue, midi_queue_buffer[outbuf_counter & 15], 0, NULL);
    outbuf_counter++;
}

static int open_pcm_output(void) __attribute__((noinline));
static int open_pcm_output(void)
{
    AudioStreamBasicDescription format;
    OSStatus err;

    format.mSampleRate = 11025 << frequency;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = 4;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 16;
    format.mReserved = 0;

    err = AudioQueueNewOutput(&format, audio_callback_proc, NULL, NULL, NULL, 0, &midi_pcm_queue);
    if (err != noErr)
    {
        fprintf(stderr, "Error creating PCM queue: %i\n", err);
        return -1;
    }

    for (int i = 0; i < 16; i++)
    {
        err = AudioQueueAllocateBuffer(midi_pcm_queue, bytes_per_call, &midi_queue_buffer[i]);
        if (err != noErr)
        {
            fprintf(stderr, "Error allocating queue buffer: %i\n", err);
            return -2;
        }

        midi_queue_buffer[i]->mUserData = (void *)(uintptr_t)i;
    }

    return 0;
}

static void close_pcm_output(void) __attribute__((noinline));
static void close_pcm_output(void)
{
    AudioQueueDispose(midi_pcm_queue, 1);
}


static int open_midi_endpoint(void) __attribute__((noinline));
static int open_midi_endpoint(void)
{
    OSStatus err;

    err = MIDIClientCreate(CFSTR(MIDI_NAME), NULL, NULL, &midi_client);
    if (err != noErr)
    {
        fprintf(stderr, "Error creating MIDI client: %i\n", err);
        return -1;
    }

    running_status = 0;

#if (MIDI_API >= 0)
#if (MIDI_API == 0)
    if (__builtin_available(macOS 11.0, *))
#endif
    {
        err = MIDIDestinationCreateWithProtocol(midi_client, CFSTR(MIDI_NAME), kMIDIProtocol_1_0, &midi_endpoint, ^(const MIDIEventList *evtlist, void * __nullable srcConnRefCon) { midi_receive_proc(evtlist, srcConnRefCon); } );
    }
#endif
#if (MIDI_API <= 0)
#if (MIDI_API == 0)
    else
#endif
    {
        err = MIDIDestinationCreate(midi_client, CFSTR(MIDI_NAME), midi_read_proc, NULL, &midi_endpoint);
    }
#endif
    if (err != noErr)
    {
        MIDIClientDispose(midi_client);
        fprintf(stderr, "Error creating MIDI destination: %i\n", err);
        return -2;
    }

    printf("MIDI destination is %s\n", MIDI_NAME);

    if (MIDIObjectSetIntegerProperty(midi_client, kMIDIPropertyUniqueID, midi_name_crc32) == noErr)
    {
        printf("Unique ID is %i\n", (int)(int32_t)midi_name_crc32);
    }
    else
    {
        for (int i = 0; i < 32; i++)
        {
            int32_t unique_id = midi_name_crc32 ^ (1 << i);
            if (MIDIObjectSetIntegerProperty(midi_client, kMIDIPropertyUniqueID, unique_id) == noErr)
            {
                printf("Unique ID is %i\n", (int)unique_id);
                break;
            }
        }
    }

    return 0;
}

static void close_midi_endpoint(void) __attribute__((noinline));
static void close_midi_endpoint(void)
{
    MIDIEndpointDispose(midi_endpoint);
    MIDIClientDispose(midi_client);
}


static void main_loop(void) __attribute__((noinline));
static void main_loop(void)
{
    int is_paused;
    uint64_t last_written_time, current_time;

    for (int i = 2; i < 16; i++)
    {
        midi_queue_buffer[i]->mAudioDataByteSize = bytes_per_call;
        memset(midi_queue_buffer[i]->mAudioData, 0, bytes_per_call);
        AudioQueueEnqueueBuffer(midi_pcm_queue, midi_queue_buffer[i], 0, NULL);
    }

    // pause pcm playback at the beginning
    is_paused = 1;

    midi_event_written = 0;

    for (;;)
    {
        if (is_paused)
        {
            struct timespec req;

            req.tv_sec = 0;
            req.tv_nsec = 10000000;
            nanosleep(&req, NULL);

            if (midi_event_written)
            {
                midi_event_written = 0;

                // remember time of last written event
                last_written_time = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);

                if (AudioQueueStart(midi_pcm_queue, NULL) == noErr)
                {
                    is_paused = 0;
                    printf("PCM playback unpaused\n");
                }
            }
            else
            {
                continue;
            }
        }

        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1, 0);

        current_time = clock_gettime_nsec_np(CLOCK_UPTIME_RAW);
        if (midi_event_written)
        {
            midi_event_written = 0;

            // remember time of last written event
            last_written_time = current_time;

            continue;
        }

        // if more than 60 seconds elapsed from last written event, then pause pcm playback
        if (current_time - last_written_time > 60000000000ull)
        {
            if (AudioQueuePause(midi_pcm_queue) == noErr)
            {
                is_paused = 1;
                printf("PCM playback paused\n");
            }
            else
            {
                // if pausing doesn't work then set time of last written event as current time, so the next attempt to pause will be in 60 seconds
                last_written_time = current_time;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    read_arguments(argc, argv);

    if (daemonize)
    {
        if (run_as_daemon_start(argv) < 0)
        {
            return 1;
        }
    }

    if (start_synth() < 0)
    {
        return 2;
    }

    handle_privileges();

    if (open_pcm_output() < 0)
    {
        stop_synth();
        return 5;
    }

    if (open_midi_endpoint() < 0)
    {
        close_pcm_output();
        stop_synth();
        return 6;
    }

    if (daemonize)
    {
        run_as_daemon_finish();
    }

    main_loop();

    close_midi_endpoint();
    close_pcm_output();
    stop_synth();
    return 0;
}

