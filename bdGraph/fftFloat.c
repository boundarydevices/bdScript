#include <math.h>
typedef struct CMPLX                      /* complex pair */
{
	float	r;                      /* real part */
	float	i;                      /* imaginary part */
} cmplx;

#define logN 10
#define N (1<<logN)

void fft(cmplx * vect)
{
	const double pi = M_PI;
	int j = (N>>1);
	int i,k;
	int advance;
	for (i = 1; i<N-1; i++) {
		if (i<j) {
			double tr = vect[j].r;
			double ti = vect[j].i;
			vect[j].r = vect[i].r;
			vect[j].i = vect[i].i;
			vect[i].r = tr;
			vect[i].i = ti;
		}
		k = (N>>1);
		while (k<=j) {
			j -= k;
			k >>= 1;
		}
		j += k;
	}

	for (advance=1; advance<N; advance<<=1) {
		double cosa = 1.0;
		double sina = 0;
		double cosb = cos(pi/advance);
		double sinb = sin(pi/advance);
		for (j=0; j<advance; j++) {
			double tt;
			for (i=j; i<N; i+=(advance<<1)) {
				cmplx* a = &vect[i];
				cmplx* b = &vect[i+advance];
				double tr =  b->r*cosa + b->i*sina;
				double ti = -b->r*sina + b->i*cosa;
#if 1
				printf("{%i:%12.6g, %12.6g}, {%i:%12.6g, %12.6g}",i,a->r,a->i,i+advance,b->r,b->i);
#endif
				b->r = a->r - tr;
				b->i = a->i - ti;
				a->r += tr;
				a->i += ti;
#if 1
				printf("{%i:%12.6g, %12.6g}, {%i:%12.6g, %12.6g}\n",i,a->r,a->i,i+advance,b->r,b->i);
#endif
			}
#if 0
			if (advance==(N>>2)) {
				if ((j&7)==0) printf("\nj:%i ",j);
				printf("%12.6g, ",cosa);
			}
#endif
			tt = cosa;
			cosa = cosa*cosb - sina*sinb;	//cos(a+b) = cosa * cosb - sina * sinb
			sina = sina*cosb + tt*sinb;	//sin(a+b) = sina * cosb + cosa * sinb
		}
	}
	printf("\n");
}
static void PrintTable(cmplx* vect,int idiv)
{
	int i;
	double real,img,div;
	div = idiv;
	printf("\n---------------------------\n");
	for (i=0; i<N; i++) {
		if ((i&3)==0) printf("\n%3i:",i);
		real = vect[i].r;
		img = vect[i].i;
		if (div>1) {
			real = real/div;
			img = img/div;
		}
		printf("{%8.6g,%8.6g}, ",real,img);
	}
	printf("\n---------------------------\n");
}
static void ChangeImgSign(cmplx* vect,const int llogN)
{
	const int n = 1<<llogN;
	int i;
	for (i=0; i<N; i++) {
		vect[i].i = -vect[i].i;
	}
}
int main(void)
{
	int i;
	cmplx vect[N];
	for (i=0; i<N; i++) {vect[i].r=i; vect[i].i=0;}
	fft(vect);
#if 1
	PrintTable(vect,1);
#endif
	ChangeImgSign(vect,logN);
	fft(vect);
	PrintTable(vect,N);
	printf("\n");
}

