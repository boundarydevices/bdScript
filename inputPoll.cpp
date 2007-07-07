/*
 * Module inputPoll.cpp
 *
 * This module defines the inputPoll_t class, which 
 * reads and delivers input events to an application
 *
 * Change History : 
 *
 * $Log: inputPoll.cpp,v $
 * Revision 1.1  2007-07-07 00:25:52  ericn
 * -[bdScript] added mouse cursor and input handling
 *
 *
 * Copyright Boundary Devices, Inc. 2007
 */


#include "inputPoll.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/input.h>

inputPoll_t::inputPoll_t( pollHandlerSet_t &set, char const *devName )
	: pollHandler_t( open( devName, O_RDWR ), set )
	, fileName_( strdup(devName) )
{
	if( isOpen() )
	{
		fcntl( fd_, F_SETFD, FD_CLOEXEC );
		fcntl( fd_, F_SETFL, O_NONBLOCK );
		setMask( POLLIN );
		set.add( *this );
	}
	else
		perror( devName );
}

inputPoll_t::~inputPoll_t( void )
{
	if( fileName_ ){
		free( (char *)fileName_ );
		fileName_ = 0 ;
	}
}
   
static char const * const eventTypeNames[] = {
	"EV_SYN"
,	"EV_KEY"
,	"EV_REL"
,	"EV_ABS"
,	"EV_MSC"
};

// override this to handle keystrokes, button presses and the like
void inputPoll_t::onData( struct input_event const &event )
{
	printf( "%s: type %d (%s), code 0x%x, value %d\n",
		fileName_,
		event.type, 
		event.type <= EV_MSC ? eventTypeNames[event.type] : "other",
		event.code,
		event.value );
}

#define MAX_EVENTS_PER_READ 16
void inputPoll_t::onDataAvail( void )
{
	struct input_event events[MAX_EVENTS_PER_READ];
	do {
		int bytesRead = read( fd_, events, sizeof(events) );
		if( 0 < bytesRead ){
		    int itemsRead = bytesRead / sizeof(events[0]);
		    unsigned leftover = bytesRead % sizeof(events[0]);
		    if( leftover ){
			    unsigned left = sizeof(events[0])-leftover ;
			    bytesRead = read( fd_, ((char *)events)+bytesRead, left );
			    if( bytesRead == left ){
				    itemsRead++ ;
			    }
		    } // try to read tail-end
		    for( int i = 0 ; i < itemsRead ; i++ ){
			    onData(events[i]);
		    }
		}
		else
			break ;
	} while( 1 );
}



#ifdef INPUT_STANDALONE

int main( int argc, char const *const argv[] )
{
	pollHandlerSet_t handlers ;
	inputPoll_t **pollers = new inputPoll_t *[argc-1];
	for( int arg = 1 ; arg < argc ; arg++ ){
		pollers[arg-1] = new inputPoll_t( handlers, argv[arg] );
	}
      
	int iterations = 0 ;
	while( 1 ){
		handlers.poll( -1 );
	}

	return 0 ;
}

#endif
