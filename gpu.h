#pragma once

#include <inttypes.h>

// extern volatile int numNewGpuFrames;
// extern int gpuFramebufferSizeBytes;

// extern int excessPixelsLeft;
// extern int excessPixelsRight;
// extern int excessPixelsTop;
// extern int excessPixelsBottom;

// #define FRAME_HISTORY_MAX_SIZE 240
// extern int frameTimeHistorySize;



extern int gpuFramebufferScanlineStrideBytes = 480;
extern int displayXOffset = 0;
extern int displayYOffset = 0;
extern int gpuFrameHeight = 240;
extern int gpuFrameWidth = 240;
