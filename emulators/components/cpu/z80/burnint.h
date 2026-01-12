// Compatibility header for FBNeo Z80
// Maps FBNeo types to our codebase types

#ifndef _BURNINT_H
#define _BURNINT_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// Type definitions matching FBNeo
typedef unsigned char   UINT8;
typedef signed char    INT8;
typedef unsigned short UINT16;
typedef signed short   INT16;
typedef unsigned int   UINT32;
typedef signed int     INT32;

#ifdef _MSC_VER
typedef signed __int64     INT64;
typedef unsigned __int64   UINT64;
#else
typedef unsigned long long UINT64;
typedef long long          INT64;
#endif

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
typedef int bool;
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

// SCAN_VAR macro (for save states - stub for now)
#define SCAN_VAR(x) ((void)0)

// Action flags for save/load states
#define ACB_NONE    0
#define ACB_READ    1
#define ACB_WRITE   2
#define ACB_RW      (ACB_READ | ACB_WRITE)

// Memory allocation stubs (FBNeo uses BurnMalloc, but we'll use standard malloc)
#define BurnMalloc(size) malloc(size)
#define BurnFree(ptr) free(ptr)

// Dummy macros/functions that FBNeo Z80 might need but we don't use
#define bprintf(...) ((void)0)
#define PRINT_ERROR 0
#define _T(x) x

// Z80 specific - these are set by Z80Init/Z80Set*Handler functions
// We'll handle these in our wrapper

#endif // _BURNINT_H
