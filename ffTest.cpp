#include "mpDemux.h"

#include <signal.h>
#include <execinfo.h>
#include <map>
#include <set>
#include <unistd.h>
#include <sys/time.h>
#include "fbDev.h"
#include <stdio.h>
#include <fcntl.h>
#include "mpegDecode.h"
#include "madDecode.h"
#include <zlib.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/soundcard.h>
#include <poll.h>
#include "memFile.h"
#include "videoQueue.h"
#include "tickMs.h"

static struct sigaction sa;
static struct sigaction oldint;

typedef std::map<unsigned long,unsigned long> longByLong_t ;
static longByLong_t traceAddrs_ ;
static unsigned long numAlarms_ = 0 ;
static unsigned long numAddrs_ = 0 ;

struct countAndAddress_t {
   unsigned long count_ ;
   unsigned long address_ ;

   inline bool operator==( countAndAddress_t const &rhs ) const
      { return ( count_ == rhs.count_ ) && ( address_ == rhs.address_ ); }
   inline bool operator<( countAndAddress_t const &rhs ) const
      { if( count_ == rhs.count_ ) 
           return ( address_ == rhs.address_ ); 
        else 
           return ( count_ < rhs.address_ );
      }
};

typedef std::multiset<countAndAddress_t> addressesByCount_t ;

bool volatile done_ = false ;
bool first = false ; // set true to capture first frame

static void handler(int sig) 
{
   void *btArray[128];
   int const btSize = backtrace(btArray, sizeof(btArray) / sizeof(btArray[0]) );
   if (btSize > 0)
   {
      for (int i = 0 ; i < btSize ; i++ )
         traceAddrs_[(unsigned long)btArray[i]]++ ;
      ++numAlarms_ ;
      numAddrs_ += btSize ;
   }
   
   struct itimerval     itimer;
   itimer . it_interval . tv_sec = 0;
   itimer . it_interval . tv_usec = 0;
   itimer . it_value . tv_sec = 0 ;
   itimer . it_value . tv_usec = 10000 ;

   if( !done_ )
   {
      if( setitimer (ITIMER_REAL, &itimer, NULL) < 0)
      {
         perror ("StartTimer could not setitimer");
         exit (0);
      }
   }
}

static void dumpTraces( void )
{
   printf( "------------- trace long ---------------\n"
           "%lu address hits\n"
           "iterations\taddress\n", numAddrs_ );

   addressesByCount_t byCount ;
   longByLong_t :: const_iterator it = traceAddrs_.begin();
   for( ; it != traceAddrs_.end(); it++ )
   {
      countAndAddress_t cna ;
      cna.count_ = (*it).second ;
      cna.address_ = (*it).first ;
      byCount.insert( cna );
   }

   unsigned long sum = 0 ;
   addressesByCount_t :: reverse_iterator rit = byCount.rbegin();
   for( ; rit != byCount.rend(); rit++ )
   {
      char **symbolName = backtrace_symbols( (void **)&(*rit).address_, 1 );
      printf( "%10d\t%p\t%s\n", (*rit).count_, (*rit).address_, symbolName[0] );
      sum += (*rit).count_ ;
   }
   printf( "sum == %lu\n", sum );
}

static char const * const frameTypes_[] = {
   "video",
   "audio",
   "endOfFile"
};


int main( int argc, char const * const argv[] )
{
   struct itimerval     itimer;

   /*
    * Set up a signal handler for the SIGALRM signal which is what the
    * expiring timer will set.
    */

   signal (SIGALRM, handler);

   /*
    * Set the timer up to be non repeating, so that once it expires, it
    * doesn't start another cycle.  What you do depends on what you need
    * in a particular application.
    */

   itimer . it_interval . tv_sec = 0;
   itimer . it_interval . tv_usec = 0;
 
   /*
    * Set the time to expiration to interval seconds.
    * The timer resolution is milliseconds.
    */
   
   itimer . it_value . tv_sec = 0 ;
   itimer . it_value . tv_usec = 100000 ;


   if (setitimer (ITIMER_REAL, &itimer, NULL) < 0)
   {
      perror ("StartTimer could not setitimer");
      exit (0);
   }

   if( 2 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         mpegDemux_t demuxer( fIn.getData(), fIn.getLength() );
   
         int const dspFd = open( "/dev/dsp", O_WRONLY );
         if( 0 <= dspFd )
         {
            int const format = AFMT_S16_LE ;
            if( 0 != ioctl( dspFd, SNDCTL_DSP_SETFMT, &format) ) 
               fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
   
            int const vol = 0x5050 ;
            if( 0 > ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
               perror( "Error setting volume" );

            time_t start ; time( &start );
            mpegDemux_t::bulkInfo_t const * const bi = demuxer.getFrames();
            
            for( unsigned char sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
            {
               mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[sIdx];
               mpegDemux_t::frameType_e const ft = sAndF.sInfo_.type ;
               printf( "%s stream %u: %u frames\n", frameTypes_[ft], sIdx, sAndF.numFrames_ );
               switch( ft )
               {
                  case mpegDemux_t::videoFrame_e :
                  {
long long const start = tickMs();
                     mpegDecoder_t videoDecode ;
                     fbDevice_t    &fb = getFB();
                     videoQueue_t  *vidQueue = 0 ;
                     long long      firstPTS = 0 ;
                     long long      lastPTS = 0 ;
                     long long      prevTick = 0 ;
                     long long      lastInPTS = 0 ;
                     for( unsigned f = 0 ; f < sAndF.numFrames_ ; f++ )
                     {
                        mpegDemux_t::frame_t const &frame = sAndF.frames_[f];
                        if( 0 != frame.when_ms_ )
                           lastPTS = frame.when_ms_ ;

                        videoDecode.feed( frame.data_, frame.length_ );
                        void const *picture ;
                        mpegDecoder_t::picType_e type ;
                        while( videoDecode.getPicture( picture, type, videoDecode.ptAll_e ) )
                        {
//static char const picTypes_[] = { 'I', 'P', 'B', 'D' };
//printf( "%c", picTypes_[type] ); fflush( stdout );
                           if( 0 == vidQueue )
                           {
                              firstPTS = lastPTS ;
                              prevTick = tickMs()+firstPTS ;
                              unsigned const width  = ( videoDecode.width() < fb.getWidth() )
                                                      ? videoDecode.width()
                                                      : fb.getWidth();
                              unsigned const height = ( videoDecode.height() < fb.getHeight() )
                                                      ? videoDecode.height()
                                                      : fb.getHeight();
                              vidQueue = new videoQueue_t( width, height );
//                              printf( "vQueue : %u x %u, stride %u\n", vidQueue->width_, vidQueue->height_, vidQueue->rowStride_ );
                           }

                           videoQueue_t :: entry_t *entry ;
                           if( !vidQueue->isFull() )
                              entry = vidQueue->getEmpty();
                           else
                           {
                              entry = vidQueue->getFull();
                              unsigned char const *src = entry->data_ ;
                              for( unsigned i = 0 ; i < videoDecode.height() ; i++, src += vidQueue->rowStride_ )
                              {
                                 memcpy( fb.getRow(i), src, vidQueue->rowStride_ );
                              }
//                              printf( "out %u %p/%llu\n", entry->type_, entry, entry->when_ms_ );
                           }

                           if( entry )
                           {
/*
                              if( lastPTS < lastInPTS )
                                 lastPTS = lastInPTS ;
                              if( lastPTS >= lastInPTS )
*/
                              {
                                 lastInPTS = lastPTS ;
                                 entry->when_ms_ = lastPTS ;
                                 entry->type_    = type ;
                                 unsigned char *dest = entry->data_ ;
                                 unsigned char const *src = (unsigned char *)picture ;
                                 unsigned const imgStride = videoDecode.width()*2 ;
                                 for( unsigned i = 0 ; i < vidQueue->height_ ; i++, dest += vidQueue->rowStride_, src += imgStride )
                                 {
                                    memcpy( dest, src, vidQueue->rowStride_ );
                                 }
                                 vidQueue->putFull( entry );
                              }
/*
                              else
                              {
                                 vidQueue->putEmpty( entry );
                                 printf( "skip %u %llu\n", type, lastPTS );
                              } // skip out-of-order frames
                              printf( ".in %u %p/%llu\n", type, entry, entry->when_ms_ );
*/                              
                           }
                           else
                              fprintf( stderr, "video allocation error\n" );
                        }
                     }
long long const end = tickMs();
                     unsigned long long msElapsed = end-start ;
                     printf( "%llu.%03llu seconds elapsed\n", msElapsed / 1000, msElapsed % 1000 );
                     msElapsed = lastPTS-firstPTS ;
                     printf( "%llu.%03llu seconds of video\n", msElapsed / 1000, msElapsed % 1000 );
                     break;
                  }
   
                  case mpegDemux_t::audioFrame_e :
                  {
                     audio_buf_info ai ;
                     if( 0 == ioctl( dspFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
                     {
                        //
                        // Because I'm lazy and can't figure out the
                        // right set of loops for this, I'm bailing out
                        // and making this a state machine of sorts 
                        // based on four inputs:
                        //
                        //    deviceEmpty - set when fragments==fragstotal
                        //    deviceAvail - set when fragments > 0
                        //    fragfull    - have a full PCM buffer to write
                        //    mp3Avail    - set when we have more input frames
                        //
                        // deviceEmpty    deviceAvail   fragfull    mp3Avail
                        //      0             0             0           0           wait for deviceEmpty
                        //      0             0             0           1           pull into frag
                        //      0             0             1           0           wait for deviceEmpty
                        //      0             0             1           1           wait for fragsavail
                        //      0             1             0           0           wait for deviceEmpty
                        //      0             1             0           1           pull into frag
                        //      0             1             1           0           write to device, wait for deviceEmpty
                        //      0             1             1           1           write to device, pull into frag
                        //      1             1             0           0           done
                        //      1             1             0           1           pull into frag
                        //      1             1             1           0           write to device
                        //      1             1             1           1           write to device, pull into frag
                        // 
   #define DEVICEEMPTY  0x08
   #define DEVICEAVAIL  0x04
   #define FRAGFULL     0x02
   #define MP3AVAIL     0x01
   #define MP3DONE      (DEVICEEMPTY|DEVICEAVAIL)
      
                        unsigned      frameIdx = 0 ;
                        long long     startPTS = 0 ;
                        unsigned char state = (frameIdx < sAndF.numFrames_)
                                              ? MP3AVAIL   // this will force a check of the device (if data avail)
                                              : MP3DONE ;
                        madDecoder_t mp3Decode ;
                        time_t startDecode ; time( &startDecode );
                        unsigned short * samples = 0 ;
                        unsigned         numFilled = 0 ;
                        unsigned         maxSamples = 0 ;
                        bool             firstFrame = true ;
      
                        while( MP3DONE != state )
                        {
                           //
                           // build a new fragment for output
                           //
                           while( MP3AVAIL == ( state & (FRAGFULL|MP3AVAIL) ) )
                           {
                              if( 0 == samples )
                              {
                                 maxSamples = ai.fragsize/sizeof( samples[0] );
                                 samples = new unsigned short [maxSamples];
                                 numFilled = 0 ;
                              }
   
                              if( 0 < mp3Decode.numSamples() )
                              {
                                 unsigned numRead ;
                                 if( mp3Decode.readSamples( samples+numFilled, maxSamples-numFilled, numRead ) )
                                 {
                                    numFilled += numRead ;
                                    if( maxSamples == numFilled )
                                    {
                                       state |= FRAGFULL ;
                                       if( firstFrame )
                                       {
                                          firstFrame = false ;
                                          unsigned nChannels = mp3Decode.numChannels();
                                          unsigned sampleRate = mp3Decode.sampleRate();
                                          printf( "mad header: %u channels, %u Hz\n", nChannels, sampleRate );
                                          if( 0 != ioctl( dspFd, SNDCTL_DSP_CHANNELS, &nChannels ) )
                                             fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
                                          if( 0 != ioctl( dspFd, SNDCTL_DSP_SPEED, &sampleRate ) )
                                             fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );
printf( "starting at pts %llu\n", startPTS );
                                       }
                                    }
                                 }
                                 else
                                 {
                                 }
                              } // tail end of previous buffer
                              else if( mp3Decode.getData() )
                              {
                              } // synth next frame?
                              else if( frameIdx < sAndF.numFrames_ )
                              {
                                 mpegDemux_t::frame_t const &frame = sAndF.frames_[frameIdx];
                                 if( ( 0 == startPTS ) && ( 0 != frame.when_ms_ ) )
                                    startPTS = frame.when_ms_ ;
                                 mp3Decode.feed( frame.data_, frame.length_ );
                                 frameIdx++ ;
                              } // feed mp3 decoder
                              else
                              {
                                 state &= ~MP3AVAIL ;
                                 if( 0 < numFilled )
                                 {
                                    memset( samples+numFilled, 0, (maxSamples-numFilled)*sizeof(samples[0]) );
                                    numFilled = maxSamples ;
                                    state |= FRAGFULL ;
                                 }
                              }
                           } // read MP3 input
   
                           if( (DEVICEAVAIL|FRAGFULL) == (state & (DEVICEAVAIL|FRAGFULL)) )
                           {
                              unsigned const bytesToWrite = numFilled*sizeof(samples[0]);
                              int numWritten = write( dspFd, samples, bytesToWrite );
                              if( numWritten != bytesToWrite )
                              {
                                 fprintf( stderr, "!!! short write !!!\n" );
                                 return 0 ;
                              }
   
                              numFilled = 0 ;
                              state &= ~FRAGFULL ;
                           } // write data!!!
   
                           if( FRAGFULL == (state & (DEVICEAVAIL|FRAGFULL)) )
                           {
                              pollfd filedes ;
                              filedes.fd = dspFd ;
                              filedes.events = POLLOUT ;
                              if( 0 < poll( &filedes, 1, 1 ) )
                              {
                                 if( 0 == ioctl( dspFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
                                 {
                                    if( ai.fragments == ai.fragstotal )
                                       state |= DEVICEEMPTY ;
                                    else
                                       state &= ~DEVICEEMPTY ;
                                    if( 0 < ai.fragments )
                                       state |= DEVICEAVAIL ;
                                    else
                                       state &= ~DEVICEAVAIL ;
                                 }
                                 else
                                 {
                                    perror( "OSPACE2" );
                                    return 0 ;
                                 }
                              }
                              else
                              {
                                 perror( "POLL" );
                                 return 0 ;
                              }
                           } // wait for device available
                           else if( 0 == (state & (FRAGFULL|MP3AVAIL)) )
                           {
                              usleep( 10000 );
                              if( 0 == ioctl( dspFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
                              {
                                 if( ai.fragments == ai.fragstotal )
                                    state |= DEVICEEMPTY ;
                                 else
                                    state &= ~DEVICEEMPTY ;
                                 if( 0 < ai.fragments )
                                    state |= DEVICEAVAIL ;
                                 else
                                    state &= ~DEVICEAVAIL ;
                              }
                              else
                              {
                                 perror( "OSPACE3" );
                                 return 0 ;
                              }
                           } // no more data, wait for empty
                        } // while !done
                     }
                     else
                        perror( "GETOSPACE" );
                     
   //                  char inBuf[80];
   //                  gets( inBuf );
                     break;
                  }
               }
            } // for each stream in the file
   
   //      dumpTraces();
   
            time_t end ; time( &end );
            printf( "%lu seconds elapsed\n", end-start );
   
            close( dspFd );
   
         }
         else
         {
            perror( "opening /dev/dsp" );
            return 0 ;
         }
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: mpDemux fileName\n" );

   return 0 ;
}

