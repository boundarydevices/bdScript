#include "ICommon.h"
#include "ScaleUpObj.h"
#include "Scale.h"
#include "ScaleRGB.h"


void ScaleRGB::MoveLineRGB(BYTE* dest,DWORD* yPtr,int srcWidth)
{
#ifdef NOASM
	do { *dest = (BYTE)(*yPtr++); dest += 3; } while (--srcWidth);
#else
	__asm
	{
		pushf
		cld
		mov		edi,dest
		mov		esi,yPtr
		mov		ecx,srcWidth
	ss1:
		lodsd
		mov		[edi],al
		add		edi,3
		loop	ss1
		popf
	}
#endif
}
void ScaleRGB::DoLineRGB(BYTE* dest,DWORD* yPtr,int srcWidth,DWORD yMult)
{
#ifdef NOASM
	do
	{
		ULONGLONG mm = ((ULONGLONG)yMult) * (*yPtr++);
		DWORD y = (DWORD)(mm>>32);
		int round = (int)mm;
		if (round<0) y++;
		if (y>255) y=255;
		*dest = (BYTE)y;
		dest+=3;
	} while (--srcWidth);
#else
	__asm
	{
		pushf
		cld
		mov		edi,dest
		mov		esi,yPtr
		mov		ecx,srcWidth
		mov		ebx,yMult
	ww1:
		lodsd
		mul		ebx
		add		eax,eax
		mov		eax,edx
		adc		eax,0
		cmp		eax,256
		setc	dl				//(eax<256) ? 1 : 0
		dec		dl				//(eax<256) ? 0 : 255
		or		al,dl			//(eax<256) ? al : 255
		mov		[edi],al
		add		edi,3
		loop	ww1
		popf
	}
#endif
}



void ScaleRGB::CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth)
{
//blue - y
//green - cb
//red - cr
	DWORD yDiv = yObj->GetDiv();
	DWORD cbDiv = cbObj->GetDiv();
	DWORD crDiv = crObj->GetDiv();
	DWORD yMult = GetReciprocal(yDiv);		//0x100000000/yDiv;
	DWORD cbMult = GetReciprocal(cbDiv);		//0x100000000/cbDiv;
	DWORD crMult = GetReciprocal(crDiv);		//0x100000000/crDiv;
	while (rowCnt--)
	{
		DWORD* yPtr = yObj->SampleRow();
		DWORD* cbPtr = cbObj->SampleRow();
		DWORD* crPtr = crObj->SampleRow();
		if (yDiv>1) DoLineRGB(dest,yPtr,srcWidth,yMult);
		else MoveLineRGB(dest,yPtr,srcWidth);
		if (cbDiv>1) DoLineRGB(dest+1,cbPtr,srcWidth,cbMult);
		else MoveLineRGB(dest+1,cbPtr,srcWidth);
		if (crDiv>1) DoLineRGB(dest+2,crPtr,srcWidth,crMult);
		else MoveLineRGB(dest+2,crPtr,srcWidth);
		dest += rowWidth;
	}
}

