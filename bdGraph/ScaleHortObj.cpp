#include "ICommon.h"
#include "ScaleHortObj.h"
#include "SImagePosition.h"

/////////////////////////////////////////////////////////////////////////////
class ScaleHortObj1to1 : public ScaleHortObj
{
public:
	virtual BOOL ScaleRow(DWORD* destScaledRow);
};

class ScaleHortObj1to2 : public ScaleHortObj
{
public:
	virtual BOOL ScaleRow(DWORD* destScaledRow);
};

class ScaleHortObj1to3 : public ScaleHortObj
{
public:
	virtual BOOL ScaleRow(DWORD* destScaledRow);
};

class ScaleHortObjDown : public ScaleHortObj
{
public:
	virtual BOOL ScaleRow(DWORD* destScaledRow);
};

ScaleHortObj* ScaleHortObj::GetObject(int hUpCode,DWORD hScaleFrom,DWORD hScaleTo,DWORD* phDiv)
{
	DWORD hDiv = hScaleTo<<1;
	ScaleHortObj* p = NULL;
	switch(hUpCode)
	{
		case 1:
		{
			p = new ScaleHortObj1to1();
			hDiv =1;
			break;
		}
		case 2:
		{
			p = new ScaleHortObj1to2();
			break;
		}
		case 3:
		{
			p = new ScaleHortObj1to3();
			hDiv =3;
			break;
		}
		default:
			if (hUpCode>=0)
			{
				p = new ScaleHortObj();
				if (!((hScaleTo-hScaleFrom)&1) ) hDiv = hScaleTo;
				break;
			}
			else
			{
				p = new ScaleHortObjDown();
				hDiv = hScaleFrom;
				break;
			}
	}
	*phDiv = hDiv;
	return p;
}
void ScaleHortObj::Release()
{
	delete this;
}

/////////////////////////////////////////////////////////////////////////////

ScaleHortObj::ScaleHortObj()
{
	m_pSample = NULL;
	m_rowWidth = 0;
	m_outWidth = 0;
	m_hScaleFrom = 0;
	m_hScaleTo = 0;
}

ScaleHortObj::~ScaleHortObj()
{
}

BOOL ScaleHortObj::Init(ImagePosition* pSample,DWORD rowWidth,DWORD outWidth,DWORD hScaleFrom,DWORD hScaleTo,int initialWeight)
{
	if (pSample)
	{
		m_pSample = pSample;
		m_rowWidth = rowWidth;				//# of relevent samples in row
		m_outWidth = outWidth;				//# of values to be produced
		if (hScaleFrom<hScaleTo)
		{
			if (initialWeight&1)
			{
				hScaleFrom <<=1;
				hScaleTo <<=1;
			}
			else
			{
				initialWeight>>=1;
			}
		}
		m_hScaleFrom = hScaleFrom;
		m_hScaleTo = hScaleTo;
		m_initialWeight = initialWeight;
		return TRUE;
	}
	return FALSE;
}

BOOL ScaleHortObj::ScaleRow(DWORD* destScaledRow)
{
	const BYTE* src = m_pSample->GetNextRow();
	if (!src) {return FALSE;}
//offset lower sample relative to higher sampled
//xoffset[0,0]=(maxHFactor/compHFact -1)/2  or (n/m -1)/2 or (n-m)/(2m)
//Yoffset[0,0]=(maxVFactor/compVFact -1)/2

//for i=0..m-1 : l[i] = n/m(i) + (n-m)/(2m) //spacing between higher sample n is 1, lower is n/m
//for i=0..m-1 : l[i] = 2ni + (n-m)			//spacing between higher sample n is 2m, lower is 2n

//now let's make the higher sample relative to lower sample
//h[0] = -(n-m)/(2m)		//spacing of h[] = 1; spacing of lower is n/m
//h[0] = -(n-m)/(2n)		//spacing of h[] = m/n; spacing of lower is 1
//h[i] = (m/n)i - (n-m)/(2n)
//for i=0..n-1 : h[i] = 2mi -(n-m)		//spacing of h[] = 2m; spacing of lower is 2n

//anti-alias the lower sampled items to increase to max sampling resolution
//j=0;
//lVal[-1]=lVal[0];
//lVal[m] = lVal[m-1];
//hm = n-m
//hmSub = 2m;
//hmAdd = 2n;
//if (!(hm&1) ) {hm >>=1; hmSub>>=1; hmAdd>>=1;}
//for i=0..n-1
//{
//    hVal[i] = (hm*(lVal[j-1]-lVal[j]))/hmAdd + lVal[j];
//	  hm-=hmSub;
//	  if (hm<0) {j++;hm+=hmAdd;}
//}

	int weightA = m_initialWeight;
	DWORD max2 = m_hScaleTo;		//already shifted in Init()
	int low2 = -((int)m_hScaleFrom); //already shifted in Init()
	DWORD srcLeft = m_rowWidth-1;
	DWORD* destEnd = destScaledRow+m_outWidth;
#ifdef NOASM
	if (srcLeft>0)
	{
		DWORD a = *src;
		do
		{
			if (weightA<0)
			{
				weightA += max2;
				a= *src++;
				if (!(--srcLeft)) break;
			}
			//Scaled values need divided by 2*m_hMax eventually
			*destScaledRow++ = ( (a* ((DWORD)weightA)) + ((*src)*((DWORD)(max2-weightA))) );
			weightA += low2;
		} while(TRUE);
//  ****************
		do
		{
			*destScaledRow++ = ( (a* ((DWORD)weightA)) + ((*src)*((DWORD)(max2-weightA))) );
			if (destScaledRow>=destEnd) return TRUE;
			weightA += low2;
		} while (weightA>=0);
	}
//  ****************
	DWORD a = max2 * (*src);
	do
	{
		*destScaledRow++ = a;
	} while (destScaledRow<destEnd);
//  ****************
#else
	__asm
	{
		pushf
		cld
		mov		edi,destScaledRow
		mov		esi,src
		mov		eax,srcLeft
		or		eax,eax
		jle		endloop
		mov		eax,weightA
		movzx	ebx,BYTE PTR[esi]
		or		eax,eax
		js		sNextSample

	sm1:
		mov		eax,weightA
		mul		ebx
		mov		ecx,eax
		mov		eax,max2
		sub		eax,weightA
		movzx	edx,BYTE PTR[esi]
		mul		edx
		add		eax,ecx
		stosd
		mov		eax,low2
		add		weightA,eax
		jns		sm1
	sNextSample:	
		mov		eax,max2
		add		weightA,eax
		lodsb
		mov		bl,al
		dec		srcLeft
		jnz		sm1

//  ****************

	sm2:
		mov		eax,weightA
		mul		ebx
		mov		ecx,eax
		mov		eax,max2
		sub		eax,weightA
		movzx	edx,BYTE PTR[esi]
		mul		edx
		add		eax,ecx
		stosd
		mov		eax,low2
		cmp		edi,destEnd
		jnc		sm5
		add		weightA,eax
		jns		sm2

	endloop:
//  ****************
		mov		eax,max2
		movzx	edx,BYTE PTR[esi]
		mul		edx
	sm4:
		stosd
		cmp		edi,destEnd
		jc		sm4
	sm5:
		popf
	}
#endif


	return TRUE;
}

BOOL ScaleHortObjDown::ScaleRow(DWORD* destScaledRow)
{
	const BYTE* src = m_pSample->GetNextRow();
	if (!src) {return FALSE;}
//offset lower sample relative to higher sampled
//xoffset[0,0]=(maxHFactor/compHFact -1)/2  or (n/m -1)/2 or (n-m)/(2m)
//Yoffset[0,0]=(maxVFactor/compVFact -1)/2

//for i=0..m-1 : l[i] = n/m(i) + (n-m)/(2m) //spacing between higher sample n is 1, lower is n/m
//for i=0..m-1 : l[i] = 2ni + (n-m)			//spacing between higher sample n is 2m, lower is 2n


//anti-alias the higher sampled items to decrease sampling resolution
//j=0;
//hm = n-m
//lVal[0] = 0;
//i=0;
//while (true)
//{
//  sum = 0
//	while (hm>=0)
//	{		
//		sum += hVal[j++];
//		hm-=m;
//	}
//  sum*=m;
//	sum += hVal[j]*(m+hm)
//	lVal[i++] += sum;
//	if (i==drawWidth) break;
//	lVal[i] = hVal[j++]*(-hm)
//  hm+=n;
//}


	DWORD val = 0;
	const DWORD scaleFrom = m_hScaleFrom;
	const DWORD scaleTo = m_hScaleTo;
	const DWORD weightAadd = scaleFrom-scaleTo;
	int weightA = m_initialWeight;
	DWORD destLeft= m_outWidth;
#ifdef NOASM
	if (weightA<0)
	{
		val = (*src++)* ((DWORD)(-weightA));
		weightA += weightAadd;
	}
	do
	{
//Scaled values need divided by m_hScaleFrom eventually
		DWORD sum = 0;
		while (weightA>0)
		{
			sum += *src++;
			weightA-=scaleTo;
		}
		if  (weightA)
		{
			sum *= scaleTo;
			sum += (*src) * ( (DWORD)(weightA+scaleTo));
			*destScaledRow++ = val+sum;
			val = (*src++)* ((DWORD)(-weightA));
			weightA += weightAadd;
		}
		else
		{
			sum += *src++;
			sum *= scaleTo;
			*destScaledRow++ = val+sum;
			val = 0;
			weightA = weightAadd;
		}
	} while(--destLeft);
#else
	__asm
	{
		pushf
		cld
		mov		edi,destScaledRow
		mov		esi,src
		mov		ebx,weightA
		or		ebx,ebx
		jns		sn1

		mov		eax,ebx
		neg		eax
		movzx	edx,BYTE PTR[esi]
		mul		edx
		inc		esi
		mov		val,eax			//val = (*src++)* ((DWORD)(-weightA));
		add		ebx,weightAadd
	sn1:
		xor		eax,eax
		xor		ecx,ecx			//sum
		mov		edx,scaleTo
		or		ebx,ebx
		jle		sn3
	sn2:
		lodsb
		add		ecx,eax
		sub		ebx,edx
		jg		sn2
	sn3:
		jz		sn4
		mov		eax,edx
		mul		ecx				//sum*scaleTo
		mov		ecx,eax
		movzx	edx,BYTE PTR[esi]
		mov		eax,ebx
		add		eax,scaleTo
		mul		edx				//(*src) * ( (DWORD)(weightA+scaleTo));
		add		eax,ecx
		add		eax,val
		stosd

		mov		eax,ebx
		neg		eax
		movzx	edx,BYTE PTR[esi]
		mul		edx
		inc		esi
		mov		val,eax			//val = (*src++)* ((DWORD)(-weightA));
		add		ebx,weightAadd
		dec		destLeft
		jnz		sn1
		jmp		sn5

	sn4:
		lodsb
		add		ecx,eax			//sum += *src++;
		mov		eax,edx
		mul		ecx				//sum *= scaleTo;
		add		eax,val
		stosd					//*destScaledRow++ = val+sum;
		xor		eax,eax
		mov		val,eax			//val=0;
		mov		ebx,weightAadd	//weightA = weightAadd;
		xor		ecx,ecx			//sum = 0
		mov		edx,scaleTo
		dec		destLeft
		jnz		sn2
	sn5:
		popf
	}
#endif

	return TRUE;
}

BOOL ScaleHortObj1to1::ScaleRow(DWORD* destScaledRow)
{
	const BYTE* src = m_pSample->GetNextRow();
	if (!src) {return FALSE;}

	DWORD srcLeft = m_outWidth;		//==m_rowWidth
#ifdef NOASM
	do { *destScaledRow++ = (DWORD)(*src++);} while(--srcLeft);
#else
	__asm
	{
		pushf
		cld
		mov		edi,destScaledRow
		mov		esi,src
		mov		ecx,srcLeft
		xor		eax,eax
	rr1:
		lodsb
		stosd
		loop	rr1
		popf
	}
#endif

	return TRUE;
}

BOOL ScaleHortObj1to2::ScaleRow(DWORD* destScaledRow)
{
	const BYTE* src = m_pSample->GetNextRow();
	if (!src) {return FALSE;}

	DWORD srcLeft = m_rowWidth-1;
	DWORD* destEnd = destScaledRow+m_outWidth;
	int initialWeight = m_initialWeight;

#ifdef NOASM
	DWORD a;
	DWORD b = *src++;
	if (srcLeft<=0) *destScaledRow++ = (DWORD)(b<<2);
	else
	{
		if (initialWeight>0)
		{
			*destScaledRow++ = (DWORD)( (b<<2) );
		}
		else if (initialWeight!=-1)
		{
			a=b; b= *src++;
			*destScaledRow++ = (DWORD)( a+ b+b+b );	//a+3b = (a-b) + (b<<2)
			if ((--srcLeft)==0)
			{
				if (destScaledRow<destEnd)
				{
					*destScaledRow++ = (DWORD)(b<<2);
				}
				return TRUE;
			}
		}
		a=b;
		b = *src++;
		while (--srcLeft)
		{
			*destScaledRow++ = (DWORD)( a+a+a + b );	//3a+b = (b-a) + (a<<2)
			*destScaledRow++ = (DWORD)( a+ b+b+b );	//a+3b = (a-b) + (b<<2)
			a=b;
			b = *src++;
		}
		*destScaledRow++ = (DWORD)( a+a+a + b );	//3a+b = (b-a) + (a<<2)
		if (destScaledRow<destEnd)
		{
			*destScaledRow++ = (DWORD)( a+ b+b+b );	//a+3b = (a-b) + (b<<2)
			if (destScaledRow<destEnd)
			{
				*destScaledRow++ = (DWORD)(b<<2);
			}
		}
	}
#else
	__asm
	{
		pushf
		cld
		mov		edi,destScaledRow
		mov		esi,src
		mov		ecx,srcLeft
		xor		eax,eax
		lodsb				//b
		or		ecx,ecx
		jle		s5
		mov		ebx,initialWeight
		mov		edx,eax		//a=b
		or		ebx,ebx
		jns		tt1
		inc		ebx
		jz		tt2			//else if (initialWeight==-1) goto s3
		xor		eax,eax
		lodsb				//b=*src++;
		mov		ebx,eax
		add		eax,eax
		add		eax,ebx		//3b
		add		eax,edx		//a+3b
		dec		ecx
		jz		s4
		stosd
		mov		edx,ebx		//a=b
		jmp		tt2
	tt1:
		shl		eax,2
		stosd				//*destScaledRow++ = (DWORD)( (b<<2) );
	tt2:
		push	ebp
		jmp		s3
	//edx - a, eax - b
	rr2:
		mov		ebp,eax
		sub		eax,edx		//b-a
		shl		edx,2		//a<<2
		mov		ebx,eax		//b-a
		add		eax,edx		//3a+b,  4a+(b-a)
		add		ebx,ebx		//2b-2a
		stosd				//3a+b
		add		eax,ebx		//3a+b+2b-2a = a+3b
		stosd
		mov		edx,ebp
	s3:
		xor		eax,eax
		lodsb				//b
		loop	rr2
		pop		ebp

		mov		ebx,eax
		sub		eax,edx		//b-a
		shl		edx,2		//a<<2
		mov		ecx,eax		//b-a
		add		eax,edx		//3a+b, 4a+(b-a)
		add		ecx,ecx		//2b-2a
		stosd				//3a+b
		cmp		edi,destEnd
		jnc		rr3
		add		eax,ecx		//3a+b+2b-2a = a+3b
	s4:
		stosd
		cmp		edi,destEnd
		jnc		rr3
		mov		eax,ebx
	s5:
		shl		eax,2
		stosd
	rr3:

		popf
	}
#endif

	return TRUE;
}


BOOL ScaleHortObj1to3::ScaleRow(DWORD* destScaledRow)
{
	const BYTE* src = m_pSample->GetNextRow();
	if (!src) {return FALSE;}
//offset lower sample relative to higher sampled
//xoffset[0,0]=(maxHFactor/compHFact -1)/2  or (3 -1)/2 or (2)/(2) or 1
//Yoffset[0,0]=(maxVFactor/compVFact -1)/2


//now let's make the higher sample relative to lower sample
//h[0] = -(3-1)/(2)	= -1	//spacing of h[] = 1; spacing of lower is 3
//h[0] = -(3-1)/(6)	= -1/3	//spacing of h[] = 1/3; spacing of lower is 1
//h[i] = (1/3)i - 1/3
//for i=0..2 : h[i] = i -1		//spacing of h[] = 1; spacing of lower is 3

//anti-alias the lower sampled items to increase to max sampling resolution
//j=0;
//lVal[-1]=lVal[0];
//lVal[m] = lVal[m-1];
//hm = 1
//i=0
//while (i<n)
//{
//    hVal[i++] = (lVal[j-1]-lVal[j])/3 + lVal[j];
//	  hVal[i++] = lVal[j];
//	  hVal[i++] = 2(lVal[j]-lVal[j+1])/3 + lVal[j+1];
//	  j++;
//}

	int weightA = m_initialWeight;
//max2 = 3
//low2 = -1
	DWORD srcLeft = m_rowWidth-1;
	DWORD* destEnd = destScaledRow+m_outWidth;
#ifdef NOASM
	DWORD a;
	DWORD b;
	a = *src++;
	b = a;
	if (srcLeft>0)
	{
		if (weightA<0)
		{
			b=*src++; weightA+=3; srcLeft--;
		}
	    if (srcLeft>0)
		{
			if (weightA==2) *destScaledRow++ = ( a+a + b );
			if (weightA>=1) *destScaledRow++ = ( a   + b+b );
			do
			{
				//Scaled values need divided eventually
				*destScaledRow++ = ( b+b+b );
				a=b;
				b = *src++;
				if (!(--srcLeft)) break;
				*destScaledRow++ = ( a+a + b );
				*destScaledRow++ = ( a   + b+b );
			} while(TRUE);
// ********************
			*destScaledRow++ = ( a+a + b );
			if (destScaledRow<destEnd) *destScaledRow++ = ( a + b+b );
		}
	}
// ********************
	b +=b+b;
	while (destScaledRow<destEnd)
	{
		*destScaledRow++ = b;
	}

// eax - val to store, ebx - a, ecx - srcLeft, edx - b
#else
	__asm
	{
		pushf
		cld
		mov		edi,destScaledRow
		mov		esi,src
		xor		ebx,ebx
		xor		edx,edx
		lodsb					
		mov		bl,al			//a = *src++;
		mov		dl,al			//b = a;
		mov		ecx,srcLeft

		or		ecx,ecx
		jle		endloop
		mov		eax,weightA
		or		eax,eax
		jns		s1
		lodsb
		mov		dl,al			//b = *src++
		dec		ecx
		je		endloop
		add		eax,3			//weightA+=3;

	s1:
		je		sW0
		dec		eax
		je		sW1

	sW2:
		mov		eax,ebx
		add		eax,eax
		add		eax,edx		//2a+b
		stosd
	sw1:
		mov		eax,edx
		add		eax,eax
		add		eax,ebx		//a+2b
		stosd
	sw0:
		mov		eax,edx
		add		eax,edx
		add		eax,edx		//3b
		stosd
		mov		bl,dl		//a=b;
		lodsb
		mov		dl,al		//b = *src++;
		loop	sW2
// ********************
		mov		eax,ebx
		add		eax,eax
		add		eax,edx		//2a+b
		stosd
		cmp		edi,destEnd
		jnc		sm5

		mov		eax,edx
		add		eax,eax
		add		eax,ebx		//a+2b
		stosd
		cmp		edi,destEnd
		jnc		sm5

	endloop:
// ********************
		mov		eax,edx
		add		eax,edx
		add		eax,edx
	sm4:
		stosd
		cmp		edi,destEnd
		jc		sm4
	sm5:
		popf
	}
#endif


	return TRUE;
}
