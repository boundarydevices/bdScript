#
# Makefile for curlCache library and utility programs
#

#HARDWARE_TYPE=-DCONFIG_PXA_GAME_CONTROLLER
HARDWARE_TYPE=-DCONFIG_BD2003

OBJS = \
       barcodePoll.o \
       baudRate.o \
       box.o \
       ccActiveURL.o \
       ccDiskCache.o \
       ccWorker.o \
       childProcess.o \
       codeQueue.o \
       curlGet.o \
       ddtoul.o \
       dirByATime.o \
       dither.o \
       fbDev.o \
       flashThread.o \
       ftObjs.o \
       gpioPoll.o \
       hexDump.o \
       image.o \
       imgFile.o \
       imgGIF.o \
       imgJPEG.o \
       imgPNG.o \
       jsAlphaMap.o \
       jsBarcode.o \
       jsBitmap.o \
       jsCurl.o \
       jsDir.o \
       jsEnviron.o \
       jsExit.o \
       jsFileIO.o \
       jsGlobals.o \
       jsGpio.o \
       jsHyperlink.o \
       jsImage.o \
       jsJPEG.o \
       jsKernel.o \
       jsMonWLAN.o \
       jsNewTouch.o \
       jsPing.o \
       jsPopen.o \
       jsProcess.o \
       jsScreen.o \
       jsSerial.o \
       jsSniffWLAN.o \
       jsTCP.o \
       jsTTY.o \
       jsText.o \
       jsTimer.o \
       jsUDP.o \
       jsURL.o \
       jsUse.o \
       md5.o \
       memFile.o \
       monitorWLAN.o \
       openFds.o \
       parsedFlash.o \
       parsedURL.o \
       ping.o \
       pollHandler.o \
       pollTimer.o \
       popen.o \
       relativeURL.o \
       semClasses.o \
       serialPoll.o \
       sniffWLAN.o \
       tcpPoll.o \
       ttyPoll.o \
       udpPoll.o \
       ultoa.o \
       ultodd.o \
       urlFile.o \

ifneq ($(HARDWARE_TYPE),-DCONFIG_PXA_GAME_CONTROLLER)

OBJS += \
       audioQueue.o \
       cbmImage.o \
       flashVar.o \
       jsButton.o \
       jsCBM.o \
       jsCamera.o \
       jsFlash.o \
       jsFlashVar.o \
       jsMP3.o \
       jsMPEG.o \
       jsVolume.o \
       madDecode.o \
       madHeaders.o \
       mpDemux.o \
       mpegDecode.o \
       touchPoll.o \
       videoFrames.o \
       videoQueue.o \
       zOrder.o \

endif

CC=arm-linux-gcc
LIBBDGRAPH=bdGraph/libbdGraph.a
LIBRARYREFS=../install/arm-linux/lib/libflash.a \
            ../install/arm-linux/lib/libmpeg2.a \
            ../install/arm-linux/lib/libmad.a 

ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   NM=arm-linux-nm
	LD=arm-linux-ld
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
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

#jsImage.o : jsImage.cpp Makefile
#	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -shared -DXP_UNIX=1 $(IFLAGS) -o jsImage.o -O2 $<

$(LIBBDGRAPH):
	make -C bdGraph all

$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl -lz

curlGet: curlGet.cpp $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o curlGet -O2 -DSTANDALONE $(IFLAGS) curlGet.cpp $(LIBS) -lCurlCache -lcurl -lstdc++ -lz -lm -lssl

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o urlTest.o -O2 -DSTANDALONE urlFile.cpp

urlTest: urlTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o urlTest urlTest.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lm -lpthread -lssl

mp3Play: mp3Play.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mp3Play mp3Play.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lmad
	$(STRIP) mp3Play

testJS: testJS.cpp $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o testJS testJS.cpp -DXP_UNIX=1 $(IFLAGS) $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lpthread -lm -lz

jsExec: jsExec.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o jsExec jsExec.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lvo -lCurlCache -lmpeg2 -lflash -lpthread -lm -lz -lLinuxWLAN -lcrypto -lssl
	arm-linux-nm --demangle jsExec | sort >jsExec.map
	$(STRIP) $@

flashThreadMain.o : flashThread.cpp
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -DSTANDALONE=1 -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o flashThreadMain.o

flashThread: flashThreadMain.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o flashThread flashThreadMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lvo -lCurlCache -lmpeg2 -lflash -lpthread -lm -lz -lLinuxWLAN -lcrypto
	arm-linux-nm flashThread >flashThread.map
	arm-linux-strip flashThread

madDecode: madDecode.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -DSTANDALONE=1 -o madDecode madDecode.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lvo -lCurlCache -lid3tag -lmpeg2 -lflash -lpthread -lm -lz
	arm-linux-nm madDecode >madDecode.map

jpegview: jpegview.o $(LIBBDGRAPH)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o jpegview jpegview.o -L./bdGraph -lbdGraph

madTest: madTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o madTest madTest.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lpthread -lm -lz
	arm-linux-nm madTest >madTest.map
	$(STRIP) madTest

ftRender.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o ftRender.o -O2 -D__MODULETEST__ $(IFLAGS) ftObjs.cpp

ftRender: ftRender.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o ftRender ftRender.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftRender >ftRender.map
	$(STRIP) ftRender

ftDump.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o ftDump.o -O2 -D__DUMP__ $(IFLAGS) ftObjs.cpp

ftDump: ftDump.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o ftDump ftDump.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftDump >ftDump.map
	$(STRIP) ftDump

recordTest: recordTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o recordTest recordTest.o $(LIBS) -lCurlCache -lstdc++ -ljs -lnspr4 -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm recordTest >recordTest.map
	$(STRIP) recordTest

madHeadersMain.o: madHeaders.h madHeaders.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o madHeadersMain.o -O2 -D__STANDALONE__ $(IFLAGS) madHeaders.cpp

madHeaders: madHeadersMain.o Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o madHeaders madHeadersMain.o $(LIBS) -lCurlCache -lstdc++ -lmad -lid3tag -lm -lz
	arm-linux-nm madHeaders >madHeaders.map
	$(STRIP) madHeaders

imgJPEGMain.o : imgJPEG.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o imgJPEGMain.o -O2 -D__STANDALONE__ $(IFLAGS) imgJPEG.cpp

imgJPEG : imgJPEGMain.o memFile.o hexDump.o fbDev.o
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o imgJPEG imgJPEGMain.o memFile.o hexDump.o fbDev.o -lstdc++ -ljpeg
	$(STRIP) imgJPEG

imgPNGMain.o : imgPNG.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o imgPNGMain.o -O2 -D__STANDALONE__ $(IFLAGS) imgPNG.cpp

imgPNG : imgPNGMain.o memFile.o hexDump.o fbDev.o
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o imgPNG imgPNGMain.o memFile.o hexDump.o fbDev.o -lstdc++ -lpng -lz
	$(STRIP) imgPNG

bc : dummyBC.cpp
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o bc dummyBC.cpp -lstdc++
	$(STRIP) $@

ccDiskCache: ccDiskCache.cpp memFile.o Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -D__STANDALONE__ -o ccDiskCache ccDiskCache.cpp memFile.o -lstdc++

ccWorker: ccWorker.cpp memFile.o Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -ggdb -D__STANDALONE__ -o ccWorker ccWorker.cpp memFile.o -lstdc++ -lcurl -lpthread -lz -lm

ccActiveURL: ccActiveURL.cpp memFile.o $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -ggdb -D__STANDALONE__ -o ccActiveURL ccActiveURL.cpp $(LIBS) -lCurlCache -lstdc++ -lcurl -lpthread

testffFormat: testffFormat.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o testffFormat testffFormat.cpp $(LIBS) -lavformat -lavcodec -lm -lz
	$(STRIP) $@

ffFormat: ffFormat.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o ffFormat ffFormat.cpp $(LIBS) -lavformat -lavcodec -lm -lz
	$(STRIP) $@

ffFrames: ffFrames.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o ffFrames ffFrames.cpp $(LIBS) -lavformat -lavcodec -lmpeg2 -lCurlCache -lvo -lmad -lm -lz -lpthread
	$(STRIP) $@

ffPlay: ffPlay.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o ffPlay -Xlinker -Map -Xlinker ffPlay.map ffPlay.cpp $(LIBS) -lmpeg2 -lCurlCache -lvo -lmpeg2 -lmad -lid3tag -lm -lz -lpthread 
	$(STRIP) $@

ffTest: ffTest.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DSTANDALONE=1 -o ffTest -Xlinker -Map -Xlinker ffTest.map ffTest.cpp $(LIBS) -lCurlCache -lmpeg2 -lvo -lmad -lm -lz -lpthread -lmpeg2 -lCurlCache -lstdc++ -lvo 
	$(NM) --demangle $@ | sort >$@.sym
#	$(STRIP) $@

mpDemux: mpDemux.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DSTANDALONE=1 -o mpDemux -Xlinker -Map -Xlinker mpDemux.map mpDemux.cpp $(LIBS) -lavformat -lavcodec -lmpeg2 -lCurlCache -lvo -lmad -lm -lz -lpthread -lmpeg2
	$(STRIP) $@

mpeg2mp3: mpeg2mp3.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -ggdb -o mpeg2mp3 -Xlinker -Map -Xlinker mpeg2mp3.map mpeg2mp3.cpp $(LIBS) -lavformat -lavcodec -lmpeg2 -lCurlCache -lvo -lmad -lm -lz -lpthread -lstdc++
	$(NM) --demangle $@ | sort >$@.sym
	$(STRIP) $@

cbmGraph: cbmGraph.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o cbmGraph -Xlinker -Map -Xlinker cbmGraph.map cbmGraph.cpp $(LIBS)
	$(STRIP) $@

cbmStat: cbmStat.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o cbmStat -Xlinker -Map -Xlinker cbmStat.map cbmStat.cpp $(LIBS)
	$(STRIP) $@

cbmImage: cbmImage.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DMODULETEST=1 -o cbmImage -Xlinker -Map -Xlinker cbmImage.map cbmImage.cpp hexDump.cpp $(LIBS)
	$(STRIP) $@

pcapTest: pcapTest.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o pcapTest -Xlinker -Map -Xlinker pcapTest.map pcapTest.cpp $(LIBS)
	$(STRIP) $@

md5: md5.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o md5 -DSTANDALONE=1 -Xlinker -Map -Xlinker md5.map md5.cpp $(LIBS) -lcrypto -lCurlCache
	$(STRIP) $@

avSendTo: avSendTo.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o avSendTo -DSTANDALONE=1 -Xlinker -Map -Xlinker avSendTo.map avSendTo.cpp $(LIBS) -lCurlCache -lpng -ljpeg -lungif -lcrypto -lz -lm -lpthread
	$(STRIP) $@

pollHandlerTest: pollHandler.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o pollHandlerTest -DSTANDALONE=1 -Xlinker -Map -Xlinker pollHandlerTest.map pollHandler.cpp $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

pollTimerTest: pollTimer.cpp $(LIB)
	$(CC) -fno-rtti $(HARDWARE_TYPE) $(IFLAGS) -o pollTimerTest -DSTANDALONE=1 -Xlinker -Map -Xlinker pollTimerTest.map pollTimer.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

ttyPoll: ttyPoll.cpp $(LIB)
	$(CC) -fno-rtti $(HARDWARE_TYPE) $(IFLAGS) -o ttyPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker ttyPoll.map ttyPoll.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

udpPoll: udpPoll.cpp $(LIB)
	$(CC) -fno-rtti $(HARDWARE_TYPE) $(IFLAGS) -o udpPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker udpPoll.map udpPoll.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

barcodePoll: barcodePoll.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o barcodePoll -DSTANDALONE=1 -Xlinker -Map -Xlinker barcodePoll.map barcodePoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

serialPoll: serialPoll.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o serialPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker serialPoll.map serialPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

touchPoll: touchPoll.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o touchPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker touchPoll.map touchPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

urlPoll: urlPoll.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o urlPoll -D__MODULETEST__ -Xlinker -Map -Xlinker urlPoll.map urlPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

tcpPoll: tcpPoll.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o tcpPoll -D__MODULETEST__ -Xlinker -Map -Xlinker tcpPoll.map tcpPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

flashVar: flashVar.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o flashVar -DSTANDALONE -Xlinker -Map -Xlinker flashVar.map flashVar.cpp pollHandler.o $(LIBS) -lCurlCache -lstdc++
	$(STRIP) $@

hexDump: hexDump.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o hexDump -D__STANDALONE__ -Xlinker -Map -Xlinker hexDump.map hexDump.cpp $(LIBS) -ljpeg -lcrypto -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

start.o: start.c
	$(CC) $(HARDWARE_TYPE) -fno-rtti -c -nodefaultlibs -o $@ $<

progFlash.o: progFlash.cpp
	$(CC) $(HARDWARE_TYPE) -fno-rtti -c -nodefaultlibs -o $@ $<

progFlash: start.o progFlash.o
	$(LD) -o $@ start.o progFlash.o -L../install/lib/gcc-lib/arm-linux/2.95.3 -lgcc
	$(STRIP) $@


all: curlGet dirTest urlTest jsExec ftRender ftDump madHeaders bc cbmGraph cbmStat jpegview progFlash flashVar

.PHONY: install-libs install-headers

shared-headers = ddtoul.h dirByATime.h hexDump.h  \
   imgGIF.h imgJPEG.h imgPNG.h \
   macros.h madHeaders.h memFile.h \
   mtQueue.h relativeURL.h semClasses.h \
   ultoa.h ultodd.h urlFile.h

install-headers:
	cp -f -v $(shared-headers) ../install/arm-linux/include

install-bin: all
	$(OBJCOPY) -S -v jsExec   ../install/arm-linux/bin/jsExec
	$(OBJCOPY) -S -v jpegview ../install/arm-linux/bin/jpegview
	$(OBJCOPY) -S -v flashVar ../install/arm-linux/bin/flashVar
	cp wget ../install/arm-linux/bin

install: install-bin install-headers

clean:
	rm -f *.o *.a *.map *.lst *.sym bc curlGet dirTest urlTest \
         jsExec testJS ftRender madHeaders backtrace \
         cbmImage cbmGraph cbmReset cbmStat ffPlay ffTest jpegview \
         mpeg2mp3 \
         $(LIB)
