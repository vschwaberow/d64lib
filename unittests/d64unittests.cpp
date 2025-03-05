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
    TEST(d64lib_unit_test, add_file_unit_test)
    {
        d64 disk;
        for (auto file = 1; disk.getFreeSectorCount() > 5; ++file) {
            std::vector<uint8_t> filedata(SECTOR_SIZE * 2 -5);
            for (auto i = 0; i < filedata.capacity(); i++) {
                filedata[i] = file + i;
            }
            char buffer[10] = { 0 };
            itoa(file, buffer, 10);

            std::string filename = "FILE";
            filename += buffer;
            auto added = disk.addFile(filename, d64::FileTypes::PRG, filedata);
            EXPECT_TRUE(added);
            auto dir = disk.directory();
            EXPECT_TRUE(dir.size() == file);
        }
     }
}