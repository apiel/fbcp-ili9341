#include <fcntl.h>
#include <linux/fb.h>
#include <linux/futex.h>
#include <linux/spi/spidev.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <math.h>
#include <signal.h>

#include "config.h"
#include "spi.h"
#include "gpu.h"
#include "tick.h"
#include "display.h"
#include "util.h"
#include "mailbox.h"
#include "diff.h"

int CountNumChangedPixels(uint16_t *framebuffer, uint16_t *prevFramebuffer)
{
  int changedPixels = 0;
  for (int y = 0; y < DISPLAY_HEIGHT; ++y)
  {
    for (int x = 0; x < DISPLAY_WIDTH; ++x)
      if (framebuffer[x] != prevFramebuffer[x])
        ++changedPixels;

    framebuffer += gpuFramebufferScanlineStrideBytes >> 1;
    prevFramebuffer += gpuFramebufferScanlineStrideBytes >> 1;
  }
  return changedPixels;
}

volatile bool programRunning = true;

void MarkProgramQuitting()
{
  programRunning = false;
}

void ProgramInterruptHandler(int signal)
{
  printf("Signal (%d) received, quitting\n", signal);
  static int quitHandlerCalled = 0;
  if (++quitHandlerCalled >= 5)
  {
    printf("Ctrl-C handler invoked five times, looks like fbcp-ili9341 is not gracefully quitting - performing a forcible shutdown!\n");
    exit(1);
  }
  MarkProgramQuitting();
  __sync_synchronize();
  // Wake the SPI thread if it was sleeping so that it can gracefully quit
  if (spiTaskMemory)
  {
    __atomic_fetch_add(&spiTaskMemory->queueHead, 1, __ATOMIC_SEQ_CST);
    __atomic_fetch_add(&spiTaskMemory->queueTail, 1, __ATOMIC_SEQ_CST);
    syscall(SYS_futex, &spiTaskMemory->queueTail, FUTEX_WAKE, 1, 0, 0, 0); // Wake the SPI thread if it was sleeping to get new tasks
  }

}

int main()
{
  signal(SIGINT, ProgramInterruptHandler);
  signal(SIGQUIT, ProgramInterruptHandler);
  signal(SIGUSR1, ProgramInterruptHandler);
  signal(SIGUSR2, ProgramInterruptHandler);
  signal(SIGTERM, ProgramInterruptHandler);

  OpenMailbox();
  InitSPI();

  // Track current SPI display controller write X and Y cursors.
  int spiX = -1;
  int spiY = -1;
  int spiEndX = DISPLAY_WIDTH;

  spans = (Span *)malloc((DISPLAY_WIDTH * DISPLAY_HEIGHT / 2) * sizeof(Span));

  int gpuFramebufferSizeBytes = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
  int size = gpuFramebufferSizeBytes;

  uint16_t *framebuffer[2] = {(uint16_t *)malloc(size), (uint16_t *)malloc(gpuFramebufferSizeBytes)};
  memset(framebuffer[0], 0, size);                    // Doublebuffer received GPU memory contents, first buffer contains current GPU memory,
  memset(framebuffer[1], 0, gpuFramebufferSizeBytes); // second buffer contains whatever the display is currently showing. This allows diffing pixels between the two.

  uint32_t curFrameEnd = spiTaskMemory->queueTail;
  uint32_t prevFrameEnd = spiTaskMemory->queueTail;

  bool prevFrameWasInterlacedUpdate = false;
  bool interlacedUpdate = false; // True if the previous update we did was an interlaced half field update.
  int frameParity = 0;           // For interlaced frame updates, this is either 0 or 1 to denote evens or odds.
  printf("All initialized, now running main loop...\n");
  // while (programRunning)
  // {
  //   bool framebufferHasNewChangedPixels = true;

  //   // Draw some pixel in framebuffer[0] if we have new data.
  //   // draw diagonal line
  //   for (int y = 0; y < DISPLAY_DRAWABLE_HEIGHT; ++y)
  //     framebuffer[0][y * DISPLAY_DRAWABLE_WIDTH + y] = 0xff00ff;

  //   // If too many pixels have changed on screen, drop adaptively to interlaced updating to keep up the frame rate.
  //   double inputDataFps = 1000000.0 / (1000000 / TARGET_FRAME_RATE); // EstimateFrameRateInterval();
  //   double desiredTargetFps = MAX(1, MIN(inputDataFps, TARGET_FRAME_RATE));

  //   const double timesliceToUseForScreenUpdates = 1500000;
  //   const double tooMuchToUpdateUsecs = timesliceToUseForScreenUpdates / desiredTargetFps; // If updating the current and new frame takes too many frames worth of allotted time, drop to interlacing.

  //   int numChangedPixels = framebufferHasNewChangedPixels ? CountNumChangedPixels(framebuffer[0], framebuffer[1]) : 0;

  //   uint32_t bytesToSend = numChangedPixels * SPI_BYTESPERPIXEL + (DISPLAY_DRAWABLE_HEIGHT << 1);
  //   interlacedUpdate = ((bytesToSend + spiTaskMemory->spiBytesQueued) * spiUsecsPerByte > tooMuchToUpdateUsecs); // Decide whether to do interlacedUpdate - only updates half of the screen

  //   if (interlacedUpdate)
  //     frameParity = 1 - frameParity; // Swap even-odd fields every second time we do an interlaced update (progressive updates ignore field order)
  //   int bytesTransferred = 0;
  //   Span *head = 0;

  //   // Collect all spans in this image
  //   if (framebufferHasNewChangedPixels || prevFrameWasInterlacedUpdate)
  //   {
  //     // If possible, utilize a faster 4-wide pixel diffing method
  //     // if (DISPLAY_WIDTH % 4 == 0 && gpuFramebufferScanlineStrideBytes % 8 == 0)
  //     DiffFramebuffersToScanlineSpansFastAndCoarse4Wide(framebuffer[0], framebuffer[1], interlacedUpdate, frameParity, head);
  //     // else
  //     //   DiffFramebuffersToScanlineSpansExact(framebuffer[0], framebuffer[1], interlacedUpdate, frameParity, head); // If disabled, or framebuffer width is not compatible, use the exact method
  //   }

  //   // Merge spans together on adjacent scanlines - works only if doing a progressive update
  //   if (!interlacedUpdate)
  //     MergeScanlineSpanList(head);

  //   // Submit spans
  //   for (Span *i = head; i; i = i->next)
  //   {
  //     // Update the write cursor if needed
  //     if (spiY != i->y)
  //     {
  //       QUEUE_MOVE_CURSOR_TASK(DISPLAY_SET_CURSOR_Y, displayYOffset + i->y);
  //       IN_SINGLE_THREADED_MODE_RUN_TASK();
  //       spiY = i->y;
  //     }

  //     if (i->endY > i->y + 1 && (spiX != i->x || spiEndX != i->endX)) // Multiline span?
  //     {
  //       QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x, displayXOffset + i->endX - 1);
  //       IN_SINGLE_THREADED_MODE_RUN_TASK();
  //       spiX = i->x;
  //       spiEndX = i->endX;
  //     }
  //     else // Singleline span
  //     {
  //       if (spiEndX < i->endX) // Need to push the X end window?
  //       {
  //         // We are doing a single line span and need to increase the X window. If possible,
  //         // peek ahead to cater to the next multiline span update if that will be compatible.
  //         int nextEndX = DISPLAY_WIDTH;
  //         for (Span *j = i->next; j; j = j->next)
  //           if (j->endY > j->y + 1)
  //           {
  //             if (j->endX >= i->endX)
  //               nextEndX = j->endX;
  //             break;
  //           }
  //         QUEUE_SET_WRITE_WINDOW_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x, displayXOffset + nextEndX - 1);
  //         IN_SINGLE_THREADED_MODE_RUN_TASK();
  //         spiX = i->x;
  //         spiEndX = nextEndX;
  //       }
  //       else if (spiX != i->x)
  //       {
  //         QUEUE_MOVE_CURSOR_TASK(DISPLAY_SET_CURSOR_X, displayXOffset + i->x);
  //         IN_SINGLE_THREADED_MODE_RUN_TASK();
  //         spiX = i->x;
  //       }
  //     }

  //     // Submit the span pixels
  //     SPITask *task = AllocTask(i->size * SPI_BYTESPERPIXEL);
  //     task->cmd = DISPLAY_WRITE_PIXELS;

  //     bytesTransferred += task->PayloadSize() + 1;
  //     uint16_t *scanline = framebuffer[0] + i->y * (gpuFramebufferScanlineStrideBytes >> 1);
  //     uint16_t *prevScanline = framebuffer[1] + i->y * (gpuFramebufferScanlineStrideBytes >> 1);

  //     uint16_t *data = (uint16_t *)task->data;
  //     for (int y = i->y; y < i->endY; ++y, scanline += gpuFramebufferScanlineStrideBytes >> 1, prevScanline += gpuFramebufferScanlineStrideBytes >> 1)
  //     {
  //       int endX = (y + 1 == i->endY) ? i->lastScanEndX : i->endX;
  //       int x = i->x;
  //       while (x < endX && (x & 1))
  //         *data++ = __builtin_bswap16(scanline[x++]);
  //       while (x < (endX & ~1U))
  //       {
  //         uint32_t u = *(uint32_t *)(scanline + x);
  //         *(uint32_t *)data = ((u & 0xFF00FF00U) >> 8) | ((u & 0x00FF00FFU) << 8);
  //         data += 2;
  //         x += 2;
  //       }
  //       while (x < endX)
  //         *data++ = __builtin_bswap16(scanline[x++]);
  //       memcpy(prevScanline + i->x, scanline + i->x, (endX - i->x) * 2);
  //     }
  //     CommitTask(task);
  //     IN_SINGLE_THREADED_MODE_RUN_TASK();
  //   }

  //   // Remember where in the command queue this frame ends, to keep track of the SPI thread's progress over it
  //   if (bytesTransferred > 0)
  //   {
  //     prevFrameEnd = curFrameEnd;
  //     curFrameEnd = spiTaskMemory->queueTail;
  //   }
  // }

  // DeinitGPU();
  DeinitSPI();
  CloseMailbox();
  printf("Quit.\n");
}
