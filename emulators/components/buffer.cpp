#include "buffer.h"
#include <algorithm>
#include <cstring>
#include <filesystem>

void buffer_write(Buffer* buf, const void* data, size_t data_size) {
    if (!buf || !data || data_size == 0) return;

    size_t needed_size = buf->write_pos + data_size;
    buf->data.resize(needed_size);

    std::memcpy(buf->data.data() + buf->write_pos, data, data_size);
    buf->write_pos += data_size;
}

size_t buffer_read(Buffer* buf, void* data, size_t data_size) {
    if (!buf || !data || data_size == 0) return 0;

    std::memcpy(data, buf->data.data() + buf->read_pos, data_size);
    buf->read_pos += data_size;

    return data_size;
}

bool buffer_load_from_file(Buffer* buf, const std::filesystem::path& filename) {
    FILE* file = fopen(filename.c_str(), "rb");
    if (!file) return false;

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buf->data.resize(file_size);
    fread(buf->data.data(), 1, file_size, file);
    fclose(file);
    return true;
}

bool buffer_save_to_file(Buffer* buf, const std::filesystem::path& filename) {
    FILE* file = fopen(filename.c_str(), "wb");
    if (!file) return false;

    fwrite(buf->data.data(), 1, buf->data.size(), file);
    fclose(file);
    return true;
}
