#include "ICommon.h"
#include "ScaleUpObj.h"
#include "Scale.h"
#include "ScaleYf1.h"

//RGB lcd display of Black and White JPEG image
void ScaleYf1::CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth)
{
	DWORD yDiv = yObj->GetDiv();
	DWORD yMult = GetReciprocal(yDiv);		//0x100000000/yDiv;
	srcWidth = srcWidth/3;
	if (yDiv>1)
	{
		while (rowCnt--)
		{
			DWORD* yPtr = yObj->SampleRow();
#ifdef NOASM
			BYTE* pd = dest;
			int colsLeft = srcWidth;
			do
			{
				ULONGLONG ydata = (static_cast<ULONGLONG>(*yPtr++))*yMult;
				DWORD y = static_cast<DWORD>(ydata >> 32);
				int frac = static_cast<int>(ydata);
				if (frac<0) y++;
				if (y>255) y=255;
				*(pd+2) = (BYTE)y;

				ydata = (static_cast<ULONGLONG>(*yPtr++))*yMult;
				y = static_cast<DWORD>(ydata >> 32);
				frac = static_cast<int>(ydata);
				if (frac<0) y++;
				if (y>255) y=255;
				*(pd+1) = (BYTE)y;

				ydata = (static_cast<ULONGLONG>(*yPtr++))*yMult;
				y = static_cast<DWORD>(ydata >> 32);
				frac = static_cast<int>(ydata);
				if (frac<0) y++;
				if (y>255) y=255;
				*pd = (BYTE)y;
				pd+=3;
			} while (--colsLeft);
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
				mov		2[edi],al

				lodsd
				mul		ebx
				add		eax,eax
				mov		eax,edx
				adc		eax,0
				cmp		eax,256
				setc	dl				//(eax<256) ? 1 : 0
				dec		dl				//(eax<256) ? 0 : 255
				or		al,dl			//(eax<256) ? al : 255
				mov		1[edi],al

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
			dest += rowWidth;
		}
	}
	else
	{
		while (rowCnt--)
		{
			DWORD* yPtr = yObj->SampleRow();
#ifdef NOASM
			BYTE* pd = dest;
			int colsLeft = srcWidth;
			while (colsLeft--)
			{
				*(pd+2) = (BYTE)(*yPtr++);
				*(pd+1) = (BYTE)(*yPtr++);
				*pd = (BYTE)(*yPtr++);
				pd+=3;
			}
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
				mov		2[edi],al
				lodsd
				mov		1[edi],al
				lodsd
				mov		[edi],al
				add		edi,3
				loop	ss1
				popf
			}
#endif
			dest += rowWidth;
		}
	}
}


