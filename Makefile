# 
# Makefile for curlCache library and utility programs
# 
ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   STRIP=arm-linux-strip
   LIBS=-L /usr/local/arm/2.95.3/arm-linux/lib
   IFLAGS=-I/usr/local/arm/2.95.3/arm-linux/include/freetype2 
else
   CC=gcc
   LIBS=-L /usr/local/lib
   IFLAGS=-I/usr/include/freetype2 
   STRIP=strip
endif

%.o : %.cpp
	$(CC) -c -DXP_UNIX=1 $(IFLAGS) -O2 $<

%.o : ../boundary1/%.cpp
	$(CC) -c -DXP_UNIX=1 -O2 $<
   
curlCacheMain.o: curlCache.h curlCache.cpp Makefile
	$(CC) -c -o curlCacheMain.o -O2 -DSTANDALONE curlCache.cpp
curlCache: curlCacheMain.o dirByATime.o relativeURL.o ultoa.o memFile.o Makefile
	$(CC) -o curlCache curlCacheMain.o dirByATime.o relativeURL.o ultoa.o memFile.o $(LIBS) -lcurl -lstdc++ -lz

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) -c -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl

curlGetMain.o: curlCache.h curlGet.h curlGet.cpp Makefile
	$(CC) -c -o curlGetMain.o -O2 -DSTANDALONE curlGet.cpp

curlGet: curlGetMain.o curlCache.o relativeURL.o ultoa.o dirByATime.o memFile.o Makefile
	$(CC) -o curlGet curlGetMain.o curlCache.o relativeURL.o ultoa.o dirByATime.o memFile.o $(LIBS) -lcurl -lstdc++ -lz

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) -c -o urlTest.o -O2 -DSTANDALONE -I ../zlib urlFile.cpp

urlTest: urlTest.o curlCache.o relativeURL.o ultoa.o dirByATime.o
	$(CC) -o urlTest urlTest.o curlCache.o relativeURL.o ultoa.o dirByATime.o $(LIBS) -lstdc++ -lcurl -lz

mp3Play: mp3Play.o curlCache.o dirByATime.o relativeURL.o ultoa.o 
	$(CC) -o mp3Play mp3Play.o curlCache.o dirByATime.o relativeURL.o ultoa.o $(LIBS) -lstdc++ -lcurl -lz -lmad
	$(STRIP) mp3Play

testJS.o: testJS.cpp Makefile
	$(CC) -c -o testJS.o -DXP_UNIX=1 -I ../ testJS.cpp

testJS: testJS.o childProcess.o urlFile.o curlCache.o jsCurl.o jsScreen.o jsImage.o dirByATime.o fbDev.o hexDump.o ultoa.o jsText.o jsMP3.o jsURL.o relativeURL.o Makefile
	$(CC) -o testJS testJS.o childProcess.o urlFile.o curlCache.o jsCurl.o jsScreen.o jsImage.o jsText.o jsMP3.o jsURL.o relativeURL.o dirByATime.o fbDev.o \
      hexDump.o ultoa.o \
      $(LIBS) -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lfreetype -lm -lz
	$(STRIP) testJS

ifneq (,$(findstring arm, $(CC)))
   all: curlGet dirTest urlTest testJS mp3Play
else
   all: curlCache curlGet dirTest urlTest testJS mp3Play
endif


clean:
	rm -f *.o curlCache curlGet dirTest urlTest testJS