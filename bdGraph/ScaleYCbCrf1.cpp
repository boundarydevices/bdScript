#include "ICommon.h"
#include "ScaleUpObj.h"
#include "Scale.h"
#include "ScaleYCbCrf1.h"


void ScaleYCbCrf1::DoLineJPEGf1(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD yMult,DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult)
{

	DWORD y2;
	DWORD y3;
	DWORD y4;

	DWORD crr1;
	DWORD crr2;
	DWORD crr3;
	DWORD crr4 = *crPtr;

	DWORD cb2;
	DWORD cb3;
	DWORD cb4;
	DWORD cb5 = *cbPtr++;


#ifdef NOASM
	do
	{
		y2 = *yPtr++;
		y3 = *yPtr++;
		y4 = *yPtr++;

		crr1 = crr4;
		crr2 = *crPtr++;
		crr3 = *crPtr++;
		crr4 = *crPtr++;
// ********************
		cb2 = cb5;
		cb3 = *cbPtr++;
		cb4 = *cbPtr++;
		cb5 = (colsLeft>1)? *cbPtr++ : cb4;

// ********************

//now convert from Sample space to RGB space.
//B = Y + 1.72200*(Cb-128)
//B = Y + 1.72200*Cb - 1.72*128
//B = Y + 1.72200*Cb - 220.16
//B = Y + (.86100*Cb)*2 - 220.16
		ULONGLONG yData = (yMult)? (static_cast<ULONGLONG>(yMult)*y4) : (static_cast<ULONGLONG>(y4) <<32);
		ULONGLONG tval = yData + ((static_cast<ULONGLONG>(cbBlueMult)*(cb3+cb4+cb5))<<1) - ((static_cast<ULONGLONG>(220)<<32) + 687194767);
		int val = static_cast<int>(tval >> 32);
		int frac = static_cast<int>(tval);
		if (frac<0) val++;
		if (((DWORD)val)>255)
		{
			if (val<0){val = 0;}
			else {val = 255;}
		}
		*dest++ = (BYTE)val;
// ********************

//G = Y - 0.34414*(Cb-128) - 0.71414*(Cr-128)
//G = Y - 0.34414*Cb - 0.71414*Cr + ((0.34414+0.71414)*128)
//G = Y - 0.34414*Cb - 0.71414*Cr + 135.45984
//G = Y + 135.45984 - 0.34414*Cb - 0.71414*Cr

		yData = (yMult)? (static_cast<ULONGLONG>(yMult)*y3) : (static_cast<ULONGLONG>(y3) <<32);
		tval = (yData + ((static_cast<ULONGLONG>(135) << 32) + 1974997761)) - static_cast<ULONGLONG>(cbGreenMult)*(cb2+cb3+cb4) - static_cast<ULONGLONG>(crGreenMult)*(crr2+crr3+crr4);
		val = static_cast<int>(tval >> 32);
		frac = static_cast<int>(tval);
		if (frac<0) val++;
		if (((DWORD)val)>255)
		{
			if (val<0){val = 0;}
			else {val = 255;}
		}
		*dest++ = (BYTE)val;
// ********************

//R = Y          + 1.40200*(Cr-128)
//R = Y          + 1.40200*Cr     - 1.402*128
//R = Y          + 1.40200*Cr     - 179.456
//R = Y          + (.70100*Cr<<1) - 179.456
		yData = (yMult)? (static_cast<ULONGLONG>(yMult)*y2) : (static_cast<ULONGLONG>(y2) <<32);
		tval = yData + ((static_cast<ULONGLONG>(crRedMult)*(crr1+crr2+crr3))<<1) - ((static_cast<ULONGLONG>(179) << 32) + 1958505087);
		val = static_cast<int>(tval >> 32);
		frac = static_cast<int>(tval);
		if (frac<0) val++;
		if (((DWORD)val)>255)
		{
			if (val<0){val = 0;}
			else {val = 255;}
		}
		*dest++ = (BYTE)val;
// ********************
	} while (--colsLeft);
#else
	__asm
	{
		pushf
		cld
		mov		edi,dest
	m1:
		mov		esi,yPtr
		lodsd
		mov		y2,eax
		lodsd
		mov		y3,eax
		lodsd
		mov		y4,eax
		mov		yPtr,esi

		mov		eax,crr4
		mov		crr1,eax
		mov		esi,crPtr;
		lodsd
		mov		crr2,eax
		lodsd
		mov		crr3,eax
		lodsd
		mov		crr4,eax
		mov		crPtr,esi
// ********************

		mov		eax,cb5
		mov		cb2,eax
		mov		esi,cbPtr
		lodsd
		mov		ecx,colsLeft
		mov		cb3,eax
		lodsd
		mov		cb4,eax
		dec		ecx
		jle		m8
		lodsd
	m8:
		mov		cb5,eax
		mov		cbPtr,esi

// ********************
		mov		ecx,y4
		mov		eax,yMult
		or		eax,eax
		jz		z1
		mul		ecx
		mov		ecx,edx
	z1:	mov		ebx,eax				//ULONGLONG yData = (yMult)? (static_cast<ULONGLONG>(yMult)*y4) : (static_cast<ULONGLONG>(y4) <<32);

		mov		eax,cbBlueMult
		mov		edx,cb3
		add		edx,cb4
		add		edx,cb5
		mul		edx
		add		eax,eax
		adc		edx,edx
		add		eax,ebx
		adc		edx,ecx
		sub		eax,687194767
		sbb		edx,220				//ULONGLONG tval = yData + ((static_cast<ULONGLONG>(cbBlueMult)*(cb3+cb4+cb5))<<1) - ((static_cast<ULONGLONG>(220)<<32) + 687194767);
		add		eax,eax
		mov		eax,edx				//int val = tval >> 32;
		adc		eax,0				//if (frac<0) val++;
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb
// ********************
		mov		ecx,y3
		mov		eax,yMult
		or		eax,eax
		jz		z2
		mul		ecx
		mov		ecx,edx
	z2: mov		ebx,eax				//yData = (yMult)? (static_cast<ULONGLONG>(yMult)*y3) : (static_cast<ULONGLONG>(y3) <<32);

		mov		edx,cb2
		add		edx,cb3
		add		edx,cb4
		mov		eax,cbGreenMult
		mul		edx
		add		ebx,1974997761
		adc		ecx,135
		sub		ebx,eax
		sbb		ecx,edx

		mov		eax,crGreenMult
		mov		edx,crr2
		add		edx,crr3
		add		edx,crr4
		mul		edx
		sub		ebx,eax
		sbb		ecx,edx				//tval = (yData + ((static_cast<ULONGLONG>(135) << 32) + 1974997761)) - static_cast<ULONGLONG>(cbGreenMult)*(cb2+cb3+cb4) - static_cast<ULONGLONG>(crGreenMult)*(crr2+crr3+crr4)

		add		ebx,ebx
		mov		eax,ecx				//val = tval >> 32;
		adc		eax,0				//if (frac<0) val++;
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al
		stosb
// ********************
		mov		ecx,y2
		mov		eax,yMult
		or		eax,eax
		jz		z3
		mul		ecx
		mov		ecx,edx
	z3:	mov		ebx,eax				//yData = (yMult)? (static_cast<ULONGLONG>(yMult)*y2) : (static_cast<ULONGLONG>(y2) <<32);

		mov		edx,crr1
		add		edx,crr2
		add		edx,crr3
		mov		eax,crRedMult
		mul		edx
		add		eax,eax
		adc		edx,edx

		add		eax,ebx
		adc		edx,ecx
		sub		eax,1958505087
		sbb		edx,179				//tval = yData + ((static_cast<ULONGLONG>(crRedMult)*(crr1+crr2+crr3))<<1) - ((static_cast<ULONGLONG>(179) << 32) + 1958505087)
		add		eax,eax
		mov		eax,edx				//val = tval >> 32;
		adc		eax,0				//frac = tval;
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb

		dec		colsLeft
		jnz		m1
		popf
	}
#endif
}

void ScaleYCbCrf1::CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth)
{
	DWORD yDiv = yObj->GetDiv();
	DWORD cbDiv = cbObj->GetDiv(); cbDiv += (cbDiv<<1);
	DWORD crDiv = crObj->GetDiv(); crDiv += (crDiv<<1);

	DWORD cbBlueMult  =  GetMult( ((LONG)(((double)1.72200*0x10000000)*8)),cbDiv);
	DWORD cbGreenMult =  GetMult( ((LONG)(((double)0.34414*0x10000000)*0x10)),cbDiv);
	DWORD crGreenMult =  GetMult( ((LONG)(((double)0.71414*0x10000000)*0x10)),crDiv);
	DWORD crRedMult   =  GetMult( ((LONG)(((double)1.40200*0x10000000)*8)),crDiv);
	DWORD yMult = GetReciprocal(yDiv);		//0x100000000/yDiv;
	srcWidth /= 3;
	while (rowCnt--)
	{
		DoLineJPEGf1(dest,yObj->SampleRow(),cbObj->SampleRow(),crObj->SampleRow(),srcWidth,
				yMult,cbBlueMult,cbGreenMult,crGreenMult,crRedMult);
		dest += rowWidth;
	}
}
