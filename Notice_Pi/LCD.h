#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <wiringPiI2C.h>
#include <wiringPi.h>
#include "LCD.h"



void *lcdThread(void *arg) {
    
    if (wiringPiSetup() == -1) {
        fprintf(stderr, "WiringPi 초기화 실패\n");
        exit(1);
    }
    
    int fd = wiringPiI2CSetup(I2C_ADDR);
    if (fd == -1) {
        fprintf(stderr, "I2C 초기화 실패\n");
        exit(1);
    }

    char *string = (char*) arg;

    lcd_init(fd);
    ClearLcd(fd);
    printf("출력할 문자열 : %s\n", string);
    lcdStart(LINE1, fd, string);

    printf("lcd 출력 완료\n");

    return NULL;
}

void lcd_byte(int bits, int mode, int fd) {     
    // bits = 전송할 데이터
    // mode = 1 데이터, mode = 0 명령
    int bits_high;
    int bits_low;

    // uses the two half byte writes to LCD
    bits_high = mode | (bits & 0xF0) | LCD_BACKLIGHT;
    bits_low = mode | ((bits << 4) & 0xF0) | LCD_BACKLIGHT;

    // High bits
    wiringPiI2CReadReg8(fd, bits_high);
    lcd_toggle_enable(bits_high, fd);

    // Low bits
    wiringPiI2CReadReg8(fd, bits_low);
    lcd_toggle_enable(bits_low, fd);
}

void typeln(const char* s,int fd) {
    // s = 출력할 문자열
    while (*s) {
        lcd_byte(*(s++), LCD_CHR,fd);
    }
}

void typeChar(char val, int fd) {
    // val = 출력할 문자
    lcd_byte(val, LCD_CHR,fd);
}

void typeInt(int i, int fd) {
    // i = 출력할 정수
    char array1[20];
    sprintf(array1, "%d", i);
    typeln(array1,fd);
}

void typeFloat(float myFloat,int fd) {
    // myFloat = 출력할 소수
    char buffer[20];
    sprintf(buffer, "%4.2f", myFloat);
    typeln(buffer,fd);
}

void lcd_toggle_enable(int bits, int fd) {
    // bits = Enable 핀을 토글하기 위한 비트
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits | ENABLE));
    delayMicroseconds(500);
    wiringPiI2CReadReg8(fd, (bits & ~ENABLE));
    delayMicroseconds(500);
}

void lcdLoc(int line,int fd) {
    lcd_byte(line, LCD_CMD,fd);
}

void lcd_init(int fd) {
    lcd_byte(0x33, LCD_CMD,fd); // Initialise
    lcd_byte(0x32, LCD_CMD,fd); // Initialise
    lcd_byte(0x06, LCD_CMD,fd); // Cursor move direction
    lcd_byte(0x0C, LCD_CMD,fd); // 0x0F On, Blink Off
    lcd_byte(0x28, LCD_CMD,fd); // Data length, number of lines, font size
    lcd_byte(0x01, LCD_CMD,fd); // Clear display
    delayMicroseconds(500);
}

void ClearLcd(int fd) {
    lcd_byte(0x01, LCD_CMD,fd);
    lcd_byte(0x02, LCD_CMD,fd);
}

void lcdStart(int line, int fd, char* string) {
    // line = LCD의 행 번호
    // string = 출력할 문자열
    lcdLoc(line, fd);
    typeln(string, fd);
}


// int main()
// {
//     pthread_t lcdThreadId;

//     if (pthread_create(&lcdThreadId, NULL, lcdThread, "HII~!") != 0)
//     {
//         fprintf(stderr, "LCD 쓰레드 생성 실패\n");
//     }

//     pthread_join(lcdThreadId, NULL);
//     return 0;
// }