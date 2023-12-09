#include "SPI.h"

const char *DEVICE = "/dev/spidev0.0";
uint8_t MODE = SPI_MODE_0;
uint8_t BITS = 8;
uint32_t CLOCK = 1000000;
uint16_t DELAY = 5;

int prepare(int fd){
    if (ioctl(fd, SPI_IOC_WR_MODE,&MODE)==-1){
        perror("can't set MODE");
        return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS)==-1){
        perror("can't set number of bits");
        return -1; 
    }
    if(ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK)==-1){
        perror("can't set write clock");
        return -1;
    }
    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK)==-1){
        perror("can't set read clock");
        return -1;
    }
    return 0;
}

uint8_t control_bits_differential(uint8_t channel){
    return (channel & 7)<<4;
}

uint8_t control_bits(uint8_t channel){
    return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel){
    uint8_t tx[] = {1, control_bits(channel),0};
    uint8_t rx[3];
    
    struct spi_ioc_transfer tr={
        .tx_buf = (unsigned long)tx, 
        .rx_buf = (unsigned long)rx, 
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word=BITS,
    };
    if(ioctl(fd, SPI_IOC_MESSAGE(1), &tr)==1){
        perror("IO ERROR");
        abort();
    }
    return ((rx[1]<<8) & 0x300 | rx[2] & 0xFF);
}