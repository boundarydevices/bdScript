/*
 * Module audioQueue.cpp
 *
 * This module defines the methods of the audioQueue_t
 * class as declared in audioQueue.h
 *
 *
 * Change History : 
 *
 * $Log: audioQueue.cpp,v $
 * Revision 1.39  2006-05-14 14:43:17  ericn
 * -expose fd routines, timestamp variables
 *
 * Revision 1.38  2005/11/06 20:26:25  ericn
 * -conditional FFT
 *
 * Revision 1.37  2005/11/06 00:49:22  ericn
 * -more compiler warning cleanup
 *
 * Revision 1.36  2005/04/24 18:51:14  ericn
 * -add SETPLANE call for sm501yuv
 *
 * Revision 1.35  2004/10/30 19:37:33  ericn
 * -Neon yuv support
 *
 * Revision 1.34  2004/03/17 04:56:19  ericn
 * -updates for mini-board (no sound, video, touch screen)
 *
 * Revision 1.33  2004/01/02 23:37:13  ericn
 * -debugPrint()
 *
 * Revision 1.32  2003/09/26 00:43:50  tkisky
 * -fft stuff
 *
 * Revision 1.31  2003/09/22 02:02:03  ericn
 * -separated boost and changed record level params
 *
 * Revision 1.30  2003/09/15 02:22:49  ericn
 * -added settable record amplification
 *
 * Revision 1.29  2003/08/06 13:24:14  ericn
 * -fixed audio playback for flash
 *
 * Revision 1.28  2003/08/04 12:37:51  ericn
 * -added raw MP3 (for flash)
 *
 * Revision 1.27  2003/08/02 19:30:03  ericn
 * -modified to allow clipping of video
 *
 * Revision 1.26  2003/08/02 16:13:08  ericn
 * -fixed memory leak for audio samples
 *
 * Revision 1.25  2003/08/01 14:26:28  ericn
 * -better error msg
 *
 * Revision 1.24  2003/07/30 20:26:03  ericn
 * -added MPEG support
 *
 * Revision 1.23  2003/07/29 20:03:56  tkisky
 * -close dsp after successful getVolume call
 *
 * Revision 1.22  2003/02/24 03:37:49  ericn
 * -
 *
 * Revision 1.21  2003/02/08 19:52:43  ericn
 * -removed debug msgs
 *
 * Revision 1.20  2003/02/08 14:56:42  ericn
 * -added mixer fd (to no avail)
 *
 * Revision 1.19  2003/02/02 23:30:46  ericn
 * -added read setup ioctls
 *
 * Revision 1.18  2003/02/02 21:00:50  ericn
 * -modified to set secsRecorded
 *
 * Revision 1.17  2003/02/02 13:46:17  ericn
 * -added recordBuffer support
 *
 * Revision 1.16  2003/02/01 18:15:29  ericn
 * -preliminary wave file and record support
 *
 * Revision 1.15  2003/01/12 03:04:26  ericn
 * -put sync back
 *
 * Revision 1.14  2003/01/05 01:46:27  ericn
 * -removed SYNC call on completion
 *
 * Revision 1.13  2002/12/15 00:07:58  ericn
 * -removed debug msgs
 *
 * Revision 1.12  2002/12/02 15:14:35  ericn
 * -modified to use pull instead of twiddlin' internals of queue
 *
 * Revision 1.11  2002/12/01 03:13:52  ericn
 * -modified to root objects through audio queue
 *
 * Revision 1.10  2002/11/30 18:52:57  ericn
 * -modified to queue jsval's instead of strings
 *
 * Revision 1.9  2002/11/30 00:53:43  ericn
 * -changed name to semClasses.h
 *
 * Revision 1.8  2002/11/24 19:08:19  ericn
 * -fixed output queueing
 *
 * Revision 1.7  2002/11/15 14:40:10  ericn
 * -modified to leave speed alone when possible
 *
 * Revision 1.6  2002/11/14 14:55:48  ericn
 * -fixed mono output
 *
 * Revision 1.5  2002/11/14 14:44:40  ericn
 * -fixed clear() routine, modified to always use stereo
 *
 * Revision 1.4  2002/11/14 13:14:08  ericn
 * -modified to expose dsp file descriptor
 *
 * Revision 1.3  2002/11/07 14:39:07  ericn
 * -added audio output, buffering and scheduler spec
 *
 * Revision 1.2  2002/11/07 02:18:13  ericn
 * -fixed shutdown_ flag
 *
 * Revision 1.1  2002/11/07 02:16:33  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */


#include "audioQueue.h"
#include "semClasses.h"
#include <unistd.h>
#include "codeQueue.h"
#include "mad.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#include "madHeaders.h"
#include "jsGlobals.h"
#include "mpegDecode.h"
#include "madDecode.h"
#include <zlib.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <poll.h>
#include "videoQueue.h"
#include "tickMs.h"
#include "videoFrames.h"
#include <pthread.h>
#include "fbDev.h"
#include "debugPrint.h"
typedef int irqreturn_t ;
#include "linux/sm501yuv.h"

static bool volatile _cancel = false ;
static bool volatile _playing = false ;
static bool volatile _recording = false ;
static audioQueue_t *audioQueue_ = 0 ; 
static mutex_t       openMutex_ ;
timeval lastPlayStart_ = { 0 };
timeval lastPlayEnd_ = { 0 };
                  
#define OUTBUFSIZE 8192

static int readFd_ = -1 ;
static int readFdRefs_ = 0 ;
static int writeFd_ = -1 ;
static int writeFdRefs_ = 0 ;

int openReadFd( void )
{
   mutexLock_t lock( openMutex_ );

   if( 0 > readFd_ )
      readFd_ = open( "/dev/dsp", O_RDONLY );
   
   if( 0 <= readFd_ )
      ++readFdRefs_ ;

   return readFd_ ;
}

void closeReadFd( void )
{
   mutexLock_t lock( openMutex_ );
   assert( 0 < readFdRefs_ );
   assert( 0 <= readFd_ );
   --readFdRefs_ ;
   if( 0 == readFdRefs_ )
   {
      close( readFd_ );
      readFd_ = -1 ;
   } // last close
}

int openWriteFd( void )
{
   mutexLock_t lock( openMutex_ );

   if( 0 > writeFd_ )
      writeFd_ = open( "/dev/dsp", O_WRONLY );
   
   if( 0 <= writeFd_ ){
      ++writeFdRefs_ ;
debugPrint( "Audio opened fd %d for write %d\n", writeFd_, writeFdRefs_ );
   }

   return writeFd_ ;
}

void closeWriteFd( void )
{
   mutexLock_t lock( openMutex_ );
   assert( 0 < writeFdRefs_ );
   assert( 0 <= writeFd_ );
   --writeFdRefs_ ;
   if( 0 == writeFdRefs_ )
   {
      close( writeFd_ );
      writeFd_ = -1 ;
   } // last close
//   fprintf( stderr, "Audio closed for write %d\n", writeFdRefs_ );
}

unsigned char getVolume( void )
{
   int vol = 0;
   int const dspFd = openWriteFd();
   if( 0 <= dspFd )
   {
      if( 0 <= ioctl( dspFd, SOUND_MIXER_READ_VOLUME, &vol)) vol &= 0xFF;
      else perror( "SOUND_MIXER_READ_VOLUME" );
      closeWriteFd();
   }
   else
      perror( "audioWriteFd" );
   
   return vol ;
}

void setVolume( unsigned char volParam )
{
   int const vol = (volParam << 8) | volParam ;
   int const dspFd = openWriteFd();
   if( 0 <= dspFd )
   {
      if( 0 > ioctl( dspFd, SOUND_MIXER_WRITE_VOLUME, &vol)) 
         perror( "Error setting volume" );
      closeWriteFd();
   }
   else
      perror( "audioWriteFd" );
}

//
// This number is magic and was determined by the scientific process
// of trying to record something soft.
//
// It is used to keep the slightest bit of background noise from being
// amplified to full-scale.
//
static void normalize( short int *samples,
                       unsigned   numSamples )
{
	long const maxNormalizeRatio = 0x800000 ;//* 128 max
	signed short *next = samples ;
	signed short min = *next;
	signed short max = *next++;
	unsigned i;
	signed long ratio;
	if (!numSamples) return;

	for( i = 1 ; i < numSamples ; i++ )
	{
		signed short s = *next++ ;
		if( max < s) max = s ;
		else if( min > s ) min = s ;
	}
	min = 0-min ;
	max = max > min ? max : min ;
   printf( "max sample %d\n", max );
	if (!max) return;

	ratio = ( 0x70000000UL / max );
	if (ratio > 0x10000) {	//don't reduce volume
		if( ratio > maxNormalizeRatio ) ratio = maxNormalizeRatio ;
   printf( "ratio %lx\n", ratio );
		next = samples ;
		for( i = 0 ; i < numSamples ; i++ ) {
			signed long x16 = ratio * ((signed long)*next) ;
			x16 += 1<<15;	//round
			*next++ = (signed short)( x16 >> 16 );
		}
	}
}

static void audioCallback( void *cbParam )
{
   audioQueue_t::item_t *item = (audioQueue_t::item_t *)cbParam ;

   if( audioQueue_t::wavRecord_ == item->type_ )
   {
      JS_DefineProperty( execContext_, 
                         item->obj_, 
                         "isRecording",
                         JSVAL_FALSE,
                         0, 0, 
                         JSPROP_ENUMERATE
                         |JSPROP_PERMANENT
                         |JSPROP_READONLY );
      audioQueue_t::waveHeader_t const &header = *(audioQueue_t::waveHeader_t const *)item->data_ ;
      if( ( 0 < header.sampleRate_ ) && ( 0 < header.numSamples_ ) )
      {
         double secs = (1.0*header.numSamples_)/header.sampleRate_ ;
         jsdouble *jsSecs = JS_NewDouble( execContext_, secs );
         JS_DefineProperty( execContext_, 
                            item->obj_, 
                            "secsRecorded",
                            DOUBLE_TO_JSVAL( jsSecs ),
                            0, 0, 
                            JSPROP_ENUMERATE
                            |JSPROP_PERMANENT
                            |JSPROP_READONLY );
      }
   }

   jsval handler = item->isComplete_ ? item->onComplete_ : item->onCancel_ ;

   if( JSVAL_VOID != handler )
   {
      executeCode( item->obj_, handler, "audioQueue" );
   }

   JS_RemoveRoot( execContext_, &item->obj_ );
   JS_RemoveRoot( execContext_, &item->onComplete_ );
   JS_RemoveRoot( execContext_, &item->onCancel_ );

   delete item ;
}

inline unsigned short scale( mad_fixed_t sample )
{
   return (unsigned short)( sample >> (MAD_F_FRACBITS-15) );
}

struct playbackStreams_t {
   mpegDemux_t::streamAndFrames_t const *audioFrames_ ;
   mpegDemux_t::streamAndFrames_t const *videoFrames_ ;
};

struct videoParams_t {
   videoFrames_t &frames_ ;
   unsigned       x_ ;
   unsigned       y_ ;
   unsigned       width_ ;
   unsigned       height_ ;
};

static void *videoThreadRtn( void *arg )
{
   videoParams_t const &params = *(videoParams_t const *)arg ;
   videoFrames_t &frames = params.frames_ ;
printf( "play video at %u:%u, w:%u, h:%u\n", params.x_, params.y_, params.width_, params.height_ );
   unsigned picCount = 0 ;
   unsigned const rowStride = frames.getRowStride();
   unsigned const height    = frames.getHeight();

#ifndef NEON
   fbDevice_t    &fb = getFB();
   videoQueue_t :: entry_t *entry ;
//   frames.setStart( tickMs() );
   unsigned const fbStride  = 2*fb.getWidth();
   unsigned char *fbStart = (unsigned char *)fb.getRow(params.y_) 
                            + params.x_ * 2 ;

   while( !_cancel && frames.pull( entry ) )
   {
      unsigned char *fbMem = fbStart ;
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
#else
   
   int const fdYUV = open( "/dev/yuv", O_WRONLY );
   if( 0 <= fdYUV )
   {
printf( "input row stride: %u\n", rowStride );
printf( "frame size: %u x %u\n", frames.maxWidth_, frames.maxHeight_ );
printf( "queue size: %u x %u, %u, %u\n", 
      frames.queue_->width_, 
      frames.queue_->height_,
      frames.queue_->rowStride_,
      frames.queue_->entrySize_ );
      
      struct sm501yuvPlane_t pi ;
      pi.xLeft_     = params.x_ ;
      pi.yTop_      = params.y_ ;
      pi.inWidth_   = frames.queue_->width_ ;
      pi.inHeight_  = frames.queue_->height_ ;
      pi.outWidth_  = params.width_ ;
      pi.outHeight_ = params.height_ ;
      
      int const ior = ioctl( fdYUV, SM501YUV_SETPLANE, &pi );
      printf( "setplane: %d\n", ior );

      unsigned const bytesPerFrame = rowStride*height ;
printf( "%u bytes/frame\n", bytesPerFrame );
      
      videoQueue_t :: entry_t *entry ;
      while( !_cancel && frames.pull( entry ) )
      {
         unsigned char const *dataMem = entry->data_ ;
         write( fdYUV, dataMem, bytesPerFrame );
         picCount++ ;
      }

      close( fdYUV );
   }
   else
      perror( "/dev/yuv" );
#endif 
   printf( "%u pictures\n", picCount );
   
   return 0 ;
}

void audioQueue_t::GetAudioSamples(const int readFd,waveHeader_t* header)
{
	unsigned char *nextSample = (unsigned char *)header->samples_ ;
	unsigned long const maxSamples = header->numSamples_ ;
	unsigned long bytesLeft = maxSamples * sizeof( header->samples_[0] );
	unsigned const maxRead = readFragSize_ ;
	while( !( _cancel || shutdown_ ) && ( 0 < bytesLeft ) ) {
		int const readSize = ( bytesLeft > maxRead ) ? maxRead : bytesLeft ;
		int numRead = read( readFd, nextSample, readSize );
		if( 0 < numRead ) {
			bytesLeft -= numRead ;
			nextSample += numRead ;
		} else {
			perror( "audio read" );
			break;
		}
	}
	header->numSamples_ = ( maxSamples*sizeof(header->samples_[0]) - bytesLeft ) / sizeof( header->samples_[0] );
}

#ifdef FILTERAUDIO
void audioQueue_t::GetAudioSamples2(const int readFd,waveHeader_t* header)
{
	const int logN=cnw->logN;	//1024 points
	const int n=(1<<logN);
	const int n_d2=(1<<(logN-1));
//	const int n_d4=(1<<(logN-2));
	const int n3_d4=(3<<(logN-2));
	int i;

	unsigned char* inSamples;
	short* outSamples = (short *)header->samples_;
	int outSampleCntMax = header->numSamples_;

	int blkSize=readFragSize_;
	int readPos=0;
	int writePos=0;
	const unsigned int fmask = (n<<1)-1;
	int outSampleCnt = 0;
	if (readFd<0) return;


	printf("blkSize:%i\r\n",blkSize);
	blkSize = (blkSize+fmask) & ~fmask;	//round to multiple of fft
	printf("blkSize:%i\r\n",blkSize);
	inSamples = (unsigned char *)malloc(blkSize);
	if (inSamples==NULL) return;

	memset(outSamples,0,MAX_ADD_SIZE*sizeof(*outSamples));

	while ( !( _cancel || shutdown_ ) && (outSampleCnt < outSampleCntMax) ) {
		int numRead = 0;
//		printf("readPos:%i outSampleCnt:%i, max:%i\r\n",readPos,outSampleCnt,outSampleCntMax);
		while ((((writePos-readPos)& (blkSize-1))+numRead)<n) {
			int max;

			writePos=(writePos+numRead)& (blkSize-1);
			numRead=0;

			if (readPos > writePos) max = (readPos-writePos);
			else max = blkSize-writePos;

//			printf("max:%i\r\n",max);
#if 1
			numRead = read(readFd, &inSamples[writePos], max );
#else
			for (i=0,pSamples=(short*)(&inSamples[writePos]); i<(max>>1); i++) {*pSamples++ = 0x4000;}
//			for (i=0,rSamples=(short*)(&inSamples[writePos]); i<(max>>1); i++) {printf("__%i %i\r\n",i,*rSamples++);}
			numRead = max;
#endif
//			printf("numRead:%i\r\n",numRead);
			if (numRead<=0 ) return;
		}
		writePos=(writePos+numRead)& (blkSize-1);


		//shifts needed to convert from char oriented to short oriented
		i = CleanNoise(outSamples+outSampleCnt,outSampleCntMax-outSampleCnt,(short*)inSamples,(readPos>>1),(blkSize>>1)-1,cnw);
		if (i<0) break;
		{
			int max = outSampleCntMax - outSampleCnt;
			if (max>i) max = i;
			outSampleCnt += max;
		}
		readPos =(readPos+SAMPLE_ADVANCE)& (blkSize-1);
//		printf("readPos:%i,outSampleCnt*2:%i\r\n",readPos,outSampleCnt<<1);
	}
	printf("\r\n");
//	printf("%04x %04x %04x %04x\r\n",outSamples[0],outSamples[1],outSamples[2],outSamples[3]);
//	WriteWavFile(outSamples,outSampleCnt);
	free(inSamples);

	header->numSamples_ = outSampleCnt;
}

#endif

//
// Because I'm lazy and can't figure out the
// right set of loops for putting audio out,
// I'm bailing out and making this a state
// machine of sorts based on four inputs:
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

void *audioThread( void *arg )
{
debugPrint( "audioThread %p (id %x)\n", &arg, pthread_self() );
   audioQueue_t *queue = (audioQueue_t *)arg ;

   int writeFd = openWriteFd();
   if( 0 <= writeFd )
   {
      if( 0 != ioctl(writeFd, SNDCTL_DSP_SYNC, 0 ) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
            
      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( writeFd, SNDCTL_DSP_SETFMT, &format) ) 
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
               
      int const channels = 2 ;
      if( 0 != ioctl( writeFd, SNDCTL_DSP_CHANNELS, &channels ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
      
      closeWriteFd();
   }
   else
      fprintf( stderr, "Error %m opening audio device\n" );

   int lastSampleRate = -1 ;
   int lastRecordRate = -1 ;

   unsigned short * const outBuffer = new unsigned short [ OUTBUFSIZE ];
   unsigned short *       nextOut = outBuffer ;
   unsigned short         spaceLeft = OUTBUFSIZE ;

   while( !queue->shutdown_ )
   {
      audioQueue_t :: item_t *item ;
      if( queue->pull( item ) )
      {
         if( audioQueue_t :: mp3Play_ == item->type_ )
         {
            gettimeofday( &lastPlayStart_, 0 );
            writeFd = openWriteFd();
            if( 0 < writeFd )
            {
               unsigned long numWritten = 0 ;
               _playing = true ;

               madHeaders_t headers( item->data_, item->length_ );
               if( headers.worked() )
               {

/*
                  printf( "playback %u bytes (%lu seconds) from %p here\n", 
                          item->length_, 
                          headers.lengthSeconds(),
                          item->data_ );
*/

                  struct mad_stream stream;
                  struct mad_frame	frame;
                  struct mad_synth	synth;
                  mad_stream_init(&stream);
                  mad_stream_buffer(&stream, (unsigned char const *)item->data_, item->length_ );
                  mad_frame_init(&frame);
                  mad_synth_init(&synth);
   
                  bool eof = false ;
                  unsigned short nChannels = 0 ;               
   
                  spaceLeft = OUTBUFSIZE ;
                  nextOut = outBuffer ;

unsigned wrote = 0 ;   
                  unsigned frameId = 0 ;
                  do {
                     if( -1 != mad_frame_decode(&frame, &stream ) )
                     {
                        if( 0 == frameId++ )
                        {
                           int sampleRate = frame.header.samplerate ;
                           if( sampleRate != lastSampleRate )
                           {   
                              lastSampleRate = sampleRate ;
                              if( 0 != ioctl( writeFd, SNDCTL_DSP_SPEED, &sampleRate ) )
                              {
                                 fprintf( stderr, "Error setting sampling rate to %d:%m\n", sampleRate );
                                 break;
                              }
                           }

                           nChannels = MAD_NCHANNELS(&frame.header) ;

                        } // first frame... check sample rate
                        else
                        {
                        } // not first frame

                        mad_synth_frame( &synth, &frame );
                        if( 1 == nChannels )
                        {
                           mad_fixed_t const *left = synth.pcm.samples[0];
                           for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                           {
                              unsigned short const sample = scale( *left++ );
                              *nextOut++ = sample ;
                              *nextOut++ = sample ;
                              spaceLeft -= 2 ;
                              if( 0 == spaceLeft )
                              {
                                 write( writeFd, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                                 numWritten += OUTBUFSIZE ;
                                 nextOut = outBuffer ;
                                 spaceLeft = OUTBUFSIZE ;
wrote += numWritten ;                                 
                              }
                           }
                        } // mono
                        else
                        {
                           mad_fixed_t const *left  = synth.pcm.samples[0];
                           mad_fixed_t const *right = synth.pcm.samples[1];
                           for( unsigned i = 0 ; i < synth.pcm.length ; i++ )
                           {
                              *nextOut++ = scale( *left++ );
                              *nextOut++ = scale( *right++ );
                              spaceLeft -= 2 ;
                              if( 0 == spaceLeft )
                              {
                                 write( writeFd, outBuffer, OUTBUFSIZE*sizeof(outBuffer[0]) );
                                 nextOut = outBuffer ;
                                 spaceLeft = OUTBUFSIZE ;
                                 numWritten += OUTBUFSIZE ;
                              }
                           }
                        } // stereo
                     } // frame decoded
                     else
                     {
                        if( MAD_RECOVERABLE( stream.error ) )
                           ;
                        else if( MAD_ERROR_BUFLEN == stream.error )
                        {
                           eof = true ;
                        }
                        else
                           break;
                     }
                  } while( !( eof || _cancel || queue->shutdown_ ) );

                  if( eof )
                  {
                     if( OUTBUFSIZE != spaceLeft )
                     {
                        write( writeFd, outBuffer, (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]) );
                        numWritten += OUTBUFSIZE-spaceLeft ;
                        wrote += (OUTBUFSIZE-spaceLeft);
                     } // flush tail end of output
                  }
//printf( "wrote %u samples, cancel %d, eof %d, shutdown %d\n", wrote, _cancel, eof, queue->shutdown_ );

                  /* close input file */
                  
                  mad_synth_finish( &synth );
                  mad_frame_finish( &frame );
                  mad_stream_finish(&stream);

                  gettimeofday( &lastPlayEnd_, 0 );

               }
               else
               {
                  fprintf( stderr, "Error parsing MP3 headers\n" );
               }
               
               if( !queue->shutdown_ )
               {
                  if( _cancel )
                  {
                     _cancel = false ;
//                     if( 0 != ioctl( writeFd, SNDCTL_DSP_RESET, 0 ) ) 
//                        fprintf( stderr, ":ioctl(SNDCTL_DSP_RESET):%m" );
                  }
                  else
                  {
                     if( 0 != ioctl( writeFd, SNDCTL_DSP_POST, 0 ) ) 
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_POST):%m" );
                     if( 0 != ioctl( writeFd, SNDCTL_DSP_SYNC, 0 ) ) 
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
                     item->isComplete_ = true ;
                  }
                  queueCallback( audioCallback, item );
               }
               
               _playing = false ;

               closeWriteFd();
            }
            else
               perror( "audioWriteFd" );

//            printf( "wrote %lu samples\n", numWritten );
         }
         else if( audioQueue_t :: mp3Raw_ == item->type_ )
         {
            gettimeofday( &lastPlayStart_, 0 );
            writeFd = openWriteFd();
            if( 0 < writeFd )
            {
               _playing = true ;
               
               audio_buf_info ai ;
               if( 0 == ioctl( writeFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
               {
                  madDecoder_t mp3Decode ;
                  unsigned short * samples = 0 ;
                  unsigned         numFilled = 0 ;
                  unsigned         maxSamples = 0 ;
                  mp3Decode.feed( item->data_, item->length_ );
                  bool firstFrame = true ;

                  unsigned char state = DEVICEEMPTY ;
                  while( !( _cancel || queue->shutdown_ )
                         &&
                         ( state != MP3DONE ) )
                  {
                     //
                     // build a new fragment for output
                     //
                     while( 0 == ( state & FRAGFULL ) )
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
                                    if( 0 != ioctl( writeFd, SNDCTL_DSP_CHANNELS, &nChannels ) )
                                       fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
                                    if( 0 != ioctl( writeFd, SNDCTL_DSP_SPEED, &sampleRate ) )
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
                        else if( 0 < numFilled )
                        {
                           memset( samples+numFilled, 0, (maxSamples-numFilled)*sizeof(samples[0]) );
                           numFilled = maxSamples ;
                           state |= FRAGFULL ;
                        } // fill to end of frag
                        else
                           break;
                     } // read MP3 input
            
                     if( (DEVICEAVAIL|FRAGFULL) == (state & (DEVICEAVAIL|FRAGFULL)) )
                     {
                        unsigned const bytesToWrite = numFilled*sizeof(samples[0]);
                        int numWritten = write( writeFd, samples, bytesToWrite );
                        if( (unsigned)numWritten != bytesToWrite )
                        {
                           fprintf( stderr, "!!! short write %u/%u\n", numWritten, bytesToWrite );
                           return 0 ;
                        }
            
                        numFilled = 0 ;
                        state &= ~FRAGFULL ;
                     } // write data!!!
            
                     if( FRAGFULL == (state & (DEVICEAVAIL|FRAGFULL)) )
                     {
                        pollfd filedes ;
                        filedes.fd = writeFd ;
                        filedes.events = POLLOUT ;
                        if( 0 < poll( &filedes, 1, 0 ) )
                        {
                           if( 0 == ioctl( writeFd, SNDCTL_DSP_GETOSPACE, &ai) )
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
                              break;
                           }
                        }
                     } // wait for device available
                     else if( 0 == (state & (FRAGFULL|MP3AVAIL)) )
                     {
                        usleep( 10000 );

                        if( 0 == ioctl( writeFd, SNDCTL_DSP_GETOSPACE, &ai) )
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
                           break;
                        }
                     } // no more data, wait for empty
                  } // while !done

                  gettimeofday( &lastPlayEnd_, 0 );

                  if( samples )
                     delete [] samples ;

                  _playing = false ;

                  if( !queue->shutdown_ )
                  {
                     if( _cancel )
                        _cancel = false ;
                     else
                        item->isComplete_ = true ;

                     if( item->callback_ )
                        item->callback_( item->callbackParam_ );
                  }
               }
               else
                  perror( "GETOSPACE" );

               closeWriteFd();
            }
            else
               perror( "audioWriteFd" );
         }
         else if( audioQueue_t::wavPlay_ == item->type_ )
         {
            writeFd = openWriteFd();
            if( 0 <= writeFd )
            {
               _playing = true ;

               audioQueue_t :: waveHeader_t const &header = *( audioQueue_t :: waveHeader_t const * )item->data_ ;
               bool eof = false ;

               spaceLeft = OUTBUFSIZE ;
               nextOut = outBuffer ;

               if( header.sampleRate_ != lastSampleRate )
               {
                  lastSampleRate = header.sampleRate_ ;
                  if( 0 != ioctl( writeFd, SNDCTL_DSP_SPEED, &lastSampleRate ) )
                     fprintf( stderr, "Error setting sampling rate to %d:%m\n", lastSampleRate );
               }

               if( 1 == header.numChannels_ )
               {
                  unsigned short const *nextIn = header.samples_ ;
                  unsigned i ;
                  for( i = 0 ; !( _cancel || queue->shutdown_ )
                               &&
                               ( i < header.numSamples_ ) ; i++ )
                  {
                     unsigned short const sample = *nextIn++ ;
                     *nextOut++ = sample ;
                     *nextOut++ = sample ;
                     spaceLeft -= 2 ;
                     if( 0 == spaceLeft )
                     {
                        int const outSize = OUTBUFSIZE*sizeof(outBuffer[0]);
                        int numWritten = write( writeFd, outBuffer, outSize );
                        if( numWritten == outSize )
                        {
                           numWritten += OUTBUFSIZE ;
                           nextOut = outBuffer ;
                           spaceLeft = OUTBUFSIZE ;
                        }
                        else
                           break ;
                     }
                  }
                  eof = ( i == header.numSamples_ )
                        &&
                        ( !_cancel )
                        &&
                        ( !queue->shutdown_ );

                  if( eof && ( spaceLeft != OUTBUFSIZE ) )
                  {
                     int const outSize = (OUTBUFSIZE-spaceLeft)*sizeof(outBuffer[0]);
                     int const numWritten = write( writeFd, outBuffer, outSize );
                     eof = ( outSize == numWritten );
                  } // flush tail of buffer
               }
               else
               {
                  int const outSize = header.numSamples_*sizeof(header.samples_[0])*header.numChannels_ ;
                  int const numWritten = write( writeFd, header.samples_, outSize );
                  eof = ( numWritten == outSize )
                        &&
                        ( !_cancel )
                        &&
                        ( !queue->shutdown_ );
               } // stereo, we can write directly

               gettimeofday( &lastPlayEnd_, 0 );

               _playing = false ;

               if( !queue->shutdown_ )
               {
                  if( _cancel )
                  {
                     _cancel = false ;
                     if( 0 != ioctl( writeFd, SNDCTL_DSP_RESET, 0 ) )
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_RESET):%m" );
                  }
                  else
                  {
                     if( 0 != ioctl( writeFd, SNDCTL_DSP_POST, 0 ) )
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_POST):%m" );
                     if( 0 != ioctl( writeFd, SNDCTL_DSP_SYNC, 0 ) )
                        fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC):%m" );
                     item->isComplete_ = true ;
                  }
                  queueCallback( audioCallback, item );
               }

               closeWriteFd();

            }
            else
               perror( "audioWriteFd" );
         }
         else if( audioQueue_t::wavRecord_ == item->type_ )
         {
            int const readFd = openReadFd();
            if( 0 <= readFd )
            {
               audioQueue_t :: waveHeader_t *header = ( audioQueue_t :: waveHeader_t * )item->data_ ;
               _recording = true ;
               if( header->sampleRate_ != lastRecordRate )
               {
                  lastRecordRate = header->sampleRate_ ;
                  if( 0 != ioctl( readFd, SNDCTL_DSP_SPEED, &lastRecordRate ) )
                     fprintf( stderr, "Error setting sampling rate to %d:%m\n", lastRecordRate );
               }

               header->numChannels_ = 1 ;
               
#ifdef FILTERAUDIO
               queue->GetAudioSamples2(readFd,header);
#else
               queue->GetAudioSamples(readFd,header);
#endif

//                normalize( (short *)header->samples_, header->numSamples_ );
               _recording = false ;

               if( !queue->shutdown_ )
               {
                  if( _cancel )
                     _cancel = false ;
                  else
                     item->isComplete_ = true ;
                  queueCallback( audioCallback, item );
               }

               closeReadFd();
            }
            else
               perror( "audioReadFd" );
         }
         else if( audioQueue_t :: mpegPlay_ == item->type_ )
         {
            writeFd = openWriteFd();
            if( 0 < writeFd )
            {
printf( "MPEG playback here at x:%u, y:%u, w:%u, h:%u\n", item->xPos_, item->yPos_, item->width_, item->height_ );
               _playing = true ;
               
               mpegDemux_t demuxer( item->data_, item->length_ );

printf( "demuxing mpeg data\n" );
               mpegDemux_t::bulkInfo_t const * const bi = demuxer.getFrames();

printf( "have %u streams\n", bi->count_ ); fflush( stdout );
               playbackStreams_t streams = { 0, 0 };
      
               for( unsigned char sIdx = 0 ; sIdx < bi->count_ ; sIdx++ )
               {
                  mpegDemux_t::streamAndFrames_t const &sAndF = *bi->streams_[sIdx];
                  mpegDemux_t::frameType_e const ft = sAndF.sInfo_.type ;
                  switch( ft )
                  {
                     case mpegDemux_t::videoFrame_e :
                     {
                        streams.videoFrames_ = bi->streams_[sIdx];
                        break;
                     }
      
                     case mpegDemux_t::audioFrame_e :
                     {
                        streams.audioFrames_ = bi->streams_[sIdx];
                        break;
                     }
                     case mpegDemux_t::endOfFile_e :
                        sIdx = bi->count_ ;
                        break ;
                  }
               } // for each stream in the file

               if( ( 0 != streams.audioFrames_ )
                   &&
                   ( 0 != streams.videoFrames_ ) )
               {
                  printf( "0x%llx milli-seconds total\n", bi->msTotal_ );
                  printf( "%llu.%03llu seconds total\n", bi->msTotal_ / 1000, bi->msTotal_ % 1000 );
                  long long msStart = tickMs();
                  audio_buf_info ai ;
                  if( 0 == ioctl( writeFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
                  {
                     fbDevice_t    &fb = getFB();
                     unsigned width = ( 0 != item->width_ ) ? item->width_ : fb.getWidth();
                     if( width > fb.getWidth() )
                        width = fb.getWidth();
                     unsigned height = ( 0 != item->height_ ) ? item->height_ : fb.getHeight();
                     if( height > fb.getHeight() )
                        height = fb.getHeight();
printf( "allocate frames: %u x %u\n", width, height );
                     videoFrames_t vFrames( *streams.videoFrames_, width, height );
                     if( vFrames.preload() )
                     {
                        unsigned picCount = 0 ;
                  
                        unsigned      frameIdx = 0 ;
                        long long     startPTS = 0 ;
                        mpegDemux_t::streamAndFrames_t const &audioFrames = *streams.audioFrames_ ;
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
                        videoParams_t    videoParams = { vFrames, item->xPos_, item->yPos_, item->width_, item->height_ };

                        while( !( _cancel || queue->shutdown_ ) 
                               && 
                               ( MP3DONE != state ) )
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
                                          if( 0 != ioctl( writeFd, SNDCTL_DSP_CHANNELS, &nChannels ) )
                                             fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%m\n" );
                                          if( 0 != ioctl( writeFd, SNDCTL_DSP_SPEED, &sampleRate ) )
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

                                    vFrames.setStart( tickMs() - startPTS - 384 );

                                    int const create = pthread_create( &videoThread, 0, videoThreadRtn, &videoParams );
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
                              int numWritten = write( writeFd, samples, bytesToWrite );
                              if( (unsigned)numWritten != bytesToWrite )
                              {
                                 fprintf( stderr, "!!! short write %u/%u\n", numWritten, bytesToWrite );
                                 return 0 ;
                              }
                  
                              numFilled = 0 ;
                              state &= ~FRAGFULL ;
                           } // write data!!!
                  
                           if( FRAGFULL == (state & (DEVICEAVAIL|FRAGFULL)) )
                           {
                              pollfd filedes ;
                              filedes.fd = writeFd ;
                              filedes.events = POLLOUT ;
                              if( 0 < poll( &filedes, 1, 0 ) )
                              {
                                 if( 0 == ioctl( writeFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
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
                                    break ;
                                 }
                              }
                           } // wait for device available
                           else if( 0 == (state & (FRAGFULL|MP3AVAIL)) )
                           {
                              usleep( 10000 );
            
                              if( 0 == ioctl( writeFd, SNDCTL_DSP_GETOSPACE, &ai) ) 
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
                                 break ;
                              }
                           } // no more data, wait for empty
                        } // while !done
                        
                        if( samples )
                           delete [] samples ;

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
                  long long msEnd = tickMs();
                  unsigned long long msElapsed = msEnd-msStart ;
                  printf( "%llu.%03llu seconds elapsed\n", msElapsed / 1000, msElapsed % 1000 );
               }
               
               closeWriteFd();

               mpegDemux_t::bulkInfo_t::clear( bi );

               if( !queue->shutdown_ )
               {
                  if( _cancel )
                     _cancel = false ;
                  else
                     item->isComplete_ = true ;
                  queueCallback( audioCallback, item );
               }
            }
            else
               perror( "audioWriteFd" );
         }
         else
            fprintf( stderr, "unknown audio queue request %d\n", item->type_ );
      }
   }
   
   delete [] outBuffer ;

   return 0 ;
}

audioQueue_t :: audioQueue_t( void )
   : queue_(),
     threadHandle_( (void *)-1 ),
     shutdown_( false )
{
   assert( 0 == audioQueue_ );
   audioQueue_ = this ;
   pthread_t tHandle ;

   numReadFrags_ = 16 ;
   readFragSize_ = 8192 ;
   maxReadBytes_ = numReadFrags_ * readFragSize_ ;

#ifdef FILTERAUDIO
   cnw = new CleanNoiseWork;
   Init_cnw(cnw,10);
#endif

   int readFd = openReadFd();
   if( 0 <= readFd )
   {
      ioctl( readFd, SNDCTL_DSP_SYNC, 0 );

      int const format = AFMT_S16_LE ;
      if( 0 != ioctl( readFd, SNDCTL_DSP_SETFMT, &format) )
         perror( "set record format" );

      int const channels = 1 ;
      if( 0 != ioctl( readFd, SNDCTL_DSP_CHANNELS, &channels ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );

      int speed = 44100 ;
      if( 0 != ioctl( readFd, SNDCTL_DSP_SPEED, &speed ) )
         fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );

      int recordLevel = 0 ;
      if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_MIC ), &recordLevel ) )
         perror( "get record level" );

      recordLevel = 100 ;
      if( 0 != ioctl( readFd, MIXER_WRITE( SOUND_MIXER_MIC ), &recordLevel ) )
         perror( "set record level" );

      if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_MIC ), &recordLevel ) )
         perror( "get record level" );

      int recSrc ;
      if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_RECSRC ), &recSrc ) )
         perror( "get record srcs" );
      
      int recMask ;
      if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_RECMASK ), &recMask ) )
         perror( "get record mask" );

      audio_buf_info bufInfo ;
      int biResult = ioctl( readFd, SNDCTL_DSP_GETISPACE, &bufInfo );
      if( 0 == biResult )
      {
         numReadFrags_ = bufInfo.fragstotal ;
         readFragSize_ = bufInfo.fragsize ;
         maxReadBytes_ = (numReadFrags_*readFragSize_);
      }
      else
      {
         perror( "getISpace" );
      }
      
      closeReadFd();
   }
   else
      perror( "/dev/dsp - read" );

   int const create = pthread_create( &tHandle, 0, audioThread, this );
   if( 0 == create )
   {
      struct sched_param tsParam ;
      tsParam.__sched_priority = 90 ;
      pthread_setschedparam( tHandle, SCHED_FIFO, &tsParam );
      threadHandle_ = (void *)tHandle ;
   }
   else
      fprintf( stderr, "Error %m creating curl-reader thread\n" );
}

audioQueue_t :: ~audioQueue_t( void )
{
#ifdef FILTERAUDIO
  Finish_cnw(cnw);
  delete cnw;
#endif  
}

bool audioQueue_t :: queuePlayback
   ( JSObject            *mp3Obj,
     unsigned char const *data,
     unsigned             length,
     jsval                onComplete,
     jsval                onCancel )
{
   item_t *item = new item_t ;
   item->type_       = mp3Play_ ;
   item->obj_        = mp3Obj ;
   item->data_       = data ;
   item->length_     = length ;
   item->onComplete_ = onComplete ;
   item->onCancel_   = onCancel ;
   item->isComplete_ = false ;
   JS_AddRoot( execContext_, &item->obj_ );
   JS_AddRoot( execContext_, &item->onComplete_ );
   JS_AddRoot( execContext_, &item->onCancel_ );
   return queue_.push( item );
}
   
bool audioQueue_t :: queuePlayback
   ( unsigned char const *data,
     unsigned             length,
     void                *cbParam,
     void (*callback)( void *param ) )
{
   item_t *item = new item_t ;
   item->type_       = mp3Raw_ ;
   item->obj_        = 0 ;
   item->data_       = data ;
   item->length_     = length ;
   item->onComplete_ = JSVAL_VOID ;
   item->onCancel_   = JSVAL_VOID ;
   item->isComplete_ = false ;
   item->callbackParam_ = cbParam ;
   item->callback_   = callback ;
   return queue_.push( item );
}
   
bool audioQueue_t :: queueMPEG
   ( JSObject            *mpegObj,
     unsigned char const *data,
     unsigned             length,
     jsval                onComplete,
     jsval                onCancel,
     unsigned             xPos,
     unsigned             yPos,
     unsigned             width,
     unsigned             height )
{
   item_t *item = new item_t ;
   item->type_       = mpegPlay_ ;
   item->obj_        = mpegObj ;
   item->data_       = data ;
   item->length_     = length ;
   item->onComplete_ = onComplete ;
   item->onCancel_   = onCancel ;
   item->isComplete_ = false ;
   item->xPos_       = xPos ;
   item->yPos_       = yPos ;
   item->width_      = width ;
   item->height_     = height ;
   JS_AddRoot( execContext_, &item->obj_ );
   JS_AddRoot( execContext_, &item->onComplete_ );
   JS_AddRoot( execContext_, &item->onCancel_ );
   return queue_.push( item );
}
   
bool audioQueue_t :: queuePlayback
   ( JSObject            *obj,
     waveHeader_t const  &data,
     jsval                onComplete,
     jsval                onCancel )
{
   item_t *item = new item_t ;
   item->type_       = wavPlay_ ;
   item->obj_        = obj ;
   item->data_       = (unsigned char const *)&data ;
   item->length_     = 0 ;
   item->onComplete_ = onComplete ;
   item->onCancel_   = onCancel ;
   item->isComplete_ = false ;
   JS_AddRoot( execContext_, &item->obj_ );
   JS_AddRoot( execContext_, &item->onComplete_ );
   JS_AddRoot( execContext_, &item->onCancel_ );
   return queue_.push( item );
}

bool audioQueue_t :: queueRecord
   ( JSObject            *obj,
     waveHeader_t        &data,
     jsval                onComplete,
     jsval                onCancel )
{
   item_t *item = new item_t ;
   item->type_       = wavRecord_ ;
   item->obj_        = obj ;
   item->data_       = (unsigned char const *)&data ;
   item->length_     = 0 ;
   item->onComplete_ = onComplete ;
   item->onCancel_   = onCancel ;
   item->isComplete_ = false ;
   JS_AddRoot( execContext_, &item->obj_ );
   JS_AddRoot( execContext_, &item->onComplete_ );
   JS_AddRoot( execContext_, &item->onCancel_ );
   return queue_.push( item );
}

bool audioQueue_t :: clear( unsigned &numCancelled )
{
   numCancelled = 0 ;

   {
      //
      // This should only be run in the context of 
      // a lock on Javascript execution (i.e. from 
      // Javascript code), so there's no need to lock
      // execMutex_.
      //
      item_t *item ;
      while( queue_.pull( item, 0 ) )
      {
         item->isComplete_ = false ;
         queueCallback( audioCallback, item );
         ++numCancelled ;
      }
   
      //
      // tell player to cancel at next frame
      //
      if( _playing || _recording )
      {
         _cancel = true ;
         ++numCancelled ;
      }
   } // limit scope of lock
      
//   ioctl( readFd_, SNDCTL_DSP_SYNC, 0 );
//   ioctl( writeFd_, SNDCTL_DSP_SYNC, 0 );

   return true ;
}

bool audioQueue_t :: stopRecording( void )
{
   //
   // tell player to cancel at next frame
   //
   if( _recording )
   {
      _cancel = true ;
      return true ;
   }
   else
      return false ;
}

bool audioQueue_t :: setRecordLevel( bool boost20db,
                                     int  newLevel )
{
   int readFd = openReadFd();
   if( 0 <= readFd )
   {
      int val = (int)boost20db ;
      if( 0 != ioctl( readFd, MIXER_WRITE( SOUND_MIXER_MIC ), &val ) )
         perror( "set boost" );
      if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_MIC ), &val ) )
         perror( "get boost" );
      else
         printf( "boost is %d\n", val );

printf( "set input gain to %d\n", newLevel );
      val = newLevel*7 ;
      val |= ( val << 8 );
      if( 0 != ioctl( readFd, MIXER_WRITE( SOUND_MIXER_IGAIN ), &val ) )
         perror( "set record level" );
      if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_IGAIN ), &val ) )
         perror( "get record level" );
      else
         printf( "input gain is %d\n", val );

      closeReadFd();
   }
   
   return true ;
}

void audioQueue_t :: shutdown( void )
{
   audioQueue_t *const old = audioQueue_ ;
   audioQueue_ = 0 ;

   if( old )
   {
      {
         mutexLock_t lock( openMutex_ );
         if( 0 <= readFd_ )
            close( readFd_ );
         if( 0 <= writeFd_ )
            close( writeFd_ );
      }
      old->shutdown_ = true ;
      unsigned cancelled ;
      old->clear( cancelled );
      old->queue_.abort();

      void *exitStat ;
      
      if( 0 <= old->threadHandle_ )
      {
         pthread_join( (pthread_t)old->threadHandle_, &exitStat );
         old->threadHandle_ = (void *)-1 ;
      }
      
      delete old ;
   }
}

bool audioQueue_t :: pull( item_t *&item )
{
   return queue_.pull( item );
}
   
audioQueue_t &getAudioQueue( void )
{
   if( 0 == audioQueue_ )
   {
      audioQueue_ = new audioQueue_t();
   }
   return *audioQueue_ ;
}

