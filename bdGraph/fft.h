#ifndef NP_CNT
#define NP_CNT 1
#endif

#define NPD_CNT (NP_CNT+NP_CNT)

#define DO_AVERAGE
#define SKIP_REVERSAL_SORT

#ifdef DO_AVERAGE
#define NP_DATA_UNIT (16)		//# of bits
#define AFTER_FFT_SHIFT logN
#define FINAL_SHIFT 0

#else
#define NP_DATA_UNIT (16+10+2)		//# of bits
#define AFTER_FFT_SHIFT 0
#define FINAL_SHIFT (-logN)	//!defined(DO_AVERAGE
#endif

#define NPD_P1_CNT (NPD_CNT+1)
typedef struct NP {
	unsigned int n[NP_CNT];
} np;

typedef struct NPD {
	unsigned int n[NPD_CNT];
} npd;

typedef struct NPD_P1 {
	unsigned int n[NPD_P1_CNT];
} npd_p1;

typedef struct CMPLX                      /* complex pair */
{
	np	r;                      /* real part */
	np	i;                      /* imaginary part */
} cmplx;
void fft_forward(cmplx* vect,const short* src,const int logN);
void fft_inverse(short* dest,cmplx* vect,const int logN);
void PrintTable(cmplx* vect,const int logN,const int level,const int frequencyDomain);

#ifdef ARM
#if (NP_CNT==1)
#define ARM_1
#endif
static void inline AddM(unsigned int * p, unsigned a, unsigned b)
{
	__asm__ ("%@ AddM		\n\
		umull	%1,%2,%3,%4	\n\
		ldr	%3,[%0]		\n\
		ldr	%4,[%0,#4]	\n\
		adds	%1,%1,%3	\n\
		ldr	%3,[%0,#8]	\n\
		adcs	%2,%2,%4	\n\
		addcs	%3,%3,#1"
		  : "+r" (p), "=&r" (p[0]),"=&r" (p[1]),"=r" (p[2])
		  : "r" (a), "3" (b)
		  : "cc" );
//: output
//: input
//: clobbered
}
#elif defined(X386)

#if (NP_CNT==1)
#define X386_1
#endif

#define AddM(_p1,_a,_b) \
({  register unsigned int* _p = _p1;		\
	__asm__ ("mul	%%ebx		\n\
		movl	8(%%esi),%%ebx	\n\
		add	(%%esi),%%eax	\n\
		adc	4(%%esi),%%edx	\n\
		adc	$0,%%ebx"	\
		  : "+S" (_p), "=a" (_p[0]),"=d" (_p[1]),"=b" (_p[2])	\
		  : "1" (_a), "3" (_b)	\
		  : "cc" );})


/*
static void inline AddM(unsigned int * p, unsigned a, unsigned b)
{
	__asm__ ("mul	%%ebx		\n\
		movl	8(%%esi),%%ebx	\n\
		add	(%%esi),%%eax	\n\
		adc	4(%%esi),%%edx	\n\
		adc	$0,%%ebx"
		  : "+S" (p), "=a" (p[0]),"=d" (p[1]),"=b" (p[2])
		  : "1" (a), "3" (b)
		  : "cc" );

//: output
//: input
//: clobbered
}
*/
#else

#define LOW(a) (a&0xffff)
#define HIGH(a) (a>>16)
// *p += a*b
static void inline AddM(unsigned int * p, unsigned a, unsigned b)
{
	unsigned int alow = LOW(a);
	unsigned int ahigh = HIGH(a);
	unsigned int blow = LOW(b);
	unsigned int bhigh = HIGH(b);
	unsigned int r0,r1;
	unsigned int c0,c1;
	unsigned int t;
	r0 = alow * blow;
	r1 = ahigh * bhigh;
	c0 = alow * bhigh;
	c1 = ahigh * blow;
	t = c0 + c1;
	if (t < c0) r1 += 1<<16;	//if overflow
	r1 += HIGH(t);
	c0 = r0;
	r0 += (LOW(t) << 16);
	if (r0 < c0) r1++;		//if overflow
	c0 = p[0];
	c1 = p[1];
	c0 += r0;
	if (c0 < r0) c1++;		//if overflow
	c1 += r1;
	p[0] = c0;
	p[1] = c1;
	if (c1 < r1) p[2]++;
}
#endif
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
static void inline Negate(np* dest)
{
#if (NP_CNT==1)
	dest->n[0] = -dest->n[0];
#else
	int i;
	int c=1;
	for (i=0; i<NP_CNT; i++) {
		dest->n[i] = ~dest->n[i] + c;
		if (dest->n[i]!=0) c=0;
	}
#endif
}
static void inline Negate2(np* dest,const np* src)
{
#if (NP_CNT==1)
	dest->n[0] = -src->n[0];
#else
	int i;
	int c=1;
	for (i=0; i<NP_CNT; i++) {
		dest->n[i] = ~src->n[i] + c;
		if (dest->n[i]!=0) c=0;
	}
#endif
}
static int inline AddNpx(np* dest, const np* src)
{
#ifdef ARM_1
	register int d1;
	__asm__ ("%@ AddNpx		\n\
		adds	%0,%0,%2	\n\
		mov	%1,%0,ASR #32	\n\
		mvnvs	%1,%1"
		  : "+r" (dest->n[0]), "=r" (d1)
		  : "r" (src->n[0])
		  :  "cc" );
//: output
//: input
//: clobbered

#else
	int i,j;
	int d1=((int)(dest->n[NP_CNT-1]))>>31;	//extend sign
	int s1=((int)( src->n[NP_CNT-1]))>>31;	//extend sign
	for (i=0; i<NP_CNT; i++) {
		dest->n[i] += src->n[i];
		if (dest->n[i] < src->n[i]) {
			j=i+1;
			do {
				if (j==NP_CNT) {d1++; break;}
				dest->n[j]++;
				if (dest->n[j]!=0) break;
				j++;
			} while (1);
		}
	}
	d1+=s1;
#endif
	return d1;
}
static int inline SubNpx(np* dest, const np* src)
{
#ifdef ARM_1
	register int d1;
	__asm__ ("%@ SubNpx		\n\
		subs	%0,%0,%2	\n\
		mov	%1,%0,ASR #32	\n\
		mvnvs	%1,%1"
		  : "+r" (dest->n[0]), "=r" (d1)
		  : "r" (src->n[0])
		  : "cc" );

#else
	int i,j;
	int d1=((int)(dest->n[NP_CNT-1]))>>31;	//extend sign
	int s1=((int)( src->n[NP_CNT-1]))>>31;	//extend sign
	for (i=0; i<NP_CNT; i++) {
		if (dest->n[i] < src->n[i]) {
			j=i+1;
			do {
				if (j==NP_CNT) {d1--; break;}
				if (dest->n[j]!=0) {
					dest->n[j]--;
					break;
				}
				dest->n[j]--;
				j++;
			} while (1);
		}
		dest->n[i] -= src->n[i];
	}
	d1-=s1;
#endif
	return d1;
}
static int inline RSubNpx(np* dest, const np* src)
{
#ifdef ARM_1
	register int d1;
	__asm__ ("%@ RSubNpx		\n\
		rsbs	%0,%0,%2	\n\
		mov	%1,%0,ASR #32	\n\
		mvnvs	%1,%1"
		  : "+r" (dest->n[0]), "=r" (d1)
		  : "r" (src->n[0])
		  : "cc" );

#else
	int i,j;
	int d1=((int)(dest->n[NP_CNT-1]))>>31;	//extend sign
	int s1=((int)( src->n[NP_CNT-1]))>>31;	//extend sign
	for (i=0; i<NP_CNT; i++) {
		if (dest->n[i] > src->n[i]) {
			j=i+1;
			do {
				if (j==NP_CNT) {d1++; break;}
				dest->n[j]++;
				if (dest->n[j]!=0) break;
				j++;
			} while (1);
		}
		dest->n[i] = src->n[i] - dest->n[i];
	}
	d1 = s1 - d1;
#endif
	return d1;
}

static void CheckOverFlow(np* dest,int d1)
{
	int s1=((int)(dest->n[NP_CNT-1]))>>31;	//extend sign
	if (d1!=s1) {
		unsigned int c=d1 & (1<<31);
		unsigned int k=0;
		int i;
		printf("Overflow happened\n");
		if (c==0) {c = 0x7fffffff; k=0xffffffff;}	//max positive #
		for (i=0; i<NP_CNT-1; i++) dest->n[NP_CNT-1] = k;
		dest->n[NP_CNT-1] = c;
	}
}
//Shift right with d1
static void inline DoShift(np* dest, int d1)
{
#ifdef ARM_1
	__asm__ ("%@ DoShift		\n\
		teq	%1,%1,LSR #1	\n\
		mov	%0,%0,RRX"
		  : "+r" (dest->n[0])
		  : "r" (d1)
		  : "cc" );

#else
	int i;
	for (i=0; i<NP_CNT-1; i++) {
		dest->n[i] = (dest->n[i+1]<<31) + (dest->n[i]>>1);
	}
	dest->n[NP_CNT-1] = (d1<<31) + (dest->n[NP_CNT-1]>>1);
#endif
}
static void inline AddNp(np* dest, const np* src)
{
#ifdef ARM_1
	register int v;
	__asm__ ("%@ AddNp		\n\
		adds	%0,%0,%2	\n\
		movvs	%0,%0,ASR #32	\n\
		eorvs	%0,%0,#1<<31	\n\
		movvc	%1,#0		\n\
		movvs	%1,#1"
		  : "+r" (dest->n[0]), "=r" (v)
		  : "r" (src->n[0])
		  : "cc" );
	if (v) printf("Overflow happened\n");
#else
	int d1 = AddNpx(dest,src);
	CheckOverFlow(dest,d1);
#endif
}
static void inline Add3Np(np* dest, const np* src1, const np* src2)
{
#ifdef ARM_1
	register int v;
	__asm__ ("%@ AddNp		\n\
		adds	%0,%2,%3	\n\
		movvs	%0,%0,ASR #32	\n\
		eorvs	%0,%0,#1<<31	\n\
		movvc	%1,#0		\n\
		movvs	%1,#1"
		  : "=r" (dest->n[0]), "=r" (v)
		  : "r" (src1->n[0]),"r" (src2->n[0])
		  : "cc" );
	if (v) printf("Overflow happened\n");
#else
	memcpy(dest,src1,sizeof(*dest));
	AddNp(dest,src2);
#endif
}
static void inline SubNp(np* dest, const np* src)
{
#ifdef ARM_1
	register int v;
	__asm__ ("%@ SubNp		\n\
		subs	%0,%0,%2	\n\
		movvs	%0,%0,ASR #32	\n\
		eorvs	%0,%0,#1<<31	\n\
		movvc	%1,#0		\n\
		movvs	%1,#1"
		  : "+r" (dest->n[0]), "=r" (v)
		  : "r" (src->n[0])
		  : "cc" );
	if (v) printf("Overflow happened\n");
#else
	int d1 = SubNpx(dest,src);
	CheckOverFlow(dest,d1);
#endif
}
static void inline RSubNp(np* dest, const np* src)
{
#ifdef ARM_1
	register int v;
	__asm__ ("%@ RSubNp		\n\
		rsbs	%0,%0,%2	\n\
		movvs	%0,%0,ASR #32	\n\
		eorvs	%0,%0,#1<<31	\n\
		movvc	%1,#0		\n\
		movvs	%1,#1"
		  : "+r" (dest->n[0]), "=r" (v)
		  : "r" (src->n[0])
		  : "cc" );
	if (v) printf("Overflow happened\n");
#else
	int d1 = RSubNpx(dest,src);
	CheckOverFlow(dest,d1);
#endif
}
static void inline AddNpAverage(np* dest, const np* src)
{
#ifdef ARM_1
	__asm__ ("%@ AddNpAverage	\n\
		adds	%0,%0,%1	\n\
		mov	%0,%0,ASR #1	\n\
		eorvs	%0,%0,#1<<31"
		  : "+r" (dest->n[0])
		  : "r" (src->n[0])
		  : "cc" );
#else
	int d1 = AddNpx(dest,src);
	DoShift(dest,d1);
#endif
}
static void inline SubNpAverage(np* dest, const np* src)
{
#ifdef ARM_1
	__asm__ ("%@ SubNpAverage	\n\
		subs	%0,%0,%1	\n\
		mov	%0,%0,ASR #1	\n\
		eorvs	%0,%0,#1<<31"
		  : "+r" (dest->n[0])
		  : "r" (src->n[0])
		  : "cc" );
#else
	int d1 = SubNpx(dest,src);
	DoShift(dest,d1);
#endif
}
static void inline RSubNpAverage(np* dest, const np* src)
{
#ifdef ARM_1
	__asm__ ("%@ RSubNpAverage	\n\
		rsbs	%0,%0,%1	\n\
		mov	%0,%0,ASR #1	\n\
		eorvs	%0,%0,#1<<31"
		  : "+r" (dest->n[0])
		  : "r" (src->n[0])
		  : "cc" );
#else
	int d1 = RSubNpx(dest,src);
	DoShift(dest,d1);
#endif
}
static void inline Add3NpAverage(np* dest, const np* src1, const np* src2)
{
#ifdef ARM_1
	__asm__ ("%@ Add3NpAverage	\n\
		adds	%0,%1,%2	\n\
		mov	%0,%0,ASR #1	\n\
		eorvs	%0,%0,#1<<31"
		  : "=r" (dest->n[0])
		  : "r" (src1->n[0]), "r" (src2->n[0])
		  : "cc" );
#else
	memcpy(dest,src1,sizeof(*dest));
	AddNpAverage(dest, src2);
#endif
}
static void inline Sub3NpAverage(np* dest, const np* src1, const np* src2)
{
#ifdef ARM_1
	__asm__ ("%@ Sub3NpAverage	\n\
		subs	%0,%1,%2	\n\
		mov	%0,%0,ASR #1	\n\
		eorvs	%0,%0,#1<<31"
		  : "=r" (dest->n[0])
		  : "r" (src1->n[0]), "r" (src2->n[0])
		  : "cc" );
#else
	memcpy(dest,src1,sizeof(*dest));
	SubNpAverage(dest, src2);
#endif
}

static void inline ZeroNpd(npd* dest)
{
#if (NP_CNT==1)
	dest->n[0] = 0;
	dest->n[1] = 0;
#else
	memset(dest,0,sizeof(*dest));
#endif
}

//dest is unsigned, src is unsigned
static void inline MultNp(npd* dest,const np* a, const np* b)
{
#ifdef ARM_1
	__asm__ ("%@ MultSigned		\n\
		umull	%0,%1,%2,%0"
		  : "=&r" (dest->n[0]), "=&r" (dest->n[1])
		  : "r" (b->n[0]), "0" (a->n[0])
		  );
#else
	int i,j;
	ZeroNpd(dest);
	for (i=0; i<NPD_CNT-1; i++) {
		int max = (i<=(NP_CNT-1))? i : NP_CNT-1;
		for (j=i-max; j<=max; j++) {
			AddM(&dest->n[i],a->n[j],b->n[i-j]);
//			printf("%i = %i + %i\n",i,j,i-j);
		}
	}
#if 0
	PrintNpd(dest,0);
	printf(" = ");
	PrintNpData(a,0);
	printf(" * ");
	PrintNpData(b,0);
	printf("\n");
#endif
#endif
}
//dest is signed, src is unsigned
static void inline MultSigned(np* dest,const np* src)
{
#ifdef ARM_1
	register int temp;
	__asm__ ("%@ MultSigned		\n\
		teq	%0,%0,LSL #1	\n\
		rsbcs	%0,%0,#0	\n\
		umull	%1,%0,%2,%0	\n\
		rsbcs	%0,%0,#0"
		  : "+r" (dest->n[0]), "=&r" (temp)
		  : "r" (src->n[0])
		  : "cc" );
#else
	int sign=0;
	npd temp;
	if ( ((int)(dest->n[NP_CNT-1]))<0) {
		Negate(dest);
		sign = 1;
	}
	MultNp(&temp,dest,src);
	memcpy(dest,&temp.n[NP_CNT],sizeof(*dest));
	if (sign) Negate(dest);
#endif
}

////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////


static void inline AddNPD_P1(npd_p1* dest, const npd* src)
{
#if (NP_CNT==1)
#ifdef ARM_1
	__asm__ ("%@ AddNPD_P1		\n\
		ldr	%2,[%0]		\n\
		ldr	%3,[%1]		\n\
		ldr	%4,[%1,#4]	\n\
		adds	%2,%2,%3	\n\
		ldr	%3,[%0,#4]	\n\
		adcs	%3,%3,%4	\n\
		ldr	%4,[%0,#8]	\n\
		addcs	%4,%4,#1"
		  : "+r" (dest), "+r" (src), "=&r" (dest->n[0]),"=&r" (dest->n[1]),"=r" (dest->n[2])
		  :
		  : "cc" );
//: output
//: input
//: clobbered

#else
	dest->n[0] += src->n[0];
	if (dest->n[0] < src->n[0]) {
		dest->n[1]++;
		if (dest->n[1]==0) dest->n[2]++;
	}
	dest->n[1] += src->n[1];
	if (dest->n[1] < src->n[1]) dest->n[2]++;
#endif
#else
	int i,j;
	for (i=0; i<NPD_CNT; i++) {
		dest->n[i] += src->n[i];
		if (dest->n[i] < src->n[i]) {
			j=i+1;
			do {
				if (j>NPD_CNT) {break;}
				dest->n[j]++;
				if (dest->n[j]!=0) break;
				j++;
			} while (1);
		}
	}
#endif
}

static int inline ShiftLeft(npd* dest,int c)
{
#ifdef ARM_1
	__asm__ ("%@ ShiftLeft thru carry \n\
		ldr	%2,[%0]		\n\
		ldr	%3,[%0,#4]	\n\
		movs	%1,%1,LSR #1	\n\
		adcs	%2,%2,%2	\n\
		adcs	%3,%3,%3	\n\
		movcs	%1,#1"
		  : "+r" (dest), "+r" (c), "=&r" (dest->n[0]),"=&r" (dest->n[1])
		  :
		  : "cc" );
#else
	int i;
	for (i=0; i<NPD_CNT; i++) {
		unsigned int val1 = dest->n[i];
		unsigned int val2 = (val1<<1) + c;
		c = val1>>31;
		dest->n[i] = val2;
	}
#endif
	return c;
}
static void inline ShiftRight(npd* dest)
{
#ifdef ARM_1
	__asm__ ("%@ ShiftRight		\n\
		ldr	%1,[%0]		\n\
		ldr	%2,[%0,#4]	\n\
		mov	%2,%2,LSR #1	\n\
		mov	%1,%1,RRX"
		  : "+r" (dest), "=&r" (dest->n[0]),"=&r" (dest->n[1])
		  :
		  : "cc" );
#else
	int i;
	for (i=0; i<NPD_CNT-1; i++) {
		dest->n[i] = (dest->n[i] >> 1) + (dest->n[i+1]<<31);
	}
	dest->n[NPD_CNT-1] = (dest->n[NPD_CNT-1] >> 1);
#endif
}
static void inline ShiftRightCnt(npd* dest,int cnt)
{
#ifdef ARM_1
	__asm__ ("%@ ShiftRightCnt	\n\
		ldr	%1,[%0]		\n\
		ldr	%2,[%0,#4]	\n\
		mov	%1,%1,LSR %3	\n\
		add	%1,%1,%2,LSL %4	\n\
		mov	%1,%1,LSL %3"
		  : "+r" (dest), "=&r" (dest->n[0]),"=&r" (dest->n[1])
		  : "r" (cnt), "r" (32-cnt)
		  : "cc" );
#else
	int i;
	int j=cnt>>5;
	const int k=cnt&31;
	if (k) {
		const int l=32-k;
		for (i=0; j<NPD_CNT-1; i++,j++) {
			dest->n[i] = (dest->n[j] >> k) + (dest->n[j+1]<<l);
		}
		dest->n[i] = (dest->n[j] >> k);
		i++;
	} else {
		for (i=0; j<NPD_CNT; i++,j++) {
			dest->n[i] = dest->n[j];
		}
	}
	while (i<NPD_CNT) {dest->n[i] = 0; i++;}
#endif
}
static int inline CmpNpd(const npd* low,const npd* high)
{
#ifdef ARM_1
	register int result;
	__asm__ ("%@ cmpNpd		\n\
		cmp	%2,%3		\n\
		ldreq	%2,[%0]		\n\
		ldreq	%3,[%1]		\n\
		cmpeq	%2,%3		\n\
		moveq	%2,#0		\n\
		movlo	%2,#-1		\n\
		movhi	%2,#1"
		  : "+r" (low), "+r" (high), "=r" (result)
		  : "r" (high->n[1]), "2" (low->n[1])
		  : "cc" );
	return result;
#else
	int i;
	for (i=NPD_CNT-1; i>=0; i--) {
		if (low->n[i] < high->n[i]) return -1;
		if (low->n[i] > high->n[i]) return 1;
	}
	return 0;
#endif
}
static int inline CmpNPD_P1(const npd_p1* low,const npd_p1* high)
{
#ifdef ARM_1
	register int result;
	__asm__ ("%@ cmpNPD_P1		\n\
		cmp	%2,%3		\n\
		ldreq	%2,[%0,#4]	\n\
		ldreq	%3,[%1,#4]	\n\
		cmpeq	%2,%3		\n\
		ldreq	%2,[%0]		\n\
		ldreq	%3,[%1]		\n\
		cmpeq	%2,%3		\n\
		moveq	%2,#0		\n\
		movlo	%2,#-1		\n\
		movhi	%2,#1"
		  : "+r" (low), "+r" (high), "=r" (result)
		  : "r" (high->n[2]), "2" (low->n[2])
		  : "cc" );
	return result;
#else
	int i;
	for (i=NPD_P1_CNT-1; i>=0; i--) {
		if (low->n[i] < high->n[i]) return -1;
		if (low->n[i] > high->n[i]) return 1;
	}
	return 0;
#endif
}
static void inline IncNpd(npd* dest)
{
#ifdef ARM_1
	__asm__ ("%@ IncNpd		\n\
		adds	%0,%0,#1	\n\
		addcss	%1,%1,#1	\n\
		mvncs	%0,#0		\n\
		mvncs	%1,#0"
		  : "+r" (dest->n[0]),"+r" (dest->n[1])
		  :
		  : "cc" );
#else
	int i;
	for (i=0; i<NPD_CNT; i++) {
		dest->n[i]++;
		if (dest->n[i] != 0 ) return;
	}
	//overflow, reset to max
	for (i=0; i<NPD_CNT; i++) {
		dest->n[i] = 0xffffffff;
	}
#endif
}
static void AddNpd(npd* dest, const npd* src)
{
#ifdef ARM_1
	__asm__ ("%@ AddNpd		\n\
		adds	%0,%0,%2	\n\
		adc	%1,%1,%3"
		  : "+r" (dest->n[0]),"+r" (dest->n[1])
		  : "r" (src->n[0]),"r" (src->n[1])
		  : "cc" );
#else
	int i,j;
	for (i=0; i<NPD_CNT; i++) {
		dest->n[i] += src->n[i];
		if (dest->n[i] < src->n[i]) {
			j=i+1;
			do {
				if (j==NPD_CNT) {break;}
				dest->n[j]++;
				if (dest->n[j]!=0) break;
				j++;
			} while (1);
		}
	}
#endif
}

static void inline SubNpd(npd* dest,const npd* src)
{
#ifdef ARM_1
	__asm__ ("%@ SubNpd		\n\
		subs	%0,%0,%2	\n\
		sbc	%1,%1,%3"
		  : "+r" (dest->n[0]),"+r" (dest->n[1])
		  : "r" (src->n[0]),"r" (src->n[1])
		  : "cc" );
#else
	int i,j;
	for (i=0; i<NPD_CNT; i++) {
		if (dest->n[i] < src->n[i]) {
			j=i+1;
			do {
				if (j==NPD_CNT) {break;}
				if (dest->n[j]!=0) {
					dest->n[j]--;
					break;
				}
				dest->n[j]--;
				j++;
			} while (1);
		}
		dest->n[i] -= src->n[i];
	}
#endif
}
//Assumed!!!!!
//*a < *b
static void inline DivNpd(npd* dest,const npd* a,const npd* b)
{
#ifdef ARM_1
	__asm__ ("%@ DivNpd		\n\
	L1:	adds	%2,%2,%2	\n\
		adcs	%3,%3,%3	\n\
		bcs	L2		\n\
		cmp	%3,%5		\n\
		cmpne	%2,%4		\n\
		blo	L3		\n\
	L2:	subs	%2,%2,%4	\n\
		sbc	%3,%3,%5	\n\
		teq	%6,%6,LSR #1	\n\
	L3:	adcs	%0,%0,%0	\n\
		adc	%1,%1,%1	\n\
		subs	%6,%6,#2	\n\
		bpl	L1"
		  : "=&r" (dest->n[0]), "=&r" (dest->n[1])
		  : "r"  (a->n[0]), "r" (a->n[1]), "r"  (b->n[0]), "r" (b->n[1]), "r" (63*2+1)
		  : "cc" );
#else
	int i;
	int c;
	npd temp = *a;
	for (i=0; i<(NPD_CNT<<5); i++) {
		c = ShiftLeft(&temp,0);
		if (c==0) if (CmpNpd(&temp,b) >=0 ) c=1;
		if (c==1) SubNpd(&temp,b);
		ShiftLeft(dest,c);
	}
#endif
}
static void inline DivNpd_p1(npd* dest,const npd_p1* src,const int divisor)
{
	int i,j;
	int c;
	const unsigned int* s = &src->n[NPD_P1_CNT-1];
	unsigned int a= *s; s--;
#if 0
	printf("{");
	for (i=0; i<NPD_P1_CNT-1; i++) printf("0x%08x,",src->n[i]);
	printf("0x%08x}/ %i = ",src->n[i],divisor);
#endif
	if (a<divisor) {
		for (j=0; j<NPD_CNT; j++) {
			unsigned int temp = *s; s--;
			for (i=0; i<32; i++) {
				c = a>>31;
				a = (a<<1) + (temp>>31); temp<<=1;
				if (c==0) if (a >= divisor) c=1;
				if (c==1) a -= divisor;
				ShiftLeft(dest,c);
			}
		}
	} else {
		for (j=0; j<NPD_CNT; j++) dest->n[j]=0xffffffff;
	}
#if 0
	printf("{");
	for (i=0; i<NPD_CNT-1; i++) printf("0x%08x,",dest->n[i]);
	printf("0x%08x}\r\n",dest->n[i]);
#endif
}

static void inline SetNpd(npd* dest,unsigned int val,const int logN)
{
	int i;
#define SHCNT ((AFTER_FFT_SHIFT+NP_DATA_UNIT)<<1)
	for (i=0; i<NPD_CNT; i++) dest->n[i] = 0;
#if (SHCNT&31) != 0
	dest->n[NPD_CNT - ((SHCNT+31)>>5)] = val<<((32-(SHCNT&31))&31);
	dest->n[NPD_CNT - (SHCNT>>5)] = val>>(SHCNT&31));
#else
	dest->n[NPD_CNT - (SHCNT>>5)] = val;
#endif
}
static int inline SubOrZero(npd* dest,const npd* src)
{
#ifdef ARM_1
	register int result;
	__asm__ ("%@ SubOrZero		\n\
		subs	%0,%0,%3	\n\
		sbcs	%1,%1,%4	\n\
		movlo	%2,#0		\n\
		movhs	%2,#1"
		  : "+r" (dest->n[0]),"+r" (dest->n[1]), "=r" (result)
		  : "r" (src->n[0]),"r" (src->n[1])
		  : "cc" );
	return result;
#else
	int i,j;
	for (i=0; i<NPD_CNT; i++) {
		if (dest->n[i] < src->n[i]) {
			j=i+1;
			do {
				if (j==NPD_CNT) { return 0;}
				if (dest->n[j]!=0) {
					dest->n[j]--;
					break;
				}
				dest->n[j]--;
				j++;
			} while (1);
		}
		dest->n[i] -= src->n[i];
	}
	return 1;
#endif
}
static inline int Cntbits(const npd* src)
{
	int i=NPD_CNT-1;
	unsigned int a;
	do {
		a = src->n[i];
		if (a) {
			int cnt = i*32;
			do {
				cnt++;
				a>>=1;
			} while (a);
			return cnt;
		}
		i--;
	} while (i>=0);
	return 0;
}


static void Sqroot(np* dest,const npd* src)
{
#ifdef ARM_1
	register int b2Shift=0;
	register int mask=1;
	register int temp=2;	//0 would be better, but then input registers are shared
//	printf("src: 0x%08x %08x\r\n",src->n[1],src->n[0]);
	__asm__ ("%@ Sqroot		\n\
		movs	%0,%2		\n\
		movne	%3,#32		\n\
		moveqs	%0,%1		\n\
		beq	10f		\n\
		cmp	%0,#0x10000	\n\
		addhs	%3,%3,#16	\n\
		movhs	%0,%0,LSR #16	\n\
		cmp	%0,#0x100	\n\
		addhs	%3,%3,#8	\n\
		movhs	%0,%0,LSR #8	\n\
		cmp	%0,#0x10	\n\
		addhs	%3,%3,#4	\n\
		movhs	%0,%0,LSR #4	\n\
		cmp	%0,#0x4		\n\
		addhs	%3,%3,#2	\n\
		movhs	%0,%0,LSR #2	\n\
		cmp	%0,#0x2		\n\
		addhs	%3,%3,#1	\n\
		movhs	%0,%0,LSR #1	\n\
		movs	%0,%0,LSR #1	\n\
		addcs	%3,%3,#1	\n\
		sub	%3,%3,#1	\n\
		bic	%3,%3,#1	\n\
		and	%0,%3,#31	\n\
		mov	%4,%4,LSL %0	\n\
		mov	%0,#0		\n\
		mov	%5,#0		\n\
	1:	tst	%3,#1<<5	\n\
		orreq	%0,%0,%4	\n\
		orrne	%5,%5,%4	\n\
		cmp	%2,%5		\n\
		cmpeq	%1,%0		\n\
		beq	9f		\n\
		blo	2f		\n\
		subs	%1,%1,%0	\n\
		sbc	%2,%2,%5	\n\
		tst	%3,#32		\n\
		addeq	%0,%0,%4	\n\
		addne	%5,%5,%4	\n\
		b	3f		\n\
	2:	tst	%3,#32		\n\
		biceq	%0,%0,%4	\n\
		bicne	%5,%5,%4	\n\
	3:	movs	%5,%5,LSR #1	\n\
		mov	%0,%0,RRX	\n\
		mov	%4,%4,ROR #2	\n\
		subs	%3,%3,#2	\n\
		bpl	1b		\n\
		cmp	%2,%5		\n\
		cmpeq	%1,%0		\n\
		addhis	%0,%0,#1	\n\
		mvncs	%0,#0		\n\
		b	10f		\n\
	9:	tst	%3,#1<<5	\n\
		addeq	%0,%0,%4	\n\
		addne	%5,%5,%4	\n\
		mov	%1,%3,LSR #1	\n\
		add	%1,%1,#1	\n\
		mov	%0,%0,LSR %1	\n\
		rsb	%1,%1,#32	\n\
		add	%0,%0,%5,LSL %1	\n\
	10:"
		  : "=&r" (dest->n[0])
		  : "r" (src->n[0]), "r" (src->n[1]), "r" (b2Shift), "r" (mask), "r" (temp)
		  : "cc" );
//	printf("src: %u  dest: %u\r\n",src->n[0],dest->n[0]);
#else
	int i;
	npd b_2a;	//%0, %5
	npd rem;	//%1, %2
	int b2Shift=Cntbits(src);	//%3
//printf("x2:%8x %8x %8x %8x %8x %8x\n",src->n[5],src->n[4],src->n[3],src->n[2],src->n[1],src->n[0]);
	if (b2Shift<=0) {
		memset(dest,0,sizeof(*dest));
		return;
	}
	rem = *src;
	memset(&b_2a,0,sizeof(b_2a));
	b2Shift = (b2Shift-1) & ~1;
	do {
		b_2a.n[b2Shift>>5] += 1<<(b2Shift&31);
		i = CmpNpd(&rem,&b_2a);
		if (i>=0) {
			SubNpd(&rem,&b_2a);
			b_2a.n[b2Shift>>5] += 1<<(b2Shift&31);
			if (i==0) {
				ShiftRightCnt(&b_2a,(b2Shift>>1)+1);
				memcpy(dest,&b_2a.n[0],sizeof(*dest));
				return;
			}
		} else {
			b_2a.n[b2Shift>>5] -= 1<<(b2Shift&31);
		}
		b2Shift -= 2;
		ShiftRight(&b_2a);
	} while (b2Shift>=0);

	if (CmpNpd(&rem,&b_2a)>0) IncNpd(&b_2a);
	memcpy(dest,&b_2a.n[0],sizeof(*dest));
#if 0
	printf("sqroot(");
	PrintNpd(src);
	printf(") = ");
	PrintNp(dest);
	printf("\n");
#endif
#endif
}
/////////////////////////////////////////////////////////////
//unused functions below
static void inline Power2(np* dest,int power)
{
	int i=0;
	while (power>=32) {
		dest->n[i++] = 0;
		power -= 32;
	}
	if (i<NP_CNT) {
		dest->n[i++] = 1<<power;
		while (i<NP_CNT) {
			dest->n[i++] = 0;
		}
	} else {
		//shouldn't happen, but to be safe
		i=0;
		while (i<NP_CNT) {
			dest->n[i++]=0xffffffff;
		}
	}
}
