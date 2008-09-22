/*
 * Module inputDevs.cpp
 *
 * This module defines ...
 *
 *
 * Change History : 
 *
 * $Log: inputDevs.cpp,v $
 * Revision 1.1  2008-09-22 19:07:52  ericn
 * [touch] Use input devices for mouse, touch screen
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include "inputDevs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/input.h>

static char const INPUTPROCFILE[] = { "/proc/bus/input/devices" };

inputDevs_t::inputDevs_t()
	: count_(0)
	, info_(0)
{
	FILE *fIn = fopen( INPUTPROCFILE, "r" );
	if(fIn){
		unsigned numDevs = 0 ;
		char inBuf[256];
		while(fgets(inBuf,sizeof(inBuf),fIn)){
			if( ('I' == inBuf[0]) && (':' == inBuf[1])){
				++numDevs ;
			}
		}
		fclose(fIn);
		info_ = new struct info_t[numDevs];

		bool haveIdx = false ;
		unsigned idx = 0 ;
		unsigned events = 0 ;
		fIn = fopen( INPUTPROCFILE, "r" );
		while(fgets(inBuf,sizeof(inBuf),fIn)){
			char c = inBuf[0];
			if( (0!=c) && (':' == inBuf[1])){
				switch(c){
					case 'I':
						if(haveIdx){
							addDev(idx,events);
							haveIdx = false ;
                                                }
						break;
					case 'B':
						if( 0==memcmp(inBuf+2," EV=",4)){
							events=strtoul(inBuf+6,0,16);
						}
						break;
					case 'H': {
						char const *event=strstr(inBuf,"event");
						if(event){
							idx=strtoul(event+5,0,10);
							haveIdx=true ;
						}
						break;
					}
				}
			}
		}
		fclose(fIn);
		if(haveIdx){
			addDev(idx,events);
			haveIdx = false ;
		}
	}
	else
		perror( INPUTPROCFILE );
}

inputDevs_t::~inputDevs_t()
{
	if( info_ )
		delete [] info_ ;
}

bool inputDevs_t::findFirst( type_e type, unsigned &evIdx )
{
   for( unsigned i = 0 ; i < count(); i++ ){
      type_e t ;
      getInfo(i,t,evIdx);
      if( t == type ){
         return true ;
      }
   }
   return false ;
}

static char const * const typeNames_[] = {
	"mouse",
	"touchscreen",
	"keyboard"
};

char const *inputDevs_t::typeName(type_e type){
	assert((unsigned)type < ((sizeof(typeNames_)/sizeof(typeNames_[0]))));
	return typeNames_[type];
}

void inputDevs_t::addDev( unsigned evidx, unsigned events )
{
	assert(info_);
	struct info_t &info = info_[count_];
	info.evidx = evidx ;
	if((events&(1<<EV_ABS)) && (0==(events&(1<<EV_REL)))){
                info.type = TOUCHSCREEN ;
	} else if(events&(1<<EV_REL)){
		info.type = MOUSE ;
	} else if(events&(1<<EV_KEY)){
		info.type = KEYBOARD ;
	} else {
		fprintf( stderr, "Unknown input event mask %x\n", events );
		return ;
	}
	++count_ ;
}

#ifdef INPUTDEVS_STANDALONE
int main( void ){
	inputDevs_t devs ;

	printf( "%u devices\n", devs.count() );
	for( unsigned i = 0 ; i < devs.count(); i++ ){
		unsigned evIdx ;
		inputDevs_t::type_e type ;
		devs.getInfo(i, type, evIdx);
		printf( "devs[%u]: event%u --> %s\n", i, evIdx, devs.typeName(type));
	}
	return 0 ;
}
#endif
