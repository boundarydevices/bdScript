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
#include "videoFrames.h"
#include <pthread.h>

static char const * const frameTypes_[] = {
   "video",
   "audio",
   "endOfFile"
};

struct playbackArgs_t {
   mpegDemux_t::streamAndFrames_t const *audioFrames_ ;
   mpegDemux_t::streamAndFrames_t const *videoFrames_ ;
};

static void *videoThreadRtn( void *arg )
{
   videoFrames_t &frames = *( videoFrames_t *) arg ;
   fbDevice_t    &fb = getFB();
   videoQueue_t :: entry_t *entry ;
   unsigned picCount = 0 ;
//   frames.setStart( tickMs() );
   unsigned const rowStride = frames.getRowStride();
   unsigned const height    = frames.getHeight();
   unsigned const fbStride  = 2*fb.getWidth();
   while( frames.pull( entry ) )
   {
      unsigned char *fbMem = (unsigned char *)fb.getMem();
      unsigned char const *dataMem = entry->data_ ;
      for( unsigned i = 0 ; i < height ; i++ )
      {
         memcpy( fbMem, dataMem, rowStride );
         fbMem += fbStride ;
         dataMem += rowStride ;
      }
//      memcpy( fb.getMem(), entry->data_, fb.getMemSize() );
      picCount++ ;
   }

   printf( "%u pictures\n", picCount );
}

static void *playbackThread( void *arg )
{
   playbackArgs_t const &args = *( playbackArgs_t const * )arg ;
   int const dspFd = open( "/dev/dsp", O_WRONLY );
   if( 0 <= dspFd )
   {
      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( dspFd, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );

      int const vol = 0x5050 ;
      if( 0 > ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
         perror( "Error setting volume" );

      audio_buf_info ai ;
      if( 0 == ioctl( dspFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
      {
         fbDevice_t    &fb = getFB();
         videoFrames_t vFrames( *args.videoFrames_, fb.getWidth(), fb.getHeight() );
         if( vFrames.preload() )
         {
            unsigned picCount = 0 ;
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
            mpegDemux_t::streamAndFrames_t const &audioFrames = *args.audioFrames_ ;
            unsigned char state = (frameIdx < audioFrames.numFrames_)
                                  ? MP3AVAIL   // this will force a check of the device (if data avail)
                                  : MP3DONE ;
            madDecoder_t mp3Decode ;
            time_t startDecode ; time( &startDecode );
            unsigned short * samples = 0 ;
            unsigned         numFilled = 0 ;
            unsigned         maxSamples = 0 ;
            bool             firstFrame = true ;
            pthread_t        videoThread = 0 ;

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
                  else if( frameIdx < audioFrames.numFrames_ )
                  {
                     mpegDemux_t::frame_t const &frame = audioFrames.frames_[frameIdx];
                     if( ( 0 == startPTS ) && ( 0 != frame.when_ms_ ) )
                        startPTS = frame.when_ms_ ;
                     mp3Decode.feed( frame.data_, frame.length_ );
                     frameIdx++ ;

                     if( 0 == videoThread )
                     {
                        struct sched_param tsParam ;
                        tsParam.__sched_priority = 1 ;
                        pthread_setschedparam( pthread_self(), SCHED_FIFO, &tsParam );
      
                        vFrames.setStart( tickMs() - 2*startPTS );
   
                        int const create = pthread_create( &videoThread, 0, videoThreadRtn, &vFrames );
                        if( 0 == create )
                           printf( "thread started at %lld\n", tickMs() );
                        else
                           perror( "vThread" );
      
                     }

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
                  if( 0 < poll( &filedes, 1, 0 ) )
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
            
            printf( "%u pictures\n", picCount );
            if( videoThread )
            {
               void *exitStat ;
               pthread_join( videoThread, &exitStat );
            }
         }
         else
            fprintf( stderr, "Error preloading video frames\n" );
      }
      else
         perror( "GETOSPACE" );
   }
   else
      perror( "/dev/dsp" );
}

int main( int argc, char const * const argv[] )
{
   if( 2 == argc )
   {
      memFile_t fIn( argv[1] );
      if( fIn.worked() )
      {
         mpegDemux_t demuxer( fIn.getData(), fIn.getLength() );

         mpegDemux_t::bulkInfo_t const * const bi = demuxer.getFrames();
         playbackArgs_t playbackArgs = { 0, 0 };

         for( unsigned char sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
         {
            mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[sIdx];
            mpegDemux_t::frameType_e const ft = sAndF.sInfo_.type ;
            printf( "%s stream %u: %u frames\n", frameTypes_[ft], sIdx, sAndF.numFrames_ );
            switch( ft )
            {
               case mpegDemux_t::videoFrame_e :
               {
                  playbackArgs.videoFrames_ = bi->streams_[sIdx];
                  break;
               }

               case mpegDemux_t::audioFrame_e :
               {
                  playbackArgs.audioFrames_ = bi->streams_[sIdx];
                  break;
               }
            }
         } // for each stream in the file

//      dumpTraces();

         if( ( 0 != playbackArgs.audioFrames_ )
             &&
             ( 0 != playbackArgs.videoFrames_ ) )
         {
            printf( "0x%llx milli-seconds total\n", bi->msTotal_ );
            printf( "%llu.%03llu seconds total\n", bi->msTotal_ / 1000, bi->msTotal_ % 1000 );
            long long msStart = tickMs();
            playbackThread( &playbackArgs );
            long long msEnd = tickMs();
            unsigned long long msElapsed = msEnd-msStart ;
            printf( "%llu.%03llu seconds elapsed\n", msElapsed / 1000, msElapsed % 1000 );
         }
      }
      else
         perror( argv[1] );
   }
   else
      fprintf( stderr, "Usage: mpDemux fileName\n" );

   return 0 ;
}

