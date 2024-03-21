#ifndef KERNEL_MODULE
#include <stdio.h>    // printf, stderr
#include <syslog.h>   // syslog
#include <fcntl.h>    // open, O_RDWR, O_SYNC
#include <sys/mman.h> // mmap, munmap
#include <pthread.h>  // pthread_create
#include <bcm_host.h> // bcm_host_get_peripheral_address, bcm_host_get_peripheral_size, bcm_host_get_sdram_address
#endif

#include "config.h"
#include "spi.h"
#include "util.h"

static uint32_t writeCounter = 0;

int mem_fd = -1;
volatile void *bcm2835 = 0;
volatile GPIORegisterFile *gpio = 0;
volatile SPIRegisterFile *spi = 0;

// Errata to BCM2835 behavior: documentation states that the SPI0 DLEN register is only used for DMA. However, even when DMA is not being utilized, setting it from
// a value != 0 or 1 gets rid of an excess idle clock cycle that is present when transmitting each byte. (by default in Polled SPI Mode each 8 bits transfer in 9 clocks)
// With DLEN=2 each byte is clocked to the bus in 8 cycles, observed to improve max throughput from 56.8mbps to 63.3mbps (+11.4%, quite close to the theoretical +12.5%)
// https://www.raspberrypi.org/forums/viewtopic.php?f=44&t=181154
#define UNLOCK_FAST_8_CLOCKS_SPI() (spi->dlen = 2)

void WaitForPolledSPITransferToFinish()
{
  uint32_t cs;
  while (!(((cs = spi->cs) ^ BCM2835_SPI0_CS_TA) & (BCM2835_SPI0_CS_DONE | BCM2835_SPI0_CS_TA))) // While TA=1 and DONE=0
    if ((cs & (BCM2835_SPI0_CS_RXR | BCM2835_SPI0_CS_RXF)))
      spi->cs = BCM2835_SPI0_CS_CLEAR_RX | BCM2835_SPI0_CS_TA | DISPLAY_SPI_DRIVE_SETTINGS;

  if ((cs & BCM2835_SPI0_CS_RXD))
    spi->cs = BCM2835_SPI0_CS_CLEAR_RX | BCM2835_SPI0_CS_TA | DISPLAY_SPI_DRIVE_SETTINGS;
}

void sendCmd(uint8_t cmd, uint8_t *payload, uint32_t payloadSize)
{
  WaitForPolledSPITransferToFinish();

  BEGIN_SPI_COMMUNICATION();

  // An SPI transfer to the display always starts with one control (command) byte, followed by N data bytes.
  CLEAR_GPIO(GPIO_TFT_DATA_CONTROL);

  spi->fifo = cmd;
  while (!(spi->cs & (BCM2835_SPI0_CS_RXD | BCM2835_SPI0_CS_DONE))) /*nop*/
    ;

  if (payloadSize > 0)
  {
    SET_GPIO(GPIO_TFT_DATA_CONTROL);

    uint8_t *tStart = payload;
    uint8_t *tEnd = payload + payloadSize;
    uint8_t *tPrefillEnd = tStart + MIN(15, payloadSize);

    while (tStart < tPrefillEnd)
      spi->fifo = *tStart++;
    while (tStart < tEnd)
    {
      uint32_t cs = spi->cs;
      if ((cs & BCM2835_SPI0_CS_TXD))
        spi->fifo = *tStart++;
      if ((cs & (BCM2835_SPI0_CS_RXR | BCM2835_SPI0_CS_RXF)))
        spi->cs = BCM2835_SPI0_CS_CLEAR_RX | BCM2835_SPI0_CS_TA | DISPLAY_SPI_DRIVE_SETTINGS;
    }
  }

  END_SPI_COMMUNICATION();
}

void sendCmd(uint8_t cmd)
{
  sendCmd(cmd, nullptr, 0);
}

void sendCmd(uint8_t cmd, uint8_t data)
{
  sendCmd(cmd, &data, 1);
}

int InitSPI()
{
  // Memory map GPIO and SPI peripherals for direct access
  mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (mem_fd < 0)
    FATAL_ERROR("can't open /dev/mem (run as sudo)");
  printf("bcm_host_get_peripheral_address: %p, bcm_host_get_peripheral_size: %u, bcm_host_get_sdram_address: %p\n", bcm_host_get_peripheral_address(), bcm_host_get_peripheral_size(), bcm_host_get_sdram_address());
  bcm2835 = mmap(NULL, bcm_host_get_peripheral_size(), (PROT_READ | PROT_WRITE), MAP_SHARED, mem_fd, bcm_host_get_peripheral_address());
  if (bcm2835 == MAP_FAILED)
    FATAL_ERROR("mapping /dev/mem failed");
  spi = (volatile SPIRegisterFile *)((uintptr_t)bcm2835 + BCM2835_SPI0_BASE);
  gpio = (volatile GPIORegisterFile *)((uintptr_t)bcm2835 + BCM2835_GPIO_BASE);
  // systemTimerRegister = (volatile uint64_t *)((uintptr_t)bcm2835 + BCM2835_TIMER_BASE + 0x04); // Generates an unaligned 64-bit pointer, but seems to be fine.

  // By default all GPIO pins are in input mode (0x00), initialize them for SPI and GPIO writes
  // #ifdef GPIO_TFT_DATA_CONTROL
  SET_GPIO_MODE(GPIO_TFT_DATA_CONTROL, 0x01); // Data/Control pin to output (0x01)
  SET_GPIO_MODE(GPIO_SPI0_MOSI, 0x04);
  SET_GPIO_MODE(GPIO_SPI0_CLK, 0x04);

  // The Adafruit 1.65" 240x240 ST7789 based display is unique compared to others that it does want to see the Chip Select line go
  // low and high to start a new command. For that display we let hardware SPI toggle the CS line, and actually run TA<-0 and TA<-1
  // transitions to let the CS line live. For most other displays, we just set CS line always enabled for the display throughout
  // fbcp-ili9341 lifetime, which is a tiny bit faster.
  // SET_GPIO_MODE(GPIO_SPI0_CE0, 0x04);

  spi->cs = BCM2835_SPI0_CS_CLEAR | DISPLAY_SPI_DRIVE_SETTINGS; // Initialize the Control and Status register to defaults: CS=0 (Chip Select), CPHA=0 (Clock Phase), CPOL=0 (Clock Polarity), CSPOL=0 (Chip Select Polarity), TA=0 (Transfer not active), and reset TX and RX queues.
  spi->clk = SPI_BUS_CLOCK_DIVISOR;                             // Clock Divider determines SPI bus speed, resulting speed=256MHz/clk

  // Enable fast 8 clocks per byte transfer mode, instead of slower 9 clocks per byte.
  UNLOCK_FAST_8_CLOCKS_SPI();

  printf("Initializing display\n");
  InitSPIDisplay();

  LOG("InitSPI done");
  return 0;
}

void DeinitSPI()
{
  spi->cs = BCM2835_SPI0_CS_CLEAR | DISPLAY_SPI_DRIVE_SETTINGS;

  SET_GPIO_MODE(GPIO_TFT_DATA_CONTROL, 0);

  SET_GPIO_MODE(GPIO_SPI0_CE1, 0);
  SET_GPIO_MODE(GPIO_SPI0_CE0, 0);
  SET_GPIO_MODE(GPIO_SPI0_MISO, 0);
  SET_GPIO_MODE(GPIO_SPI0_MOSI, 0);
  SET_GPIO_MODE(GPIO_SPI0_CLK, 0);

  if (bcm2835)
  {
    munmap((void *)bcm2835, bcm_host_get_peripheral_size());
    bcm2835 = 0;
  }

  if (mem_fd >= 0)
  {
    close(mem_fd);
    mem_fd = -1;
  }
}
