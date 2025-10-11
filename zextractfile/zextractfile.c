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

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include "blast/blast.h"

#if defined(__GNUC__)
#define INLINE __inline__
#elif defined(_MSC_VER)
#define INLINE __inline
#define strcasecmp _stricmp
#else
#define INLINE inline
#endif


typedef struct
{
    int files_count;
    int name_length;
    char *name;
} directories_t;

static const char *arg_archive = NULL;
static const char *arg_file = NULL;

static FILE *farchive;

static uint16_t files_count;
static uint32_t archive_length;
static uint32_t entries_offset;
static uint16_t directories_count;

static directories_t *directories = NULL;

static char filename[256];


static void usage(void)
{
    printf("Usage: zextractfile ARCHIVE FILE\n");
    exit(1);
}

static INLINE uint16_t READ_LE_UINT16(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8);
}

static INLINE uint32_t READ_LE_UINT32(const uint8_t *ptr)
{
    return ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
}

static int read_header(void)
{
    uint8_t header[0x33];
    uint32_t signature;

    if (fread(header, 0x33, 1, farchive) != 1)
    {
        return -1;
    }

    signature = READ_LE_UINT32(header);
    files_count = READ_LE_UINT16(header + 0x0c);
    archive_length = READ_LE_UINT32(header + 0x12);
    entries_offset = READ_LE_UINT32(header + 0x29);
    directories_count = READ_LE_UINT16(header + 0x31);

    //printf("signature: 0x%x\n", signature);
    //printf("files_count: %i\n", files_count);
    //printf("archive_length: %i\n", archive_length);
    //printf("entries_offset: %i (0x%x)\n", entries_offset, entries_offset);
    //printf("directories_count: %i\n", directories_count);

    if (signature != 0x8c655d13)
    {
        return -2;
    }

    return 0;
}

static int read_directories(void)
{
    int index;
    uint8_t header[6];
    uint16_t files_count;
    uint16_t chunk_length;
    uint16_t name_length;

    if (fseek(farchive, entries_offset, SEEK_SET))
    {
        return -1;
    }

    if (directories_count == 0)
    {
        return 0;
    }

    directories = (directories_t *)malloc(directories_count * sizeof(directories_t));
    if (directories == NULL)
    {
        return -2;
    }

    for (index = 0; index < directories_count; index++)
    {
        if (fread(header, 6, 1, farchive) != 1)
        {
            return -3;
        }

        files_count = READ_LE_UINT16(header);
        chunk_length = READ_LE_UINT16(header + 2);
        name_length = READ_LE_UINT16(header + 4);

        //printf("files_count: %i\n", files_count);
        //printf("chunk_length: %i\n", chunk_length);
        //printf("name_length: %i\n", name_length);

        directories[index].files_count = files_count;
        directories[index].name_length = name_length;
        directories[index].name = (char *)malloc(name_length + 1);
        if (directories[index].name == NULL)
        {
            return -4;
        }

        if (name_length != 0)
        {
            if (fread(directories[index].name, name_length, 1, farchive) != 1)
            {
                return -5;
            }
        }

        directories[index].name[name_length] = 0;

        //printf("name: %s\n", directories[index].name);

        if (fseek(farchive, chunk_length - name_length - 6, SEEK_CUR))
        {
            return -6;
        }
    }

    return 0;
}

static int compare_filename(int dir_index, const char *filename)
{
    int index;

    if (directories[dir_index].name_length == 0)
    {
        if (strcasecmp(filename, arg_file) == 0)
        {
            return 1;
        }

        return 0;
    }

    for (index = 0; index < directories[dir_index].name_length; index++)
    {
        if (directories[dir_index].name[index] == '/' || directories[dir_index].name[index] == '\\')
        {
            if (arg_file[index] != '/' && arg_file[index] != '\\')
            {
                return 0;
            }
        }
        else
        {
            if (toupper(arg_file[index]) != toupper(directories[dir_index].name[index]))
            {
                return 0;
            }
        }
    }

    if (arg_file[directories[dir_index].name_length] != '/' && arg_file[directories[dir_index].name_length] != '\\')
    {
        return 0;
    }

    if (strcasecmp(filename, arg_file + directories[dir_index].name_length + 1) == 0)
    {
        return 1;
    }

    return 0;
}

static int find_file(void)
{
    int index1, index2;
    uint32_t file_offset;
    uint8_t header[0x1e];
    //uint32_t uncompressed_length;
    uint32_t compressed_length;
    uint16_t chunk_length;
    uint8_t name_length;

    file_offset = 255;
    for (index1 = 0; index1 < directories_count; index1++)
    {
        for (index2 = 0; index2 < directories[index1].files_count; index2++)
        {
            if (fread(header, 0x1e, 1, farchive) != 1)
            {
                return -1;
            }

            //uncompressed_length = READ_LE_UINT32(header + 0x03);
            compressed_length = READ_LE_UINT32(header + 0x07);
            chunk_length = READ_LE_UINT16(header + 0x17);
            name_length = header[0x1d];

            //printf("uncompressed_length: %i\n", uncompressed_length);
            //printf("compressed_length: %i\n", compressed_length);
            //printf("chunk_length: %i\n", chunk_length);
            //printf("name_length: %i\n", name_length);

            if (name_length != 0)
            {
                if (fread(filename, name_length, 1, farchive) != 1)
                {
                    return -2;
                }
            }

            filename[name_length] = 0;

            //printf("name: %s\n", filename);

            // compare filename
            if (compare_filename(index1, filename))
            {
                if (fseek(farchive, file_offset, SEEK_SET))
                {
                    return -3;
                }

                return 1;
            }

            if (fseek(farchive, chunk_length - name_length - 0x1e, SEEK_CUR))
            {
                return -4;
            }

            file_offset += compressed_length;
        }
    }

    return 0;
}

unsigned blast_read(void *how, unsigned char **buf)
{
    static uint8_t buffer[4096];

    *buf = buffer;
    return (unsigned)fread(buffer, 1, 4096, (FILE *)how);
}

int blast_write(void *how, unsigned char *buf, unsigned len)
{
    if (fwrite(buf, len, 1, (FILE *)how) != 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static int extract_file(void)
{
    FILE *fout;
    int err;

#if (defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__) || (defined(__MINGW32__) && defined(_UCRT)) || (defined(__STDC_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__)
    if (fopen_s(&fout, filename, "wb"))
#else
    fout = fopen(filename, "wb");
    if (fout == NULL)
#endif
    {
        return -1;
    }

    err = blast(blast_read, farchive, blast_write, fout, NULL, NULL);

    fclose(fout);

    if (err)
    {
        remove(filename);
        return -2;
    }

    return 0;
}


int main(int argc, char *argv[])
{
    int err;

    if (argc < 3)
    {
        usage();
    }

    arg_archive = argv[1];
    arg_file = argv[2];

    if (arg_archive == NULL || *arg_archive == 0)
    {
        fprintf(stderr, "Missing archive argument\n");
        usage();
    }

    if (arg_file == NULL || *arg_file == 0)
    {
        fprintf(stderr, "Missing file argument\n");
        usage();
    }

#if (defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__) || (defined(__MINGW32__) && defined(_UCRT)) || (defined(__STDC_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__)
    if (fopen_s(&farchive, arg_archive, "rb"))
#else
    farchive = fopen(arg_archive, "rb");
    if (farchive == NULL)
#endif
    {
        fprintf(stderr, "Unable to open archive\n");
        return 2;
    }

    if (read_header() < 0)
    {
        fclose(farchive);
        fprintf(stderr, "Error reading header\n");
        return 3;
    }

    if (read_directories() < 0)
    {
        fclose(farchive);
        fprintf(stderr, "Error reading directories\n");
        return 4;
    }

    err = find_file();
    if (err < 0)
    {
        fclose(farchive);
        fprintf(stderr, "Error reading files\n");
        return 5;
    }

    if (err == 0)
    {
        fclose(farchive);
        fprintf(stderr, "File not found in archive\n");
        return 6;
    }

    if (extract_file() < 0)
    {
        fclose(farchive);
        fprintf(stderr, "Error extracting file\n");
        return 7;
    }

    fclose(farchive);
    return 0;
}

