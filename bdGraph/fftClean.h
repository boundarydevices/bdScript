//#define AVERAGE_FFT
//#define NOISE_ACCUM
#define REP_FFT_CNT 1024
#ifdef AVERAGE_FFT
#ifndef NOISE_ACCUM
#define NOISE_ACCUM
#endif
#endif
#define SILENCE_BEFORE_UPDATE 10		//# of silent frames to wait before updating noise profile
#define MAX_ADD_SIZE n3_d4

#ifdef AVERAGE_FFT
#define SAMPLE_ADVANCE (n<<1)
#define NOISE_SIZE (n_d2+1)
#else
#define SAMPLE_ADVANCE (n_d2)	//advance n_d4 samples (n_d4<<1 == n_d2)
#define NOISE_SIZE (n_d4-4)		//entries not zeroed are 4 .. (n_d4-1)
#endif

typedef struct CLEAN_NOISE_WORK
{
	int logN;
	int silenceCnt;
	npd_p1 noiseSum;
#ifdef NOISE_ACCUM
	int noiseCnt;
	npd_p1* noiseAccum;	//NOISE_SIZE # of entries
#endif
	npd* noise;		//NOISE_SIZE # of entries
} CleanNoiseWork;

void Init_cnw(CleanNoiseWork* cnw,int logN);
//src buffer is a circular buffer
int CleanNoise(short* dest,int bufSize,short* src,int startPos,int srcBufMask,CleanNoiseWork* cnw);
void Finish_cnw(CleanNoiseWork* cnw);

static inline void Magnitude2(npd* dest, const np* a, const np* b)
{
	int i,j;
	np a_,b_;
	ZeroNpd(dest);
	if ( ((int)(a->n[NP_CNT-1]))<0) {Negate2(&a_,a); a= &a_;}
	if ( ((int)(b->n[NP_CNT-1]))<0) {Negate2(&b_,b); b= &b_;}

	for (i=0; i<NPD_CNT-1; i++) {
		const int max = (i<=(NP_CNT-1))? i : NP_CNT-1;
		for (j=i-max; j<=max; j++) {
			AddM(&dest->n[i],a->n[j],a->n[i-j]);
			AddM(&dest->n[i],b->n[j],b->n[i-j]);
		}
	}
}
static inline void Magnitude(np* dest, np* a, np* b)
{
	npd temp;
	Magnitude2(&temp,a,b);
	Sqroot(dest,&temp);
#if 0
	PrintNpd(&temp,0);
	printf(" = ");
	PrintNpData(a,0);
	printf("**2 + ");
	PrintNpData(b,0);
	printf("**2\n");
#endif
}
