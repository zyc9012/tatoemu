/*
 * Compatibility shim for FBNeo's burnint.h
 * Provides the types and stubs needed by the ARM7 CPU core
 */
#ifndef BURNINT_H
#define BURNINT_H

#include "../../compact.h"

// Stub out FBNeo scan/state system (we use our own save state mechanism)
#define ACB_DRIVER_DATA 0
struct BurnArea {
    unsigned char* Data;
    unsigned int nLen;
    const char* szName;
};
static inline void BurnAcb(BurnArea*) {}
#define SCAN_VAR(x) do { (void)(x); } while(0)

#endif // BURNINT_H
