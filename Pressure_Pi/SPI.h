#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ARRAY_SIZE(array) sizeof(array)/sizeof(array[0])
extern const char *DEVICE;
extern uint8_t MODE;
extern uint8_t BITS;
extern uint32_t CLOCK;
extern uint16_t DELAY;

int prepare(int fd);
uint8_t control_bits_differential(uint8_t channel);
uint8_t control_bits(uint8_t channel);
int readadc(int fd, uint8_t channel);

#endif // spi_h 