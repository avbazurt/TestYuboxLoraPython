#if defined TARGET_PIGPIO
#include "boards/mcu/board.h"
#include <pigpio.h>
#include <pthread.h>

uint32_t BoardGetRandomSeed(void)
{
	return random();
}

uint8_t BoardGetBatteryLevel(void)
{
	uint8_t batteryLevel = 0;

	//TO BE IMPLEMENTED

	return batteryLevel;
}

void BoardGetUniqueId(uint8_t *id)
{
    // Read serial from device tree and present it as board unique ID
    const char * devtreepath = "/sys/firmware/devicetree/base/serial-number";
    char devString[9];

    /* In order for this file operation to work, there must exist a file called
       /opt/pigpio/access, and it should contain at least one line like this:

       /sys/firmware/devicetree/base/serial-number R
     */

    memset(id, 0, 8);
    int handle = fileOpen((char *)devtreepath, PI_FILE_READ);
    if (handle < 0) {
        return;
    }

    // Skip over the first 8 bytes, all zeroes
    fileSeek(handle, 8, PI_FROM_START);

    fileRead(handle, devString, 8);
    devString[8] = '\0';

    fileClose(handle);

    // Parse 32-bit hexadecimal string, and use it twice
    sscanf(devString, "%02hhx%02hhx%02hhx%02hhx", id, id+1, id+2, id+3);
    memcpy(id+4, id, 4);
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void BoardDisableIrq(void)
{
    pthread_mutex_lock(&mutex);
}

void BoardEnableIrq(void)
{
    pthread_mutex_unlock(&mutex);
}

// Arduino API compatibility functions

void pinMode(uint8_t pin, uint8_t mode)
{
    unsigned pimode = 0;
    if (mode == INPUT) pimode = PI_INPUT;
    if (mode == OUTPUT) pimode = PI_OUTPUT;
    gpioSetMode(pin, pimode);
}

int digitalRead(uint8_t pin)
{
    return gpioRead(pin);
}

void digitalWrite(uint8_t pin, uint8_t val)
{
    gpioWrite(pin, val);
}

void delay(uint32_t ms)
{
    int seconds = ms / 1000;
    int micros = (ms % 1000) * 1000;
    gpioSleep(PI_TIME_RELATIVE, seconds, micros);
}

unsigned long millis(void)
{
    // gpioTick() cannot be used here, since it returns a uint32_t value of
    // microseconds, which wraps around too soon to be useful. Dividing by
    // 1000 its result would result in a wraparound at 4294967, way below
    // 2^32, which will throw off calculations at the wraparound time.
    int sec = 0; int usec = 0;
    gpioTime(PI_TIME_ABSOLUTE, &sec, &usec); // PI_TIME_ABSOLUTE saves one subtraction
    return (unsigned long)(usec / 1000) + ((unsigned long)sec * 1000);
}

// This exists only to strip extra parameters from pigpio-style IRQ handler.
// There is exactly one IRQ handler used by the code, so a single pointer
// should do.
static void (* _arduinoIrq)(void) = NULL;
static void _arduinoIrqBridge(int, int, uint32_t) { _arduinoIrq(); }

void attachInterrupt(uint8_t pin, void (* irqHandler)(void), int mode)
{
    unsigned edge = 0;
    if (mode == RISING) edge = RISING_EDGE;
    if (mode == FALLING) edge = FALLING_EDGE;
    if (mode == CHANGE) edge = EITHER_EDGE;
    _arduinoIrq = irqHandler;
    int r = gpioSetISRFunc(pin, edge, -1, _arduinoIrqBridge);
    if (r != 0) {
        _arduinoIrq = NULL;
        fprintf(stderr, "attachInterrupt gpioSetISRFunc() fail: %d\n", r);
    }
}

void detachInterrupt(uint8_t pin)
{
    int r = gpioSetISRFunc(pin, RISING_EDGE, -1, NULL);
    _arduinoIrq = NULL;
    if (r != 0) {
        fprintf(stderr, "detachInterrupt gpioSetISRFunc() fail: %d\n", r);
    }
}


#endif