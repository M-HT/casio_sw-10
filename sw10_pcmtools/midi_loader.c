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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "midi_loader.h"


#define GETU32FBE(buf) (                    \
            (uint32_t) ( (buf)[0] ) << 24 | \
            (uint32_t) ( (buf)[1] ) << 16 | \
            (uint32_t) ( (buf)[2] ) <<  8 | \
            (uint32_t) ( (buf)[3] )       )

#define GETU16FBE(buf) (                    \
            (uint32_t) ( (buf)[0] ) <<  8 | \
            (uint32_t) ( (buf)[1] )       )

// Midi Status Bytes
#define MIDI_STATUS_NOTE_OFF    0x08
#define MIDI_STATUS_NOTE_ON     0x09
#define MIDI_STATUS_AFTERTOUCH  0x0A
#define MIDI_STATUS_CONTROLLER  0x0B
#define MIDI_STATUS_PROG_CHANGE 0x0C
#define MIDI_STATUS_PRESSURE    0x0D
#define MIDI_STATUS_PITCH_WHEEL 0x0E
#define MIDI_STATUS_SYSEX       0x0F


typedef struct {
    const uint8_t *ptr;
    unsigned int len, delta;
    uint8_t prev_event;
    int eot;
} midi_track_info;


void free_midi_data(midi_event_info *data)
{
    unsigned int index;

    if (data != NULL)
    {
        for (index = data[0].len; index != 0; index--)
        {
            if (data[index].sysex != NULL)
            {
                free(data[index].sysex);
                data[index].sysex = NULL;
            }
        }

        free(data);
    }
}

static int readmidi(const uint8_t *midi, unsigned int midilen, unsigned int *number_of_tracks_ptr, unsigned int *time_division_ptr, midi_track_info **tracks_ptr)
{
    unsigned int format_type, number_of_tracks, time_division, index;
    midi_track_info *tracks;
    const uint8_t *cur_position;
    int retval;

    if (midilen < 14)
    {
        // not enough place for midi header
        return 1;
    }

    if (GETU32FBE(midi) != 0x4D546864)
    {
        // "MThd"
        return 2;
    }

    if (GETU32FBE(midi + 4) != 6)
    {
        // wrong midi header size
        return 3;
    }

    format_type = GETU16FBE(midi + 8);
    number_of_tracks = GETU16FBE(midi + 10);
    time_division = GETU16FBE(midi + 12);

    if ((format_type != 0) && (format_type != 1))
    {
        // unsupported midi format
        return 4;
    }

    if ((number_of_tracks == 0) || ((format_type == 0) && (number_of_tracks != 1)))
    {
        // wrong number of tracks
        return 5;
    }

    if (time_division == 0)
    {
        // wrong time division
        return 6;
    }

    tracks = (midi_track_info *) malloc(number_of_tracks * sizeof(midi_track_info));
    if (tracks == NULL)
    {
        return 7;
    }

    // find tracks
    cur_position = midi + 14;
    for (index = 0; index < number_of_tracks; index++)
    {
        unsigned int track_len;

        if ((uintptr_t)(cur_position - midi) + 8 > midilen)
        {
            // not enough place for track header
            retval = 8;
            goto midi_error_1;
        }

        if (GETU32FBE(cur_position) != 0x4D54726B)
        {
            // "MTrk"
            retval = 9;
            goto midi_error_1;
        }

        track_len = GETU32FBE(cur_position + 4);

        if ((uintptr_t)(cur_position - midi) + track_len > midilen)
        {
            // not enough place for track
            retval = 10;
            goto midi_error_1;
        }

        tracks[index].len = track_len;
        tracks[index].ptr = cur_position + 8;
        tracks[index].prev_event = 0;
        tracks[index].eot = (track_len == 0)?1:0;

        cur_position = cur_position + 8 + track_len;
    }

    *number_of_tracks_ptr = number_of_tracks;
    *time_division_ptr = time_division;
    *tracks_ptr = tracks;

    return 0;

midi_error_1:
    free(tracks);
    return retval;
}

static unsigned int read_varlen(midi_track_info *track)
{
    unsigned int varlen, ___data;

    varlen = 0;
    if (track->len != 0)
    {
        do {
            ___data = *(track->ptr);
            track->ptr++;
            track->len--;
            varlen = (varlen << 7) | (___data & 0x7f);
        } while ((___data & 0x80) && (track->len != 0));
    }

    track->eot = (track->len == 0)?1:0;

    return (track->eot)?0:varlen;
}

static int preprocessmidi(const uint8_t *midi, unsigned int midilen, unsigned int *timediv, midi_event_info **dataptr)
{
    unsigned int number_of_tracks, time_division, index, varlen;
    midi_track_info *tracks, *curtrack;
    unsigned int num_allocated, num_events, last_tick;
    midi_event_info *events;
    int retval, lasttracknum, eventextralen;
    midi_event_info event;
    unsigned int tempo, tempo_tick;
    uint32_t tempo_time;

    retval = readmidi(midi, midilen, &number_of_tracks, &time_division, &tracks);
    if (retval) return retval;

    // prepare tracks
    for (index = 0; index < number_of_tracks; index++)
    {
        curtrack = &(tracks[index]);

        // read delta
        curtrack->delta = read_varlen(curtrack);
    }

    num_allocated = midilen / 4;
    num_events = 1;

    events = (midi_event_info *) malloc(sizeof(midi_event_info) * num_allocated);
    if (events == NULL)
    {
        retval = 11;
        goto midi_error_1;
    }

    events[0].tick = 0;
    events[0].len = 0;
    events[0].sysex = NULL;
    events[0].time = 0;

    lasttracknum = -1;
    last_tick = 0;
    tempo = 500000; // 500000 MPQN = 120 BPM
    tempo_tick = 0;
    tempo_time = 0;
    while (1)
    {
        curtrack = NULL;

        if ((lasttracknum >= 0) && (!tracks[lasttracknum].eot) && (tracks[lasttracknum].delta == 0))
        {
            curtrack = &(tracks[lasttracknum]);
        }
        else
        {
            unsigned int mindelta;
            mindelta = UINT_MAX;
            for (index = 0; index < number_of_tracks; index++)
            {
                if ((!tracks[index].eot) && (tracks[index].delta < mindelta))
                {
                    mindelta = tracks[index].delta;
                    curtrack = &(tracks[index]);
                    lasttracknum = index;
                }
            }
        }

        if (curtrack == NULL) break;

        // update deltas
        if (curtrack->delta != 0)
        for (index = 0; index < number_of_tracks; index++)
        {
            if ((!tracks[index].eot) && (index != lasttracknum))
            {
                tracks[index].delta -= curtrack->delta;
            }
        }

        // read and process data
        event.tick = last_tick + curtrack->delta;
        last_tick = event.tick;
        event.sysex = NULL;

        // calculate event time in miliseconds
        event.time = ( ((event.tick - tempo_tick) * (uint64_t) tempo) / (time_division * 1000) ) + tempo_time;

        if (event.time > events[0].time) events[0].time = event.time;

        eventextralen = -1;

        if (*curtrack->ptr & 0x80)
        {
            curtrack->prev_event = *curtrack->ptr;
            curtrack->ptr += 1;
            curtrack->len -= 1;
        }

        switch (curtrack->prev_event >> 4)
        {
            case MIDI_STATUS_NOTE_OFF:
            case MIDI_STATUS_NOTE_ON:
            case MIDI_STATUS_AFTERTOUCH:
            case MIDI_STATUS_CONTROLLER:
            case MIDI_STATUS_PITCH_WHEEL:
                if (curtrack->len >= 2)
                {
                    event.data[0] = curtrack->prev_event;
                    event.data[1] = curtrack->ptr[0];
                    event.data[2] = curtrack->ptr[1];
                    event.len = 3;
                    curtrack->ptr += 2;
                    curtrack->len -= 2;
                    eventextralen = 0;
                }
                else
                {
                    curtrack->len = 0;
                    curtrack->eot = 1;
                }

                break;

            case MIDI_STATUS_PROG_CHANGE:
            case MIDI_STATUS_PRESSURE:
                if (curtrack->len >= 1)
                {
                    event.data[0] = curtrack->prev_event;
                    event.data[1] = curtrack->ptr[0];
                    event.len = 2;
                    curtrack->ptr += 1;
                    curtrack->len -= 1;
                    eventextralen = 0;
                }
                else
                {
                    curtrack->len = 0;
                    curtrack->eot = 1;
                }
                break;

            case MIDI_STATUS_SYSEX:
                if (curtrack->prev_event == 0xff) // meta events
                {
                    if (curtrack->len >= 2)
                    {
                        if (curtrack->ptr[0] == 0x2f) // end of track
                        {
                            curtrack->len = 0;
                            curtrack->eot = 1;
                        }
                        else
                        {
                            if ((curtrack->ptr[0] == 0x51) && (curtrack->ptr[1] == 3) && (curtrack->len >= 5)) // set tempo
                            {
                                // time_division is assumed to be positive (ticks per beat / PPQN - Pulses (i.e. clocks) Per Quarter Note)

                                event.data[0] = curtrack->prev_event;
                                event.data[1] = curtrack->ptr[0];
                                event.data[2] = curtrack->ptr[1];
                                event.data[3] = curtrack->ptr[2];
                                event.data[4] = curtrack->ptr[3];
                                event.data[5] = curtrack->ptr[4];
                                event.len = 6;
                                eventextralen = 0;

                                tempo = (((uint32_t)(curtrack->ptr[2])) << 16) | (((uint32_t)(curtrack->ptr[3])) << 8) | ((uint32_t)(curtrack->ptr[4]));
                                tempo_tick = event.tick;
                                tempo_time = event.time;
                            }

                            // read length and skip event
                            curtrack->ptr += 1;
                            curtrack->len -= 1;
                            varlen = read_varlen(curtrack);
                            if (varlen <= curtrack->len)
                            {
                                curtrack->ptr += varlen;
                                curtrack->len -= varlen;
                            }
                            else
                            {
                                curtrack->len = 0;
                                curtrack->eot = 1;
                            }
                        }
                    }
                    else
                    {
                        curtrack->len = 0;
                        curtrack->eot = 1;
                    }
                }
                else if ((curtrack->prev_event == 0xf0) || (curtrack->prev_event == 0xf7)) // sysex
                {
                    const uint8_t *startevent;

                    startevent = curtrack->ptr;

                    varlen = read_varlen(curtrack);
                    if (varlen <= curtrack->len)
                    {
                        event.len = varlen + ((curtrack->prev_event == 0xf0)?1:0);
                        if (event.len)
                        {
                            if (event.len <= 8)
                            {
                                if ((curtrack->prev_event == 0xf0))
                                {
                                    event.data[0] = 0xf0;
                                    memcpy(&(event.data[1]), curtrack->ptr, varlen);
                                }
                                else
                                {
                                    memcpy(&(event.data[0]), curtrack->ptr, varlen);
                                }
                            }
                            else
                            {
                                event.sysex = (uint8_t *) malloc(event.len);
                                if (event.sysex == NULL)
                                {
                                    retval = 12;
                                    goto midi_error_2;
                                }

                                if ((curtrack->prev_event == 0xf0))
                                {
                                    event.sysex[0] = 0xf0;
                                    memcpy(event.sysex + 1, curtrack->ptr, varlen);
                                }
                                else
                                {
                                    memcpy(event.sysex, curtrack->ptr, varlen);
                                }
                            }

                            curtrack->ptr += varlen;
                            curtrack->len -= varlen;

                            eventextralen = 1 + curtrack->ptr - startevent;
                        }
                    }
                    else
                    {
                        curtrack->len = 0;
                        curtrack->eot = 1;
                    }
                }
                else
                {
                    curtrack->len = 0;
                    curtrack->eot = 1;
                }
                break;

            default:
                curtrack->len = 0;
                curtrack->eot = 1;
                break;
        }

        if (eventextralen >= 0)
        {
            if (num_events >= num_allocated)
            {
                midi_event_info *new_events;

                new_events = (midi_event_info *) realloc(events, sizeof(midi_event_info) * num_allocated * 2);
                if (new_events == NULL)
                {
                    retval = 13;
                    goto midi_error_2;
                }

                num_allocated = num_allocated * 2;
                events = new_events;
            }

            events[num_events] = event;
            events[0].len = num_events;
            num_events++;
        }

        // read delta
        curtrack->delta = read_varlen(curtrack);
    };

    if (events[0].len == 0)
    {
        retval = 14;
        goto midi_error_2;
    }

    // return values
    *timediv = time_division;
    *dataptr = events;

    free(tracks);
    return 0;

midi_error_2:
    free_midi_data(events);
midi_error_1:
    free(tracks);
    return retval;
}

int load_midi_file(const char *filename, unsigned int *timediv, midi_event_info **dataptr)
{
    FILE *f;
    long fsize;
    uint8_t *midi;
    int retval;

    f = fopen(filename, "rb");
    if (f == NULL) return 21;

    midi = NULL;

    // get file size
    retval = 22;
    if (fseek(f, 0, SEEK_END)) goto FILE_ERROR;

    fsize = ftell(f);
    if (fsize == -1) goto FILE_ERROR;

    if (fseek(f, 0, SEEK_SET)) goto FILE_ERROR;

    // allocate memory for the whole file
    retval = 24;
    midi = (uint8_t *)malloc(fsize);
    if (midi == NULL) goto FILE_ERROR;

    // read the whole file
    retval = 25;
    if (fread(midi, 1, fsize, f) != fsize) goto FILE_ERROR;

    fclose(f);
    f = NULL;

    // preprocess midi
    retval = preprocessmidi(midi, fsize, timediv, dataptr);

    free(midi);

    return retval;

FILE_ERROR:
    if (midi != NULL) free(midi);
    fclose(f);
    return retval;
}

