// Written by Paul Baxter
#pragma once
#include <string>
#include <array>
#include <optional>
#include <functional>
#include <algorithm>
#include <cstdint>

#pragma pack(push, 1)

const int TRACKS = 35;
const int SECTOR_SIZE = 256;
const int DISK_NAME_SZ = 16;
const int FILE_NAME_SZ = 16;
const int UNUSED_SZ = 85;
const int DIR_ENTRY_SZ = 30;
const int DIRECTORY_TRACK = 18;
const int DIRECTORY_SECTOR = 1;
const int BAM_SECTOR = 0;
const int FILES_PER_SECTOR = 8;

const int D64_DISK_SZ = 174848;

class d64 {
public:
    // Constants for D64 format
    const std::array<int, TRACKS> SECTORS_PER_TRACK = {
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, // Tracks 1-17
        19, 19, 19, 19, 19, 19, 19,                                         // Tracks 18-24
        18, 18, 18, 18, 18, 18,                                             // Tracks 25-30
        17, 17, 17, 17, 17                                                  // Tracks 31-35
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

    class FileType {
        public:
            FileTypes type : 6;
            uint8_t locked : 1;
            uint8_t allocated : 1;

    public:
        FileType() : allocated(0), locked(0), type(FileTypes::DEL) {}
        FileType(bool a, bool l, FileTypes t) : allocated(a ? 1 : 0), locked(l ? 1 : 0), type(t) {}
        FileType(FileTypes t) : allocated(1), locked(0), type(t) {}
        FileType(uint8_t value) : allocated(value & 0x80), locked(value & 0x40), type(static_cast<FileTypes>(value & 0x3F)) {}
 
        operator uint8_t() const { return (allocated << 7) | (locked << 6) | type; }
        operator FileTypes() const { return type; }        
       
    };

    struct BAM_TRACK_ENTRY {
        uint8_t free;
        std::array<uint8_t, 3> bytes;
    };

    struct BAM {
        uint8_t dir_track;                  // $00          track of directory entry
        uint8_t dir_sector;                 // $01          sector of next directory entry
        uint8_t dos_version;                // $02          'A' dos version
        uint8_t unused;                     // $03          unused should be 0
        BAM_TRACK_ENTRY bam_track[TRACKS];  // $04 - $8F    BAM to each track
        char disk_name[DISK_NAME_SZ];       // $90 - $9F    disk name padded with A0
        uint8_t a0[2];                      // $A0 - $A1    contains A0
        uint8_t disk_id[2];                 // $A2 - $A3    disk id
        uint8_t unused2;                    // $A4          contains A0
        char dos_type[2];                   // $A5 - $A6    '2' 'A'
        uint8_t unused3[4];                 // $A7 - $AA    A0
        uint8_t unused4[UNUSED_SZ];         // $AB - $FF    00
    };
    typedef struct BAM* BAMPtr;

    struct Directory_Entry {
        FileType file_type;                 // $00          file type
        uint8_t track;                      // $01          first track of file entry
        uint8_t sector;                     // $02          first sector of file entry
        char file_name[FILE_NAME_SZ];       // $03 - $12    file name padded with $A0
        uint8_t side_track;                 // $13          first side track .REL file only
        uint8_t side_sector;                // $14          first side track .REL file only
        uint8_t record_length;              // $15          record side track .REL file only
        uint8_t unused[4];                  // $16 - $19    unused
        uint8_t replace_track;              // $1A          track of replacement file during @save
        uint8_t replace_sector;             // $1B          sector of replacement file during @save
        uint8_t file_size[2];               // $1C - $1D    low byte high byte for file size
        uint8_t padd[2];                    // $1E - $1F    undocumented padd

        bool operator==(const Directory_Entry& other) const
        {
            return (uint8_t)file_type == (uint8_t)other.file_type &&
                track == other.track &&
                sector == other.sector &&
                std::memcmp(file_name, other.file_name, FILE_NAME_SZ) == 0 &&
                side_track == other.side_track &&
                side_sector == other.side_sector &&
                record_length == other.record_length &&
                std::memcmp(unused, other.unused, sizeof(unused)) == 0 &&
                replace_track == other.replace_track &&
                replace_sector == other.replace_sector &&
                std::memcmp(file_size, other.file_size, sizeof(file_size)) == 0;
        }
        bool operator!=(const Directory_Entry& other) const
        {
            return !(*this == other);
        }
    };
    typedef struct Directory_Entry* Directory_EntryPtr;

    struct Directory_Sector {
        uint8_t track;
        uint8_t sector;
        Directory_Entry fileEntry[FILES_PER_SECTOR];
    };
    typedef struct Directory_Sector* Directory_SectorPtr;

    d64();
    d64(std::string name);

    void formatDisk(std::string_view name);
    bool rename_disk(std::string_view name) const;
    std::string diskname();

    std::optional<Directory_EntryPtr> findFile(std::string_view filename);
    bool addFile(std::string_view filename, FileType type, const std::vector<uint8_t>& fileData);
    bool removeFile(std::string_view filename);
    bool renameFile(std::string_view oldfilename, std::string_view newfilename);
    bool extractFile(std::string filename);
    bool save(std::string filename);
    bool load(std::string filename);

    int calcOffset(int track, int sector) const;
    bool writeSector(int track, int sector, int offset, uint8_t value);
    bool writeSector(int track, int sector, std::vector<uint8_t> bytes);
    std::optional<uint8_t> readSector(int track, int sector, int offset);
    std::optional<std::vector<uint8_t>> readSector(int track, int sector);
    bool freeSector(const int& track, const int& sector);
    bool allocateSector(const int& track, const int& sector);
    bool findAndAllocateFreeSector(int& track, int& sector);
    std::optional<std::vector<uint8_t>> getFile(std::string filename);

    uint16_t getFreeSectorCount();
    BAMPtr getBAMPtr();
    Directory_SectorPtr getDirectory_SectorPtr(const int& track, const int& sector);
    bool compactDirectory();
    bool verifyBAMIntegrity(bool fix, const std::string& logFile);
    bool reorderDirectory(std::function<bool(const Directory_Entry&, const Directory_Entry&)> compare);
    bool reorderDirectory(std::vector<Directory_Entry>& files);
    bool reorderDirectory(const std::vector<std::string>& fileOrder);
    bool movefileFirst(std::string file);
    bool movefile(std::string file, bool up);
    bool lockfile(std::string file);
    bool unlockfile(std::string file);
    std::vector<Directory_Entry> directory();
    static std::string Trim(const char filename[FILE_NAME_SZ]);

private:
    static constexpr int INTERLEAVE = 10;
    std::array<int, TRACKS> lastSectorUsed = { -1 };

    void initBAM(std::string_view name);
    void initializeBAMFields(std::string_view name);
    BAMPtr bamPtr;
};

#pragma pack(pop)
 