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

#pragma pack(push, 1)

const int TRACKS_35 = 35;
const int TRACKS_40 = 40;
const int SECTOR_SIZE = 256;
const int DISK_NAME_SZ = 16;
const int FILE_NAME_SZ = 16;
const int UNUSED3_SZ = 5;
const int UNUSED4_SZ = 84;
const int DIR_ENTRY_SZ = 30;
const int DIRECTORY_TRACK = 18;
const int DIRECTORY_SECTOR = 1;
const int TRACK_SECTOR = 0;
const int BAM_SECTOR = 0;
const int FILES_PER_SECTOR = 8;

const int D64_DISK35_SZ = 174848;
const int D64_DISK40_SZ = 196608;

const int SIDE_SECTOR_ENTRY_SIZE = 6;
const int SIDE_SECTOR_CHAIN_SZ = ((SECTOR_SIZE - 15) / (2));

class d64 {
public:
    enum diskType {
        thirty_five_track,
        forty_track
    };


    d64();
    d64(diskType type);
    d64(std::string name);

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

private:
    std::vector<uint8_t> data;

    static constexpr uint8_t A0_VALUE = 0xA0;
    static constexpr uint8_t DOS_VERSION = 'A';
    static constexpr uint8_t DOS_TYPE = '2';

public:
    enum FileTypes : uint8_t {
        DEL = 0,
        SEQ = 1,
        PRG = 2,
        USR = 3,
        REL = 4
    };

    // Track and sector
    struct TrackSector {
    public:
        uint8_t track;
        uint8_t sector;

        bool operator ==(const TrackSector& other) const
        {
            return track == other.track && sector == other.sector;
        }

        TrackSector(int track, int sector) : track(track), sector(sector) {};
        TrackSector(uint8_t track, uint8_t sector) : track(track), sector(sector) {};
    };

    struct Sector {
    public:
        TrackSector next;
        std::array<uint8_t, SECTOR_SIZE - sizeof(TrackSector)> data;
    };
    typedef Sector* SectorPtr;

    // side sector
    class SideSector {
    public:
        TrackSector next;                                   // $01 - $02
        uint8_t block;                                      // $02
        uint8_t recordsize;                                 // $03
        TrackSector side_sectors[SIDE_SECTOR_ENTRY_SIZE];   // $04 - $0F
        TrackSector chain[SIDE_SECTOR_CHAIN_SZ];            // chain T/S
    };
    typedef SideSector* SideSectorPtr;

    class FileType {
    public:
        FileTypes type : 4;
        uint8_t unused : 1;
        uint8_t replace : 1;
        uint8_t locked : 1;
        uint8_t closed : 1;

    public:
        FileType() : closed(0), locked(0), replace(0), unused(0), type(FileTypes::DEL) {}
        FileType(bool a, bool l, FileTypes t) : closed(a ? 1 : 0), locked(l ? 1 : 0), unused(0), type(t) {}
        FileType(FileTypes t) : closed(1), locked(0), unused(0), type(t) {}
        FileType(uint8_t value) :
            closed(value & 0x80),
            locked(value & 0x40),
            replace(value & 0x20),
            unused(value & 0x10),
            type(static_cast<FileTypes>(value & 0x0F))
        {
        }

        operator uint8_t() const { return (closed << 7) | (locked << 6) | (replace << 5) | (unused << 4) | type; }
        operator FileTypes() const { return type; }
    };

    struct BAM_TRACK_ENTRY {

    public:
        uint8_t free;

    private:
        std::array <uint8_t, 3> bytes;

    public:
        /// <summary>
        /// test if a sector is used in bam  
        /// </summary>
        /// <param name="sector">sector to test</param>
        bool test(int sector)
        {
            auto byte = sector / 8;
            auto bit = sector % 8;

            std::bitset<8> bits(bytes[byte]);
            return bits.test(bit);
        }

        /// <summary>
        /// mark a sector as free in bam  
        /// </summary>
        /// <param name="sector">sector to mark</param>
        inline void set(int sector)
        {
            auto byte = sector / 8;
            auto bit = sector % 8;

            std::bitset<8> bits(bytes[byte]);
            bits.set(bit);
            bytes[byte] = static_cast<uint8_t>(bits.to_ulong());
        }

        /// <summary>
        /// mark a sector as used in bam  
        /// </summary>
        /// <param name="sector">sector to mark</param>
        inline void reset(int sector)
        {
            auto byte = sector / 8;
            auto bit = sector % 8;

            std::bitset<8> bits(bytes[byte]);
            bits.reset(bit);
            bytes[byte] = static_cast<uint8_t>(bits.to_ulong());
        }

        /// <summary>
        /// clear the bam sectors
        /// this marks them all as in use
        /// </summary>
        inline void clear()
        {
            bytes[0] = 0;
            bytes[1] = 0;
            bytes[2] = 0;
        }
    };

    // This folows DOLPHIN DOS for 40 tracks
    struct BAM {
        TrackSector dir_start;                  // $00 - $01
        uint8_t dos_version;                    // $02          'A' dos version
        uint8_t unused;                         // $03          unused should be 0
        BAM_TRACK_ENTRY bam_track[TRACKS_35];   // $04 - $8F    BAM to each track
        char disk_name[DISK_NAME_SZ];           // $90 - $9F    disk name padded with A0
        uint8_t a0[2];                          // $A0 - $A1    contains A0
        uint8_t disk_id[2];                     // $A2 - $A3    disk id
        uint8_t unused2;                        // $A4          contains A0
        char dos_type[2];                       // $A5 - $A6    '2' 'A'
        uint8_t unused3[UNUSED3_SZ];            // $A7 - $AB    00
        uint8_t unused4[UNUSED4_SZ];            // $AC - $FF    00
    };
    typedef struct BAM* BAMPtr;

    struct Directory_Entry {
        FileType file_type;                 // $00          file type
        TrackSector start;                  // $01 - $02    first track  and sector of file entry
        char file_name[FILE_NAME_SZ];       // $03 - $12    file name padded with $A0
        TrackSector side;                   // $13 - $14    first side track/sector .REL file only
        uint8_t record_length;              // $15          record side track .REL file only
        uint8_t unused[4];                  // $16 - $19    unused
        TrackSector replace;                // $1A - $1B    track / sector of replacement file during @save
        uint8_t file_size[2];               // $1C - $1D    low byte high byte for file size
        uint8_t padd[2];                    // $1E - $1F    undocumented padd

        bool operator==(const Directory_Entry& other) const
        {
            return (uint8_t)file_type == (uint8_t)other.file_type &&
                start.track == other.start.track &&
                start.sector == other.start.sector &&
                std::memcmp(file_name, other.file_name, FILE_NAME_SZ) == 0 &&
                side.track == other.side.track &&
                side.sector == other.side.sector &&
                record_length == other.record_length &&
                std::memcmp(unused, other.unused, sizeof(unused)) == 0 &&
                replace.track == other.replace.track &&
                replace.sector == other.replace.sector &&
                std::memcmp(file_size, other.file_size, sizeof(file_size)) == 0;
        }
        bool operator!=(const Directory_Entry& other) const
        {
            return !(*this == other);
        }
    };
    typedef struct Directory_Entry* Directory_EntryPtr;

    struct Directory_Sector {
        TrackSector next;
        Directory_Entry fileEntry[FILES_PER_SECTOR];
    };
    typedef struct Directory_Sector* Directory_SectorPtr;

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

    void formatDisk(std::string_view name);
    bool rename_disk(std::string_view name) const;
    std::string diskname();

    std::optional<Directory_EntryPtr> findFile(std::string_view filename);
    bool addFile(std::string_view filename, FileType type, const std::vector<uint8_t>& fileData);
    bool addRelFile(std::string_view filename, FileType type, uint8_t record_size, const std::vector<uint8_t>& fileData);
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

private:
    static constexpr int INTERLEAVE = 10;
    std::array<int, TRACKS_40> lastSectorUsed = { -1 };

    bool validateD64();
    void initBAM(std::string_view name);
    void initializeBAMFields(std::string_view name);
    bool writeData(int track, int sector, std::vector<uint8_t> bytes, int byteoffset);
    std::vector<d64::TrackSector> parseSideSectors(int sideTrack, int sideSector);
    BAMPtr bamPtr;
    BAM_TRACK_ENTRY* bamTrackPtr;
    BAM_TRACK_ENTRY* bamExtraTrackPtr;
    diskType disktype = thirty_five_track;
    void init_disk();
    bool findAndAllocateFreeOnTrack(int t, int& sector);
    std::optional<d64::Directory_EntryPtr> findEmptyDirectorySlot();
    std::optional<std::vector<uint8_t>> readRELFile(d64::Directory_EntryPtr fileEntry);
    std::optional<std::vector<uint8_t>> readPRGFile(d64::Directory_EntryPtr fileEntry);

    bool allocateSideSector(int& track, int& sector, SideSectorPtr& side);
    bool allocateDataSector(int& track, int& sector, SectorPtr& sectorPtr);
    void writeDataToSector(SectorPtr sectorPtr, const std::vector<uint8_t>& fileData, int& offset, int& bytesLeft);
    int writeFileDataToSectors(int start_track, int start_sector, const std::vector<uint8_t>& fileData);
    bool createDirectoryEntry(std::string_view filename, FileType type, int start_track, int start_sector, int allocated_sectors);
    bool findAndAllocateFirstSector(int& start_track, int& start_sector, std::string_view filename);
    inline void initBAMPtr()
    {
        auto index = calcOffset(DIRECTORY_TRACK, BAM_SECTOR);
        bamPtr = reinterpret_cast<BAMPtr>(&data[index]);
        bamTrackPtr = &(bamPtr->bam_track[0]);
        bamExtraTrackPtr = reinterpret_cast<BAM_TRACK_ENTRY*>(&data[index + 0xAC]);
    }
    bool isValidTrackSector(int track, int sector) const;

};

#pragma pack(pop)
