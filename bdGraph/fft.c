#include "fft.h"
//#define DEBUG

#define NP_UNIT 0		//# of bits

#ifdef DO_AVERAGE
#define LEVEL(a) a
#define AVE(rtn,inverse,dest,src) (inverse)? rtn(dest,src) : rtn##Average(dest,src)


#else
#define LEVEL(a) 0
#define AVE(rtn,inverse,dest,src) rtn(dest,src)

#endif


//Skipping the Bit revsersal sort on both the fft & ifft will cancel each other out
#define LOG_SIN_TABLE_SIZE 8	//where size the index of entry pi/2
#include "sinTable8.c"	//table isn't reversed so algorithm is

#define F_SE_PI (1<<(LOG_SIN_TABLE_SIZE+1))
#define F_SE_PI_D2 (1<<LOG_SIN_TABLE_SIZE)
#define F_SE_SIGN F_SE_PI
#define F_SIN_TABLE(i) &sinTable8[(F_SE_PI_D2&i) ? (F_SE_PI_D2-(i&(F_SE_PI_D2-1))) : (i&(F_SE_PI_D2-1))]
#define F_PLUS_PI_D2(i) (i+F_SE_PI_D2)
#define F_TEST_USE_LOOKUP(i) (i&(F_SE_PI_D2-1))
#define F_ADJUST_PHASE(phase) IncReversed(phase,F_SE_PI_D2)


#if defined(SKIP_REVERSAL_SORT) && 1		//this can be disabled to use only a single table, but slower
#include "sinTable8r.c"	//table is reversed so algorithm isn't

#define R_SE_PI 1
#define R_SE_PI_D2 2
#define R_SE_SIGN R_SE_PI
#define R_SIN_TABLE(i) &sinTable8r[(i>>1)]
#define R_PLUS_PI_D2(i)  ((i&R_SE_PI_D2) ? (i^(R_SE_PI_D2|R_SE_SIGN)) : i^(R_SE_PI_D2))
#define R_TEST_USE_LOOKUP(i) (i>>2)
#define R_ADJUST_PHASE(phase) (phase+R_SE_PI_D2)

#define SE_PI(inverse)			((inverse) ? F_SE_PI :			R_SE_PI)
#define SE_PI_D2(inverse)		((inverse) ? F_SE_PI_D2 :		R_SE_PI_D2)
#define SE_SIGN(inverse)		((inverse) ? F_SE_SIGN : 		R_SE_SIGN)
#define SIN_TABLE(i,inverse)		((inverse) ? F_SIN_TABLE(i) :		R_SIN_TABLE(i))
#define PLUS_PI_D2(i,inverse)		((inverse) ? F_PLUS_PI_D2(i) :		R_PLUS_PI_D2(i))
#define TEST_USE_LOOKUP(i,inverse)	((inverse) ? F_TEST_USE_LOOKUP(i) :	R_TEST_USE_LOOKUP(i))
#define ADJUST_PHASE(phase,inverse)	((inverse) ? F_ADJUST_PHASE(phase) :	R_ADJUST_PHASE(phase))

#else
#define SE_PI(inverse)			F_SE_PI
#define SE_PI_D2(inverse)		F_SE_PI_D2
#define SE_SIGN(inverse)		F_SE_SIGN
#define SIN_TABLE(i,inverse)		F_SIN_TABLE(i)
#define PLUS_PI_D2(i,inverse)		F_PLUS_PI_D2(i)
#define TEST_USE_LOOKUP(i,inverse)	F_TEST_USE_LOOKUP(i)
#define ADJUST_PHASE(phase,inverse)	F_ADJUST_PHASE(phase)
#endif

#define LOG_SIN2_TABLE_SIZE 7	//where size the index of entry pi/2
#include "sin2Table7.c"
#define SIN2_SE_PI_D2 (1<<LOG_SIN2_TABLE_SIZE)
#define SIN2_TABLE(i)  &sin2Table7[(SIN2_SE_PI_D2&i) ? (SIN2_SE_PI_D2-(i&(SIN2_SE_PI_D2-1))) : (i&(SIN2_SE_PI_D2-1))]
#define SIN2_TEST_USE_LOOKUP(i) (i&(SIN2_SE_PI_D2-1))

static inline int IncReversed(int i,int k)
{
	//now increment bit reversed i
//	printf("IncR(%i,%i)=",i,k);
	do {
		i ^= k;
		if (i&k) break;
		k>>=1;
	} while (k);
//	printf("%i\n",i);
	return i;
}



void PrintNpd(const npd* src,int level)
{
	int i;
	unsigned int t[NPD_CNT];
	unsigned int dest[NPD_CNT+1];
	unsigned int c=0;
	memset(&dest,0,(NPD_CNT+1)*sizeof(dest[0]));
//	for (i=NPD_CNT-1; i>=0; i--) printf("%8x ",src->n[i]);

	for (i=0; i<NPD_CNT; i++) {
		unsigned int a = src->n[i];
		t[i] = (a<<(NP_DATA_UNIT+NP_UNIT+level))+c;
		c = a>>(32-(NP_DATA_UNIT+NP_UNIT+level));
	}
	for (i=0; i<NPD_CNT; i++) {
		AddM(&dest[i],t[i],100000000);
	}
	if (dest[NPD_CNT-1]>>31) {
		if (dest[NPD_CNT]!=99999999) dest[NPD_CNT]++;
		else {dest[NPD_CNT]=0; c++;}
	}
	printf("(%i.%08i)",c,dest[NPD_CNT]);
}
void PrintNpx(const np* src,int level,int signedVal)
{
	int i;
	unsigned int t[NP_CNT];
	unsigned int dest[NP_CNT+1];
	unsigned int c=0;
	unsigned int d;
	unsigned int d1;
	const unsigned int* s;
	int sign=0;
	np temp;
//	printf("level:%i,signedVal:%i ",level,signedVal);
//	for (i=NP_CNT-1; i>=0; i--) printf("%8x ",src->n[i]);
	memset(&dest,0,(NP_CNT+1)*sizeof(dest[0]));
	if (signedVal) {
		sign = (src->n[NP_CNT-1]>>31);
		if (sign) {
			Negate2(&temp,src);
			src = &temp;
		}
	}
	s = &src->n[0];
	if (level) {
		for (i=0; i<NP_CNT; i++) {
			unsigned int a = *s++;
			t[i] = (a<<level)+c;
			c = a>>(32-level);
		}
		s = &t[0];
	}
	for (i=0; i<NP_CNT; i++) {
		AddM(&dest[i],s[i],100000000);
	}
	d1 = dest[NP_CNT-1];
	d = dest[NP_CNT];
	if (d1>>31) {
		if (d!=99999999) {d++;}
		else {d=0; c++;}
	}
#if 0
	for (i=NP_CNT-1; i>=0; i--) printf("%8x ",src->n[i]);
	printf("(%i.%08i)",c,dest[NP_CNT]);
#else
	if (sign) c=-c;
	if (d) printf("%3i.%08i",c,d);
	else printf("%3i",c);
#endif
}
void PrintNpData(const np* src,int level)
{
	PrintNpx(src,level+NP_DATA_UNIT,1);
}
void PrintNp(const np* src)
{
	PrintNpx(src,NP_UNIT,0);
}

static inline void __MultSin(np* dest,int i,const int inverse)
{
	int sign=i&SE_SIGN(inverse);
#if 0
		printf("sin(%i) *",i);
		PrintNpData(dest,0);
		printf("=");
#endif
	if (TEST_USE_LOOKUP(i,inverse)) {
		const np* psin = SIN_TABLE(i,inverse);
		npd temp;
		if ( ((int)(dest->n[NP_CNT-1]))<0) {
			Negate(dest);
			sign ^= SE_SIGN(inverse);
		}
		MultNp(&temp,dest,psin);
		memcpy(dest,&temp.n[NP_CNT],sizeof(*dest));
	} else if (i & SE_PI_D2(inverse)) {
		//*1 do nothing
	} else {
		//*0
		memset(dest,0,sizeof(*dest));
#if 0
		PrintNpData(dest,0);
#endif
		return;
	}
	if (sign) Negate(dest);
#if 0
		PrintNpData(dest,0);
#endif
}
static inline void __MultCos(np* dest,int i,const int inverse)
{
#if 0
		printf("cos(%i) *",i);
		PrintNpData(dest,0);
		printf("=");
#endif
	__MultSin(dest,PLUS_PI_D2(i,inverse),inverse);
#if 0
		PrintNpData(dest,0);
#endif
}
#ifdef SKIP_REVERSAL_SORT
static void MultSin(np* dest,int i,const int inverse)
{
	if (inverse) __MultSin(dest,i,1);		//let compiler optimize it
	else __MultSin(dest,i,0);
}
static void MultCos(np* dest,int i,const int inverse)
{
	if (inverse) __MultCos(dest,i,1);		//let compiler optimize it
	else __MultCos(dest,i,0);
}
#else
static void MultSin(np* dest,int i,const int inverse)
{
	__MultSin(dest,i,1);		//let compiler optimize it
}
static void MultCos(np* dest,int i,const int inverse)
{
	__MultCos(dest,i,1);		//let compiler optimize it
}
#endif

static inline void fft_butterfly(cmplx* a,cmplx* b,const int phase,const int inverse, const int level,const int i,const int j)
{
    	cmplx t;
#ifdef DEBUG
#if 0
	printf("phase:%i,sin:",phase);
	if (phase!=SE_PI_D2(inverse)) {
		const np* psin = SIN_TABLE(phase,inverse);
		PrintNp(psin);
	} else printf("1.0");
	printf("\n");
#endif
	printf("{%i:",i);
	PrintNpData(&a->r,LEVEL(level));
	printf(", ");
	PrintNpData(&a->i,LEVEL(level));
	printf("}, ");

	printf("{%i:",j);
	PrintNpData(&b->r,LEVEL(level));
	printf(", ");
	PrintNpData(&b->i,LEVEL(level));
	printf("}");
#endif
/*
	double tr =  b->r*cosa + b->i*sina;
	double ti = -b->r*sina + b->i*cosa;
	b->r = a->r - tr;
	b->i = a->i - ti;
	a->r += tr;
	a->i += ti;
*/
//	printf("%i,%i\n",i,j);
	t = *b;
	MultCos(&t.r,phase,inverse);
	MultSin(&t.i,phase,inverse);

	AddNp(&t.r,&t.i);		//worst case is phase=pi/4 sin,cos=.707107 won't overflow
					//because you can't maximize both tr & ti and
					//they will be calculated twice before getting to pi/4 (I hope)
	MultSin(&b->r,phase,inverse);
	MultCos(&b->i,phase,inverse);
	SubNp(&b->i,&b->r);
	t.i = b->i;
	b->r = t.r;	//now *b == t

	AVE(RSubNp,inverse,&b->r,&a->r);
	AVE(RSubNp,inverse,&b->i,&a->i);

	AVE(AddNp,inverse,&a->r,&t.r);
	AVE(AddNp,inverse,&a->i,&t.i);
#ifdef DEBUG
	printf("{%i:",i);
	PrintNpData(&a->r,LEVEL(level+1));
	printf(", ");
	PrintNpData(&a->i,LEVEL(level+1));
	printf("}, ");

	printf("{%i:",j);
	PrintNpData(&b->r,LEVEL(level+1));
	printf(", ");
	PrintNpData(&b->i,LEVEL(level+1));
	printf("}\n");
#endif
}

static inline void ReverseOrder(cmplx* vect,const int n,const int n_d2)
{
	int i,j;
	for (i=1,j=n_d2; i<n-1; i++,j=IncReversed(j,n_d2)) {
		if (i<j) {
			cmplx t;
			t = vect[i];
			vect[i] = vect[j];
			vect[j] = t;
		}
	}
}
static inline void NormalSynthesis(cmplx* vect,const int n,const int n_d2,const int inverse)
{
    int advance;
    int i,j;
    int phase;
    int base;
    int level;
    int phaseAdd;
    for (advance=1,phaseAdd=SE_PI(1),level=0; advance<n; advance<<=1, phaseAdd>>=1, level++) {
        for (base=0,phase=0; base < advance; base++,phase += phaseAdd) {
#ifdef DEBUG
	    printf("base:%i, advance*2:%i, n:%i, phase:%i, sin:",base,(advance<<1),n,phase);
	    if (phase!=SE_PI_D2(1)) {
		const np* psin = SIN_TABLE(phase,1);
		PrintNp(psin);
	    } else printf("1.0");
	    printf("\n");
#endif
            for (i = base; i < n; i += (advance<<1)) {
		fft_butterfly(&vect[i],&vect[i+advance],phase,inverse,level,i,i+advance);
            };
        };
#ifdef DEBUG
	printf("\n");
#endif
    };
};
static inline void ReverseSynthesis(cmplx* vect,const int n,const int n_d2)
{
    int advance;
    int i;
    int phase;
    int base;
    int level;

    for (advance=n_d2, level=0; advance; advance>>=1, level++) {
        for (base=0,phase=0; base < n; base+=(advance<<1),phase=ADJUST_PHASE(phase,0)) {
#ifdef DEBUG
	    printf("base:%i, advance*2:%i, n:%i, phase:%i, sin:",base,(advance<<1),n,phase);
	    if (phase!=SE_PI_D2(0)) {
		const np* psin = SIN_TABLE(phase,0);
		PrintNp(psin);
	    } else printf("1.0");
	    printf("\n");
#endif
            for (i=base; i < base+advance; i++) {
		fft_butterfly(&vect[i],&vect[i+advance],phase,0,level,i,i+advance);
            };
        };
#ifdef DEBUG
	printf("\n");
#endif
    };
};

//synthesis order
// 0 128 256 384  512 640 768 896
// 0|512 128|640  256|768 384|896
// 0|512|256|768  128|640|384|896
// 0|512|256|768|128|640|384|896
// >>7
// 0|  4|  2|  6|  1|  5|  3|  7
//bit reverse
// 0|  1|  2|  3|  4|  5|  6|  7
#ifdef SKIP_REVERSAL_SORT
#define FORWARD_FFT(vect,logN) fft_Sin_Rout(vect,logN)
#define INVERSE_FFT(vect,logN) fft_Rin_Sout(vect,logN)
//Straight in Reversed out
static void fft_Sin_Rout(cmplx * vect,const int logN)
{
    const int n=1<<logN;
    const int n_d2=1<<(logN-1);

    if (logN > (LOG_SIN_TABLE_SIZE+2)) {
    	printf("sin table is too small");
	return;
    }
    ReverseSynthesis(vect,n,n_d2);	//Forward FFT
}

//Reversed in, Straight Out
static void fft_Rin_Sout(cmplx * vect,const int logN)
{
    const int n=1<<logN;
    const int n_d2=1<<(logN-1);

    if (logN > (LOG_SIN_TABLE_SIZE+2)) {
    	printf("sin table is too small");
	return;
    }
    NormalSynthesis(vect,n,n_d2,1);	//Inverse FFT
}
#else

#define INVERSE_FFT(vect,logN) I_fft_Sin_Sout(vect,logN)
static void I_fft_Sin_Sout(cmplx * vect,const int logN)
{
    const int n=1<<logN;
    const int n_d2=1<<(logN-1);

    if (logN > (LOG_SIN_TABLE_SIZE+2)) {
    	printf("sin table is too small");
	return;
    }
    ReverseOrder(vect,n,n_d2);
    NormalSynthesis(vect,n,n_d2,1);
}
#ifdef DO_AVERAGE
#define FORWARD_FFT(vect,logN) F_fft_Sin_Sout(vect,logN)
//Straight in, Straight out
static void F_fft_Sin_Sout(cmplx * vect,const int logN)
{
    const int n=1<<logN;
    const int n_d2=1<<(logN-1);

    if (logN > (LOG_SIN_TABLE_SIZE+2)) {
    	printf("sin table is too small");
	return;
    }
    ReverseOrder(vect,n,n_d2);
    NormalSynthesis(vect,n,n_d2,0);
}
#else
#define FORWARD_FFT(vect,logN) I_fft_Sin_Sout(vect,logN)
#endif
#endif

static inline void ChooseFFT(cmplx * vect,const int logN,const int inverse)
{
	if (inverse) {INVERSE_FFT(vect,logN);}
	else {FORWARD_FFT(vect,logN);}
}

//the real samples 0 - (n-1) are in the real&cmpl part of samples 0 - (n/2-1)
//high half of array is ununsed on input
static void fft_combine(cmplx* vect,const int logN,const int inverse)
{
	int i,phase;
	const int n=1<<logN;
	const int n_d4=1<<(logN-2);
	cmplx* a;
	cmplx* b;
	cmplx* c;
	cmplx* d;
	ChooseFFT(vect,logN-1,inverse);	//this calculates the fft of 2 signals
				//which now need combined
//separate the 2 fft's
#ifdef SKIP_REVERSAL_SORT
	if (inverse==0) {
//0 1 2 3 4 5 6 7 8 9 a b c d e f
//              |___|
//            |_______|
//          |___________|
//        |_______________|
//      |___________________|
//    |_______________________|
//  |___________________________|

//Bit reversed symmetry
//0 1 2 3 4 5 6 7 8 9 a b c d e f
//0 8 4 c 2 a 6 e 1 9 5 d 3 b 7 f
//    |_|   |_|         |_|
//        |_____|     |_____|
//                  |_________|
//                |_____________|

		for (i=n_d4; i>1; i>>=1) {
			a = &vect[i];
			b = &vect[(i<<1)-1];
			c = &vect[(i<<1)];
			d = &vect[(i<<2)-1];
			for (; a<b; a++,b--) {
				Add3NpAverage(&c->r,&a->r,&b->r);
				Sub3NpAverage(&c->i,&a->i,&b->i);
				d->r = c->r;
				Negate2(&d->i,&c->i);
				c++;
				d--;

				Add3NpAverage(&c->r,&a->i,&b->i);
				Sub3NpAverage(&d->i,&a->r,&b->r);
				d->r = c->r;
				Negate2(&c->i,&d->i);
				c++;
				d--;
			}
		}

		vect[3].r = vect[1].i;				//bit-reversed 3 is n3_d4, bit-reversed 2 is n_d4
		vect[2].r = vect[1].r;
		vect[1].r = vect[0].i;					//bit-reversed 1 is n_d2
		memset(&vect[0].i,0,sizeof(vect[0].i));			//vect[0].i = 0;
		memset(&vect[1].i,0,sizeof(vect[1].i));			//vect[n_d2].i = 0;
		memset(&vect[2].i,0,sizeof(vect[2].i));			//vect[n_d4].i = 0;
		memset(&vect[3].i,0,sizeof(vect[3].i));			//vect[n3_d4].i = 0;

	        for (i=0,phase=0; i < n; i+=2,phase=ADJUST_PHASE(phase,0)) {
			fft_butterfly(&vect[i],&vect[i+1],phase,0,logN-1,i,i+1);
	        };
	} else
#endif
	{
		const int phaseAdd = SE_PI(inverse)>>(logN-1);
		const int n3_d4=3<<(logN-2);
		const int n_d2=1<<(logN-1);
		const int advance=n_d2;
		a = &vect[1];
		b = &vect[n_d2-1];
		c = &vect[n_d2+1];
		d = &vect[n-1];
		for (; a<b; a++,b--,c++,d--) {
			Add3NpAverage(&c->r,&a->i,&b->i);
			Sub3NpAverage(&d->i,&a->r,&b->r);
			d->r = c->r;
			Negate2(&c->i,&d->i);

			AddNpAverage(&a->r,&b->r);
			SubNpAverage(&a->i,&b->i);
			b->r = a->r;
			Negate2(&b->i,&a->i);
		}
		vect[n3_d4].r = vect[n_d4].i;
		vect[n_d2].r = vect[0].i;
		memset(&vect[n3_d4].i,0,sizeof(vect[n3_d4].i));
		memset(&vect[n_d2].i,0,sizeof(vect[n_d2].i));		//vect[n_d2].i = 0;
		memset(&vect[n_d4].i,0,sizeof(vect[n_d4].i));		//vect[n_d4].i = 0;
		memset(&vect[0].i,0,sizeof(vect[0].i));			//vect[0].i = 0;
//combine into 1 larger fft
		for (i=0,phase=0; i < advance; i++,phase += phaseAdd) {
			fft_butterfly(&vect[i],&vect[i+advance],phase,inverse,logN-1,i,i+advance);
		}
	}
}

static void ChangeImgSign(cmplx* vect,const int logN)
{
	const int n = 1<<logN;
	int i;
	for (i=0; i<n; i++) {
		Negate(&vect[i].i);
	}
}
static void Symmetry(cmplx* vect,const int logN)
{
	const int n = 1<<logN;
	const int n_d2 = 1<<(logN-1);
	int i;
	for (i=(n_d2+1); i<n; i++) {
		vect[i].r = vect[n-i].r;
		Negate2(&vect[i].i,&vect[n-i].i);
	}
}
static Ifft(cmplx* vect,const int logN)
{
	const int n = 1<<logN;
	const int n_d2 = 1<<(logN-1);
	int i;
#ifdef SKIP_REVERSAL_SORT
//Bit reversed symmetry
//0 1 2 3 4 5 6 7
//0 4 2 6 1 5 3 7
//
//0 1 2 3 4 5 6 7 8 9 a b c d e f
//0 8 4 c 2 a 6 e 1 9 5 d 3 b 7 f
//real
//0 2 4 6 8 a c e	//sample #
//0 4 2 6 1 5 3 7	//bit reversed
//0 1 2 3 4 5 6 7	//new sample order
//imaginary
//1 3 5 7 9 b d f
//8 c a e 9 d b f
//8 9 a b c d e f

	for (i=0; i<n_d2; i++) {
		AddNp(&vect[i].r,&vect[i].i);
		Add3Np(&vect[i].i,&vect[i+n_d2].r,&vect[i+n_d2].i);
	}
#else
//	Symmetry(vect,logN);		//should already be symmetrical
	for (i=0; i<n_d2; i++) {
		Add3Np(&vect[i].r,&vect[i<<1].r,&vect[i<<1].i);
		Add3Np(&vect[i].i,&vect[(i<<1)+1].r,&vect[(i<<1)+1].i);
	}
#endif

	fft_combine(vect,logN,1);
	for (i=0; i<n; i++) {
		AddNp(&vect[i].r,&vect[i].i);	// just using the most significant bits will divide by N
		memset(&vect[i].i,0,sizeof(vect[i].i));
	}
}
static void SetCmplx(cmplx* dest,int i, int val)
{
	if ((i&1)==0) dest[i>>1].r.n[NP_CNT-1] = val<<(32-NP_DATA_UNIT);
	else dest[i>>1].i.n[NP_CNT-1] = val<<(32-NP_DATA_UNIT);
}
static void inline SetSin2(np* dest,int val,int phase)
{
	int i;
	for (i=0; i<NP_CNT-1; i++) dest->n[i]=0;

	if (SIN2_TEST_USE_LOOKUP(phase)) {
		const np* psin2 = SIN2_TABLE(phase);
		int sign=0;
		npd temp;
		if (val<0) { val=-val; sign=1;}
		dest->n[NP_CNT-1] = val<<(32-NP_DATA_UNIT);
		MultNp(&temp,dest,psin2);
		memcpy(dest,&temp.n[NP_CNT],sizeof(*dest));
		if (sign) Negate(dest);
	} else if (phase & SIN2_SE_PI_D2) {
		//*1 do nothing
		dest->n[NP_CNT-1] = val<<(32-NP_DATA_UNIT);
	} else {
		//*0
		dest->n[NP_CNT-1] = 0;
	}
}

static void inline SetCmplxSin2(cmplx* dest,int i, int val,int phase)
{
	np* a;

	if ((i&1)==0) a = &dest[i>>1].r;
	else a = &dest[i>>1].i;

	SetSin2(a,val,phase);
}

void PrintTable(cmplx* vect,const int logN,const int level,const int bitReversal)
{
	const int n=(1<<logN);
	int i;
	const int n_d2=(1<<(logN-1));
	int j;
	printf("\n---------------------------\n");
	for (i=0,j=0; i<n; i++,j=(bitReversal) ? IncReversed(j,n_d2) : (j+1)) {
		if ((i&3)==0) printf("\n%3i:",i);
		printf("{");
#ifndef SKIP_REVERSAL_SORT
		j = i;
#endif
		PrintNpData(&vect[j].r,level);
		printf(", ");
		PrintNpData(&vect[j].i,level);
		printf("}, ");
	}
	printf("\n---------------------------\n");
}
void fft_forward(cmplx* vect,const short* src,const int logN)
{
	const int n=(1<<logN);
	int i;
	memset(vect,0,n*sizeof(vect[0]));
#if 1
	for (i=0; i<n; i++) {
		SetCmplx(vect,i,src[i]);
//		printf("%i %i\r\n",i,src[i]);
	}
//	PrintTable(vect,logN,0,0);

	fft_combine(vect,logN,0);

#else
	for (i=0; i<n; i++) {
		vect[i].r.n[NP_CNT-1] = src[i]<<(32-NP_DATA_UNIT);
	}
//	PrintTable(vect,logN,0,0);

	FORWARD_FFT(vect,logN);
#endif
}

//both dest and vect are updated
void fft_inverse(short* dest,cmplx* vect,const int logN)
{
	int i;
	const int n=(1<<logN);
#if 1
	Ifft(vect,logN);
#else
	ChangeImgSign(vect,logN);
	INVERSE_FFT(vect,logN);
#endif
	for (i=0; i<n; i++) {
		int val = vect->r.n[NP_CNT-1] + (1<<(31-FINAL_SHIFT-NP_DATA_UNIT));
		vect++;
		val >>= (32-FINAL_SHIFT-NP_DATA_UNIT);
		*dest++ = (short)val;
	}
}
//Blackman window
// w(k) = 0.42 + 0.50cos(2 pi k/N) + 0.08cos(4 pi k/N)
// where -N/2 <= k < N/2


//this will take 1<<(logN-1) samples, and multiply by sin*sin
//then pad with zeroes and take fft
void fft_forward_overlap(cmplx* vect,const short* src,int start,const int bufMask,const int logN)
{
	const int n=(1<<logN);
	const int n_d2=(1<<(logN-1));
	int i,phase;
	const int phaseAdvance = 1<<(LOG_SIN2_TABLE_SIZE+3-logN);
	if (logN > (LOG_SIN_TABLE_SIZE+2)) {
		printf("sin table is too small");
		return;
	}
	if (logN > (LOG_SIN2_TABLE_SIZE+3)) {
		printf("sin2 table is too small");
		return;
	}
#if 1
	for (i=0,phase=0; i<n_d2; i++,phase+=phaseAdvance) {
		SetCmplxSin2(vect,i,src[start],phase);
		start = (start+1)& bufMask;
//		printf("%i %i\r\n",i,src[i]);
	}
	memset(&vect[n_d2],0,n_d2*sizeof(vect[0]));

//	PrintTable(vect,logN,0,0);

	fft_combine(vect,logN,0);

#else
	memset(vect,0,n*sizeof(vect[0]));
	for (i=0,phase=0; i<n_d2; i++,phase+=phaseAdvance) {
		SetSin2(&vect[i].r,src[start],phase);
		start = (start+1)& bufMask;
	}
//	PrintTable(vect,logN,0,0);

	FORWARD_FFT(vect,logN);
#endif
}
