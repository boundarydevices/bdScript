#
# Makefile for curlCache library and utility programs
#

-include config.mk

KERNEL_VER=-DKERNEL_2_6

MPEG2LIBS = -lmpeg2arch -lmpeg2 -lmpeg2arch -lvo -lmpeg2convert -lmpeg2arch 

ifeq (y, $(CONFIG_MPLAYER))
        MPEG2LIBS += -lavformat -lavutil -lavcodec -lxvidcore -lavutil 
endif

#
# These are needed with newer (0.4.0) mpeg2dec library
#
#MPEG2LIBS = -lmpeg2 -lmpeg2convert

OBJS = \
       ascii85.o \
       baudRate.o \
       bitmap.o \
       blockSig.o \
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
       dspOutSignal.o \
       dumpCPP.o \
       fbDev.o \
       ftObjs.o \
       gpioPoll.o \
       hexDump.o \
       httpPoll.o \
       image.o \
       imageInfo.o \
       imageToPS.o \
       imgFile.o \
       imgGIF.o \
       imgJPEG.o \
       imgPNG.o \
       imgToPNG.o \
       imgTransparent.o \
       jsAlphaMap.o \
       jsBCWidths.o \
       jsBitmap.o \
       jsCurl.o \
       jsData.o \
       jsDir.o \
       jsEnviron.o \
       jsExit.o \
       jsFileIO.o \
       jsGlobals.o \
       jsHyperlink.o \
       jsImage.o \
       jsJPEG.o \
       jsPNG.o \
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
       jsUsblp.o \
       jsUse.o \
       logTraces.o \
       md5.o \
       memFile.o \
       mpegStream.o \
       multiSignal.o \
       openFds.o \
       palette.o \
       parsedFlash.o \
       parsedURL.o \
       ping.o \
       pngToPS.o \
       pollHandler.o \
       pollTimer.o \
       popen.o \
       rawKbd.o \
       relativeURL.o \
       rollingMean.o \
       rollingMedian.o \
       rtSignal.o \
       screenImage.o \
       semClasses.o \
       serialPoll.o \
       serialSignal.o \
       setSerial.o \
       sniffWLAN.o \
       tcpPoll.o \
       trace.o \
       ttyPoll.o \
       udpPoll.o \
       ultoa.o \
       ultodd.o \
       urlFile.o \
       usblpPoll.o \
       flashVar.o \
       jsFlashVar.o \


ifeq (y, $(CONFIG_MCP_UCB1400_TS))
OBJS += ucb1x00_pins.o
endif

ifneq ("", $(CONFIG_JSGPIO))
OBJS += jsGpio.o
endif

ifeq (y,$(KERNEL_PCCARD))
ifneq ("", $(CONFIG_JSMONITORWLAN))
OBJS += monitorWLAN.o
endif
endif

ifeq (y,$(KERNEL_INPUT))
OBJS += inputPoll.o \
        jsInput.o
endif

KERNEL_FB ?= y
CONFIG_JSCAIRO ?= n
ifeq (y,$(CONFIG_JSCAIRO))
OBJS += jsCairo.o
endif

ifeq (y,$(KERNEL_FB))
OBJS += \
       audioOutPoll.o \
       audioQueue.o \
       cbmImage.o \
       jsButton.o \
       jsCBM.o \
       jsCamera.o \
       jsFlash.o \
       jsMP3.o \
       jsVolume.o \
       madDecode.o \
       madHeaders.o \
       touchCalibrate.o \
       touchPoll.o \
       touchSignal.o \
       zOrder.o \

HARDWARE_TYPE += -DKERNEL_FB=1
else
endif

ifeq (y,$(CONFIG_JSMPEG))
   OBJS += mpegDecode.o mpegPS.o videoQueue.o videoFrames.o mpDemux.o jsMPEG.o mediaQueue.o mpegQueue.o aviHeader.o riffRead.o 
endif

ifeq (y,$(KERNEL_FB_SM501))
OBJS += fbMem.o yuyv.o
SM501LIB = $(INSTALL_ROOT)/lib/libSM501.a
SM501OBJS = asyncScreenObj.o \
            cylinderShadow.o \
            fbcCircular.o \
            fbcHideable.o \
            fbcMoveHide.o \
            fbCmdBlt.o \
            fbCmdClear.o \
            fbCmdFinish.o \
            fbCmdList.o \
            fbCmdListSignal.o \
            fbCmdWait.o \
            fbImage.o \
            fbMem.o \
            img4444.o \
            slotWheel.o \
            sm501alpha.o \
            vsyncSignal.o \
            yuvSignal.o
OBJS += jsCursor.o
endif

ifeq (y,$(CONFIG_JSSTARUSB))
   OBJS += jsPrinter.o \
       jsStar.o \
       jsStarUSB.o \

endif

ifeq (y,$(CONFIG_JSBARCODE))       
   OBJS += barcodePoll.o \
           jsBarcode.o
endif       

CONFIG_LIBFLASH?=y
ifeq (y,$(CONFIG_LIBFLASH))       
   OBJS += flashThread.o 
endif       

ifeq (y,$(CONFIG_MPLAYER))
   OBJS += mplayerWrap.o
endif

INSTALL_ROOT ?= ../install/arm-linux

CROSS_COMPILE ?= arm-linux-
CC= $(CROSS_COMPILE)gcc
LIBBDGRAPH=bdGraph/libbdGraph.a
LIBRARYREFS=$(INSTALL_ROOT)/lib/libflash.a \
            $(INSTALL_ROOT)/lib/libmpeg2.a \
            $(INSTALL_ROOT)/lib/libmad.a

ifneq (,$(findstring arm, $(CC)))
   AR=$(CROSS_COMPILE)ar
   NM=$(CROSS_COMPILE)nm
   LD=$(CROSS_COMPILE)ld
   RANLIB=$(CROSS_COMPILE)ranlib
   STRIP=$(CROSS_COMPILE)strip
   OBJCOPY=$(CROSS_COMPILE)objcopy
   LIBS=-L./ -L$(INSTALL_ROOT)/lib
   IFLAGS= -I$(CONFIG_KERNELPATH)/include \
          -I../linux-wlan-ng-0.1.16-pre8/src/include/ \
          -I$(INSTALL_ROOT)/include \
          -I$(INSTALL_ROOT)/include/mad \
          -I$(INSTALL_ROOT)/include/nspr \
          -I$(INSTALL_ROOT)/include/freetype2 \
          -I$(TOOLCHAINROOT)/include 
   LIB = $(INSTALL_ROOT)/lib/libCurlCache.a
else
#   CC=g++
   AR=ar
   NM=nm
   LIBS=-L./
   RANLIB=ranlib
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

ifeq (y,$(CONFIG_JSCAIRO))
echo "CAIRO == $(CONFIG_JSCAIRO)"
IFLAGS += -I$(INSTALL_ROOT)/include/cairo
LIBS   +=-lCurlCache -lcairo -lpixman -lfontconfig -lexpat
endif

ifeq (y,$(CONFIG_FFMPEG))
IFLAGS += -I$(INSTALL_ROOT)/include/libavcodec -I$(INSTALL_ROOT)/include/libavformat -I$(INSTALL_ROOT)/include/libavutil
LIBS   +=-lavformat -lavcodec -lavutil -lxvidcore -lz
endif

ifeq (y,$(KERNEL_SND))
IFLAGS += -DALSA_SOUND=1
endif

ODOMOBJS = \
       mpegRxUDP.o \
       odomDigit.o \
       odomGraphics.o \
       odomValue2.o \
       odomVideo.o

ODOMLIB = $(INSTALL_ROOT)/lib/libOdometer.a

#
# build empty version to avoid configuration
#
config.h:
	echo "#define CONFIG_LIBMPEG2_OLD 1" > $@

%.o : %.cpp config.h
	$(CC) -ggdb -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

#jsImage.o : jsImage.cpp Makefile
#	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -shared -DXP_UNIX=1 $(IFLAGS) -o jsImage.o -O2 $<

$(LIBBDGRAPH):
	INSTALL_ROOT=$(INSTALL_ROOT) KERNEL_DIR=$(CONFIG_KERNELPATH) make -C bdGraph all

$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

$(SM501LIB): Makefile $(SM501OBJS)
	$(AR) r $(SM501LIB) $(SM501OBJS)
	$(RANLIB) $(SM501LIB)

$(ODOMLIB): Makefile $(ODOMOBJS)
	$(AR) r $(ODOMLIB) $(ODOMOBJS)
	$(RANLIB) $(ODOMLIB)

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl -lz

curlGet: curlGet.cpp $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o curlGet -O2 -DSTANDALONE $(IFLAGS) curlGet.cpp $(LIBS) -lCurlCache -lcurl -lstdc++ -lz -lm -lssl -lcrypto -ldl

daemonize: daemonize.cpp $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o daemonize -O2 -DSTANDALONE $(IFLAGS) daemonize.cpp $(LIBS) -lCurlCache -lstdc++
	$(STRIP) $@

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
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -ggdb -o jsExec jsExec.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle jsExec | sort >jsExec.map
	cp $@ $@.prestrip
	$(STRIP) $@

jsData: jsData.cpp $(LIB) Makefile
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -DMODULETEST=1 -D_REENTRANT=1 -DXP_UNIX=1 $(IFLAGS) -O2 -o jsData jsData.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle jsData | sort >jsData.map
	cp $@ $@.prestrip
	$(STRIP) $@

odomGraphicsMain.o: odomGraphics.cpp odomGraphics.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

odomGraphics: odomGraphicsMain.o $(LIB) Makefile $(ODOMLIB) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o odomGraphics odomGraphicsMain.o $(LIBS) -lOdometer -lSM501 -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle odomGraphics | sort >odomGraphics.map
	cp $@ $@.prestrip
	$(STRIP) $@

slotWheelMain.o: slotWheel.cpp slotWheel.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

slotWheel: slotWheelMain.o $(LIB) Makefile $(ODOMLIB) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o slotWheel slotWheelMain.o $(LIBS) -lOdometer -lSM501 -lCurlCache -lSM501 -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle slotWheel | sort >slotWheel.map
	cp $@ $@.prestrip
	$(STRIP) $@

tradeShow: tradeShow.o $(LIB) Makefile $(ODOMLIB) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o $@ tradeShow.o $(LIBS) -lOdometer -lSM501 -lCurlCache -lSM501 -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle $@ | sort >$@.map
	cp $@ $@.prestrip
	$(STRIP) $@

mpegQueueMain.o: mpegQueue.cpp mpegQueue.h 
	$(CC) -ggdb -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -DMD5OUTPUT -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

mpegQueue: mpegQueueMain.o $(LIB) Makefile $(ODOMLIB) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -ggdb -o mpegQueue mpegQueueMain.o $(LIBS) -lOdometer -lSM501 -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle mpegQueue | sort >mpegQueue.map
	cp $@ $@.prestrip
	$(STRIP) $@

mpegStreamMain.o: mpegStream.cpp mpegStream.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

mpegStream: mpegStreamMain.o $(LIB) Makefile $(ODOMLIB) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mpegStream mpegStreamMain.o $(LIBS) -lOdometer -lSM501 -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle mpegStream | sort >mpegStream.map
	cp $@ $@.prestrip
	$(STRIP) $@

mpegStreamMain2.o: mpegStream.cpp mpegStream.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST2 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

mpegStream2: mpegStreamMain2.o $(LIB) Makefile $(ODOMLIB) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mpegStream2 mpegStreamMain2.o $(LIBS) -lOdometer -lSM501 -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle mpegStream2 | sort >mpegStream2.map
	cp $@ $@.prestrip
	$(STRIP) $@

digitStrip: digitStrip.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o digitStrip digitStrip.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle digitStrip | sort >digitStrip.map
	cp $@ $@.prestrip
	$(STRIP) $@

shadeGrad: shadeGrad.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o shadeGrad shadeGrad.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle shadeGrad | sort >shadeGrad.map
	cp $@ $@.prestrip
	$(STRIP) $@

cylinderPNG: cylinderPNG.o $(LIB) Makefile $(LIBBDGRAPH) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o $@ $@.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lSM501 -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	cp $@ $@.prestrip
	$(STRIP) $@

cylinderShadow: cylinderShadow.cpp $(LIB) Makefile $(LIBBDGRAPH) $(SM501LIB) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -DMODULETEST=1 -o $@ $@.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lSM501 -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	cp $@ $@.prestrip
	$(STRIP) $@

dictionary.o: dictionary.cpp dictionary.h
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

dictionary: dictionary.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	echo $(KERNEL_BOARDTYPE)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o dictionary dictionary.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle dictionary | sort >dictionary.map
	cp $@ $@.prestrip
	$(STRIP) $@

odomDigitMain.o: odomDigit.cpp odomDigit.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

odomDigit: odomDigitMain.o $(ODOMLIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o odomDigit odomDigitMain.o $(LIBS) -lCurlCache -lstdc++ -lOdometer -lSM501 -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz

odomValueMain.o: odomValue.cpp odomValue.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

odomValue: odomValueMain.o $(LIB) $(ODOMLIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o odomValue odomValueMain.o $(LIBS) -lCurlCache -lstdc++ -lOdometer -lSM501 -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz
	arm-linux-nm --demangle odomValue | sort >odomValue.map

odomValue2Main.o: odomValue2.cpp odomValue2.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

odomValue2: odomValue2Main.o $(LIB) $(ODOMLIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o odomValue2 odomValue2Main.o $(LIBS) -lCurlCache -lstdc++ -lOdometer -lSM501 -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz -lpthread
	arm-linux-nm --demangle odomValue2 | sort >odomValue2.map

sm501mem: sm501mem.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o sm501mem sm501mem.o $(LIBS) -lCurlCache -lstdc++ -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz

fbMemMain.o: fbMem.cpp fbMem.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

fbMem: fbMemMain.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o fbMem fbMemMain.o $(LIBS) -lCurlCache -lstdc++ -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz

asyncTest: asyncTest.o $(LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o asyncTest asyncTest.o $(LIBS) -lCurlCache -lstdc++ -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz

fbCmdListMain.o: fbCmdList.cpp fbCmdList.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

fbCmdList: fbCmdListMain.o $(LIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o fbCmdList fbCmdListMain.o $(LIBS) -lCurlCache -lstdc++ -lCurlCache -lSM501 -lpng -ljpeg -lungif -lfreetype -lm -lz

fbcHideableMain.o: fbcHideable.cpp fbcHideable.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

fbcHideable: fbcHideableMain.o $(LIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o fbcHideable fbcHideableMain.o $(LIBS) -lCurlCache -lSM501 -lCurlCache -lstdc++ -lpng -ljpeg -lungif -lfreetype -lm -lz -lpthread

fbcMoveHideMain.o: fbcMoveHide.cpp fbcMoveHide.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

fbcMoveHide: fbcMoveHideMain.o $(LIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o fbcMoveHide fbcMoveHideMain.o $(LIBS) -lCurlCache -lSM501 -lCurlCache -lstdc++ -lpng -ljpeg -lungif -lfreetype -lm -lz -lpthread

fbcCircularMain.o: fbcCircular.cpp fbcCircular.h 
	$(CC) -fno-rtti -Wall $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

fbcCircular: fbcCircularMain.o $(LIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o fbcCircular fbcCircularMain.o $(LIBS) -lCurlCache -lSM501 -lCurlCache -lstdc++ -lpng -ljpeg -lungif -lfreetype -lm -lz -lpthread

fbCmdClearMain.o: fbCmdClear.cpp fbCmdClear.h 
	$(CC) -fno-rtti -Wall -DMODULETEST=1 $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DMODULETEST -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o $@

fbCmdClear: fbCmdClearMain.o $(LIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o fbCmdClear fbCmdClearMain.o $(LIBS) -lSM501 -lOdometer -lCurlCache -lstdc++ -lCurlCache -lpng -ljpeg -lungif -lfreetype -lm -lz

sm501reg: sm501reg.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o sm501reg sm501reg.cpp -lstdc++ 
	arm-linux-nm sm501reg >sm501reg.map
	$(STRIP) sm501reg

sm501poke: sm501poke.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o sm501poke sm501poke.cpp -lstdc++ 
	arm-linux-nm sm501poke >sm501poke.map
	$(STRIP) sm501poke

sm501dump: sm501dump.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o sm501dump sm501dump.cpp hexDump.o -lstdc++ 
	arm-linux-nm sm501dump >sm501dump.map
	$(STRIP) sm501dump

sm501cmdlist: sm501cmdlist.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o sm501cmdlist sm501cmdlist.cpp -lstdc++ 
	arm-linux-nm sm501cmdlist >sm501cmdlist.map
	$(STRIP) sm501cmdlist

sm501alpha: sm501alpha.cpp $(LIB) 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -D MODULETEST -o sm501alpha sm501alpha.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm sm501alpha >sm501alpha.map
	$(STRIP) sm501alpha

multiSignal: multiSignal.cpp $(LIB) 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -D MODULETEST -o multiSignal multiSignal.cpp $(LIBS) -lCurlCache -lpthread -lstdc++ 
	arm-linux-nm multiSignal >multiSignal.map

videoSet: videoSet.cpp $(LIB) 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o videoSet videoSet.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm videoSet >videoSet.map
	$(STRIP) videoSet

img4444: img4444.cpp $(LIB) $(SM501LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -D MODULETEST -o img4444 img4444.cpp $(LIBS) -lCurlCache -lSM501 -lpng -ljpeg -lungif -lz -lstdc++ 
	arm-linux-nm img4444 >img4444.map
	$(STRIP) img4444

flashInt: flashInt.cpp $(LIB) 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o flashInt flashInt.cpp $(LIBS) -lCurlCache -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lm -lz -lssl -lcrypto -ldl -lstdc++
	arm-linux-nm flashInt >flashInt.map
	$(STRIP) flashInt

flashPlay: flashPlay.cpp $(LIB) 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o flashPlay flashPlay.cpp $(LIBS) -lCurlCache -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lm -lz -lssl -lcrypto -ldl -lstdc++
	arm-linux-nm flashPlay >flashPlay.map
	$(STRIP) flashPlay

rtsCTS: rtsCTS.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o rtsCTS rtsCTS.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm rtsCTS >rtsCTS.map
	$(STRIP) rtsCTS

setBaud: setBaud.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o setBaud setBaud.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm setBaud >setBaud.map
	$(STRIP) setBaud

miniterm: miniterm.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o miniterm miniterm.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm miniterm >miniterm.map
	$(STRIP) miniterm

serialSignal: serialSignal.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DMODULETEST -O2 -o serialSignal serialSignal.cpp $(LIBS) -lCurlCache -lsupc++ -lpthread
	$(STRIP) serialSignal

ttySignal: ttySignal.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DMODULETEST -O2 -o ttySignal ttySignal.cpp $(LIBS) -lCurlCache -lsupc++ -lpthread
	$(STRIP) ttySignal

serialCounts: serialCounts.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o serialCounts serialCounts.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm serialCounts >serialCounts.map
	$(STRIP) serialCounts

serialTouch: serialTouch.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o serialTouch serialTouch.cpp $(LIBS) -lCurlCache -lstdc++ 
	arm-linux-nm serialTouch >serialTouch.map
	$(STRIP) serialTouch

sm501flip: sm501flip.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o sm501flip sm501flip.cpp -lstdc++ 
	arm-linux-nm sm501flip >sm501flip.map
	$(STRIP) sm501flip

markSpace: markSpace.cpp
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -O2 -o markSpace markSpace.cpp -lstdc++ 
	arm-linux-nm markSpace >markSpace.map
	$(STRIP) markSpace

flashThreadMain.o : flashThread.cpp
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -DSTANDALONE=1 -c -DXP_UNIX=1 $(IFLAGS) -O2 $< -o flashThreadMain.o

flashThread: flashThreadMain.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o flashThread flashThreadMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lpthread -lm -lz -lcrypto
	arm-linux-nm flashThread >flashThread.map
	arm-linux-strip flashThread

madDecode: madDecode.o $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -DSTANDALONE=1 $(IFLAGS) -o madDecode madDecode.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache -lid3tag $(MPEG2LIBS) -lflash -lpthread -lm -lz
	arm-linux-nm madDecode >madDecode.map

madDecode2: madDecode.cpp $(LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D_REENTRANT=1 -DSTANDALONE2=1 $(IFLAGS) -o madDecode2 madDecode.cpp $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs  -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache -lid3tag $(MPEG2LIBS) -lflash -lpthread -lm -lz
	arm-linux-nm madDecode2 >madDecode2.map

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
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o imgPNG imgPNGMain.o memFile.o hexDump.o fbDev.o $(LIBS) $(LIB) -lstdc++ -lpng -lz
	$(STRIP) imgPNG

imgToPNGMain.o : imgToPNG.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -c $(IFLAGS) -o imgToPNGMain.o -O2 -D__STANDALONE__ $(IFLAGS) imgToPNG.cpp

imgToPNG : imgToPNGMain.o $(LIB) 
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o imgToPNG imgToPNGMain.o $(LIBS) -lCurlCache -lpng -ljpeg -lungif -lstdc++ -lpng -lz
	$(STRIP) imgToPNG

bc : dummyBC.cpp
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o bc dummyBC.cpp -lstdc++
	$(STRIP) $@

ccDiskCache: ccDiskCache.cpp memFile.o Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -D__STANDALONE__ -o ccDiskCache ccDiskCache.cpp memFile.o -lstdc++

ccWorker: ccWorker.cpp memFile.o Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -ggdb -D__STANDALONE__ -o ccWorker ccWorker.cpp memFile.o -lstdc++ -lcurl -lpthread -lz -lm

ccActiveURL: ccActiveURL.cpp memFile.o $(LIB) Makefile
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -ggdb -D__STANDALONE__ -o ccActiveURL ccActiveURL.cpp $(LIBS) -lCurlCache -lstdc++ -lcurl -lpthread

mpDemux: mpDemux.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DSTANDALONE=1 -o mpDemux -Xlinker -Map -Xlinker mpDemux.map mpDemux.cpp $(LIBS) $(MPEG2LIBS) -lCurlCache -lmad -lm -lz -lpthread -lsupc++
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

backtrace: backtrace.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -rdynamic -o backtrace -DSTANDALONE=1 -Xlinker -Map -Xlinker backtrace.map backtrace.cpp $(LIBS) -lcrypto -lCurlCache

logTraces: logTraces.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -rdynamic -o logTraces -DMODULETEST=1 -Xlinker -Map -Xlinker logTraces.map logTraces.cpp $(LIBS) -lcrypto -lCurlCache -lstdc++

avSendTo: avSendTo.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o avSendTo -DSTANDALONE=1 -Xlinker -Map -Xlinker avSendTo.map avSendTo.cpp $(LIBS) -lCurlCache -lpng -ljpeg -lungif -lcrypto -lz -lm -lpthread -lsupc++
	$(STRIP) $@

pollHandlerTest: pollHandler.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -o pollHandlerTest -DSTANDALONE=1 -Xlinker -Map -Xlinker pollHandlerTest.map pollHandler.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

mplayerTest: mplayerWrap.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti  -o $@ -DSTANDALONE=1 -Xlinker -Map -Xlinker $@.map mplayerWrap.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
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

inputPoll: inputPoll.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o inputPoll -DINPUT_STANDALONE=1 -Xlinker -Map -Xlinker inputPoll.map inputPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lsupc++
	$(STRIP) $@

serialPoll: serialPoll.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o serialPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker serialPoll.map serialPoll.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

pollTimer: pollTimer.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o pollTimer -DSTANDALONE=1 -Xlinker -Map -Xlinker pollTimer.map pollTimer.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

bltTest: bltTest.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o bltTest -DSTANDALONE=1 -Xlinker -Map -Xlinker bltTest.map bltTest.cpp pollHandler.o $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

touchPoll: touchPoll.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o touchPoll -DSTANDALONE=1 -Xlinker -Map -Xlinker touchPoll.map touchPoll.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

touchSignal: touchSignal.cpp $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o touchSignal -DSTANDALONE=1 -Xlinker -Map -Xlinker touchSignal.map touchSignal.cpp $(LIBS) -lCurlCache -lpthread -lstdc++
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
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) $(KERNEL_VER) -fno-rtti -o flashVar -DSTANDALONE -Xlinker -Map -Xlinker flashVar.map flashVar.cpp pollHandler.o $(LIBS) -lCurlCache -lsupc++
	$(STRIP) $@

tsTest: tsTest.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o tsTest -DSTANDALONE -Xlinker -Map -Xlinker tsTest.map tsTest.cpp pollHandler.o $(LIBS) -lCurlCache -lstdc++
	$(STRIP) $@

mpegDecodeMain.o: mpegDecode.o
	$(CC) -fno-rtti $(HARDWARE_TYPE) -D__MODULETEST__ -c $(IFLAGS) -O2 -o mpegDecodeMain.o mpegDecode.cpp

mpegDecode: mpegDecodeMain.o $(LIB) $(SM501LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mpegDecode mpegDecodeMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lSM501 -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle mpegDecode | sort >mpegDecode.map
	$(STRIP) $@

mpegRxUDPMain.o: mpegRxUDP.o
	$(CC) -fno-rtti $(HARDWARE_TYPE) -DMODULETEST -c $(IFLAGS) -O2 -o mpegRxUDPMain.o mpegRxUDP.cpp

mpegRxUDP: mpegRxUDPMain.o $(LIB) $(SM501LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o mpegRxUDP mpegRxUDPMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lSM501 -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle mpegRxUDP | sort >mpegRxUDP.map
	$(STRIP) $@

usbPostscriptMain.o: usbPostscript.cpp
	$(CC) -fno-rtti $(HARDWARE_TYPE) -DMODULETEST -c $(IFLAGS) -O2 -o usbPostscriptMain.o usbPostscript.cpp

usbPostscript: usbPostscriptMain.o $(LIB) $(SM501LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o usbPostscript usbPostscriptMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle usbPostscript | sort >usbPostscript.map
	$(STRIP) $@

usblpPollMain.o: usblpPoll.cpp
	$(CC) -fno-rtti $(HARDWARE_TYPE) -DMODULETEST_USBLP -c $(IFLAGS) -O2 -o usblpPollMain.o usblpPoll.cpp

usblpPoll: usblpPollMain.o $(LIB) $(SM501LIB) Makefile $(LIBBDGRAPH) $(LIBRARYREFS)
	$(CC) $(HARDWARE_TYPE) -D_REENTRANT=1 -o usblpPoll usblpPollMain.o $(LIBS) -lCurlCache -L./bdGraph -lbdGraph -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lCurlCache $(MPEG2LIBS) -lSM501 -lflash -lusb -lpthread -lm -lz -lssl -lcrypto -ldl
	arm-linux-nm --demangle usblpPoll | sort >usblpPoll.map
	$(STRIP) $@


hexDump: hexDump.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o hexDump -D__STANDALONE__ -Xlinker -Map -Xlinker hexDump.map hexDump.cpp $(LIBS) -ljpeg -lcrypto -lCurlCache -lpthread -lstdc++
	$(STRIP) $@

vsync: vsync.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o vsync -D__STANDALONE__ -Xlinker -Map -Xlinker vsync.map vsync.cpp $(LIBS) -ljpeg -lcrypto -lCurlCache -lpthread -lstdc++
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
      $(LIBS) -lCurlCache -ljpeg -lungif -lpng -lz -lstdc++ 
	$(STRIP) $@

anigifMain.o: imgGIF.cpp
	$(CC) -o $@ -fno-rtti -Wall -Wno-invalid-offsetof -DSTANDALONE=1 $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

ANIGIFOBJS = anigifMain.o image.o memFile.o fbDev.o imgJPEG.o imgPNG.o
aniGIF: $(ANIGIFOBJS) Makefile 
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti \
      -o $@ -D__STANDALONE__ -Xlinker -Map \
      -Xlinker aniGIF.map \
      $(IMGFILEOBJS) \
      $(LIBS) -lCurlCache -ljpeg -lungif -lpng -lz -lstdc++ 
	$(STRIP) $@

imgTransparentMain.o: imgTransparent.cpp
	$(CC) -o $@ -fno-rtti -Wall -Wno-invalid-offsetof -DMODULETEST=1 $(HARDWARE_TYPE) $(KERNEL_VER) -D_REENTRANT=1 -DTSINPUTAPI=$(TSINPUTFLAG) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

imgTransparent: imgTransparentMain.o Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o imgTransparent -Xlinker -Map -Xlinker imgTransparent.map imgTransparentMain.o $(LIBS) -lCurlCache -ljpeg -lpng -lungif -lz -lstdc++
	$(STRIP) $@

mpegSendUDP: mpegSendUDP.cpp mpegUDP.h mpegPS.cpp mpegPS.h mpegStream.cpp mpegStream.h
	gcc -o mpegSendUDP mpegSendUDP.cpp memFile.cpp mpegPS.cpp mpegStream.cpp -lsupc++

ffmpeg_test: ffmpeg_test.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -ggdb -fno-rtti -o ffmpeg_test -D__STANDALONE__ -Xlinker -Map -Xlinker ffmpeg_test.map ffmpeg_test.cpp $(LIBS) -lSM501 -lCurlCache -ljpeg -lcrypto -lpthread -lstdc++
#	$(STRIP) $@

xvidToZip: xvidToZip.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o xvidToZip -D__STANDALONE__ -Xlinker -Map -Xlinker xvidToZip.map xvidToZip.cpp $(LIBS) -lSM501 -lCurlCache -ljpeg -lcrypto -lpthread -lstdc++
	$(STRIP) $@

i2cwrite: i2cwrite.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o i2cwrite -D__STANDALONE__ -Xlinker -Map -Xlinker i2cwrite.map i2cwrite.cpp $(LIBS) -lSM501 -lCurlCache -ljpeg -lcrypto -lpthread -lstdc++
	$(STRIP) $@

i2cwriteread: i2cwriteread.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o i2cwriteread -D__STANDALONE__ -Xlinker -Map -Xlinker i2cwriteread.map i2cwriteread.cpp $(LIBS) -lSM501 -lCurlCache -ljpeg -lcrypto -lpthread -lstdc++
	$(STRIP) $@

i2cread: i2cread.cpp Makefile $(LIB)
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -fno-rtti -o i2cread -D__STANDALONE__ -Xlinker -Map -Xlinker i2cread.map i2cread.cpp $(LIBS) -lSM501 -lCurlCache -ljpeg -lcrypto -lpthread -lstdc++
	$(STRIP) $@

imgMonoToPCL: imgMonoToPCL.cpp Makefile
	$(CC) $(HARDWARE_TYPE) $(IFLAGS) -DMODULETEST=1 -fno-rtti -o imgMonoToPCL -Xlinker -Map -Xlinker imgMonoToPCL.map imgMonoToPCL.cpp image.cpp dither.cpp imgFile.cpp memFile.cpp imgPNG.cpp imgJPEG.cpp imgGIF.cpp $(LIBS) -ljpeg -lpng -lungif -lz -lbdGraph -lCurlCache -lpthread -lstdc++ 
	$(STRIP) $@

parsedURL: parsedURL.cpp Makefile
	$(CC) $(HARDWARE_TYPE) -DSTANDALONE $(IFLAGS) -DMODULETEST=1 -fno-rtti -o parsedURL -Xlinker -Map -Xlinker parsedURL.map parsedURL.cpp $(LIBS) -ljpeg -lpng -lungif -lz -lbdGraph -lCurlCache -lpthread -lstdc++ 
	$(STRIP) $@

#
# This will need additional setup for location of gcc static lib (for udivsi3)
#
#progFlash: start.o progFlash.o
#	$(LD) -o $@ start.o progFlash.o -L../install/lib/gcc-lib/arm-linux/2.95.3 -lgcc
#	$(STRIP) $@
#

all: $(LIB) curlGet dirTest urlTest jsExec ftRender ftDump madHeaders bc cbmGraph cbmStat jpegview flashVar daemonize

.PHONY: install-libs install-headers

shared-headers = \
   baudRate.h \
   bitmap.h \
   blockSig.h \
   config.h \
   ddtoul.h \
   debugPrint.h \
   dirByATime.h \
   dspOutSignal.h \
   fbCmdListSignal.h \
   fbDev.h \
   ftObjs.h \
   hexDump.h  \
   image.h \
   imgGIF.h \
   imgJPEG.h \
   imgPNG.h \
   jsData.h \
   macros.h \
   madDecode.h \
   madHeaders.h \
   mediaQueue.h \
   memFile.h \
   mpegPS.h \
   mpegQueue.h \
   mpegStream.h \
   mtQueue.h \
   multiSignal.h \
   relativeURL.h \
   rtSignal.h \
   semClasses.h \
   serialSignal.h \
   setSerial.h \
   tickMs.h \
   ultoa.h \
   ultodd.h \
   urlFile.h \
   vsyncSignal.h \
   yuvSignal.h

ifeq (y,$(KERNEL_FB_SM501))
shared-headers += \
   asyncScreenObj.h \
   fbImage.h \
   fbcCircular.h \
   fbcHideable.h \
   fbcMoveHide.h \
   fbCmdBlt.h \
   fbCmdClear.h \
   fbCmdFinish.h \
   fbCmdList.h \
   fbCmdWait.h \
   fbMem.h \
   odomDigit.h \
   odomGraphics.h \
   odomMode.h \
   odomValue2.h \
   odomVideo.h \
   sm501alpha.h
endif

install-headers:
	cp -f -v $(shared-headers) $(INSTALL_ROOT)/include

install-bin: all
	$(OBJCOPY) -S -v jsExec   $(INSTALL_ROOT)/bin/jsExec
	$(OBJCOPY) -S -v jpegview $(INSTALL_ROOT)/bin/jpegview
	$(OBJCOPY) -S -v flashVar $(INSTALL_ROOT)/bin/flashVar
	$(OBJCOPY) -S -v flashVar $(INSTALL_ROOT)/bin/daemonize
	cp wget $(INSTALL_ROOT)/bin

install: install-bin install-headers
all: curlGet dirTest urlTest jsExec ftRender ftDump madHeaders bc cbmGraph \
        cbmStat jpegview flashVar daemonize $(ODOMLIB) $(SM501LIB)

clean:
	rm -f *.o *.a *.map *.lst *.sym *.prestrip bc curlGet dirTest \
	   urlTest jsExec testJS ftRender madHeaders backtrace \
      cbmImage cbmGraph cbmReset cbmStat ffPlay ffTest jpegview \
      mpeg2mp3 pixmanTest mpegQueue mpegDecode \
	   pxaregs flashVar ftDump jsData setBaud serialSignal \
	   serialCounts fbCmdClear sm501reg sm501dump imgFile \
      serialTouch odomValue fbcCircular fbcHideable odomValue2 \
      sm501alpha fbcMoveHide ttySignal \
      pngToPS usbPostscript usblpPoll imageInfo ascii85 imageToPS \
      $(LIB) $(ODOMLIB) $(SM501LIB)
