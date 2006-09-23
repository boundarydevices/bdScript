/*
 * Module madDecode.cpp
 *
 * This module defines the methods of the madDecoder_t
 * class as declared in madDecode.h
 *
 *
 * Change History : 
 *
 * $Log: madDecode.cpp,v $
 * Revision 1.10  2006-09-23 19:33:22  ericn
 * -add output to standalone prog
 *
 * Revision 1.9  2006/09/05 02:16:32  ericn
 * -don't spin on errors
 *
 * Revision 1.8  2006/09/04 14:33:29  ericn
 * -add timing flags, timer string
 *
 * Revision 1.7  2006/05/14 14:34:55  ericn
 * -add madDecodeAll() routine, madDecode2 test program
 *
 * Revision 1.6  2005/08/12 04:19:15  ericn
 * -include <string.h>
 *
 * Revision 1.5  2003/08/04 03:13:56  ericn
 * -added standalone test
 *
 * Revision 1.4  2003/08/02 19:28:57  ericn
 * -remove debug statement
 *
 * Revision 1.3  2003/07/27 15:14:28  ericn
 * -modified to keep track of unread samples
 *
 * Revision 1.2  2003/07/20 15:42:32  ericn
 * -separated feed from read
 *
 * Revision 1.1  2002/11/24 19:08:58  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include "madDecode.h"
#include <mad.h>
#include <stdio.h>
#include <assert.h>
#include "id3tag.h"
#include <string.h>

int volatile in_synth_frame = 0 ;

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

madDecoder_t :: madDecoder_t( void )
   : timer_( mad_timer_zero ),
     haveHeader_( false ),
     numChannels_( 0 ),
     sampleRate_( 0 ),
     assEndLength_( 0 ),
     numSamples_( 0 ),
     sampleStart_( 0 )
{
   mad_stream_init(&mp3Stream_);
}

madDecoder_t :: ~madDecoder_t( void )
{
   mad_stream_finish(&mp3Stream_);
}

//
// returns true if output is available, false otherwise
//
void madDecoder_t :: feed( void const *inData, unsigned long inBytes )
{
   if( 0 != assEndLength_ )
   {
      if( sizeof( assEnd_ ) > inBytes + assEndLength_ )
      {
         memcpy( assEnd_+assEndLength_, inData, inBytes );
         assEndLength_ += inBytes ;
         mad_stream_buffer( &mp3Stream_, assEnd_, assEndLength_ );
      }
      else
      {
         fprintf( stderr, "packet too big %lu/%lu\n", assEndLength_, inBytes );
      }
      assEndLength_ = 0 ;
   }
   else
   {
      mad_stream_buffer( &mp3Stream_, (unsigned char const *)inData, inBytes );
   }
}
   
bool madDecoder_t :: getData( void )
{
   while( !haveHeader_ )
   {
      struct mad_header header; 
      if( -1 != mad_header_decode( &header, &mp3Stream_ ) )
      {
         numChannels_ = MAD_NCHANNELS(&header);
         sampleRate_  = header.samplerate;
         haveHeader_ = true ;
         mad_frame_init(&mp3Frame_);
         mad_synth_init(&mp3Synth_);
         timer_ = mad_timer_zero ;
      }
      else
      {
         if( MAD_RECOVERABLE( mp3Stream_.error ) )
         {
            if( MAD_ERROR_LOSTSYNC == mp3Stream_.error )
            {
               /* ignore LOSTSYNC due to ID3 tags */
               int tagsize = id3_tag_query (mp3Stream_.this_frame, mp3Stream_.bufend - mp3Stream_.this_frame);
               if (tagsize > 0)
               {
                  mad_stream_skip (&mp3Stream_, tagsize);
                  continue;
               }
            }
            printf("error decoding header: %s\n", 
                   mad_stream_errorstr (&mp3Stream_));
            return false ;
         }
         else if( MAD_ERROR_BUFLEN == mp3Stream_.error )
         {
            assEndLength_ = mp3Stream_.bufend-mp3Stream_.buffer ;
            if( assEndLength_ <= sizeof( assEnd_ ) )
            {
               if( assEndLength_ > 0 )
                  memcpy( assEnd_, mp3Stream_.buffer, assEndLength_ );
               else
                  return false ;
            }
            else
            {
               fprintf( stderr, "buffer too large for ass-end:%u\n", assEndLength_ );
               assEndLength_ = 0 ;
               return false ;
            }
         }
         else
         {
            printf( "unrecoverable error decoding header: %s\n", 
                    mad_stream_errorstr( &mp3Stream_ ) );
            return false ;
         }
      }
   }

   numSamples_ = 0 ;

   if( haveHeader_ )
   {
      do {
         if( -1 != mad_frame_decode(&mp3Frame_, &mp3Stream_ ) )
         {
            numChannels_ = MAD_NCHANNELS(&mp3Frame_.header);
            sampleRate_ = mp3Frame_.header.samplerate ;
in_synth_frame = 1 ;
            mad_synth_frame( &mp3Synth_, &mp3Frame_ );
in_synth_frame = 0 ;
            mad_timer_add( &timer_, mp3Frame_.header.duration );
            numSamples_ = mp3Synth_.pcm.length ;
            if( 0 < numSamples_ )
            {
               sampleStart_ = 0 ;
               break;
            }
         } // mp3Frame_ decoded
         else
         {
            if( MAD_RECOVERABLE( mp3Stream_.error ) )
               printf( "try to recover\n" );
            else 
            {
               if( MAD_ERROR_BUFLEN != mp3Stream_.error )
                  fprintf( stderr, "Error %d decoding audio mp3Frame_\n", mp3Stream_.error );
               else if( 0 != mp3Stream_.next_frame )
               {
                  assEndLength_ = mp3Stream_.bufend-mp3Stream_.next_frame ;
                  if( 0 < assEndLength_ )
                  {
                     memcpy( assEnd_, mp3Stream_.next_frame, assEndLength_ );
                  }
               }
               else
                  assEndLength_ = 0 ;
               break;
            }
         }
      } while( 1 );
   }

   if( 0 == numSamples_ )
   {
      return false ;
   }
   else
      return true ;
}

bool madDecoder_t :: readSamples( unsigned short samples[],
                                  unsigned       maxSamples,
                                  unsigned      &numRead )
{
   
   numRead = 0 ;

   unsigned short *nextOut = samples ;

   if( 1 == numChannels_ )
   {     
      if( maxSamples > numSamples_ )
         maxSamples = numSamples_ ;

      mad_fixed_t const *left = mp3Synth_.pcm.samples[0] + sampleStart_ ;

      for( unsigned i = 0 ; i < maxSamples ; i++ )
         *nextOut++ = scale( *left++ );
      
      numSamples_  -= maxSamples ;
      sampleStart_ += maxSamples ;
   
   } // mono
   else
   {
      maxSamples /= 2 ;

      if( maxSamples > numSamples_ )
         maxSamples = numSamples_ ;

      mad_fixed_t const *left  = mp3Synth_.pcm.samples[0] + sampleStart_ ;
      mad_fixed_t const *right = mp3Synth_.pcm.samples[1] + sampleStart_ ;

      for( unsigned i = 0 ; i < maxSamples ; i++ )
      {
         *nextOut++ = scale( *left++ );
         *nextOut++ = scale( *right++ );
      }

      numSamples_  -= maxSamples ;
      sampleStart_ += maxSamples ;

   } // stereo

   numRead = nextOut - samples ;
   return 0 != numRead ;
}

char const *madDecoder_t :: timerString( void )
{
   mad_timer_string(timer_,timerBuf_,"%lu:%02lu.%03u", MAD_UNITS_MINUTES, MAD_UNITS_MILLISECONDS,0);
   return timerBuf_ ;
}

bool madDecodeAll( void const      *mp3Data,             // input
                   unsigned         mp3Length,           // input
                   unsigned short *&decodedData,         // output
                   unsigned        &numBytes,            // output
                   unsigned        &sampleRate,          // output
                   unsigned        &numChannels,         // output
                   unsigned         padToPage,           // input
                   unsigned         maxBytes )           // input
{
   decodedData = 0 ;
   
   unsigned long totalRead = 0 ;
   bool hadHeader = false ;

   { // limit decoder scope
      madDecoder_t decoder ;
      decoder.feed( mp3Data, mp3Length );
   
      do {
         if( !hadHeader && decoder.haveHeader() ){
            numChannels = decoder.numChannels();
            sampleRate  = decoder.sampleRate();
            hadHeader = true ;
         }
   
         enum { maxSamples = 2048 };
         unsigned short samples[maxSamples];
         unsigned numRead ;
         while( decoder.readSamples( samples, maxSamples, numRead ) ){
            totalRead += numRead ;
         }
         if( !decoder.getData() )
            break;
      } while( 1 );
   }
   
   numBytes = totalRead * sizeof(unsigned short);   
   if( hadHeader && ( numBytes > 0 ) && ( numBytes < maxBytes ) )
   {
      // round up to page size
      numBytes = ((numBytes+padToPage-1)/padToPage)*padToPage ;
      totalRead = numBytes/sizeof(unsigned short);

      decodedData = new unsigned short [totalRead];

      madDecoder_t decoder ;
      decoder.feed( mp3Data, mp3Length );

      unsigned numFilled = 0 ;
      do {
         unsigned numRead ;
         while( decoder.readSamples( decodedData+numFilled, totalRead-numFilled, numRead ) ){
            numFilled += numRead ;
         }
         if( !decoder.getData() )
            break;
      } while( numFilled < totalRead );

      return true ;
   }
   
   return false ;
}

#ifdef STANDALONE
#include "memFile.h"

int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         FILE *fOut = ( 2 < argc ) ? fopen( argv[2], "wb" ) : 0 ;
         madDecoder_t decoder ;
         decoder.feed( fIn.getData(), fIn.getLength() );
         do {
            enum { maxSamples = 512 };
            unsigned short samples[maxSamples];
            unsigned numRead ;
            while( decoder.readSamples( samples, maxSamples, numRead ) ){
               printf( "%s: %u samples\n", decoder.timerString(), numRead );
               if( fOut )
                  fwrite( samples, 2, numRead, fOut );
            }
            if( !decoder.getData() )
               break;
            else
               printf( "more data\n" );
         } while( 1 );

         if( fOut ){
            fclose( fOut );
            printf( "wrote file <%s>\n", argv[2] );
         }
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage : madDecode fileName [outFile]\n" );

   return 0 ;
}

#elif defined( STANDALONE2 )

//
// touch effect latency test (gather touch-to-play time with and without decode)
// 
#include "memFile.h"
#include "tickMs.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "pollHandler.h"
#include "touchPoll.h"
#include "ttyPoll.h"
#include <signal.h>

struct mp3Data_t {
   char const     *fileName_ ;
   unsigned        numBytes_ ;
   unsigned        channels_ ;
   unsigned        sampleRate_ ;
   long long       endOfDecode_ ;
   unsigned short *data_ ;
};

class touchPlay_t : public touchPoll_t {
public:
   touchPlay_t( pollHandlerSet_t &set,
                int               dspFd,
                mp3Data_t        &sound )
      : touchPoll_t( set )
      , dspFd_( dspFd )
      , sound_( sound )
      , wasDown_( false ){}
   ~touchPlay_t( void ){}

   virtual void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   virtual void onRelease( timeval const &tv );

   timeval const &when( void ) const { return touchTime_ ; }

protected:
private:
   touchPlay_t(touchPlay_t const &); // no copies
   
   int const  dspFd_ ;
   mp3Data_t &sound_ ;
   timeval    touchTime_ ;
   timeval    endPlay_ ;
   bool       wasDown_ ;
};


void touchPlay_t :: onTouch
   ( int x, int y, unsigned pressure, timeval const &tv )
{
   if( !wasDown_ )
   {
      wasDown_ = 1 ;
      touchTime_ = tv ;
      write( dspFd_, sound_.data_, sound_.numBytes_ );
      if( 0 != ioctl( dspFd_, SNDCTL_DSP_POST, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_POST):%m" );
      gettimeofday( &endPlay_, 0 );
   }
}

void touchPlay_t :: onRelease( timeval const &tv )
{
   wasDown_ = 0 ;
   unsigned long start = (touchTime_.tv_sec*1000)+(touchTime_.tv_usec / 1000);
   unsigned long end = (endPlay_.tv_sec*1000)+(endPlay_.tv_usec / 1000);
   unsigned long elapsed = end-start ;
   printf( "elapsed: %lu\n", elapsed );
}


#include "hexDump.h"
#include <execinfo.h>
#include <stdlib.h>

static struct sigaction sa;
static struct sigaction oldint;

void handler(int sig) 
{
   fprintf( stderr, "got signal, id %x\n", sig );
   fprintf( stderr, "sighandler at %p\n", handler );

   unsigned long addr = (unsigned long)&sig ;
   unsigned long page = addr & ~0xFFF ; // 4K
   unsigned long size = page+0x1000-addr ;
   hexDumper_t dumpStack( &sig, size ); // just dump this page
   while( dumpStack.nextLine() )
      fprintf( stderr, "%s\n", dumpStack.getLine() );

   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(void *) );

   fprintf( stderr, "########## Backtrace ##########\n"
                    "Number of elements in backtrace: %u\n", btSize );

   if (btSize > 0)
      backtrace_symbols_fd( btArray, btSize, fileno(stdout) );

   fprintf( stderr, "Handler done.\n" );
   fflush( stderr );
   if( oldint.sa_handler )
      oldint.sa_handler( sig );

   exit( 1 );
}


int main( int argc, char const * const argv[] )
{
   long long const start = tickMs();

   if( 2 <= argc )
   {
      unsigned const numFiles = argc-1;
      mp3Data_t *const data = new mp3Data_t[numFiles];
      memset( data, 0, numFiles*sizeof(data[0]));

      long long beforeFiles = tickMs();

      for( unsigned i = 0 ; i < numFiles ; i++ )
      {
         data[i].fileName_ = argv[i+1];
         memFile_t fIn( data[i].fileName_ );
         if( fIn.worked() )
         {
            unsigned long totalRead = 0 ;
            bool hadHeader = false ;
            madDecoder_t decoder ;
            decoder.feed( fIn.getData(), fIn.getLength() );
            do {
               if( !hadHeader && decoder.haveHeader() ){
                  data[i].channels_ = decoder.numChannels();
                  data[i].sampleRate_ = decoder.sampleRate();
                  hadHeader = true ;
               }
               enum { maxSamples = 2048 };
               unsigned short samples[maxSamples];
               unsigned numRead ;
               while( decoder.readSamples( samples, maxSamples, numRead ) ){
                  totalRead += numRead ;
               }
               if( !decoder.getData() )
                  break;
            } while( 1 );
            
            data[i].endOfDecode_ = tickMs();
            data[i].numBytes_ = totalRead*sizeof(*data[i].data_);
         }
         else
            perror( data[i].fileName_ );
      }

      long long endOfFiles = tickMs();

      audio_buf_info info ;
      memset( &info, 0, sizeof(info) );
      unsigned lastSpeed = 0 ;

      int dspFd = open( "/dev/dsp", O_WRONLY );
      if( 0 <= dspFd )
      {
         if( 0 == ioctl(dspFd, SNDCTL_DSP_SYNC, 0 ) ) 
         {
            int const format = AFMT_S16_LE ;
            if( 0 == ioctl( dspFd, SNDCTL_DSP_SETFMT, &format) ) 
            {
               int const channels = 1 ;
               if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &channels ) )
                  fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );
   
               int speed = 44100 ;
               while( 0 < speed )
               {
                  if( 0 == ioctl( dspFd, SNDCTL_DSP_SPEED, &speed ) )
                  {
                     lastSpeed = speed ;
                     break;
                  }
                  else
                     fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
                  speed /= 2 ;
               }
   
               if( 0 != speed )
               {
                  if( 0 != ioctl( dspFd, SNDCTL_DSP_GETOSPACE, &info ) )
                     fprintf( stderr, "Error %m getting outBuffer stats\n" );
                     
                  long long readyToWrite = tickMs();
                  
                  printf( "%lu ms in open, fragsize == %lu\n", 
                           (unsigned long)(readyToWrite-endOfFiles),
                           info.fragsize );
               }
               else
                  fprintf( stderr, "No known speed supported\n" );
            }
            else
               fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
         }
         else
            fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC)\n" );
      }
      else
         perror( "/dev/dsp" );

      printf( "%lu ms in decode\n", (unsigned long)(endOfFiles-beforeFiles) );

      for( unsigned i = 0 ; i < numFiles ; i++ )
      {
         printf( "%s: %lu bytes, %u channels, %u Hz, %lu ms\n", 
                 data[i].fileName_, 
                 data[i].numBytes_,
                 data[i].channels_, 
                 data[i].sampleRate_,
                 ( 0 == i ) 
                 ? (unsigned long)(data[i].endOfDecode_-beforeFiles)
                 : (unsigned long)(data[i].endOfDecode_-data[i-1].endOfDecode_) );
         if( 0 != info.fragsize ){
            data[i].numBytes_ = ((data[i].numBytes_ + info.fragsize - 1)/info.fragsize)*info.fragsize ;
            unsigned const numSamples = data[i].numBytes_/sizeof(data[i].data_[0]);
            data[i].data_ = new unsigned short [numSamples];
            
            long long startDecode2 = tickMs();
            memFile_t fIn( data[i].fileName_ );
            if( fIn.worked() )
            {
               unsigned long totalRead = 0 ;
               bool hadHeader = false ;
               madDecoder_t decoder ;
               decoder.feed( fIn.getData(), fIn.getLength() );

               do {
                  unsigned numRead ;
                  while( decoder.readSamples( data[i].data_+totalRead, numSamples-totalRead, numRead ) ){
                     totalRead += numRead ;
                  }
                  if( !decoder.getData() )
                     break;
               } while( 1 );

               printf( "   %lu ms decode2\n", tickMs()-startDecode2 );

            }
            else
               perror( data[i].fileName_ );
         }
      }

      if( 0 <= dspFd ){
         for( unsigned i = 0 ; i < numFiles ; i++ ){
            if( lastSpeed != data[i].sampleRate_ ){
                  if( 0 != ioctl( dspFd, SNDCTL_DSP_SPEED, &data[i].sampleRate_ ) )
                  {
                     perror( "SPEED" );
                     break;
                  }
                  lastSpeed = data[i].sampleRate_ ;
            }
            write( dspFd, data[i].data_, data[i].numBytes_ );
         }
      
         if( 0 < numFiles )
         {
            signal(SIGTERM,SIG_IGN);
            sa.sa_handler = handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            // Set up the signal handler
            sigaction(SIGSEGV, &sa, NULL);
            
            pollHandlerSet_t poll ;
   
            ttyPollHandler_t *tty = new ttyPollHandler_t( poll );
            touchPlay_t *player = new touchPlay_t( poll, dspFd, data[0] );

            while( !tty->ctrlcHit() ){
               poll.poll(-1);
            }
            
            sleep(4);
            printf( "exiting...\n" );
         }
      
         close( dspFd );
      }      

      delete [] data ;
   }
   else
      fprintf( stderr, "Usage : madDecode2 fileName [fileName...]\n" );

   return 0 ;
}

#endif
