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
	$(CC) -c -O2 $<
   
curlCacheMain.o: curlCache.h curlCache.cpp Makefile
	$(CC) -c -o curlCacheMain.o -O2 -DSTANDALONE curlCache.cpp
curlCache: curlCacheMain.o dirByATime.o Makefile
	$(CC) -o curlCache curlCacheMain.o dirByATime.o $(LIBS) -lcurl -lstdc++ 

dirTest.o: dirByATime.cpp dirByATime.h Makefile
	$(CC) -c -o dirTest.o -O2 -DSTANDALONE dirByATime.cpp

dirTest: Makefile dirTest.o
	$(CC) -o dirTest dirTest.o $(LIBS) -lstdc++ -lcurl

urlTest.o: urlFile.cpp urlFile.h Makefile
	$(CC) -c -o urlTest.o -O2 -DSTANDALONE -I ../zlib urlFile.cpp

urlTest: urlTest.o curlCache.o dirByATime.o
	$(CC) -o urlTest urlTest.o curlCache.o dirByATime.o $(LIBS) -lstdc++ -lcurl -lz

testJS.o: testJS.cpp Makefile
	$(CC) -c -o testJS.o -DXP_UNIX=1 -I ../ testJS.cpp

testJS: testJS.o urlFile.o curlCache.o dirByATime.o Makefile
	$(CC) -o testJS testJS.o urlFile.o curlCache.o dirByATime.o $(LIBS) -lstdc++ -ljs -lcurl -lm

all: curlCache dirTest urlTest testJS

clean:
	rm -f *.o curlCache dirTest urlTest testJS