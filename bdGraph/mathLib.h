#if 0
extern "C" unsigned long uDivRem(unsigned long value,unsigned long divisor,unsigned long * rem);
//return (1<<32)/value and remainder
extern "C" unsigned long uReciprocal(unsigned long value,unsigned long * rem);
#else
#include <stdio.h>
extern "C" unsigned long uDivRemM(unsigned long value,unsigned long divisor,unsigned long * rem);
static unsigned long uDivRem(unsigned long value,unsigned long divisor,unsigned long * rem)
{
	unsigned long result1 = value / divisor;
	unsigned long rem1 = value - (result1 * divisor);
	unsigned long result2 = uDivRemM(value,divisor,rem);
	if ((result1 != result2)||(rem1 != *rem)) {
		printf("\r\n%i / %i = %i or %i, rem = %i or %i\r\n",value,divisor,result1,result2,rem1,*rem);
		*rem = rem1;
	}
	return result1;
}

extern "C" unsigned long uReciprocalM(unsigned long value,unsigned long * rem);
static unsigned long uReciprocal(unsigned long value,unsigned long * rem)
{
	ULONGLONG u = static_cast<ULONGLONG>(1)<<32;
	unsigned long result1 = static_cast<DWORD>(u/value);
	DWORD rem1 = static_cast<DWORD>(u % value);

	unsigned long result2 = uReciprocalM(value,rem);
	if ((result1 != result2)||(rem1 != *rem)) {
		printf("\r\n(1<<32) / %i = %i or %i, rem = %i or %i\r\n",value,result1,result2,rem1,*rem);
		*rem = rem1;
	}
	return result1;
}
#endif


extern "C" unsigned long uShiftRight(ULONGLONG* m_nextBitsBuffer,int shift);
