#include <stdio.h>
#include "fft.h"
#include "fftClean.h"

//#define DBGP(a) printf(a "\r\n");
#define DBGP(a)

static void printNpd_p1(char* s,npd_p1* p)
{
	int i;
	printf("%s",s);
	for (i=NPD_P1_CNT-1; i>=0; i--) printf("%08x ",p->n[i]);
	printf("\r\n");
}



//        . . . . . . . . .                 //"." high frequency to zero,
//0 1 2 3 4 5 6 7 8 9 a b c d e f
//              |___|
//            |_______|
//          |___________|
//        |_______________|
//      |___________________|
//    |_______________________|
//  |___________________________|

//Bit reversed symmetry
//  . . .   . .     . .     . .            //"." high frequency to zero, "*" low frequency to zero entries (0-3)
//0 1 2 3 4 5 6 7 8 9 a b c d e f
//0 8 4 c 2 a 6 e 1 9 5 d 3 b 7 f
//    |_|   |_|         |_|
//        |_____|     |_____|
//                  |_________|
//                |_____________|

//00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f 80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f 90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff
//                                                                                                                                                                                                                                                                                                                                                                                              |_____|
//                                                                                                                                                                                                                                                                                                                                                                                           |___________|
//                                                                                                                                                                                                                                                                                                                                                                                        |_________________|
//...
//          |_____________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________|
//       |___________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________|
//    |_________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________|

//
//             ^           ^       ^               ^        ^  ^        ^
// *  .  .  .     .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .     *  .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .     *  .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .     *  .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .        .  .
//00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f 10 11 12 13 14 15 16 17 18 19 1a 1b 1c 1d 1e 1f 20 21 22 23 24 25 26 27 28 29 2a 2b 2c 2d 2e 2f 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f 40 41 42 43 44 45 46 47 48 49 4a 4b 4c 4d 4e 4f 50 51 52 53 54 55 56 57 58 59 5a 5b 5c 5d 5e 5f 60 61 62 63 64 65 66 67 68 69 6a 6b 6c 6d 6e 6f 70 71 72 73 74 75 76 77 78 79 7a 7b 7c 7d 7e 7f 80 81 82 83 84 85 86 87 88 89 8a 8b 8c 8d 8e 8f 90 91 92 93 94 95 96 97 98 99 9a 9b 9c 9d 9e 9f a0 a1 a2 a3 a4 a5 a6 a7 a8 a9 aa ab ac ad ae af b0 b1 b2 b3 b4 b5 b6 b7 b8 b9 ba bb bc bd be bf c0 c1 c2 c3 c4 c5 c6 c7 c8 c9 ca cb cc cd ce cf d0 d1 d2 d3 d4 d5 d6 d7 d8 d9 da db dc dd de df e0 e1 e2 e3 e4 e5 e6 e7 e8 e9 ea eb ec ed ee ef f0 f1 f2 f3 f4 f5 f6 f7 f8 f9 fa fb fc fd fe ff
//00 80 40 c0 20 a0 60 e0 10 90 50 d0 30 b0 70 f0 08 88 48 c8 28 a8 68 e8 18 98 58 d8 38 b8 78 f8 04 84 44 c4 24 a4 64 e4 14 94 54 d4 34 b4 74 f4 0c 8c 4c cc 2c ac 6c ec 1c 9c 5c dc 3c bc 7c fc 02 82 42 c2 22 a2 62 e2 12 92 52 d2 32 b2 72 f2 0a 8a 4a ca 2a aa 6a ea 1a 9a 5a da 3a ba 7a fa 06 86 46 c6 26 a6 66 e6 16 96 56 d6 36 b6 76 f6 0e 8e 4e ce 2e ae 6e ee 1e 9e 5e de 3e be 7e fe 01 81 41 c1 21 a1 61 e1 11 91 51 d1 31 b1 71 f1 09 89 49 c9 29 a9 69 e9 19 99 59 d9 39 b9 79 f9 05 85 45 c5 25 a5 65 e5 15 95 55 d5 35 b5 75 f5 0d 8d 4d cd 2d ad 6d ed 1d 9d 5d dd 3d bd 7d fd 03 83 43 c3 23 a3 63 e3 13 93 53 d3 33 b3 73 f3 0b 8b 4b cb 2b ab 6b eb 1b 9b 5b db 3b bb 7b fb 07 87 47 c7 27 a7 67 e7 17 97 57 d7 37 b7 77 f7 0f 8f 4f cf 2f af 6f ef 1f 9f 5f df 3f bf 7f ff
//       |__|     |__|              |__|                                |__|                                                                    |__|                                                                                                                                            |__|                                                                                                                                                                                                                                                                                            |__|
//             |________|        |________|                          |________|                                                              |________|
//                            |______________|                    |______________|                                                        |______________|
//                         |____________________|              |____________________|                                                  |____________________|
//                                                          |__________________________|                                            |__________________________|
//                                                       |________________________________|                                      |________________________________|
//                                                    |______________________________________|                                |______________________________________|
//                                                 |____________________________________________|                          |____________________________________________|
//                                                                                                 |____________________________________________________________________________________________|  |____________________________________________________________________________________________________________________________________________________________________________________________|  |____________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________________|
static void inline SaveProfile(CleanNoiseWork* cnw,cmplx* vect)
{
	int i,k;
	int logN = cnw->logN;
	const int n_d2=(1<<(logN-1));
	const int n_d4=(1<<(logN-2));
	const int n3_d4=(3<<(logN-2));
	int index = cnw->vIndex;
	cmplx* v = cnw->v + (index * PROFILE_SIZE);
	int cnt=0;
	npd_p1* powerSum = &cnw->powerSum[index];
#ifdef NOISE_ACCUM
	npd_p1* pn = &cnw->noiseAccum[0];
#endif
	memset(powerSum,0,sizeof(*powerSum));
	cnw->vIndex++;
	if (cnw->vIndex >= CONSECUTIVE_CNT) cnw->vIndex = 0;
#ifdef SKIP_REVERSAL_SORT
	for (k=4; k<=n_d2; k <<= 1) {
		int stop = k + (k>>1);
		for (i=k; i<stop; i = (i&1)? (i+1) : (i+3)) {
			if ((i & ~n3_d4) && ((i+1)& ~n3_d4)) {	//if not a zeroed low frequency
				npd power;
				*v = vect[i];
				Magnitude2(&power,v);
				AddNPD_P1(powerSum,&power);
#ifdef NOISE_ACCUM
				AddNPD_P1(pn,&power);
				pn++;
#endif
				v++; cnt++;
			}
		}
	}
	if (cnt!=PROFILE_SIZE) printf("Error: SaveProfile, PROFILE_SIZE != cnt");
#else
	memcpy(v,(&vect[4]),PROFILE_SIZE*sizeof(*v));
	for (i=0; i<PROFILE_SIZE; i++) {
		npd power;
		Magnitude2(&power,v);
		AddNPD_P1(powerSum,&power);
#ifdef NOISE_ACCUM
		AddNPD_P1(pn,&power);
		pn++;
#endif
		v++;
	}
#endif
}
static inline void LoadProfile(CleanNoiseWork* cnw,cmplx* vect)
{
	int i,k;
	int logN = cnw->logN;
	const int n_d2=(1<<(logN-1));
	const int n_d4=(1<<(logN-2));
	const int n3_d4=(3<<(logN-2));
	cmplx* v = cnw->v;
	int cnt=0;
	int index = cnw->vIndex + 1;
	if (index >= CONSECUTIVE_CNT) index = 0;
	index *= PROFILE_SIZE;
	v += index;

	memset(vect,0,4*sizeof(*vect));		//zero 1st 4 entries
#ifdef SKIP_REVERSAL_SORT
	for (k=4; k<=n_d2; k <<= 1) {
		int stop = k + (k>>1);
		int i = k;
		cmplx* a = &vect[i];
		cmplx* b = &vect[(i<<1)-1];
		while (i<stop) {
			if ( ((i & ~n3_d4)==0) || (((i+1)& ~n3_d4)==0) ) {	//if a zeroed low frequency
				memset(a,0,sizeof(*a));
				memset(b,0,sizeof(*b));
			} else {
				*a = *v++;
				cnt++;
				b->r = a->r;
				Negate2(&b->i,&a->i);
			}
			a++;
			i++;
			if (i&1) {
				memset(a,0,2*sizeof(*a));
				b-=2;
				memset(b,0,2*sizeof(*b));
				a+=2;
				i += 2;
			}
			b--;
		}
	}
	if (cnt!=PROFILE_SIZE) printf("Error: LoadProfile, PROFILE_SIZE != cnt");
#else
	memcpy(&vect[4],v,PROFILE_SIZE*sizeof(*vect));
	memset(&vect[PROFILE_SIZE+4],0,(n_d2+1-(PROFILE_SIZE+4))*sizeof(*vect));
	for (i=1; i<n_d2; i++) {
		vect[n-i].r = vect[i].r;
		Negate2(&vect[n-i].i,&vect[i].i);
	}
#endif
}
static inline void DoSymmetry(cmplx * vect,const int n_d2,const int n)
{
	int i;
#ifdef SKIP_REVERSAL_SORT
	for (i=2; i<=n_d2; i<<=1) {
		cmplx* a = &vect[i];
		cmplx* b = &vect[(i<<1)-1];
		for (; a<b; a++,b--) {
			b->r = a->r;
			Negate2(&b->i,&a->i);
		}
	}
#else
	for (i=1; i<n_d2; i++) {
		vect[n-i].r = vect[i].r;
		Negate2(&vect[n-i].i,&vect[i].i);
//		printf("%i %i %8x %8x\r\n",i,n-i,vect[i].r.n[NP_CNT-1],vect[n-i].r.n[NP_CNT-1]);
	}
#endif
}

static inline int NonZero(cmplx* vPrev)
{
	int i;
	for (i=0; i<NP_CNT; i++) if (vPrev->r.n[i] ||vPrev->i.n[i]) return 1;
	return 0;
}
static inline int AllNonZero(cmplx* vect,cmplx* vPrev,cmplx* vStart,int index,int psize,npd* noise)
{
	do {
		index++;
		if (index >= CONSECUTIVE_CNT) {index = 0; vect = vStart;}
		else vect += psize;
		if (vect == vPrev) break;
		if (CmpPower(vect,noise) < 0) return 0;
	} while (1);
	return 1;
}
//Polar conversions
//M = (A**2 + B**2) ** 1/2
//Phase = arctan(B/A)

//A = M * cos(Phase)
//B = M * sin(Phase)
static void SubtractNoise(CleanNoiseWork* cnw,npd* noise,const int psize)
{
	int i;
	int index = cnw->vIndex;
	cmplx* vStart = cnw->v;
	cmplx* vPrev = vStart + (index * psize);
	cmplx* vect;
	index++;
	if (index >= CONSECUTIVE_CNT) index = 0;
	vect = vStart + (index * psize);

	for (i=0; i<psize; i++)
	{
		npd origPower;
		npd newPower;
		Magnitude2(&origPower,vect);
		newPower = origPower;
		if (SubOrZero(&newPower,noise) && (NonZero(vPrev) || AllNonZero(vect,vPrev,vStart,index,psize,noise))) {
			np temp1;
			npd t1;
			DivNpd(&t1,&newPower,&origPower);	//newPower<origPower, so fraction is <1

			Sqroot(&temp1,&t1);
			MultSigned(&vect->r,&temp1);
			MultSigned(&vect->i,&temp1);
		} else {
			memset(vect,0,sizeof(*vect));	//below noise threshold
		}
		vect++; vPrev++; vStart++;
		noise++;
	}
}
static inline npd_p1* GetPowerSum(CleanNoiseWork* cnw)
{
	int index = cnw->vIndex + 1;
	if (index >= CONSECUTIVE_CNT) index = 0;
	return &cnw->powerSum[index];
}
static inline int CheckAfterPowerSum(CleanNoiseWork* cnw)
{
	int stop = cnw->vIndex;
	int i = stop+2;
	int cnt = 0;
	if (i >= CONSECUTIVE_CNT) i -= CONSECUTIVE_CNT;
	while ((i!=stop) && (cnt<SILENCE_AFTER_UPDATE)) {
		if (CmpNPD_P1(&cnw->powerSum[i],&cnw->noiseSum) > 0) return 1;
		cnt++;
		i++;
		if (i >= CONSECUTIVE_CNT) i = 0;
	}
	return 0;
}
static inline void PrintFFT_Average(const npd_p1* noise,const int noiseCnt,const int size)
{
	int i,j;
	npd temp;
	int cnt = 0;
	int divisor = noiseCnt;
	for (i=0; i<size; i++) {
		DivNpd_p1(&temp,noise,divisor);
		if ((i & 3)==0) printf("\r\n%i: ",i);
		printf(" %iHz{",((i*44100)>>10));

		for (j=0; j<NP_CNT-1; j++) printf("0x%08x,",temp.n[j]);
		printf("0x%08x}",temp.n[j]);
		printf((i!=(size-1)) ? "," : "};");
		noise++;
		cnt++;
	}
	printf("\r\n");
}
static inline void PrintNoise(const npd_p1* noise,const int noiseCnt,const int size)
{
	int i,j;
	npd temp;
	int divisor = (noiseCnt*2/3);	//!!!!!   make noise 1.5 times the average  !!!!!!!
#ifdef ARM
	FILE* f = fopen("noise_default-arm.c","w");
#else
	FILE* f = fopen("noise_default.c","w");
#endif
	fprintf(f,"const npd noise_default[%i] = {",size);
	for (i=0; i<size; i++) {
		DivNpd_p1(&temp,noise,divisor);
		if ((i & 3)==0) fprintf(f,"\r\n");

		fprintf(f,"{");
		for (j=0; j<NPD_CNT-1; j++) fprintf(f,"0x%08x,",temp.n[j]);
		fprintf(f,"0x%08x}",temp.n[j]);
		fprintf(f,(i!=(size-1)) ? "," : "};");
		noise++;
	}
	fprintf(f,"\r\n");
}
static inline void PrintSamples(const short* pSamples,const int n)
{
	int i;
	printf("--------------------\n");
	for (i=0; i<n; i++) {
		if ((i&7)==0) printf("\r\n%3i:",i);
//		printf("%7i",*pSamples);
		printf("%04x ",*pSamples);
		pSamples++;
	}
	printf("\n");
}
static void inline ShiftRight4Signed(npd* dest)
{
	int i;
	const int k=4;
	const int l=32-k;
	for (i=0; i<NPD_CNT-1; i++) {
		dest->n[i] = (dest->n[i] >> k) + (dest->n[i+1]<<l);
	}
	dest->n[i] = (unsigned int)(  ((int)dest->n[i]) >> k);
}

static inline void UpdateNoise(npd* n,const npd* p)
{
	int i;
	npd temp1 = *p;
	npd temp2 = *p;
	ShiftRight(&temp2);			//!!!!!   make noise 1.5 times the average  !!!!!!!
	AddNpd(&temp1,&temp2);

	SubNpd(&temp1,n);
	ShiftRight4Signed(&temp1);
	AddNpd(n,&temp1);
}
static inline void UpdateNoiseProfile(CleanNoiseWork* cnw,const int psize)
{
	int i;
	npd* n = &cnw->noise[0];
	int index = cnw->vIndex + 1;
	cmplx* vect;
	if (index >= CONSECUTIVE_CNT) index = 0;
	vect = cnw->v + (index * psize);

	memset(&cnw->noiseSum,0,sizeof(cnw->noiseSum));
	for (i=0; i<psize; i++) {
		npd power;
		Magnitude2(&power,vect);
		UpdateNoise(n,&power);
		AddNPD_P1(&cnw->noiseSum,n);
		n++; vect++;
	}
//	printNpd_p1("noiseSum: 0x",&noiseSum);
}


int CleanNoise(short* dest,int bufSize,short* src,int startPos,int srcBufMask,CleanNoiseWork* cnw)
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

	DBGP("a");
	fft_forward_overlap(vect,src,startPos,srcBufMask,logN);
//	PrintTable(vect,logN,AFTER_FFT_SHIFT,1);
	DBGP("b");
//	printNpd_p1("a noiseSum: 0x",&cnw->noiseSum);

	SaveProfile(cnw,vect);
	DBGP("c");
#ifdef NOISE_ACCUM
	cnw->noiseCnt++;
	if (cnw->noiseCnt >= REP_FFT_CNT) return -1;
#endif

	if (cnw->state < (CONSECUTIVE_CNT-1)) {
		DBGP("z");
		cnw->state++;
		return 0;
	}
	DBGP("d");
//	printNpd_p1("b noiseSum: 0x",&cnw->noiseSum);
	if (CmpNPD_P1(GetPowerSum(cnw),&cnw->noiseSum) > 0) cnw->silenceCnt=0;
	else {
//			printf("0x%08x %08x,  0x%08x %08x\r\n",powerSum.n[NPD_P1_CNT-1],powerSum.n[NPD_P1_CNT-2],cnw->noiseSum.n[NPD_P1_CNT-1],cnw->noiseSum.n[NPD_P1_CNT-2]);
			cnw->silenceCnt++;
			if (cnw->silenceCnt >= SILENCE_BEFORE_UPDATE) {
				if (CheckAfterPowerSum(cnw) > 0) cnw->silenceCnt=SILENCE_BEFORE_UPDATE-1;
			}
	}

	if (cnw->silenceCnt < SILENCE_BEFORE_UPDATE) {
		//this is a signal
		int max = bufSize;
		short* p = dest;
		if (max>MAX_ADD_SIZE) max = MAX_ADD_SIZE;
		printf(".");
		SubtractNoise(cnw,&cnw->noise[0],PROFILE_SIZE);
		LoadProfile(cnw,vect);
#if 0
		PrintTable(vect,logN,AFTER_FFT_SHIFT,1);
#endif
		pSamples = outputBuffer;
		fft_inverse(pSamples,vect,logN);
//		PrintTable(vect,logN,FINAL_SHIFT,0);
//		PrintSamples(pSamples,n);
//		PrintSamples(p,max);
		for (i=0; i<max; i++) {
			int val = *p + *pSamples++;
			if (val>0x7fff) val = 0x7fff;
			else if (val< -0x8000) val = -0x8000;
			*p++ = val;
		}
//		PrintSamples(p-max,max);
		max = bufSize - MAX_ADD_SIZE;
		if (max>n_d4) max = n_d4;
		if (max>0) memcpy(p,pSamples,max*sizeof(*p));
	} else {
		short* p = dest+MAX_ADD_SIZE;
		int max = bufSize - MAX_ADD_SIZE;
		if (max>n_d4) max = n_d4;
		if (max > 0) memset(p,0,max*sizeof(*p));

		//now update noise values
		printf(" ");
		UpdateNoiseProfile(cnw,PROFILE_SIZE);
	}
	return n_d4;
}

void Finish_cnw(CleanNoiseWork* cnw)
{
	free(cnw->noise);
	cnw->noise = NULL;
	free(cnw->v);
	cnw->v = NULL;
#ifdef NOISE_ACCUM
	if (cnw->noiseAccum) {
		const int logN=cnw->logN;
		const int n_d2=(1<<(logN-1));
		const int n_d4=(1<<(logN-2));
#ifdef AVERAGE_FFT
		PrintFFT_Average(cnw->noiseAccum,cnw->noiseCnt,PROFILE_SIZE);
#else
		PrintNoise(cnw->noiseAccum,cnw->noiseCnt,PROFILE_SIZE);
#endif
		free(cnw->noiseAccum);
		cnw->noiseAccum = NULL;
	}
#endif
}
static void Init1_cnw(CleanNoiseWork* cnw,const int logN)
{
	const int n_d2=(1<<(logN-1));
	const int n_d4=(1<<(logN-2));
	int i;
	int size;
#ifndef NOISE_ACCUM
#ifdef ARM
#include "noise_default-arm.c"
#else
#include "noise_default.c"
#endif
#endif

	if (cnw->logN != logN) Finish_cnw(cnw);	//free space on size change
	cnw->logN = logN;
#ifdef NOISE_ACCUM
	if (cnw->noiseAccum==NULL) {
		cnw->noiseAccum = (npd_p1*)malloc(PROFILE_SIZE*sizeof(*cnw->noiseAccum));
		memset(cnw->noiseAccum,0,(PROFILE_SIZE)*sizeof(cnw->noiseAccum[0]));
		cnw->noiseCnt = 0;
	}
#endif

	if (cnw->noise==NULL) {
		cnw->noise = (npd*)malloc(PROFILE_SIZE*sizeof(*cnw->noise));
#ifdef NOISE_ACCUM
		for (i=0; i<PROFILE_SIZE; i++) SetNpd(&cnw->noise[i],90000,logN);
#else
		memcpy(cnw->noise,noise_default,(PROFILE_SIZE)*sizeof(cnw->noise[0]));
#endif
	}
//we keep a buffer of 4 fft's. If the level is not above 1.5x silence for
//4 consecutive frames, then the frequency is zeroed
//this ensures that short duration noise spikes are removed
	size= CONSECUTIVE_CNT*PROFILE_SIZE*sizeof(*cnw->v);
	if (cnw->v==NULL) cnw->v = (cmplx*)malloc(size);
	memset(cnw->v,0,size);
	cnw->vIndex = 0;
	cnw->state = 0;

	cnw->silenceCnt = 0; //SILENCE_BEFORE_UPDATE-1;
	memset(&cnw->noiseSum,0,sizeof(cnw->noiseSum));
	for (i=0; i<PROFILE_SIZE; i++) AddNPD_P1(&cnw->noiseSum,&cnw->noise[i]);
//	printNpd_p1("noiseSum: 0x",&cnw->noiseSum);
}
void Init_cnw(CleanNoiseWork* cnw,int logN)
{
#ifdef NOISE_ACCUM
	cnw->noiseAccum = NULL;
#endif
	cnw->noise = NULL;
	cnw->v = NULL;
	Init1_cnw(cnw,logN);
}
