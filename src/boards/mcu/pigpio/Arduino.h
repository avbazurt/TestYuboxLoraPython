#if defined TARGET_PIGPIO

#ifndef _ARDUINO_GLUE_H
#define _ARDUINO_GLUE_H

#include <stdint.h>

#define HIGH 0x1
#define LOW  0x0

#define INPUT  0x01
#define OUTPUT 0x02

void pinMode(uint8_t pin, uint8_t mode);
int digitalRead(uint8_t pin);
void digitalWrite(uint8_t pin, uint8_t val);

//Interrupt Modes
#define RISING    0x01
#define FALLING   0x02
#define CHANGE    0x03
#define ONLOW     0x04
#define ONHIGH    0x05
#define ONLOW_WE  0x0C
#define ONHIGH_WE 0x0D

void attachInterrupt(uint8_t pin, void (*)(void), int mode);
void detachInterrupt(uint8_t pin);

void delay(uint32_t ms);
unsigned long millis(void);

// The pigpio implementation does not use discrete pins for hardware SPI, so
// these four declarations are for compile compatibility only.
const int MISO = -1;
const int MOSI = -1;
const int SS = -1;
const int SCK = -1;

#endif

#endif
