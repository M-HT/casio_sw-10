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

#include <stddef.h>
#include <string.h>
#include "VLSG.h"


#define MIDI_CHANNELS 16
#define DRUM_CHANNEL 9
#define MAX_VOICES 64


typedef struct
{
    uint16_t program_change;
    int16_t modulation;
    int16_t channel_pressure;
    int16_t expression;
    int16_t volume;
    int16_t pitch_bend;
    int16_t pan;
    uint16_t chflags;
    int16_t pitch_bend_sense;
    int16_t fine_tune;
    int16_t coarse_tune;
    uint8_t parameter_number_MSB;
    uint8_t parameter_number_LSB;
    uint8_t data_entry_MSB;
    uint8_t data_entry_LSB;
} Channel_Data;

typedef struct
{
    uint32_t field_00;
    uint32_t field_04;
    uint32_t field_08;
    int32_t field_0C[4];
    uint32_t field_1C;
    uint32_t field_20;
    uint32_t field_24;
    int32_t field_28;
    int32_t field_2C;
    int32_t field_30;
    int32_t field_34;
    int32_t field_38;
    int32_t note_number;
    int16_t note_velocity;
    int16_t channel_num_2;
    int16_t field_44;
    uint16_t vflags;
    int16_t field_48;
    int16_t field_4A;
    int16_t field_4C;
    uint16_t field_4E;
    int16_t field_50;
    int16_t field_52;
    int16_t field_54;
    int16_t field_56;
    int16_t field_58;
    int16_t field_5A;
    uint16_t field_5C;
    uint16_t field_5E;
    int16_t field_60;
    int16_t field_62;
    int16_t field_64;
    int16_t field_66;
    int16_t field_68;
    int16_t field_6A;
} Voice_Data;

typedef struct
{
    int16_t data[28];
} struc_6;


enum Voice_Flags
{
    VFLAG_Mask07    = 0x07,
    VFLAG_NotMask07 = 0xF8,

    VFLAG_Mask38    = 0x38,
    VFLAG_NotMask38 = 0xC7,

    VFLAG_Value40   = 0x40,
    VFLAG_Value80   = 0x80,
    VFLAG_MaskC0    = 0xC0,
};

enum Channel_Flags
{
    CHFLAG_Sostenuto  = 0x2000,
    CHFLAG_Soft       = 0x4000,
    CHFLAG_Sustain    = 0x8000,
};


static const char VLSG_Name[] = "CASIO SW-10";
static uint32_t (*VLSG_GetTime)(void);

static uint32_t dword_C0000000;
static uint32_t dword_C0000004;
static uint32_t dword_C0000008;
static int32_t output_size_para;
static uint32_t system_time_2;
static uint8_t event_data[32];
static uint32_t recent_voice_index;
static struc_6 *stru6_ptr;
static Channel_Data *channel_data_ptr;
static uint32_t event_type;
static int32_t event_length;
static int32_t reverb_data_buffer[32768];
static uint32_t reverb_data_index;
static int32_t is_reverb_enabled;
static uint32_t reverb_shift;
static volatile uint32_t midi_data_read_index;
static uint8_t midi_data_buffer[65536];
static volatile uint32_t midi_data_write_index;
static uint32_t processing_phase;
static uint32_t rom_offset;
static struc_6 stru_C0030080[MIDI_CHANNELS];
static Channel_Data channel_data[MIDI_CHANNELS];
static Voice_Data voice_data[MAX_VOICES];
static uint32_t effect_type;
static int32_t current_polyphony;
static uint8_t *romsxgm_ptr;
static uint32_t output_frequency;
static int32_t maximum_polyphony_new_value;
static uint32_t system_time_1;
static int32_t maximum_polyphony;
static uint8_t *output_data_ptr;
static uint32_t output_buffer_size_samples;
static uint32_t output_buffer_size_bytes;
static uint32_t effect_param_value;

static const uint32_t dword_C0032188[112+104+40] =
{
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     0,     0,     0,     0,
        0,     0,     0,     0,     1,     1,     1,     1,
        1,     1,     1,     1,     1,     1,     1,     1,
        2,     2,     2,     2,     2,     2,     2,     2,
        3,     3,     3,     3,     4,     4,     4,     4,
        5,     5,     5,     5,     6,     6,     7,     7,
        8,     8,     8,     9,    10,    10,    11,    11,
       12,    13,    14,    15,    16,    16,    17,    19,
// dword_C0032348[104] // dword_C0032348 = &dword_C0032188[112]
       20,    21,    22,    23,    25,    26,    28,    30,
       32,    33,    35,    38,    40,    42,    45,    47,
       50,    53,    57,    60,    64,    67,    71,    76,
       80,    85,    90,    95,   101,   107,   114,   120,
      128,   135,   143,   152,   161,   170,   181,   191,
      203,   215,   228,   241,   256,   271,   287,   304,
      322,   341,   362,   383,   406,   430,   456,   483,
      512,   542,   574,   608,   645,   683,   724,   767,
      812,   861,   912,   966,  1024,  1084,  1149,  1217,
     1290,  1366,  1448,  1534,  1625,  1722,  1824,  1933,
     2048,  2169,  2298,  2435,  2580,  2733,  2896,  3068,
     3250,  3444,  3649,  3866,  4096,  4339,  4597,  4870,
     5160,  5467,  5792,  6137,  6501,  6888,  7298,  7732,
// dword_C00324E8[40] // dword_C00324E8 = &dword_C0032188[216]
     8192,  8679,  9195,  9741, 10321, 10935, 11585, 12274,
    13003, 13777, 14596, 15464, 16384, 17358, 18390, 19483,
    20642, 21870, 23170, 24548, 26007, 27554, 29192, 30928,
    32768, 34716, 36780, 38967, 41285, 43740, 46340, 49096,
    52015, 55108, 58385, 61857, 65536, 69432, 73561, 77935
};
static const uint32_t dword_C0032588[256] =
{
    32768, 32775, 32782, 32790, 32797, 32804, 32812, 32819,
    32827, 32834, 32842, 32849, 32856, 32864, 32871, 32879,
    32886, 32893, 32901, 32908, 32916, 32923, 32931, 32938,
    32945, 32953, 32960, 32968, 32975, 32983, 32990, 32998,
    33005, 33012, 33020, 33027, 33035, 33042, 33050, 33057,
    33065, 33072, 33080, 33087, 33094, 33102, 33109, 33117,
    33124, 33132, 33139, 33147, 33154, 33162, 33169, 33177,
    33184, 33192, 33199, 33207, 33214, 33222, 33229, 33237,
    33244, 33252, 33259, 33267, 33274, 33282, 33289, 33297,
    33304, 33312, 33319, 33327, 33334, 33342, 33349, 33357,
    33364, 33372, 33379, 33387, 33394, 33402, 33410, 33417,
    33425, 33432, 33440, 33447, 33455, 33462, 33470, 33477,
    33485, 33493, 33500, 33508, 33515, 33523, 33530, 33538,
    33546, 33553, 33561, 33568, 33576, 33583, 33591, 33599,
    33606, 33614, 33621, 33629, 33636, 33644, 33652, 33659,
    33667, 33674, 33682, 33690, 33697, 33705, 33712, 33720,
    33728, 33735, 33743, 33751, 33758, 33766, 33773, 33781,
    33789, 33796, 33804, 33811, 33819, 33827, 33834, 33842,
    33850, 33857, 33865, 33873, 33880, 33888, 33896, 33903,
    33911, 33918, 33926, 33934, 33941, 33949, 33957, 33964,
    33972, 33980, 33987, 33995, 34003, 34010, 34018, 34026,
    34033, 34041, 34049, 34057, 34064, 34072, 34080, 34087,
    34095, 34103, 34110, 34118, 34126, 34133, 34141, 34149,
    34157, 34164, 34172, 34180, 34187, 34195, 34203, 34211,
    34218, 34226, 34234, 34241, 34249, 34257, 34265, 34272,
    34280, 34288, 34296, 34303, 34311, 34319, 34327, 34334,
    34342, 34350, 34358, 34365, 34373, 34381, 34389, 34396,
    34404, 34412, 34420, 34427, 34435, 34443, 34451, 34458,
    34466, 34474, 34482, 34490, 34497, 34505, 34513, 34521,
    34528, 34536, 34544, 34552, 34560, 34567, 34575, 34583,
    34591, 34599, 34606, 34614, 34622, 34630, 34638, 34646,
    34653, 34661, 34669, 34677, 34685, 34692, 34700, 34708
};
static const int32_t dword_C0032988[38+34+1] =
{
// this is possibly a bug in the original code
// maybe there were supposed to be two zero terminated lists (dword_C0032988 and dword_C0032A20)
    42, 44, 42, 46, 44, 42, 44, 46,
    46, 42, 46, 44, 71, 72, 72, 71,
    73, 74, 74, 73, 78, 79, 79, 78,
    80, 81, 81, 80, 29, 30, 30, 29,
    86, 87, 87, 86,
    255, 255,
// dword_C0032A20[34] // dword_C0032A20 = &dword_C0032988[38]
    27, 28, 27, 29, 28, 27, 28, 29,
    29, 27, 29, 28, 71, 72, 72, 71,
    73, 74, 74, 73, 78, 79, 79, 78,
    80, 81, 81, 80, 86, 87, 87, 86,
    255, 255,
// zero terminator
    0
};
static const int32_t dword_C0032AA8[12][128] =
{
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   8,   9,
         11,  13,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,  36,  39,  41,
         43,  45,  47,  49,  51,  52,  54,  55,  57,  59,  60,  61,  63,  64,  66,  67,
         68,  69,  70,  72,  73,  74,  76,  77,  78,  79,  81,  82,  83,  84,  85,  86,
         87,  87,  88,  89,  90,  91,  91,  92,  93,  93,  94,  95,  95,  96,  97,  97,
         98,  99, 100, 100, 101, 102, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108,
        109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 117, 117, 118, 119, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    // dword_C0032CA8[11][128]
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   8,   9,
         11,  13,  14,  16,  18,  20,  22,  24,  26,  28,  30,  32,  34,  36,  39,  41,
         43,  45,  47,  49,  51,  52,  54,  55,  57,  59,  60,  61,  63,  64,  66,  67,
         68,  69,  70,  72,  73,  74,  76,  77,  78,  79,  81,  82,  83,  84,  85,  86,
         87,  87,  88,  89,  90,  91,  91,  92,  93,  93,  94,  95,  95,  96,  97,  97,
         98,  99, 100, 100, 101, 102, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108,
        109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 117, 117, 118, 119, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   8,   9,
         11,  12,  13,  15,  17,  19,  21,  23,  25,  27,  29,  31,  33,  35,  37,  39,
         41,  43,  45,  47,  49,  50,  52,  53,  55,  57,  58,  59,  60,  61,  63,  64,
         65,  66,  67,  69,  70,  71,  73,  74,  75,  76,  78,  79,  80,  81,  82,  83,
         83,  84,  85,  86,  87,  88,  88,  89,  90,  90,  91,  92,  92,  93,  94,  94,
         95,  96,  97,  97,  98,  99,  99, 101, 102, 102, 103, 104, 104, 105, 106, 106,
        107, 108, 109, 110, 111, 112, 112, 113, 114, 114, 115, 116, 117, 118, 119, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   6,   7,   7,   8,
         10,  12,  13,  15,  17,  18,  20,  22,  24,  26,  28,  29,  31,  33,  36,  38,
         40,  41,  43,  45,  47,  48,  50,  51,  52,  54,  55,  56,  58,  59,  61,  62,
         62,  63,  64,  66,  67,  68,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
         80,  80,  81,  82,  83,  84,  84,  85,  86,  87,  88,  89,  89,  90,  91,  91,
         92,  93,  94,  95,  96,  97,  97,  98,  99,  99, 101, 102, 102, 103, 104, 104,
        106, 107, 108, 108, 109, 110, 111, 112, 113, 113, 115, 116, 116, 117, 118, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   5,   5,   6,   7,   8,
         10,  11,  12,  14,  16,  18,  19,  21,  23,  25,  26,  28,  30,  32,  34,  36,
         38,  40,  41,  43,  45,  46,  47,  48,  50,  52,  53,  54,  55,  56,  58,  59,
         60,  61,  61,  63,  64,  65,  67,  68,  69,  69,  71,  72,  73,  74,  75,  76,
         76,  77,  78,  79,  80,  81,  81,  82,  83,  83,  84,  86,  86,  87,  88,  88,
         89,  91,  92,  92,  93,  94,  94,  96,  97,  97,  98, 100, 100, 101, 102, 103,
        104, 105, 106, 107, 108, 109, 110, 111, 112, 112, 114, 115, 116, 117, 118, 119,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   4,   4,   5,   6,   7,   8,
          9,  11,  12,  13,  15,  17,  18,  20,  22,  23,  25,  27,  28,  30,  33,  34,
         36,  38,  39,  41,  43,  44,  45,  46,  48,  49,  50,  51,  53,  54,  55,  56,
         57,  58,  59,  60,  61,  62,  64,  65,  65,  66,  68,  69,  70,  70,  71,  72,
         73,  73,  74,  75,  76,  77,  78,  79,  80,  80,  81,  82,  83,  84,  85,  85,
         87,  88,  89,  89,  90,  92,  92,  93,  94,  95,  96,  97,  98,  99, 100, 101,
        102, 103, 105, 105, 107, 108, 108, 110, 111, 112, 113, 115, 115, 116, 118, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   5,   6,   7,   7,
          9,  10,  11,  13,  14,  16,  18,  19,  21,  22,  24,  26,  27,  29,  31,  33,
         34,  36,  37,  39,  41,  41,  43,  44,  45,  47,  48,  49,  50,  51,  53,  53,
         54,  55,  56,  57,  58,  59,  61,  61,  62,  63,  65,  65,  66,  67,  68,  69,
         69,  70,  71,  72,  73,  74,  74,  76,  77,  77,  78,  79,  80,  81,  82,  82,
         84,  85,  86,  87,  88,  89,  89,  91,  92,  93,  94,  95,  96,  97,  98,  99,
        100, 102, 103, 104, 105, 107, 107, 109, 110, 111, 112, 114, 115, 116, 118, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   3,   3,   4,   5,   6,   6,   7,
          8,  10,  11,  12,  14,  15,  17,  18,  20,  21,  23,  24,  26,  27,  30,  31,
         33,  34,  36,  37,  39,  39,  41,  42,  43,  45,  45,  46,  48,  48,  50,  51,
         51,  52,  53,  54,  55,  56,  58,  58,  59,  60,  61,  62,  63,  64,  64,  65,
         66,  66,  67,  68,  70,  71,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,
         81,  82,  83,  84,  85,  86,  87,  88,  90,  90,  92,  93,  94,  95,  97,  97,
         99, 100, 102, 102, 104, 105, 106, 108, 109, 110, 112, 113, 114, 116, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   3,   2,   3,   4,   5,   5,   6,   7,
          8,   9,  10,  11,  13,  14,  16,  17,  19,  20,  21,  23,  24,  26,  28,  29,
         31,  32,  34,  35,  37,  37,  39,  39,  41,  42,  43,  44,  45,  46,  47,  48,
         49,  49,  50,  52,  52,  53,  54,  55,  56,  57,  58,  59,  59,  60,  61,  62,
         62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,
         78,  79,  81,  81,  83,  84,  84,  86,  87,  88,  89,  91,  92,  93,  95,  95,
         97,  98, 100, 101, 102, 104, 105, 107, 108, 109, 111, 113, 114, 115, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   4,   4,   5,   6,   6,
          7,   9,   9,  11,  12,  14,  15,  16,  18,  19,  20,  22,  23,  24,  26,  28,
         29,  30,  32,  33,  34,  35,  36,  37,  39,  40,  41,  41,  43,  43,  45,  45,
         46,  47,  47,  49,  49,  50,  51,  52,  53,  53,  55,  55,  56,  57,  57,  58,
         59,  59,  60,  62,  63,  64,  64,  66,  67,  67,  69,  70,  70,  72,  73,  74,
         75,  76,  78,  78,  80,  81,  82,  83,  85,  86,  87,  89,  89,  91,  93,  93,
         95,  97,  99,  99, 101, 103, 104, 106, 107, 108, 110, 112, 113, 115, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   4,   4,   5,   5,   6,
          7,   8,   9,  10,  11,  13,  14,  15,  17,  18,  19,  20,  22,  23,  25,  26,
         27,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,
         43,  44,  44,  46,  46,  47,  48,  49,  50,  50,  51,  52,  53,  53,  54,  55,
         55,  56,  57,  58,  59,  61,  61,  62,  64,  64,  65,  67,  67,  69,  70,  71,
         72,  74,  75,  76,  77,  79,  79,  81,  83,  83,  85,  87,  87,  89,  91,  92,
         93,  95,  97,  98, 100, 102, 103, 104, 106, 107, 109, 111, 113, 115, 117, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    },
    {
          0,   1,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   4,   5,   5,   6,
          7,   8,   8,  10,  11,  12,  13,  14,  15,  17,  18,  19,  20,  21,  23,  24,
         26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  36,  37,  38,  39,  40,
         40,  41,  42,  43,  43,  44,  45,  46,  46,  47,  48,  49,  49,  50,  51,  51,
         52,  52,  53,  55,  56,  57,  58,  59,  60,  61,  62,  64,  64,  66,  67,  68,
         69,  71,  72,  73,  75,  76,  77,  79,  80,  81,  83,  84,  85,  87,  89,  90,
         92,  94,  95,  96,  98, 100, 101, 103, 105, 107, 109, 111, 112, 114, 116, 118,
        120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127
    }
};
static const uint8_t drum_kits[8] = { 0, 8, 16, 24, 25, 32, 40, 48 };
static const uint8_t drum_kit_numbers[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static int32_t *reverb_data_ptr;
static const int32_t dword_C00342C0[4] = { 0, 1, 2, -1 };
static const uint16_t word_C00342D0[17] = { 0, 250, 561, 949, 1430, 2030, 2776, 3704, 4858, 6295, 8083, 10307, 13075, 16519, 20803, 26135, 32768 };


uint32_t VLSG_GetVersion(void)
{
    return 0x103;
}

const char *VLSG_GetName(void)
{
    return VLSG_Name;
}

void VLSG_SetFunc_GetTime(uint32_t (*get_time)(void))
{
    VLSG_GetTime = get_time;
}


static int32_t InitializeEffect(void);
static int32_t EMPTY_DeinitializeEffect(void);
static int32_t InitializeVariables(void);
static int32_t EMPTY_DeinitializeVariables(void);
static void CountActiveVoices(void);
static void SetMaximumVoices(int maximum_voices);
static void ProcessMidiData(void);
static Voice_Data *FindAvailableVoice(int32_t channel_num_2, int32_t note_number);
static Voice_Data *FindVoice(int32_t channel_num_2, int32_t note_number);
static void NoteOff(void);
static void NoteOn(int32_t arg_0);
static void ControlChange(void);
static void SystemExclusive(void);
static int32_t InitializeReverbBuffer(void);
static int32_t DeinitializeReverbBuffer(void);
static void EnableReverb(void);
static void DisableReverb(void);
static void SetReverbShift(uint32_t shift);
static void DefragmentVoices(void);
static void GenerateOutputData(uint8_t *output_ptr, uint32_t offset1, uint32_t offset2);
static int32_t InitializeMidiDataBuffer(void);
static int32_t EMPTY_DeinitializeMidiDataBuffer(void);
static void AddByteToMidiDataBuffer(uint8_t value);
static uint8_t GetValueFromMidiDataBuffer(void);
static int32_t InitializePhase(void);
static int32_t EMPTY_DeinitializePhase(void);
static void sub_C0036A20(Voice_Data *voice_data_ptr);
static void sub_C0036A80(Voice_Data *voice_data_ptr);
static void sub_C0036B00(Voice_Data *voice_data_ptr);
static void sub_C0036C20(Voice_Data *voice_data_ptr);
static void ProcessPhase(void);
static int32_t sub_C0036FB0(int16_t value3);
static void sub_C0036FE0(void);
static void sub_C0037140(void);
static int32_t InitializeStructures(void);
static int32_t EMPTY_DeinitializeStructures(void);
static void ResetAllControllers(Channel_Data *channel_data_ptr);
static void ResetChannel(Channel_Data *channel_data_ptr);
static uint32_t sub_C00373A0(uint32_t arg_0, int32_t arg_4);
static uint16_t sub_C0037400(void);
static int16_t sub_C0037420(uint32_t arg_0);


static inline uint16_t READ_LE_UINT16(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8);
}


int32_t VLSG_SetParameter(uint32_t type, uintptr_t value)
{
    uint32_t buffer_size;
    int32_t polyphony;

    switch (type)
    {
        case PARAMETER_OutputBuffer:
            output_data_ptr = (uint8_t *)value;
            return 1;

        case PARAMETER_ROMAddress:
            romsxgm_ptr = (uint8_t *)value;
            return 1;

        case PARAMETER_Frequency:
            if (value == 0)
            {
                output_frequency = 11025;
                output_size_para = 64;
                buffer_size = 4096;
            }
            else if (value == 2)
            {
                output_frequency = 44100;
                output_size_para = 256;
                buffer_size = 16384;
            }
            else
            {
                output_frequency = 22050;
                output_size_para = 128;
                buffer_size = 8192;
            }

            output_buffer_size_samples = buffer_size;
            output_buffer_size_bytes = 4 * buffer_size;
            InitializeReverbBuffer();
            return 1;

        case PARAMETER_Polyphony:
            if (value == 0x11)
            {
                polyphony = 32;
            }
            else if (value == 0x12)
            {
                polyphony = 48;
            }
            else if (value == 0x13)
            {
                polyphony = 64;
            }
            else
            {
                polyphony = 24;
            }

            maximum_polyphony = polyphony;
            maximum_polyphony_new_value = polyphony;
            return 1;

        case PARAMETER_Effect:
            effect_param_value = value;
            DisableReverb();
            if (effect_param_value == 0x20)
            {
                DisableReverb();
                return 1;
            }
            else if (effect_param_value == 0x22)
            {
                SetReverbShift(0);
                EnableReverb();
                return 1;
            }
            else
            {
                SetReverbShift(1);
                EnableReverb();
                return 1;
            }

        default:
            return 0;
    }
}

int32_t VLSG_PlaybackStart(void)
{
    current_polyphony = 0;
    dword_C0000000 = 0;

    if (InitializeEffect())
    {
        return 0;
    }

    if (InitializeVariables())
    {
        EMPTY_DeinitializeEffect();
        return 0;
    }

    if (InitializeReverbBuffer())
    {
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeEffect();
        return 0;
    }

    if (InitializePhase())
    {
        DeinitializeReverbBuffer();
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeEffect();
        return 0;
    }

    if (InitializeMidiDataBuffer())
    {
        EMPTY_DeinitializePhase();
        DeinitializeReverbBuffer();
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeEffect();
        return 0;
    }

    if (InitializeStructures())
    {
        EMPTY_DeinitializeMidiDataBuffer();
        EMPTY_DeinitializePhase();
        DeinitializeReverbBuffer();
        EMPTY_DeinitializeVariables();
        EMPTY_DeinitializeEffect();
        return 0;
    }

    dword_C0000004 = 2972;
    return 1;
}

int32_t VLSG_PlaybackStop(void)
{
    current_polyphony = 0;

    EMPTY_DeinitializeStructures();
    EMPTY_DeinitializeMidiDataBuffer();
    EMPTY_DeinitializePhase();
    DeinitializeReverbBuffer();
    EMPTY_DeinitializeVariables();
    return EMPTY_DeinitializeEffect();
}

void VLSG_AddMidiData(uint8_t *ptr, uint32_t len)
{
    for (; len != 0; len--)
    {
        AddByteToMidiDataBuffer(*ptr++);
    }
}

int32_t VLSG_FillOutputBuffer(uint32_t output_buffer_counter)
{
    uint32_t time1, value1, time2, time3, offset1;
    int counter;
    uint8_t *output_ptr;
    uint32_t time4;

    time1 = VLSG_GetTime();

    if ((output_buffer_counter == 0) || (time1 - system_time_1 > 200))
    {
        value1 = 0;
        time2 = time1;
        system_time_1 = time1;
        system_time_2 = time1;
    }
    else
    {
        value1 = dword_C0000000;
        time2 = dword_C0000008;
    }

    dword_C0000000 = value1;
    dword_C0000008 = time2;

    if (value1 >= 512)
    {
        dword_C0000000 = 0;
        time2 += dword_C0000004;
        dword_C0000008 = time2;
        time3 = (7 * dword_C0000004 - system_time_2) + time1;

        if (time1 < time2)
        {
            time3 -= (time2 - time1) >> 4;
        }
        else if (time1 > time2)
        {
            time3 += (time1 - time2) >> 4;
        }

        system_time_2 = time1;
        dword_C0000004 = (time3 >> 3) + ((time3 & 4) >> 2);
    }

    offset1 = 0;
    output_ptr = &output_data_ptr[((output_buffer_counter & 0x0F) * output_size_para) << 4];
    for (counter = 4; counter != 0; counter--)
    {
        ProcessMidiData();
        ProcessPhase();
        GenerateOutputData(output_ptr, offset1, offset1 + output_size_para);
        offset1 += output_size_para;
        dword_C0000000++;
        system_time_1 = (((uint32_t)(dword_C0000000 * dword_C0000004)) >> 9) + dword_C0000008;
    }

    time4 = VLSG_GetTime();
    CountActiveVoices();
    time4 -= time1;
    maximum_polyphony = maximum_polyphony_new_value;

    if (time4 > 300)
    {
        SetMaximumVoices(2);
        return current_polyphony;
    }

    if (time4 >= 16)
    {
        if (time4 >= 20)
        {
            SetMaximumVoices((3 * current_polyphony) >> 2);
            return current_polyphony;
        }

        SetMaximumVoices((7 * current_polyphony) >> 3);
        return current_polyphony;
    }

    return current_polyphony;
}


static int32_t InitializeEffect(void)
{
    effect_type = 6;
    return 0;
}

static int32_t EMPTY_DeinitializeEffect(void)
{
    return 0;
}

static void sub_C0034890(Voice_Data *voice_data_ptr, int32_t arg_4)
{
    Channel_Data *channel_ptr;
    int32_t value1;
    uint32_t value2;

    channel_ptr = &(channel_data[voice_data_ptr->channel_num_2 >> 1]);
    value1 = (((int32_t)(channel_ptr->pitch_bend * channel_ptr->pitch_bend_sense)) >> 13) + arg_4 + channel_ptr->fine_tune + 2180;
    value2 = dword_C0032188[216 + (value1 >> 8)] * dword_C0032588[value1 & 0xFF];

    voice_data_ptr->field_24 = value2;
    switch (output_frequency)
    {
        case 11025:
            voice_data_ptr->field_24 = value2 >> 17;
            break;
        case 22050:
            voice_data_ptr->field_24 = value2 >> 18;
            break;
        case 44100:
            voice_data_ptr->field_24 = value2 >> 19;
            break;
        case 16538:
            voice_data_ptr->field_24 = (value2 / 3) >> 16;
            break;
        default:
            voice_data_ptr->field_24 = (uint32_t)((value2 >> 17) * 11025) / output_frequency;
            break;
    }
}

static int32_t sub_C0034970(Voice_Data *voice_data_ptr, int32_t arg_4)
{
    uint32_t offset1;
    int32_t channel_num_2;
    int32_t note_number;

    offset1 = sub_C00373A0(3, arg_4);
    channel_num_2 = (int16_t)(voice_data_ptr->channel_num_2 & ~1);
    note_number = voice_data_ptr->note_number;

    if (channel_num_2 != (2 * DRUM_CHANNEL))
    {
        note_number += channel_data[channel_num_2 >> 1].coarse_tune;
        note_number += (voice_data_ptr->field_56 + 128) >> 8;

        if (note_number < 12)
        {
            note_number += 12 * ((23 - note_number) / 12);
        }

        if (note_number > 108)
        {
            note_number -= 12 * ((note_number - 97) / 12);
        }
    }

    return sub_C0037420(offset1 + 2 * note_number);
}

static void ProgramChange(struc_6 *stru6_channel_ptr, uint32_t program_number)
{
    int16_t *data;
    int counter, index;

    data = stru6_channel_ptr->data;
    if (stru6_channel_ptr == &(stru_C0030080[DRUM_CHANNEL]))
    {
        program_number = (program_number & 7) + 128;
    }

    sub_C00373A0(1, sub_C0037420(sub_C00373A0(19, 0) + 2 * program_number));

    for (counter = 2; counter != 0; counter--)
    {
        for (index = 0; index < 14; index++)
        {
            data[index] = (int16_t)sub_C0037400();
        }

        data[3] >>= 8;
        data[4] >>= 8;
        data[5] >>= 8;
        data[6] >>= 8;
        data[7] >>= 8;
        data[8] >>= 8;
        data[11] >>= 8;
        data[12] >>= 8;
        data[13] >>= 8;

        data += 14;
    }
}

static void VoiceSoundOff(Voice_Data *voice_data_ptr)
{
    voice_data_ptr->field_50 = 0x7FFF;
    voice_data_ptr->vflags &= ~VFLAG_Value40;
    voice_data_ptr->vflags |= VFLAG_Value80;
    voice_data_ptr->vflags &= VFLAG_MaskC0;
    sub_C0036B00(voice_data_ptr);
    sub_C0036A80(voice_data_ptr);
}

static void VoiceNoteOff(Voice_Data *voice_data_ptr)
{
    voice_data_ptr->vflags |= VFLAG_Value80;

    if ((voice_data_ptr->vflags & VFLAG_Value40) == 0)
    {
        voice_data_ptr->vflags &= VFLAG_MaskC0;
        sub_C0036B00(voice_data_ptr);
        sub_C0036A80(voice_data_ptr);
    }
}

static void AllChannelNotesOff(int32_t channel_num)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            VoiceNoteOff(&(voice_data[index]));
        }
    }
}

static void AllChannelSoundsOff(int32_t channel_num)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            VoiceSoundOff(&(voice_data[index]));
        }
    }
}

static void ControllerSettingsOn(int32_t channel_num)
{
    int index;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            if (voice_data[index].note_number != 255)
            {
                if ((voice_data[index].vflags & VFLAG_Value80) == 0)
                {
                    voice_data[index].vflags |= VFLAG_Value40;
                }
            }
        }
    }
}

static void ControllerSettingsOff(int32_t channel_num)
{
    int index;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if ((voice_data[index].channel_num_2 >> 1) == channel_num)
        {
            if (voice_data[index].note_number != 255)
            {
                voice_data[index].vflags &= ~VFLAG_Value40;
                if ((voice_data[index].vflags & VFLAG_Value80) != 0)
                {
                    voice_data[index].vflags &= VFLAG_MaskC0;

                    sub_C0036B00(&(voice_data[index]));
                    sub_C0036A80(&(voice_data[index]));
                }
            }
        }
    }
}

static void StartPlayingVoice(Voice_Data *voice_data_ptr, Channel_Data *channel_data_ptr, int16_t *stru6_data_ptr)
{
    uint16_t value0;
    uint32_t value1;
    int32_t value2;
    int32_t value3;
    int32_t channel_num_2;
    int32_t value4;
    int32_t value5;
    int32_t value6;
    int index;
    const int32_t *drum_note_ptr;
    uint32_t value7;
    int32_t value8;

    voice_data_ptr->field_56 = stru6_data_ptr[2];
    voice_data_ptr->field_58 = stru6_data_ptr[7];
    voice_data_ptr->field_5A = stru6_data_ptr[8];
    voice_data_ptr->field_5C = stru6_data_ptr[9];
    voice_data_ptr->field_5E = stru6_data_ptr[10];

    value1 = (uint16_t)sub_C0037420(sub_C00373A0(2, (stru6_data_ptr[1] & 0xFFF) + sub_C0034970(voice_data_ptr, (*(uint16_t *)stru6_data_ptr) >> 8)));
    value2 = 0;
    value0 = sub_C0037400();
    value1 |= (value0 & 0xFF) << 16;
    voice_data_ptr->field_00 = value1 << 10;

    value1 = value0 >> 8;
    value0 = sub_C0037400();
    value1 |= value0 << 8;
    voice_data_ptr->field_04 = value1 & 0x3FFFFF;

    sub_C0037400();
    value1 = sub_C0037400();

    value0 = sub_C0037400();
    value1 |= (value0 & 0xFF) << 16;
    voice_data_ptr->field_68 = value0 >> 8;
    voice_data_ptr->field_66 = value0 & 0xFF;
    voice_data_ptr->field_08 = value1 & 0x3FFFFF;

    voice_data_ptr->field_44 = sub_C0037400();
    value0 = sub_C0037400();

    voice_data_ptr->field_60 = value0 & 0xFF;
    voice_data_ptr->field_0C[3] = 0;
    voice_data_ptr->field_0C[2] = 0;
    voice_data_ptr->field_20 = ((voice_data_ptr->field_00 & ~0x400u) >> 10) - 2;
    voice_data_ptr->field_1C = value0 >> 8;

    value3 = stru6_data_ptr[1] & 0x7000;
    if ( value3 != 0x7000 )
    {
        value2 = voice_data_ptr->note_number;
        channel_num_2 = (int16_t)(voice_data_ptr->channel_num_2 & ~1);
        if (channel_num_2 != (2 * DRUM_CHANNEL))
        {
            value2 += channel_data[channel_num_2 >> 1].coarse_tune;
            value2 += (voice_data_ptr->field_56 + 128) >> 8;

            if (value2 < 12)
            {
                value2 += 12 * ((23 - value2) / 12);
            }

            if (value2 > 108)
            {
                value2 -= 12 * ((value2 - 97) / 12);
            }
        }

        value2 = (value2 - voice_data_ptr->field_68) << 8;

        for (; value3 != 0; value3 -= 0x1000)
        {
            value2 >>= 1;
        }
    }

    value2 += voice_data_ptr->field_44;
    value2 += (int8_t)voice_data_ptr->field_56;
    voice_data_ptr->field_44 = value2;
    sub_C0034890(voice_data_ptr, value2);
    sub_C0036C20(voice_data_ptr);

    value4 = stru6_data_ptr[12];
    value5 = dword_C0032AA8[effect_type + 1][voice_data_ptr->note_velocity];

    if (value4 >= 0)
    {
        value5 = 127 - value5;
    }
    else
    {
        value4 = -value4;
    }

    value6 = (127 - (((int32_t)(value4 * value5)) >> 7)) + stru6_data_ptr[13];

    if ((channel_data_ptr->chflags & CHFLAG_Soft) != 0)
    {
        value6 >>= 1;
    }

    if (value6 > 127)
    {
        voice_data_ptr->field_62 = 127;
    }
    else
    {
        if (value6 <= 0)
        {
            value6 = 0;
        }
        voice_data_ptr->field_62 = value6;
    }

    voice_data_ptr->field_4C = 0;
    voice_data_ptr->field_2C = 0;
    voice_data_ptr->field_52 = 0;
    voice_data_ptr->vflags = 0;
    voice_data_ptr->field_4E = 0;
    sub_C0036A80(voice_data_ptr);
    sub_C0036B00(voice_data_ptr);

    if ((channel_data_ptr->chflags & CHFLAG_Sostenuto) != 0)
    {
        for (index = 0; index < maximum_polyphony; index++)
        {
            if (voice_data[index].note_number == 255) continue;
            if (voice_data_ptr->note_number != voice_data[index].note_number) continue;
            if (voice_data[index].channel_num_2 != voice_data_ptr->channel_num_2) continue;
            if ((voice_data[index].vflags & VFLAG_Value80) == 0) continue;
            if ((voice_data[index].vflags & VFLAG_Value40) == 0) continue;

            voice_data_ptr->vflags |= VFLAG_Value40;
            break;
        }
    }

    if ((channel_data_ptr->chflags & CHFLAG_Sustain) != 0)
    {
        voice_data_ptr->vflags |= VFLAG_Value40;
    }

    if ((voice_data_ptr->channel_num_2 & ~1) == (2 * DRUM_CHANNEL))
    {
        voice_data_ptr->field_6A = sub_C0037420(sub_C00373A0(18, 0) + 4 * voice_data_ptr->note_number);
        sub_C0036A20(voice_data_ptr);

// this is possibly a bug in the original code
// maybe there were supposed to be two zero terminated lists (dword_C0032988 and dword_C0032A20)
        drum_note_ptr = &(dword_C0032988[38]);
// question: can program_change have a value of 135 ?
        if (channel_data[DRUM_CHANNEL].program_change != 135)
        {
            drum_note_ptr = &(dword_C0032988[0]);
        }

        for (; drum_note_ptr[0] != 0; drum_note_ptr += 2)
        {
            if (drum_note_ptr[0] != voice_data_ptr->note_number) continue;

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number == drum_note_ptr[1])
                {
                    if ((voice_data[index].channel_num_2 & ~1) == (2 * DRUM_CHANNEL))
                    {
                        voice_data[index].note_number = 255;
                    }
                }
            }
        }
    }
    else
    {
        value7 = sub_C00373A0(17, 0);
        value8 = channel_data_ptr->pan + stru6_data_ptr[5];

        if (value8 > 127)
        {
            value8 = 127;
        }
        else if (value8 <= -127)
        {
            value8 = -127;
        }

        voice_data_ptr->field_6A = sub_C0037420(value7 + 2 * value8 + 256);
        sub_C0036A20(voice_data_ptr);
    }
}

static void AllVoicesSoundsOff(void)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        if (voice_data[index].note_number != 255)
        {
            VoiceSoundOff(&(voice_data[index]));
        }
    }
}

static int32_t InitializeVariables(void)
{
    recent_voice_index = 0;
    event_length = 0;
    event_type = 0;
    return 0;
}

static int32_t EMPTY_DeinitializeVariables(void)
{
    return 0;
}

static void CountActiveVoices(void)
{
    int active_voices, index;

    active_voices = 0;
    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number != 255)
        {
            active_voices++;
        }
    }
    current_polyphony = active_voices;
}

static void ReduceActiveVoices(int32_t maximum_voices)
{
    int index1, index2, index3;
    int active_voices;

    if (maximum_voices >= maximum_polyphony) return;

    if (maximum_voices > 0)
    {
        index2 = recent_voice_index + 1;
        if (index2 >= maximum_polyphony)
        {
            index2 = 0;
        }

        active_voices = 0;
        for (index1 = 0; index1 < maximum_polyphony; index1++)
        {
            if (voice_data[index1].note_number != 255)
            {
                active_voices++;
            }
        }

        index3 = index2;
        do
        {
            if (voice_data[index3].note_number != 255)
            {
                if (voice_data[index3].vflags & VFLAG_Value80)
                {
                    voice_data[index3].note_number = 255;
                    active_voices--;

                    if (active_voices <= maximum_voices)
                    {
                        current_polyphony = active_voices;
                        return;
                    }
                }
            }

            index3++;
            if (index3 >= maximum_polyphony)
            {
                index3 = 0;
            }
        } while (index3 != recent_voice_index);

        while (1)
        {
            if (voice_data[index2].note_number != 255)
            {
                voice_data[index2].note_number = 255;
                active_voices--;

                if (active_voices <= maximum_voices)
                {
                    break;
                }
            }

            index2++;
            if (index2 >= maximum_polyphony)
            {
                index2 = 0;
            }
            if (index2 == recent_voice_index)
            {
                return;
            }
        }

        current_polyphony = active_voices;
    }
    else
    {
        for (index1 = 0; index1 < maximum_polyphony; index1++)
        {
            voice_data[index1].note_number = 255;
        }
        current_polyphony = 0;
    }
}

static void SetMaximumVoices(int maximum_voices)
{
    int index;

    ReduceActiveVoices(maximum_voices);
    DefragmentVoices();
    maximum_polyphony = maximum_voices;

    for (index = maximum_voices; index < MAX_VOICES; index++)
    {
        voice_data[index].note_number = 255;
    }

    CountActiveVoices();

    recent_voice_index = 0;
}

static void ProcessMidiData(void)
{
    uint8_t midi_value;

    while (1)
    {
        midi_value = GetValueFromMidiDataBuffer();
        if (midi_value == 0xFF) break;

        if (midi_value > 0xF7) continue;

        if (midi_value == 0xF7)
        {
            if (event_data[0] != 0xF0) continue;
        }
        else if ((midi_value & 0x80) != 0)
        {
            event_length = 0;
            event_type = midi_value & 0xF0;
            event_data[0] = midi_value;
            channel_data_ptr = &(channel_data[midi_value & 0x0F]);
            stru6_ptr = &(stru_C0030080[midi_value & 0x0F]);

            continue;
        }
        else
        {
            event_length++;
            if (event_length >= 32) continue;

            event_data[event_length] = midi_value;

            if (event_data[0] == 0xF0) continue;

            if ((event_type != 0xC0) && (event_type != 0xD0) && (event_length != 2)) continue;
        }

        switch (event_type)
        {
            case 0x80: // Note Off
                NoteOff();
                break;

            case 0x90: // Note On
                if (event_data[2] != 0)
                {
                    NoteOn(0);

                    if (stru6_ptr->data[1] & 0x8000)
                    {
                        NoteOn(1);
                    }
                }
                else
                {
                    NoteOff();
                }
                break;

            case 0xB0: // Controller
                ControlChange();
                break;

            case 0xC0: // Program Change
                if ((event_data[0] & 0x0F) == DRUM_CHANNEL)
                {
                    int drum_kit_index;

                    for (drum_kit_index = 0; drum_kit_index < 8; drum_kit_index++)
                    {
                        if (drum_kits[drum_kit_index] == event_data[1]) break;
                    }
                    if (drum_kit_index >= 8) break;

                    channel_data_ptr->program_change = drum_kit_numbers[drum_kit_index];
                    ProgramChange(stru6_ptr, drum_kit_numbers[drum_kit_index]);
                }
                else
                {
                    channel_data_ptr->program_change = event_data[1];
                    ProgramChange(stru6_ptr, event_data[1]);
                }
                break;

            case 0xD0: // Channel Pressure
                channel_data_ptr->channel_pressure = event_data[1];
                break;

            case 0xE0: // Pitch Bend
                channel_data_ptr->pitch_bend = event_data[1] + ((event_data[2] - 64) << 7);
                break;

            case 0xF0: // SysEx
                SystemExclusive();
                break;

            default:
                break;
        }

        event_length = 0;
    }
}

static Voice_Data *FindAvailableVoice(int32_t channel_num_2, int32_t note_number)
{
    int index1, index2, index3, index4;

    index1 = recent_voice_index + 1;
    if (index1 >= maximum_polyphony)
    {
        index1 = 0;
    }

    for (index2 = 0; index2 < maximum_polyphony; index2++)
    {
        if (voice_data[index2].note_number == 255)
        {
            recent_voice_index = index2;
            return &(voice_data[index2]);
        }
    }

    index3 = index1;
    do
    {
        if ((voice_data[index3].vflags & VFLAG_Value80) != 0)
        {
            recent_voice_index = index3;
            return &(voice_data[index3]);
        }

        index3++;
        if (index3 >= maximum_polyphony)
        {
            index3 = 0;
        }
    } while (index3 != index1);

    index4 = index1;
    do
    {
        if ((voice_data[index4].channel_num_2 & ~1) == (2 * DRUM_CHANNEL))
        {
            recent_voice_index = index4;
            return &(voice_data[index4]);
        }

        index4++;
        if (index4 >= maximum_polyphony)
        {
            index4 = 0;
        }
    } while (index4 != index1);

    recent_voice_index = index1;
    return &(voice_data[index1]);
}

static Voice_Data *FindVoice(int32_t channel_num_2, int32_t note_number)
{
    int index;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number != 255)
        {
            if (voice_data[index].channel_num_2 == channel_num_2)
            {
                if (voice_data[index].note_number == note_number)
                {
                    if ((voice_data[index].vflags & VFLAG_Value80) == 0)
                    {
                        return &(voice_data[index]);
                    }
                }
            }
        }

    }

    return NULL;
}

static void NoteOff(void)
{
    Voice_Data *voice;

    if ((event_data[0] & 0x0F) == DRUM_CHANNEL)
    {
        if (channel_data_ptr->program_change != 7) return; // drum kit 49 (Orchestra Kit ?)
        if (event_data[1] != 88) return; // Applause ?
    }

    voice = FindVoice(2 * (event_data[0] & 0x0F), event_data[1]);
    if (voice != NULL)
    {
        VoiceNoteOff(voice);
    }

    voice = FindVoice(2 * (event_data[0] & 0x0F) + 1, event_data[1]);
    if (voice != NULL)
    {
        VoiceNoteOff(voice);
    }
}

static void NoteOn(int32_t arg_0)
{
    Voice_Data *voice;

    voice = FindAvailableVoice(arg_0 + 2 * (event_data[0] & 0x0F), event_data[1]);
    if (voice->note_number != 255)
    {
        VoiceSoundOff(voice);
    }

    voice->channel_num_2 = arg_0 + 2 * (event_data[0] & 0x0F);
    voice->note_number = event_data[1];
    voice->note_velocity = event_data[2];
    StartPlayingVoice(voice, channel_data_ptr, &(stru6_ptr->data[14 * arg_0]));
}

static void ControlChange(void)
{
    switch (event_data[1])
    {
        case 0x01: // Modulation
            channel_data_ptr->modulation = event_data[2];
            break;
        case 0x06: // Data Entry (MSB)
            channel_data_ptr->data_entry_MSB = event_data[2];
            if (channel_data_ptr->parameter_number_MSB == 0)
            {
                if (channel_data_ptr->parameter_number_LSB == 0) // Pitch bend range
                {
                    if (channel_data_ptr->data_entry_MSB <= 12)
                    {
                        channel_data_ptr->pitch_bend_sense = 2 * ((channel_data_ptr->data_entry_MSB << 7) + channel_data_ptr->data_entry_LSB);
                    }
                }
                else if (channel_data_ptr->parameter_number_LSB == 1) // Fine tuning
                {
                    channel_data_ptr->fine_tune = ((channel_data_ptr->data_entry_LSB & 0x60) >> 5) + 4 * channel_data_ptr->data_entry_MSB - 256;
                }
                else if (channel_data_ptr->parameter_number_LSB == 2) // Coarse tuning
                {
                    if (channel_data_ptr->data_entry_MSB >= 40 && channel_data_ptr->data_entry_MSB <= 88)
                    {
                        channel_data_ptr->coarse_tune = channel_data_ptr->data_entry_MSB - 64;
                    }
                }
            }
            break;
        case 0x07: // Main Volume
            channel_data_ptr->volume = event_data[2];
            break;
        case 0x0A: // Pan
            channel_data_ptr->pan = (2 * event_data[2]) - 128;
            break;
        case 0x0B: // Expression Controller
            channel_data_ptr->expression = event_data[2];
            break;
        case 0x26: // Data Entry (LSB)
            channel_data_ptr->data_entry_LSB = event_data[2];
            if (channel_data_ptr->parameter_number_MSB == 0)
            {
                if (channel_data_ptr->parameter_number_LSB == 0) // Pitch bend range
                {
                    if (channel_data_ptr->data_entry_MSB <= 12)
                    {
                        channel_data_ptr->pitch_bend_sense = 2 * ((channel_data_ptr->data_entry_MSB << 7) + channel_data_ptr->data_entry_LSB);
                    }
                }
                else if (channel_data_ptr->parameter_number_LSB == 1) // Fine tuning
                {
                    channel_data_ptr->fine_tune = ((channel_data_ptr->data_entry_LSB & 0x60) >> 5) + 4 * channel_data_ptr->data_entry_MSB - 256;
                }
                else if (channel_data_ptr->parameter_number_LSB == 2) // Coarse tuning
                {
                    if (channel_data_ptr->data_entry_MSB >= 40 && channel_data_ptr->data_entry_MSB <= 88)
                    {
                        channel_data_ptr->coarse_tune = channel_data_ptr->data_entry_MSB - 64;
                    }
                }
            }
            break;
        case 0x40: // Damper pedal (sustain)
            if (event_data[2] <= 63)
            {
                channel_data_ptr->chflags &= ~CHFLAG_Sustain;
                ControllerSettingsOff(event_data[0] & 0x0F);
            }
            else
            {
                channel_data_ptr->chflags |= CHFLAG_Sustain;
                ControllerSettingsOn(event_data[0] & 0x0F);
            }
            break;
        case 0x42: // Sostenuto
            if (event_data[2] <= 63)
            {
                channel_data_ptr->chflags &= ~CHFLAG_Sostenuto;
                ControllerSettingsOff(event_data[0] & 0x0F);
            }
            else
            {
                channel_data_ptr->chflags |= CHFLAG_Sostenuto;
                ControllerSettingsOn(event_data[0] & 0x0F);
            }
          break;
        case 0x43: // Soft Pedal
            if (event_data[2] <= 63)
            {
                channel_data_ptr->chflags &= ~CHFLAG_Soft;
            }
            else
            {
                channel_data_ptr->chflags |= CHFLAG_Soft;
            }
            break;
        case 0x62: // Non-Registered Parameter Number (LSB)
            channel_data_ptr->parameter_number_LSB = 255;
            break;
        case 0x63: // Non-Registered Parameter Number (MSB)
            channel_data_ptr->parameter_number_MSB = 255;
            break;
        case 0x64: // Registered Parameter Number (LSB)
            channel_data_ptr->parameter_number_LSB = event_data[2];
            break;
        case 0x65: // Registered Parameter Number (MSB)
            channel_data_ptr->parameter_number_MSB = event_data[2];
            break;
        case 0x78: // All sounds off
            AllChannelSoundsOff(event_data[0] & 0x0F);
            break;
        case 0x79: // Reset all controllers
            ResetAllControllers(channel_data_ptr);
            ControllerSettingsOff(event_data[0] & 0x0F);
            break;
        case 0x7B: // All notes off
            AllChannelNotesOff(event_data[0] & 0x0F);
            break;
        default:
            break;
    }
}

static void SystemExclusive(void)
{
    int index;

    // GM reset / GS reset
    if ((event_data[0] == 0xF0 && event_data[1] == 0x7E && event_data[2] == 0x7F && event_data[3] == 0x09 && event_data[4] == 0x01) ||
        (event_data[0] == 0xF0 && event_data[1] == 0x41 && event_data[2] == 0x10 && event_data[3] == 0x42 && event_data[4] == 0x12 && event_data[5] == 0x40 && event_data[6] == 0x00 && event_data[7] == 0x7F && event_data[8] == 0x00 && event_data[9] == 0x41)
       )
    {
        AllVoicesSoundsOff();

        for (index = 0; index < MIDI_CHANNELS; index++)
        {
            ResetChannel(&(channel_data[index]));
        }

        for (index = 0; index < MIDI_CHANNELS; index++)
        {
            ProgramChange(&(stru_C0030080[index]), 0);
        }

        return;
    }

    // change polyphony
    if (event_data[0] == 0xF0 && event_data[1] == 0x44 && event_data[2] == 0x0E && event_data[3] == 0x03)
    {
        switch (event_data[4])
        {
            case 0x10:
                SetMaximumVoices(24);
                maximum_polyphony_new_value = 24;
                return;

            case 0x11:
                SetMaximumVoices(32);
                maximum_polyphony_new_value = 32;
                return;

            case 0x12:
                SetMaximumVoices(48);
                maximum_polyphony_new_value = 48;
                return;

            case 0x13:
                maximum_polyphony = 64;
                maximum_polyphony_new_value = 64;
                return;

            default:
                break;
        }
    }

    // change reverb
    if (event_data[0] == 0xF0 && event_data[1] == 0x44 && event_data[2] == 0x0E && event_data[3] == 0x03)
    {
        switch (event_data[4])
        {
            case 0x20:
                DisableReverb();
                return;

            case 0x21:
                EnableReverb();
                SetReverbShift(1);
                return;

            case 0x22:
                EnableReverb();
                SetReverbShift(0);
                return;

            default:
                break;
        }
    }

    // change effect
    if (event_data[0] == 0xF0 && event_data[1] == 0x44 && event_data[2] == 0x0E && event_data[3] == 0x03)
    {
        switch (event_data[4])
        {
            case 0x40:
                effect_type = 0;
                return;
            case 0x41:
                effect_type = 1;
                return;
            case 0x42:
                effect_type = 2;
                return;
            case 0x43:
                effect_type = 3;
                return;
            case 0x44:
                effect_type = 4;
                return;
            case 0x45:
                effect_type = 5;
                return;
            case 0x46:
                effect_type = 6;
                return;
            case 0x47:
                effect_type = 7;
                return;
            case 0x48:
                effect_type = 8;
                return;
            case 0x49:
                effect_type = 9;
                return;
            case 0x4A:
                effect_type = 10;
                return;
            default:
                break;
        }
    }
}

static int32_t InitializeReverbBuffer(void)
{
    reverb_data_ptr = reverb_data_buffer;
    reverb_data_index = 0;
    return 0;
}

static int32_t DeinitializeReverbBuffer(void)
{
    reverb_data_ptr = NULL;
    return 0;
}

static void EnableReverb(void)
{
    is_reverb_enabled = 1;
}

static void DisableReverb(void)
{
    is_reverb_enabled = 0;
    memset(reverb_data_buffer, 0, sizeof(reverb_data_buffer));
}

static void SetReverbShift(uint32_t shift)
{
    reverb_shift = shift;
}

static void DefragmentVoices(void)
{
    int index1, index2;

    index2 = 0;
    for (index1 = 0; index1 < maximum_polyphony; index1++)
    {
        if (voice_data[index1].note_number != 255) continue;

        if (index2 < index1)
        {
            index2 = index1;
        }
        while (voice_data[index2].note_number == 255)
        {
            index2++;
            if (index2 >= maximum_polyphony) return;
        }

        voice_data[index1] = voice_data[index2];
        voice_data[index2].note_number = 255;
    }
}

static void GenerateOutputData(uint8_t *output_ptr, uint32_t offset1, uint32_t offset2)
{
    int index1, max_active_index;
    unsigned int index2;
    int32_t left;
    int32_t right;
    uint32_t value1;
    uint32_t value2;
    uint32_t value3;
    uint8_t *rom_ptr;
    int32_t value4;
    int32_t value5;
    int32_t value6;
    int32_t value7;
    int32_t reverb_value1;
    int32_t reverb_value2;
    int32_t reverb_value3;
    int32_t reverb_value4;

    DefragmentVoices();

    max_active_index = -1;
    for (index1 = 0; index1 < maximum_polyphony; index1++)
    {
        if (voice_data[index1].note_number != 255)
        {
            max_active_index = index1;
        }
    }

    for (index2 = offset1; index2 < offset2; index2++)
    {
        left = 0;
        right = 0;
        for (index1 = 0; index1 <= max_active_index; index1++)
        {
            value1 = voice_data[index1].field_04;
            value2 = voice_data[index1].field_00 >> 10;
            if (value2 >= value1)
            {
                if (value1 == voice_data[index1].field_08)
                {
                    voice_data[index1].note_number = 255;
                    voice_data[index1].field_28 = 0;
                    continue;
                }

                value3 = (value2 + (voice_data[index1].field_08 & 1) - value1) & ~1;
                if (value3 >= 10)
                {
                    voice_data[index1].field_00 += (8 - value3) << 10;
                    value3 = 8;
                }

                rom_ptr = &(romsxgm_ptr[voice_data[index1].field_04]);
                value4 = ((int32_t)(READ_LE_UINT16(&(rom_ptr[value3])) << 17)) >> 17;
                voice_data[index1].field_1C = (((int32_t)READ_LE_UINT16(&(rom_ptr[10]))) >> (value3 + (value3 >> 1))) & 7;

                voice_data[index1].field_0C[1] = value4;
                voice_data[index1].field_0C[0] = value4 - ((((int32_t)(READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].field_08 & ~1])) << 16)) >> 25) << voice_data[index1].field_1C);

                voice_data[index1].field_00 += (voice_data[index1].field_08 - voice_data[index1].field_04) << 10;
                value2 = voice_data[index1].field_00 >> 10;
                voice_data[index1].field_20 = (value2 & ~1) + 2;
                value5 = READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].field_20]));
                voice_data[index1].field_1C += dword_C00342C0[value5 & 3];
                voice_data[index1].field_0C[2] = voice_data[index1].field_0C[1] + ((((int32_t)(value5 << 23)) >> 25) << voice_data[index1].field_1C);
                voice_data[index1].field_0C[3] = voice_data[index1].field_0C[2] + ((((int32_t)(value5 << 16)) >> 25) << voice_data[index1].field_1C);
            }
            else
            {
                while (voice_data[index1].field_20 <= (value2 & ~1))
                {
                    voice_data[index1].field_20 += 2;
                    if (voice_data[index1].field_04 <= voice_data[index1].field_20)
                    {
                        voice_data[index1].field_0C[0] = voice_data[index1].field_0C[2];
                        voice_data[index1].field_0C[1] = voice_data[index1].field_0C[3];

                        if ((voice_data[index1].field_08 & 1) != 0)
                        {
                            rom_ptr = &(romsxgm_ptr[voice_data[index1].field_04]);
                            value4 = ((int32_t)(READ_LE_UINT16(rom_ptr) << 17)) >> 17;
                            voice_data[index1].field_1C = rom_ptr[10] & 7;

                            voice_data[index1].field_0C[2] = value4;
                        }
                        else
                        {
                            rom_ptr = &(romsxgm_ptr[voice_data[index1].field_04]);
                            value4 = ((int32_t)(READ_LE_UINT16(rom_ptr) << 17)) >> 17;
                            voice_data[index1].field_1C = rom_ptr[10] & 7;

                            voice_data[index1].field_0C[3] = value4;
                            voice_data[index1].field_0C[2] = value4 - ((((int32_t)(READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].field_08 & ~1])) << 16)) >> 25) << voice_data[index1].field_1C);
                        }
                    }
                    else
                    {
                        value5 = READ_LE_UINT16(&(romsxgm_ptr[voice_data[index1].field_20]));
                        voice_data[index1].field_0C[0] = voice_data[index1].field_0C[2];
                        voice_data[index1].field_0C[1] = voice_data[index1].field_0C[3];
                        voice_data[index1].field_1C += dword_C00342C0[value5 & 3];
                        voice_data[index1].field_0C[2] = voice_data[index1].field_0C[1] + ((((int32_t)(value5 << 23)) >> 25) << voice_data[index1].field_1C);
                        voice_data[index1].field_0C[3] = voice_data[index1].field_0C[2] + ((((int32_t)(value5 << 16)) >> 25) << voice_data[index1].field_1C);
                    }
                }
            }

            value7 = voice_data[index1].field_0C[value2 & 1];
            value7 += ((int32_t)((voice_data[index1].field_0C[(value2 & 1) + 1] - value7) * (voice_data[index1].field_00 & 0x3FF))) >> 10;
            value6 = ((int32_t)(15 * voice_data[index1].field_2C + voice_data[index1].field_38)) >> 4;
            value7 = ((int32_t)(value7 * value6)) >> 12;

            voice_data[index1].field_2C = value6;
            voice_data[index1].field_00 += voice_data[index1].field_24;
            left += value7 >> voice_data[index1].field_30;
            right += value7 >> voice_data[index1].field_34;
        }

        if (is_reverb_enabled == 1)
        {
            reverb_value1 = (left + right) >> 3;

            reverb_value2 = reverb_data_ptr[reverb_data_index & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 500) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value2 = reverb_data_ptr[(reverb_data_index + 501) & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 826) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value2 = reverb_data_ptr[(reverb_data_index + 827) & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 1038) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value2 = reverb_data_ptr[(reverb_data_index + 1039) & 0x7FFF];
            reverb_data_ptr[(reverb_data_index + 1176) & 0x7FFF] = reverb_value1 - (reverb_value2 >> 1);
            reverb_value1 = (reverb_value1 >> 1) + reverb_value2;

            reverb_value3 = reverb_value1 >> 1;

            reverb_value4 = reverb_data_ptr[(reverb_data_index + 1177) & 0x7FFF] - ((96 * reverb_data_ptr[(reverb_data_index + 1179) & 0x7FFF]) >> 8);
            reverb_data_ptr[(reverb_data_index + 1178) & 0x7FFF] = reverb_value4 >> 3;
            reverb_data_ptr[(reverb_data_index + 3177) & 0x7FFF] = reverb_value4 + reverb_value3;

            reverb_value4 = reverb_data_ptr[(reverb_data_index + 3178) & 0x7FFF] - ((97 * reverb_data_ptr[(reverb_data_index + 3180) & 0x7FFF]) >> 8);
            reverb_data_ptr[(reverb_data_index + 3179) & 0x7FFF] = reverb_value4 >> 3;
            reverb_data_ptr[(reverb_data_index + 5118) & 0x7FFF] = reverb_value4 + reverb_value3;

            left += (reverb_data_ptr[(reverb_data_index + 1179) & 0x7FFF] + reverb_data_ptr[(reverb_data_index + 3335) & 0x7FFF]) >> reverb_shift;
            right += (reverb_data_ptr[(reverb_data_index + 1339) & 0x7FFF] + reverb_data_ptr[(reverb_data_index + 3180) & 0x7FFF]) >> reverb_shift;

            reverb_data_index = (reverb_data_index + 1) & 0x7FFF;
        }

        if (left > 32767)
        {
            left = 32767;
        }
        else if (left <= -32767)
        {
            left = -32767;
        }

        if (right > 32767)
        {
            right = 32767;
        }
        else if (right <= -32767)
        {
            right = -32767;
        }

        ((int16_t *)output_ptr)[2 * index2] = left;
        ((int16_t *)output_ptr)[2 * index2 + 1] = right;
    }
}

static int32_t InitializeMidiDataBuffer(void)
{
    midi_data_write_index = 0;
    midi_data_read_index = 0;
    return 0;
}

static int32_t EMPTY_DeinitializeMidiDataBuffer(void)
{
    return 0;
}

static void AddByteToMidiDataBuffer(uint8_t value)
{
    uint32_t write_index;

    write_index = midi_data_write_index;
    midi_data_buffer[write_index] = value;
    midi_data_write_index = (write_index + 1) & 0xFFFF;
}

static uint8_t GetValueFromMidiDataBuffer(void)
{
    uint32_t write_index, read_index;
    int index;
    uint32_t event_time;
    uint32_t time_2;
    uint8_t result;

    write_index = midi_data_write_index;
    read_index = midi_data_read_index;
    if (write_index == read_index)
    {
        return 0xFF;
    }

    event_time = 0;
    for (index = 0; index < 4; index++)
    {
        event_time |= midi_data_buffer[read_index] << (8 * index);
        read_index = (read_index + 1) & 0xFFFF;

        if (write_index == read_index)
        {
            midi_data_read_index = read_index;
            return 0xFF;
        }
    }

    time_2 = (system_time_1 >= 600000) ? (system_time_1 - 600000) : 0;

    if ((system_time_1 + 600000 <= event_time) || (time_2 >= event_time))
    {
        AllVoicesSoundsOff();
        midi_data_read_index = 0;
        midi_data_write_index = 0;
        return 0xFF;
    }

    if (event_time + 100 > system_time_1)
    {
        return 0xFF;
    }

    result = midi_data_buffer[read_index];
    midi_data_read_index = (read_index + 1) & 0xFFFF;
    return result;
}

static int32_t InitializePhase(void)
{
    processing_phase = 0;
    return 0;
}

static int32_t EMPTY_DeinitializePhase(void)
{
    return 0;
}

static void sub_C0036A20(Voice_Data *voice_data_ptr)
{
    voice_data_ptr->field_34 = sub_C0036FB0(voice_data_ptr->field_6A >> 8);
    voice_data_ptr->field_30 = sub_C0036FB0(voice_data_ptr->field_6A & 0x1F);
}

static void sub_C0036A80(Voice_Data *voice_data_ptr)
{
    uint32_t offset1;

    offset1 = sub_C00373A0(10, (voice_data_ptr->field_5C >> 8) + sub_C0034970(voice_data_ptr, voice_data_ptr->field_5C & 0xFF));
    offset1 += 4 * (voice_data_ptr->vflags & VFLAG_Mask07);

    if ((voice_data_ptr->vflags & VFLAG_MaskC0) == VFLAG_Value80)
    {
        offset1 += 32;
    }

    voice_data_ptr->field_48 = sub_C0037420(offset1);
    voice_data_ptr->field_4A = sub_C0037400();
    voice_data_ptr->vflags = (voice_data_ptr->vflags & VFLAG_NotMask07) | (voice_data_ptr->field_48 & 7);
}

static void sub_C0036B00(Voice_Data *voice_data_ptr)
{
    uint32_t offset1;
    uint16_t value1;
    int32_t value2;
    int32_t value3;

    offset1 = sub_C00373A0(11, (voice_data_ptr->field_5E >> 8) + sub_C0034970(voice_data_ptr, voice_data_ptr->field_5E & 0xFF));
    offset1 += (voice_data_ptr->vflags & VFLAG_Mask38) >> 1;

    if ((voice_data_ptr->vflags & VFLAG_MaskC0) == VFLAG_Value80)
    {
        offset1 += 32;
    }

    value1 = sub_C0037420(offset1);
    value1 = ((voice_data_ptr->field_62 * (value1 >> 8)) & 0xFF00) | (value1 & 0xFF);
    voice_data_ptr->field_4E = value1;

    if ((((voice_data_ptr->vflags & VFLAG_Mask38) >> 3) == value1) && (voice_data_ptr->field_52 == 0))
    {
        voice_data_ptr->note_number = 0xFF;
        return;
    }

    value2 = sub_C0037400() >> 8;
    if ((value2 & 0xE0) == 0x20)
    {
        value3 = (value2 & 0x1F) << 8;
    }
    else
    {
        value3 = value2;
        if ((value2 & 0xE0) != 0)
        {
            value2 = (value2 >> 5) + 6;
            value3 = (value3 & 0x1F) + 32;
        }
        else
        {
            value2 >>= 2;
            value3 &= 3;
        }
        value3 <<= value2;
    }

    if (value3 > 0x7FFF)
    {
        value3 = 0x7FFF;
    }

    voice_data_ptr->vflags = (voice_data_ptr->vflags & VFLAG_NotMask38) | ((voice_data_ptr->field_4E & 7) << 3);
    voice_data_ptr->field_50 = value3;
}

static void sub_C0036C20(Voice_Data *voice_data_ptr)
{
    int32_t value0;

    value0 = channel_data[voice_data_ptr->channel_num_2 >> 1].expression * channel_data[voice_data_ptr->channel_num_2 >> 1].volume;
    value0 = ((int32_t)(value0 * value0)) >> 13;
    voice_data_ptr->field_64 = ((int32_t)(value0 * voice_data_ptr->field_60)) >> 7;

    sub_C0036A20(voice_data_ptr);
}

static void ProcessPhase(void)
{
    int phase, index, value;
    Channel_Data *channel;

    phase = processing_phase & 7;
    processing_phase++;

    switch ( phase )
    {
        case 0:
            sub_C0037140();

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    voice_data[index].field_54 += dword_C0032188[voice_data[index].field_5A + 112];
                }
            }

            break;

        case 1:
            sub_C0037140();
            sub_C0036FE0();
            break;

        case 2:
            sub_C0037140();
            break;

        case 3:
            sub_C0037140();

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    channel = &(channel_data[voice_data[index].channel_num_2 >> 1]);
                    value = voice_data[index].field_58 + channel->channel_pressure + channel->modulation;
                    if (value > 127)
                    {
                        value = 127;
                    }
                    else if (value < 0)
                    {
                        value = 0;
                    }

                    sub_C0034890(&(voice_data[index]), (int16_t)(voice_data[index].field_44 + (((int32_t)(value * (voice_data[index].field_54 >> 8))) >> 7) + (voice_data[index].field_4C >> 3)));
                }
            }

            break;

        case 4:
            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    sub_C0036C20(&(voice_data[index]));
                }
            }

            sub_C0037140();
            break;

        case 5:
            sub_C0037140();
            sub_C0036FE0();
            break;

        case 6:
            sub_C0037140();
            break;

        case 7:
            sub_C0037140();

            for (index = 0; index < maximum_polyphony; index++)
            {
                if (voice_data[index].note_number != 255)
                {
                    channel = &(channel_data[voice_data[index].channel_num_2 >> 1]);
                    value = voice_data[index].field_58 + channel->channel_pressure + channel->modulation;
                    if (value > 127)
                    {
                        value = 127;
                    }
                    else if (value < 0)
                    {
                        value = 0;
                    }

                    sub_C0034890(&(voice_data[index]), (int16_t)(voice_data[index].field_44 + (((int32_t)(value * (voice_data[index].field_54 >> 8))) >> 7) + (voice_data[index].field_4C >> 3)));
                }
            }

            break;

        default:
            break;
    }
}

static int32_t sub_C0036FB0(int16_t value3)
{
    int32_t value1, value2;

    value1 = 0;
    value2 = 16;
    do
    {
        if (value2 < value3)
        {
            break;
        }
        value1++;
        value2 >>= 1;
    } while (value2);
    return value1;
}

static void sub_C0036FE0(void)
{
    int index;
    int32_t value1, value2, value3;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number == 255) continue;

        value1 = voice_data[index].field_48;
        value2 = voice_data[index].field_4C;
        if (value1 > value2)
        {
            value3 = value2 + voice_data[index].field_4A;
            if (value3 > 32767)
            {
                value3 = 32767;
            }

            if (value1 > value3)
            {
                voice_data[index].field_4C = value3;
                continue;
            }
        }
        else
        {
            value3 = value2 - voice_data[index].field_4A;
            if (value3 < -32767)
            {
                value3 = -32767;
            }

            if (value1 < value3)
            {
                voice_data[index].field_4C = value3;
                continue;
            }
        }

        voice_data[index].field_4C = value1;

        sub_C0036A80(&(voice_data[index]));
    }
}

static void sub_C0037140(void)
{
    int index, choice, index2;
    int32_t value1, value2, value3;

    for (index = 0; index < maximum_polyphony; index++)
    {
        if (voice_data[index].note_number == 255) continue;

        value1 = voice_data[index].field_52;
        value2 = voice_data[index].field_50;
        value3 = voice_data[index].field_4E & 0xFF00;

        if (value3 > value1)
        {
            value1 += value2;
            if (value1 > 32767)
            {
                value1 = 32767;
            }

            choice = (value3 <= value1)?1:0;
        }
        else
        {
            value1 -= value2;
            if (value1 < -32767)
            {
                value1 = -32767;
            }

            choice = (value3 >= value1)?1:0;
        }

        if (choice)
        {
            voice_data[index].field_52 = value3;
            index2 = (value3 & 0x7fff) >> 11;
            voice_data[index].field_28 = word_C00342D0[index2] + (((int32_t)((word_C00342D0[index2 + 1] - word_C00342D0[index2]) * (value3 & 0x07ff))) >> 11);
            sub_C0036B00(&(voice_data[index]));
        }
        else
        {
            voice_data[index].field_52 = value1;
            index2 = (value1 & 0x7fff) >> 11;
            voice_data[index].field_28 = word_C00342D0[index2] + (((int32_t)((word_C00342D0[index2 + 1] - word_C00342D0[index2]) * (value1 & 0x07ff))) >> 11);
        }

        voice_data[index].field_38 = ((int32_t)(voice_data[index].field_28 * voice_data[index].field_64)) >> 14;
    }
}

static int32_t InitializeStructures(void)
{
    int index;

    for (index = 0; index < MAX_VOICES; index++)
    {
        voice_data[index].note_number = 255;
    }

    for (index = 0; index < MIDI_CHANNELS; index++)
    {
        channel_data[index].program_change = 0;
        channel_data[index].pitch_bend = 0;
        channel_data[index].channel_pressure = 0;
        channel_data[index].modulation = 0;
        channel_data[index].volume = 100;
        channel_data[index].pan = 0;
        channel_data[index].expression = 127;
        channel_data[index].chflags &= ~CHFLAG_Sustain;
        channel_data[index].pitch_bend_sense = 512;
        channel_data[index].fine_tune = 0;
        channel_data[index].coarse_tune = 0;
        channel_data[index].parameter_number_LSB = 255;
        channel_data[index].parameter_number_MSB = 255;
        channel_data[index].data_entry_MSB = 0;
        channel_data[index].data_entry_LSB = 0;
    }

    for (index = 0; index < MIDI_CHANNELS; index++)
    {
        ProgramChange(&(stru_C0030080[index]), 0);
    }

    return 0;
}

static int32_t EMPTY_DeinitializeStructures(void)
{
    return 0;
}

static void ResetAllControllers(Channel_Data *channel_data_ptr)
{
    channel_data_ptr->expression = 127;
    channel_data_ptr->pitch_bend = 0;
    channel_data_ptr->channel_pressure = 0;
    channel_data_ptr->parameter_number_LSB = 255;
    channel_data_ptr->modulation = 0;
    channel_data_ptr->parameter_number_MSB = 255;
    channel_data_ptr->data_entry_MSB = 0;
    channel_data_ptr->data_entry_LSB = 0;
    channel_data_ptr->chflags &= ~CHFLAG_Sustain;
}

static void ResetChannel(Channel_Data *channel_data_ptr)
{
    channel_data_ptr->volume = 100;
    channel_data_ptr->program_change = 0;
    channel_data_ptr->expression = 127;
    channel_data_ptr->pitch_bend_sense = 512;
    channel_data_ptr->chflags &= ~CHFLAG_Sustain;
    channel_data_ptr->pitch_bend = 0;
    channel_data_ptr->channel_pressure = 0;
    channel_data_ptr->modulation = 0;
    channel_data_ptr->parameter_number_LSB = 255;
    channel_data_ptr->pan = 0;
    channel_data_ptr->parameter_number_MSB = 255;
    channel_data_ptr->fine_tune = 0;
    channel_data_ptr->data_entry_MSB = 0;
    channel_data_ptr->coarse_tune = 0;
    channel_data_ptr->data_entry_LSB = 0;
}

static uint32_t sub_C00373A0(uint32_t arg_0, int32_t arg_4)
{
    uint8_t *address1;
    uint32_t offset1;
    int32_t offset2;

    address1 = &(romsxgm_ptr[4 * arg_0 + 65588]);
    offset1 = (READ_LE_UINT16(address1 + 2) << 8) + (READ_LE_UINT16(address1) >> 8);
    offset2 = 4 + arg_4 * (int16_t)READ_LE_UINT16(romsxgm_ptr + offset1 + 2);

    rom_offset = offset1 + offset2;
    return rom_offset;
}

static uint16_t sub_C0037400(void)
{
    uint16_t result;

    result = READ_LE_UINT16(romsxgm_ptr + rom_offset);
    rom_offset += 2;
    return result;
}

static int16_t sub_C0037420(uint32_t arg_0)
{
    rom_offset = arg_0 + 2;
    return (int16_t)READ_LE_UINT16(romsxgm_ptr + arg_0);
}

