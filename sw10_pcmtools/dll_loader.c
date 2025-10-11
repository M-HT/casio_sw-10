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

#if (defined(__WIN32__) || defined(__WINDOWS__)) && !defined(_WIN32)
    #define _WIN32
#endif

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>

    #define handle HMODULE
    #define load_dll LoadLibraryA
    #define get_proc_address GetProcAddress
    #define free_dll FreeLibrary
#else
    #define _FILE_OFFSET_BITS 64
    #include "pe_loader.h"
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h>
    #include <dirent.h>

    #define handle void *
    #define load_dll pe_load_dll
    #define get_proc_address pe_get_proc_address
    #define free_dll pe_free_dll
#endif

#include "dll_loader.h"


void *load_vlsg_dll(const char *dllname, VLSG_Functions *functions)
{
    handle hVLSG;

    hVLSG = load_dll(dllname);
    if (hVLSG == NULL)
    {
#ifndef _WIN32
        FILE *f;
        char *dllnamecopy, *slash, *filename;
        DIR *dir;
        struct dirent *entry;

        f = fopen(dllname, "rb");
        if (f != NULL)
        {
            fclose(f);
            return NULL;
        }

        dllnamecopy = strdup(dllname);
        if (dllnamecopy == NULL) return NULL;

        slash = strrchr(dllnamecopy, '/');
        if (slash != NULL)
        {
            filename = slash + 1;
            if (slash != dllnamecopy)
            {
                *slash = 0;
                dir = opendir(dllnamecopy);
                *slash = '/';
            }
            else
            {
                dir = opendir("/");
            }
        }
        else
        {
            filename = dllnamecopy;
            dir = opendir(".");
        }

        if (dir == NULL)
        {
            free(dllnamecopy);
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
            free(dllnamecopy);
            return NULL;
        }

        hVLSG = load_dll(dllnamecopy);

        free(dllnamecopy);

        if (hVLSG == NULL)
        {
            return NULL;
        }
#else
        return NULL;
#endif
    }

    functions->VLSG_GetVersion = (uint32_t (*)(void)) get_proc_address(hVLSG, "VLSG_GetVersion");
    functions->VLSG_PlaybackStart = (int (*)(void)) get_proc_address(hVLSG, "VLSG_PlaybackStart");
    functions->VLSG_PlaybackStop = (void (*)(void)) get_proc_address(hVLSG, "VLSG_PlaybackStop");
    functions->VLSG_SetParameter = (int (*)(uint32_t, uint32_t)) get_proc_address(hVLSG, "VLSG_SetParameter");
    functions->VLSG_AddMidiData = (void (*)(const void *, uint32_t)) get_proc_address(hVLSG, "VLSG_AddMidiData");
    functions->VLSG_FillOutputBuffer = (int32_t (*)(uint32_t)) get_proc_address(hVLSG, "VLSG_FillOutputBuffer");
    functions->VLSG_SetFunc_GetTime = (void (*)(uint32_t (*)(void))) get_proc_address(hVLSG, "VLSG_SetFunc_GetTime");
    functions->VLSG_GetName = (const char *(*)(void)) get_proc_address(hVLSG, "VLSG_GetName");

    if ((functions->VLSG_GetVersion == NULL) ||
        (functions->VLSG_PlaybackStart == NULL) ||
        (functions->VLSG_PlaybackStop == NULL) ||
        (functions->VLSG_SetParameter == NULL) ||
        (functions->VLSG_AddMidiData == NULL) ||
        (functions->VLSG_FillOutputBuffer == NULL) ||
        (functions->VLSG_SetFunc_GetTime == NULL) ||
        (functions->VLSG_GetName == NULL)
       )
    {
        free_dll(hVLSG);
        return NULL;
    }

    return hVLSG;
}

void free_vlsg_dll(void *dll)
{
    free_dll((handle)dll);
}

