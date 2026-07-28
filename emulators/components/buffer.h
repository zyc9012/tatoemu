#pragma once

#include <stddef.h>
#ifdef __cplusplus
#include <vector>
#include <filesystem>
#endif

// Forward declaration for C compatibility
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Buffer {
#ifdef __cplusplus
    std::vector<std::byte> data;
    size_t read_pos;
    size_t write_pos;
#else
    size_t dummy;
#endif
} Buffer;

// Data access functions
void buffer_write(Buffer* buf, const void* data, size_t data_size);
size_t buffer_read(Buffer* buf, void* data, size_t data_size);

#ifdef __cplusplus
bool buffer_load_from_file(Buffer* buf, const std::filesystem::path& filename);
bool buffer_save_to_file(Buffer* buf, const std::filesystem::path& filename, bool create_backup = true);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct StateWriter {
    Buffer* buf;
    template <typename T> void operator()(T& field) const {
        buffer_write(buf, &field, sizeof(field));
    }
};

struct StateReader {
    Buffer* buf;
    template <typename T> void operator()(T& field) const {
        buffer_read(buf, &field, sizeof(field));
    }
};
#endif
