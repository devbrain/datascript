//
// Comprehensive End-to-End Test: Labels, Alignment, Nested Structs, and Unions
//
#include <doctest/doctest.h>
#include <e2e_labels_complex.h>
#include <vector>
#include <cstring>
#include <iostream>

using namespace generated;

TEST_SUITE("E2E - Complex Labels") {

    // ========================================================================
    // ComplexFile with Image Data
    // ========================================================================

    TEST_CASE("ComplexFile - Image type with labels and alignment") {
        // File layout (absolute byte positions):
        // 0-3:   magic = 0x12345678
        // 4-7:   version = 1
        // 8-11:  data_offset = 64 (absolute position of TypeSpecificData)
        // 12-15: metadata_offset = 92 (absolute position of Metadata)
        // 16-19: created.seconds = 1234567890
        // 20-23: created.microseconds = 500000
        // 24:    file_type = TYPE_IMAGE (1)
        // 25-31: padding to align to 8-byte boundary
        // 32-63: padding to reach data_offset (64)
        // 64-67: width = 1920
        // 68-71: height = 1080
        // 72:    bpp = 24
        // 73-75: padding to align to 4-byte boundary
        // 76-79: pixel_format = 0x52474241 ('RGBA')
        // 80-91: padding to reach metadata_offset (92)
        // 92:    flags = 0x01
        // 93-95: padding to align to 4-byte boundary
        // 96-99: checksum = 0xDEADBEEF
        // 100-103: modified.seconds = 1234567900
        // 104-107: modified.microseconds = 600000

        std::vector<uint8_t> data(108, 0x00);

        // FileHeader
        *reinterpret_cast<uint32_t*>(&data[0]) = 0x12345678;  // magic
        *reinterpret_cast<uint32_t*>(&data[4]) = 1;           // version
        *reinterpret_cast<uint32_t*>(&data[8]) = 64;          // data_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[12]) = 92;         // metadata_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[16]) = 1234567890; // created.seconds
        *reinterpret_cast<uint32_t*>(&data[20]) = 500000;     // created.microseconds

        // file_type
        data[24] = 1;  // TYPE_IMAGE

        // At position 64: ImageData
        *reinterpret_cast<uint32_t*>(&data[64]) = 1920;       // width
        *reinterpret_cast<uint32_t*>(&data[68]) = 1080;       // height
        data[72] = 24;                                         // bpp
        *reinterpret_cast<uint32_t*>(&data[76]) = 0x52474241; // pixel_format 'RGBA'

        // At position 92: Metadata
        data[92] = 0x01;                                       // flags
        *reinterpret_cast<uint32_t*>(&data[96]) = 0xDEADBEEF; // checksum
        *reinterpret_cast<uint32_t*>(&data[100]) = 1234567900; // modified.seconds
        *reinterpret_cast<uint32_t*>(&data[104]) = 600000;     // modified.microseconds

        const uint8_t* ptr = data.data();
        ComplexFile obj = ComplexFile::read(ptr, ptr + data.size());

        // Verify file type (FIRST, before any other checks)
        FileType actual_file_type = obj.file_type;
        FileType expected_type = FileType::TYPE_IMAGE;
        REQUIRE(static_cast<uint8_t>(actual_file_type) == static_cast<uint8_t>(expected_type));

        // Verify header
        CHECK(obj.header.magic == 0x12345678);
        CHECK(obj.header.version == 1);
        CHECK(obj.header.data_offset == 64);
        CHECK(obj.header.metadata_offset == 92);
        CHECK(obj.header.created.seconds == 1234567890);
        CHECK(obj.header.created.microseconds == 500000);

        // Verify image data (accessed via choice case wrapper)
        auto* img_case = obj.type_specific_data.as_image_data();
        REQUIRE(img_case != nullptr);
        const auto& img = img_case->value;
        CHECK(img.width == 1920);
        CHECK(img.height == 1080);
        CHECK(img.bpp == 24);
        CHECK(img.pixel_format == 0x52474241);

        // Verify metadata
        CHECK(obj.metadata.flags == 0x01);
        CHECK(obj.metadata.checksum == 0xDEADBEEF);
        CHECK(obj.metadata.modified.seconds == 1234567900);
        CHECK(obj.metadata.modified.microseconds == 600000);
    }

    // ========================================================================
    // ComplexFile with Audio Data
    // ========================================================================

    TEST_CASE("ComplexFile - Audio type with labels and alignment") {
        // File layout:
        // 0-23:  FileHeader (same as before)
        // 24:    file_type = TYPE_AUDIO (2)
        // 25-31: padding to align to 8-byte boundary
        // 32-63: padding to reach data_offset (64)
        // 64-67: sample_rate = 44100
        // 68-69: channels = 2
        // 70-71: bits_per_sample = 16
        // 72-79: padding to align to 8-byte boundary
        // 80-87: total_samples = 1000000
        // 88-99: padding to reach metadata_offset (100)
        // 100+:  Metadata

        std::vector<uint8_t> data(116, 0x00);

        // FileHeader
        *reinterpret_cast<uint32_t*>(&data[0]) = 0x41554449;  // magic 'AUDI'
        *reinterpret_cast<uint32_t*>(&data[4]) = 2;           // version
        *reinterpret_cast<uint32_t*>(&data[8]) = 64;          // data_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[12]) = 100;        // metadata_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[16]) = 1234567890;
        *reinterpret_cast<uint32_t*>(&data[20]) = 500000;

        // file_type
        data[24] = 2;  // TYPE_AUDIO

        // At position 64: AudioData
        *reinterpret_cast<uint32_t*>(&data[64]) = 44100;      // sample_rate
        *reinterpret_cast<uint16_t*>(&data[68]) = 2;          // channels
        *reinterpret_cast<uint16_t*>(&data[70]) = 16;         // bits_per_sample
        // After 4+2+2=8 bytes, we're at offset 72, which IS 8-byte aligned
        *reinterpret_cast<uint64_t*>(&data[72]) = 1000000;    // total_samples (at aligned position)

        // At position 100: Metadata
        data[100] = 0x02;
        *reinterpret_cast<uint32_t*>(&data[104]) = 0xCAFEBABE;
        *reinterpret_cast<uint32_t*>(&data[108]) = 1234567900;
        *reinterpret_cast<uint32_t*>(&data[112]) = 600000;

        const uint8_t* ptr = data.data();
        ComplexFile obj = ComplexFile::read(ptr, ptr + data.size());

        // Verify file type (BEFORE header checks to avoid corruption)
        FileType actual_file_type = obj.file_type;
        FileType expected_type = FileType::TYPE_AUDIO;
        REQUIRE(static_cast<uint8_t>(actual_file_type) == static_cast<uint8_t>(expected_type));

        // Verify header
        CHECK(obj.header.magic == 0x41554449);
        CHECK(obj.header.data_offset == 64);
        CHECK(obj.header.metadata_offset == 100);

        // Verify audio data
        auto* audio_case = obj.type_specific_data.as_audio_data();
        REQUIRE(audio_case != nullptr);
        const auto& audio = audio_case->value;
        CHECK(audio.sample_rate == 44100);
        CHECK(audio.channels == 2);
        CHECK(audio.bits_per_sample == 16);
        CHECK(audio.total_samples == 1000000);

        // Verify metadata
        CHECK(obj.metadata.flags == 0x02);
        CHECK(obj.metadata.checksum == 0xCAFEBABE);
    }

    // ========================================================================
    // ComplexFile with Video Data
    // ========================================================================

    TEST_CASE("ComplexFile - Video type with labels and alignment") {
        std::vector<uint8_t> data(116, 0x00);

        // FileHeader
        *reinterpret_cast<uint32_t*>(&data[0]) = 0x5649444F;  // magic 'VIDO'
        *reinterpret_cast<uint32_t*>(&data[4]) = 3;           // version
        *reinterpret_cast<uint32_t*>(&data[8]) = 64;          // data_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[12]) = 96;         // metadata_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[16]) = 1234567890;
        *reinterpret_cast<uint32_t*>(&data[20]) = 500000;

        // file_type
        data[24] = 3;  // TYPE_VIDEO

        // At position 64: VideoData
        *reinterpret_cast<uint32_t*>(&data[64]) = 3840;       // width (4K)
        *reinterpret_cast<uint32_t*>(&data[68]) = 2160;       // height
        *reinterpret_cast<uint32_t*>(&data[72]) = 60;         // fps
        data[76] = 0x48;                                       // codec (H264)
        *reinterpret_cast<uint32_t*>(&data[80]) = 50000000;   // bitrate (at aligned position)

        // At position 96: Metadata
        data[96] = 0x03;
        *reinterpret_cast<uint32_t*>(&data[100]) = 0x12345678;
        *reinterpret_cast<uint32_t*>(&data[104]) = 1234567900;
        *reinterpret_cast<uint32_t*>(&data[108]) = 600000;

        const uint8_t* ptr = data.data();
        ComplexFile obj = ComplexFile::read(ptr, ptr + data.size());

        // Verify file type
        FileType actual_file_type = obj.file_type;
        FileType expected_type = FileType::TYPE_VIDEO;
        REQUIRE(static_cast<uint8_t>(actual_file_type) == static_cast<uint8_t>(expected_type));

        // Verify video data
        auto* video_case = obj.type_specific_data.as_video_data();
        REQUIRE(video_case != nullptr);
        const auto& video = video_case->value;
        CHECK(video.width == 3840);
        CHECK(video.height == 2160);
        CHECK(video.fps == 60);
        CHECK(video.codec == 0x48);
        CHECK(video.bitrate == 50000000);
    }

    // ========================================================================
    // Nested Field Access in Labels
    // ========================================================================

    TEST_CASE("NestedData - nested field access in labels") {
        // File layout:
        // 0-3:   level1_offset = 16 (absolute)
        // 4-7:   level2_offset = 32 (absolute)
        // 8-15:  padding to reach level1_offset
        // 16-19: first_value = 0xAAAABBBB
        // 20-23: padding
        // 24-31: padding to align to 8-byte boundary
        // 32-39: second_value = 0x1122334455667788

        std::vector<uint8_t> data(40, 0x00);

        *reinterpret_cast<uint32_t*>(&data[0]) = 16;  // level1_offset (absolute)
        *reinterpret_cast<uint32_t*>(&data[4]) = 32;  // level2_offset (absolute)

        // At position 16: first_value
        *reinterpret_cast<uint32_t*>(&data[16]) = 0xAAAABBBB;

        // At position 32 (aligned): second_value
        *reinterpret_cast<uint64_t*>(&data[32]) = 0x1122334455667788ULL;

        const uint8_t* ptr = data.data();
        NestedData obj = NestedData::read(ptr, ptr + data.size());

        CHECK(obj.offsets.level1_offset == 16);
        CHECK(obj.offsets.level2_offset == 32);
        CHECK(obj.first_value == 0xAAAABBBB);
        CHECK(obj.second_value == 0x1122334455667788ULL);
    }

    // ========================================================================
    // Multiple Unions with Labels
    // ========================================================================

    TEST_CASE("MultiUnionFile - compact and extended formats") {
        // File layout:
        // 0-3:   offset1 = 16 (absolute)
        // 4-7:   offset2 = 24 (absolute)
        // 8:     format1 = FORMAT_COMPACT (1)
        // 9:     format2 = FORMAT_EXTENDED (2)
        // 10-15: padding to align to 4-byte boundary
        // 16-17: compact.value = 0x1234
        // 18-23: padding to align to 4-byte boundary and reach offset2
        // 24-27: extended.value = 0xABCDEF01
        // 28-31: extended.extra = 0x12345678

        std::vector<uint8_t> data(32, 0x00);

        *reinterpret_cast<uint32_t*>(&data[0]) = 16;  // offset1 (absolute)
        *reinterpret_cast<uint32_t*>(&data[4]) = 24;  // offset2 (absolute)
        data[8] = 1;   // FORMAT_COMPACT
        data[9] = 2;   // FORMAT_EXTENDED

        // At position 16: CompactFormat
        *reinterpret_cast<uint16_t*>(&data[16]) = 0x1234;

        // At position 24: ExtendedFormat
        *reinterpret_cast<uint32_t*>(&data[24]) = 0xABCDEF01;
        *reinterpret_cast<uint32_t*>(&data[28]) = 0x12345678;

        const uint8_t* ptr = data.data();
        MultiUnionFile obj = MultiUnionFile::read(ptr, ptr + data.size());

        CHECK(obj.offset1 == 16);
        CHECK(obj.offset2 == 24);
        CHECK(obj.format1 == DataFormat::FORMAT_COMPACT);
        CHECK(obj.format2 == DataFormat::FORMAT_EXTENDED);

        // Verify first choice (compact)
        auto* compact_case = obj.data1.as_compact();
        REQUIRE(compact_case != nullptr);
        const auto& compact = compact_case->value;
        CHECK(compact.value == 0x1234);

        // Verify second choice (extended)
        auto* extended_case = obj.data2.as_extended();
        REQUIRE(extended_case != nullptr);
        const auto& extended = extended_case->value;
        CHECK(extended.value == 0xABCDEF01);
        CHECK(extended.extra == 0x12345678);
    }

    // ========================================================================
    // Multiple Unions with Same Type
    // ========================================================================

    TEST_CASE("MultiUnionFile - both compact format") {
        std::vector<uint8_t> data(28, 0x00);

        *reinterpret_cast<uint32_t*>(&data[0]) = 16;  // offset1 (absolute)
        *reinterpret_cast<uint32_t*>(&data[4]) = 24;  // offset2 (absolute)
        data[8] = 1;   // FORMAT_COMPACT
        data[9] = 1;   // FORMAT_COMPACT

        // At position 16: first CompactFormat
        *reinterpret_cast<uint16_t*>(&data[16]) = 0xAAAA;

        // At position 24: second CompactFormat
        *reinterpret_cast<uint16_t*>(&data[24]) = 0xBBBB;

        const uint8_t* ptr = data.data();
        MultiUnionFile obj = MultiUnionFile::read(ptr, ptr + data.size());

        CHECK(obj.format1 == DataFormat::FORMAT_COMPACT);
        CHECK(obj.format2 == DataFormat::FORMAT_COMPACT);

        auto* compact1_case = obj.data1.as_compact();
        auto* compact2_case = obj.data2.as_compact();
        REQUIRE(compact1_case != nullptr);
        REQUIRE(compact2_case != nullptr);
        const auto& compact1 = compact1_case->value;
        const auto& compact2 = compact2_case->value;

        CHECK(compact1.value == 0xAAAA);
        CHECK(compact2.value == 0xBBBB);
    }

    // ========================================================================
    // Edge Cases: Labels at Beginning
    // ========================================================================

    TEST_CASE("NestedData - label pointing to start of struct") {
        // Test when a label points to the very beginning (offset 0)
        std::vector<uint8_t> data(40, 0x00);

        *reinterpret_cast<uint32_t*>(&data[0]) = 8;   // level1_offset = 8 (just after offsets)
        *reinterpret_cast<uint32_t*>(&data[4]) = 16;  // level2_offset = 16

        *reinterpret_cast<uint32_t*>(&data[8]) = 0x99887766;
        *reinterpret_cast<uint64_t*>(&data[16]) = 0xFEDCBA9876543210ULL;

        const uint8_t* ptr = data.data();
        NestedData obj = NestedData::read(ptr, ptr + data.size());

        CHECK(obj.first_value == 0x99887766);
        CHECK(obj.second_value == 0xFEDCBA9876543210ULL);
    }

    // ========================================================================
    // Edge Cases: Out of Bounds Label
    // ========================================================================

    TEST_CASE("ComplexFile - label out of bounds throws exception") {
        std::vector<uint8_t> data(40, 0x00);

        // FileHeader with invalid offsets
        *reinterpret_cast<uint32_t*>(&data[0]) = 0x12345678;
        *reinterpret_cast<uint32_t*>(&data[4]) = 1;
        *reinterpret_cast<uint32_t*>(&data[8]) = 1000;  // data_offset way beyond buffer
        *reinterpret_cast<uint32_t*>(&data[12]) = 2000; // metadata_offset way beyond buffer

        data[24] = 1;  // TYPE_IMAGE

        const uint8_t* ptr = data.data();
        CHECK_THROWS_AS(ComplexFile::read(ptr, ptr + data.size()), std::runtime_error);
    }
}
