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


#define I2C_ADDR   0x27     // I2C device address
#define LCD_CHR  1          // Sending data mode
#define LCD_CMD  0          // Sending command mode
#define LINE1  0x80         
#define LINE2  0xC0         

#define LCD_BACKLIGHT   0x08    // on -> 0x08    off -> 0x00
#define ENABLE  0b00000100      // Enable bit

void lcd_byte(int bits, int mode, int fd);          // I2C를 통해 데이터를 LCD에 전송
void typeln(const char* s, int fd);                 // 문자열을 한 글자씩 lcd_byte 함수를 통해 LCD에 출력
void typeChar(char val, int fd);                    // 한 문자를 lcd_byte 함수를 통해 LCD에 출력
void typeInt(int i, int fd);                        // 정수를 문자열로 변환해 typeln 함수를 호출하여 LCD에 출력
void typeFloat(float myFloat, int fd);              // 소수를 문자열로 변환해 typeln 함수를 호출하여 LCD에 출력

void lcd_toggle_enable(int bits, int fd);           // I2C 통신을 통해 LCD의 Enable 핀을 토글
void lcdLoc(int line, int fd);                      // LCD 커서의 위치 설정
void lcd_init(int fd);                              // LCD를 초기화하는 몇 가지 초기화 명령을 전송
void ClearLcd(int fd);                              // lcd 초기화
void lcdStart(int line, int fd, char* string);      // 지정된 LCD 행에 문자열을 출력


int main(){

    int fd;

    if (wiringPiSetup() == -1) {
        exit(1);
    }
    
    fd = wiringPiI2CSetup(I2C_ADDR);
    if (fd == -1){
        fprintf(stderr, "I2C 초기화 실패\n");
        exit(1);
    }

    lcd_init(fd); // init LCD
    usleep(1000*1000);

    ClearLcd(fd);
    lcdStart(LINE1, fd, "Hello"); 
    printf("lcd 출력 완료\n");

    return 0;
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
    lcd_toggle_enable(bits_high,fd);

    // Low bits
    wiringPiI2CReadReg8(fd, bits_low);
    lcd_toggle_enable(bits_low,fd);
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
    lcdLoc(line,fd);
    typeln(string,fd);
}
