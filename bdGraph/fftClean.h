//#define NOISE_ACCUM
#define SILENCE_BEFORE_UPDATE 10		//# of silent frames to wait before updating noise profile
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

