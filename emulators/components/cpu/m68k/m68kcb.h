#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Callback function types for Musashi memory access */
typedef unsigned int (*m68k_read_8_callback)(unsigned int address);
typedef unsigned int (*m68k_read_16_callback)(unsigned int address);
typedef unsigned int (*m68k_read_32_callback)(unsigned int address);
typedef void (*m68k_write_8_callback)(unsigned int address, unsigned int value);
typedef void (*m68k_write_16_callback)(unsigned int address, unsigned int value);
typedef void (*m68k_write_32_callback)(unsigned int address, unsigned int value);

/* Structure to hold all callbacks - 1:1 match with Musashi interface */
typedef struct {
    /* Read from anywhere */
    m68k_read_8_callback read_memory_8;
    m68k_read_16_callback read_memory_16;
    m68k_read_32_callback read_memory_32;
    
    /* Read data immediately following the PC */
    m68k_read_16_callback read_immediate_16;
    m68k_read_32_callback read_immediate_32;
    
    /* Read data relative to the PC */
    m68k_read_8_callback read_pcrelative_8;
    m68k_read_16_callback read_pcrelative_16;
    m68k_read_32_callback read_pcrelative_32;
    
    /* Memory access for the disassembler */
    m68k_read_8_callback read_disassembler_8;
    m68k_read_16_callback read_disassembler_16;
    m68k_read_32_callback read_disassembler_32;
    
    /* Write to anywhere */
    m68k_write_8_callback write_memory_8;
    m68k_write_16_callback write_memory_16;
    m68k_write_32_callback write_memory_32;
    
    /* Special predecrement write (write high word first, then low word) */
    m68k_write_32_callback write_memory_32_pd;
} m68k_memory_callbacks;

/* Set the memory callbacks */
void m68k_set_memory_callbacks(const m68k_memory_callbacks* callbacks);

/* Clear all callbacks (set to NULL) */
void m68k_clear_memory_callbacks(void);

#ifdef __cplusplus
}
#endif
