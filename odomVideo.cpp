/*
 * Module odomVideo.cpp
 *
 * This module defines the methods of the odomVideo_t
 * class as declared in odomVideo.h
 *
 *
 * Change History : 
 *
 * $Log: odomVideo.cpp,v $
 * Revision 1.12  2007-08-23 00:31:55  ericn
 * -use mplayer
 *
 * Revision 1.11  2006/10/10 20:50:16  ericn
 * -use fds, not playlist
 *
 * Revision 1.10  2006/09/17 15:54:02  ericn
 * -use pipeline for mpeg file reads
 *
 * Revision 1.9  2006/09/10 01:15:43  ericn
 * -trace events
 *
 * Revision 1.8  2006/09/05 02:20:45  ericn
 * -don't spin on eof
 *
 * Revision 1.7  2006/09/04 16:42:22  ericn
 * -add audio playback call
 *
 * Revision 1.6  2006/09/04 15:17:19  ericn
 * -add audio
 *
 * Revision 1.5  2006/09/01 22:54:15  ericn
 * -change buffer time to ms, decode up to 4 frames per poll
 *
 * Revision 1.4  2006/09/01 20:24:17  ericn
 * -keep track of PTS stats
 *
 * Revision 1.3  2006/09/01 01:02:27  ericn
 * -use mpegQueue for odomVideo
 *
 * Revision 1.2  2006/08/22 15:50:10  ericn
 * -remove GOP pseudo-pic from mpegDecode
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomVideo.h"

#ifndef CONFIG_MPLAYER
   #include <stdio.h>
   #include <stdlib.h>
   #include <unistd.h>
   #include "tickMs.h"
   #include <sys/stat.h>
   #include <linux/soundcard.h>
   // #define LOGTRACES
   #include "logTraces.h"
   
   static traceSource_t traceVideo( "odomVideo" );
   
   #define INITTING 1
   #define PULLING  2 
   #define PUTTING  4
   
   unsigned long volatile flags_ = 0 ;
   
   static FILE *procFile( char *cmdLine, unsigned cmdSize, char const *fileName )
   {
      snprintf( cmdLine, cmdSize, "cat %s", fileName );
      return popen( cmdLine, "r" );
   }
   
   odomVideo_t::odomVideo_t( 
      int                fdDsp,
      int                fdYUV,
      char const        *fileName,
      rectangle_t const &outRect )
      : fIn_( procFile(cmdLine_,sizeof(cmdLine_), fileName ) )
      , stream_( fileno(fIn_) )
      , outQueue_( fdDsp,
                   fdYUV,
                   2000,
                   outRect )
      , outRect_( outRect )
      , bytesPerPicture_( 0 )
      , firstPTS_( 0LL )
      , lastPTS_( 0LL )
      , start_( 0LL )
   {
      if( 0 == fIn_ )fprintf( stderr, "%s: %m", fileName );
      TRACEINCR(traceVideo);
   }
   
   odomVideo_t::~odomVideo_t( void )
   {
      if( fIn_ )
         pclose( fIn_ );
      TRACEDECR(traceVideo);
   }
   
   bool odomVideo_t::startPlayback( void )
   {
      if( !initialized() )
         return false ;
   
      bool firstVideo = true ;
      while( outQueue_.msToBuffer() > outQueue_.msVideoQueued() ){
         unsigned char const *frame ;
         unsigned             frameLen ;
         
         CodecType      type ;
         CodecID        codecId ;
         long long      pts ;
         unsigned char  streamId ;
      
         if( stream_.getFrame( frame, frameLen, pts, streamId, type, codecId ) ){
            bool isVideo = ( CODEC_TYPE_VIDEO == type );
            long long ms = outQueue_.ptsToMs(pts);
            if( isVideo ){
               if( 0LL != pts ){
                  if( 0LL == firstPTS_ )
                     firstPTS_ = pts ;
                  lastPTS_ = pts ;
               }
               outQueue_.feedVideo( frame, frameLen, firstVideo, ms );
               firstVideo = false ;
            }
            else {
               outQueue_.feedAudio( frame, frameLen, false, ms );
            }
         }
         else
            return false ;
      }
   
      outQueue_.startPlayback();
      start_ = tickMs();
   
      return true ;
   }
   
   bool odomVideo_t::playback( void )
   {
      unsigned numPictures = 0 ;
      
      outQueue_.playAudio(tickMs());
   
      while( !completed() 
             && 
             ( 4 > numPictures ) 
             && 
             ( outQueue_.msVideoQueued() < outQueue_.highWater_ms() ) )
      {
         unsigned char const *frame ;
         unsigned             frameLen ;
         
         CodecType      type ;
         CodecID        codecId ;
         long long      pts ;
         unsigned char  streamId ;
      
         if( stream_.getFrame( frame, frameLen, pts, streamId, type, codecId ) ){
            bool isVideo = ( CODEC_TYPE_VIDEO == type );
            if( isVideo ){
               outQueue_.feedVideo( frame, frameLen, false, outQueue_.ptsToMs(pts) );
               ++numPictures ;
            }
            else
               outQueue_.feedAudio( frame, frameLen, false, outQueue_.ptsToMs(pts) );
         }
         else
            break ;
      }
      
      outQueue_.playAudio(tickMs());
      return !completed();
   }
   
   bool odomVideo_t::completed( void )
   {
      if( ( ( 0 == fIn_ )
            || 
            stream_.eof() )
          &&
          outQueue_.isEmpty() ){
         printf( "video completed after %lu ms\n"
                 , (unsigned long)( tickMs() - start_ ) );
         dump();
   
         return true ;
      }
      else
         return false ;
   }
   
   void odomVideo_t::doOutput( void ){
      long long const now = tickMs();
      outQueue_.playVideo( now );
   }
   
   void odomVideo_t::doAudio( void ){
      long long const now = tickMs();
      outQueue_.playAudio( now );
   }
   
   void odomVideo_t::dump( void )
   {
      outQueue_.dumpStats();
      printf( "PTS range: %lld..%lld (%lu)\n"
            ,  firstPTS_
            ,  lastPTS_
            ,  (unsigned long)(lastPTS_-firstPTS_)
            );
   }
#else

#define NUMINPIPES 2

class watchForClose_t : public pollHandler_t {
public:
   watchForClose_t( int fd, pollHandlerSet_t &set, odomVideo_t *video )
      : pollHandler_t( fd, set )
      , video_( video )
   {
      setMask( POLLIN|POLLERR|POLLHUP );
      set.add( *this );
   }
   virtual ~watchForClose_t( void ){}

   virtual void onDataAvail( void );     // POLLIN
   virtual void onError( void );     // POLLIN
   virtual void onHUP( void );     // POLLIN

private:
   odomVideo_t *video_ ;
};

void watchForClose_t :: onDataAvail( void )
{
   char inBuf[256];
   int numRead ; 
   while( 0 < (numRead = read( fd_, inBuf, sizeof(inBuf) )) )
   {
      printf( "rx <" ); fwrite( inBuf, numRead, 1, stdout ); printf( ">\n" );
   }
   if( 0 == numRead )
      onHUP();
}

void watchForClose_t :: onError( void )         // POLLERR
{
   printf( "<Error>\n" );
   video_->closed();
}

void watchForClose_t :: onHUP( void )           // POLLHUP
{
   printf( "<HUP>\n" );
   close();
   video_->closed();
}

odomVideo_t::odomVideo_t
   ( char const        *fileName,
     rectangle_t const &outRect,
     char const       **extraParams,
     unsigned           numExtraParams )
   : player_( 
        new mplayerWrap_t( 
               fileName, 
               outRect.xLeft_, 
               outRect.yTop_, 
               outRect.width_, 
               outRect.height_,
               extraParams,
               numExtraParams ) )
   , closed_( 0 > player_->pid() )
   , pipeHandlers_( new pollHandler_t *[NUMINPIPES] )
{
   if( !closed_ ){
      pipeHandlers_[0] = new watchForClose_t( player_->child_stdout(), handlers_, this );
      pipeHandlers_[1] = new watchForClose_t( player_->child_stderr(), handlers_, this );
   }
}

odomVideo_t::~odomVideo_t( void )
{
   for( unsigned i = 0 ; i < NUMINPIPES ; i++ ){
      if( pipeHandlers_[i] )
         delete pipeHandlers_[i];
   }
   if( pipeHandlers_ ){
      delete [] pipeHandlers_ ;
   }
   if( player_ ){
      delete player_ ;
      player_ = 0 ;
   }
}

// start playback (opens YUV) - returns true if successful
bool odomVideo_t::startPlayback( void )
{
   return initialized();
}

bool odomVideo_t::completed( void )
{
   handlers_.poll(0);
   return closed_ ;
}

#endif
