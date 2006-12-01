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
 * Revision 1.12  2006-12-01 19:57:53  tkisky
 * -include <assert.h>
 *
 * Revision 1.11  2006/10/10 20:50:51  ericn
 * -use fds, not playlist in odomVideo constructor
 *
 * Revision 1.10  2006/09/24 16:26:34  ericn
 * -remove vsync propagation from odometerSet_t (use multiSignal instead)
 *
 * Revision 1.9  2006/09/05 02:17:50  ericn
 * -fix doOutput()/doAudio() bug
 *
 * Revision 1.8  2006/09/04 16:43:20  ericn
 * -add audio signal callback
 *
 * Revision 1.7  2006/09/04 15:16:53  ericn
 * -add audio interfaces
 *
 * Revision 1.6  2006/09/01 01:02:27  ericn
 * -use mpegQueue for odomVideo
 *
 * Revision 1.5  2006/08/31 03:45:20  ericn
 * -use non-blocking I/O on YUV device
 *
 * Revision 1.4  2006/08/26 17:13:04  ericn
 * -add dump for streams
 *
 * Revision 1.3  2006/08/26 16:05:44  ericn
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
#include <linux/soundcard.h>
#include "rtSignal.h"
#include <signal.h>
#include "multiSignal.h"
#include <assert.h>

#define PLAYLISTMASK (odomPlaylist_t::MAXENTRIES-1)

static int const dspSignal_ = nextRtSignal();

odomPlaylist_t *lastPlaylistInst_ = 0 ;

static void playlistVSync( int signo, void *param )
{
   odomPlaylist_t *playlist = (odomPlaylist_t *)param ;
   if( playlist )
      playlist->vsyncHandler();
}

odomPlaylist_t::odomPlaylist_t( void )
   : playing_( 0 )
   , add_( 0 )
   , take_( 0 )
   , fdYUV_(-1)
   , yuvInW_(-1U)
   , yuvInH_(-1U)
   , yuvOutX_(-1U)
   , yuvOutY_(-1U)
   , yuvOutW_(-1U)
   , yuvOutH_(-1U)
   , fdDsp_(-1)
   , channels_(0)
   , speed_(0)
{
   current_.type_ = PLAYLIST_NONE ;
   current_.repeat_ = false ;

   sigset_t mask ;
   sigemptyset( &mask );
   sigaddset( &mask, odometerSet_t::get().getVSyncSignal() );
   sigaddset( &mask, odometerSet_t::get().getCmdListSignal() );

   setSignalHandler( odometerSet_t::get().getVSyncSignal(),
                     mask, playlistVSync, this );

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
            rectangle_t outRect ;
            outRect.xLeft_  = current_.x_ ;
            outRect.yTop_   = current_.y_ ;
            outRect.width_  = current_.w_ ;
            outRect.height_ = current_.h_ ;
            odomVideo_t *video = new odomVideo_t( fdDsp(), fdYUV(), current_.fileName_, outRect );
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
   "CMD",
   "STREAM"
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
   } else if( PLAYLIST_STREAM == current_.type_ ){
      ((odomVideoStream_t *)playing_)->dump();
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
      fcntl(fdYUV_, F_SETFL, fcntl(fdYUV_, F_GETFL) | O_NONBLOCK | FASYNC );
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


void odomPlaylist_t::dspHandler( void )
{
   if( PLAYLIST_MPEG == current_.type_ ){
      odomVideo_t *video = (odomVideo_t *)playing_ ;
      if( video ){
         video->doAudio();
      }
   }
   else if( PLAYLIST_STREAM == current_.type_ ){
      odomVideoStream_t *stream = (odomVideoStream_t *)playing_ ;
      if( stream ){
         stream->doAudio();
      }
   }
}

static void dspHandler( int signo, siginfo_t *info, void *context )
{
   if( lastPlaylistInst_ )
      lastPlaylistInst_->dspHandler();
}

int odomPlaylist_t::fdDsp( void )
{
   if( 0 > fdDsp_ )
      fdDsp_ = open( "/dev/dsp", O_WRONLY );

   if( 0 <= fdDsp_ ){
      fcntl(fdDsp_, F_SETFL, fcntl(fdDsp_, F_GETFL) | O_NONBLOCK | FASYNC );

      if( 0 != ioctl(fdDsp_, SNDCTL_DSP_SYNC, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );

      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( fdDsp_, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
      
      int vol = (75<<8)|75 ;
      if( 0 > ioctl( fdDsp_, SOUND_MIXER_WRITE_VOLUME, &vol)) 
         perror( "Error setting volume" );
      
      struct sigaction sa ;
      sa.sa_flags = SA_SIGINFO|SA_RESTART ;
      sa.sa_restorer = 0 ;
      sigemptyset( &sa.sa_mask );

      fcntl(fdDsp_, F_SETOWN, getpid() );
      fcntl(fdDsp_, F_SETSIG, dspSignal_ );
      sa.sa_sigaction = ::dspHandler ;
      sigaddset( &sa.sa_mask, dspSignal_ );
      sigaction(dspSignal_, &sa, 0 );
   }
   
   return fdDsp_ ;
}

int odomPlaylist_t::fdDsp( unsigned speed, unsigned channels )
{
   fdDsp();
   if( 0 <= fdDsp_ ){
      if( speed != speed_ ){
         if( 0 == ioctl( fdDsp_, SNDCTL_DSP_SPEED, &speed ) ){
            speed_ = speed ;
         }
         else
            fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
      }
      if( channels != channels_ ){
         if( 0 == ioctl( fdDsp_, SNDCTL_DSP_CHANNELS, &channels ) ){
            channels_ = channels ;
         }
         else
            fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%u:%m\n", channels );
      }
   }
   return fdDsp_ ;
}

void odomPlaylist_t::closeDsp( void )
{
   if( 0 <= fdDsp_ ){
      close( fdDsp_ );
      fdDsp_ = -1 ; 
   }
}

void odomPlaylist_t::setVolume( unsigned volume )
{
   volume &= 0xFF ;
   volume = ( volume << 8 ) | volume ;
   fdDsp();
   if( 0 != ioctl( fdDsp_, SOUND_MIXER_WRITE_VOLUME, &volume ) )
      fprintf( stderr, ":ioctl(SOUND_MIXER_WRITE_VOLUME):%04x:%m\n", volume );
}
