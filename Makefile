CC = gcc
LD = gcc
CFLAGS = -O2 -Wall -DTARGET_PIGPIO -Isrc -Isrc/boards/mcu/pigpio 
# -DLIB_DEBUG=1
CPPFLAGS = $(CFLAGS)
LDFLAGS = -L.

all: lorawan-rpi lorawan-helloworld

lorawan-rpi: lorawan-rpi.o libSX126x-Arduino.a
	$(LD) lorawan-rpi.o -o $@ $(LDFLAGS) -lSX126x-Arduino -lpigpio -lm -lstdc++

lorawan-helloworld: lorawan-helloworld.o libSX126x-Arduino.a
	$(LD) lorawan-helloworld.o -o $@ $(LDFLAGS) -lSX126x-Arduino -lpigpio -lm

lorawan-rpi.o: lorawan-rpi.cpp

lorawan-helloworld.o: lorawan-helloworld.cpp

LIBOBJS := $(patsubst %.cpp,%.o,$(wildcard src/mac/*.cpp src/mac/region/*.cpp src/radio/sx126x/*.cpp src/boards/sx126x/*.cpp src/boards/mcu/*.cpp src/system/*.cpp src/system/crypto/*.cpp src/boards/mcu/pigpio/*.cpp ))

libSX126x-Arduino.a: $(LIBOBJS)
	$(AR) rv $@ $^

clean:
	rm -f *.o *.a lorawan-rpi
	rm -f $(LIBOBJS)
