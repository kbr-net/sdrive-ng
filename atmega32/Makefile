###############################################################################
# Makefile for the project SDrive
###############################################################################

## General Flags
PROJECT = SDrive
MCU = atmega32
TARGET = SDrive.elf
CC = avr-gcc
CRCTOOL = ../crc-gen/crcgen

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
//CFLAGS += -Wall -w -Wtraditional -Wshadow -pedantic -Wa,-gdwarf2     -DF_CPU=14318180UL -Os -funsigned-char -fpack-struct
CFLAGS += -Wall -Wpadded -Wshadow -Wa,-gdwarf2     -DF_CPU=14318180UL -Os -funsigned-char -fpack-struct
#CFLAGS += -Wall -Wa,-gdwarf2     -DF_CPU=14318180UL -Os -funsigned-char -fpack-struct
CFLAGS += -MD -MP -MT $(*F).o -MF dep/$(@F).d 

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS +=  -Wl,-Map=SDrive.map


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

BIN_FLASH_FLAGS = --change-section-address .eeprom=$(FLASHSIZE) --gap-fill 0xff

DATE = $(shell date +%Y%m%d)
FORMAT = binary
DEVID = 0x474e4453
#SWVERSIONMAJOR = 1	moved to SDrive.c
#SWVERSIONMINOR = 2
#BOOTLDRSIZE = 0x800
BOOTLDRSIZE = 0x1000
#FLASHSIZE - BOOTLDRSIZE
FLASHSIZE = 0x7000
#FLASHSIZE - BOOTLDRSIZE + EEPROMSIZE - 8
BOOTLDRINFOSTART = 0x73F8

CDEFS += -DDATE=$(DATE)
CDEFS += -DDEVID=$(DEVID)
#CDEFS += -DSWVERSIONMAJOR=$(SWVERSIONMAJOR)
#CDEFS += -DSWVERSIONMINOR=$(SWVERSIONMINOR)
CFLAGS += $(CDEFS)

LDFLAGS	+= -Wl,--section-start=.bootldrinfo=$(BOOTLDRINFOSTART)

## Include Directories
#INCLUDES = -I"C:\My\BOB\Sdrive\AVRSDrive\..\..\..\..\AVR\WinAVR\avr\include" -I"C:\My\BOB\Sdrive\AVRSDrive\..\..\..\..\AVR\WinAVR\bin" 

## Objects that must be built in order to link
#OBJECTS = SDrive.o spi.o mmc.o fat.o delay100us.o timer.o
OBJECTS = SDrive.o spi.o mmc.o fat.o timer.o usart.o

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) SDrive.hex SDrive.eep SDrive.lss SDrive.bin size

## Compile
#delay100us.o: ../delay100us.s
#	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

SDrive.o: ../SDrive.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

spi.o: ../spi.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

mmc.o: ../mmc.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

fat.o: ../fat.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

timer.o: ../timer.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

usart.o: ../usart.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

%.bin: $(TARGET)
	avr-objcopy -O $(FORMAT) $(BIN_FLASH_FLAGS) $< $@
	$(CRCTOOL) $@

size: ${TARGET}
	@echo
	@avr-size -C --mcu=${MCU} ${TARGET}

## Clean target
.PHONY: clean
clean:
	-rm -rf $(OBJECTS) SDrive.elf dep/* SDrive.hex SDrive.eep SDrive.lss SDrive.map SDrive.bin


## Other dependencies
-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

dist:
	tar cvzf sdrive-ng-$(MCU)-bin.tgz SDrive.hex SDrive.bin SDrive.eep README
