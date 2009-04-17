/*
 * Module circularLog.cpp
 *
 * This module defines the methods of the circularLog_t
 * class as declared in circularLog.h
 *
 *
 * Change History : 
 *
 * $Log: circularLog.cpp,v $
 * Revision 1.2  2009-04-17 00:00:03  ericn
 * [circularLog] Close stdin on read() == 0
 *
 * Revision 1.1  2009-04-02 20:32:53  ericn
 * [circularLog] Added module and test program
 *
 *
 * Copyright Boundary Devices, Inc. 2009
 */

#include "circularLog.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

circularLog_t::circularLog_t( char const *fileName, unsigned maxLen )
	: fileName_( strdup(fileName) )
	, chunkSize_( maxLen / 4 )
	, chunkIdx_( 0 )
	, curChunkLen_( 0 )
	, fd_( -1 )
	, wrapped_( false )
{
	for( unsigned i = 0 ; i < NUMDATACHUNKS ; i++ ){
		dataChunks_[ i ] = new char [ chunkSize_ ];
		memset(dataChunks_[i],0,chunkSize_);
	}
	fd_ = open( fileName_, O_RDWR|O_CREAT );
	if( isOpen() ){
                ftruncate(fd_,0);
	}
}

circularLog_t::~circularLog_t( void )
{
	if( isOpen() ){
		close(fd_);
		fd_ = -1 ;
	}
	free((void *)fileName_);
	for( unsigned i = 0 ; i < NUMDATACHUNKS ; i++ ){
		free( dataChunks_[ i ] );
	}
}

void circularLog_t::write( void const *data, unsigned len )
{
	if( !isOpen() ){
		return ;
	}
	while( 0 < len ){
		unsigned chunkIdx = chunkIdx_ % NUMDATACHUNKS ;
		unsigned space = chunkSize_ - curChunkLen_ ;
		unsigned thisLen = space > len ? len : space ;
		memcpy( dataChunks_[chunkIdx]+curChunkLen_, data, thisLen );
		::write( fd_, data, thisLen );
		len -= thisLen ;
		if( 0 < len ){
			data = (char *)data + thisLen ;
			curChunkLen_ = 0 ;
			++chunkIdx_ ;
			wrapped_ = wrapped_ || (chunkIdx_ >= NUMDATACHUNKS);
			if( wrapped_ ){
				lseek(fd_,0,SEEK_SET);
				unsigned idx = (chunkIdx + 1)%NUMDATACHUNKS ;
				// printf( "moving chunks (%u..%u]\n", idx, chunkIdx );
				while( idx != chunkIdx ){
                                        idx = (idx + 1)%NUMDATACHUNKS ;
					::write(fd_,dataChunks_[idx],chunkSize_);
				}
				ftruncate(fd_,lseek(fd_,0,SEEK_CUR));
			} // move stuff
		}
		else
			curChunkLen_ += thisLen ;
	}
}


#ifdef STANDALONE_CIRCULARLOG
#include "pipeProcess.h"
#include "pollHandler.h"
#include <signal.h>
#include "rawKbd.h"

static bool die = false ;
static void ctrlcHandler( int signo )
{
   printf( "<ctrl-c>\n" );
   die = true ;
}

class logHandler_t : public pollHandler_t {
public:
	logHandler_t( int fd, pollHandlerSet_t &set, circularLog_t &log )
		: pollHandler_t( fd, set )
		, log_(log)
	{
		setMask( POLLIN|POLLERR|POLLHUP );
		set.add( *this );
	}
	virtual ~logHandler_t( void ){}
	
	virtual void onDataAvail( void );     // POLLIN
	virtual void onError( void );     // POLLIN
	virtual void onHUP( void );     // POLLIN
private:
        circularLog_t &log_ ;
};

void logHandler_t :: onDataAvail( void )
{
   char inBuf[256];
   int numRead ; 
   while( 0 < (numRead = read( fd_, inBuf, sizeof(inBuf) )) )
   {
	   log_.write(inBuf,numRead);
	   write(1,inBuf,numRead);
   }
   if( 0 == numRead )
      onHUP();
}

void logHandler_t :: onError( void )         // POLLERR
{
   printf( "<Error>\n" );
   die = true ;
}

void logHandler_t :: onHUP( void )           // POLLHUP
{
   printf( "<HUP>\n" );
   close();
   die = true ;
}

class passthruHandler_t : public pollHandler_t {
public:
	passthruHandler_t( int fdReadFrom, pollHandlerSet_t &set, int fdWriteTo )
		: pollHandler_t( fdReadFrom, set )
		, fdWriteTo_(fdWriteTo)
	{
		setMask( POLLIN|POLLERR|POLLHUP );
		set.add( *this );
	}
	virtual ~passthruHandler_t( void ){}
	
	virtual void onDataAvail( void );     // POLLIN
	virtual void onError( void );     // POLLIN
	virtual void onHUP( void );     // POLLIN
private:
        int fdWriteTo_ ;
};

void passthruHandler_t::onDataAvail( void )
{
   char inBuf[256];
   int numRead ; 
   while( 0 < (numRead = read( fd_, inBuf, sizeof(inBuf) )) )
   {
	   int numWritten = ::write(fdWriteTo_,inBuf,numRead);
   }
   if( 0 == numRead ){
	   setMask(0);
	   close();
   }
}

void passthruHandler_t::onError( void )
{
	setMask(0);
	close();
}

void passthruHandler_t::onHUP( void )
{
	setMask(0);
	close();
}

int main( int argc, char const **argv )
{
	if( 4 <= argc ){
		unsigned maxSize = strtoul(argv[2],0,0);
		if( 0 == maxSize ){
			fprintf( stderr, "Invalid size: %s\n", argv[2] );
			return -1 ;
		}
		circularLog_t log( argv[1], maxSize );
		if( !log.isOpen() ){
			perror( argv[1] );
			return -1 ;
		}
		pipeProcess_t child( argv + 3 );
		if( 0 <= child.pid() ){
			signal( SIGINT, ctrlcHandler );

			pollHandlerSet_t handlers ;
			logHandler_t childOut( child.child_stdout(), handlers, log );
			logHandler_t childErr( child.child_stderr(), handlers, log );
			
//                        rawKbd_t kbd ;
                        passthruHandler_t childIn( 0, handlers, child.child_stdin() );

			signal( SIGINT, ctrlcHandler );
			
			while( childOut.isOpen() && !die ){
				if( !handlers.poll(500) ){
				}
				if( !childIn.isOpen() )
					handlers.removeMe(childIn);
			}
			
			if( childOut.isOpen() ){
				child.shutdown();
			}
			
			if( 0 < child.pid() ){
				int exitStat = child.wait();
			}
		}
		else
			perror(argv[3]);
	}
	else
		fprintf( stderr, "Usage: %s outFile maxSize program [args...]\n", argv[0] );
	return 0 ;
}

#endif
