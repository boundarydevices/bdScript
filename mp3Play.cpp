/*
 * Program mp3Play.cpp
 *
 * This program plays an MP3-encoded audio file
 * on /dev/dsp0.
 *
 * Change History : 
 *
 * $Log: mp3Play.cpp,v $
 * Revision 1.1  2002-10-25 02:55:01  ericn
 * -initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

#include <stdio.h>
#include "mad.h"
#include "curlCache.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>

static int dspFd_ = -1 ;
static int sampleRate_ = 0 ;
static int requestedChannels_ = 0 ;
static int numChannels_ = 0 ;

/*
 * This is a private message structure. A generic pointer to this structure
 * is passed to each of the callback functions. Put here any data you need
 * to access from within the callbacks.
 */

struct buffer {
  unsigned char const *start;
  unsigned long length;
};

/*
 * This is the input callback. The purpose of this callback is to (re)fill
 * the stream buffer which is to be decoded. In this example, an entire file
 * has been mapped into memory, so we just call mad_stream_buffer() with the
 * address and length of the mapping. When this callback is called a second
 * time, we are finished decoding.
 */

static
enum mad_flow input(void *data,
		    struct mad_stream *stream)
{
  struct buffer *buffer = (struct buffer *)data;

  if (!buffer->length)
    return MAD_FLOW_STOP;

  mad_stream_buffer(stream, buffer->start, buffer->length);

  buffer->length = 0;

  return MAD_FLOW_CONTINUE;
}

/*
 * The following utility routine performs simple rounding, clipping, and
 * scaling of MAD's high-resolution samples down to 16 bits. It does not
 * perform any dithering or noise shaping, which would be recommended to
 * obtain any exceptional audio quality. It is therefore not recommended to
 * use this routine if high-quality output is desired.
 */

static inline
signed short scale(mad_fixed_t sample)
{
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return (short)( sample >> (MAD_F_FRACBITS + 1 - 16) );
}

/*
 * This is the output callback function. It is called after each frame of
 * MPEG audio data has been completely decoded. The purpose of this callback
 * is to output (or play) the decoded PCM audio.
 */
static unsigned short *outBuffer  = 0 ;
static unsigned short numSamples_ = 0 ;
static unsigned short numBuffers_ = 0 ;
static unsigned short fragSize    = 0 ;
static unsigned short spaceLeft   = 0 ;

#define OUTBUFSIZE 8192

static
enum mad_flow output(void *data,
		     struct mad_header const *header,
		     struct mad_pcm *pcm)
{
   unsigned int nchannels, nsamples;
   mad_fixed_t const *left_ch, *right_ch;
 
   if( ( pcm->channels != requestedChannels_ )
       ||
       ( pcm->samplerate != sampleRate_ ) )
   {
      int dummy = 0 ;
      if( 0 != ioctl( dspFd_, SNDCTL_DSP_SYNC, &dummy ) )
         fprintf( stderr, "Error (%m) syncing audio device\n" );

      if( pcm->channels != requestedChannels_ )
      {
         numChannels_ = requestedChannels_ = pcm->channels ;
         printf( "changing #channels to %d\n", requestedChannels_ );
         if( 0 != ioctl( dspFd_, SNDCTL_DSP_CHANNELS, &numChannels_ ) )
         {
            fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS):%d:%m\n" );
            return MAD_FLOW_BREAK ;
         }
         if( requestedChannels_ != numChannels_ )
         {
            printf( "%d channels not allowed, using %d\n", requestedChannels_, numChannels_ );
            return MAD_FLOW_BREAK ;
         }
      }
   
      if( pcm->samplerate != sampleRate_ )
      {
         sampleRate_ = pcm->samplerate ;
         if( 0 == ioctl( dspFd_, SNDCTL_DSP_SPEED, &sampleRate_ ) )
            printf( "new sampling rate %d\n", sampleRate_ );
         else
         {
            fprintf( stderr, "Error setting sampling rate to %d\n", sampleRate_ );
            return MAD_FLOW_BREAK ;
         }
      }
   } // sound format changed.. 

   nchannels = pcm->channels;
   nsamples  = pcm->length;
   left_ch   = pcm->samples[0];
   right_ch  = pcm->samples[1];
 
   unsigned short *nextOut = outBuffer + fragSize-spaceLeft;
   if( 1 == numChannels_ )
   {
      while( nsamples-- )
      {
         *nextOut++ = scale(*left_ch++);
         if( 0 == --spaceLeft )
         {
            write( dspFd_, outBuffer, fragSize*sizeof( outBuffer[0] ) );
            spaceLeft = fragSize ;
            nextOut = outBuffer ;
         }
      }
   } // mono output
   else if( 2 == numChannels_ )
   {
      while( nsamples-- )
      {
         *nextOut++ = scale( *left_ch++ );
         *nextOut++ = scale( *right_ch++ );
         spaceLeft -= 2 ;
         if( 0 == spaceLeft )
         {
            write( dspFd_, outBuffer, fragSize*sizeof( outBuffer[0] ) );
            spaceLeft = fragSize ;
            nextOut = outBuffer ;
         }
      }
   } // stereo output

   return MAD_FLOW_CONTINUE;
}

/*
 * This is the error callback function. It is called whenever a decoding
 * error occurs. The error is indicated by stream->error; the list of
 * possible MAD_ERROR_* errors can be found in the mad.h (or
 * libmad/stream.h) header file.
 */

static
enum mad_flow error(void *data,
		    struct mad_stream *stream,
		    struct mad_frame *frame)
{
  struct buffer *buffer = (struct buffer *)data;

  fprintf(stderr, "decoding error 0x%04x (%s) at byte offset %u\n",
	  stream->error, mad_stream_errorstr(stream),
	  stream->this_frame - buffer->start);

  return MAD_FLOW_CONTINUE;
}

/*
 * This is the function called by main() above to perform all the
 * decoding. It instantiates a decoder object and configures it with the
 * input, output, and error callback functions above. A single call to
 * mad_decoder_run() continues until a callback function returns
 * MAD_FLOW_STOP (to stop decoding) or MAD_FLOW_BREAK (to stop decoding and
 * signal an error).
 */

static
int decode(unsigned char const *start, unsigned long length)
{
   struct buffer buffer;
   struct mad_decoder decoder;
   int result;
   
   /* initialize our private message structure */
   
   buffer.start  = start;
   buffer.length = length;
   
   /* configure input, output, and error functions */
   
   mad_decoder_init(&decoder, &buffer,
         input, 0 /* header */, 0 /* filter */, output,
         error, 0 /* message */);

   /* start decoding */
   
   result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
   
  /* release the decoder */

   mad_decoder_finish(&decoder);
   
   return result;
}

int main( int argc, char *argv[] )
{
   if( ( 2 == argc ) || ( 3 == argc ) )
   {
      dspFd_ = open( ( 3 == argc ) ? argv[2] : "/dev/dsp", O_WRONLY );
      if( 0 <= dspFd_ )
      {
         if( 0 == ioctl(dspFd_, SNDCTL_DSP_SYNC, 0 ) ) 
         {
            int const format = AFMT_S16_LE ;
            if( 0 == ioctl( dspFd_, SNDCTL_DSP_SETFMT, &format) ) 
            {
               int const channels = 2 ;
               if( 0 != ioctl( dspFd_, SNDCTL_DSP_CHANNELS, &channels ) )
                  fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );
   
               int speed = 44100 ;
               while( 0 < speed )
               {
                  if( 0 == ioctl( dspFd_, SNDCTL_DSP_SPEED, &speed ) )
                  {
   //                  printf( "initialized at %u samples/sec\n", speed );
                     break;
                  }
                  else
                     fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
                  speed /= 2 ;
               }
   
               if( 0 != speed )
               {
                  curlCache_t &cache = getCurlCache();
                  char *cURL = argv[1];
                  curlFile_t f( cache.get( cURL, true ) );
                  if( f.isOpen() )
                  {
                     audio_buf_info info ;
                     if( 0 == ioctl( dspFd_, SNDCTL_DSP_GETOSPACE, &info ) )
                     {   
/*
                        printf( "frags %d\n"
                                "fragsTotal %d\n"
                                "fragSize %d\n"
                                "bytes %d\n", info.fragments, info.fragstotal, info.fragsize, info.bytes );
*/                                
                        fragSize = info.fragsize ;
                     }
                     else
                     {
                        fragSize = OUTBUFSIZE ;
                        fprintf( stderr, "Error %m getting outBuffer stats\n" );
                     }

//                     printf( "playing %lu bytes of audio\n", f.getSize() );
                     outBuffer = new unsigned short [ fragSize ];
                     spaceLeft = fragSize ;
// printf( "spaceLeft %u\n", spaceLeft );
                     nice( -10 );
                     decode( (unsigned char *)f.getData(), f.getSize() );
                     
                     if( fragSize != spaceLeft )
                     {
                        unsigned const used = (fragSize-spaceLeft);
                        write( dspFd_, outBuffer, used * sizeof( outBuffer[0] ) ); // flush output
                     }
                  }
                  else
                     fprintf( stderr, "Error retrieving %s\n", cURL );
               }
               else
               {
                  fprintf( stderr, "No known speed supported\n" );
                  close( dspFd_ );
                  dspFd_ = -1 ;
               }
            }
            else
               fprintf( stderr, ":ioctl(SNDCTL_DSP_SETFMT):%m\n" );
         }
         else
            fprintf( stderr, ":ioctl(SNDCTL_DSP_SYNC)\n" );
   
      }
      else
         fprintf( stderr, "Error %m opening audio device\n" );
   }
   else
      fprintf( stderr, "usage %s mp3URL\n", argv[0] );
   
   return 0 ;
}
