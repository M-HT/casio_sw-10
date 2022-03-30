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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "pe_loader.h"
#include "pe_helper.h"

#ifdef __GNUC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#endif

#pragma pack(1)

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

typedef struct ATTR_PACKED __IMAGE_SECTION_HEADER {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} _IMAGE_SECTION_HEADER, *P_IMAGE_SECTION_HEADER;

typedef struct ATTR_PACKED __IMAGE_EXPORT_DIRECTORY {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} _IMAGE_EXPORT_DIRECTORY, *P_IMAGE_EXPORT_DIRECTORY;

#pragma pack()


typedef struct {
    uint8_t *allocated_memory;
    uint8_t *image_base;
    bool allocated_address;
    unsigned int image_size;
    P_IMAGE_EXPORT_DIRECTORY export_directory;
    uint32_t export_virtual_address;
    uint32_t export_size;
} dll_info;



static void *host_alloc_page_rw_prot(uint32_t size)
{
    long int pagesize;
    void *memptr;

    pagesize = sysconf(_SC_PAGE_SIZE);
    if ((pagesize != -1) && (pagesize < 4096) && (4096 % pagesize == 0))
    {
        pagesize = 4096;
    }

    if (posix_memalign(&memptr, pagesize, size) == 0)
    {
        return memptr;
    }
    else return NULL;
}

static int host_free_page(void *memptr)
{
    free(memptr);
    return 1;
}

static void *host_alloc_address(uint32_t size, void *address)
{
    void *ret;

    ret = mmap(address, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED)
    {
        return NULL;
    }
    else if (ret != address)
    {
        munmap(ret, size);
        return NULL;
    }
    else
    {
        return ret;
    }
}

static int host_free_address(void *memptr, uint32_t size)
{
    if (munmap(memptr, size))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

static int host_set_page_address_range_protection(void *start, uint32_t length, int32_t executable, int32_t writable)
{
    return mprotect(start, length, PROT_READ | (writable?PROT_WRITE:0) | (executable?PROT_EXEC:0));
}


void *pe_load_dll(const char *fname)
{
    FILE *f;
    int retval;
    P_IMAGE_NT_HEADERS Headers;

    dll_info dll;

    {
        uint32_t pe_address, pe_size, pe_section_alignment, pe_size_of_headers;
        bool pe_relocations;

        retval = pe_get_file_address(fname, &pe_address, &pe_size, &pe_relocations, &pe_section_alignment, &pe_size_of_headers);
        if (retval) return NULL;

        // allocate memory for the code
        dll.allocated_memory = (uint8_t *) host_alloc_address(pe_size, (void *) ((uintptr_t)pe_address));
        if (dll.allocated_memory != NULL)
        {
            dll.allocated_address = true;
            dll.image_base = (uint8_t *) ((uintptr_t)pe_address);
        }
        else
        {
            if (!pe_relocations)
            {
                // no relocations
                return NULL;
            }
            else
            {
                dll.allocated_memory = (uint8_t *) host_alloc_page_rw_prot(pe_size + pe_section_alignment - 1);
                if (dll.allocated_memory == NULL)
                {
                    return NULL;
                }

                dll.allocated_address = false;
                dll.image_base = (uint8_t *) (((uintptr_t) dll.allocated_memory) / pe_section_alignment);
                dll.image_base = (uint8_t *) (((uintptr_t) dll.image_base) * pe_section_alignment);
                if (dll.image_base != dll.allocated_memory)
                {
                    dll.image_base += pe_section_alignment;
                }
            }
        }

        dll.image_size = pe_size;
        memset(dll.image_base, 0, pe_size);

        f = fopen(fname, "rb");
        if (f == NULL) goto MEMORY_ERROR;

        if (fread(dll.image_base, 1, pe_size_of_headers, f) != pe_size_of_headers) goto FILE_ERROR;
    }

    Headers = (P_IMAGE_NT_HEADERS) &(dll.image_base[*((uint32_t *) &(dll.image_base[0x3c]))]);

    // read pages into allocated memory
    {
        unsigned int Entry, Length, NumSections;
        P_IMAGE_SECTION_HEADER SectionHeader;

        SectionHeader = (P_IMAGE_SECTION_HEADER) (((uintptr_t) Headers) + 4 + sizeof(_IMAGE_FILE_HEADER) + Headers->FileHeader.SizeOfOptionalHeader);
        NumSections = Headers->FileHeader.NumberOfSections;

        for (Entry = 0; Entry < NumSections; Entry++)
        {
            Length = ((SectionHeader[Entry].VirtualSize == 0) || (SectionHeader[Entry].VirtualSize > SectionHeader[Entry].SizeOfRawData))?SectionHeader[Entry].SizeOfRawData:SectionHeader[Entry].VirtualSize;

            if ((SectionHeader[Entry].SizeOfRawData != 0) && (SectionHeader[Entry].PointerToRawData != 0))
            {
                if (fseek(f, SectionHeader[Entry].PointerToRawData, SEEK_SET)) goto FILE_ERROR;

                if (fread(dll.image_base + SectionHeader[Entry].VirtualAddress, 1, Length, f) != Length) goto FILE_ERROR;
            }

            //if (host_set_page_address_range_protection(dll.image_base + SectionHeader[Entry].VirtualAddress, Length, (SectionHeader[Entry].Characteristics & 0x20000000)?true:false, (SectionHeader[Entry].Characteristics & 0x80000000)?true:false)) goto FILE_ERROR;
        }
    }

    fclose(f);


    // entry point is not expected

    // apply fixups (relocations)
    if (!dll.allocated_address)
    {
        unsigned int FixupOffset, FixupSize, PageFixupSize, FixupType;
        uint_fast32_t FixupField;
        uint8_t *Fixups, *PageAddress, *FixupAddress;
        uint32_t FixupDifference;


        Fixups = dll.image_base + Headers->OptionalHeader.BaseRelocationTable.VirtualAddress;
        FixupOffset = 0;
        FixupSize = Headers->OptionalHeader.BaseRelocationTable.Size;
        FixupDifference = (uintptr_t)dll.image_base - Headers->OptionalHeader.ImageBase;

        while (FixupOffset < FixupSize)
        {
            PageAddress = dll.image_base + *((uint32_t *) &(Fixups[FixupOffset]));
            PageFixupSize = FixupOffset + *((uint32_t *) &(Fixups[FixupOffset + 4]));
            FixupOffset += 8;
            while (FixupOffset < PageFixupSize)
            {
                FixupField = *((uint16_t *) &(Fixups[FixupOffset]));
                FixupOffset += 2;
                FixupAddress = PageAddress + (FixupField & 0x0fff);
                FixupType = (FixupField & 0xf000) >> 12;

                switch (FixupType)
                {
                    case 0:     // IMAGE_REL_BASED_ABSOLUTE
                        break;

                    case 1:     // IMAGE_REL_BASED_HIGH
                        *((uint16_t *) FixupAddress) += ((FixupDifference >> 16) & 0xffff);
                        break;

                    case 2:     // IMAGE_REL_BASED_LOW
                        *((uint16_t *) FixupAddress) += (FixupDifference & 0xffff);
                        break;

                    case 3:     // IMAGE_REL_BASED_HIGHLOW
                        *((uint32_t *) FixupAddress) += FixupDifference;
                        break;

                    default:
                        goto MEMORY_ERROR;
                }
            }
        }

    }

    // imports are not expected

    // exports
    dll.export_directory = (P_IMAGE_EXPORT_DIRECTORY) (dll.image_base + Headers->OptionalHeader.ExportTable.VirtualAddress);
    dll.export_virtual_address = Headers->OptionalHeader.ExportTable.VirtualAddress;
    dll.export_size = Headers->OptionalHeader.ExportTable.Size;

    memcpy(dll.image_base, &dll, sizeof(dll));


    // set page attributes
    {
        unsigned int Entry, Length, NumSections;
        P_IMAGE_SECTION_HEADER SectionHeader;

        SectionHeader = (P_IMAGE_SECTION_HEADER) (((uintptr_t) Headers) + 4 + sizeof(_IMAGE_FILE_HEADER) + Headers->FileHeader.SizeOfOptionalHeader);
        NumSections = Headers->FileHeader.NumberOfSections;

        for (Entry = 0; Entry < NumSections; Entry++)
        {
            Length = ((SectionHeader[Entry].VirtualSize == 0) || (SectionHeader[Entry].VirtualSize > SectionHeader[Entry].SizeOfRawData))?SectionHeader[Entry].SizeOfRawData:SectionHeader[Entry].VirtualSize;

            if (host_set_page_address_range_protection(dll.image_base + SectionHeader[Entry].VirtualAddress, Length, (SectionHeader[Entry].Characteristics & 0x20000000)?true:false, (SectionHeader[Entry].Characteristics & 0x80000000)?true:false)) goto FILE_ERROR;
        }
    }


    return dll.image_base;

FILE_ERROR:
    fclose(f);

MEMORY_ERROR:
    if (dll.allocated_address)
    {
        host_free_address(dll.allocated_memory, dll.image_size);
    }
    else
    {
        host_free_page(dll.allocated_memory);
    }

    return NULL;
}

static int proc_address_compar(const void *pkey, const void *pmember)
{
    const char *procname = (const char *) ((void **)pkey)[0];
    const char *membername = ((const char *) ((void **)pkey)[1]) + *(uint32_t *)pmember;

    return strcmp(procname, membername);
}

void *pe_get_proc_address(void *dll, const char *procname)
{
    dll_info *dllinfo;
    P_IMAGE_EXPORT_DIRECTORY Directory;
    const void *key[2];
    void *result;
    int nameindex, ordinalvalue;
    uint32_t addressvalue;


    dllinfo = (dll_info *)dll;

    if ((dllinfo == NULL) || (procname == NULL)) return NULL;

    Directory = dllinfo->export_directory;

    key[0] = procname;
    key[1] = dllinfo->image_base;

    result = bsearch(&key[0], dllinfo->image_base + Directory->AddressOfNames, Directory->NumberOfNames, sizeof(uint32_t), &proc_address_compar);
    if (result == NULL) return NULL;

    nameindex = ( (uintptr_t)result - (uintptr_t)(dllinfo->image_base + Directory->AddressOfNames) ) / sizeof(uint32_t);
    ordinalvalue = ((uint16_t *) (dllinfo->image_base + Directory->AddressOfNameOrdinals))[nameindex];
    addressvalue = ((uint32_t *) (dllinfo->image_base + Directory->AddressOfFunctions))[ordinalvalue];

    if ((addressvalue >= dllinfo->export_virtual_address) &&
        (addressvalue <= dllinfo->export_virtual_address + dllinfo->export_size)
       )
    {
        // export forwarder
        return NULL;
    }

    return dllinfo->image_base + addressvalue;
}

void pe_free_dll(void *dll)
{
    dll_info *dllinfo;

    dllinfo = (dll_info *)dll;

    if (dllinfo == NULL) return;

    if (dllinfo->allocated_address)
    {
        host_free_address(dllinfo->allocated_memory, dllinfo->image_size);
    }
    else
    {
        host_free_page(dllinfo->allocated_memory);
    }
}

