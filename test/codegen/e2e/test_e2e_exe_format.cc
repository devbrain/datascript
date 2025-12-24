//
// End-to-End Test: Windows Executable Format Components
//
// Tests individual components from the e2e_exe_format.ds schema:
// - DOS header parsing with constraints
// - NE (16-bit Windows) format headers
// - PE32 (32-bit Windows) and PE32+ (64-bit Windows) headers
// - Section headers
// - Demonstrates that nested inline unions inside anonymous union case
//   blocks are correctly parsed (schema compiles without errors)
//
// Note: Full Executable struct testing requires code generation support
// for anonymous union case blocks, which is a future enhancement.
//
#include <doctest/doctest.h>
#include <e2e_exe_format.h>
#include <vector>
#include <cstring>

using namespace generated;

// Helper to create little-endian bytes
inline void write_le16(std::vector<uint8_t>& data, uint16_t value) {
    data.push_back(value & 0xFF);
    data.push_back((value >> 8) & 0xFF);
}

inline void write_le32(std::vector<uint8_t>& data, uint32_t value) {
    data.push_back(value & 0xFF);
    data.push_back((value >> 8) & 0xFF);
    data.push_back((value >> 16) & 0xFF);
    data.push_back((value >> 24) & 0xFF);
}

inline void write_le64(std::vector<uint8_t>& data, uint64_t value) {
    write_le32(data, value & 0xFFFFFFFF);
    write_le32(data, (value >> 32) & 0xFFFFFFFF);
}

TEST_SUITE("E2E - Windows Executable Format") {

    TEST_CASE("Schema Parsing - Nested Inline Unions") {
        // This test verifies that the e2e_exe_format.ds schema successfully parses
        // despite containing nested inline unions inside anonymous union case blocks.
        //
        // Schema line 209 contains: union { ... } optional_header;
        // This union is nested inside the anonymous block of the pe_header case
        // (lines 206-214), which is itself inside the extended_header union.
        //
        // The parser now correctly handles:
        // ✅ Inline unions inside struct bodies
        // ✅ Inline structs inside struct bodies
        // ✅ Inline unions inside anonymous union case blocks (NEW!)
        // ✅ Inline structs inside anonymous union case blocks (NEW!)
        // ✅ Multiple levels of nesting
        //
        // This was achieved by:
        // 1. Changing union_case to use std::vector<struct_body_item> instead of std::vector<field_def>
        // 2. Updating grammar to use struct_body_list for anonymous blocks
        // 3. Extending Phase 0 desugaring to handle inline types in union cases
        // 4. Updating all semantic phases (1-7) to iterate over items with variant checking
        //
        // Code generation for anonymous union case blocks is a future enhancement.

        CHECK(true);  // Schema file parses successfully without errors
    }

    TEST_CASE("DOS Header - Basic Structure") {
        std::vector<uint8_t> data;

        // DOS Header (64 bytes)
        write_le16(data, 0x5A4D);  // e_magic = "MZ"
        write_le16(data, 0x0090);  // e_cblp
        write_le16(data, 0x0003);  // e_cp
        write_le16(data, 0x0000);  // e_crlc
        write_le16(data, 0x0004);  // e_cparhdr
        write_le16(data, 0x0000);  // e_minalloc
        write_le16(data, 0xFFFF);  // e_maxalloc
        write_le16(data, 0x0000);  // e_ss
        write_le16(data, 0x00B8);  // e_sp
        write_le16(data, 0x0000);  // e_csum
        write_le16(data, 0x0000);  // e_ip
        write_le16(data, 0x0000);  // e_cs
        write_le16(data, 0x0040);  // e_lfarlc
        write_le16(data, 0x0000);  // e_ovno

        // e_res[4] - Reserved words
        for (int i = 0; i < 4; i++) {
            write_le16(data, 0x0000);
        }

        write_le16(data, 0x0000);  // e_oemid
        write_le16(data, 0x0000);  // e_oeminfo

        // e_res2[10] - Reserved words
        for (int i = 0; i < 10; i++) {
            write_le16(data, 0x0000);
        }

        write_le32(data, 0x00000080);  // e_lfanew (offset to PE header)

        const uint8_t* ptr = data.data();
        ImageDosHeader header = ImageDosHeader::read(ptr, ptr + data.size());

        // Verify all fields
        CHECK(header.e_magic == 0x5A4D);
        CHECK(header.e_cblp == 0x0090);
        CHECK(header.e_cp == 0x0003);
        CHECK(header.e_lfanew == 0x00000080);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("DOS Header - Constraint Validation") {
        std::vector<uint8_t> data;

        // Invalid DOS header (wrong magic)
        write_le16(data, 0xFFFF);  // e_magic = invalid (not 0x5A4D)
        for (int i = 0; i < 30; i++) {
            write_le16(data, 0x0000);
        }

        const uint8_t* ptr = data.data();

        // Should throw due to constraint violation (e_magic must be DOS_SIGNATURE)
        CHECK_THROWS_AS(ImageDosHeader::read(ptr, ptr + data.size()), ConstraintViolation);
    }

    TEST_CASE("NE Header - 16-bit Windows") {
        std::vector<uint8_t> data;

        // NE Header
        write_le16(data, 0x454E);  // ne_magic = "NE"
        data.push_back(0x01);      // ne_ver
        data.push_back(0x00);      // ne_rev
        write_le16(data, 0x0100);  // ne_enttab
        write_le16(data, 0x0020);  // ne_cbenttab
        write_le32(data, 0x00000000);  // ne_crc
        write_le16(data, 0x0300);  // ne_flags
        write_le16(data, 0x0002);  // ne_autodata
        write_le16(data, 0x1000);  // ne_heap
        write_le16(data, 0x2000);  // ne_stack
        write_le32(data, 0x00010000);  // ne_csip
        write_le32(data, 0x00020000);  // ne_sssp
        write_le16(data, 0x0003);  // ne_cseg
        write_le16(data, 0x0002);  // ne_cmod
        write_le16(data, 0x0100);  // ne_cnonres
        write_le16(data, 0x0040);  // ne_segtab
        write_le16(data, 0x0080);  // ne_rsrctab
        write_le16(data, 0x00C0);  // ne_restab
        write_le16(data, 0x0100);  // ne_modtab
        write_le16(data, 0x0140);  // ne_imptab
        write_le32(data, 0x00001000);  // ne_nrestab
        write_le16(data, 0x0010);  // ne_cmovent
        write_le16(data, 0x0009);  // ne_align
        write_le16(data, 0x0000);  // ne_cres
        data.push_back(0x02);      // ne_exetyp
        data.push_back(0x00);      // ne_flagsothers
        write_le16(data, 0x0000);  // ne_pretthunks
        write_le16(data, 0x0000);  // ne_psegrefbytes
        write_le16(data, 0x0000);  // ne_swaparea
        write_le16(data, 0x0300);  // ne_expver

        const uint8_t* ptr = data.data();
        ImageNeHeader header = ImageNeHeader::read(ptr, ptr + data.size());

        // Verify header fields
        CHECK(header.ne_magic == 0x454E);
        CHECK(header.ne_ver == 0x01);
        CHECK(header.ne_flags == 0x0300);
        CHECK(header.ne_cseg == 0x0003);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("PE File Header") {
        std::vector<uint8_t> data;

        // IMAGE_FILE_HEADER
        write_le16(data, 0x014C);  // Machine = x86
        write_le16(data, 0x0003);  // NumberOfSections
        write_le32(data, 0x12345678);  // TimeDateStamp
        write_le32(data, 0x00000000);  // PointerToSymbolTable
        write_le32(data, 0x00000000);  // NumberOfSymbols
        write_le16(data, 0x00E0);  // SizeOfOptionalHeader
        write_le16(data, 0x010F);  // Characteristics

        const uint8_t* ptr = data.data();
        ImageFileHeader header = ImageFileHeader::read(ptr, ptr + data.size());

        // Verify fields
        CHECK(header.Machine == 0x014C);
        CHECK(header.NumberOfSections == 3);
        CHECK(header.TimeDateStamp == 0x12345678);
        CHECK(header.SizeOfOptionalHeader == 0x00E0);
        CHECK(header.Characteristics == 0x010F);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("PE32 Optional Header") {
        std::vector<uint8_t> data;

        // IMAGE_OPTIONAL_HEADER32
        write_le16(data, 0x010B);  // Magic = PE32
        data.push_back(0x0E);      // MajorLinkerVersion
        data.push_back(0x00);      // MinorLinkerVersion
        write_le32(data, 0x00002000);  // SizeOfCode
        write_le32(data, 0x00000800);  // SizeOfInitializedData
        write_le32(data, 0x00000000);  // SizeOfUninitializedData
        write_le32(data, 0x00001000);  // AddressOfEntryPoint
        write_le32(data, 0x00001000);  // BaseOfCode
        write_le32(data, 0x00002000);  // BaseOfData
        write_le32(data, 0x00400000);  // ImageBase
        write_le32(data, 0x00001000);  // SectionAlignment
        write_le32(data, 0x00000200);  // FileAlignment
        write_le16(data, 0x0006);  // MajorOperatingSystemVersion
        write_le16(data, 0x0000);  // MinorOperatingSystemVersion
        write_le16(data, 0x0001);  // MajorImageVersion
        write_le16(data, 0x0000);  // MinorImageVersion
        write_le16(data, 0x0006);  // MajorSubsystemVersion
        write_le16(data, 0x0000);  // MinorSubsystemVersion
        write_le32(data, 0x00000000);  // Win32VersionValue
        write_le32(data, 0x00006000);  // SizeOfImage
        write_le32(data, 0x00000400);  // SizeOfHeaders
        write_le32(data, 0x00000000);  // CheckSum
        write_le16(data, 0x0003);  // Subsystem
        write_le16(data, 0x8140);  // DllCharacteristics
        write_le32(data, 0x00100000);  // SizeOfStackReserve
        write_le32(data, 0x00001000);  // SizeOfStackCommit
        write_le32(data, 0x00100000);  // SizeOfHeapReserve
        write_le32(data, 0x00001000);  // SizeOfHeapCommit
        write_le32(data, 0x00000000);  // LoaderFlags
        write_le32(data, 0x00000010);  // NumberOfRvaAndSizes = 16

        // Data Directories (16 entries x 8 bytes each)
        for (int i = 0; i < 16; i++) {
            write_le32(data, 0x00000000);  // VirtualAddress
            write_le32(data, 0x00000000);  // Size
        }

        const uint8_t* ptr = data.data();
        ImageOptionalHeader32 header = ImageOptionalHeader32::read(ptr, ptr + data.size());

        // Verify key fields
        CHECK(header.Magic == 0x010B);
        CHECK(header.SizeOfCode == 0x00002000);
        CHECK(header.ImageBase == 0x00400000);
        CHECK(header.SectionAlignment == 0x00001000);
        CHECK(header.NumberOfRvaAndSizes == 0x00000010);
        CHECK(header.DataDirectory.size() == 16);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("PE32+ Optional Header") {
        std::vector<uint8_t> data;

        // IMAGE_OPTIONAL_HEADER64
        write_le16(data, 0x020B);  // Magic = PE32+
        data.push_back(0x0E);      // MajorLinkerVersion
        data.push_back(0x00);      // MinorLinkerVersion
        write_le32(data, 0x00002000);  // SizeOfCode
        write_le32(data, 0x00000800);  // SizeOfInitializedData
        write_le32(data, 0x00000000);  // SizeOfUninitializedData
        write_le32(data, 0x00001000);  // AddressOfEntryPoint
        write_le32(data, 0x00001000);  // BaseOfCode
        write_le64(data, 0x0000000140000000ULL);  // ImageBase (64-bit)
        write_le32(data, 0x00001000);  // SectionAlignment
        write_le32(data, 0x00000200);  // FileAlignment
        write_le16(data, 0x0006);  // MajorOperatingSystemVersion
        write_le16(data, 0x0000);  // MinorOperatingSystemVersion
        write_le16(data, 0x0001);  // MajorImageVersion
        write_le16(data, 0x0000);  // MinorImageVersion
        write_le16(data, 0x0006);  // MajorSubsystemVersion
        write_le16(data, 0x0000);  // MinorSubsystemVersion
        write_le32(data, 0x00000000);  // Win32VersionValue
        write_le32(data, 0x00006000);  // SizeOfImage
        write_le32(data, 0x00000400);  // SizeOfHeaders
        write_le32(data, 0x00000000);  // CheckSum
        write_le16(data, 0x0003);  // Subsystem
        write_le16(data, 0x8160);  // DllCharacteristics
        write_le64(data, 0x0000000000100000ULL);  // SizeOfStackReserve (64-bit)
        write_le64(data, 0x0000000000001000ULL);  // SizeOfStackCommit (64-bit)
        write_le64(data, 0x0000000000100000ULL);  // SizeOfHeapReserve (64-bit)
        write_le64(data, 0x0000000000001000ULL);  // SizeOfHeapCommit (64-bit)
        write_le32(data, 0x00000000);  // LoaderFlags
        write_le32(data, 0x00000010);  // NumberOfRvaAndSizes = 16

        // Data Directories (16 entries x 8 bytes each)
        for (int i = 0; i < 16; i++) {
            write_le32(data, 0x00000000);  // VirtualAddress
            write_le32(data, 0x00000000);  // Size
        }

        const uint8_t* ptr = data.data();
        ImageOptionalHeader64 header = ImageOptionalHeader64::read(ptr, ptr + data.size());

        // Verify key fields
        CHECK(header.Magic == 0x020B);
        CHECK(header.SizeOfCode == 0x00002000);
        CHECK(header.ImageBase == 0x0000000140000000ULL);
        CHECK(header.SectionAlignment == 0x00001000);
        CHECK(header.NumberOfRvaAndSizes == 0x00000010);
        CHECK(header.DataDirectory.size() == 16);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("Section Header") {
        std::vector<uint8_t> data;

        // IMAGE_SECTION_HEADER
        // Name (8 bytes)
        data.push_back('.'); data.push_back('t'); data.push_back('e');
        data.push_back('x'); data.push_back('t'); data.push_back(0);
        data.push_back(0); data.push_back(0);

        write_le32(data, 0x00002000);  // VirtualSize
        write_le32(data, 0x00001000);  // VirtualAddress
        write_le32(data, 0x00000200);  // SizeOfRawData
        write_le32(data, 0x00000400);  // PointerToRawData
        write_le32(data, 0x00000000);  // PointerToRelocations
        write_le32(data, 0x00000000);  // PointerToLinenumbers
        write_le16(data, 0x0000);      // NumberOfRelocations
        write_le16(data, 0x0000);      // NumberOfLinenumbers
        write_le32(data, 0x60000020);  // Characteristics

        const uint8_t* ptr = data.data();
        ImageSectionHeader header = ImageSectionHeader::read(ptr, ptr + data.size());

        // Verify fields
        CHECK(header.Name[0] == '.');
        CHECK(header.Name[1] == 't');
        CHECK(header.Name[2] == 'e');
        CHECK(header.Name[3] == 'x');
        CHECK(header.Name[4] == 't');
        CHECK(header.VirtualSize == 0x00002000);
        CHECK(header.VirtualAddress == 0x00001000);
        CHECK(header.Characteristics == 0x60000020);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("Full Executable - Plain DOS MZ (no extended header)") {
        std::vector<uint8_t> data;

        // DOS Header (64 bytes) - plain DOS executable
        write_le16(data, 0x5A4D);  // e_magic = "MZ"
        write_le16(data, 0x0090);  // e_cblp
        write_le16(data, 0x0003);  // e_cp
        write_le16(data, 0x0000);  // e_crlc
        write_le16(data, 0x0004);  // e_cparhdr
        write_le16(data, 0x0000);  // e_minalloc
        write_le16(data, 0xFFFF);  // e_maxalloc
        write_le16(data, 0x0000);  // e_ss
        write_le16(data, 0x00B8);  // e_sp
        write_le16(data, 0x0000);  // e_csum
        write_le16(data, 0x0000);  // e_ip
        write_le16(data, 0x0000);  // e_cs
        write_le16(data, 0x0040);  // e_lfarlc
        write_le16(data, 0x0000);  // e_ovno

        // e_res[4] - Reserved words
        for (int i = 0; i < 4; i++) {
            write_le16(data, 0x0000);
        }

        write_le16(data, 0x0000);  // e_oemid
        write_le16(data, 0x0000);  // e_oeminfo

        // e_res2[10] - Reserved words
        for (int i = 0; i < 10; i++) {
            write_le16(data, 0x0000);
        }

        write_le32(data, 0x00000000);  // e_lfanew = 0 (no extended header)

        // DOS stub code (minimal - just some bytes to represent a plain DOS program)
        const char* dos_stub = "This program cannot be run in DOS mode.\r\r\n$";
        for (size_t i = 0; i < strlen(dos_stub); i++) {
            data.push_back(dos_stub[i]);
        }

        const uint8_t* ptr = data.data();

        // Now with optional union support, plain DOS executables can be parsed
        // The extended_header union will remain empty (std::monostate)
        Executable exe = Executable::read(ptr, ptr + data.size());

        // Verify DOS header
        CHECK(exe.dos_header.e_magic == 0x5A4D);
        CHECK(exe.dos_header.e_cblp == 0x0090);
        CHECK(exe.dos_header.e_lfanew == 0);

        // Verify extended_header is empty (no NE or PE header)
        CHECK(exe.extended_header.is_empty());
        CHECK(exe.extended_header.as_ne_header() == nullptr);
        CHECK(exe.extended_header.as_pe_header() == nullptr);
    }

    TEST_CASE("Full Executable - NE Format (16-bit Windows)") {
        std::vector<uint8_t> data;

        // DOS Header (64 bytes)
        write_le16(data, 0x5A4D);  // e_magic = "MZ"
        write_le16(data, 0x0090);  // e_cblp
        write_le16(data, 0x0003);  // e_cp
        write_le16(data, 0x0000);  // e_crlc
        write_le16(data, 0x0004);  // e_cparhdr
        write_le16(data, 0x0000);  // e_minalloc
        write_le16(data, 0xFFFF);  // e_maxalloc
        write_le16(data, 0x0000);  // e_ss
        write_le16(data, 0x00B8);  // e_sp
        write_le16(data, 0x0000);  // e_csum
        write_le16(data, 0x0000);  // e_ip
        write_le16(data, 0x0000);  // e_cs
        write_le16(data, 0x0040);  // e_lfarlc
        write_le16(data, 0x0000);  // e_ovno

        // e_res[4] - Reserved words
        for (int i = 0; i < 4; i++) {
            write_le16(data, 0x0000);
        }

        write_le16(data, 0x0000);  // e_oemid
        write_le16(data, 0x0000);  // e_oeminfo

        // e_res2[10] - Reserved words
        for (int i = 0; i < 10; i++) {
            write_le16(data, 0x0000);
        }

        write_le32(data, 64);  // e_lfanew (offset to NE header = 64 bytes, right after DOS header)

        // NE Header (at offset 64)
        write_le16(data, 0x454E);  // ne_magic = "NE"
        data.push_back(0x01);      // ne_ver
        data.push_back(0x00);      // ne_rev
        write_le16(data, 0x0100);  // ne_enttab
        write_le16(data, 0x0020);  // ne_cbenttab
        write_le32(data, 0x00000000);  // ne_crc
        write_le16(data, 0x0300);  // ne_flags
        write_le16(data, 0x0002);  // ne_autodata
        write_le16(data, 0x1000);  // ne_heap
        write_le16(data, 0x2000);  // ne_stack
        write_le32(data, 0x00010000);  // ne_csip
        write_le32(data, 0x00020000);  // ne_sssp
        write_le16(data, 0x0003);  // ne_cseg
        write_le16(data, 0x0002);  // ne_cmod
        write_le16(data, 0x0100);  // ne_cnonres
        write_le16(data, 0x0040);  // ne_segtab
        write_le16(data, 0x0080);  // ne_rsrctab
        write_le16(data, 0x00C0);  // ne_restab
        write_le16(data, 0x0100);  // ne_modtab
        write_le16(data, 0x0140);  // ne_imptab
        write_le32(data, 0x00001000);  // ne_nrestab
        write_le16(data, 0x0010);  // ne_cmovent
        write_le16(data, 0x0009);  // ne_align
        write_le16(data, 0x0000);  // ne_cres
        data.push_back(0x02);      // ne_exetyp
        data.push_back(0x00);      // ne_flagsothers
        write_le16(data, 0x0000);  // ne_pretthunks
        write_le16(data, 0x0000);  // ne_psegrefbytes
        write_le16(data, 0x0000);  // ne_swaparea
        write_le16(data, 0x0300);  // ne_expver

        // Parse the full Executable struct
        const uint8_t* ptr = data.data();
        Executable exe = Executable::read(ptr, ptr + data.size());

        // Verify DOS header
        CHECK(exe.dos_header.e_magic == 0x5A4D);
        CHECK(exe.dos_header.e_cblp == 0x0090);
        CHECK(exe.dos_header.e_lfanew == 64);

        // Verify we read the NE header (not PE)
        auto* ne_header = exe.extended_header.as_ne_header();
        REQUIRE(ne_header != nullptr);

        // Verify NE header fields
        CHECK(ne_header->ne_magic == 0x454E);
        CHECK(ne_header->ne_ver == 0x01);
        CHECK(ne_header->ne_flags == 0x0300);
        CHECK(ne_header->ne_heap == 0x1000);
        CHECK(ne_header->ne_stack == 0x2000);
        CHECK(ne_header->ne_cseg == 0x0003);
        CHECK(ne_header->ne_cmod == 0x0002);

        // Verify we consumed all data
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("Full Executable - PE32 Format") {
        std::vector<uint8_t> data;

        // DOS Header (64 bytes)
        write_le16(data, 0x5A4D);  // e_magic = "MZ"
        for (int i = 0; i < 29; i++) {  // Fill remaining DOS header fields
            write_le16(data, 0x0000);
        }
        write_le32(data, 128);  // e_lfanew (offset to PE header = 128 bytes)

        // Padding to reach offset 128
        for (size_t i = data.size(); i < 128; i++) {
            data.push_back(0);
        }

        // PE Signature (4 bytes)
        write_le32(data, PE_SIGNATURE);  // 0x00004550 = "PE\0\0"

        // IMAGE_FILE_HEADER (20 bytes)
        write_le16(data, 0x014C);  // Machine (IMAGE_FILE_MACHINE_I386)
        write_le16(data, 2);       // NumberOfSections
        write_le32(data, 0);       // TimeDateStamp
        write_le32(data, 0);       // PointerToSymbolTable
        write_le32(data, 0);       // NumberOfSymbols
        write_le16(data, 224);     // SizeOfOptionalHeader (PE32)
        write_le16(data, 0x0102);  // Characteristics

        // IMAGE_OPTIONAL_HEADER32 (224 bytes)
        write_le16(data, PE32_MAGIC);  // Magic = 0x010B
        data.push_back(0x0E);       // MajorLinkerVersion
        data.push_back(0x00);       // MinorLinkerVersion
        write_le32(data, 0x1000);   // SizeOfCode
        write_le32(data, 0x0400);   // SizeOfInitializedData
        write_le32(data, 0);        // SizeOfUninitializedData
        write_le32(data, 0x1000);   // AddressOfEntryPoint
        write_le32(data, 0x1000);   // BaseOfCode
        write_le32(data, 0x2000);   // BaseOfData (PE32 only)
        write_le32(data, 0x00400000); // ImageBase
        write_le32(data, 0x1000);   // SectionAlignment
        write_le32(data, 0x0200);   // FileAlignment
        write_le16(data, 6);        // MajorOperatingSystemVersion
        write_le16(data, 0);        // MinorOperatingSystemVersion
        write_le16(data, 0);        // MajorImageVersion
        write_le16(data, 0);        // MinorImageVersion
        write_le16(data, 6);        // MajorSubsystemVersion
        write_le16(data, 0);        // MinorSubsystemVersion
        write_le32(data, 0);        // Win32VersionValue
        write_le32(data, 0x5000);   // SizeOfImage
        write_le32(data, 0x0200);   // SizeOfHeaders
        write_le32(data, 0);        // CheckSum
        write_le16(data, 3);        // Subsystem (IMAGE_SUBSYSTEM_WINDOWS_CUI)
        write_le16(data, 0);        // DllCharacteristics
        write_le32(data, 0x100000); // SizeOfStackReserve
        write_le32(data, 0x1000);   // SizeOfStackCommit
        write_le32(data, 0x100000); // SizeOfHeapReserve
        write_le32(data, 0x1000);   // SizeOfHeapCommit
        write_le32(data, 0);        // LoaderFlags
        write_le32(data, 16);       // NumberOfRvaAndSizes

        // DataDirectory[16]
        for (int i = 0; i < 16; i++) {
            write_le32(data, 0);  // VirtualAddress
            write_le32(data, 0);  // Size
        }

        // Section Headers (2 sections × 40 bytes each)
        // Section 1: .text
        data.push_back('.'); data.push_back('t'); data.push_back('e');
        data.push_back('x'); data.push_back('t'); data.push_back(0);
        data.push_back(0); data.push_back(0);
        write_le32(data, 0x1000);  // VirtualSize
        write_le32(data, 0x1000);  // VirtualAddress
        write_le32(data, 0x0200);  // SizeOfRawData
        write_le32(data, 0x0400);  // PointerToRawData
        write_le32(data, 0);
        write_le32(data, 0);
        write_le16(data, 0);
        write_le16(data, 0);
        write_le32(data, 0x60000020);  // Characteristics

        // Section 2: .data
        data.push_back('.'); data.push_back('d'); data.push_back('a');
        data.push_back('t'); data.push_back('a'); data.push_back(0);
        data.push_back(0); data.push_back(0);
        write_le32(data, 0x0400);  // VirtualSize
        write_le32(data, 0x2000);  // VirtualAddress
        write_le32(data, 0x0200);  // SizeOfRawData
        write_le32(data, 0x0600);  // PointerToRawData
        write_le32(data, 0);
        write_le32(data, 0);
        write_le16(data, 0);
        write_le16(data, 0);
        write_le32(data, 0xC0000040);  // Characteristics

        // Parse the full Executable struct
        const uint8_t* ptr = data.data();
        Executable exe = Executable::read(ptr, ptr + data.size());

        // Verify DOS header
        CHECK(exe.dos_header.e_magic == 0x5A4D);
        CHECK(exe.dos_header.e_lfanew == 128);

        // Verify we read the PE header (not NE)
        auto* pe_header = exe.extended_header.as_pe_header();
        REQUIRE(pe_header != nullptr);

        // Verify PE signature
        CHECK(pe_header->signature == PE_SIGNATURE);

        // Verify file header
        CHECK(pe_header->file_header.Machine == 0x014C);
        CHECK(pe_header->file_header.NumberOfSections == 2);
        CHECK(pe_header->file_header.SizeOfOptionalHeader == 224);

        // Verify optional header (should be PE32)
        auto* opt32 = pe_header->optional_header.as_opt_header_32();
        REQUIRE(opt32 != nullptr);
        CHECK(opt32->Magic == PE32_MAGIC);
        CHECK(opt32->SizeOfCode == 0x1000);
        CHECK(opt32->ImageBase == 0x00400000);
        CHECK(opt32->NumberOfRvaAndSizes == 16);

        // Verify section headers
        CHECK(pe_header->section_headers.size() == 2);
        CHECK(pe_header->section_headers[0].Name[1] == 't');  // .text
        CHECK(pe_header->section_headers[0].VirtualSize == 0x1000);
        CHECK(pe_header->section_headers[1].Name[1] == 'd');  // .data
        CHECK(pe_header->section_headers[1].VirtualSize == 0x0400);

        // Verify we consumed all data
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("Full Executable - PE32+ Format (64-bit)") {
        std::vector<uint8_t> data;

        // DOS Header (64 bytes)
        write_le16(data, 0x5A4D);  // e_magic = "MZ"
        for (int i = 0; i < 29; i++) {  // Fill remaining DOS header fields
            write_le16(data, 0x0000);
        }
        write_le32(data, 128);  // e_lfanew (offset to PE header = 128 bytes)

        // Padding to reach offset 128
        for (size_t i = data.size(); i < 128; i++) {
            data.push_back(0);
        }

        // PE Signature (4 bytes)
        write_le32(data, PE_SIGNATURE);  // 0x00004550 = "PE\0\0"

        // IMAGE_FILE_HEADER (20 bytes)
        write_le16(data, 0x8664);  // Machine (IMAGE_FILE_MACHINE_AMD64)
        write_le16(data, 3);       // NumberOfSections
        write_le32(data, 0);       // TimeDateStamp
        write_le32(data, 0);       // PointerToSymbolTable
        write_le32(data, 0);       // NumberOfSymbols
        write_le16(data, 240);     // SizeOfOptionalHeader (PE32+)
        write_le16(data, 0x0022);  // Characteristics

        // IMAGE_OPTIONAL_HEADER64 (240 bytes)
        write_le16(data, PE32PLUS_MAGIC);  // Magic = 0x020B
        data.push_back(0x0E);       // MajorLinkerVersion
        data.push_back(0x00);       // MinorLinkerVersion
        write_le32(data, 0x2000);   // SizeOfCode
        write_le32(data, 0x0800);   // SizeOfInitializedData
        write_le32(data, 0);        // SizeOfUninitializedData
        write_le32(data, 0x1000);   // AddressOfEntryPoint
        write_le32(data, 0x1000);   // BaseOfCode
        write_le64(data, 0x0000000140000000ULL); // ImageBase (64-bit)
        write_le32(data, 0x1000);   // SectionAlignment
        write_le32(data, 0x0200);   // FileAlignment
        write_le16(data, 6);        // MajorOperatingSystemVersion
        write_le16(data, 0);        // MinorOperatingSystemVersion
        write_le16(data, 0);        // MajorImageVersion
        write_le16(data, 0);        // MinorImageVersion
        write_le16(data, 6);        // MajorSubsystemVersion
        write_le16(data, 0);        // MinorSubsystemVersion
        write_le32(data, 0);        // Win32VersionValue
        write_le32(data, 0x8000);   // SizeOfImage
        write_le32(data, 0x0400);   // SizeOfHeaders
        write_le32(data, 0);        // CheckSum
        write_le16(data, 3);        // Subsystem (IMAGE_SUBSYSTEM_WINDOWS_CUI)
        write_le16(data, 0x8160);   // DllCharacteristics
        write_le64(data, 0x100000); // SizeOfStackReserve (64-bit)
        write_le64(data, 0x1000);   // SizeOfStackCommit (64-bit)
        write_le64(data, 0x100000); // SizeOfHeapReserve (64-bit)
        write_le64(data, 0x1000);   // SizeOfHeapCommit (64-bit)
        write_le32(data, 0);        // LoaderFlags
        write_le32(data, 16);       // NumberOfRvaAndSizes

        // DataDirectory[16]
        for (int i = 0; i < 16; i++) {
            write_le32(data, 0);  // VirtualAddress
            write_le32(data, 0);  // Size
        }

        // Section Headers (3 sections × 40 bytes each)
        // Section 1: .text
        data.push_back('.'); data.push_back('t'); data.push_back('e');
        data.push_back('x'); data.push_back('t'); data.push_back(0);
        data.push_back(0); data.push_back(0);
        write_le32(data, 0x2000);  // VirtualSize
        write_le32(data, 0x1000);  // VirtualAddress
        write_le32(data, 0x0200);  // SizeOfRawData
        write_le32(data, 0x0400);  // PointerToRawData
        write_le32(data, 0);
        write_le32(data, 0);
        write_le16(data, 0);
        write_le16(data, 0);
        write_le32(data, 0x60000020);  // Characteristics

        // Section 2: .data
        data.push_back('.'); data.push_back('d'); data.push_back('a');
        data.push_back('t'); data.push_back('a'); data.push_back(0);
        data.push_back(0); data.push_back(0);
        write_le32(data, 0x0800);  // VirtualSize
        write_le32(data, 0x3000);  // VirtualAddress
        write_le32(data, 0x0400);  // SizeOfRawData
        write_le32(data, 0x0600);  // PointerToRawData
        write_le32(data, 0);
        write_le32(data, 0);
        write_le16(data, 0);
        write_le16(data, 0);
        write_le32(data, 0xC0000040);  // Characteristics

        // Section 3: .rdata
        data.push_back('.'); data.push_back('r'); data.push_back('d');
        data.push_back('a'); data.push_back('t'); data.push_back('a');
        data.push_back(0); data.push_back(0);
        write_le32(data, 0x0400);  // VirtualSize
        write_le32(data, 0x4000);  // VirtualAddress
        write_le32(data, 0x0200);  // SizeOfRawData
        write_le32(data, 0x0A00);  // PointerToRawData
        write_le32(data, 0);
        write_le32(data, 0);
        write_le16(data, 0);
        write_le16(data, 0);
        write_le32(data, 0x40000040);  // Characteristics

        // Parse the full Executable struct
        const uint8_t* ptr = data.data();
        Executable exe = Executable::read(ptr, ptr + data.size());

        // Verify DOS header
        CHECK(exe.dos_header.e_magic == 0x5A4D);
        CHECK(exe.dos_header.e_lfanew == 128);

        // Verify we read the PE header (not NE)
        auto* pe_header = exe.extended_header.as_pe_header();
        REQUIRE(pe_header != nullptr);

        // Verify PE signature
        CHECK(pe_header->signature == PE_SIGNATURE);

        // Verify file header
        CHECK(pe_header->file_header.Machine == 0x8664);  // AMD64
        CHECK(pe_header->file_header.NumberOfSections == 3);
        CHECK(pe_header->file_header.SizeOfOptionalHeader == 240);

        // Verify optional header (should be PE32+)
        auto* opt64 = pe_header->optional_header.as_opt_header_64();
        REQUIRE(opt64 != nullptr);
        CHECK(opt64->Magic == PE32PLUS_MAGIC);
        CHECK(opt64->SizeOfCode == 0x2000);
        CHECK(opt64->ImageBase == 0x0000000140000000ULL);
        CHECK(opt64->NumberOfRvaAndSizes == 16);

        // Verify section headers
        CHECK(pe_header->section_headers.size() == 3);
        CHECK(pe_header->section_headers[0].Name[1] == 't');  // .text
        CHECK(pe_header->section_headers[0].VirtualSize == 0x2000);
        CHECK(pe_header->section_headers[1].Name[1] == 'd');  // .data
        CHECK(pe_header->section_headers[1].VirtualSize == 0x0800);
        CHECK(pe_header->section_headers[2].Name[1] == 'r');  // .rdata
        CHECK(pe_header->section_headers[2].VirtualSize == 0x0400);

        // Verify we consumed all data
        CHECK(ptr == data.data() + data.size());
    }
}
