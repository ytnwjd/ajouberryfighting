#ifndef __GPIO_H__
#define __GPIO_H__

#define VALUE_MAX   256
#define BUFFER_MAX 3
#define DIRECTION_MAX 45

#define IN  0
#define OUT 1

#define LOW 0
#define HIGH 1

int GPIOExport(int pin);
int GPIODirection(int pin, int dir);
int GPIOWrite(int pin , int value);
int GPIORead(int pin);
int GPIOUnexport(int pin);

#endif