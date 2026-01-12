/*
 * Adapter header for FBNeo burnint functionality
 */

#ifndef BURNINT_H
#define BURNINT_H

#include "driver.h"
#include <cstdlib>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

// Memory allocation (use standard malloc/free)
#define BurnMalloc(size) malloc(size)
#define BurnFree(ptr) free(ptr)

// Sound rate (will be set by wrapper)
extern INT32 nBurnSoundRate;

// Interpolation setting (default to linear)
extern INT32 nInterpolation;

// Route definitions
#define BURN_SND_ROUTE_LEFT    0x01
#define BURN_SND_ROUTE_RIGHT   0x02
#define BURN_SND_ROUTE_BOTH    (BURN_SND_ROUTE_LEFT | BURN_SND_ROUTE_RIGHT)

// Debug flags (stub)
extern UINT8 DebugSnd_MSM6295Initted;
extern UINT8 DebugSnd_AY8910Initted;

// Clip function
#define BURN_SND_CLIP(x) \
    ((x) > 32767 ? 32767 : ((x) < -32768 ? -32768 : (x)))

#ifdef __cplusplus
}
#endif

#endif /* BURNINT_H */
