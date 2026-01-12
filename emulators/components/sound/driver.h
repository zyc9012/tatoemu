/*
 * Adapter header for FBNeo sound cores
 * Maps FBNeo types to project types
 */

#ifndef DRIVER_H
#define DRIVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>

#if !defined (_WIN32)
 #define __cdecl
#endif

#ifndef INLINE
 #define INLINE static inline
#endif

// Map FBNeo types to standard types
typedef uint8_t     UINT8;
typedef int8_t      INT8;
typedef uint16_t    UINT16;
typedef int16_t     INT16;
typedef uint32_t    UINT32;
typedef int32_t     INT32;
typedef uint64_t    UINT64;
typedef int64_t     INT64;

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

// Stub out logerror (can be implemented if needed)
#define logerror(...) ((void)0)

// Macro to determine the size of a struct up to and including "member"
#define STRUCT_SIZE_HELPER(type, member) offsetof(type, member) + sizeof(((type*)0)->member)

// write8_handler type (port, data)
typedef void (*write8_handler)(UINT8 port, UINT8 data);

#endif /* DRIVER_H */
