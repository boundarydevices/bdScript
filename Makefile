# 
# Makefile for curlCache library and utility programs
# 
OBJS = childProcess.o codeQueue.o curlCache.o curlCacheMain.o curlThread.o \
       dirByATime.o fbDev.o hexDump.o imgGIF.o imgPNG.o imgJPEG.o \
       jsCurl.o jsGlobals.o jsHyperlink.o jsImage.o jsMP3.o jsProc.o \
       jsScreen.o jsText.o jsTimer.o jsURL.o \
       memFile.o relativeURL.o ultoa.o urlFile.o 
LIB = curlCacheLib.a

ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   AR=arm-linux-ar
   STRIP=arm-linux-strip
   LIBS=-L /usr/local/arm/2.95.3/arm-linux/lib
   IFLAGS=-I/usr/local/arm/2.95.3/arm-linux/include/freetype2 
else
   CC=gcc
   AR=ar
   LIBS=-L /usr/local/lib
   IFLAGS=-I/usr/include/freetype2 
   STRIP=strip
endif

%.o : %.cpp Makefile
	$(CC) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

%.o : ../boundary1/%.cpp Makefile
	$(CC) -c -DXP_UNIX=1 -O2 $<
   
$(LIB): Makefile $(OBJS)
	$(AR) r $(LIB) $(OBJS)

curlCacheMain.o: curlCache.h curlCache.cpp Makefile
	$(CC) -c -o curlCacheMain.o -O2 -DSTANDALONE curlCache.cpp
curlCache: curlCacheMain.o $(LIB)
	$(CC) -o curlCache curlCacheMain.o $(LIB) $(LIBS) -lcurl -lstdc++ -lz

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) -c -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl

curlGetMain.o: curlCache.h curlGet.h curlGet.cpp Makefile
	$(CC) -c -o curlGetMain.o -O2 -DSTANDALONE curlGet.cpp

curlGet: curlGetMain.o $(LIB) 
	$(CC) -o curlGet curlGetMain.o $(LIB) $(LIBS) -lcurl -lstdc++ -lz

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) -c -o urlTest.o -O2 -DSTANDALONE -I ../zlib urlFile.cpp

urlTest: urlTest.o $(LIB) 
	$(CC) -o urlTest urlTest.o $(LIB) $(LIBS) -lstdc++ -lcurl -lz

mp3Play: mp3Play.o $(LIB) 
	$(CC) -o mp3Play mp3Play.o $(LIB) $(LIBS) -lstdc++ -lcurl -lz -lmad
	$(STRIP) mp3Play

testJS.o: testJS.cpp Makefile
	$(CC) -c -o testJS.o -DXP_UNIX=1 -I ../ testJS.cpp

testJS: testJS.o $(LIB)
	$(CC) -o testJS testJS.o $(LIB) $(LIBS) -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	$(STRIP) testJS

testEvents: testEvents.o $(LIB)
	$(CC) -o testEvents testEvents.o $(LIB) $(LIBS) -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm testEvents >testEvents.map
	$(STRIP) testEvents

ftRender.o: ftObjs.h ftObjs.cpp Makefile
	$(CC) -c -o ftRender.o -O2 -D__MODULETEST__ $(IFLAGS) ftObjs.cpp

ftRender: ftRender.o $(LIB)
	$(CC) -o ftRender ftRender.o $(LIB) $(LIBS) -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lpthread -lm -lz
	arm-linux-nm ftRender >ftRender.map
	$(STRIP) ftRender

all: curlCache curlGet dirTest urlTest testEvents testJS mp3Play

clean:
	rm -f *.o *.a curlCache curlGet dirTest urlTest testEvents testJS mp3Play