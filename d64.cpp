// Written by Paul Baxter

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <stdexcept>

#include "d64.h"

#pragma warning(disable:4267 28020)

/// <summary>
/// constructor with no parameters
/// create a blank disk
/// </summary>
d64::d64()
{
    disktype = thirty_five_track;
    init_disk();
}

/// <summary>
/// constructor with disktype
/// </summary>
/// <param name="type">disktype</param>
d64::d64(diskType type)
{
    disktype = type;
    init_disk();
}

/// <summary>
/// constructor with a file name
/// 
/// load the disk from an existing d64 file
/// </summary>
/// <param name="name"></param>
d64::d64(std::string name)
{
    // load the disk
    if (!load(name)) {
        throw std::invalid_argument("Unable to load disk");
    }
}

/// <summary>
/// Initialize 35 or 40 track
/// </summary>
void d64::init_disk()
{
    TRACKS = (disktype == thirty_five_track) ? TRACKS_35 : TRACKS_40;

    // allocate the data
    data.resize
    (disktype == thirty_five_track ? D64_DISK35_SZ : D64_DISK40_SZ, 1);

    // create a new disk
    formatDisk("NEW DISK");
}

// NOTE: track starts at 1. returns offset int datafor track and sector
inline int d64::calcOffset(int track, int sector) const
{
    if (track < 1 || track > TRACKS || sector < 0 || sector > SECTORS_PER_TRACK[track - 1]) {
        std::cerr << "Invalid Tack and Sector TRACK:" << track << " SECTOR:" << sector << std::endl;
        return -1;
    }
    return TRACK_OFFSETS[track - 1] + sector * SECTOR_SIZE;
}

/// <summary>
/// Initialize BAM
/// and name the disk
/// </summary>
/// <param name="name">new name for disk</param>
void d64::initBAM(std::string_view name)
{
    // set the BAM pointer
    bamPtr = getBAMPtr();

    // initialize the BAM fields
    // bamPtr must already be set!
    initializeBAMFields(name);

    // Mark all sectors free
    for (int t = 0; t < TRACKS; ++t) {
        bamtrack(t)->free = SECTORS_PER_TRACK[t];
        // There are more 16 sectors per track om all tracks
        // This allows us to use 0xFF for the next 2 bytes

        bamtrack(t)->bytes[0] = 0xFF;
        bamtrack(t)->bytes[1] = 0xFF;
        auto val = 0;

        auto bits = SECTORS_PER_TRACK[t] % 8;
        for (auto b = 0; b < bits; ++b) {
            val |= (1 << b);
        }

        bamtrack(t)->bytes[2] = val;
    }

    // Initialize the directory structure
    auto index = calcOffset(DIRECTORY_TRACK, DIRECTORY_SECTOR);
    std::fill_n(data.begin() + index, SECTOR_SIZE, 0);

    // mark as the last directory sector
    data[index + 1] = 0xFF;

    // allocate the BAM sector
    allocateSector(DIRECTORY_TRACK, BAM_SECTOR);

    // allocate the 1st directory sector
    allocateSector(DIRECTORY_TRACK, DIRECTORY_SECTOR);
}

/// <summary>
/// Rename the disk
/// </summary>
/// <param name="name">new name for disk</param>
bool d64::rename_disk(std::string_view name) const
{
    auto len = std::min(name.size(), static_cast<size_t>(DISK_NAME_SZ));
    std::copy_n(name.begin(), len, bamPtr->disk_name);
    std::fill(bamPtr->disk_name + len, bamPtr->disk_name + DISK_NAME_SZ, static_cast<char>(A0_VALUE));
    return true;
}

/// <summary>
/// Format the disk and set new name
/// </summary>
/// <param name="name">new name for disk</param>
void d64::formatDisk(std::string_view name)
{
    // format with 1's
    fill(data.begin(), data.end(), 0x01);

    // intialize BAM
    initBAM(name);
}

/// <summary>
/// write an entire sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="bytes">arrray of SECTOR_SIZE bytes</param>
/// <returns>true on success</returns>
bool d64::writeSector(int track, int sector, std::vector<uint8_t> bytes)
{
    if (bytes.size() != SECTOR_SIZE) return false;
    auto index = calcOffset(track, sector);
    if (index >= 0 && index + SECTOR_SIZE < data.size()) {
        for (auto& byte : bytes)
            data[index++] = byte;
        return true;
    }
    return false;
}

/// <summary>
/// Write a byte to a sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="offset">byte of sector</param>
/// <param name="value">value to write</param>
/// <returns>true on success</returns>
bool d64::writeSector(int track, int sector, int offset, uint8_t value)
{
    auto index = calcOffset(track, sector) + offset;
    if (index >= 0 && index < data.size()) {
        data[index] = value;
        return true;
    }
    return false;
}

/// <summary>
/// Read a byte from a sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <param name="offset">byte of sector</param>
/// <returns>optional data read</returns>
std::optional<uint8_t> d64::readSector(int track, int sector, int offset)
{
    auto index = calcOffset(track, sector) + offset;
    if (index >= 0 && index < data.size()) {
        auto value = data[index];
        return value;
    }
    return std::nullopt;
}

/// <summary>
/// Read a sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <returns>optional vector of data read</returns>
std::optional<std::vector<uint8_t>> d64::readSector(int track, int sector)
{
    std::vector<uint8_t> bytes;
    auto index = calcOffset(track, sector);
    if (index >= 0 && index + SECTOR_SIZE < data.size()) {
        for (auto byte = 0; byte < SECTOR_SIZE; ++byte) {
            bytes.push_back(data[index + byte]);
        }
        return bytes;
    }
    return std::nullopt;
}

/// <summary>
/// Add a file to the disk
/// for now do not support for .REL
/// </summary>
/// <param name="filename">file to load</param>
/// <param name="type">file type</param>
/// <param name="fileData">data to the file</param>
/// <returns>true if successful</returns>
bool d64::addFile(std::string_view filename, FileType type, const std::vector<uint8_t>& fileData)
{
    // see if the file already exists
    auto fileEntry = findFile(filename);
    if (fileEntry.has_value()) {
        std::cerr << "File already exists. " << filename << "\n";
        return false;
    }

    // to add a file
    // 1) check to see if we have enough free sectors
    // 2) for each block of a file find and allocate a sector
    // 3) the 1st 2 bytes point to next file block
    // 4) the last block set the track to 0 and sector to remaining bytes
    // 5) search the directory for an empty slot
    // 6) add the file to the empty slot

    // Find free sectors using BAM
    auto sz = fileData.size();
    auto sectors_needed = sz / (SECTOR_SIZE - 2);
    if (sz % (SECTOR_SIZE - 2)) {
        ++sectors_needed;
    }

    // see if we have the needed disk space
    auto free_sectors = getFreeSectorCount();
    if (free_sectors < sectors_needed) {
        std::cerr << "Disk full. Unable to add" << filename << "\n";
        return false;
    }

    // Write file data to sectors
    // BAM updated as each sector is written
    auto offset = 0;
    int start_track;
    int start_sector;
    int next_track;
    int next_sector;

    // find and allocate 1st sector for file
    if (!findAndAllocateFreeSector(next_track, next_sector)) {
        std::cerr << "Disk full. Unable to add" << filename << "\n";
        return false;
    }

    // save the start track and sector
    start_track = next_track;
    start_sector = next_sector;

    // loop until we write all the bytes of the file
    while (offset < sz) {
        // copy next_track sector to track and sector
        auto track = next_track;
        auto sector = next_sector;

        // check if we need another sector
        if (sz - offset > (SECTOR_SIZE - 2)) {
            if (!findAndAllocateFreeSector(next_track, next_sector)) {
                std::cerr << "Disk full. Unable to add" << filename << "\n";
                return false;
            }
        }
        else {
            // this is the last sector
            next_track = 0;                               // mark as final sector
            next_sector = static_cast<int>(sz) - offset;  // remaining bytes of file
        }

        // write the 1st 2 bytes the show the next track and sector of the file
        writeSector(track, sector, 0, static_cast<uint8_t>(next_track));
        writeSector(track, sector, 1, static_cast<uint8_t>(next_sector));

        // write the file to the disk
        for (auto byte = 0; byte < SECTOR_SIZE - 2; ++byte) {
            writeSector(track, sector, 2 + byte, (offset < sz) ? fileData[offset++] : 0);
        }
    }

    // set the directory start track and sector
    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;

    // get the directory sector
    Directory_SectorPtr dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

    // loop until we are out of directory sectors
    do {
        // get the 1st file entry the directory sector

        // loop through 8 entrys per sector
        for (auto file_entry_index = 1; file_entry_index <= 8; ++file_entry_index) {
            auto fileEntry = &(dirSectorPtr->fileEntry[file_entry_index - 1]);

            // if it is allocated entry skip
            if ((fileEntry->file_type.closed) != 0) {
                continue;
            }

            // set the file type for file
            fileEntry->file_type = type;

            // set the the 1st block of the file
            fileEntry->track = start_track;
            fileEntry->sector = start_sector;

            // set the name of the file
            auto len = std::min(filename.size(), static_cast<size_t>(FILE_NAME_SZ));
            std::copy_n(filename.begin(), len, fileEntry->file_name);
            std::fill(fileEntry->file_name + len, fileEntry->file_name + DISK_NAME_SZ, static_cast<char>(A0_VALUE));

            // these are only for REL files
            fileEntry->side_track = 0;
            fileEntry->side_sector = 0;
            fileEntry->record_length = 0;

            // clear the unused bytes
            std::fill_n(fileEntry->unused, 4, 0);

            // set the replace track and sector
            fileEntry->replace_track = fileEntry->track;
            fileEntry->replace_sector = fileEntry->sector;

            // set the file size
            fileEntry->file_size[0] = fileData.size() & 0xFF;
            fileEntry->file_size[1] = (fileData.size() & 0xFF00) >> 8;
            return true;
        }
        // check if there is another allocated directory sector if so go to it
        if (dir_track > 0 && dir_track < TRACKS && dirSectorPtr->sector < SECTORS_PER_TRACK[dir_track - 1]) {
            dirSectorPtr = getDirectory_SectorPtr(dirSectorPtr->track, dirSectorPtr->sector);
        }
        else {
            if (!findAndAllocateFreeSector(dir_track, dir_sector)) {
                std::cerr << "Disk full. Unable to add" << filename << "\n";
                return false;
            }

            dirSectorPtr->track = dir_track;
            dirSectorPtr->sector = dir_sector;

            // get the directory sector at newly allocated sector
            dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

            // zero the sector out
            memset(dirSectorPtr, 0, SECTOR_SIZE);

            // mark the sector with 0xFF to signify last directory sector
            dirSectorPtr->track = 0;
            dirSectorPtr->sector = 0xFF;
        }
        // we will either be sucessful or run out of disk space
        // so keep looping until that happens
    } while (true);
}

/// <summary>
/// verify the BAM inegrity
/// </summary>
/// <param name="fix">true to auto fix</param>
/// <param name="logFile">logfile name or "" for std::cerr</param>
/// <returns>true on success</returns>
bool d64::verifyBAMIntegrity(bool fix, const std::string& logFile)
{
    // Open log file if specified
    std::ofstream logStream;
    std::ostream* logOutput = &std::cerr;

    if (!logFile.empty()) {
        logStream.open(logFile, std::ios::out);
        if (logStream.is_open()) {
            logOutput = &logStream;
        }
        else {
            std::cerr << "WARNING: Failed to open log file. Logging to std::cerr instead.\n";
        }
    }

    // Temporary map to count sector usage
    std::array<std::array<bool, 21>, TRACKS_40> sectorUsage = {}; // Max sectors per track

    // **Step 1: Mark BAM itself as used**
    sectorUsage[DIRECTORY_TRACK - 1][BAM_SECTOR] = true;

    // **Step 2: Scan directory for used sectors**
    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;
    Directory_SectorPtr dirSectorPtr;

    while (dir_track != 0) {
        dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        // Mark directory sector as used
        sectorUsage[dir_track - 1][dir_sector] = true;

        for (int i = 0; i < FILES_PER_SECTOR; ++i) {
            auto& entry = dirSectorPtr->fileEntry[i];

            if ((entry.file_type.closed) == 0) continue; // Skip deleted files

            int track = entry.track;
            int sector = entry.sector;

            while (track != 0) {
                sectorUsage[track - 1][sector] = true;

                auto next_track = readSector(track, sector, 0);
                auto next_sector = readSector(track, sector, 1);

                track = next_track.value_or(0);
                sector = next_sector.value_or(0xFF);
            }
        }

        dir_track = dirSectorPtr->track;
        dir_sector = dirSectorPtr->sector;
    }

    // **Step 3: Compare BAM against actual usage**
    bool errorsFound = false;

    for (int track = 1; track <= TRACKS; ++track) {
        int correctFreeCount = 0;

        for (int sector = 0; sector < SECTORS_PER_TRACK[track - 1]; ++sector) {
            int byteIndex = sector / 8;
            int bitMask = (1 << (sector % 8));
            bool isFreeInBAM = bamtrack(track - 1)->bytes[byteIndex] & bitMask;
            bool isUsedInDirectory = sectorUsage[track - 1][sector];

            // Error: Sector incorrectly marked as used
            if (!isUsedInDirectory && !isFreeInBAM) {
                *logOutput << "ERROR: Sector " << sector << " on Track " << track
                    << " is incorrectly marked as used in BAM.\n";
                errorsFound = true;

                if (fix) {
                    *logOutput << "FIXING: Freeing sector " << sector << " on Track " << track << ".\n";
                    bamtrack(track - 1)->bytes[byteIndex] |= bitMask;
                }
            }

            // Error: Sector incorrectly marked as free
            else if (isUsedInDirectory && isFreeInBAM) {
                *logOutput << "ERROR: Sector " << sector << " on Track " << track
                    << " is incorrectly marked as free in BAM.\n";
                errorsFound = true;

                if (fix) {
                    *logOutput << "FIXING: Marking sector " << sector << " on Track " << track << " as used.\n";
                    bamtrack(track - 1)->bytes[byteIndex] &= ~bitMask;
                }
            }

            if (!isUsedInDirectory) {
                correctFreeCount++;
            }
        }

        // Error: Incorrect free sector count
        int free = bamtrack(track - 1)->free;

        if (free != correctFreeCount) {
            *logOutput << "WARNING: BAM free sector count mismatch on Track " << track
                << " (BAM: " << free
                << ", Expected: " << correctFreeCount << ")\n";
            errorsFound = true;

            if (fix) {
                *logOutput << "FIXING: Correcting free sector count for Track " << track << ".\n";
                bamtrack(track - 1)->free = correctFreeCount;
            }
        }
    }

    // Close log file if used
    if (logStream.is_open()) {
        logStream.close();
    }

    return !errorsFound; // Return true if BAM is valid
}

/// <summary>
/// Reorder the files on the disk
/// </summary>
/// <param name="fileOrder">order of files</param>
/// <returns>true on success</returns>
bool d64::reorderDirectory(const std::vector<std::string>& fileOrder)
{
    std::vector<Directory_Entry> files = directory();
    std::vector<Directory_Entry> reorderedFiles;

    // **Step 1: Add files in the specified order**
    for (const auto& filename : fileOrder) {
        auto it = std::find_if(files.begin(), files.end(), [&](const Directory_Entry& entry)
            {
                return Trim(entry.file_name) == filename;
            });

        if (it != files.end()) {
            reorderedFiles.push_back(*it);
            files.erase(it);
        }
    }

    // **Step 2: Append any remaining files that were not explicitly ordered**
    reorderedFiles.insert(reorderedFiles.end(), files.begin(), files.end());

    // **Step 3: Avoid unnecessary writes**
    if (directory() == reorderedFiles)
        return false; // No change needed

    // **Step 4: Write new order to directory**
    return reorderDirectory(reorderedFiles);
}

/// <summary>
/// compact directory
/// </summary>
/// <returns>true on success</returns>
bool d64::compactDirectory()
{
    std::vector<Directory_Entry> files;

    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;
    Directory_SectorPtr dirSectorPtr;

    // **Step 1: Collect all valid directory entries**
    while (dir_track != 0) {
        dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        for (int i = 0; i < FILES_PER_SECTOR; ++i) {
            auto& entry = dirSectorPtr->fileEntry[i];

            if ((entry.file_type.closed) == 0)
                continue; // Skip deleted files

            files.push_back(entry);
        }

        dir_track = dirSectorPtr->track;
        dir_sector = dirSectorPtr->sector;
    }

    if (files.empty()) return false; // No valid files

    // **Step 2: Rewrite the directory with compacted entries**
    dir_track = DIRECTORY_TRACK;
    dir_sector = DIRECTORY_SECTOR;
    dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

    size_t index = 0;
    bool freedSector = false;

    while (dir_track != 0) {
        std::fill_n(reinterpret_cast<uint8_t*>(dirSectorPtr), SECTOR_SIZE, 0); // Clear sector

        for (int i = 0; i < FILES_PER_SECTOR && index < files.size(); ++i, ++index) {
            dirSectorPtr->fileEntry[i] = files[index];
        }

        // **Step 3: If no more files, free remaining sectors**
        if (index >= files.size()) {
            // Mark remaining directory sectors as free in BAM
            while (dir_track != 0) {
                int next_track = dirSectorPtr->track;
                int next_sector = dirSectorPtr->sector;

                // never mark track 18 as free
                if (dir_track != DIRECTORY_TRACK || dir_sector != DIRECTORY_SECTOR) {

                    // Free the sector in BAM
                    freeSector(dir_track, dir_sector);
                    freedSector = true;
                }

                if (next_track == 0) break; // No more directory sectors

                dir_track = next_track;
                dir_sector = next_sector;
                dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
            }
            break;
        }

        dir_track = dirSectorPtr->track;
        dir_sector = dirSectorPtr->sector;
        if (dir_track != 0) dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
    }

    if (freedSector) {
        std::cerr << "FIXED: Freed unused directory sectors and updated BAM.\n";
    }

    return true;
}

/// <summary>
/// find a file on the disk
/// </summary>
/// <param name="filename">file to find</param>
/// <returns>optional pointer to the fiels directory entry</returns>
std::optional<d64::Directory_EntryPtr> d64::findFile(std::string_view filename)
{
    // set thE initial directory track and sector
    auto dir_track = DIRECTORY_TRACK;
    auto dir_sector = DIRECTORY_SECTOR;

    // track set to 0 signifies last directory sector
    while (dir_track != 0) {
        // get a pointer to current dirctory sector
        auto dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        // get the 1st file entry
        auto fileEntry = &(dirSectorPtr->fileEntry[0]);

        // loop through the 8 file entries
        for (int file_entry = 1; file_entry <= 8; ++file_entry, ++fileEntry) {
            // see if the file is allocated
            if ((fileEntry->file_type.closed) == 0) {
                continue;
            }
            // Extract and trim the file name
            std::string entryName(fileEntry->file_name, FILE_NAME_SZ);
            entryName.erase(std::find_if(entryName.begin(), entryName.end(), [](char c) { return c == static_cast<char>(A0_VALUE); }), entryName.end());
            if (entryName == filename) {
                return fileEntry;
            }
        }
        // get the next directory track and sector
        dir_track = dirSectorPtr->track;
        dir_sector = dirSectorPtr->sector;
    }

    // did not find the entry
    return std::nullopt;
}

/// <summary>
/// remove a file from the disk
/// </summary>
/// <param name="filename">file to remove</param>
/// <returns>true if successful</returns>
bool d64::removeFile(std::string_view filename)
{
    // find the file
    auto fileEntry = findFile(filename);
    // return false if we cant find it
    if (!fileEntry.has_value()) {
        std::cerr << "File not found: " << filename << std::endl;
        return false;
    }
    // Free all file sectors
    // get the start track and sector
    int track = fileEntry.value()->track;
    int sector = fileEntry.value()->sector;

    // now follow the next track sector of the fiule
    while (track != 0) {
        auto next_track = readSector(track, sector, 0);
        auto next_sector = readSector(track, sector, 1);
        freeSector(track, sector);
        track = next_track.value_or(0);
        sector = next_sector.value_or(0xFF);
    }

    // Mark directory entry as deleted
    memset(fileEntry.value(), 0, sizeof(Directory_Entry));
    return true;
}

/// <summary>
/// Rename a file
/// </summary>
/// <param name="oldfilename">old file name</param>
/// <param name="newfilename">new file name</param>
/// <returns>true if successful</returns>
bool d64::renameFile(std::string_view oldfilename, std::string_view newfilename)
{
    // see if we can find the file
    auto fileEntry = findFile(oldfilename);
    if (!fileEntry.has_value()) {
        std::cerr << "File not found: " << oldfilename << std::endl;
        return false;
    }
    // padd the name with A0 and limit length to FILE_NAME_SZ
    auto len = std::min(newfilename.size(), static_cast<size_t>(FILE_NAME_SZ));
    std::copy_n(newfilename.begin(), len, fileEntry.value()->file_name);
    std::fill(fileEntry.value()->file_name + len, fileEntry.value()->file_name + FILE_NAME_SZ, static_cast<char>(A0_VALUE));
    return true;
}

/// <summary>
/// extract a file from the disk
/// </summary>
/// <param name="filename">file to extrack</param>
/// <returns>tru if successful</returns>
bool d64::extractFile(std::string filename)
{
    // find the file
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        std::cerr << "File not found: " << filename << std::endl;
        return false;
    }

    // set the file extension based off file type
    std::string ext;
    if (fileEntry.value()->file_type.type == FileTypes::PRG) {
        ext = ".prg";
    }
    else if (fileEntry.value()->file_type.type == FileTypes::SEQ) {
        ext = ".seq";
    }
    else if (fileEntry.value()->file_type.type == FileTypes::USR) {
        ext = ".usr";
    }
    else if (fileEntry.value()->file_type.type == FileTypes::REL) {
        ext = ".rel";
    }
    else {
        std::cerr << "Unknown file type: " << static_cast<uint8_t>(fileEntry.value()->file_type) << std::endl;
        return false;
    }
    std::ofstream outFile((filename + ext).c_str(), std::ios::binary);

    // get the files start track and sector
    int track = fileEntry.value()->track;
    int sector = fileEntry.value()->sector;

    // track will be 0 at end of the file
    while (track != 0) {
        // get teh next track and sector of file
        auto next_track = readSector(track, sector, 0);
        auto next_sector = readSector(track, sector, 1);

        // get the byte offset of the file sector
        auto offset = calcOffset(track, sector);
        // add 2 to skip past next track and sector
        offset += 2;

        // see if we need to write the whole sector
        // if the track is not zero then write the whole block
        if (next_track.has_value() && next_track.value() != 0) {
            outFile.write(reinterpret_cast<char*>(&data[offset]), SECTOR_SIZE - 2);
        }
        else {
            // the sector in the last block is actually bytes left of the file
            int bytes_left = static_cast<int>(next_sector.value());
            outFile.write(reinterpret_cast<char*>(&data[offset]), bytes_left);
        }
        // set current track and sector
        track = next_track.value_or(0);
        sector = next_sector.value_or(0xFF);
    }
    // close the file
    outFile.close();

    // exit
    return true;
}

/// <summary>
/// get file data from the disk
/// </summary>
/// <param name="filename">file to extrack</param>
/// <returns>tru if successful</returns>
std::optional<std::vector<uint8_t>> d64::getFile(std::string filename)
{
    // find the file
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        std::cerr << "File not found: " << filename << std::endl;
        return std::nullopt;
    }
    std::vector<uint8_t> fileData;

    // get the files start track and sector
    int track = fileEntry.value()->track;
    int sector = fileEntry.value()->sector;

    // track will be 0 at end of the file
    while (track != 0) {
        // get the next track and sector of file
        auto next_track = readSector(track, sector, 0);
        auto next_sector = readSector(track, sector, 1);

        // get the byte offset of the file sector
        auto offset = calcOffset(track, sector);
        // add 2 to skip past next track and sector
        offset += 2;

        // see if we need to write the whole sector
        // if the track is not zero then write the whole block
        if (next_track.has_value() && next_track.value() != 0) {
            for (auto byte = 0; byte < SECTOR_SIZE - 2; ++byte) {
                fileData.push_back(data[offset + byte]);
            }
        }
        else {
            // the sector in the last block is actually bytes left of the file
            int bytes_left = static_cast<int>(next_sector.value());
            for (auto byte = 0; byte < bytes_left; ++byte) {
                fileData.push_back(data[offset + byte]);
            }
        }

        // set current track and sector
        track = next_track.value_or(0);
        sector = next_sector.value_or(0xFF);
    }

    // exit
    return fileData;
}

/// <summary>
/// Get the name of the disk
/// </summary>
/// <returns>Name of the disk</returns>
std::string d64::diskname()
{
    std::string name;
    if (bamPtr) {
        for (auto& ch : bamPtr->disk_name) {
            if (ch == static_cast<char>(0xA0)) return name;
            name += ch;
        }
    }
    return name;
}

/// <summary>
/// save image to disk
/// </summary>
/// <param name="filename">name of file</param>
/// <returns>true if successful</returns>
bool d64::save(std::string filename)
{
    // open the file
    std::ofstream outFile(filename.c_str(), std::ios::binary);
    if (!outFile) {
        std::cerr << "Error: Could not open file for writing.\n";
        return false;
    }
    // write all the data
    outFile.write(reinterpret_cast<char*>(data.data()), data.size());
    outFile.close();
    return true;
}

/// <summary>
/// load a disk image
/// </summary>
/// <param name="filename">name of .d64 file to load</param>
/// <returns>true if sucessful</returns>
bool d64::load(std::string filename)
{
    // open the file
    std::ifstream inFile(filename, std::ios::binary);
    if (!inFile) {
        std::cerr << "Error: Could not open disk file " << filename << " for reading.\n";
        return false;
    }
    inFile.seekg(0, SEEK_END);
    auto pos = inFile.tellg();
    inFile.seekg(0, SEEK_SET);
    if (pos == D64_DISK35_SZ) {
        disktype = thirty_five_track;
    }
    else if (pos == D64_DISK40_SZ) {
        disktype = forty_track;
    }
    else {
        inFile.close();
        std::cerr << "Error: Invalid disk " << filename << "\n";
        return false;
    }

    // allocate the disk
    init_disk();

    // read the data
    inFile.read(reinterpret_cast<char*>(data.data()), data.size());

    // close the file
    inFile.close();

    // validate the disk
    if (!validateD64()) {
        formatDisk("NEW DISK");
    }

    // exit
    return true;
}

/// <summary>
/// Free a sector
/// </summary>
/// <param name="track">Track number</param>
/// <param name="sector">sector number</param>
/// <returns>true if successful. If the sector is already free false</returns>
bool d64::freeSector(const int& track, const int& sector)
{
    // validate track sector number
    if (track < 1 || track > TRACKS || sector < 0 || sector > SECTORS_PER_TRACK[track]) {
        std::cerr << "Invalid Tack and Sector TRACK:" << track << " SECTOR:" << sector << std::endl;
        return false;
    }

    if (track == DIRECTORY_TRACK && sector == DIRECTORY_SECTOR) {
        std::cerr << "Warning: Attempt to free directory sector ignored (Track 18, Sector 1)\n";
        return false;
    }

    if (track == DIRECTORY_TRACK && sector == BAM_SECTOR) {
        std::cerr << "Warning: Attempt to free directory sector ignored (Track 18, Sector 0)\n";
        return false;
    }

    // calculate byte of BAM entry for track
    // is stored as a bitmap of 3 bytes. 1 if free and 0 if allocated
    auto byte = (sector / 8);
    auto bit = sector % 8;

    auto val = bamtrack(track - 1)->bytes[byte];

    // check if sector is already free
    if (val & (1 << bit)) {
        return false;
    }

    // increment free sectors
    // byte 0 is the number of free sectors
    bamtrack(track - 1)->free++;

    // mark track sector as free
    val |= (1 << bit);
    bamtrack(track - 1)->bytes[byte] = val;

    return true;
}

/// <summary>
/// allocate a sector
/// </summary>
/// <param name="track">Track number</param>
/// <param name="sector">sector number</param>
/// <returns>true if successful. If the sector is already allocated return false</returns>
bool d64::allocateSector(const int& track, const int& sector)
{
    // validate track sector number
    if (track < 1 || track > TRACKS || sector < 0 || sector > SECTORS_PER_TRACK[track]) {
        std::cerr << "Invalid Tack and Sector TRACK:" << track << " SECTOR:" << sector << std::endl;
        return false;
    }

    // calculate byte of BAM entry for track
    // is stored as a bitmap of 3 bytes. 1 if free and 0 if allocated
    auto byte = (sector / 8);
    auto bit = sector % 8;
    auto val = bamtrack(track - 1)->bytes[byte];

    // see if its already allocated
    if ((val | (1 << bit)) == 0) {
        return false;
    }

    val &= ~(1 << bit);

    bamtrack(track - 1)->free--;            // decrement free sectors
    bamtrack(track - 1)->bytes[byte] = val; // mark track sector as used

    return true;
}

/// <summary>
/// Find and allocate a sector in provided track
/// </summary>
/// <param name="track"></param>
/// <param name="sector"></param>
/// <returns></returns>
bool d64::findAndAllocateFree(int track, int& sector)
{
    // if there are no free sectors in the track go to next track
    if (bamtrack(track - 1)->free < 1) return false;

    auto start_sector = (lastSectorUsed[track - 1] + INTERLEAVE) % SECTORS_PER_TRACK[track - 1];

    // find the free sector
    for (auto i = 0; i < SECTORS_PER_TRACK[track - 1]; ++i) {
        auto s = (start_sector + i) % SECTORS_PER_TRACK[track - 1]; // Wrap around
        auto byte = (s / 8);
        auto bit = s % 8;
        auto val = bamtrack(track - 1)->bytes[byte];

        if (val & (1 << bit)) {
            allocateSector(track, s);
            sector = s;
            // update the last sector used for the track
            lastSectorUsed[track - 1] = sector;
            return true;
        }
    }
    return false;
}

/// <summary>
/// Find and allocate a sector
/// </summary>
/// <param name="track">out track number</param>
/// <param name="sector">out sector number</param>
/// <returns>true if successful</returns>
bool d64::findAndAllocateFreeSector(int& track, int& sector)
{
    // prioritized track search for free sectors
    static const std::array<int, TRACKS_35> TRACK_SEARCH_ORDER = {
        18, 17, 19, 16, 20, 15, 21, 14, 22, 13, 23, 12, 24, 11, 25, 10, 26, 9,
        27, 8, 28, 7, 29, 6, 30, 5, 31, 4, 32, 3, 33, 2, 34, 1, 35
    };

    static const std::array<int, 40> TRACK_40_SEARCH_ORDER = {
        18, 17, 19, 16, 20, 15, 21, 14, 22, 13, 23, 12, 24, 11, 25, 10, 26, 9,
        27, 8, 28, 7, 29, 6, 30, 5, 31, 4, 32, 3, 33, 2, 34, 1, 35, 36, 37, 38, 39, 40
    };

    if (disktype == TRACKS_35) {
        // iterate the tracks
        for (auto& t : TRACK_SEARCH_ORDER) {
            if (findAndAllocateFree(t, sector)) {
                track = t;
                return true;
            }
        }
    }
    else {
        for (auto& t : TRACK_40_SEARCH_ORDER) {
            if (findAndAllocateFree(t, sector)) {
                track = t;
                return true;
            }
        }
    }

    return false;
}

/// <summary>
/// Get a pointer the the directory at a given sector
/// </summary>
/// <param name="track">track number</param>
/// <param name="sector">sector number</param>
/// <returns>pointer to directory</returns>
inline d64::Directory_SectorPtr d64::getDirectory_SectorPtr(const int& track, const int& sector)
{
    // calculate data offset
    auto index = calcOffset(track, sector);
    return reinterpret_cast<Directory_SectorPtr>(&data[index]);
}

/// <summary>
/// get a pointer to BAM
/// </summary>
/// <returns>pointer to BAM</returns>
inline d64::BAMPtr d64::getBAMPtr()
{
    auto index = calcOffset(DIRECTORY_TRACK, BAM_SECTOR);
    return reinterpret_cast<BAMPtr>(&data[index]);
}

/// <summary>
/// Get the number of free sectors
/// </summary>
/// <returns>number of free sectors</returns>
uint16_t d64::getFreeSectorCount()
{
    // init free to 0
    uint16_t free = 0;

    // loop through tracks
    for (auto t = 1; t <= TRACKS; ++t) {

        // skip directory track
        if (t == DIRECTORY_TRACK)
            continue;

        // add the free bytes of each track
        free += static_cast<uint16_t>(bamtrack(t - 1)->free);
    }
    return free;
}

/// <summary>
/// Initialize the fields of BAM to default values
/// set the name of the disk
/// </summary>
/// <param name="bamPtr"></param>
/// <param name="name"></param>
void d64::initializeBAMFields(std::string_view name)
{
    // set directory track and sector
    bamPtr->dir_track = DIRECTORY_TRACK;
    bamPtr->dir_sector = DIRECTORY_SECTOR;

    // set dos version
    bamPtr->dos_version = DOS_VERSION;
    bamPtr->unused = 0;

    // Initialize disk name
    auto len = std::min(name.size(), static_cast<size_t>(DISK_NAME_SZ));
    std::copy_n(name.begin(), len, bamPtr->disk_name);
    std::fill(bamPtr->disk_name + len, bamPtr->disk_name + DISK_NAME_SZ, static_cast<char>(A0_VALUE));

    // Initialize unused fields
    bamPtr->a0[0] = A0_VALUE;
    bamPtr->a0[1] = A0_VALUE;

    // set the disk id
    bamPtr->disk_id[0] = A0_VALUE;
    bamPtr->disk_id[1] = A0_VALUE;
    bamPtr->unused2 = A0_VALUE;

    // set dos and version
    bamPtr->dos_type[0] = DOS_TYPE;
    bamPtr->dos_type[1] = DOS_VERSION;

    // fill in other unused fields
    std::fill_n(bamPtr->unused3, UNUSED3_SZ, 0x00);
    std::fill_n(bamPtr->unused4, UNUSED4_SZ, 0x00);
}

/// <summary>
/// Trim a file name
/// stops at A0 padding
/// </summary>
/// <param name="file_name">name to trim</param>
/// <returns>trimmed name</returns>
std::string d64::Trim(const char filename[FILE_NAME_SZ])
{
    std::string name(filename, FILE_NAME_SZ);
    name.erase(std::find_if(name.rbegin(), name.rend(), [](char c) { return c != static_cast<char>(A0_VALUE); }).base(), name.end());
    return name;
}

/// <summary>
/// Move a file to the top of the directory list
/// </summary>
/// <param name="file">File to move</param>
/// <returns>true on success</returns>
bool d64::movefileFirst(std::string file)
{
    std::vector<Directory_Entry> files = directory();

    auto it = std::find_if(files.begin(), files.end(), [&](const Directory_Entry& entry)
        {
            return Trim(entry.file_name) == file;
        });

    if (it == files.end() || it == files.begin())
        return false;  // File not found or already at the top

    std::iter_swap(files.begin(), it);
    return reorderDirectory(files);
}

/// <summary>
/// Move a file up in the directory list
/// </summary>
/// <param name="file">File to move</param>
/// <returns>true on success</returns>
bool d64::movefile(std::string file, bool up)
{
    std::vector<Directory_Entry> files = directory();
    auto it = std::find_if(files.begin(), files.end(), [&](const Directory_Entry& entry)
        {
            return Trim(entry.file_name) == file;
        });

    if (it == files.end())
        return false;  // File not found

    if ((up && it == files.begin()) || (!up && std::next(it) == files.end()))
        return false;  // Already at top/bottom

    std::iter_swap(it, up ? std::prev(it) : std::next(it));
    return reorderDirectory(files);
}

/// <summary>
/// Lock a file
/// </summary>
/// <param name="filename">file to lock</param>
/// <returns>true on success</returns>
bool d64::lockfile(std::string filename)
{
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        std::cerr << "File not found. " << filename << "\n";
        return false;
    }
    fileEntry.value()->file_type.locked = 1;
    return true;
}

/// <summary>
/// Unlock a file
/// </summary>
/// <param name="filename">file to unlock</param>
/// <returns>true on success</returns>
bool d64::unlockfile(std::string filename)
{
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value()) {
        std::cerr << "File not found. " << filename << "\n";
        return false;
    }
    fileEntry.value()->file_type.locked = 0;
    return true;
}

/// <summary>
/// Reorder the directory by the vector for directory entrys
/// </summary>
/// <param name="files">vector of directory entries</param>
/// <returns>true on success</returns>
bool d64::reorderDirectory(std::vector<Directory_Entry>& files)
{
    std::vector<Directory_Entry> currentFiles = directory();

    if (currentFiles == files)
        return false;  // No need to rewrite if already in the correct order

    // Rewrite directory only if order changed
    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;
    auto dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
    size_t index = 0;

    while (dir_track != 0 && index < files.size()) {
        std::fill_n(reinterpret_cast<uint8_t*>(dirSectorPtr), SECTOR_SIZE, 0); // Clear sector

        for (int i = 0; i < FILES_PER_SECTOR && index < files.size(); ++i, ++index) {
            dirSectorPtr->fileEntry[i] = files[index];
        }

        dir_track = dirSectorPtr->track;
        dir_sector = dirSectorPtr->sector;
        if (dir_track != 0) dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);
    }
    return true;
}

/// <summary>
/// reorder a directory based on a compare function
/// </summary>
/// <param name="compare">function comparer for directory entries</param>
/// <returns>true on success</returns>
bool d64::reorderDirectory(std::function<bool(const Directory_Entry&, const Directory_Entry&)> compare)
{
    // get the current directory entries
    std::vector<Directory_Entry> files = directory();

    if (files.empty())
        return false; // No files to reorder

    // Sort files based on user-defined comparison function**
    std::sort(files.begin(), files.end(), compare);

    // reorder based on sorted list
    return reorderDirectory(files);
}

/// <summary>
/// return the current directory entries
/// </summary>
/// <returns>current directory entries</returns>
std::vector<d64::Directory_Entry> d64::directory()
{
    std::vector<Directory_Entry> files;

    int dir_track = DIRECTORY_TRACK;
    int dir_sector = DIRECTORY_SECTOR;
    Directory_SectorPtr dirSectorPtr;

    // Read all directory entries
    while (dir_track != 0) {
        dirSectorPtr = getDirectory_SectorPtr(dir_track, dir_sector);

        for (int i = 0; i < FILES_PER_SECTOR; ++i) {
            auto& entry = dirSectorPtr->fileEntry[i];
            if ((entry.file_type.closed) == 0)
                continue; // Skip deleted files
            files.push_back(entry);
        }

        dir_track = dirSectorPtr->track;
        dir_sector = dirSectorPtr->sector;
    }
    return files;
}

/// <summary>
/// Validate that this is a .d64 disk
/// </summary>
/// <returns>true if valid</returns>
bool d64::validateD64()
{
    // Check file size
    auto sz = disktype == thirty_five_track ? D64_DISK35_SZ : D64_DISK40_SZ;
    if (data.size() != sz) {
        std::cerr << "Error: Invalid .d64 size (" << data.size() << " bytes).\n";
        return false;
    }

    // Check BAM structure
    if (bamPtr->dir_track != DIRECTORY_TRACK || bamPtr->dir_sector != DIRECTORY_SECTOR) {
        std::cerr << "Error: BAM structure is invalid (Incorrect directory track/sector).\n";
        return false;
    }

    // Check sector data integrity (optional deeper check)
    auto track = readSector(DIRECTORY_TRACK, DIRECTORY_SECTOR, 0);
    auto sector = readSector(DIRECTORY_TRACK, DIRECTORY_SECTOR, 1);
    auto valid = track == DIRECTORY_TRACK || (track == 0 && sector == 0xFF);
    if (!valid) {
        std::cerr << "Error: Directory sector does not match expected values.\n";
        return false;
    }

    return true;
}

/// <summary>
/// Extract a .rel file from the disk 
/// </summary>
/// <param name="filename">file to extract</param>
/// <returns>true on sucess</returns>
bool d64::extractRELFile(const std::string& filename)
{
    auto fileEntry = findFile(filename);
    if (!fileEntry.has_value() || fileEntry.value()->file_type.type != FileTypes::REL) {
        std::cerr << "Error: \"" << filename << "\" is not a REL file.\n";
        return false;
    }

    // Get initial track/sector & record length
    int track = fileEntry.value()->track;
    int sector = fileEntry.value()->sector;
    int sideTrack = fileEntry.value()->side_track;
    int sideSector = fileEntry.value()->side_sector;
    int recordLength = fileEntry.value()->record_length;

    if (recordLength == 0) {
        std::cerr << "Error: Invalid REL file structure.\n";
        return false;
    }

    std::ofstream outFile(filename + ".rel", std::ios::binary);
    std::vector<int> recordMap;  // Holds the logical order of sectors

    std::cout << "Extracting REL file: " << filename << "\n";
    std::cout << "Main data starts at Track " << track << ", Sector " << sector << "\n";
    std::cout << "Side sector chain starts at Track " << sideTrack << ", Sector " << sideSector << "\n";
    std::cout << "Record length: " << recordLength << " bytes\n";

    // Read the side sector chain
    while (sideTrack != 0) {
        int offset = calcOffset(sideTrack, sideSector);
        std::cout << "Reading side sector at Track " << sideTrack << ", Sector " << sideSector << "\n";

        // Each side sector stores pointers to logical record sectors
        for (int i = 2; i < SECTOR_SIZE; i += 2) {  // Skip first 2 bytes (next track/sector)
            uint8_t recTrack = data[offset + i];
            uint8_t recSector = data[offset + i + 1];

            if (recTrack == 0) break;  // End of side sector

            int recOffset = calcOffset(recTrack, recSector);
            recordMap.push_back(recOffset);

            std::cout << "Record maps to Track " << (int)recTrack << ", Sector " << (int)recSector << "\n";
        }

        // Move to the next side sector in the chain
        sideTrack = data[offset];    // Next track
        sideSector = data[offset + 1]; // Next sector
    }

    // Extract file data using the logical record mapping
    for (int recOffset : recordMap) {
        outFile.write(reinterpret_cast<char*>(&data[recOffset]), recordLength);
    }

    outFile.close();
    std::cout << "REL file extracted: " << filename << ".rel\n";
    return true;
}
