// Written by Paul Baxter
#pragma once
#include <string>
#include <array>
#include <optional>
#include <functional>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <bitset>

#include "d64_types.h"

#pragma pack(push, 1)

class d64 {
public:

    d64();
    d64(diskType type);
    d64(std::string name);

    void formatDisk(std::string_view name);
    bool rename_disk(std::string_view name) const;
    std::string diskname();
    std::optional<Directory_EntryPtr> findFile(std::string_view filename);
    bool addFile(std::string_view filename, FileType type, const std::vector<uint8_t>& fileData, int recirdSize = 0);
    bool removeFile(std::string_view filename);
    bool renameFile(std::string_view oldfilename, std::string_view newfilename);
    bool extractFile(std::string filename);
    bool save(std::string filename);
    bool load(std::string filename);
    int calcOffset(int track, int sector) const;
    bool writeByte(int track, int sector, int offset, uint8_t value);
    bool writeSector(int track, int sector, std::vector<uint8_t> bytes);
    std::optional<uint8_t> readByte(int track, int sector, int offset);
    std::optional<std::vector<uint8_t>> readSector(int track, int sector);
    bool freeSector(const int& track, const int& sector);
    bool allocateSector(const int& track, const int& sector);
    bool findAndAllocateFreeSector(int& track, int& sector);
    std::optional<std::vector<uint8_t>> readFile(std::string filename);
    uint16_t getFreeSectorCount();
    bool compactDirectory();
    bool verifyBAMIntegrity(bool fix, const std::string& logFile);
    bool reorderDirectory(std::function<bool(const Directory_Entry&, const Directory_Entry&)> compare);
    bool reorderDirectory(std::vector<Directory_Entry>& files);
    bool reorderDirectory(const std::vector<std::string>& fileOrder);
    bool movefileFirst(std::string file);
    bool lockfile(std::string file, bool lock);
    std::vector<Directory_Entry> directory();
    static std::string Trim(const char filename[FILE_NAME_SZ]);

    int TRACKS;

    // Constants for D64 format
    const std::array<int, TRACKS_40> SECTORS_PER_TRACK = {
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, // Tracks 1-17
        19, 19, 19, 19, 19, 19, 19,                                         // Tracks 18-24
        18, 18, 18, 18, 18, 18,                                             // Tracks 25-30
        17, 17, 17, 17, 17,                                                 // Tracks 31-35
        17, 17, 17, 17, 17                                                  // Tracks 36-40
    };

    const std::array<int, TRACKS_40> TRACK_OFFSETS = {
        0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00, 0x09300, 0x0A800, 0x0BD00,
        0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600, 0x13B00, 0x15000, 0x16500, 0x17800, 0x18B00,
        0x19E00, 0x1B100, 0x1C400, 0x1D700, 0x1EA00, 0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400,
        0x25600, 0x26700, 0x27800, 0x28900, 0x29A00, 0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
    };

    inline BAM_TRACK_ENTRY* bamtrack(int t)
    {
        return (t < TRACKS_35) ?
            &bamTrackPtr[(t)] :
            &bamExtraTrackPtr[((t)-TRACKS_35)];
    }
    inline SectorPtr getSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<SectorPtr>(&data[calcOffset(track, sector)]);
    }
    inline TrackSector* getTrackSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<TrackSector*>(&data[calcOffset(track, sector)]);
    }
    inline SideSectorPtr getSideSectorPtr(uint8_t track, uint8_t sector)
    {
        return reinterpret_cast<SideSectorPtr>(&data[calcOffset(track, sector)]);
    }
    inline Directory_SectorPtr getDirectory_SectorPtr(const int& track, const int& sector)
    {
        return reinterpret_cast<Directory_SectorPtr>(&data[calcOffset(track, sector)]);
    }

private:
    static constexpr int INTERLEAVE = 10;
    std::array<int, TRACKS_40> lastSectorUsed = { -1 };
    BAMPtr bamPtr;
    BAM_TRACK_ENTRY* bamTrackPtr;
    BAM_TRACK_ENTRY* bamExtraTrackPtr;
    diskType disktype = thirty_five_track;

    bool validateD64();
    void initBAM(std::string_view name);
    void initializeBAMFields(std::string_view name);
    bool writeData(int track, int sector, std::vector<uint8_t> bytes, int byteoffset);
    std::vector<TrackSector> parseSideSectors(int sideTrack, int sideSector);
    void init_disk();
    bool findAndAllocateFreeOnTrack(int t, int& sector);
    std::optional<Directory_EntryPtr> findEmptyDirectorySlot();
    bool allocateSideSector(int& track, int& sector, SideSectorPtr& side);
    bool allocateDataSector(int& track, int& sector, SectorPtr& sectorPtr);
    void writeDataToSector(SectorPtr sectorPtr, const std::vector<uint8_t>& fileData, int& offset, int& bytesLeft);
    std::vector<TrackSector> writeFileDataToSectors(int start_track, int start_sector, const std::vector<uint8_t>& fileData);
    std::optional<std::vector<SideSectorPtr>> createSideSectors(const std::vector<TrackSector>& allocatedSectors, uint8_t record_size);
    bool createDirectoryEntry(std::string_view filename, FileType type, int start_track, int start_sector, const std::vector<TrackSector>& allocatedSectors, uint8_t record_size);
    bool findAndAllocateFirstSector(int& start_track, int& start_sector);
    bool allocateNewDirectorySector(int& dir_track, int& dir_sector, Directory_SectorPtr& dirSectorPtr);

    inline void initBAMPtr()
    {
        auto index = calcOffset(DIRECTORY_TRACK, BAM_SECTOR);
        bamPtr = reinterpret_cast<BAMPtr>(&data[index]);
        bamTrackPtr = &(bamPtr->bam_track[0]);
        bamExtraTrackPtr = reinterpret_cast<BAM_TRACK_ENTRY*>(&data[index + 0xAC]);
    }
    bool isValidTrackSector(int track, int sector) const;

    std::vector<uint8_t> data;
};

#pragma pack(pop)
