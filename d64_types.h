// Written by Paul Baxter
#pragma once

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

static constexpr uint8_t A0_VALUE = 0xA0;
static constexpr uint8_t DOS_VERSION = 'A';
static constexpr uint8_t DOS_TYPE = '2';

enum diskType {
    thirty_five_track,
    forty_track
};

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

#pragma pack(pop)
