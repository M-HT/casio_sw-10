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

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include "pe_helper.h"

#ifdef __GNUC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#endif

#pragma pack(1)

typedef struct ATTR_PACKED __MZ_HEADER {
    uint16_t signature; // == 0x5a4d
    uint16_t bytes_in_last_block;
    uint16_t blocks_in_file;
    uint16_t num_relocs;
    uint16_t header_paragraphs;
    uint16_t min_extra_paragraphs;
    uint16_t max_extra_paragraphs;
    uint16_t ss;
    uint16_t sp;
    uint16_t checksum;
    uint16_t ip;
    uint16_t cs;
    uint16_t reloc_table_offset;
    uint16_t overlay_number;
    uint16_t reserved[4];
    uint16_t oem_id;
    uint16_t oem_info;
    uint16_t reserved2[10];
    uint32_t pe_header_offset;
} _MZ_HEADER;

typedef struct ATTR_PACKED __IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} _IMAGE_FILE_HEADER, *P_IMAGE_FILE_HEADER;

typedef struct ATTR_PACKED __IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
} _IMAGE_DATA_DIRECTORY, *P_IMAGE_DATA_DIRECTORY;

typedef struct ATTR_PACKED __IMAGE_OPTIONAL_HEADER {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    _IMAGE_DATA_DIRECTORY ExportTable;
    _IMAGE_DATA_DIRECTORY ImportTable;
    _IMAGE_DATA_DIRECTORY ResourceTable;
    _IMAGE_DATA_DIRECTORY ExceptionTable;
    _IMAGE_DATA_DIRECTORY CertificateTable;
    _IMAGE_DATA_DIRECTORY BaseRelocationTable;
    _IMAGE_DATA_DIRECTORY Debug;
    _IMAGE_DATA_DIRECTORY Architecture;
    _IMAGE_DATA_DIRECTORY GlobalPtr;
    _IMAGE_DATA_DIRECTORY TLSTable;
    _IMAGE_DATA_DIRECTORY LoadConfigTable;
    _IMAGE_DATA_DIRECTORY BoundImport;
    _IMAGE_DATA_DIRECTORY IAT;
    _IMAGE_DATA_DIRECTORY DelayImportDescriptor;
    _IMAGE_DATA_DIRECTORY CLRRuntimeHeader;
    _IMAGE_DATA_DIRECTORY Reserved;
} _IMAGE_OPTIONAL_HEADER, *P_IMAGE_OPTIONAL_HEADER;


typedef struct ATTR_PACKED __IMAGE_NT_HEADERS {
    uint32_t Signature;
    struct __IMAGE_FILE_HEADER FileHeader;
    struct __IMAGE_OPTIONAL_HEADER OptionalHeader;
} _IMAGE_NT_HEADERS, *P_IMAGE_NT_HEADERS;

#pragma pack()


int pe_get_file_address(const char *fname, uint32_t *p_pe_address, uint32_t *p_pe_size, bool *p_pe_relocations, uint32_t *p_pe_section_alignment, uint32_t *p_pe_size_of_headers)
{
    FILE *f;
    long fsize;
    int retval;
    _MZ_HEADER mz_header;
    _IMAGE_NT_HEADERS pe_header;

    f = fopen(fname, "rb");
    if (f == NULL) return 1;

    retval = 2;
    if (fseek(f, 0, SEEK_END)) goto FILE_ERROR;

    retval = 3;
    fsize = ftell(f);
    if (fsize == -1) goto FILE_ERROR;

    retval = 4;
    if (fseek(f, 0, SEEK_SET)) goto FILE_ERROR;

    // check if file is big enough to contain MZ EXE header
    retval = 5;
    if (fsize <= 0x40) goto FILE_ERROR;

    retval = 6;
    if (fread(&mz_header, 1, sizeof(mz_header), f) != sizeof(mz_header)) goto FILE_ERROR;

    // check MZ signature
    retval = 7;
    if (mz_header.signature != 0x5a4d) goto FILE_ERROR;

    // check if file is big enough to contain PE EXE header
    retval = 8;
    if ((unsigned long) fsize <= mz_header.pe_header_offset + sizeof(_IMAGE_NT_HEADERS)) goto FILE_ERROR;

    retval = 9;
    if (fseek(f, mz_header.pe_header_offset, SEEK_SET)) goto FILE_ERROR;

    retval = 10;
    if (fread(&pe_header, 1, sizeof(pe_header), f) != sizeof(pe_header)) goto FILE_ERROR;

    fclose(f);

    // check PE signature
    if (pe_header.Signature != 0x00004550) return 11;

    // check PE header
    if ( (pe_header.FileHeader.Machine != 0x014c) || // 386 or later
         (pe_header.FileHeader.SizeOfOptionalHeader < sizeof(_IMAGE_OPTIONAL_HEADER)) || // optional header size
         (pe_header.OptionalHeader.Magic != 0x10b) || // normal (PE32) executable
         ((pe_header.OptionalHeader.Subsystem != 2) && (pe_header.OptionalHeader.Subsystem != 3)) || // Windows GUI subsystem / Windows character subsystem
         (pe_header.OptionalHeader.NumberOfRvaAndSizes < 16)  || // optional header contents
         (pe_header.FileHeader.NumberOfSections == 0) // number of sections
        )
    {
        return 12;
    }

    if (p_pe_address != NULL)
    {
        *p_pe_address = pe_header.OptionalHeader.ImageBase;
    }
    if (p_pe_size != NULL)
    {
        *p_pe_size = pe_header.OptionalHeader.SizeOfImage;
    }
    if (p_pe_relocations != NULL)
    {
        *p_pe_relocations = (pe_header.FileHeader.Characteristics & 1)?false:true;
    }
    if (p_pe_section_alignment != NULL)
    {
        *p_pe_section_alignment = pe_header.OptionalHeader.SectionAlignment;
    }
    if (p_pe_size_of_headers != NULL)
    {
        *p_pe_size_of_headers = pe_header.OptionalHeader.SizeOfHeaders;
    }

    return 0;

FILE_ERROR:
    fclose(f);
    return retval;
}

