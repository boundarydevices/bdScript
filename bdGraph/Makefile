#
# Makefile for bdGraph library and utility programs
#
OBJS = Scale.o Scale16.o SImageData16.o ScaleUpObj.o ScaleYCbCrf1.o ScaleYf1.o SImageDataRLE.o SJpeg.o \
       ScaleHortObj.o ScaleVertObj.o ScaleYCbCrf2.o ScaleYf2.o SImagePositionBottom.o SRle.o \
       ScaleRGB.o ScaleYCbCr.o ScaleY.o SImageDataJpeg.o SImagePosition.o invDCT.o invDCT_JQ.o



CC=arm-linux-gcc
LIB = libbdGraph.a

ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   STRIP=arm-linux-strip
   LIBS=-L./ -L../../install/arm-linux/lib
   IFLAGS= -I../../linux-2.4.19/include
   CCAS=arm-linux-gcc
else
   CCAS = gcc
   CC=g++
   AR=ar
   LIBS=-L./
   IFLAGS=-I$(INSTALL_ROOT)/include/g++-3
   STRIP=strip
endif


CCASCOMPILE = $(CCAS) $(AM_CCASFLAGS) $(CCASFLAGS)

.S.o:
	$(CCASCOMPILE) -c `test -f '$<' || echo '$(srcdir)/'`$<

%.o : %.cpp Makefile
	$(CC) -D_REENTRANT=1 -c -DXP_UNIX=1 $(IFLAGS) -mcpu=strongarm -O2 $<

$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)


all: $(LIB)

.PHONY: install install-lib


install-lib:
	cp -f -v $(LIB) ../../install/arm-linux/lib/$(LIB)

install:  install-lib

clean:
	rm -f *.o *.a *.map  $(LIB)
