//
// End-to-End Test: Real-World Formats
//
#include <doctest/doctest.h>
#include <e2e_real_world.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Real-World Formats") {

    TEST_CASE("PNG - signature and IHDR chunk") {
        std::vector<uint8_t> data = {
            // PNG signature
            137, 80, 78, 71, 13, 10, 26, 10,
            // IHDR chunk
            0x00, 0x00, 0x00, 0x0D,  // length = 13 (BE)
            'I', 'H', 'D', 'R',       // type = IHDR
            // IHDR data (13 bytes)
            0x00, 0x00, 0x01, 0x00,  // width = 256 (BE)
            0x00, 0x00, 0x01, 0x00,  // height = 256 (BE)
            0x08,                     // bit depth = 8
            0x02,                     // color type = 2 (RGB)
            0x00,                     // compression = 0
            0x00,                     // filter = 0
            0x00,                     // interlace = 0
            0x00, 0x00, 0x00, 0x00   // CRC (placeholder)
        };

        const uint8_t* ptr = data.data();
        PNGHeader obj = PNGHeader::read(ptr, ptr + data.size());

        CHECK(obj.is_valid_png() == true);
        CHECK(obj.signature[0] == 137);
        CHECK(obj.signature[1] == 80);
        CHECK(obj.signature[2] == 78);
        CHECK(obj.signature[3] == 71);

        CHECK(obj.ihdr_chunk.length == 13);
        CHECK(obj.ihdr_chunk.is_ihdr() == true);
    }

    TEST_CASE("ELF - 32-bit little-endian header") {
        std::vector<uint8_t> data = {
            // ELF identification
            0x7F, 'E', 'L', 'F',  // magic
            1,                     // class = 32-bit
            1,                     // data = little-endian
            1,                     // version = 1
            0, 0, 0, 0, 0, 0, 0, 0, 0,  // padding
            // ELF header fields
            0x02, 0x00,            // type = EXEC
            0x03, 0x00,            // machine = x86
            0x01, 0x00, 0x00, 0x00,  // version
            0x00, 0x10, 0x00, 0x00,  // entry = 0x1000
            0x34, 0x00, 0x00, 0x00,  // phoff
            0x00, 0x00, 0x00, 0x00,  // shoff
            0x00, 0x00, 0x00, 0x00,  // flags
            0x34, 0x00,            // ehsize
            0x20, 0x00,            // phentsize
            0x02, 0x00,            // phnum
            0x28, 0x00,            // shentsize
            0x00, 0x00,            // shnum
            0x00, 0x00             // shstrndx
        };

        const uint8_t* ptr = data.data();
        ELFHeader obj = ELFHeader::read(ptr, ptr + data.size());

        CHECK(obj.ident.is_elf() == true);
        CHECK(obj.ident.is_32bit() == true);
        CHECK(obj.ident.is_little_endian() == true);
        CHECK(obj.type == 2);
        CHECK(obj.machine == 3);
    }

    TEST_CASE("TLV - multiple entries") {
        std::vector<uint8_t> data = {
            0x02,        // num_entries = 2
            // Entry 1
            0x01,        // type = 1
            0x03,        // length = 3
            0xAA, 0xBB, 0xCC,  // value
            // Entry 2
            0x02,        // type = 2
            0x02,        // length = 2
            0xDD, 0xEE   // value
        };

        const uint8_t* ptr = data.data();
        TLVMessage obj = TLVMessage::read(ptr, ptr + data.size());

        CHECK(obj.count() == 2);
        CHECK(obj.entries.size() == 2);
        CHECK(obj.entries[0].get_type() == 1);
        CHECK(obj.entries[0].get_length() == 3);
        CHECK(obj.entries[1].get_type() == 2);
        CHECK(obj.entries[1].get_length() == 2);
    }

    TEST_CASE("ZIP - local file header") {
        std::vector<uint8_t> data = {
            0x50, 0x4B, 0x03, 0x04,  // signature
            0x14, 0x00,              // version_needed = 20
            0x00, 0x00,              // flags = 0
            0x08, 0x00,              // compression_method = 8 (deflate)
            0x00, 0x00,              // last_mod_time
            0x00, 0x00,              // last_mod_date
            0x00, 0x00, 0x00, 0x00,  // crc32
            0x64, 0x00, 0x00, 0x00,  // compressed_size = 100
            0xC8, 0x00, 0x00, 0x00,  // uncompressed_size = 200
            0x04, 0x00,              // filename_length = 4
            0x00, 0x00,              // extra_field_length = 0
            't', 'e', 's', 't'       // filename = "test"
        };

        const uint8_t* ptr = data.data();
        ZIPLocalFileHeader obj = ZIPLocalFileHeader::read(ptr, ptr + data.size());

        CHECK(obj.is_valid_zip() == true);
        CHECK(obj.is_compressed() == true);
        CHECK(obj.is_encrypted() == false);
        CHECK(obj.compressed_size == 100);
        CHECK(obj.uncompressed_size == 200);
    }

    TEST_CASE("BMP - file and info headers") {
        std::vector<uint8_t> data;

        // File header
        data.push_back('B');
        data.push_back('M');
        uint32_t file_size = 54 + 100;  // Header + small image
        data.push_back(file_size & 0xFF);
        data.push_back((file_size >> 8) & 0xFF);
        data.push_back((file_size >> 16) & 0xFF);
        data.push_back((file_size >> 24) & 0xFF);
        data.push_back(0);  // reserved1
        data.push_back(0);
        data.push_back(0);  // reserved2
        data.push_back(0);
        uint32_t data_offset = 54;
        data.push_back(data_offset & 0xFF);
        data.push_back((data_offset >> 8) & 0xFF);
        data.push_back((data_offset >> 16) & 0xFF);
        data.push_back((data_offset >> 24) & 0xFF);

        // Info header (40 bytes)
        uint32_t header_size = 40;
        data.push_back(header_size & 0xFF);
        data.push_back((header_size >> 8) & 0xFF);
        data.push_back((header_size >> 16) & 0xFF);
        data.push_back((header_size >> 24) & 0xFF);

        int32_t width = 100;
        data.push_back(width & 0xFF);
        data.push_back((width >> 8) & 0xFF);
        data.push_back((width >> 16) & 0xFF);
        data.push_back((width >> 24) & 0xFF);

        int32_t height = 100;
        data.push_back(height & 0xFF);
        data.push_back((height >> 8) & 0xFF);
        data.push_back((height >> 16) & 0xFF);
        data.push_back((height >> 24) & 0xFF);

        // Remaining info header fields
        data.push_back(1); data.push_back(0);  // planes = 1
        data.push_back(24); data.push_back(0); // bits_per_pixel = 24
        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);  // compression = 0
        for (int i = 0; i < 20; i++) data.push_back(0);  // remaining fields

        const uint8_t* ptr = data.data();
        BMPFile obj = BMPFile::read(ptr, ptr + data.size());

        CHECK(obj.file_header.is_bmp() == true);
        CHECK(obj.file_header.get_data_offset() == 54);
        CHECK(obj.info_header.get_width() == 100);
        CHECK(obj.info_header.get_height() == 100);
        CHECK(obj.info_header.is_uncompressed() == true);
    }
}
