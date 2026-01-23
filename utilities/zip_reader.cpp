#include "zip_reader.h"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>
#include <filesystem>

namespace util {

ZipReader::ZipReader()
    : m_initialized(false) {
    std::memset(&m_zipArchive, 0, sizeof(mz_zip_archive));
}

ZipReader::~ZipReader() {
    close();
}

bool ZipReader::open(const fs::path& filename) {
    close();
    
    // Read ZIP file into memory
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open ZIP file: " << filename << std::endl;
        return false;
    }
    
    u32 fileSize = static_cast<u32>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    m_zipData.resize(fileSize);
    file.read(reinterpret_cast<char*>(m_zipData.data()), fileSize);
    file.close();
    
    // Initialize miniz ZIP reader with memory buffer
    if (!mz_zip_reader_init_mem(&m_zipArchive, m_zipData.data(), m_zipData.size(), 0)) {
        std::cerr << "Failed to initialize ZIP archive" << std::endl;
        return false;
    }
    
    m_initialized = true;
    return true;
}

void ZipReader::close() {
    if (m_initialized) {
        mz_zip_reader_end(&m_zipArchive);
        std::memset(&m_zipArchive, 0, sizeof(mz_zip_archive));
        m_initialized = false;
    }
    m_zipData.clear();
}

std::vector<std::string> ZipReader::getFileList() {
    std::vector<std::string> files;
    
    if (!m_initialized) {
        return files;
    }
    
    mz_uint numFiles = mz_zip_reader_get_num_files(&m_zipArchive);
    for (mz_uint i = 0; i < numFiles; i++) {
        mz_zip_archive_file_stat fileStat;
        if (mz_zip_reader_file_stat(&m_zipArchive, i, &fileStat)) {
            // Skip directories
            if (!mz_zip_reader_is_file_a_directory(&m_zipArchive, i)) {
                files.push_back(fileStat.m_filename);
            }
        }
    }
    
    return files;
}

bool ZipReader::extractFile(const std::string& filename, std::vector<u8>& output) {
    if (!m_initialized) {
        return false;
    }
    
    size_t uncompSize = 0;
    void* p = mz_zip_reader_extract_file_to_heap(&m_zipArchive, filename.c_str(), &uncompSize, 0);
    if (!p) {
        return false;
    }
    
    output.resize(uncompSize);
    std::memcpy(output.data(), p, uncompSize);
    mz_free(p);
    
    return true;
}

bool ZipReader::findAndExtractFile(const std::set<std::string>& extensions, std::vector<u8>& output, std::string& foundFilename, bool topLevelOnly) {
    if (!m_initialized) {
        return false;
    }

    // Get list of files in the ZIP
    std::vector<std::string> files = getFileList();
    foundFilename.clear();

    // Search for files with matching extensions
    for (const auto& filename : files) {
        // Skip files in subdirectories if topLevelOnly is true
        if (topLevelOnly && (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos)) {
            continue;
        }

        fs::path filePath = filename;
        fs::path fileExt = filePath.extension();
        std::string extStr = fileExt.string();

        std::transform(extStr.begin(), extStr.end(), extStr.begin(), ::tolower);

        if (extensions.count(extStr) > 0) {
            if (!foundFilename.empty()) {
                std::cerr << "Multiple ROM files found in ZIP" << std::endl;
                return false;
            }
            foundFilename = filename;
        }
    }

    if (foundFilename.empty()) {
        std::cerr << "No ROM file found in ZIP" << std::endl;
        return false;
    }

    // Extract the found file
    return extractFile(foundFilename, output);
}

bool ZipReader::extractAll(std::map<std::string, std::vector<u8>>& files) {
    files.clear();
    
    if (!m_initialized) {
        return false;
    }
    
    mz_uint numFiles = mz_zip_reader_get_num_files(&m_zipArchive);
    for (mz_uint i = 0; i < numFiles; i++) {
        // Skip directories
        if (mz_zip_reader_is_file_a_directory(&m_zipArchive, i)) {
            continue;
        }
        
        mz_zip_archive_file_stat fileStat;
        if (mz_zip_reader_file_stat(&m_zipArchive, i, &fileStat)) {
            std::vector<u8> data;
            if (extractFile(fileStat.m_filename, data)) {
                files[fileStat.m_filename] = std::move(data);
            }
        }
    }
    
    return !files.empty();
}

} // namespace util
