#
# Makefile for curlCache library and utility programs
#
OBJS = audioQueue.o childProcess.o codeQueue.o curlGet.o \
       ddtoul.o dirByATime.o fbDev.o ftObjs.o hexDump.o \
       hexDump.o imgGIF.o imgPNG.o imgJPEG.o jsJPEG.o \
       jsAlphaMap.o jsBarcode.o jsButton.o jsCamera.o jsCurl.o jsGlobals.o \
       jsHyperlink.o jsImage.o jsMP3.o jsScreen.o jsShell.o \
       jsText.o jsTimer.o jsTouch.o jsTransitions.o jsURL.o jsVolume.o \
       jsGpio.o madDecode.o madHeaders.o memFile.o parsedURL.o \
       relativeURL.o tsThread.o ultoa.o \
       ultodd.o box.o urlFile.o zOrder.o \
       cbmImage.o ccActiveURL.o ccDiskCache.o ccWorker.o semClasses.o \
       popen.o jsPopen.o jsCBM.o jsEnviron.o jsTCP.o jsTTY.o jsUse.o \
       jsFileIO.o jsExit.o mpegDecode.o mpDemux.o videoQueue.o videoFrames.o \
       jsMPEG.o jsFlash.o jsSniffWLAN.o sniffWLAN.o jsMonWLAN.o monitorWLAN.o

CC=arm-linux-gcc
LIBBDGRAPH=bdGraph/libbdGraph.a
LIBRARYREFS=../install/arm-linux/lib/libflash.a \
            ../install/arm-linux/lib/libmpeg2.a \
            ../install/arm-linux/lib/libmad.a 

ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   NM=arm-linux-nm
   STRIP=arm-linux-strip
   OBJCOPY=arm-linux-objcopy
   LIBS=-L./ -L../install/arm-linux/lib
   IFLAGS= \
          -I../linux-2.4.19/include \
          -I../install/arm-linux/include \
          -I../install/arm-linux/include/nspr \
          -I../install/arm-linux/include/freetype2 \
          -I../install/arm-linux/include/libavcodec
   LIB = ../install/arm-linux/lib/libCurlCache.a
else
#   CC=g++
   AR=ar
   NM=nm
   LIBS=-L./
   IFLAGS=-I$(INSTALL_ROOT)/include/g++-3 -I$(INSTALL_ROOT)/include/nspr -I$(INSTALL_ROOT)/include/freetype2
   STRIP=strip
   OBJCOPY=objcopy
   LIB = ./libCurlCache.a
endif

ifneq (y,TSINPUTAPI)
   TSINPUTFLAG=0
else
   TSINPUTFLAG=1
endif

%.o : %.cpp
	$(CC) -fno-rtti -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

#jsImage.o : jsImage.cpp Makefile
#	$(CC) -D_REENTRANT=1 -shared -DXP_UNIX=1 $(IFLAGS) -o jsImage.o -O2 $<

$(LIBBDGRAPH):
	make -C bdGraph all

$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) -D_REENTRANT=1 -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl

curlGet: curlGet.cpp $(LIB) Makefile
	$(CC) -D_REENTRANT=1 -o curlGet -O2 -DSTANDALONE $(IFLAGS) curlGet.cpp $(LIBS) -lCurlCache -lcurl -lstdc++ -lz -lm

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o urlTest.o -O2 -DSTANDALONE urlFile.cpp

urlTest: urlTest.o $(LIB)
	$(CC) -D_REENTRANT=1 -o urlTest urlTest.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lm -lpthread

mp3Play: mp3Play.o $(LIB)
	$(CC) -D_REENTRANT=1 -o mp3Play mp3Play.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lmad
	$(STRIP) mp3Play

testJS: testJS.cpp $(LIB) Makefile
	$(CC) -D_REENTRANT=1 -o testJS testJS.cpp -DXP_UNIX=1 $(IFLAGS) $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lts -lpthread -lm -lz

jsExec: jsExec.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) -D_REENTRANT=1 -o jsExec jsExec.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lvo -lCurlCache -lmpeg2 -lflash -lts -lpthread -lm -lz -lLinuxWLAN
	arm-linux-nm jsExec >jsExec.map

madDecode: madDecode.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) -D_REENTRANT=1 -DSTANDALONE=1 -o madDecode madDecode.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lvo -lCurlCache -lid3tag -lmpeg2 -lflash -lts -lpthread -lm -lz
	arm-linux-nm madDecode >madDecode.map

jpegview: jpegview.o $(LIBBDGRAPH)
	$(CC) $(IFLAGS) -o jpegview jpegview.o -L./bdGraph -lbdGraph

madTest: madTest.o $(LIB)
	$(CC) -D_REENTRANT=1 -o madTest madTest.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lts -lpthread -lm -lz
	arm-linux-nm madTest >madTest.map
	$(STRIP) madTest

ftRender.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o ftRender.o -O2 -D__MODULETEST__ $(IFLAGS) ftObjs.cpp

ftRender: ftRender.o $(LIB)
	$(CC) -D_REENTRANT=1 -o ftRender ftRender.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftRender >ftRender.map
	$(STRIP) ftRender

ftDump.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o ftDump.o -O2 -D__DUMP__ $(IFLAGS) ftObjs.cpp

ftDump: ftDump.o $(LIB)
	$(CC) -D_REENTRANT=1 -o ftDump ftDump.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftDump >ftDump.map
	$(STRIP) ftDump

recordTest: recordTest.o $(LIB)
	$(CC) -D_REENTRANT=1 -o recordTest recordTest.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm recordTest >recordTest.map
	$(STRIP) recordTest

tsThreadMain.o: tsThread.h tsThread.cpp Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o tsThreadMain.o -O2 -D__MODULETEST__ $(IFLAGS) tsThread.cpp

tsThread: tsThreadMain.o Makefile $(LIB)
	$(CC) -D_REENTRANT=1 -o tsThread tsThreadMain.o $(LIBS) -lCurlCache -lstdc++ -lts -lpthread -lm
	arm-linux-nm tsThread >tsThread.map
	$(STRIP) tsThread

madHeadersMain.o: madHeaders.h madHeaders.cpp Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o madHeadersMain.o -O2 -D__STANDALONE__ $(IFLAGS) madHeaders.cpp

madHeaders: madHeadersMain.o Makefile $(LIB)
	$(CC) -D_REENTRANT=1 -o madHeaders madHeadersMain.o $(LIBS) -lCurlCache -lstdc++ -lmad -lid3tag -lm -lz
	arm-linux-nm madHeaders >madHeaders.map
	$(STRIP) madHeaders

imgJPEGMain.o : imgJPEG.cpp Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o imgJPEGMain.o -O2 -D__STANDALONE__ $(IFLAGS) imgJPEG.cpp

imgJPEG : imgJPEGMain.o memFile.o hexDump.o fbDev.o
	$(CC) -D_REENTRANT=1 -o imgJPEG imgJPEGMain.o memFile.o hexDump.o fbDev.o -lstdc++ -ljpeg
	$(STRIP) imgJPEG

imgPNGMain.o : imgPNG.cpp Makefile
	$(CC) -D_REENTRANT=1 -c $(IFLAGS) -o imgPNGMain.o -O2 -D__STANDALONE__ $(IFLAGS) imgPNG.cpp

imgPNG : imgPNGMain.o memFile.o hexDump.o fbDev.o
	$(CC) -D_REENTRANT=1 -o imgPNG imgPNGMain.o memFile.o hexDump.o fbDev.o -lstdc++ -lpng -lz
	$(STRIP) imgPNG

bc : dummyBC.cpp
	$(CC) -D_REENTRANT=1 -o bc dummyBC.cpp -lstdc++
	$(STRIP) $@

ccDiskCache: ccDiskCache.cpp memFile.o Makefile
	$(CC) -D_REENTRANT=1 -D__STANDALONE__ -o ccDiskCache ccDiskCache.cpp memFile.o -lstdc++

ccWorker: ccWorker.cpp memFile.o Makefile
	$(CC) -D_REENTRANT=1 -ggdb -D__STANDALONE__ -o ccWorker ccWorker.cpp memFile.o -lstdc++ -lcurl -lpthread

ccActiveURL: ccActiveURL.cpp memFile.o $(LIB) Makefile
	$(CC) -D_REENTRANT=1 -ggdb -D__STANDALONE__ -o ccActiveURL ccActiveURL.cpp $(LIBS) -lCurlCache -lstdc++ -lcurl -lpthread

tsTest: tsTest.cpp
	$(CC) $(IFLAGS) -o tsTest tsTest.cpp $(LIBS) -lts

testffFormat: testffFormat.cpp
	$(CC) $(IFLAGS) -o testffFormat testffFormat.cpp $(LIBS) -lavformat -lavcodec -lm -lz
	$(STRIP) $@

ffFormat: ffFormat.cpp
	$(CC) $(IFLAGS) -o ffFormat ffFormat.cpp $(LIBS) -lavformat -lavcodec -lm -lz
	$(STRIP) $@

ffFrames: ffFrames.cpp $(LIB)
	$(CC) $(IFLAGS) -o ffFrames ffFrames.cpp $(LIBS) -lavformat -lavcodec -lmpeg2 -lCurlCache -lvo -lmad -lm -lz -lpthread
	$(STRIP) $@

ffPlay: ffPlay.cpp $(LIB)
	$(CC) $(IFLAGS) -o ffPlay -Xlinker -Map -Xlinker ffPlay.map ffPlay.cpp $(LIBS) -lmpeg2 -lCurlCache -lvo -lmpeg2 -lmad -lid3tag -lm -lz -lpthread 
	$(STRIP) $@

ffTest: ffTest.cpp $(LIB)
	$(CC) $(IFLAGS) -DSTANDALONE=1 -o ffTest -Xlinker -Map -Xlinker ffTest.map ffTest.cpp $(LIBS) -lCurlCache -lmpeg2 -lvo -lmad -lm -lz -lpthread -lmpeg2 -lCurlCache -lstdc++ -lvo 
	$(NM) --demangle $@ | sort >$@.sym
#	$(STRIP) $@

mpDemux: mpDemux.cpp $(LIB)
	$(CC) $(IFLAGS) -DSTANDALONE=1 -o mpDemux -Xlinker -Map -Xlinker mpDemux.map mpDemux.cpp $(LIBS) -lavformat -lavcodec -lmpeg2 -lCurlCache -lvo -lmad -lm -lz -lpthread -lmpeg2
	$(STRIP) $@

mpeg2mp3: mpeg2mp3.cpp $(LIB)
	$(CC) $(IFLAGS) -ggdb -o mpeg2mp3 -Xlinker -Map -Xlinker mpeg2mp3.map mpeg2mp3.cpp $(LIBS) -lavformat -lavcodec -lmpeg2 -lCurlCache -lvo -lmad -lm -lz -lpthread -lstdc++
	$(NM) --demangle $@ | sort >$@.sym
	$(STRIP) $@

cbmGraph: cbmGraph.cpp
	$(CC) $(IFLAGS) -o cbmGraph -Xlinker -Map -Xlinker cbmGraph.map cbmGraph.cpp $(LIBS)
	$(STRIP) $@

cbmStat: cbmStat.cpp
	$(CC) $(IFLAGS) -o cbmStat -Xlinker -Map -Xlinker cbmStat.map cbmStat.cpp $(LIBS)
	$(STRIP) $@

cbmImage: cbmImage.cpp
	$(CC) $(IFLAGS) -DMODULETEST=1 -o cbmImage -Xlinker -Map -Xlinker cbmImage.map cbmImage.cpp hexDump.cpp $(LIBS)
	$(STRIP) $@

pcapTest: pcapTest.cpp $(LIB)
	$(CC) $(IFLAGS) -o pcapTest -Xlinker -Map -Xlinker pcapTest.map pcapTest.cpp $(LIBS)
	$(STRIP) $@

all: curlGet dirTest urlTest jsExec ftRender ftDump tsTest tsThread madHeaders bc ffPlay cbmGraph cbmStat jpegview

.PHONY: install-libs install-headers

shared-headers = ddtoul.h dirByATime.h hexDump.h  \
   imgGIF.h imgJPEG.h imgPNG.h \
   macros.h madHeaders.h memFile.h \
   mtQueue.h relativeURL.h semClasses.h \
   ultoa.h ultodd.h urlFile.h

install-headers:
	cp -f -v $(shared-headers) ../install/arm-linux/include

install-bin:
	$(OBJCOPY) -S -v jsExec   ../install/arm-linux/bin/jsExec
	$(OBJCOPY) -S -v jpegview ../install/arm-linux/bin/jpegview

install: install-bin install-headers

clean:
	rm -f *.o *.a *.map *.lst *.sym bc curlGet dirTest urlTest \
         jsExec testJS ftRender tsTest tsThread madHeaders backtrace \
         cbmImage cbmGraph cbmReset cbmStat ffPlay ffTest jpegview \
         mpeg2mp3 \
         $(LIB)
