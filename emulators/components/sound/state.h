/*
 * Adapter header for FBNeo state saving
 */

#ifndef STATE_H
#define STATE_H

#include "driver.h"

#ifdef __cplusplus
 extern "C" {
#endif

// Action flags for state saving
#define ACB_READ            (1<<0)
#define ACB_WRITE           (1<<1)
#define ACB_DRIVER_DATA     (1<<6)

// ScanVar function stub
static inline void ScanVar(void* pv, INT32 nSize, char* szName) {
    (void)pv;
    (void)nSize;
    (void)szName;
}

// Stub out state saving for now (can be implemented if needed)
#define SCAN_VAR(x) ScanVar(&x, sizeof(x), #x)

#ifdef __cplusplus
}
#endif

#endif /* STATE_H */
