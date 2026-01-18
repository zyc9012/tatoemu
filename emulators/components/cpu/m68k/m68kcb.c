#include "m68kcb.h"
#include <string.h>

/* Current callbacks */
static m68k_memory_callbacks s_callbacks = {0};

void m68k_set_memory_callbacks(const m68k_memory_callbacks* callbacks) {
    if (callbacks) {
        memcpy(&s_callbacks, callbacks, sizeof(m68k_memory_callbacks));
    } else {
        memset(&s_callbacks, 0, sizeof(m68k_memory_callbacks));
    }
}

void m68k_clear_memory_callbacks(void) {
    memset(&s_callbacks, 0, sizeof(m68k_memory_callbacks));
}

/* Musashi callback implementations - 1:1 match with original interface */

/* Read from anywhere */
unsigned int m68k_read_memory_8(unsigned int address) {
    if (s_callbacks.read_memory_8) {
        return s_callbacks.read_memory_8(address);
    }
    return 0;
}

unsigned int m68k_read_memory_16(unsigned int address) {
    if (s_callbacks.read_memory_16) {
        return s_callbacks.read_memory_16(address);
    }
    return 0;
}

unsigned int m68k_read_memory_32(unsigned int address) {
    if (s_callbacks.read_memory_32) {
        return s_callbacks.read_memory_32(address);
    }
    return 0;
}

/* Read data immediately following the PC */
unsigned int m68k_read_immediate_16(unsigned int address) {
    if (s_callbacks.read_immediate_16) {
        return s_callbacks.read_immediate_16(address);
    }
    return 0;
}

unsigned int m68k_read_immediate_32(unsigned int address) {
    if (s_callbacks.read_immediate_32) {
        return s_callbacks.read_immediate_32(address);
    }
    return 0;
}

/* Read data relative to the PC */
unsigned int m68k_read_pcrelative_8(unsigned int address) {
    if (s_callbacks.read_pcrelative_8) {
        return s_callbacks.read_pcrelative_8(address);
    }
    return 0;
}

unsigned int m68k_read_pcrelative_16(unsigned int address) {
    if (s_callbacks.read_pcrelative_16) {
        return s_callbacks.read_pcrelative_16(address);
    }
    return 0;
}

unsigned int m68k_read_pcrelative_32(unsigned int address) {
    if (s_callbacks.read_pcrelative_32) {
        return s_callbacks.read_pcrelative_32(address);
    }
    return 0;
}

/* Memory access for the disassembler */
unsigned int m68k_read_disassembler_8(unsigned int address) {
    if (s_callbacks.read_disassembler_8) {
        return s_callbacks.read_disassembler_8(address);
    }
    return 0;
}

unsigned int m68k_read_disassembler_16(unsigned int address) {
    if (s_callbacks.read_disassembler_16) {
        return s_callbacks.read_disassembler_16(address);
    }
    return 0;
}

unsigned int m68k_read_disassembler_32(unsigned int address) {
    if (s_callbacks.read_disassembler_32) {
        return s_callbacks.read_disassembler_32(address);
    }
    return 0;
}

/* Write to anywhere */
void m68k_write_memory_8(unsigned int address, unsigned int value) {
    if (s_callbacks.write_memory_8) {
        s_callbacks.write_memory_8(address, value);
    }
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
    if (s_callbacks.write_memory_16) {
        s_callbacks.write_memory_16(address, value);
    }
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
    if (s_callbacks.write_memory_32) {
        s_callbacks.write_memory_32(address, value);
    }
}

void m68k_write_memory_32_pd(unsigned int address, unsigned int value) {
    if (s_callbacks.write_memory_32_pd) {
        s_callbacks.write_memory_32_pd(address, value);
    }
}
