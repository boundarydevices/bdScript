#
# Makefile for bdGraph library and utility programs
#
OBJS = Scale.o Scale16.o SImageData16.o ScaleUpObj.o ScaleYCbCrf1.o ScaleYf1.o SImageDataRLE.o SJpeg.o \
       ScaleHortObj.o ScaleVertObj.o ScaleYCbCrf2.o ScaleYf2.o SImagePositionBottom.o SRle.o \
       ScaleRGB.o ScaleYCbCr.o ScaleY.o SImageDataJpeg.o SImagePosition.o invDCT_JQ.o fft.o fftClean.o \
       ResourceView.o mathLib.o



CC=arm-linux-gcc
LIB = libbdGraph.a

ifndef INSTALL_ROOT
   INSTALL_ROOT = ../../install/arm-linux
endif

ifndef KERNEL_DIR
   KERNEL_DIR = ../../linux-2.4.19
endif

CC_BFLAGS=-c $(IFLAGS) -O2
CCC_FLAGS1=$(CC_BFLAGS) -DX386

ARM_ROOT		?= arm-linux
ARM_AR          ?= $(ARM_ROOT)-ar
ARM_GCC         ?= $(ARM_ROOT)-gcc
ARM_LD          ?= $(ARM_ROOT)-ld
ARM_STRIP       ?= $(ARM_ROOT)-strip
ARM_CCAS        ?= $(ARM_GCC)

ifneq (,$(findstring arm, $(CC)))
   CC=$(ARM_GCC)
   AR=$(ARM_AR)
   LD=$(ARM_LD)
   STRIP=$(ARM_STRIP)
   LIBS=-L./ -L$(INSTALL_ROOT)lib
   IFLAGS= -I$(KERNEL_DIR)/include
   CCAS=$(ARM_CCAS)
#SOFT_FLOAT=-msoft-float
   CCC_FLAGS =  $(CC_BFLAGS) $(SOFT_FLOAT) -Os -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing \
   			-fno-common -Uarm  -pipe -mno-apcs-frame \
            -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=5 -march=armv4 -mshort-load-bytes \
            -fno-exceptions -fomit-frame-pointer -DARM -D__ARM_ARCH_4__

   CCC_CPPFLAGS		= $(CCC_FLAGS) -fno-rtti

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
	gcc -DX86 buildSinTable.cpp -o $@ -fno-exceptions -lm


mathLib.S :
	ln -s ../../boundaryLib/mathLib.S

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

all: $(LIB) buildSinTable mic mic-x86 fftTest fftTest-x86

.PHONY: install install-lib


install-lib:
	cp -f -v $(LIB) $(INSTALL_ROOT)lib/$(LIB)

install:  install-lib

clean:
	rm -f *.o *.a *.map  $(LIB)
