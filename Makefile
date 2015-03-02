
# The name of your project (used to name the compiled .hex file)
TARGET = main

# Path to your arduino installation
ARDUINOPATH ?= ../Arduino_1.6.0_Java_7.app/Contents/Java

# configurable options
OPTIONS = -DF_CPU=96000000 -DUSB_SERIAL_HID -DLAYOUT_US_INTERNATIONAL

# options needed by many Arduino libraries to configure for Teensy 3.0
OPTIONS += -D__MK20DX256__ -DARDUINO=105 -DTEENSYDUINO=120


# Other Makefiles and project templates for Teensy 3.x:
#
# https://github.com/apmorton/teensy-template
# https://github.com/xxxajk/Arduino_Makefile_master
# https://github.com/JonHylands/uCee


#************************************************************************
# Location of Teensyduino utilities, Toolchain, and Arduino Libraries.
# To use this makefile without Arduino, copy the resources from these
# locations and edit the pathnames.  The rest of Arduino is not needed.
#************************************************************************

# path location for Teensy Loader, teensy_post_compile and teensy_reboot
#TOOLSPATH = $(ARDUINOPATH)/hardware/tools   # on Linux
TOOLSPATH = $(ARDUINOPATH)/hardware/tools   # on Mac or Windows

# path location for Arduino libraries (currently not used)
LIBRARYPATH = $(ARDUINOPATH)/libraries

# path location for the arm-none-eabi compiler
COMPILERPATH = $(ARDUINOPATH)/hardware/tools/arm/bin

# path location for Arduino library
TEENSYLIB = $(ARDUINOPATH)/hardware/teensy/avr/cores/teensy3

#************************************************************************
# Settings below this point usually do not need to be edited
#************************************************************************

# CPPFLAGS = compiler options for C and C++
CPPFLAGS = -Wall -g -Os -mcpu=cortex-m4 -mthumb -nostdlib -MMD $(OPTIONS) -I. -I$(TEENSYLIB)

# compiler options for C++ only
CXXFLAGS = -std=gnu++0x -felide-constructors -fno-exceptions -fno-rtti

# compiler options for C only
CFLAGS =

# linker options
LDFLAGS = -Os -Wl,--gc-sections -mcpu=cortex-m4 -mthumb -T$(TEENSYLIB)/mk20dx256.ld

# additional libraries to link
LIBS = -lm
LIBS += -L. -lteensy


# names for the compiler programs
CC = $(abspath $(COMPILERPATH))/arm-none-eabi-gcc
CXX = $(abspath $(COMPILERPATH))/arm-none-eabi-g++
AR = $(abspath $(COMPILERPATH))/arm-none-eabi-ar
OBJCOPY = $(abspath $(COMPILERPATH))/arm-none-eabi-objcopy
SIZE = $(abspath $(COMPILERPATH))/arm-none-eabi-size

# automatically create lists of the sources and objects
# TODO: this does not handle Arduino libraries yet...
C_FILES := $(wildcard *.c)
CPP_FILES := $(wildcard *.cpp)
OBJS := $(C_FILES:.c=.o) $(CPP_FILES:.cpp=.o)

# the actual makefile rules (all .o files built by GNU make's default implicit rules)

all: $(TARGET).hex

$(TARGET).elf: $(OBJS) libteensy.a $(TEENSYLIB)/mk20dx256.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

%.hex: %.elf
	$(SIZE) $<
	$(OBJCOPY) -O ihex -R .eeprom $< $@
	$(abspath $(TOOLSPATH))/teensy_post_compile -file=$(basename $@) -path=$(shell pwd) -tools=$(abspath $(TOOLSPATH))
	-$(abspath $(TOOLSPATH))/teensy_reboot


# compiler generated dependency info
-include $(OBJS:.o=.d)

clean:
	rm -f *.o *.d *.a $(TEENSY_OBJS) $(TARGET).elf $(TARGET).hex

TEENSY_C_FILES := $(wildcard $(TEENSYLIB)/*.c)
TEENSY_CPP_FILES := $(wildcard $(TEENSYLIB)/*.cpp)
TEENSY_OBJS := $(TEENSY_C_FILES:.c=.o) $(TEENSY_CPP_FILES:.cpp=.o)

libteensy.a: $(TEENSY_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# End
