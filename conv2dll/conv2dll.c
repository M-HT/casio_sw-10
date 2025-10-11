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
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#define MAX_SECTIONS 32

#ifdef __GNUC__
#define ATTR_PACKED __attribute__ ((__packed__))
#else
#define ATTR_PACKED
#endif

#pragma pack(1)
typedef struct ATTR_PACKED _MZHeader_ {
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
    uint32_t le_header_offset;
} MZHeader;

typedef struct ATTR_PACKED _LEHeader_ {
    uint16_t Signature;
    uint8_t  ByteOrdering;
    uint8_t  WordOrdering;
    uint32_t FormatLevel;
    uint16_t CPUType;
    uint16_t OSType;
    uint32_t ModuleVersion;
    uint32_t ModuleFlags;
    uint32_t MouduleNumOfPages;
    uint32_t EIPObjectNum;
    uint32_t EIP;
    uint32_t ESPObjectNum;
    uint32_t ESP;
    uint32_t PageSize;
    uint32_t BytesOnLastPage;
    uint32_t FixupSectionSize;
    uint32_t FixupSectionChecksum;
    uint32_t LoaderSectionSize;
    uint32_t LoaderSectionChecksum;
    uint32_t ObjectTableOffset;
    uint32_t NumObjectsInModule;
    uint32_t ObjectPageMapTableOffset;
    uint32_t ObjectIteratedPagesOffset;
    uint32_t ResourceTableOffset;
    uint32_t NumResourceTableEntries;
    uint32_t ResidentNameTableOffset;
    uint32_t EntryTableOffset;
    uint32_t ModuleDirectivesOffset;
    uint32_t NumModuleDirectives;
    uint32_t FixupPageTableOffset;
    uint32_t FixupRecordTableOffset;
    uint32_t ImportModuleTableOffset;
    uint32_t NumImportModuleEntries;
    uint32_t ImportProcedureTableOffset;
    uint32_t PerPageChecksumOffset;
    uint32_t DataPagesOffset;
    uint32_t NumPreloadPages;
    uint32_t NonResidentNameTableOffset;
    uint32_t NonResidentNameTableLength;
    uint32_t NonResidentNameTableChecksum;
    uint32_t AutoDSObjectNum;
    uint32_t DebugInfoOffset;
    uint32_t DebugInfoLength;
    uint32_t NumInstancePreload;
    uint32_t NumInstanceDemand;
    uint32_t HeapSize;
    uint8_t  Reserved[12];
    uint32_t VersionInfoResourceOffset; // MS-Windows VxD only
    uint32_t VersionInfoResourceSize;   // MS-Windows VxD only
    uint16_t DeviceID;                  // MS-Windows VxD only
    uint16_t DDKVersion;                // MS-Windows VxD only
} LEHeader;

typedef struct ATTR_PACKED _ObjectTableEntry_ {
    uint32_t VirtualSize;
    uint32_t RelocationBaseAddress;
    uint32_t ObjectFlags;
    uint32_t PageTableIndex;
    uint32_t NumPageTableEntries;
    uint32_t Reserved;
} ObjectTableEntry;

typedef struct ATTR_PACKED _ObjectPageMapTableEntry_ {
    uint8_t  Unknown;
    uint8_t  FixupTableIndex_HB; // high byte
    uint8_t  FixupTableIndex_LB; // low byte
    uint8_t  Type;
} ObjectPageMapTableEntry;

typedef struct ATTR_PACKED _VXDVersionResource_ {
    uint8_t cType;
    uint16_t wID;
    uint8_t cName;
    uint16_t wOrdinal;
    uint16_t wFlags;
    uint32_t dwResSize;
} VXDVersionResource;

typedef struct _VS_FIXEDFILEINFO_ {
    uint32_t dwSignature;
    uint32_t dwStrucVersion;
    uint32_t dwFileVersionMS;
    uint32_t dwFileVersionLS;
    uint32_t dwProductVersionMS;
    uint32_t dwProductVersionLS;
    uint32_t dwFileFlagsMask;
    uint32_t dwFileFlags;
    uint32_t dwFileOS;
    uint32_t dwFileType;
    uint32_t dwFileSubtype;
    uint32_t dwFileDateMS;
    uint32_t dwFileDateLS;
} VS_FIXEDFILEINFO;

typedef struct _VXDVersionInfo_ {
    uint16_t cbNode;
    uint16_t cbData;
    char szKey[16]; // should always be 'VS_VERSION_INFO',0
    VS_FIXEDFILEINFO Value;
} VXDVersionInfo;


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


#define IMAGE_FILE_MACHINE_I386 0x14c

#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define IMAGE_FILE_32BIT_MACHINE 0x0100
#define IMAGE_FILE_DEBUG_STRIPPED 0x0200
#define IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP 0x0400
#define IMAGE_FILE_NET_RUN_FROM_SWAP 0x0800
#define IMAGE_FILE_DLL 0x2000

#define IMAGE_SUBSYSTEM_WINDOWS_GUI 2

#define IMAGE_DLL_CHARACTERISTICS_DYNAMIC_BASE 0x0040
#define IMAGE_DLL_CHARACTERISTICS_NX_COMPAT 0x0100
#define IMAGE_DLL_CHARACTERISTICS_NO_SEH 0x0400

#define IMAGE_SCN_CNT_CODE 0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040
#define IMAGE_SCN_MEM_SHARED 0x10000000
#define IMAGE_SCN_MEM_EXECUTE 0x20000000
#define IMAGE_SCN_MEM_READ 0x40000000
#define IMAGE_SCN_MEM_WRITE 0x80000000


static int IgnoreFixup(uint32_t source_offset, uint32_t target_offset)
{
    if ((source_offset >= 0x320D0) && (source_offset < 0x3213C)) return 1;
    if ((target_offset >= 0x320D0) && (target_offset < 0x3213C)) return 2;

    if ((source_offset >= 0x34300) && (source_offset < 0x34400)) return 3;
    if ((target_offset >= 0x34300) && (target_offset < 0x34400)) return 4;

    if ((source_offset >= 0x34400) && (source_offset < 0x34418)) return 5;
    if ((target_offset >= 0x34400) && (target_offset < 0x34418)) return 4;

    if ((source_offset >= 0x3441F) && (source_offset < 0x34480)) return 7;
    if ((target_offset >= 0x3441F) && (target_offset < 0x34480)) return 8;

    return 0;
}

static int CompareRelocationStart(const void *p1, const void *p2)
{
    uint32_t s1 = ((uint32_t *)p1)[0];
    uint32_t s2 = ((uint32_t *)p2)[0];
    if (s1 < s2) return -1;
    else if (s1 > s2) return 1;
    else return 0;
}

static int CompareExportName(const void *p1, const void *p2)
{
    const char *s1 = ((const char **)p1)[0];
    const char *s2 = ((const char **)p2)[0];
    return strcmp(s1, s2);
}

static uint32_t CalcCheckSum(uint8_t *data, int data_length, int PECheckSum)
{
    uint64_t checksum = 0;
    uint64_t top = ((uint64_t)1) << 32;
    int i;

    for (i = 0; i < data_length / 4; i++)
    {
        uint32_t dword;

        if (i == PECheckSum / 4)
        {
            continue;
        }
        dword = *(uint32_t *) (data + i * 4);
        checksum = (checksum & 0xffffffff) + dword + (checksum >> 32);
        if (checksum > top)
        {
            checksum = (checksum & 0xffffffff) + (checksum >> 32);
        }
    }

    if (data_length % 4)
    {
        uint32_t dword;

        dword = 0;

        memcpy(&dword, data + data_length - (data_length % 4), data_length % 4);

        checksum = (checksum & 0xffffffff) + dword + (checksum >> 32);
        if (checksum > top)
        {
            checksum = (checksum & 0xffffffff) + (checksum >> 32);
        }
    }

    checksum = (checksum & 0xffff) + (checksum >> 16);
    checksum = (checksum) + (checksum >> 16);
    checksum = checksum & 0xffff;

    checksum += (uint32_t)data_length;
    return (uint32_t)checksum;

}

static int ConvertVxD(const char *fname, const char *dllname)
{
    FILE *f;
    long fsize;
    int retval;
    uint8_t *mz_file, *le_file, *dll_file;
    unsigned int entry, size, total_size, cur_page, remaining_pages, cur_index, next_index, target_object, num_src_ofs;
    MZHeader *mz_header;
    LEHeader *le_header;
    ObjectTableEntry *obj_table;
    uint32_t *fixup_pagetable;
    uint8_t *fixup_recordtable;
    uint32_t source_offset, target_offset;

    _IMAGE_NT_HEADERS pe_header;
    _IMAGE_SECTION_HEADER pe_section[3];

    #define NUM_EXPORTS 8
    static struct {
        const char *function_name;
        int32_t ordinal;
        uint32_t offset;
    } exported_functions[NUM_EXPORTS] = {
        {"VLSG_GetVersion"      , 1, 0x00000000}, // filled later
        {"VLSG_PlaybackStart"   , 2, 0x000345D0},
        {"VLSG_PlaybackStop"    , 3, 0x00034680},
        {"VLSG_SetParameter"    , 4, 0x00034480},
        {"VLSG_AddMidiData"     , 5, 0x000346B0},
        {"VLSG_FillOutputBuffer", 6, 0x000346D0},
        {"VLSG_SetFunc_GetTime" , 7, 0x00000000}, // filled later
        {"VLSG_GetName"         , 8, 0x00000000}, // filled later
    };

    struct {
        uint32_t source_offset;
        uint32_t target_offset;
    } relocations[1000];
    int num_relocations;

    uint8_t *misc;
    uint32_t misc_offset;

    P_IMAGE_NT_HEADERS dll_pe_header;


    mz_file = NULL;
    dll_file = NULL;

#if (defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__) || (defined(__MINGW32__) && defined(_UCRT)) || (defined(__STDC_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__)
    if (fopen_s(&f, fname, "rb")) return 1;
#else
    f = fopen(fname, "rb");
    if (f == NULL) return 1;
#endif

    // get file size
    retval = 2;
    if (fseek(f, 0, SEEK_END)) goto FILE_ERROR;

    fsize = ftell(f);
    if (fsize == -1) goto FILE_ERROR;

    if (fseek(f, 0, SEEK_SET)) goto FILE_ERROR;

    // check if file is big enough to contain MZ EXE header
    retval = 3;
    if ((unsigned long)fsize <= sizeof(MZHeader)) goto FILE_ERROR;

    // allocate memory for the whole file
    retval = 4;
    mz_file = (uint8_t *)malloc(fsize);
    if (mz_file == NULL) goto FILE_ERROR;

    // read the whole file
    retval = 5;
    if (fread(mz_file, 1, fsize, f) != (unsigned long)fsize) goto FILE_ERROR;

    fclose(f);
    f = NULL;


    mz_header = (MZHeader *) mz_file;

    // check MZ signature
    retval = 11;
    if (mz_header->signature != 0x5a4d) goto MEM_ERROR;

    // check if file can contain LE EXE header
    retval = 12;
    if ((mz_header->reloc_table_offset < 0x40) || (mz_header->le_header_offset < 0x40)) goto MEM_ERROR;

    // check if file is big enough to contain LE EXE header
    retval = 13;
    if ((unsigned long)fsize <= mz_header->le_header_offset + sizeof(LEHeader)) goto MEM_ERROR;


    le_file = mz_file + mz_header->le_header_offset;
    le_header = (LEHeader *) le_file;

    // check LE signature
    retval = 21;
    if (le_header->Signature != 0x454c) goto MEM_ERROR;

    retval = 22;
    if ((le_header->ByteOrdering != 0) || (le_header->WordOrdering != 0) || (le_header->FormatLevel != 0)) goto MEM_ERROR;

    retval = 23;
    if ((le_header->CPUType < 2) || (le_header->OSType != 4)) goto MEM_ERROR;

    retval = 24;
    if (le_header->ModuleFlags != 0x00038000) goto MEM_ERROR;

    retval = 25;
    if ((le_header->NumObjectsInModule == 0) || (le_header->NumObjectsInModule > MAX_SECTIONS)) goto MEM_ERROR;


    obj_table = (ObjectTableEntry *) (le_file + le_header->ObjectTableOffset);


    memset(&pe_header, 0, sizeof(_IMAGE_NT_HEADERS));

    pe_header.Signature = 0x00004550;

    // COFF File Header
    pe_header.FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    pe_header.FileHeader.NumberOfSections = 3;
    pe_header.FileHeader.SizeOfOptionalHeader = sizeof(_IMAGE_OPTIONAL_HEADER);
    pe_header.FileHeader.Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_32BIT_MACHINE | IMAGE_FILE_DEBUG_STRIPPED | IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP | IMAGE_FILE_NET_RUN_FROM_SWAP | IMAGE_FILE_DLL;

    // Optional Header Standard Fields
    pe_header.OptionalHeader.Magic = 0x10b; // normal (PE32) executable

    pe_header.OptionalHeader.SizeOfCode = 0x3200;
    pe_header.OptionalHeader.SizeOfInitializedData = 4096 + 0x34400;
    pe_header.OptionalHeader.SizeOfUninitializedData = 0;
    pe_header.OptionalHeader.AddressOfEntryPoint = 0;
    pe_header.OptionalHeader.BaseOfCode = 4096 + 4096 + 0x35000;
    pe_header.OptionalHeader.BaseOfData = 4096;

    // Optional Header Windows-Specific Fields
    pe_header.OptionalHeader.ImageBase = 0x10000000;
    pe_header.OptionalHeader.SectionAlignment = 4096;
    pe_header.OptionalHeader.FileAlignment = 512;
    pe_header.OptionalHeader.MajorOperatingSystemVersion = 5; // Windows 2000
    pe_header.OptionalHeader.MinorOperatingSystemVersion = 0;
    pe_header.OptionalHeader.MajorImageVersion = 0;
    pe_header.OptionalHeader.MinorImageVersion = 0;
    pe_header.OptionalHeader.MajorSubsystemVersion = 5; // Windows 2000
    pe_header.OptionalHeader.MinorSubsystemVersion = 0;

    pe_header.OptionalHeader.SizeOfImage = ((/*0x0a00*/ 4096 + 4096 + 0x35000 + 0x303E) + 4095) & 0xfffff000;
    pe_header.OptionalHeader.SizeOfHeaders = 0x0a00;
    pe_header.OptionalHeader.CheckSum = 0; // filled later
    pe_header.OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI;
    pe_header.OptionalHeader.DllCharacteristics = IMAGE_DLL_CHARACTERISTICS_DYNAMIC_BASE | IMAGE_DLL_CHARACTERISTICS_NX_COMPAT | IMAGE_DLL_CHARACTERISTICS_NO_SEH;
    pe_header.OptionalHeader.SizeOfStackReserve = 256*1024;
    pe_header.OptionalHeader.SizeOfStackCommit = 4096;
    pe_header.OptionalHeader.SizeOfHeapReserve = 1024*1024;
    pe_header.OptionalHeader.SizeOfHeapCommit = 4096;

    pe_header.OptionalHeader.NumberOfRvaAndSizes = 16;

    // Optional Header Data Directories
    pe_header.OptionalHeader.ExportTable.VirtualAddress = 4096;
    pe_header.OptionalHeader.ExportTable.Size = 0; // filled later

    pe_header.OptionalHeader.BaseRelocationTable.VirtualAddress = 0; // filled later
    pe_header.OptionalHeader.BaseRelocationTable.Size = 0; // filled later

    // Section Table
    memset(&pe_section, 0, sizeof(pe_section));

    //pe_section
    pe_section[0].Name[0] = 'V';
    pe_section[0].Name[1] = 'L';
    pe_section[0].Name[2] = 'S';
    pe_section[0].Name[3] = 'G';
    pe_section[0].Name[4] = 'M';
    pe_section[0].Name[5] = 'I';
    pe_section[0].Name[6] = 'S';
    pe_section[0].Name[7] = 'C';
    pe_section[0].VirtualSize = 4096; // filled later
    pe_section[0].VirtualAddress = 4096;
    pe_section[0].SizeOfRawData = 4096;
    pe_section[0].PointerToRawData = 0x0a00;
    pe_section[0].PointerToRelocations = 0;
    pe_section[0].NumberOfRelocations = 0;
    pe_section[0].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;

    pe_section[1].Name[0] = 'V';
    pe_section[1].Name[1] = 'L';
    pe_section[1].Name[2] = 'S';
    pe_section[1].Name[3] = 'G';
    pe_section[1].Name[4] = 'D';
    pe_section[1].Name[5] = 'A';
    pe_section[1].Name[6] = 'T';
    pe_section[1].Name[7] = 'A';
    pe_section[1].VirtualSize = 0x34300; // 256 free bytes at end
    pe_section[1].VirtualAddress = 4096 + 4096;
    pe_section[1].SizeOfRawData = 0x34400;
    pe_section[1].PointerToRawData = 0x1a00;
    pe_section[1].PointerToRelocations = 0;
    pe_section[1].NumberOfRelocations = 0;
    pe_section[1].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;

    pe_section[2].Name[0] = 'V';
    pe_section[2].Name[1] = 'L';
    pe_section[2].Name[2] = 'S';
    pe_section[2].Name[3] = 'G';
    pe_section[2].Name[4] = 'C';
    pe_section[2].Name[5] = 'O';
    pe_section[2].Name[6] = 'D';
    pe_section[2].Name[7] = 'E';
    pe_section[2].VirtualSize = 0x303E; // 450 free bytes at end
    pe_section[2].VirtualAddress = 4096 + 4096 + 0x35000;
    pe_section[2].SizeOfRawData = 0x3200;
    pe_section[2].PointerToRawData = 0x1a00 + 0x34400;
    pe_section[2].PointerToRelocations = 0; // filled later
    pe_section[2].NumberOfRelocations = 0; // filled later
    pe_section[2].Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;



//33AD0 - 320D0 = 1A00 - start of section in file (= 13*512)


    retval = 41;
    dll_file = (uint8_t *) malloc(fsize);
    if (dll_file == NULL) goto MEM_ERROR;

    memcpy(dll_file, mz_file, fsize);


    num_relocations = 0;


    // rewrite code
    {
        uint8_t *code;
        uint32_t code_offset;

        code_offset = 0x34400;
        code = dll_file + 0x1a00 + 0x34400;


        // function VLSG_GetVersion

        // mov eax, 0x0103
        code[0] = 0xb8;
        *(uint32_t *)(code + 1) = 0x0103;

        // ret
        code[5] = 0xc3;

        exported_functions[0].offset = code_offset;


        // function VLSG_SetFunc_GetTime

        // mov eax, [esp+4]
        code[6] = 0x8b;
        code[7] = 0x44;
        code[8] = 0x24;
        code[9] = 0x04;

        // mov [funcaddr], eax
        code[10] = 0xa3;

        // add relocation
        relocations[num_relocations].source_offset = code_offset + 11;
        relocations[num_relocations].target_offset = 0x320CC;
        num_relocations++;

        // ret
        code[15] = 0xc3;

        exported_functions[6].offset = code_offset + 6;


        // function VLSG_GetName

        // mov eax, offset name
        code[0x10] = 0xb8;

        // add relocation
        relocations[num_relocations].source_offset = code_offset + 0x11;
        relocations[num_relocations].target_offset = 0x3213C;
        num_relocations++;

        // ret
        code[0x15] = 0xc3;

        // int 3 (align)
        code[0x16] = 0xcc;

        exported_functions[7].offset = code_offset + 0x10;


        // rewrite function VMM_Get_System_Time
        code[0x18] = 0xff;
        code[0x19] = 0x25;

        // add relocation
        relocations[num_relocations].source_offset = code_offset + 0x1a;
        relocations[num_relocations].target_offset = 0x320CC;
        num_relocations++;

        // int 3 (align)
        code[0x1e] = 0xcc;

    }


    fprintf(stderr, "headers size: %i\n", (int)(mz_header->le_header_offset + sizeof(_IMAGE_NT_HEADERS) + sizeof(pe_section)));


    misc_offset = 4096;
    misc = dll_file + 0x0a00;

    {
        P_IMAGE_EXPORT_DIRECTORY Directory;
        int exports_size, exports_index;

        uint32_t *AddressTable, *NameLookupTable;
        uint16_t *OrdinalLookupTable;
        char *NameTable;

        Directory = (P_IMAGE_EXPORT_DIRECTORY)misc;

        memset(Directory, 0, sizeof(_IMAGE_EXPORT_DIRECTORY));

        Directory->Base = 1;
        Directory->NumberOfFunctions = NUM_EXPORTS;
        Directory->NumberOfNames = NUM_EXPORTS;
        Directory->AddressOfFunctions = misc_offset + sizeof(_IMAGE_EXPORT_DIRECTORY);
        Directory->AddressOfNames = Directory->AddressOfFunctions + NUM_EXPORTS * sizeof(uint32_t);
        Directory->AddressOfNameOrdinals = Directory->AddressOfNames + NUM_EXPORTS * sizeof(uint32_t);
        Directory->Name = Directory->AddressOfNameOrdinals + NUM_EXPORTS * sizeof(uint16_t);

        AddressTable = (uint32_t *)(misc + Directory->AddressOfFunctions - misc_offset);
        NameLookupTable = (uint32_t *)(misc + Directory->AddressOfNames - misc_offset);
        OrdinalLookupTable = (uint16_t *)(misc + Directory->AddressOfNameOrdinals - misc_offset);
        NameTable = (char *)(misc + Directory->Name - misc_offset);

#if (defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__)
        strcpy_s(NameTable, RSIZE_MAX, "VLSG.DLL");
#else
        strcpy(NameTable, "VLSG.DLL");
#endif
        NameTable += strlen(NameTable) + 1;

        for (exports_index = 0; exports_index < NUM_EXPORTS; exports_index++)
        {
            AddressTable[exports_index] = (exported_functions[exports_index].offset - 0x34400) + 4096 + 4096 + 0x35000;
        }

        // sort export table by name
        qsort(&exported_functions[0], NUM_EXPORTS, sizeof(exported_functions[0]), CompareExportName);

        for (exports_index = 0; exports_index < NUM_EXPORTS; exports_index++)
        {
            OrdinalLookupTable[exports_index] = exported_functions[exports_index].ordinal - 1;
            NameLookupTable[exports_index] = (uint32_t)(((uintptr_t)NameTable - (uintptr_t)Directory) + misc_offset);

#if (defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__)
            strcpy_s(NameTable, RSIZE_MAX, exported_functions[exports_index].function_name);
#else
            strcpy(NameTable, exported_functions[exports_index].function_name);
#endif
            NameTable += strlen(NameTable) + 1;
        }

        exports_size = (int)((uintptr_t)NameTable - (uintptr_t)Directory);

        fprintf(stderr, "exports size: %i\n", exports_size);

        exports_size = (exports_size + 511) & ~511;

        pe_section[0].VirtualSize = exports_size;

        pe_header.OptionalHeader.ExportTable.Size = exports_size;
        pe_header.OptionalHeader.BaseRelocationTable.VirtualAddress = misc_offset + exports_size;

        misc_offset += exports_size;
        misc += exports_size;
    }



    // calculate total needed size
    total_size = 0;
    for (entry = 0; entry < le_header->NumObjectsInModule; entry++)
    {
        size = obj_table[entry].VirtualSize / le_header->PageSize;
        size *= le_header->PageSize;
        if (size != obj_table[entry].VirtualSize) size += le_header->PageSize;

        obj_table[entry].Reserved = total_size;
        total_size += size;
    }

    // prepare relocations
    {
        fixup_pagetable = (uint32_t *) (le_file + le_header->FixupPageTableOffset);
        fixup_recordtable = le_file + le_header->FixupRecordTableOffset;
        for (entry = 0; entry < le_header->NumObjectsInModule; entry++)
        {
            cur_page = obj_table[entry].PageTableIndex - 1;
            for (remaining_pages = obj_table[entry].NumPageTableEntries; remaining_pages != 0; remaining_pages--)
            {
                cur_index = fixup_pagetable[cur_page];
                next_index = fixup_pagetable[cur_page + 1];

                while (cur_index < next_index)
                {
                    switch (fixup_recordtable[cur_index]) // Source type
                    {
                        case 7: // 32-bit Offset fixup
                            break;
                        case 0x27: // 32-bit Offset fixup + Source List Flag
                            break;
                        default:
                            retval = 32 + 256 * fixup_recordtable[cur_index];
                            goto MEM_ERROR;
                    }

                    switch (fixup_recordtable[cur_index + 1]) // Target Flags
                    {
                        case 0: // Internal reference + 16-bit Target Offset Flag
                            break;
                        case 0x10: // Internal reference + 32-bit Target Offset Flag
                            break;
                        default:
                            retval = 33 + 256 * fixup_recordtable[cur_index + 1];
                            goto MEM_ERROR;
                    }

                    if (fixup_recordtable[cur_index] & 0x20) // Source List Flag
                    {
                        num_src_ofs = fixup_recordtable[cur_index + 2];

                        target_object = *((uint8_t *) &(fixup_recordtable[cur_index + 3]));

                        if (fixup_recordtable[cur_index + 1]) // 32-bit Target Offset
                        {
                            target_offset = *((int32_t *) &(fixup_recordtable[cur_index + 4]));
                            cur_index += 8;
                        }
                        else
                        {
                            target_offset = *((uint16_t *) &(fixup_recordtable[cur_index + 4]));
                            cur_index += 6;
                        }
                    }
                    else
                    {
                        num_src_ofs = 0;

                        source_offset = (obj_table[entry].NumPageTableEntries - remaining_pages) * le_header->PageSize
                                     + *((int16_t *) &(fixup_recordtable[cur_index + 2]));

                        target_object = *((uint8_t *) &(fixup_recordtable[cur_index + 4]));

                        if (fixup_recordtable[cur_index + 1]) // 32-bit Target Offset
                        {
                            target_offset = *((int32_t *) &(fixup_recordtable[cur_index + 5]));
                            cur_index += 9;
                        }
                        else
                        {
                            target_offset = *((uint16_t *) &(fixup_recordtable[cur_index + 5]));
                            cur_index += 7;
                        }


                        relocations[num_relocations].source_offset = obj_table[entry].Reserved + source_offset;
                        relocations[num_relocations].target_offset = obj_table[target_object - 1].Reserved + target_offset;
                        if (!IgnoreFixup(relocations[num_relocations].source_offset, relocations[num_relocations].target_offset))
                        {
                            num_relocations++;
                        }
                        //fprintf(stdout, "source_offset: 0x%x  target_offset: 0x%x  ignored: %i\n", source_offset, target_offset, IgnoreFixup(source_offset, target_offset));
                    }

                    for (; num_src_ofs != 0; num_src_ofs--)
                    {
                        source_offset = (obj_table[entry].NumPageTableEntries - remaining_pages) * le_header->PageSize
                                     + *((int16_t *) &(fixup_recordtable[cur_index]));

                        cur_index += 2;

                        relocations[num_relocations].source_offset = obj_table[entry].Reserved + source_offset;
                        relocations[num_relocations].target_offset = obj_table[target_object - 1].Reserved + target_offset;
                        if (!IgnoreFixup(relocations[num_relocations].source_offset, relocations[num_relocations].target_offset))
                        {
                            num_relocations++;
                        }
                        //fprintf(stdout, "source_offset: 0x%x  target_offset: 0x%x  ignored: %i\n", source_offset, target_offset, IgnoreFixup(source_offset, target_offset));
                    }

                }

                cur_page++;
            }
        }


        // sort relocations
        qsort(&relocations[0], num_relocations, sizeof(relocations[0]), CompareRelocationStart);

        fprintf(stderr, "num relocations: %i\n", num_relocations);
    }



    {
        int relocations_size, relocations_index, used_relocations;
        uint32_t prev_page, cur_page, prev_start, dll_source_offset;
        uint8_t *block_start, *relocs;

        //relocations_size = 0;

        block_start = NULL;
        relocs = misc;
        used_relocations = 0;

        prev_page = 1;
        prev_start = 0xffffffff;
        for (relocations_index = 0; relocations_index < num_relocations; relocations_index++)
        {
            dll_source_offset = 4096 + 4096 + 0x35000 + (relocations[relocations_index].source_offset - 0x34400);

            cur_page = dll_source_offset & 0xfffff000;
            if (cur_page != prev_page)
            {
                if (block_start != NULL)
                {
                    *(uint32_t *)(block_start + 4) = (uint32_t)(relocs - block_start);
                }

                block_start = relocs;

                *(uint32_t *)relocs = cur_page;

                //relocations_size += 8;
                relocs += 8;
                prev_page = cur_page;
            }

            if (relocations[relocations_index].source_offset == prev_start)
            {
                if (relocations[relocations_index].target_offset != relocations[relocations_index - 1].target_offset)
                {
                    fprintf(stderr, "double relocation: 0x%x -> 0x%x & 0x%x\n", prev_start, relocations[relocations_index].target_offset, relocations[relocations_index - 1].target_offset);
                }
            } else {
                prev_start = relocations[relocations_index].source_offset;

                /*if (*(uint32_t *)(mz_file + 0x1a00 + prev_start) != (0x10000000 + relocations[relocations_index].target_offset))
                {
                    fprintf(stderr, "bad relocation: 0x%x -> 0x%x (0x%x vs 0x%x)\n", prev_start, relocations[relocations_index].target_offset, *(uint32_t *)(mz_file + 0x1a00 + prev_start), 0x10000000 + relocations[relocations_index].target_offset);
                }*/

                //fprintf(stdout, "source_offset: 0x%x  target_offset: 0x%x\n", relocations[relocations_index].source_offset, relocations[relocations_index].target_offset);

                // write relocation
                if (relocations[relocations_index].target_offset >= 0x34400)
                {
                    // reloc to code section
                    *(uint32_t *)(dll_file + 0x1a00 + prev_start) = 0x10000000 + 4096 + 4096 + 0x35000 + (relocations[relocations_index].target_offset - 0x34400);
                }
                else
                {
                    // reloc to data section
                    *(uint32_t *)(dll_file + 0x1a00 + prev_start) = 0x10000000 + 4096 + 4096 + relocations[relocations_index].target_offset;
                }

                *(uint16_t *)relocs = (3 << 12) | (dll_source_offset - cur_page);

                //relocations_size += 2;
                relocs += 2;
                used_relocations++;
            }
        }

        *(uint32_t *)(block_start + 4) = (uint32_t)(relocs - block_start);

        relocations_size = (int)(relocs - misc);

        fprintf(stderr, "relocations size: %i\n", relocations_size);

        //relocations_size = (relocations_size + 511) & ~511;

        pe_header.OptionalHeader.BaseRelocationTable.Size = relocations_size;

        pe_section[0].VirtualSize += relocations_size;

        //pe_section[2].PointerToRelocations = misc_offset;
        //pe_section[2].NumberOfRelocations = relocations_size;

    }

    // version info resource
    if (le_header->VersionInfoResourceOffset != 0)
    {
        VXDVersionResource *vxd_version_res = (VXDVersionResource *)(mz_file + le_header->VersionInfoResourceOffset);
        VXDVersionInfo *vxd_version_info = (VXDVersionInfo *)(sizeof(VXDVersionResource) + (uintptr_t)vxd_version_res);
        // VXDStringFileInfo
        // VXDVarFileInfo

        if (vxd_version_res->cType == 0xff &&
            vxd_version_res->wID == 16 &&
            vxd_version_res->cName == 0xff &&
            memcmp(vxd_version_info->szKey, "VS_VERSION_INFO", 16) == 0
           )
        {
            // todo: convert resource to portable executable resource (there should be enough free space in VLSGMISC section)

            // https://docs.microsoft.com/en-us/windows/win32/menurc/vs-versioninfo
            // https://gitcode.net/mirrors/jrsoftware/issrc/-/blob/is-5_5_1/Projects/Verinfo.pas
        }
    }


    memcpy(dll_file + mz_header->le_header_offset, &pe_header, sizeof(_IMAGE_NT_HEADERS));
    memcpy(dll_file + mz_header->le_header_offset + sizeof(_IMAGE_NT_HEADERS), pe_section, sizeof(pe_section));

    dll_pe_header = (P_IMAGE_NT_HEADERS) (dll_file + mz_header->le_header_offset);
    dll_pe_header->OptionalHeader.CheckSum = CalcCheckSum(dll_file, fsize, mz_header->le_header_offset + offsetof(_IMAGE_NT_HEADERS, OptionalHeader.CheckSum));


    retval = 42;
#if (defined(_MSC_VER) && __STDC_WANT_SECURE_LIB__) || (defined(__MINGW32__) && defined(_UCRT)) || (defined(__STDC_LIB_EXT1__) && __STDC_WANT_LIB_EXT1__)
    if (fopen_s(&f, dllname, "wb")) goto MEM_ERROR;
#else
    f = fopen(dllname, "wb");
    if (f == NULL) goto MEM_ERROR;
#endif

    fwrite(dll_file, 1, fsize, f);
    fclose(f);

    free(dll_file);

    free(mz_file);

    return 0;

MEM_ERROR:
    if (dll_file != NULL) free(dll_file);
    if (mz_file != NULL) free(mz_file);
    return retval;

FILE_ERROR:
    if (mz_file != NULL) free(mz_file);
    if (f != NULL) fclose(f);
    return retval;
}



int main(int argc, char *argv[])
{
    int err;

    err = ConvertVxD("VLSG.VXD", "VLSG.DLL");

    fprintf(stderr, "ConvertVxD: %i\n", err);

    if (err) return err;

    return 0;
}

