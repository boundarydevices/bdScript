#include <stdio.h>
#include <stdlib.h>
//#define ALSA
#ifdef ALSA
#include <alsa/asoundlib.h>

#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#endif
#include "fft.h"
#include "fftClean.h"

#define		PCM_WAVE_FORMAT   	1

#define FMT_wFormatTag PCM_WAVE_FORMAT
#define FMT_wChannels 1
#define FMT_dwSamplesPerSec 44100
#define FMT_wBitsPerSample 16

#define FMT_wBlockAlign (((FMT_wBitsPerSample+7)/8)*FMT_wChannels)
#define FMT_dwAvgBytesPerSec (FMT_dwSamplesPerSec*FMT_wBlockAlign)


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

#ifdef ALSA
static void OpenAlsa()
{
	int i;
	int err;
	short buf[128];
	snd_pcm_t *capture_handle;
	snd_pcm_hw_params_t *hw_params;
	int card=0;
	int dev=0;
	if ((err = snd_pcm_open (&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0)) < 0) {
		fprintf (stderr, "cannot open audio device %s (%s)\n",
			argv[1],snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_malloc (&hw_params)) < 0) {
		fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_any (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_access (capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		fprintf (stderr, "cannot set access type (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_format (capture_handle, hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
		fprintf (stderr, "cannot set sample format (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_rate_near (capture_handle, hw_params, FMT_dwSamplesPerSec, 0)) < 0) {
		fprintf (stderr, "cannot set sample rate (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params_set_channels (capture_handle, hw_params, 2)) < 0) {
		fprintf (stderr, "cannot set channel count (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	if ((err = snd_pcm_hw_params (capture_handle, hw_params)) < 0) {
		fprintf (stderr, "cannot set parameters (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	snd_pcm_hw_params_free (hw_params);

	if ((err = snd_pcm_prepare (capture_handle)) < 0) {
		fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
			snd_strerror (err));
		exit (1);
	}

	for (i = 0; i < 10; ++i) {
		if ((err = snd_pcm_readi (capture_handle, buf, 128)) != 128) {
			fprintf (stderr, "read from audio interface failed (%s)\n",
				snd_strerror (err));
			exit (1);
		}
	}

	snd_pcm_close (capture_handle);
	exit (0);
}
#endif

static int openReadFd(int* pBlkSize,int channels)
{
	int readFd = open( "/dev/dsp", O_RDONLY );
	int const format = AFMT_S16_LE;
	int speed = FMT_dwSamplesPerSec;
	int recordLevel = 0;
	int recSrc;
	int recMask;
	audio_buf_info bufInfo ;
	int biResult;

	if (readFd<0) return readFd;
	ioctl( readFd, SNDCTL_DSP_SYNC, 0 );

	if( 0 != ioctl( readFd, SNDCTL_DSP_SETFMT, &format) )
		perror( "set record format" );

	if( 0 != ioctl( readFd, SNDCTL_DSP_CHANNELS, &channels ) )
		fprintf( stderr, ":ioctl(SNDCTL_DSP_CHANNELS)\n" );
	printf( "channels:%i\n",channels);

	if( 0 != ioctl( readFd, SNDCTL_DSP_SPEED, &speed ) )
		fprintf( stderr, ":ioctl(SNDCTL_DSP_SPEED):%u:%m\n", speed );
	printf( "speed:%i\n",speed);

	if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_MIC ), &recordLevel ) )
		perror( "get record level" );

	recordLevel = 100 ;
	if( 0 != ioctl( readFd, MIXER_WRITE( SOUND_MIXER_MIC ), &recordLevel ) )
		perror( "set record level" );

//	if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_MIC ), &recordLevel ) )
//		perror( "get record level" );

	if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_RECSRC ), &recSrc ) )
		perror( "get record srcs" );

	if( 0 != ioctl( readFd, MIXER_READ( SOUND_MIXER_RECMASK ), &recMask ) )
		perror( "get record mask" );

	if (0 != ioctl( readFd,SNDCTL_DSP_GETBLKSIZE,pBlkSize) ) {
                perror(" Optaining DSP's block size");
        }

	biResult = ioctl( readFd, SNDCTL_DSP_GETISPACE, &bufInfo );
	if( 0 == biResult ) {
//		unsigned	numReadFrags;
//		unsigned	readFragSize;
//		unsigned	maxReadBytes;
//		numReadFrags = bufInfo.fragstotal ;
//		readFragSize = bufInfo.fragsize ;
//		maxReadBytes = (numReadFrags_*readFragSize_);
		printf("frags cnt:%i, frag size:%i\r\n",bufInfo.fragstotal,bufInfo.fragsize);
	} else {
		perror( "getISpace" );
	}
	return readFd;
}

static void inline CalcPowerSqroot(npd* power,npd_p1* powerSum,cmplx* vect,const int logN)
{
	const int n_d2=(1<<(logN-1));
	const int n_d4=(1<<(logN-2));
	const int n3_d4=(3<<(logN-2));
	int i,j,k;
	memset(powerSum,0,sizeof(*powerSum));
	for (i=0,j=0; i<NOISE_SIZE; i++,j=IncReversed(j,n_d2)) {
#ifndef SKIP_REVERSAL_SORT
		j = i;
#endif
		np temp1;
		Magnitude(&temp1,&vect[j].r,&vect[j].i);
		memcpy(power,&temp1,sizeof(temp1));
		memset(&power[NP_CNT],0,sizeof(temp1));
		AddNPD_P1(powerSum,power);
		power++;
	}
}

int AverageFFT(short* src,int startPos,int srcBufMask,CleanNoiseWork* cnw)
{
	int i;
	const int logN=cnw->logN;
	const int n=(1<<logN);
	const int n_d2=(1<<(logN-1));
	const int n_d4=(1<<(logN-2));
	const int n3_d4=(3<<(logN-2));
	cmplx vect[n];
	short outputBuffer[n];
	short* pSamples;
	npd power[NOISE_SIZE];
	npd_p1 powerSum;

	fft_forward(vect,src,logN);
//	PrintTable(vect,logN,AFTER_FFT_SHIFT,1);
//	printf("e\r\n");
//	printNpd_p1("a noiseSum: 0x",&cnw->noiseSum);
	CalcPowerSqroot(&power[0],&powerSum,vect,logN);
//	printNpd_p1("b noiseSum: 0x",&cnw->noiseSum);

#ifdef NOISE_ACCUM
	for (i=0; i<NOISE_SIZE; i++) AddNPD_P1(&cnw->noiseAccum[i],&power[i]);
	cnw->noiseCnt++;
	if (cnw->noiseCnt >= 4096) return 0;
#endif
	return n;
}

int WriteWavFile(short* pSamples,int samples);
void test2(void)
{
	const int logN=10;	//1024 points
	const int n=(1<<logN);
	const int n_d2=(1<<(logN-1));
	const int n_d4=(1<<(logN-2));
	int i;
	CleanNoiseWork cnw;

	unsigned char* inSamples;
	short* outSamples;

	int blkSize;
	int readPos=0;
	int writePos=0;
	const unsigned int fmask = (n<<1)-1;
	int readFd = openReadFd(&blkSize,1);	//set to 1 channel
	int outputSpace;
	int outSampleCnt = 0;
	int outSampleCntMax;
	if (readFd<0) return;

	Init_cnw(&cnw,logN);

	printf("blkSize:%i\r\n",blkSize);
	blkSize = (blkSize+fmask) & ~fmask;	//round to multiple of fft
	printf("blkSize:%i\r\n",blkSize);
	inSamples = (unsigned char *)malloc(blkSize);
	if (inSamples==NULL) return;

	outputSpace = FMT_dwAvgBytesPerSec*10;	//get a 10 second buffer
	outSamples = (short *)malloc(outputSpace);
	outSampleCntMax = (outputSpace) / (sizeof(*outSamples));
	memset(outSamples,0,n_d4*sizeof(*outSamples));

	while (outSampleCnt < outSampleCntMax) {
		int numRead = 0;
//		printf("readPos:%i outSampleCnt:%i, max:%i\r\n",readPos,outSampleCnt,outSampleCntMax);
		while ((((writePos-readPos)& (blkSize-1))+numRead)<n) {
			int max;
			short* pSamples;

			writePos=(writePos+numRead)& (blkSize-1);
			numRead=0;

			if (readPos > writePos) max = (readPos-writePos);
			else max = blkSize-writePos;

//			printf("max:%i\r\n",max);
#if 1
			numRead = read(readFd, &inSamples[writePos], max );
#else
			for (i=0,pSamples=(short*)(&inSamples[writePos]); i<(max>>1); i++) {*pSamples++ = i;}
//			for (i=0,rSamples=(short*)(&inSamples[writePos]); i<(max>>1); i++) {printf("__%i %i\r\n",i,*rSamples++);}
			numRead = max;
#endif
//			printf("numRead:%i\r\n",numRead);
			if (numRead<=0 ) return;
		}
		writePos=(writePos+numRead)& (blkSize-1);

#if defined(NOISE_ACCUM) || defined (AVERAGE_FFT)
		outSampleCnt=0;
#endif

#ifdef AVERAGE_FFT
		i = AverageFFT((short*)inSamples,(readPos>>1),(blkSize>>1)-1,&cnw);
#else
		i = CleanNoise(outSamples+outSampleCnt,outSampleCntMax-outSampleCnt,(short*)inSamples,(readPos>>1),(blkSize>>1)-1,&cnw);	//shifts needed to convert from char oriented to short oriented
#endif
		if (i==0) break;
		{
			int max = outSampleCntMax - outSampleCnt;
			if (max>n_d4) max = n_d4;
			outSampleCnt += max;
		}
		readPos =(readPos+SAMPLE_ADVANCE)& (blkSize-1);
//		printf("readPos:%i,outSampleCnt*2:%i\r\n",readPos,outSampleCnt<<1);
	}
	printf("\r\n");
	close(readFd);
#if !defined(NOISE_ACCUM) && !defined(AVERAGE_FFT)
	WriteWavFile(outSamples,outSampleCnt);
#endif
	free(inSamples);
	free(outSamples);
	Finish_cnw(&cnw);
}
//multiplying frequency domain signals
//Y[f].r = (X[f].r * H[f].r) - (X[f].i * H[f].i)
//Y[f].i = (X[f].i * H[f].r) + (X[f].r * H[f].i)
//Piano
//Lowest A is  27.5 Hz
//Next A's  55,110,220
//Middle C is 261.625565 Hz
//Next A's 440,880,1760,3520
//Highest C is 4186
/////////////////////////////////////////////////////////////////////////////////
typedef  struct
{	u_long     dwSize;
	u_short    wFormatTag;
	u_short    wChannels;
	u_long     dwSamplesPerSec;
	u_long     dwAvgBytesPerSec;
	u_short    wBlockAlign;
	u_short    wBitsPerSample;
} WAVEFORMAT;

typedef  struct
{	char    	RiffID[4];
	u_long    	RiffSize;
	char    	WaveID[4];
	char    	FmtID[4];
	WAVEFORMAT	fmt;
	char		DataID[4];
	u_long		nDataBytes;
} WAVE_HEADER ;

static  WAVE_HEADER  waveHeader =
{	{ 'R', 'I', 'F', 'F' },
		sizeof(WAVE_HEADER)-8,	//RiffSize,  plus samples*blockAlign
	{ 'W', 'A', 'V', 'E' },
	{ 'f', 'm', 't', ' ' },
	{sizeof(WAVEFORMAT)-4,
	 FMT_wFormatTag, FMT_wChannels, FMT_dwSamplesPerSec, FMT_dwAvgBytesPerSec,
	 FMT_wBlockAlign, FMT_wBitsPerSample
	},
	{ 'd', 'a', 't', 'a' },
		0			//nDataBytes,   plus samples*blockAlign
};


static void normalize( short int *samples,
                       unsigned   numSamples )
{
	unsigned long const maxNormalizeRatio = 0x243f6f ;
	signed short *next = samples ;
	signed short min = *next;
	signed short max = *next++;
	unsigned i;
	signed long ratio;
	for( i = 1 ; i < numSamples ; i++ )
	{
		signed short s = *next++ ;
		if( max < s) max = s ;
		else if( min > s ) min = s ;
	}
	min = 0-min ;
	max = max > min ? max : min ;

	ratio = ( 0x70000000UL / max );
	if (ratio > 0x10000) {	//don't reduce volume
		if( ratio > maxNormalizeRatio ) ratio = maxNormalizeRatio ;

		next = samples ;
		for( i = 0 ; i < numSamples ; i++ ) {
			signed long x16 = ratio * ((signed long)*next) ;
			x16 += 1<<15;	//round
			*next++ = (signed short)( x16 >> 16 );
		}
	}
}

int WriteWavFile(short* pSamples,int samples)
{
	WAVE_HEADER wh;
	int cnt,cnt2;
	int wavFile = open( "mic.wav", O_WRONLY|O_CREAT|O_TRUNC );
	if (wavFile<0) return wavFile;

	normalize(pSamples,samples);

	wh = waveHeader;
	cnt = samples * wh.fmt.wBlockAlign;
	wh.RiffSize += cnt;
	wh.nDataBytes += cnt;

	if (write(wavFile, &wh, sizeof (wh)) != sizeof (wh)) {
		printf("wave header write error\r\n");
//		err("%s",sys_errlist[errno]);	/* wwg: report the error */
		return  -1;
	}
	cnt2 = write(wavFile,pSamples,cnt);
	if (cnt2!=cnt) {
		printf("wave data write error, requested %i, wrote %i\r\n",cnt,cnt2);
//		err("%s",sys_errlist[errno]);	/* wwg: report the error */
		return  -1;
	}
	return 0;
}

static inline void testSqRoot()
{
	npd temp;
	np a;
	int i;
	for (i=0; i<=65535; i++) {
		memset(&a,0,sizeof(a));
		a.n[0] = i;
		MultNp(&temp,&a,&a);
		memcpy(&a,&temp,sizeof(a));
		MultNp(&temp,&a,&a);
		Sqroot(&a,&temp);
		memcpy(&temp,&a,sizeof(a));
		memset(&temp.n[NP_CNT],0,sizeof(a));
		Sqroot(&a,&temp);
		printf("num: %u  sqroot: %u\r\n",temp.n[0],a.n[0]);
		if (a.n[0] != i) {printf("error\r\n"); break;}
	}
	IncNpd(&temp);
	Sqroot(&a,&temp);
	printf("num: %u  sqroot: %u\r\n",temp.n[0],a.n[0]);
}
int main (int argc, char *argv[])
{
#if 1
	test2();
#else
	testSqRoot();
#endif
}
