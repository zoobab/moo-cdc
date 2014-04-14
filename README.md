Installation
============

You need to copy or link the usbdrv directory from V-USB package here.
V-USB can be downloaded from http://www.obdev.at/products/avrusb/.
Tested to work with vusb-20090822.

Specify your MCU in Makefile (Atmega8/Atmega88) and define LED in moo-cdc.h
before building.

System driver considerations can be found at http://www.recursion.jp/avrcdc/.
moo-cdc should be compatible with any driver mentioned there although some
solutions will prevent it from running with high speeds. 115200 baud works
with Linux and the inf-only driver for Windows XP.

Have fun!
S.

Links
=====

Elektroda Polish forum:
* http://www.elektroda.pl/rtvforum/topic1560638.html

Original Moo-CDC webpage on archive.org:
* https://web.archive.org/web/20120311023321/http://www.asio.pl/moo-cdc/

Toolchain
=========

I had to install an old version of avr-gcc (the one shipped with Debian
Squeeze) in order to avoid those errors:

    avr-gcc -I"usbdrv/" -I"./"  -mmcu=atmega88 -Wall -gdwarf-2 -std=gnu99     -DF_CPU=16000000UL -Os -fsigned-char -MD -MP -MT usbdrv.o -c  usbdrv/usbdrv.c
    In file included from usbdrv/usbdrv.c:12:0:
    usbdrv/usbdrv.h:455:6: error: variable ‘usbDescriptorDevice’ must be const in order to be put into read-only section by means of ‘__attribute__((progmem))’
     char usbDescriptorDevice[];
          ^
    usbdrv/usbdrv.h:461:6: error: variable ‘usbDescriptorConfiguration’ must be const in order to be put into read-only section by means of ‘__attribute__((progmem))’
     char usbDescriptorConfiguration[];
    
    [...]
