#pragma once

#include "../../types.h"
#include "../../../utilities/miniz/miniz.h"
#include <filesystem>
#include <vector>
#include <string>
#include <map>

namespace cps1 {

// ZIP file reader using miniz library for extracting ROM files
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
    
    // Extract all files (for ROM loading)
    bool extractAll(std::map<std::string, std::vector<u8>>& files);

private:
    mz_zip_archive m_zipArchive;
    bool m_initialized;
    std::vector<u8> m_zipData;  // Keep ZIP data in memory for miniz
};

} // namespace cps1
