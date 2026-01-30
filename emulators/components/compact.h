/*
 * Compactibility header extracted from FBNeo
 */

#ifndef COMPACT_H
#define COMPACT_H

// Standard includes
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#if !defined (_WIN32)
 #define __cdecl
 #define __fastcall
#endif

#ifndef INLINE
 #define INLINE static inline
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Type definitions matching FBNeo
typedef uint8_t     UINT8;
typedef int8_t      INT8;
typedef uint16_t    UINT16;
typedef int16_t     INT16;
typedef uint32_t    UINT32;
typedef int32_t     INT32;
typedef uint64_t    UINT64;
typedef int64_t     INT64;

// Boolean definitions
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

// C++ bool support for C code
#ifdef __cplusplus
// C++ has bool, true, false built-in
#else
#define true 1
#define false 0
#endif

// Endianness - assume little endian (LSB_FIRST) for most modern systems
#ifndef LSB_FIRST
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define LSB_FIRST 1
#elif defined(_WIN32) || defined(__x86_64__) || defined(__i386__)
#define LSB_FIRST 1
#else
#define LSB_FIRST 0
#endif
#endif

// PAIR type for endianness handling
#ifdef LSB_FIRST
typedef union {
    struct { UINT8 l,h,h2,h3; } b;
    struct { UINT16 l,h; } w;
    UINT32 d;
} PAIR;
#else
typedef union {
    struct { UINT8 h3,h2,h,l; } b;
    struct { UINT16 h,l; } w;
    UINT32 d;
} PAIR;
#endif

// STRUCT_SIZE_HELPER macro
#define STRUCT_SIZE_HELPER(type, member) offsetof(type, member) + sizeof(((type*)0)->member)

// Memory allocation (use standard malloc/free)
#define BurnMalloc(size) malloc(size)
#define BurnFree(ptr) free(ptr)

// Route definitions
#define BURN_SND_ROUTE_LEFT         0x01
#define BURN_SND_ROUTE_RIGHT        0x02
#define BURN_SND_ROUTE_BOTH         (BURN_SND_ROUTE_LEFT | BURN_SND_ROUTE_RIGHT)
#define BURN_SND_ROUTE_PANLEFT      4
#define BURN_SND_ROUTE_PANRIGHT     8

// Clip function
#define BURN_SND_CLIP(x) \
    ((x) > 32767 ? 32767 : ((x) < -32768 ? -32768 : (x)))

// write8_handler type
#ifndef _H_FM_FM_
typedef UINT8 (*read8_handler)(UINT32 offset);
typedef void (*write8_handler)(UINT32 offset, UINT32 data);
#endif

// Stub out logerror (can be implemented if needed)
#define logerror(...) ((void)0)

// Dummy macros/functions that FBNeo Z80 might need but we don't use
#define bprintf(...) ((void)0)
#define PRINT_ERROR 0
#define _T(x) x

/* OPN chip enable flags */
#define HAS_YM2203  0
#define HAS_YM2608  0
#define HAS_YM2610  1
#define HAS_YM2610B 0
#define HAS_YM2612  0
#define HAS_YM3438  0
/* OPL */
#define HAS_YM3812  0
#define HAS_YM3526  0
#define HAS_Y8950   0

#define timer_get_time() 0.0

#ifdef __cplusplus
}
#endif

#endif /* COMPACT_H */
