#include <stdio.h>
#include <stdlib.h>
#include "fft.h"
void test1(void)
{
	int i,j;
	const int logN=10;	//1024 points
	const int n=(1<<logN);
	cmplx vect[n];
	short samples[n];
	short* pSamples;

	memset(vect,0,n*sizeof(cmplx));
	for (i=0; i<n; i++) {samples[i] = i;}
	fft_forward(vect,samples,logN);

#if 1
	PrintTable(vect,logN,AFTER_FFT_SHIFT,1);
#endif
	fft_inverse(samples,vect,logN);
	PrintTable(vect,logN,FINAL_SHIFT,0);
	pSamples = samples;
	for (i=0; i<n; i++) {
		if ((i&7)==0) printf("\r\n");
		printf("%5i",*pSamples);
		pSamples++;
	}
	printf("\n");
}


int main(void)
{
	test1();
}
