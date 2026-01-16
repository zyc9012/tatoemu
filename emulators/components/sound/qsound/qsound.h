// QSound (DL-1425) module header
// Adapted from FBNeo implementation

#ifndef _H_QSOUND_
#define _H_QSOUND_

#include "../../compact.h"
#include <fstream>

#define MAX_QSOUND (1)

// QSound output channels
#define BURN_SND_QSND_OUTPUT_1 0
#define BURN_SND_QSND_OUTPUT_2 1

// Initialize QSound emulator
INT32 QscInit(INT32 nRate);

// Set sample rate
void QscSetSampleRate(INT32 nRate);

// Reset QSound emulator
void QscReset();

// Shutdown QSound emulator
void QscExit();

// Set routing for QSound output channels
void QscSetRoute(INT32 nIndex, double nVolume, INT32 nRouteDir);

// Write to QSound register
void QscWrite(INT32 a, INT32 d);

// Read from QSound status register
UINT8 QscRead();

// Update QSound samples
INT32 QscUpdate(INT32 nLen);

// Set sample ROM data
void QscSetSampleROM(UINT8* rom, INT32 size);

// Get current sample values
INT16 QscGetLeftSample();
INT16 QscGetRightSample();

// Save/Load context for state saving
void QscSaveContext(std::ofstream& file);
void QscLoadContext(std::ifstream& file);

#endif