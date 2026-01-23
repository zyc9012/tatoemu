#pragma once

#include "../emulators/types.h"
#include "miniz/miniz.h"
#include <filesystem>
#include <vector>
#include <string>
#include <map>
#include <set>

namespace util {

// ZIP file reader using miniz library for extracting ROM files
// Universal utility for all emulators
class ZipReader {
public:
    ZipReader();
    ~ZipReader();

    bool open(const fs::path& filename);
    void close();
    
    // Get list of files in the ZIP
    std::vector<std::string> getFileList();
    
    // Extract a file from the ZIP
    bool extractFile(const std::string& filename, std::vector<u8>& output);

    // Find and extract a file by extension
    bool findAndExtractFile(const std::set<std::string>& extensions, std::vector<u8>& output, std::string& foundFilename, bool topLevelOnly = false);

    // Extract all files (for ROM loading)
    bool extractAll(std::map<std::string, std::vector<u8>>& files);

private:
    mz_zip_archive m_zipArchive;
    bool m_initialized;
    std::vector<u8> m_zipData;  // Keep ZIP data in memory for miniz
};

} // namespace util
