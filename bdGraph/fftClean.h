//#define AVERAGE_FFT
//#define NOISE_ACCUM

#ifdef AVERAGE_FFT
#ifndef NOISE_ACCUM
#define NOISE_ACCUM
#endif

#define REP_FFT_CNT 1024
#else
#define REP_FFT_CNT 4096
#endif

#define SILENCE_BEFORE_UPDATE 2		//# of silent frames to wait before updating noise profile
#define SILENCE_AFTER_UPDATE 2
#define MAX_ADD_SIZE n3_d4

#ifdef AVERAGE_FFT
#define SAMPLE_ADVANCE (n<<1)
#define PROFILE_SIZE (n_d2+1)
#else
#define SAMPLE_ADVANCE (n_d2)	//advance n_d4 samples (n_d4<<1 == n_d2)
#define PROFILE_SIZE (n_d4-4)		//entries not zeroed are 4 .. (n_d4-1)
#endif

#define CONSECUTIVE_CNT 6

typedef struct CLEAN_NOISE_WORK
{
	int logN;
	int silenceCnt;
	npd_p1 noiseSum;
#ifdef NOISE_ACCUM
	int noiseCnt;
	npd_p1* noiseAccum;	//PROFILE_SIZE # of entries
#endif
	npd* noise;		//PROFILE_SIZE # of entries
	cmplx* v;		//PROFILE_SIZE* CONSECUTIVE_CNT # of entries
	npd_p1 powerSum[CONSECUTIVE_CNT];
	int vIndex;
	int state;
} CleanNoiseWork;

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif

EXTERN_C void Init_cnw(CleanNoiseWork* cnw,int logN);
//src buffer is a circular buffer
EXTERN_C int CleanNoise(short* dest,int bufSize,short* src,int startPos,int srcBufMask,CleanNoiseWork* cnw);
EXTERN_C void Finish_cnw(CleanNoiseWork* cnw);

static inline void Magnitude2(npd* dest, const cmplx* num)
{
	const np* a = &num->r;
	const np* b = &num->i;
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
static inline void Magnitude(np* dest, const cmplx* num)
{
	npd temp;
	Magnitude2(&temp,num);
	Sqroot(dest,&temp);
#if 0
	PrintNpd(&temp,0);
	printf(" = ");
	PrintNpData(&num->r,0);
	printf("**2 + ");
	PrintNpData(&num->i,0);
	printf("**2\n");
#endif
}
static inline int CmpPower(const cmplx* num,const npd* noise)
{
	npd power;
	Magnitude2(&power,num);
	return CmpNpd(&power,noise);
}
