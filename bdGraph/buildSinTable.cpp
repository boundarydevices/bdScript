#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PRINT_DIFF
#ifdef PRINT_DIFF
#include <math.h>
#endif

#define OUTPUT_NP_CNT 1
/*
cos(a)**2 = (1+cos(2a))/2

cos((a+b)/2) ** 2 = (1+cos(a+b))/2
cos((a+b)/2) ** 2 = (1+(cos(a)cos(b) - sin(a)sin(b)))/2
sin((a+b)/2) ** 2 = 1- cos((a+b)/2)**2
*/
#define _logN 8
#define N (1<<_logN)

#define NP_CNT 3
#include "fft.h"

typedef struct RANGE {
	unsigned short low;
	unsigned short high;
} range;

#if 1
#define NP_UNIT 0	//# of bits to shift
#else
#define NP_UNIT 1	//# of bits to shift
#endif


void ZeroNp(np* dest)
{
	memset(dest,0,sizeof(*dest));
}
void OneNp(np* dest)
{
#if NP_UNIT>=1
	memset(dest,0,sizeof(*dest)-sizeof(unsigned int));
	dest->n[NP_CNT-1] = ((1UL<<31)>>NP_UNIT)*2;	//1
#else
	memset(dest,0,sizeof(*dest));
#endif
}
void OneNpd(npd* dest)
{
#if NP_UNIT>=1
	memset(dest,0,sizeof(*dest)-sizeof(unsigned int));
	dest->n[NPD_CNT-1] = ((1UL<<31)>>(NP_UNIT+NP_UNIT))*2;	//1
#else
	memset(dest,0,sizeof(*dest));
#endif
}
void HalfNpd(npd* dest)
{
	memset(dest,0,sizeof(*dest)-sizeof(unsigned int));
	dest->n[NPD_CNT-1] = (1UL<<31)>>(NP_UNIT+NP_UNIT);	//1
}

void PrintNpd(npd* src)
{
	int i;
	unsigned int dest[NPD_CNT+1];
	memset(&dest,0,sizeof(dest));
	for (i=0; i<NPD_CNT; i++) {
		AddM(&dest[i],src->n[i],100000000<<(NP_UNIT+NP_UNIT));
	}
	for (i=NPD_CNT-1; i>=0; i--) printf("%8x ",src->n[i]);
	printf("(%i.%08i)",dest[NPD_CNT]/100000000,dest[NPD_CNT]%100000000);
}
void PrintNp(np* src)
{
	int i;
	unsigned int dest[NP_CNT+1];
	memset(&dest,0,sizeof(dest));
	for (i=0; i<NP_CNT; i++) {
		AddM(&dest[i],src->n[i],100000000<<NP_UNIT);
	}
	for (i=NP_CNT-1; i>=0; i--) printf("%8x ",src->n[i]);
	printf("(%i.%08i)",dest[NP_CNT]/100000000,dest[NP_CNT]%100000000);
}
void MultNp(npd* dest,np* a, np* b)
{
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
	PrintNpd(dest);
	printf(" = ");
	PrintNp(a);
	printf(" * ");
	PrintNp(b);
	printf("\n");
#endif
}
void AddNpd(npd* dest, npd* src)
{
	int i,j;
	for (i=0; i<NPD_CNT; i++) {
		dest->n[i] += src->n[i];
		if (dest->n[i] < src->n[i]) {
			for (j=i+1; j<NPD_CNT; j++) {
				dest->n[j]++;
				if (dest->n[j]!=0) break;
			}
		}
	}
}
void SubNpd(npd* dest, npd* src)
{
	int i,j;
	for (i=0; i<NPD_CNT; i++) {
		if (dest->n[i] < src->n[i]) {
			for (j=i+1; j<NPD_CNT; j++) {
				if (dest->n[j]!=0) {
					dest->n[j]--;
					break;
				}
				dest->n[j]--;
			}
		}
		dest->n[i] -= src->n[i];
	}
}
int Cntbits(npd* src)
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
void AverageNp(np* dest, np* src1, np* src2)
{
	unsigned int* p = &dest->n[0];
	unsigned int* pEnd = &dest->n[NP_CNT-1];
	int i,j;
	unsigned int c=0;
	np a;
	memcpy(&a,src1,sizeof(a));
	for (i=0; i<NP_CNT; i++) {
		dest->n[i] = a.n[i] + src2->n[i];
		if (dest->n[i] < a.n[i]) {
			for (j=i+1; j<NP_CNT; j++) {
				a.n[j]++;
				if (a.n[j]!=0) break;
			}
			if (j==NP_CNT) c++;
		}
	}
	while (p<pEnd) {
		*p = (p[0]>>1) + (p[1]<<31);
		p++;
	}
	*p = (p[0]>>1) + (c<<31);
}
void AverageNpd(npd* dest,npd* src,unsigned int c)
{
	unsigned int* p = &dest->n[0];
	unsigned int* pEnd = &dest->n[NPD_CNT-1];
//	PrintNpd(dest);
//	printf(" / 2 = ");
	int i,j;

	for (i=0; i<NPD_CNT; i++) {
		dest->n[i] += src->n[i];
		if (dest->n[i] < src->n[i]) {
			for (j=i+1; j<NPD_CNT; j++) {
				dest->n[j]++;
				if (dest->n[j]!=0) break;
			}
			if (j==NPD_CNT) c++;
		}
	}
	while (p<pEnd) {
		*p = (p[0]>>1) + (p[1]<<31);
		p++;
	}
	*p = (p[0]>>1) + (c<<31);
//	PrintNpd(dest);
//	printf("\n");
}
void ComputeSinTable(np* sinTable)
{
	npd sin2;
	range saved[_logN];
	int savedR = 0;
	int a=0;
	int b=N;
#if NP_UNIT>=1
	np one;
	OneNp(&one);
#endif
	ZeroNp(&sinTable[0]);
	
	printf("N:%i\n",b);
	b = (a+b)/2;
	HalfNpd(&sin2);
	Sqroot(&sinTable[b],&sin2);	//entry N/2 is sqroot(1/2)
#if 0
	printf("sqroot( = ");
	PrintNpd(&sin2);
	printf(") = ");
	PrintNp(&sinTable[b]);
	printf("\n");
#endif	
	saved[savedR].low = a;
	saved[savedR].high = b;
	savedR++;
	while (savedR) {
		savedR--;
		a = saved[savedR].low;
		b = saved[savedR].high;
//		printf("index:%i\n",savedR);
		while ((a+1)<b) {
			int i=(a+b)/2;
			unsigned int c=0;
			npd cos2;
			npd temp;
			OneNpd(&sin2);
			OneNpd(&cos2);
			if (a) {
				MultNp(&temp,&sinTable[a],&sinTable[b]);	//sin(a)*sin(b)
				SubNpd(&cos2,&temp);
				
				MultNp(&temp,&sinTable[N-a],&sinTable[N-b]);	//cos(a)*cos(b)
			} else {
#if NP_UNIT>=1
				MultNp(&temp,&one,&sinTable[N-b]);	//cos(a)*cos(b)
#else
				memset(&temp.n[0],0,sizeof(np));
				memcpy(&temp.n[NP_CNT],&sinTable[N-b],sizeof(np));
				c=1;
#endif
			}
#if 0
if (i==64) {
	printf("Average(");
	PrintNpd(&cos2);
	printf(", ");
	PrintNpd(&temp);
	printf(") = ");
			AverageNpd(&cos2,&temp,c);
	
	PrintNpd(&cos2);
	printf("\n");
}
else AverageNpd(&cos2,&temp,c);
#else	
			AverageNpd(&cos2,&temp,c);
#endif
			SubNpd(&sin2,&cos2);
			
			Sqroot(&sinTable[N-i],&cos2);
			Sqroot(&sinTable[i],&sin2);
#if 0
	printf("cos(%i) = ",a);
	if (a) {
		PrintNp(&sinTable[N-a]);
	} else {
		printf("1.0");
	}
	printf("\n");
	printf("sin(%i) = ",a);
	PrintNp(&sinTable[a]);
	printf("\n");

	printf("cos(%i) = ",b);
	PrintNp(&sinTable[N-b]);
	printf("\n");
	printf("sin(%i) = ",b);
	PrintNp(&sinTable[b]);
	printf("\n");

	printf("cos(%i) = ",i);
	PrintNp(&sinTable[N-i]);
	printf("\n");
	printf("sin(%i) = ",i);
	PrintNp(&sinTable[i]);
	printf("\n");
#endif
			saved[savedR].low = i;
			saved[savedR].high = b;
			savedR++;
			b = i;
		}
	}
}
static inline void Round(unsigned int * v,int cnt)
{
	int i;
	if (v[0]>>31) {
		for (i=1; i<cnt; i++) {
			if (v[i]!=0xffffffff) break;
		}
		if (i<cnt) {
			//round up if overflow won't happen
			for (i=1; i<cnt; i++) {
				v[i]++;
				if (v[i]!=0) break;
			}
		}
	}
}

static void CreateFile(np* sinTable,int reversed,char* prefix,char* suffix,int logTable,int squared)
{
	int i,j,k,l;
	FILE* f;
	char fileName[64];
	int cnt=1<<logTable;
#define NP_DIFF (NP_CNT - (OUTPUT_NP_CNT+1))
#define NPD_DIFF (NPD_CNT - (OUTPUT_NP_CNT+1))
	const int startk  = ((NP_DIFF>=0) ?  NP_DIFF : 0);
	const int startl  = ((NP_DIFF>=0) ? 0 : -NP_DIFF);

	const int startk2 = ((NPD_DIFF>=0) ?  NPD_DIFF : 0);
	const int startl2 = ((NPD_DIFF>=0) ? 0 : -NPD_DIFF);
	const int advance = 1<<(_logN-logTable);
	const int revAdvance = 1<<_logN;
	if (reversed) cnt<<=1;

	sprintf(fileName,"%sTable%i%s.c",prefix,logTable,suffix);
	f = fopen(fileName,"w");
	fprintf(f,"const np %sTable%i%s[%i] = {",prefix,logTable,suffix,cnt);

	for (i=0,j=0; j<cnt; j++,i = (reversed)?IncReversed(i,revAdvance) : i+advance) {
		unsigned int v[OUTPUT_NP_CNT+1];
		np* a = &sinTable[i];
		memset(v,0,(OUTPUT_NP_CNT+1)*sizeof(v[0]));
		if (squared) {
			npd temp;
			MultNp(&temp,a,a);
			for (k=startk2,l=startl2; k<NPD_CNT; k++,l++) v[l] = temp.n[k];
		} else {
			for (k=startk,l=startl; k<NP_CNT; k++,l++) v[l] = a->n[k];
		}
		if ((j & 3)==0) fprintf(f,"\n");

		Round(v,OUTPUT_NP_CNT+1);
		fprintf(f,"{");
		for (k=1; k<OUTPUT_NP_CNT; k++) fprintf(f,"0x%08x,",v[k]);
		fprintf(f,"0x%08x}",v[OUTPUT_NP_CNT]);
		fprintf(f,(j!=(cnt-1)) ? "," : "};");
//		printf("j:%i, cnt:%i\r\n",j,cnt);
	}
	fprintf(f,"\n");
	fclose(f);
}
#ifdef PRINT_DIFF
static inline void PrintDiff(np* sinTable)
{
	int i;
	np t10E8;
	np unit10e8;
	const double c32 = (double(1) * (1<<16)) * (1<<16);
	double units = double(1<<NP_UNIT);
	for (i=0; i<NP_CNT; i++) units /= c32;
	ZeroNp(&t10E8);
	t10E8.n[0] = 100000000;
	ZeroNp(&unit10e8);
	unit10e8.n[0] = 100000000<<NP_UNIT;

	for (i=0; i<N; i++) {
		double x = (i*M_PI_2)/N;
		double sinx = sin (x);
		double mysinx=0;
		int j;
		for (j=NP_CNT-1; j>=0; j--) {
			mysinx = (mysinx * c32) + sinTable[i].n[j];
		}
		mysinx = mysinx * units;
		if (mysinx != sinx) {
			npd temp;
			np t;
			unsigned int val1,val2;

//			printf("%23.20e %23.20e ",mysinx,sinx);
			if (mysinx == sinx) printf("now equals ");
//			mysinx -= sinx;
//			printf("%15.11e ",mysinx);


			MultNp(&temp,&sinTable[i],&unit10e8);
			val1 = temp.n[NP_CNT];
			memcpy(&t,&temp,sizeof(t));
			MultNp(&temp,&t,&t10E8);
			val2 = temp.n[NP_CNT];
			mysinx -= sinx;
			mysinx *= c32;
			mysinx *= c32;
			printf("%i : %i.%08i%08i vs %11.8g diff *(2<<64): %i\n",i,
				val1/100000000,val1%100000000,val2,sinx,int(mysinx));
		}
	}
}
#endif
int main()
{
	int i,j;
	np sinTable[N<<1];
//	np sinTable[N];
	ComputeSinTable(sinTable);
	for (i=0; i<NP_CNT; i++) sinTable[N].n[i] = 0xffffffff;
	for (i=N+1,j=N-1; i<(N<<1); i++,j--) memcpy(&sinTable[i],&sinTable[j],sizeof(sinTable[i]));
#ifdef PRINT_DIFF
	PrintDiff(sinTable);
#endif
	CreateFile(sinTable,0,"sin","",_logN,0);
	CreateFile(sinTable,1,"sin","r",_logN,0);
	CreateFile(sinTable,0,"sin2","",_logN,1);
}
