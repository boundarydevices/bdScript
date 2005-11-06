#
# Makefile for curlCache library and utility programs
#

all: curlGet dirTest urlTest jsExec ftRender ftDump madHeaders bc cbmGraph cbmStat jpegview flashVar

-include config.mk

KERNEL_BOARDTYPE ?= "NEON"
ifeq ("BD2003",$(KERNEL_BOARDTYPE))
   HARDWARE_TYPE=-DCONFIG_BD2003
else
ifeq ("BD2004",$(KERNEL_BOARDTYPE))
   HARDWARE_TYPE=-DCONFIG_PXA_GAME_CONTROLLER
else
ifeq ("NEON",$(KERNEL_BOARDTYPE))
   HARDWARE_TYPE= -DNEON
else
endif
endif
endif

ifneq (,$(findstring 2.4, $(CONFIG_KERNELPATH)))
   KERNEL_VER=-DKERNEL_2_4
else
   KERNEL_VER=-DKERNEL_2_6
endif

MPEG2LIBS = -lmpeg2 -lvo

#
# These are needed with newer (0.4.0) mpeg2dec library
#
#MPEG2LIBS = -lmpeg2 -lmpeg2convert

OBJS = \
       baudRate.o \
       bitmap.o \
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
       dnsPoll.o \
       dumpCPP.o \
       fbDev.o \
       ftObjs.o \
       gpioPoll.o \
       hexDump.o \
       httpPoll.o \
       image.o \
       imgFile.o \
       imgGIF.o \
       imgJPEG.o \
       imgPNG.o \
       jsAlphaMap.o \
       jsBCWidths.o \
       jsBitmap.o \
       jsCurl.o \
       jsDir.o \
       jsEnviron.o \
       jsExit.o \
       jsFileIO.o \
       jsGlobals.o \
       jsHyperlink.o \
       jsImage.o \
       jsJPEG.o \
       jsKernel.o \
       jsMD5.o \
       jsMonWLAN.o \
       jsTouch.o \
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
       openFds.o \
       palette.o \
       parsedFlash.o \
       parsedURL.o \
       ping.o \
       pollHandler.o \
       pollTimer.o \
       popen.o \
       relativeURL.o \
       rollingMean.o \
       rollingMedian.o \
       semClasses.o \
       serialPoll.o \
       sniffWLAN.o \
       tcpPoll.o \
       ttyPoll.o \
       udpPoll.o \
       ultoa.o \
       ultodd.o \
       urlFile.o \
       flashVar.o \
       jsFlashVar.o \

ifneq ("", $(CONFIG_JSGPIO))
OBJS += jsGpio.o
endif

ifeq (y,$(KERNEL_PCCARD))
ifneq ("", $(CONFIG_JSMONITORWLAN))
OBJS += monitorWLAN.o
endif
endif

ifeq (y,$(KERNEL_FB))
OBJS += \
       audioQueue.o \
       cbmImage.o \
       jsButton.o \
       jsCBM.o \
       jsCamera.o \
       jsFlash.o \
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

HARDWARE_TYPE += -DKERNEL_FB=1
else
endif

ifeq (y, $(CONFIG_JSSTARUSB))
   OBJS += jsPrinter.o \
       jsStar.o \
       jsStarUSB.o \

endif

ifeq (y,$(CONFIG_JSBARCODE))       
   OBJS += barcodePoll.o \
           jsBarcode.o
endif       

ifeq (y,$(CONFIG_LIBFLASH))       
   OBJS += flashThread.o
endif       

ifndef INSTALL_ROOT
INSTALL_ROOT=../install/arm-linux
endif

CC=arm-linux-gcc
LIBBDGRAPH=bdGraph/libbdGraph.a
LIBRARYREFS=$(INSTALL_ROOT)/lib/libflash.a \
            $(INSTALL_ROOT)/lib/libmpeg2.a \
            $(INSTALL_ROOT)/lib/libmad.a

ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   NM=arm-linux-nm
	LD=arm-linux-ld
   STRIP=arm-linux-strip
   OBJCOPY=arm-linux-objcopy
   LIBS=-L./ -L$(INSTALL_ROOT)/lib
   IFLAGS= -I$(CONFIG_KERNELPATH)/include \
          -I../linux-wlan-ng-0.1.16-pre8/src/include/ \
          -I$(INSTALL_ROOT)/include \
          -I$(INSTALL_ROOT)/include/mad \
          -I$(INSTALL_ROOT)/include/nspr \
          -I$(INSTALL_ROOT)/include/freetype2 \
          -I$(INSTALL_ROOT)/include/libavcodec \
          -I$(TOOLCHAINROOT)/include
   LIB = $(INSTALL_ROOT)/lib/libCurlCache.a
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

#
# build empty version to avoid configuration
#
config.h:
	echo "#define CONFIG_LIBMPEG2_OLD 1" > $@

%.o : %.cpp config.h
	$(CC) -fno-rtti -Wall -Wno-invalid-offsetof $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

#jsImage.o : jsImage.cpp Makefile
#	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -shared -DXP_UNIX=1 $(IFLAGS) -o jsImage.o -O2 $<

$(LIBBDGRAPH):
	INSTALL_ROOT=$(INSTALL_ROOT) KERNEL_DIR=$(CONFIG_KERNELPATH) make -C bdGraph all

$(LIB): Makefile $(OBJS)
	echo "OBJS == $(OBJS)"
	echo "KERNEL_FB == '$(KERNEL_FB)'"
	$(AR) r $(LIB) $(OBJS)

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl -lz

curlGet: curlGet.cpp $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o curlGet -O2 -DSTANDALONE $(IFLAGS) curlGet.cpp $(LIBS) -lCurlCache -lcurl -lstdc++ -lz -lm -lssl -lcrypto -ldl

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o urlTest.o -O2 -DSTANDALONE urlFile.cpp

urlTest: urlTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o urlTest urlTest.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lm -lpthread -lssl -lcrypto -ldl

mp3Play: mp3Play.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mp3Play mp3Play.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lmad
	$(STRIP) mp3Play

testJS: testJS.cpp $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o testJS testJS.cpp -DXP_UNIX=1 $(IFLAGS) $(LIBS) -lCurlCache -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lpthread -lm -lz

jsExec: jsExec.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o jsExec jsExec.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle jsExec | sort >jsExec.map
	cp $@ $@.prestrip
	$(STRIP) $@

flashThreadMain.o : flashThread.cpp
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -DSTANDALONE=1 -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o flashThreadMain.o

flashThread: flashThreadMain.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o flashThread flashThreadMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lpthread -lm -lz -lcrypto
	arm-linux-nm flashThread >flashThread.map
	arm-linux-strip flashThread

madDecode: madDecode.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -DSTANDALONE=1 -o madDecode madDecode.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache -lid3tag $(MPEG2LIBS) -lflash -lpthread -lm -lz
	arm-linux-nm madDecode >madDecode.map

jpegview: jpegview.o $(LIBBDGRAPH)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o jpegview jpegview.o -L./bdGraph -lbdGraph -lstdc++

madTest: madTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o madTest madTest.o $(LIBS) -lCurlCache -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lpthread -lm -lz
	arm-linux-nm madTest >madTest.map
	$(STRIP) madTest

ftRender.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o ftRender.o -O2 -D__MODULETEST__ $(IFLAGS) ftObjs.cpp

ftRender: ftRender.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o ftRender ftRender.o $(LIBS) -lCurlCache -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftRender >ftRender.map
	$(STRIP) ftRender

ftDump.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o ftDump.o -O2 -D__DUMP__ $(IFLAGS) ftObjs.cpp

ftDump: ftDump.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o ftDump ftDump.o $(LIBS) -lCurlCache -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftDump >ftDump.map
	$(STRIP) ftDump

recordTest: recordTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o recordTest recordTest.o $(LIBS) -lCurlCache -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
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
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o ffFrames ffFrames.cpp $(LIBS) -lavformat -lavcodec $(MPEG2LIBS) -lCurlCache -lmad -lm -lz -lpthread
	$(STRIP) $@

ffPlay: ffPlay.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o ffPlay -Xlinker -Map -Xlinker ffPlay.map ffPlay.cpp $(LIBS) $(MPEG2LIBS) -lCurlCache -lmad -lid3tag -lm -lz -lpthread 
	$(STRIP) $@

ffTest: ffTest.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DSTANDALONE=1 -o ffTest -Xlinker -Map -Xlinker ffTest.map ffTest.cpp $(LIBS) -lCurlCache $(MPEG2LIBS) -lmad -lm -lz -lpthread -lCurlCache -lstdc++ 
	$(NM) --demangle $@ | sort >$@.sym
#	$(STRIP) $@

mpDemux: mpDemux.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DSTANDALONE=1 -o mpDemux -Xlinker -Map -Xlinker mpDemux.map mpDemux.cpp $(LIBS) -lavformat -lavcodec $(MPEG2LIBS) -lCurlCache -lmad -lm -lz -lpthread 
	$(STRIP) $@

mpeg2mp3: mpeg2mp3.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -ggdb -o mpeg2mp3 -Xlinker -Map -Xlinker mpeg2mp3.map mpeg2mp3.cpp $(LIBS) -lavformat -lavcodec $(MPEG2LIBS) -lCurlCache -lmad -lm -lz -lpthread -lstdc++
	$(NM) --demangle $@ | sort >$@.sym
	$(STRIP) $@

cbmGraph: cbmGraph.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o cbmGraph -Xlinker -Map -Xlinker cbmGraph.map cbmGraph.cpp $(LIBS) -lstdc++
	$(STRIP) $@

starGraph: starGraph.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o starGraph -Xlinker -Map -Xlinker starGraph.map starGraph.cpp $(LIBS)
	$(STRIP) $@

starImage: starImage.cpp $(LIB) $(LIBBDGRAPH) 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o starImage -Xlinker -Map -Xlinker starImage.map starImage.cpp $(LIBS) -L./bdGraph -lCurlCache -lbdGraph -lungif -lpng -ljpeg -lz -lm
	$(STRIP) $@

cbmStat: cbmStat.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o cbmStat -Xlinker -Map -Xlinker cbmStat.map cbmStat.cpp $(LIBS) -lstdc++
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
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o touchPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker touchPoll.map touchPoll.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

urlPoll: urlPoll.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o urlPoll -D__MODULETEST__ -Xlinker -Map -Xlinker urlPoll.map urlPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

tcpPoll: tcpPoll.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o tcpPoll -D__MODULETEST__ -Xlinker -Map -Xlinker tcpPoll.map tcpPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread
	$(STRIP) $@

httpPoll: httpPoll.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o httpPoll -D__MODULETEST__ -Xlinker -Map -Xlinker httpPoll.map httpPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

dnsPoll: dnsPoll.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o dnsPoll -D__MODULETEST__ -Xlinker -Map -Xlinker dnsPoll.map dnsPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

flashVar: flashVar.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) $(KERNEL_VER) -fno-rtti -o flashVar -DSTANDALONE -Xlinker -Map -Xlinker flashVar.map flashVar.cpp pollHandler.o $(LIBS) -lCurlCache -lstdc++
	$(STRIP) $@

tsTest: tsTest.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o tsTest -DSTANDALONE -Xlinker -Map -Xlinker tsTest.map tsTest.cpp pollHandler.o $(LIBS) -lCurlCache -lstdc++
	$(STRIP) $@

mpegDecodeMain.o: mpegDecode.o
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D__MODULETEST__ -c $(IFLAGS) -O2 -o mpegDecodeMain.o mpegDecode.cpp

mpegDecode: mpegDecodeMain.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mpegDecode mpegDecodeMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle mpegDecode | sort >mpegDecode.map
	$(STRIP) $@

hexDump: hexDump.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o hexDump -D__STANDALONE__ -Xlinker -Map -Xlinker hexDump.map hexDump.cpp $(LIBS) -ljpeg -lcrypto -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

rollingMedian: rollingMedian.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o $@ -D__STANDALONE__ -Xlinker -Map -Xlinker rollingMedian.map rollingMedian.cpp $(LIBS) -ljpeg -lcrypto -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

pixmanTest: pixmanTest.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o $@ -D__STANDALONE__ -Xlinker -Map -Xlinker pixmanTest.map pixmanTest.cpp $(LIBS) -ljpeg -lcrypto -lCurlCache -lpixman -lpthread -lstdc++ -lm
	$(STRIP) $@

start.o: start.c
	$(CC) $(HARDWARE_TYPE) -fno-rtti -c -nodefaultlibs -o $@ $<

progFlash.o: progFlash.cpp
	$(CC) $(HARDWARE_TYPE) -fno-rtti -c -nodefaultlibs -o $@ $<

imgFileMain.o: imgFile.cpp
	$(CC) -o $@ -fno-rtti -Wall -Wno-invalid-offsetof -DSTANDALONE=1 $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

IMGFILEOBJS = imgFileMain.o image.o memFile.o fbDev.o imgGIF.o imgJPEG.o imgPNG.o
imgFile: $(IMGFILEOBJS) Makefile 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti \
      -o $@ -D__STANDALONE__ -Xlinker -Map \
      -Xlinker imgFile.map \
      $(IMGFILEOBJS) \
      $(LIBS) -ljpeg -lungif -lpng -lz -lstdc++ 
	$(STRIP) $@
   
#
# This will need additional setup for location of gcc static lib (for udivsi3)
#
#progFlash: start.o progFlash.o
#	$(LD) -o $@ start.o progFlash.o -L../install/lib/gcc-lib/arm-linux/2.95.3 -lgcc
#	$(STRIP) $@
#

all: curlGet dirTest urlTest jsExec ftRender ftDump madHeaders bc cbmGraph cbmStat jpegview flashVar

.PHONY: install-libs install-headers

shared-headers = ddtoul.h dirByATime.h hexDump.h  \
   imgGIF.h imgJPEG.h imgPNG.h \
   macros.h madHeaders.h memFile.h \
   mtQueue.h relativeURL.h semClasses.h \
   ultoa.h ultodd.h urlFile.h

install-headers:
	cp -f -v $(shared-headers) $(INSTALL_ROOT)/include

install-bin: all
	$(OBJCOPY) -S -v jsExec   $(INSTALL_ROOT)/bin/jsExec
	$(OBJCOPY) -S -v jpegview $(INSTALL_ROOT)/bin/jpegview
	$(OBJCOPY) -S -v flashVar $(INSTALL_ROOT)/bin/flashVar
	cp wget $(INSTALL_ROOT)/bin

install: install-bin install-headers

clean:
	rm -f *.o *.a *.map *.lst *.sym bc curlGet dirTest urlTest \
         jsExec testJS ftRender madHeaders backtrace \
         cbmImage cbmGraph cbmReset cbmStat ffPlay ffTest jpegview \
         mpeg2mp3 pixmanTest \
         $(LIB)
