#include "ICommon.h"
#include "ScaleUpObj.h"
#include "Scale.h"
#include "ScaleYCbCr.h"



void ScaleYCbCr::DoLine1JPEG(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult)
{
#ifdef NOASM
	do
	{
		DWORD y = *yPtr++;
		DWORD cb = *cbPtr++;
		DWORD cr = *crPtr++;
//now convert from Sample space to RGB space.
//B = Y + 1.72200*(Cb-128)
//B = Y + 1.72200*Cb - 1.72*128
//B = Y + 1.72200*Cb - 220.16
//B = Y + (.86100*Cb)*2 - 220.16
		ULONGLONG yData = static_cast<ULONGLONG>(y) << 32;
		ULONGLONG tval = yData + ((static_cast<ULONGLONG>(cbBlueMult)*cb)<<1) - ((static_cast<ULONGLONG>(220)<<32) + 687194767);
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

		tval = (yData + ((static_cast<ULONGLONG>(135) << 32) + 1974997761)) - static_cast<ULONGLONG>(cbGreenMult)*cb - static_cast<ULONGLONG>(crGreenMult)*cr;
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
		tval = yData + ((static_cast<ULONGLONG>(crRedMult)*cr)<<1) - ((static_cast<ULONGLONG>(179) << 32) + 1958505087);
		val = static_cast<int>(tval >> 32);
		frac = static_cast<int>(tval);
		if (frac<0) val++;
		if (((DWORD)val)>255)
		{
			if (val<0){val = 0;}
			else {val = 255;}
		}
		*dest++ = (BYTE)val;
	} while (--colsLeft);
#else
	DWORD yDataHigh;
	__asm
	{
		pushf
		cld
		mov		edi,dest
// ********************
	tm1:
		mov		esi,yPtr
		lodsd
		mov		yDataHigh,eax

		mov		yPtr,esi
		mov		esi,cbPtr
		mov		eax,cbBlueMult
		mov		ecx,[esi]
		mul		ecx
		add		eax,eax
		adc		edx,edx

		add		edx,yDataHigh
		sub		eax,687194767
		sbb		edx,220
		add		eax,eax
		mov		eax,edx
		adc		eax,0

		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb
// ********************

		lodsd
		mov		ecx,eax
		mov		eax,cbGreenMult
		mul		ecx
		mov		ecx,yDataHigh
		mov		ebx,1974997761
		add		ecx,135

		sub		ebx,eax
		sbb		ecx,edx
		mov		cbPtr,esi
		mov		esi,crPtr
		mov		eax,crGreenMult
		mov		edx,[esi]
		mul		edx
		sub		ebx,eax
		sbb		ecx,edx

		add		ebx,ebx
		mov		eax,ecx
		adc		eax,0
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb
// ********************
		lodsd
		mov		ecx,eax
		mov		eax,crRedMult
		mul		ecx
		mov		crPtr,esi
		add		eax,eax
		adc		edx,edx

		add		edx,yDataHigh
		sub		eax,1958505087
		sbb		edx,179
		add		eax,eax
		mov		eax,edx
		adc		eax,0
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb

		dec		colsLeft
		jnz		tm1
		popf
	}
#endif
}

void ScaleYCbCr::DoLineJPEG(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD yMult,DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult)
{
#ifdef NOASM
	do
	{
		DWORD y = *yPtr++;
		DWORD cb = *cbPtr++;
		DWORD cr = *crPtr++;
//now convert from Sample space to RGB space.
//B = Y + 1.72200*(Cb-128)
//B = Y + 1.72200*Cb - 1.72*128
//B = Y + 1.72200*Cb - 220.16
//B = Y + (.86100*Cb)*2 - 220.16
		ULONGLONG yData = static_cast<ULONGLONG>(yMult)*y;
		ULONGLONG tval = yData + ((static_cast<ULONGLONG>(cbBlueMult)*cb)<<1) - ((static_cast<ULONGLONG>(220)<<32) + 687194767);
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

		tval = (yData + ((static_cast<ULONGLONG>(135) << 32) + 1974997761)) - static_cast<ULONGLONG>(cbGreenMult)*cb - static_cast<ULONGLONG>(crGreenMult)*cr;
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
		tval = yData + ((static_cast<ULONGLONG>(crRedMult)*cr)<<1) - ((static_cast<ULONGLONG>(179) << 32) + 1958505087);
		val = static_cast<int>(tval >> 32);
		frac = static_cast<int>(tval);
		if (frac<0) val++;
		if (((DWORD)val)>255)
		{
			if (val<0){val = 0;}
			else {val = 255;}
		}
		*dest++ = (BYTE)val;
	} while (--colsLeft);
#else

	DWORD yDataHigh;
	DWORD yDataLow;
	__asm
	{
		pushf
		cld
		mov		edi,dest
	m1:
		mov		esi,yPtr
		lodsd
		mov		ecx,eax
		mov		eax,yMult
		mul		ecx
		mov		yDataHigh,edx
		mov		yDataLow,eax

		mov		yPtr,esi
		mov		esi,cbPtr
		mov		eax,cbBlueMult
		mov		ecx,[esi]
		mul		ecx
		add		eax,eax
		adc		edx,edx
		add		eax,yDataLow
		adc		edx,yDataHigh
		sub		eax,687194767
		sbb		edx,220
		add		eax,eax
		mov		eax,edx
		adc		eax,0
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb
// ********************

		lodsd
		mov		ecx,eax
		mov		eax,cbGreenMult
		mul		ecx
		mov		ebx,yDataLow
		mov		ecx,yDataHigh
		add		ebx,1974997761
		adc		ecx,135
		sub		ebx,eax
		sbb		ecx,edx
		mov		cbPtr,esi
		mov		esi,crPtr
		mov		eax,crGreenMult
		mov		edx,[esi]
		mul		edx
		sub		ebx,eax
		sbb		ecx,edx

		add		ebx,ebx
		mov		eax,ecx
		adc		eax,0
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255		//negative number have low byte = 255

		or		eax,eax
		sets	dl				//(eax<0) ? 1 : 0
		add		al,dl			//(eax<0) ? 0 : al

		stosb
// ********************

		lodsd
		mov		ecx,eax
		mov		eax,crRedMult
		mul		ecx
		mov		crPtr,esi
		add		eax,eax
		adc		edx,edx

		add		eax,yDataLow
		adc		edx,yDataHigh
		sub		eax,1958505087
		sbb		edx,179
		add		eax,eax
		mov		eax,edx
		adc		eax,0
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

#define C_1_72200_DIV2	0xdc6a7ef9
#define C_0_34414	0x58198f1d
#define C_0_71414	0xb6d1e108
#define C_1_40200_DIV2 0xb374bc6a

void ScaleYCbCr::CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth)
{
	DWORD yDiv = yObj->GetDiv();
	DWORD cbDiv = cbObj->GetDiv();
	DWORD crDiv = crObj->GetDiv();
	DWORD cbBlueMult  =  GetMult( C_1_72200_DIV2, cbDiv);
	DWORD cbGreenMult =  GetMult( C_0_34414, cbDiv);
	DWORD crGreenMult =  GetMult( C_0_71414, crDiv);
	DWORD crRedMult   =  GetMult( C_1_40200_DIV2, crDiv);
	
	if (yDiv>1)
	{
		DWORD yMult = GetReciprocal(yDiv);		//0x100000000/yDiv;
		while (rowCnt--)
		{
			DoLineJPEG(dest,yObj->SampleRow(),cbObj->SampleRow(),crObj->SampleRow(),srcWidth,
					yMult,cbBlueMult,cbGreenMult,crGreenMult,crRedMult);
			dest += rowWidth;
		}
	}
	else
	{
		while (rowCnt--)
		{
			DoLine1JPEG(dest,yObj->SampleRow(),cbObj->SampleRow(),crObj->SampleRow(),srcWidth,
				cbBlueMult,cbGreenMult,crGreenMult,crRedMult);
			dest += rowWidth;
		}
	}
}

