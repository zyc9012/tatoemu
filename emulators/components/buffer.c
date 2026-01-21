#include "compact.h"

// Memory Buffer implementation for serialization

Buffer* buffer_create(size_t initial_capacity) {
    Buffer* buf = (Buffer*)BurnMalloc(sizeof(Buffer));
    if (!buf) return NULL;

    buf->data = (UINT8*)BurnMalloc(initial_capacity);
    if (!buf->data) {
        BurnFree(buf);
        return NULL;
    }

    buf->size = 0;
    buf->capacity = initial_capacity;
    buf->read_pos = 0;
    buf->write_pos = 0;

    return buf;
}

void buffer_destroy(Buffer* buf) {
    if (!buf) return;
    if (buf->data) {
        BurnFree(buf->data);
    }
    BurnFree(buf);
}

int buffer_resize(Buffer* buf, size_t new_capacity) {
    if (!buf || new_capacity == 0) return 0;

    UINT8* new_data = (UINT8*)BurnMalloc(new_capacity);
    if (!new_data) return 0;

    // Copy existing data
    size_t copy_size = (buf->size < new_capacity) ? buf->size : new_capacity;
    memcpy(new_data, buf->data, copy_size);

    // Free old data
    BurnFree(buf->data);

    // Update buffer
    buf->data = new_data;
    buf->capacity = new_capacity;
    if (buf->size > new_capacity) {
        buf->size = new_capacity;
        if (buf->read_pos > buf->size) buf->read_pos = buf->size;
        if (buf->write_pos > buf->size) buf->write_pos = buf->size;
    }

    return 1;
}

static int buffer_ensure_capacity(Buffer* buf, size_t required_capacity) {
    if (buf->capacity >= required_capacity) return 1;

    // Double capacity until we have enough space
    size_t new_capacity = buf->capacity;
    while (new_capacity < required_capacity) {
        new_capacity *= 2;
        if (new_capacity < buf->capacity) { // Overflow check
            return 0;
        }
    }

    return buffer_resize(buf, new_capacity);
}

// Write operations
void buffer_write_data(Buffer* buf, const void* data, size_t data_size) {
    if (!buf || !data || data_size == 0) return;
    if (!buffer_ensure_capacity(buf, buf->write_pos + data_size)) return;

    memcpy(&buf->data[buf->write_pos], data, data_size);
    buf->write_pos += data_size;
    if (buf->write_pos > buf->size) buf->size = buf->write_pos;
}

// Read operations
size_t buffer_read_data(Buffer* buf, void* data, size_t data_size) {
    if (!buf || !data || data_size == 0) return 0;

    size_t available = buf->size - buf->read_pos;
    size_t actual_read = (data_size < available) ? data_size : available;

    if (actual_read > 0) {
        memcpy(data, &buf->data[buf->read_pos], actual_read);
        buf->read_pos += actual_read;
    }

    return actual_read;
}
