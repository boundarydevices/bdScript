# 
# Makefile for curlCache library and utility programs
# 
ifneq (,$(findstring arm, $(CC)))
   CC=arm-linux-gcc
   LIBS=-L /usr/local/arm/2.95.3/arm-linux/lib
else
   CC=gcc
endif

%.o : %.cpp
	$(CC) -c -DXP_UNIX=1 -O2 $<

%.o : ../boundary1/%.cpp
	$(CC) -c -DXP_UNIX=1 -O2 $<
   
curlCacheMain.o: curlCache.h curlCache.cpp Makefile
	$(CC) -c -o curlCacheMain.o -O2 -DSTANDALONE curlCache.cpp
curlCache: curlCacheMain.o dirByATime.o memFile.o Makefile
	$(CC) -o curlCache curlCacheMain.o dirByATime.o memFile.o $(LIBS) -lcurl -lstdc++ -lz

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) -c -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl

curlGetMain.o: curlCache.h curlGet.h curlGet.cpp Makefile
	$(CC) -c -o curlGetMain.o -O2 -DSTANDALONE curlGet.cpp

curlGet: curlGetMain.o curlCache.o dirByATime.o memFile.o Makefile
	$(CC) -o curlGet curlGetMain.o curlCache.o dirByATime.o memFile.o $(LIBS) -lcurl -lstdc++ -lz

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) -c -o urlTest.o -O2 -DSTANDALONE -I ../zlib urlFile.cpp

urlTest: urlTest.o curlCache.o dirByATime.o
	$(CC) -o urlTest urlTest.o curlCache.o dirByATime.o $(LIBS) -lstdc++ -lcurl -lz

testJS.o: testJS.cpp Makefile
	$(CC) -c -o testJS.o -DXP_UNIX=1 -I ../ testJS.cpp

testJS: testJS.o urlFile.o curlCache.o jsCurl.o jsImage.o dirByATime.o fbDev.o hexDump.o Makefile
	$(CC) -o testJS testJS.o urlFile.o curlCache.o jsCurl.o jsImage.o dirByATime.o fbDev.o hexDump.o \
      $(LIBS) -lstdc++ -ljs -lcurl -lpng -ljpeg -lungif -lm -lz

ifneq (,$(findstring arm, $(CC)))
   all: curlCache curlGet dirTest urlTest testJS
else
   all: curlCache curlGet dirTest urlTest testJS
endif


clean:
	rm -f *.o curlCache curlGet dirTest urlTest testJS