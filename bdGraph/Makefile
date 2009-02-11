#
# Makefile for bdGraph library and utility programs
#
OBJS = Scale.o Scale18.o Scale16.o SImageData16.o ScaleUpObj.o ScaleYCbCrf1.o ScaleYf1.o SImageDataRLE.o SJpeg.o \
       ScaleHortObj.o ScaleVertObj.o ScaleYCbCrf2.o ScaleYf2.o SImagePositionBottom.o SRle.o \
       ScaleRGB.o ScaleYCbCr.o ScaleY.o SImageDataJpeg.o SImagePosition.o invDCT_JQ.o fft.o fftClean.o \
       ResourceView.o mathLib.o

CROSS_COMPILE1 ?= $(CROSS_COMPILE)
ARM_GCC         ?= $(CROSS_COMPILE1)gcc
ARM_AR          ?= $(CROSS_COMPILE1)ar
ARM_GCC         ?= $(CROSS_COMPILE1)gcc
ARM_LD          ?= $(CROSS_COMPILE1)ld
ARM_STRIP       ?= $(CROSS_COMPILE1)strip
ARM_CCAS        ?= $(CROSS_COMPILE1)gcc

cc-option = $(shell if $(ARM_GCC) $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi ;)

MALIGN := $(call cc-option,-malignment-traps,)
FLOAT_TYPE=-msoft-float


CC=$(ARM_GCC)
CXX=g++
LIB = libbdGraph.a

ifndef INSTALL_ROOT
   INSTALL_ROOT = ../../install/arm-linux
endif

ifndef KERNEL_DIR
   KERNEL_DIR = ../../linux-2.4.19
endif

CC_BFLAGS=-c $(IFLAGS) -O2
CCC_FLAGS1=$(CC_BFLAGS) -DX386


ifneq (,$(findstring arm, $(CC)))
   CC=$(ARM_GCC)
   AR=$(ARM_AR)
   LD=$(ARM_LD)
   STRIP=$(ARM_STRIP)
   LIBS=-L./ -L$(INSTALL_ROOT)lib
   IFLAGS= -I$(KERNEL_DIR)/include
   CCAS=$(ARM_CCAS)
   _CFLAGS =  $(CFLAGS) $(CC_BFLAGS) $(FLOAT_TYPE) -Wno-trigraphs -fno-strict-aliasing \
   			-fno-common -Uarm  -pipe -mno-apcs-frame \
            -mno-sched-prolog -D__LINUX_ARM_ARCH__=5  $(MALIGN) \
            -fno-exceptions -fomit-frame-pointer -DARM -D__ARM_ARCH_4__

   CCC_FLAGS = -Wstrict-prototypes $(_CFLAGS) 
   CCC_CPPFLAGS	      =  -fno-rtti $(_CFLAGS) 

else
   CCAS = gcc
   CC=g++
   AR=ar
   LD=ld
   LIBS=-L./
   IFLAGS=-I$(INSTALL_ROOT)/include/g++-3
   STRIP=strip
   CCC_FLAGS=$(CC_BFLAGS) -DX86
endif

CCASCOMPILE = $(CCAS) $(AM_CCASFLAGS) $(CCASFLAGS)

.S.o:
	$(CCASCOMPILE) -c `test -f '$<' || echo '$(srcdir)/'`$<

%.o : %.cpp Makefile
	$(CC) $(CCC_CPPFLAGS) $<


$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)

buildSinTable: buildSinTable.cpp fft.h
	$(CXX) -DX86 buildSinTable.cpp -o $@ -fno-exceptions -lm

sinTable8.c : buildSinTable
	./buildSinTable

sinTable8r.c : buildSinTable
	./buildSinTable

fft-x86.o: fft.c fft.h sinTable8.c sinTable8r.c
	gcc $(CCC_FLAGS1) -fno-exceptions fft.c -o $@

fft.o: fft.c fft.h sinTable8.c sinTable8r.c
	$(CC) $(CCC_FLAGS) -fno-exceptions fft.c -o $@

fftClean-x86.o: fftClean.c fftClean.h fft.h
	gcc $(CCC_FLAGS1) -fno-exceptions fftClean.c -o $@

fftClean.o: fftClean.c fftClean.h fft.h
	$(CC) $(CCC_FLAGS) -fno-exceptions fftClean.c -o $@

mic-x86.o: mic.c fft.h fftClean.h
	gcc $(CCC_FLAGS1) -fno-exceptions mic.c -o $@

mic-x86: mic-x86.o fft-x86.o fftClean-x86.o
	gcc mic-x86.o fft-x86.o fftClean-x86.o -o $@

mic.o: mic.c fft.h fftClean.h
	$(CC) $(CCC_FLAGS) -fno-exceptions mic.c -o $@

mic: mic.o fft.o fftClean.o
	$(CC) mic.o fftClean.o fft.o -o $@

fftTest-x86.o: fftTest.c fft.h
	gcc $(CCC_FLAGS1) -fno-exceptions fftTest.c -o $@

fftTest-x86: fftTest-x86.o fft-x86.o
	gcc fftTest-x86.o fft-x86.o -o $@

fftTest.o: fftTest.c fft.h
	$(CC) $(CCC_FLAGS) -fno-exceptions fftTest.c -o $@

fftTest: fftTest.o fft.o
	$(CC) fftTest.o fft.o -o $@

fftFloat: fftFloat.c
	gcc $@.c -fno-exceptions -O2 -lm -o $@

all: $(LIB) buildSinTable fftTest fftTest-x86

.PHONY: install install-lib

install-lib:
	cp -f -v $(LIB) $(INSTALL_ROOT)/lib/$(LIB)

$(INSTALL_ROOT)/include/bdGraph: $(INSTALL_ROOT)
	mkdir -p $@

install:  install-lib $(INSTALL_ROOT)/include/bdGraph $(wildcard *.h)
	cp -fv $(wildcard *.h) $(INSTALL_ROOT)/include/bdGraph

clean:
	rm -f *.o *.a *.map  $(LIB)
