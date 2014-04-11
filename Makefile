MCU = atmega88
#MCU = atmega88

ifeq ($(MCU),atmega8)
AVRDUDE_MCU=m8
endif
ifeq ($(MCU),atmega88)
AVRDUDE_MCU=m88
endif

DATE=$(shell date +"%Y%m%d")

#AVRDUDE_PROG=dapa
AVRDUDE_PROG=jtag2isp -P usb

TARGET = moo-cdc.elf
CC = avr-gcc

## Options common to compile, link and assembly rules
COMMON = -mmcu=$(MCU)

## Compile options common for all C compilation units.
CFLAGS = $(COMMON)
CFLAGS += -Wall -gdwarf-2 -std=gnu99     -DF_CPU=16000000UL -Os -fsigned-char
CFLAGS += -MD -MP -MT $(*F).o

## Assembly specific flags
ASMFLAGS = $(COMMON)
ASMFLAGS += $(CFLAGS)
ASMFLAGS += -x assembler-with-cpp -Wa,-gdwarf2

## Linker flags
LDFLAGS = $(COMMON)
LDFLAGS += -Wl,-Map=moo-cdc.map -Wl,--section-start=.rxbuf=0x800200 -Wl,--cref


## Intel Hex file production flags
HEX_FLASH_FLAGS = -R .eeprom -R .rxbuf

HEX_EEPROM_FLAGS = -j .eeprom
HEX_EEPROM_FLAGS += --set-section-flags=.eeprom="alloc,load"
HEX_EEPROM_FLAGS += --change-section-lma .eeprom=0 --no-change-warnings

## Include Directories
INCLUDES = -I"usbdrv/" -I"./" 

## Objects that must be built in order to link
OBJECTS = usbdrv.o usbdrvasm.o oddebug.o main.o rx_int.o # ass.o

## Objects explicitly added by the user
LINKONLYOBJECTS = 

## Build
all: $(TARGET) moo-cdc.hex moo-cdc.eep moo-cdc.lss size

## Compile
usbdrvasm.o: usbdrv/usbdrvasm.S
	$(CC) $(INCLUDES) $(ASMFLAGS) -c  $<

usbdrv.o: usbdrv/usbdrv.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

oddebug.o: usbdrv/oddebug.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

main.o: main.c
	$(CC) $(INCLUDES) $(CFLAGS) -c  $<

rx_int.o: rx_int.S
	$(CC) $(INCLUDES) $(ASMFLAGS) -c $<

##Link
$(TARGET): $(OBJECTS)
	 $(CC) $(LDFLAGS) $(OBJECTS) $(LINKONLYOBJECTS) $(LIBDIRS) $(LIBS) -o $(TARGET)

%.hex: $(TARGET)
	avr-objcopy -O ihex $(HEX_FLASH_FLAGS)  $< $@

%.eep: $(TARGET)
	-avr-objcopy $(HEX_EEPROM_FLAGS) -O ihex $< $@ || exit 0

%.lss: $(TARGET)
	avr-objdump -h -S $< > $@

size: ${TARGET}
	@echo
	@avr-size ${TARGET}

upload:
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -D -U flash:w:moo-cdc.hex
upload-noverify:
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -V -D -U flash:w:moo-cdc.hex
erase:
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -e
download:
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -U flash:r:download.hex:i
check:
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU}
term:
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -t
ifeq ($(MCU),atmega8)
fuses:
#	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -U hfuse:w:0xd9:m -U lfuse:w:0xef:m
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -U hfuse:w:0xd9:m -U lfuse:w:0xaf:m # with 2.7V Brown-out
endif
ifeq ($(MCU),atmega88)
fuses:
#	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -U hfuse:w:0xdf:m -U lfuse:w:0xff:m
	avrdude -c ${AVRDUDE_PROG} -p ${AVRDUDE_MCU} -U hfuse:w:0xdd:m -U lfuse:w:0xff:m # with 2.7V Brown-out (BODLEVEL 2:0 = 101)
endif

.PHONY: clean
clean:
	-rm -rf $(OBJECTS) *.d moo-cdc.elf moo-cdc.hex moo-cdc.eep moo-cdc.lss moo-cdc.map
.PHONY: realclean
realclean: clean
	-rm *~
	-rm moo-cdc*.zip

.PHONY: zip
zip: moo-cdc-$(DATE).zip

moo-cdc-$(DATE).zip: Makefile LICENSE README moo-cdc.h usbconfig.h main.c rx_int.S
	zip $@ $+

## Other dependencies
#-include $(shell mkdir dep 2>/dev/null) $(wildcard dep/*)

