#include "ICommon.h"
#include "ScaleUpObj.h"
#include "Scale.h"
#include "ScaleYf2.h"

//BGR lcd display of Black and White JPEG image
void ScaleYf2::CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth)
{
	DWORD yDiv = yObj->GetDiv();
	DWORD yMult = GetReciprocal(yDiv);		//0x100000000/yDiv;
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
				*pd++ = (BYTE)y;
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
				stosb
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
			do { *pd++ = static_cast<BYTE>(*yPtr++);} while (--colsLeft);
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
				stosb
				loop	ss1
				popf
			}
#endif
			dest += rowWidth;
		}
	}
}

