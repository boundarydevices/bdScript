/*
 * Module odomStream.cpp
 *
 * This module defines the methods of the odomVideoStream_t
 * class as declared in odomStream.h
 *
 *
 * Change History : 
 *
 * $Log: odomStream.cpp,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomStream.h"

odomVideoStream_t::odomVideoStream_t
   ( odomPlaylist_t &playlist, // used to get video fd
     unsigned        port,
     unsigned        outx,
     unsigned        outy,
     unsigned        outw,
     unsigned        outh )
   : mpegRxUDP_t( port )
   , playlist_( playlist )
   , decoder_()
   , outQueue_()
   , outX_( outx )
   , outY_( outy )
   , outW_( outw )
   , outH_( outh )
   , bytesPerPicture_( 0 )
   , start_( 0LL )
   , state_( INIT )
{
}

odomVideoStream_t::~odomVideoStream_t( void )
{
}

void odomVideoStream_t::onNewFile( 
   char const *fileName,
   unsigned    fileNameLen )
{
   printf( "new file %s\n", fileName );
   state_ = INIT ;
}

static mpegDecoder_t::picType_e picTypesByState_[] = {
   mpegDecoder_t::ptNoB_e   // INIT       = 0,
,  mpegDecoder_t::ptNoB_e   // WAITIFRAME = 1,
,  mpegDecoder_t::ptAll_e   // PLAYBACK   = 2
};

void odomVideoStream_t::onRx( 
   bool                 isVideo,
   bool                 discont,
   unsigned char const *fData,
   unsigned             length,
   long long            pts,
   long long            dts )
{
   if( isVideo ){
      decoder_.feed( fData, length, pts );
      void const *picture = 0 ;
      unsigned temp_ref ;
      long long when = 0LL ;
      mpegDecoder_t::picType_e type ;

      unsigned mask = discont 
                    ? mpegDecoder_t::ptNoB_e
                    : mpegDecoder_t::ptAll_e ;

      while( decoder_.getPicture( picture, type, temp_ref, when, picTypesByState_[state_] & mask ) ){
//         printf( "state %d, picture: %d/%p\n", state_, type, picture );
         switch( state_ ){
            case INIT : {
               if( !decoder_.haveHeader() )
                  break ;
               
               outQueue_.init( decoder_.width(), decoder_.height() );
               bytesPerPicture_ = decoder_.width() * decoder_.height() * 2 ;
               printf( "%u x %u\n", decoder_.width(), decoder_.height() );

               state_ = WAITIFRAME ;
               // intentional fall-through
            }
            case WAITIFRAME: {
               if( mpegDecoder_t::ptI_e != type )
                  break ;
               
               // intentional fall-through
               state_ = PLAYBACK ;
            }
            
            case PLAYBACK: {
               if( mpegDecoder_t::ptD_e > type ){
                  unsigned char *frameMem = outQueue_.pullEmpty();
                  if( 0 == frameMem ){
                     fprintf( stderr, "Error pulling free frame\n" );
                  } else {
                     memcpy( frameMem, picture, bytesPerPicture_ );
                     outQueue_.putFull( frameMem, when );
                     if( outQueue_.isFull() && !outQueue_.started() )
                        outQueue_.start();
                  }
               }
            }
         }
      }
   }
}

void odomVideoStream_t::onEOF( 
   char const   *fileName,
   unsigned long totalBytes,
   unsigned long videoBytes,
   unsigned long audioBytes )
{
   printf( "eof(%s): %lu bytes (%lu video, %lu audio)\n", fileName, totalBytes, videoBytes, audioBytes );
   state_ = INIT ;
}

void odomVideoStream_t::doOutput( void )
{
   if( outQueue_.started() && !outQueue_.isEmpty() ){
      long long pts ;

      unsigned char *playbackFrame = outQueue_.pull( 0, &pts );

      if( playbackFrame ){
         int fd = playlist_.fdYUV( decoder_.width(),
                                   decoder_.height(),
                                   outX_,
                                   outY_,
                                   outW_,
                                   outH_ );
         if( 0 <= fd ){
            int const numWritten = write( fd, playbackFrame, bytesPerPicture_ );
            if( (unsigned)numWritten != bytesPerPicture_ )
               fprintf( stderr, "write %d of %lu bytes\n", numWritten, bytesPerPicture_ );
         }
         else
            fprintf( stderr, "Invalid fdYUV\n" );
         
         outQueue_.putEmpty( playbackFrame );
      }
   }
}

void odomVideoStream_t::dump( void )
{
}


