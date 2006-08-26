/*
 * Module odomPlaylist.cpp
 *
 * This module defines the methods of the odomPlaylist_t
 * class as declared in odomPlaylist.h
 *
 *
 * Change History : 
 *
 * $Log: odomPlaylist.cpp,v $
 * Revision 1.3  2006-08-26 16:05:44  ericn
 * -allow access to YUV without geometry, use new video stream interface
 *
 * Revision 1.2  2006/08/20 18:16:59  ericn
 * -fix speeling
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */


#include "odomPlaylist.h"
#include "odomCommand.h"
#include <stdio.h>
#include "imgFile.h"
#include "fbDev.h"
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/sm501yuv.h>
#include <linux/sm501-int.h>
#include "odomVideo.h"
#include "odomStream.h"

#define PLAYLISTMASK (odomPlaylist_t::MAXENTRIES-1)

static void playlist_vsync( void *param ){
   ((odomPlaylist_t *)param)->vsyncHandler();
}

odomPlaylist_t *lastPlaylistInst_ = 0 ;

odomPlaylist_t::odomPlaylist_t( void )
   : playing_( 0 )
   , add_( 0 )
   , take_( 0 )
   , fdYUV_(open( "/dev/yuv", O_WRONLY ))
   , yuvInW_(-1U)
   , yuvInH_(-1U)
   , yuvOutX_(-1U)
   , yuvOutY_(-1U)
   , yuvOutW_(-1U)
   , yuvOutH_(-1U)
{
   current_.type_ = PLAYLIST_NONE ;
   current_.repeat_ = false ;
   odometerSet_t::get().setHandler( playlist_vsync, this );

   lastPlaylistInst_ = this ;
}

odomPlaylist_t::~odomPlaylist_t( void )
{
}

void odomPlaylist_t::add( playlistEntry_t const &entry )
{
   unsigned numEntries = (add_ - take_) & PLAYLISTMASK ;
   if( numEntries != PLAYLISTMASK ){
      entries_[add_] = entry ;
      add_ = (add_+1) & PLAYLISTMASK ;

      //
      // if this is the first item added, it will be 
      // started in next call to play()
      //
   }
   else
      fprintf( stderr, "playlist overflow!\n" );
}

void odomPlaylist_t::stop( void )
{
   switch( current_.type_ ){
      case PLAYLIST_MPEG : {
         odomVideo_t *video = (odomVideo_t *)playing_ ;
         if( video ){
            playing_ = 0 ;
            delete video ;
         }
         break ;
      }
      case PLAYLIST_FLASH : {
         //
         // clean up flash resources
         //
         break ;
      }
      case PLAYLIST_STREAM : {
         odomVideoStream_t *stream = (odomVideoStream_t *)playing_ ;
         if( stream ){
            playing_ = 0 ;
            delete stream ;
         }
         break ;
      }
      default: {
      }
   }

   if( current_.repeat_ )
      add(current_);

   //
   // will be started in next call to play()
   //
   current_.type_ = PLAYLIST_NONE ; 
}

void odomPlaylist_t::purge( void )
{
   current_.repeat_ = false ;
   stop(); // kill current entry if needed
   take_ = add_ ;
}

void odomPlaylist_t::play( unsigned long syncCount )
{
   switch( current_.type_ ){
      case PLAYLIST_NONE : {
         next(syncCount);
         break ;
      }
      case PLAYLIST_STILLIMAGE : {
         closeYUV();
         long int diff = ( syncCount - ((unsigned long)playing_) );
         if( 0 <= diff ){
            stop();
         }
         break ;
      }
      case PLAYLIST_MPEG : {
         odomVideo_t *video = (odomVideo_t *)playing_ ;
         if( !video->playback() ){
            stop();
         }

         break ;
      }
      case PLAYLIST_FLASH : {
         fprintf( stderr, "Flash playback not yet implemented\n" );
         break ;
      }
      
      case PLAYLIST_CMD : {
         odomCmdInterp_t *interp = (odomCmdInterp_t *)current_.x_ ;
         if( interp ){
            printf( "issue command: %s\n", current_.fileName_ );
            interp->dispatch( current_.fileName_ );
         }
         stop();
         break ;
      }
      case PLAYLIST_STREAM : {
         odomVideoStream_t *stream = (odomVideoStream_t *)playing_ ;
         assert( stream );
         stream->poll();

         break ;
      }
   }
}

void odomPlaylist_t::next( unsigned long syncCount )
{
   if(add_ != take_){
      current_ = entries_[take_];

      take_ = (take_+1) & PLAYLISTMASK ;

      switch( current_.type_ ){
         case PLAYLIST_NONE : {
            // why are we here?
            fprintf( stderr, "null playlist entry\n" );
            break ;
         }

         case PLAYLIST_STILLIMAGE : {
            image_t image ;
            if( imageFromFile( current_.fileName_, image ) ){
               getFB().render( current_.x_, current_.y_, 
                               image.width_, image.height_, 
                               (unsigned short *)image.pixData_ );
               playing_ = (void *)( syncCount + current_.numTicks_ );
            }
            else
               fprintf( stderr, "%s: %m", current_.fileName_ );

            break ;
         }

         case PLAYLIST_MPEG : {
            printf( "-----> starting video %s\n", current_.fileName_ );
            odomVideo_t *video = new odomVideo_t( *this, 
                                                  current_.fileName_,
                                                  current_.x_,
                                                  current_.y_,
                                                  current_.w_,
                                                  current_.h_ );
            if( video->initialized() 
                &&
                video->startPlayback() ){
               playing_ = video ;
            }
            else {
               fprintf( stderr, "Error initializing video\n" );
               delete video ;
            }
            break ;
         }

         case PLAYLIST_CMD : {
            break ;
         }

         case PLAYLIST_FLASH : {
            fprintf( stderr, "Flash not yet implemented\n" );
            break ;
         }
         
         case PLAYLIST_STREAM : {
            printf( "-----> starting stream on port %s\n", current_.fileName_ );
            rectangle_t outRect ;
            outRect.xLeft_  = current_.x_ ;
            outRect.yTop_   = current_.y_ ;
            outRect.width_  = current_.w_ ;
            outRect.height_ = current_.h_ ;
            odomVideoStream_t *stream = new odomVideoStream_t( *this, strtoul(current_.fileName_,0,0), outRect );
            if( stream->isBound() ) 
               playing_ = stream ;
            else {
               fprintf( stderr, "Error binding to UDP port\n" );
               delete stream ;
            }
            break ;
         }
      }
   }
}

static char const * const fileTypes_[] = {
   "NONE",
   "STILL",
   "MPEG",
   "FLASH",
   "CMD"
};

void odomPlaylist_t::dump( void ){
   printf( "--> playlist content:\n" );
   printf( "add:  %u\n"
           "take: %u\n", add_, take_ );
   for( unsigned e = take_ ; e != add_ ; e = ( e + 1 ) & PLAYLISTMASK ){
      playlistEntry_t const &entry = entries_[e];
      printf( "[%u] == %s/%s\n", e, fileTypes_[entry.type_+1], entry.fileName_ );
   }
   printf( "current == %s/%s\n", fileTypes_[current_.type_+1], current_.fileName_ );
   if( PLAYLIST_MPEG == current_.type_ ){
      ((odomVideo_t *)playing_)->dump();
   }
}

bool odomPlaylist_t::dispatch( 
   odomCmdInterp_t   &interp,
   char const * const params[],
   unsigned           numParams,
   char              *errorMsg,
   unsigned           errorMsgLen )
{
   char const * const cmd = params[0];
   if( 0 == strcasecmp( cmd, "add" ) ){
      if( 4 < numParams ){
         playlistEntry_t entry ;
         memset( &entry, 0, sizeof(entry) );
         strncpy( entry.fileName_, params[1], sizeof(entry.fileName_) );
         entry.fileName_[sizeof(entry.fileName_)-1] = '\0' ;
         entry.x_ = strtoul( params[2], 0, 0 );
         entry.y_ = strtoul( params[3], 0, 0 );
         entry.w_ = ( 5 < numParams ) ? strtoul( params[5], 0, 0 ) : 0 ;
         entry.h_ = ( 6 < numParams ) ? strtoul( params[6], 0, 0 ) : 0 ;
         entry.repeat_ = ( 'N' != toupper(*params[4]) );
         char const *ext = strrchr(entry.fileName_, '.');
         if( ext ){
            ++ext ;
            char c = toupper(*ext);
            if( 'M' == c ){
               entry.type_ = PLAYLIST_MPEG ;
            }
            else if( 'S' == c ){
               entry.type_ = PLAYLIST_FLASH ;
            } // SWF
            else {
               entry.type_ = PLAYLIST_STILLIMAGE ;
               if( 7 < numParams ){
                  entry.numTicks_ = strtoul(params[7],0,0);
               }
            }
            printf( "added %s\n", entry.fileName_ );
            add(entry);
            return true ;
         }
         else
            snprintf( errorMsg, errorMsgLen, "unspecified file extension" );
      }
      else                                   //             0     1     2 3   4         5 6     7
         snprintf( errorMsg, errorMsgLen, "Usage: playlist add filename x y repeat(Y|n) w h [numTicks]" );
   }
   else if( 0 == strcasecmp( cmd, "addcmd" ) ){
      if( 2 < numParams ){
         playlistEntry_t entry ;
         memset( &entry, 0, sizeof(entry) );
         entry.repeat_ = ( 'N' != toupper(*params[1]) );

         char *nextOut = entry.fileName_ ;
         char *end = entry.fileName_+sizeof(entry.fileName_)-1 ;
         for( unsigned i = 2 ; i < numParams ; i++ ){
            nextOut += snprintf( nextOut, end-nextOut, "%s ", params[i] );
         }

         *end = '\0' ;

         entry.type_ = PLAYLIST_CMD ;
         entry.x_    = (unsigned long)(&interp);
         add(entry);

         return true ;
      }
      else
         snprintf( errorMsg, errorMsgLen, "Usage: playlist addcmd repeat(Y|n) command [param ...]" );
   }
   else if( 0 == strcasecmp( cmd, "stop" ) ){
      stop();
      return true ;
   }
   else if( 0 == strcasecmp( cmd, "purge" ) ){
      purge();
      return true ;
   }
   else if( 0 == strcasecmp( cmd, "dump" ) ){
      dump();
      return true ;
   }
   else if( 0 == strcasecmp( cmd, "stream" ) ){
      if( 3 < numParams ){
         playlistEntry_t entry ;
         memset( &entry, 0, sizeof(entry) );
         strncpy( entry.fileName_, params[1], sizeof(entry.fileName_) );
         entry.fileName_[sizeof(entry.fileName_)-1] = '\0' ;
         entry.x_ = strtoul( params[2], 0, 0 );
         entry.y_ = strtoul( params[3], 0, 0 );
         entry.w_ = ( 4 < numParams ) ? strtoul( params[4], 0, 0 ) : 0 ;
         entry.h_ = ( 5 < numParams ) ? strtoul( params[5], 0, 0 ) : 0 ;
         entry.repeat_ = false ;
         entry.type_ = PLAYLIST_STREAM ;
         add( entry );
         printf( "added stream on port %s\n", entry.fileName_ );
         return true ;
      }
      else                                   //             0      1   2 3 4 5
         snprintf( errorMsg, errorMsgLen, "Usage: playlist stream port x y w h" );
   }
   else
      snprintf( errorMsg, errorMsgLen, "unknown playlist cmd: %s", cmd );

   return false ;
}

int odomPlaylist_t::fdYUV( void )
{
   if( 0 > fdYUV_ ) {
      fdYUV_ = open( "/dev/yuv", O_WRONLY );
      yuvInW_ = -1U ; // force ioctl
   }
   return fdYUV_ ;
}

int odomPlaylist_t::fdYUV( 
   unsigned inw,
   unsigned inh,
   unsigned outx, 
   unsigned outy, 
   unsigned outw, 
   unsigned outh )
{
   fdYUV(); // force open

   if( ( inw != yuvInW_ )
       ||
       ( inh != yuvInH_ )
       ||
       ( outx != yuvOutX_ )
       ||
       ( outy != yuvOutY_ )
       ||
       ( outw != yuvOutW_ )
       ||
       ( outh != yuvOutH_ ) ){
      struct sm501yuvPlane_t plane ;
      plane.xLeft_     = outx ;
      plane.yTop_      = outy ;
      plane.inWidth_   = inw ;
      plane.inHeight_  = inh ;
      plane.outWidth_  = outw ;
      plane.outHeight_ = outh ;
      if( 0 == ioctl( fdYUV_, SM501YUV_SETPLANE, &plane ) ){
         yuvInW_  = inw ;
         yuvInH_  = inh ;
         yuvOutX_ = outx ;
         yuvOutY_ = outy ;
         yuvOutW_ = outw ;
         yuvOutH_ = outh ;
      }
      else {
         perror( "setPlane" );
         close( fdYUV_ );
         fdYUV_ = -1 ;
      }
   }

   return fdYUV_ ;
}

void odomPlaylist_t::closeYUV( void )
{
   if( 0 <= fdYUV_ ){
      close( fdYUV_ );
      fdYUV_ = -1 ;
   }
}


void odomPlaylist_t::vsyncHandler( void )
{
   if( PLAYLIST_MPEG == current_.type_ ){
      odomVideo_t *video = (odomVideo_t *)playing_ ;
      if( video ){
         video->doOutput();
      }
   }
   else if( PLAYLIST_STREAM == current_.type_ ){
      odomVideoStream_t *stream = (odomVideoStream_t *)playing_ ;
      if( stream ){
         stream->doOutput();
      }
   }
}

