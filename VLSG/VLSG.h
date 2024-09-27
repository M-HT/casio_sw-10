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

#if !defined(_VLSG_H_INCLUDED_)
#define _VLSG_H_INCLUDED_

#include <stdint.h>

enum ParameterType
{
    PARAMETER_OutputBuffer  = 1,
    PARAMETER_ROMAddress    = 2,
    PARAMETER_Frequency     = 3,
    PARAMETER_Polyphony     = 4,
    PARAMETER_Effect        = 5,
};

uint32_t VLSG_GetVersion(void);
const char *VLSG_GetName(void);
void VLSG_SetFunc_GetTime(uint32_t (*get_time)(void));

int32_t VLSG_SetParameter(uint32_t type, uintptr_t value);
int32_t VLSG_PlaybackStart(void);
int32_t VLSG_PlaybackStop(void);
void VLSG_AddMidiData(const uint8_t *ptr, uint32_t len);
int32_t VLSG_FillOutputBuffer(uint32_t output_buffer_counter);

#endif

