#
# Makefile for curlCache library and utility programs
#
OBJS = audioQueue.o childProcess.o codeQueue.o curlCache.o \
       curlThread.o ddtoul.o dirByATime.o fbDev.o ftObjs.o hexDump.o \
       hexDump.o imgGIF.o imgPNG.o imgJPEG.o \
       jsAlphaMap.o jsCurl.o jsGlobals.o jsHyperlink.o jsImage.o \
       jsMP3.o jsProc.o jsScreen.o jsText.o jsTimer.o jsTouch.o jsURL.o \
       jsVolume.o \
       madHeaders.o memFile.o relativeURL.o tsThread.o ultoa.o urlFile.o \
       ultodd.o
LIB = $(INSTALL_LIB)/libCurlCache.a

ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   STRIP=arm-linux-strip
   LIBS=-L./ -L$(TOOLS_LIB) -L$(INSTALL_LIB)
   IFLAGS=-I$(INSTALL_ROOT)/include -I$(INSTALL_ROOT)/include/freetype2
else
   CC=gcc
   AR=ar
   LIBS=-L/usr/local/lib -L./
   IFLAGS=-I/usr/include/freetype2
   STRIP=strip
endif

%.o : %.cpp Makefile
	$(CC) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)

curlCacheMain.o: curlCache.h curlCache.cpp Makefile
	$(CC) -c $(IFLAGS) -o curlCacheMain.o -O2 -DSTANDALONE curlCache.cpp
curlCache: curlCacheMain.o $(LIB)
	$(CC) -o curlCache curlCacheMain.o $(LIBS) -lCurlCache -lcurl -lstdc++ -lz

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) -c $(IFLAGS) -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl

curlGetMain.o: curlCache.h curlGet.h curlGet.cpp Makefile
	$(CC) -c $(IFLAGS) -o curlGetMain.o -O2 -DSTANDALONE curlGet.cpp

curlGet: curlGetMain.o $(LIB) 
	$(CC) -o curlGet curlGetMain.o $(LIBS) -lCurlCache -lcurl -lstdc++ -lz

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) -c $(IFLAGS) -o urlTest.o -O2 -DSTANDALONE urlFile.cpp

urlTest: urlTest.o $(LIB) 
	$(CC) -o urlTest urlTest.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz

mp3Play: mp3Play.o $(LIB) 
	$(CC) -o mp3Play mp3Play.o $(LIBS) -lCurlCache -lstdc++ -lcurl -lz -lmad
	$(STRIP) mp3Play

testJS.o: testJS.cpp Makefile
	$(CC) -c $(IFLAGS) -o testJS.o -DXP_UNIX=1 -I ../ testJS.cpp

testJS: testJS.o $(LIB)
	$(CC) -o testJS testJS.o $(LIBS) -lCurlCache -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lts -lpthread -lm -lz
	$(STRIP) testJS

testEvents: testEvents.o $(LIB)
	$(CC) -o testEvents testEvents.o $(LIBS) -lCurlCache -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lmad -lid3tag -lts -lpthread -lm -lz
	arm-linux-nm testEvents >testEvents.map
	$(STRIP) testEvents

ftRender.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) -c $(IFLAGS) -o ftRender.o -O2 -D__MODULETEST__ $(IFLAGS) ftObjs.cpp

ftRender: ftRender.o $(LIB)
	$(CC) -o ftRender ftRender.o $(LIBS) -lCurlCache -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftRender >ftRender.map
	$(STRIP) ftRender

tsThreadMain.o: tsThread.h tsThread.cpp Makefile
	$(CC) -c $(IFLAGS) -o tsThreadMain.o -O2 -D__MODULETEST__ $(IFLAGS) tsThread.cpp

tsThread: tsThreadMain.o Makefile $(LIB)
	$(CC) -o tsThread tsThreadMain.o $(LIBS) -lCurlCache -lstdc++ -lts -lpthread -lm
	arm-linux-nm tsThread >tsThread.map
	$(STRIP) tsThread

madHeadersMain.o: madHeaders.h madHeaders.cpp Makefile
	$(CC) -c $(IFLAGS) -o madHeadersMain.o -O2 -D__STANDALONE__ $(IFLAGS) madHeaders.cpp

madHeaders: madHeadersMain.o Makefile $(LIB)
	$(CC) -o madHeaders madHeadersMain.o $(LIBS) -lCurlCache -lstdc++ -lmad -lid3tag -lm -lz 
	arm-linux-nm madHeaders >madHeaders.map
	$(STRIP) madHeaders

all: curlCache curlGet dirTest urlTest testEvents testJS mp3Play ftRender tsTest tsThread madHeaders

.PHONY: install-libs install-headers

shared-headers = ddtoul.h dirByATime.h hexDump.h  \
   imgGIF.h imgJPEG.h imgPNG.h \
   macros.h madHeaders.h memFile.h \
   mtQueue.h relativeURL.h semaphore.h \
   ultoa.h ultodd.h urlFile.h

install-headers:
	cp $(shared-headers) $(INSTALL_ROOT)/include

install: install-libs install-headers

clean:
	rm -f *.o *.a curlCache curlGet dirTest urlTest testEvents testJS mp3Play ftRender tsTest tsThread madHeaders $(LIB)
