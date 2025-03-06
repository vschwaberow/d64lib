#include <gtest/gtest.h>
#include <string>

#include "d64.h"

#pragma warning(disable:4996)

namespace d64lib_unit_test
{
    static void d64lib_unit_test_method_initialize();
    static void d64lib_unit_test_method_cleanup();

    static void d64lib_unit_test_method_initialize()
    {
    }

    static void d64lib_unit_test_method_cleanup()
    {
    }

    TEST(d64lib_unit_test, sector_allocation_test)
    {
        d64 disk;

        auto count = disk.getFreeSectorCount();
        std::vector< d64::TrackSector> allocations;

        for (auto allocation = 0; allocation < count; ++allocation) {
            int track, sector;

            if (disk.findAndAllocateFreeSector(track, sector)) {
                auto ts = d64::TrackSector(track, sector);
                auto it = std::find(allocations.begin(), allocations.end(), ts);
                auto expected = it == allocations.end();
                if (!expected) {
                    EXPECT_TRUE(expected);
                }
                allocations.push_back(ts);
            }
        }
    }

    TEST(d64lib_unit_test, sector_allocation_40_test)
    {
        d64 disk(d64::forty_track);

        auto count = disk.getFreeSectorCount();
        std::vector< d64::TrackSector> allocations;

        for (auto allocation = 0; allocation < count; ++allocation) {
            int track, sector;

            if (disk.findAndAllocateFreeSector(track, sector)) {
                auto ts = d64::TrackSector(track, sector);
                auto it = std::find(allocations.begin(), allocations.end(), ts);
                auto expected = it == allocations.end();
                if (!expected) {
                    EXPECT_TRUE(expected);
                }
                allocations.push_back(ts);
            }
        }
    }

    TEST(d64lib_unit_test, create_unit_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk;
        EXPECT_STREQ(disk.diskname().c_str(), "NEW DISK");
        EXPECT_EQ(disk.TRACKS, TRACKS_35);
        auto dir = disk.directory();
        EXPECT_TRUE(dir.size() == 0);
        EXPECT_TRUE(disk.verifyBAMIntegrity(false, ""));
        EXPECT_EQ(disk.getFreeSectorCount(), 664);

        d64lib_unit_test_method_cleanup();
    }

    TEST(d64lib_unit_test, create_40_unit_test)
    {
        d64lib_unit_test_method_initialize();

        d64 disk(d64::forty_track);
        EXPECT_STREQ(disk.diskname().c_str(), "NEW DISK");
        EXPECT_EQ(disk.TRACKS, TRACKS_40);
        auto dir = disk.directory();
        EXPECT_TRUE(dir.size() == 0);
        EXPECT_TRUE(disk.verifyBAMIntegrity(false, ""));
        EXPECT_EQ(disk.getFreeSectorCount(), 749);

        d64lib_unit_test_method_cleanup();
    }

    TEST(d64lib_unit_test, large_file_unit_test)
    {
        const auto bigSize = 20000;

        std::vector<uint8_t> big_file(bigSize);

        for (auto i = 0; i < bigSize; ++i) {
            big_file[i] = i % 256;
        }

        d64 disk;

        auto added = disk.addFile("BIG", d64::FileTypes::SEQ, big_file);
        EXPECT_TRUE(added);

        auto readfile = disk.readFile("BIG");
        EXPECT_TRUE(readfile.has_value());
        if (readfile.has_value()) {
            for (auto i = 0; i < bigSize; ++i) {
                EXPECT_TRUE(readfile.value()[i] == big_file[i]);
            }
        }
    }

    TEST(d64lib_unit_test, add_file_unit_test)
    {
        std::vector<uint8_t> prog = {
0x01, 0x08, 0x0f, 0x08, 0x0a, 0x00, 0x99, 0x20, 0x22, 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x22, 0x00,
0x1b, 0x08, 0x14, 0x00, 0x81, 0x4b, 0xb2, 0x31, 0xa4, 0x31, 0x30, 0x00, 0x27, 0x08, 0x1e, 0x00,
0x81, 0x4c, 0xb2, 0x4b, 0xa4, 0x31, 0x31, 0x00, 0x31, 0x08, 0x28, 0x00, 0x99, 0x20, 0x4b, 0x2c,
0x4c, 0x00, 0x39, 0x08, 0x32, 0x00, 0x82, 0x3a, 0x82, 0x00, 0x3f, 0x08, 0x3c, 0x00, 0x80, 0x00,
0x00, 0x00
        };

        d64 disk;
        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            auto numpart = std::to_string(file);

            std::string filename = "FILE";
            filename += numpart;
            auto added = disk.addFile(filename, d64::FileTypes::PRG, prog);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);

            auto readfile = disk.readFile(filename);
            EXPECT_TRUE(readfile.has_value());
            if (readfile.has_value()) {
                EXPECT_TRUE(readfile.value() == prog);
            }
        }
    }

    TEST(d64lib_unit_test, extract_file_unit_test)
    {
        std::vector<uint8_t> prog = {
0x01, 0x08, 0x15, 0x08, 0x0a, 0x00, 0x99, 0x20, 0x22, 0x48, 0x45, 0x4c, 0x4c, 0x4f, 0x20,
0x57, 0x4f, 0x52, 0x4c, 0x44, 0x22, 0x00, 0x1b, 0x08, 0x14, 0x00, 0x80, 0x00, 0x00, 0x00
        };

        d64 disk;
        std::vector<std::string> files;

        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            auto numpart = std::to_string(file);

            std::string filename = "FILE";
            filename += numpart;
            auto added = disk.addFile(filename, d64::FileTypes::PRG, prog);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);
            files.push_back(filename);
        }

        for (auto& filename : files) {
            auto extracted = disk.extractFile(filename);
            EXPECT_TRUE(extracted);

            std::remove((filename + ".prg").c_str());
        }
        
    }


}