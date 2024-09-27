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

#if !defined(_DLL_LOADER_H_INCLUDED_)
#define _DLL_LOADER_H_INCLUDED_

#include <stdint.h>

typedef struct
{
    uint32_t (*VLSG_GetVersion)(void);
    int (*VLSG_PlaybackStart)(void);
    void (*VLSG_PlaybackStop)(void);
    int (*VLSG_SetParameter)(uint32_t ID, uint32_t value);
    void (*VLSG_AddMidiData)(const void *event, uint32_t length);
    int32_t (*VLSG_FillOutputBuffer)(uint32_t output_counter);
    void (*VLSG_SetFunc_GetTime)(uint32_t (*func)(void));
    const char *(*VLSG_GetName)(void);
} VLSG_Functions;

#ifdef __cplusplus
extern "C" {
#endif

extern void *load_vlsg_dll(const char *dllname, VLSG_Functions *functions);
extern void free_vlsg_dll(void *dll);

#ifdef __cplusplus
}
#endif

#endif

