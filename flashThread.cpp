/*
 * Module flashThread.cpp
 *
 * This module defines the methods of the flashThread_t
 * class as declared in flashThread.h
 *
 *
 * Change History : 
 *
 * $Log: flashThread.cpp,v $
 * Revision 1.1  2003-11-22 18:30:08  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

#include "flashThread.h"
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include "js/jsapi.h"
#include "audioQueue.h"
#include "flash/sound.h"
#include "fbDev.h"
#include "tickMs.h"

static void
getUrl(char *url, char *target, void *client_data)
{
	printf("GetURL : %s\n", url);
}

static void
getSwf(char *url, int level, void *client_data)
{
	printf("GetSwf : %s\n", url);
}


class flashSoundMixer : public SoundMixer {
public:
	flashSoundMixer( void );
	~flashSoundMixer();

	void		 startSound(Sound *sound);	// Register a sound to be played
	void		 stopSounds();		// Stop every current sounds in the instance
};

flashSoundMixer :: flashSoundMixer( void )
   : SoundMixer( "" )  // keep it happy
{
}

flashSoundMixer :: ~flashSoundMixer()
{
}

#include "hexDump.h"

static void flashSoundComplete( void *param )
{
   printf( "flashSoundComplete:%p\n", param );
}

void flashSoundMixer :: startSound(Sound *sound)
{
   getAudioQueue().queuePlayback( (unsigned char *)sound->getSamples(), 
                                  sound->getSampleSize()*sound->getNbSamples(),
                                  sound,
                                  flashSoundComplete );
}

void flashSoundMixer :: stopSounds()
{
   unsigned numCancelled ;
   getAudioQueue().clear( numCancelled );
}

flashThread_t :: flashThread_t
   ( FlashHandle  hFlash,
     unsigned     x, 
     unsigned     y,
     unsigned     w, 
     unsigned     h,
     unsigned     bgColor,
     bool         loop )
   : hFlash_( hFlash )
   , x_( x )
   , y_( y )
   , w_( w )
   , h_( h )
   , bgColor_( bgColor )
   , loop_( loop )
   , fbMem_( 0 )
   , mixer_( new flashSoundMixer() )
   , threadHandle_( (pthread_t)-1 )
   , fdReadCtrl_( -1 )
   , fdWriteCtrl_( -1 )
   , fdReadEvents_( -1 )
   , fdWriteEvents_( -1 )
{
   memset( &display_, 0, sizeof( display_ ) );
   int fdCtrl[2];
   if( 0 == pipe( fdCtrl ) )
   {
      fdReadCtrl_ = fdCtrl[0];
      fdWriteCtrl_ = fdCtrl[1];
      int fdEvents[2];
      if( 0 == pipe( fdEvents ) )
      {
         fdReadEvents_ = fdEvents[0];
         fdWriteEvents_ = fdEvents[1];
         
         fbDevice_t &fb = getFB();
         display_.width  = w ;
         if( display_.width > fb.getWidth() )
            display_.width = fb.getWidth();
         display_.height = h ;
         if( display_.height > fb.getHeight() )
            display_.height = fb.getHeight();
      
         unsigned short * const pixels = new unsigned short [ display_.width*display_.height ];
         display_.pixels = pixels ;
         display_.bpl    = display_.width*sizeof(pixels[0]);
         unsigned const videoBytes = display_.width*display_.height*sizeof(pixels[0]);
         unsigned short const bg = fb.get16( ( bgColor_ >> 16 ),
                                             ( bgColor_ >> 8 ) & 0xFF,
                                             ( bgColor_ & 0xFF ) );
         // set top row
         for( unsigned i = 0 ; i < display_.width ; i++ )
            pixels[i] = bg ;
         // set remaining rows 
         for( unsigned i = 1 ; i < display_.height ; i++ )
            memcpy( pixels+(i*display_.width), pixels, display_.bpl );
         fbMem_ = (unsigned short *)fb.getMem() + x_ + ( y_ * fb.getWidth() );
         fbStride_ = fb.getWidth()*sizeof(pixels[0]);
      
         display_.depth  = sizeof( pixels[0]);
         display_.bpp    = sizeof( pixels[0]);
         display_.flash_refresh = 0 ;
         display_.clip_x = 0 ;
         display_.clip_y = 0 ;
         display_.clip_width = display_.width ;
         display_.clip_height = display_.height ;
      
         printf( "   bpl %u\n"
                 "   width %u\n"
                 "   height %u\n"
                 "   depth %u\n"
                 "   bpp %u\n"
                 "   refresh %u\n"
                 "   clipx %u\n"
                 "   clipy %u\n"
                 "   clipw %u\n"
                 "   cliph %u\n",
                 display_.bpl,
                 display_.width,
                 display_.height,
                 display_.depth,
                 display_.bpp,
                 display_.flash_refresh,
                 display_.clip_x, 
                 display_.clip_y,
                 display_.clip_width, 
                 display_.clip_height );
         FlashGraphicInit( hFlash, &display_ );
      
         FlashSetGetUrlMethod( hFlash, getUrl, 0);
         FlashSetGetSwfMethod( hFlash, getSwf, 0);
         
         FlashMovie *fh = (FlashMovie *)hFlash ;
         fh->sm = mixer_ ;

         int create = pthread_create( &threadHandle_, 0, flashThread, this );
         if( 0 != create )
            perror( "flashThreadCreate" );
      }
      else
         perror( "flashEventsPipe" );
   }
   else
      perror( "flashCtrlPipe" );
}

flashThread_t :: ~flashThread_t( void )
{
printf( "closing\n" );
   if( 0 <= fdWriteCtrl_ ) { close( fdWriteCtrl_ );  fdWriteCtrl_ = -1 ; }
   if( 0 <= fdReadEvents_ ){ close( fdReadEvents_ ); fdReadEvents_ = -1 ; }

   if( isAlive() )
   {
      void *exitStat ; 
      pthread_join( threadHandle_, &exitStat ); 
      threadHandle_ = (pthread_t)-1 ; 
   }
   
   if( 0 != display_.pixels )
   {
      delete [] (unsigned short *)display_.pixels ;
      display_.pixels = 0 ;
   }

   ( (FlashMovie *)hFlash_ )->sm = 0 ;
   if( mixer_ )
      delete mixer_ ;
printf( "destructor done\n" ); fflush( stdout );
}

void flashThread_t :: sendCtrl( controlMsg_e type, unsigned param )
{
   if( 0 <= fdWriteCtrl_ )
   {
      controlMsg_t msg ;
      msg.type_  = type ;
      msg.param_ = param ;
      write( fdWriteCtrl_, &msg, sizeof( msg ) );
   }
}

bool flashThread_t :: readEvent( event_e &which ) const 
{
   if( 0 <= fdReadEvents_ )
   {
      pollfd filedes ;
      filedes.fd = fdReadEvents_ ;
      filedes.events = POLLIN ;
      if( 0 < poll( &filedes, 1, 0 ) )
      {
         return sizeof( which ) == read( fdReadEvents_, &which, sizeof( which ) );
      }
   }

   which = none_e ;
   return false ;

}

void *flashThread( void *param )
{
   flashThread_t &tObj = *( flashThread_t * )param ;

   bool running = false ;
   FlashMovie * const movie = (FlashMovie *)tObj.hFlash_ ;
   Program    * const prog = movie->main->program ;
   printf( "movie %p, prog %p\n", movie, prog );
   unsigned msNext = 0 ;

   do {
      pollfd filedes ;
      filedes.fd = tObj.fdReadCtrl_ ;
      filedes.events = POLLIN ;
      int const pollRes = poll( &filedes, 1, running ? msNext : -1U );
      if( 0 < pollRes )
      {
         flashThread_t::controlMsg_t msg ;
         int const numRead = read( tObj.fdReadCtrl_, &msg, sizeof( msg ) );
         if( sizeof( msg ) == numRead )
         {
            printf( "msg %u/%u\n", msg.type_, msg.param_ );
            switch( msg.type_ )
            {
               case flashThread_t::start_e :
               {
                  prog->rewindMovie();
                  prog->continueMovie();
                  running = true ;
                  break;
               }
               case flashThread_t::pause_e :
               {
                  prog->pauseMovie();
                  running = false ;
                  break;
               }
               case flashThread_t::cont_e :
               {
                  prog->continueMovie();
                  running = true ;
                  break;
               }
               case flashThread_t::stop_e :
               {
                  prog->pauseMovie();
                  prog->rewindMovie();
                  running = false ;
                  break;
               }
               case flashThread_t::rewind_e :
               {
                  prog->rewindMovie();
                  break;
               }
            }
         }
         else 
         {
            if( 0 != numRead ) // !broken pipe
               fprintf( stderr, "flashReadCtrl:%u:%m", numRead );

            break;
         }
      }
      else if( 0 == pollRes )
      {
          struct timeval wakeTime ;
          long wakeUp = FlashExec( tObj.hFlash_, FLASH_WAKEUP, 0, &wakeTime);
   
          if( tObj.display_.flash_refresh )
          {
             char *pixMem = (char *)tObj.fbMem_ ;
             char *flashMem = (char *)tObj.display_.pixels ;
             for( unsigned i = 0 ; i < tObj.display_.height ; i++, pixMem += tObj.fbStride_, flashMem += tObj.display_.bpl )
                memcpy( pixMem, flashMem, tObj.display_.bpl );
          }

          if( wakeUp )
          {
             long long wakeMs = timeValToMs( wakeTime );
             long long now = tickMs();
             long int delta = (wakeMs-now);
             msNext = ( delta > 0 ) ? (unsigned)delta : 0 ;
          }
          else
          {
             flashThread_t::event_e const endEvent = flashThread_t::complete_e ;
             write( tObj.fdWriteEvents_, &endEvent, sizeof( endEvent ) );
             running = tObj.loop_ ;
             if( running )
             {
               prog->rewindMovie();
               prog->continueMovie();
             }
          }
      }
      else
      {
         perror( "flashPollCtrl" );
         break;
      }
   } while( 1 );
   
   printf( "flashThread shutdown\n" );
}


#ifdef STANDALONE
#include "memFile.h"

static bool strTableLookup( char const         *needle,        // input
                            char const * const *haystack,      // input
                            unsigned            numItems,      // input: num strings in haystack
                            unsigned           &item )
{
   for( unsigned i = 0 ; i < numItems ; i++ )
   {
      if( 0 == strcmp( needle, haystack[i] ) )
      {
         item = i ;
         return true ;
      }
   }
   item = numItems ;
   return false ;
}

static char const * const commands[] = {
   "start",
   "pause",
   "cont",
   "stop",
   "rewind"
};

static unsigned const numCommands = sizeof( commands )/sizeof( commands[0] );

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         FlashHandle const hFlash = FlashNew();
         int status ;
   
         // Load level 0 movie
         do {
            status = FlashParse( hFlash, 0, (char *)fIn.getData(), fIn.getLength() );
         } while( status & FLASH_PARSE_NEED_DATA );
         
         if( FLASH_PARSE_START & status )
         {
            struct FlashInfo fi;
            FlashGetInfo( hFlash, &fi );
            printf( "   rate %lu\n"
                    "   count %lu\n"
                    "   width %lu\n"
                    "   height %lu\n"
                    "   version %lu\n", 
                    fi.frameRate, fi.frameCount, fi.frameWidth, fi.frameHeight, fi.version );
            
/*            
*/
            flashThread_t flash( hFlash, 0, 0, 222, 239, 0xFFFF, true );
            while( 1 )
            {
               pollfd filedes[2];
               filedes[0].fd = 0 ;
               filedes[0].events = POLLIN ;
               filedes[1].fd = flash.eventReadFd();
               filedes[1].events = POLLIN ;
               int const pollRes = poll( filedes, 2, -1U );
               if( 0 < pollRes )
               {
                  if( filedes[0].revents )
                  {
                     char inBuf[256];
                     if( fgets( inBuf, sizeof( inBuf ), stdin ) )
                     {
                        char *context ;
                        char *cmd = strtok_r( inBuf, " \t\r\n", &context ); 
                        if( cmd )
                        {
                           unsigned cmdId ;
                           if( strTableLookup( cmd, commands, numCommands, cmdId ) )
                           {
                              printf( "command %u\n", cmdId );
                              unsigned param = 0 ;
         
                              flash.sendCtrl( (flashThread_t::controlMsg_e) cmdId, param );
         
                           }
                           else
                           {
                              printf( "invalid command <%s>!\n"
                                      "valid commands are:\n", cmd );
                              for( unsigned i = 0 ; i < numCommands ; i++ )
                                 printf( "   %s\n", commands[i] );
                           }
                        }
                     }
                     else
                        break;
                  }
                  
                  if( filedes[1].revents )
                  {
                     flashThread_t::event_e event ;
                     while( flash.readEvent( event ) )
                     {
                        printf( "event %u\n", event );
                     }
                  }
               }

            }
         }
         else
            fprintf( stderr, "Error 0x%x parsing flash\n", status );

         printf( "closing flash\n" ); fflush( stdout );
         FlashClose( hFlash );
         printf( "done closing flash\n" ); fflush( stdout );

      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: %s fileName.swf\n", argv[0] );

   return 0 ;
}

#endif
