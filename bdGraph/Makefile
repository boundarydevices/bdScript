#
# Makefile for bdGraph library and utility programs
#
OBJS = Scale.o Scale16.o SImageData16.o ScaleUpObj.o ScaleYCbCrf1.o ScaleYf1.o SImageDataRLE.o SJpeg.o \
       ScaleHortObj.o ScaleVertObj.o ScaleYCbCrf2.o ScaleYf2.o SImagePositionBottom.o SRle.o \
       ScaleRGB.o ScaleYCbCr.o ScaleY.o SImageDataJpeg.o SImagePosition.o invDCT_JQ.o fft.o fftClean.o



CC=arm-linux-gcc
LIB = libbdGraph.a

CC_BFLAGS=-D_REENTRANT=1 -c -DXP_UNIX=1 $(IFLAGS) -O2
CCC_FLAGS1=$(CC_BFLAGS) -DX386
ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   LD=arm-linux-ld
   STRIP=arm-linux-strip
   LIBS=-L./ -L../../install/arm-linux/lib
   IFLAGS= -I../../linux-2.4.19/include
   CCAS=arm-linux-gcc
   CCC_FLAGS=$(CC_BFLAGS) -mcpu=strongarm -DARM
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
	$(CC) $(CCC_FLAGS) $<

$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)

buildSinTable: buildSinTable.cpp fft.h
	gcc -DX86 buildSinTable.cpp -o $@ -fno-exceptions -lm


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
	cp -f -v $(LIB) ../../install/arm-linux/lib/$(LIB)

install:  install-lib

clean:
	rm -f *.o *.a *.map  $(LIB)
