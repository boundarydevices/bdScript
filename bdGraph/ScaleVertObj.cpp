#include "ICommon.h"
#include "ScaleHortObj.h"
#include "ScaleVertObj.h"

/////////////////////////////////////////////////////////////////////////////
class ScaleVertObj1to1 : public IScaleVertObj
{
public:
	ScaleVertObj1to1();
	~ScaleVertObj1to1();
	DWORD*			ScaleRow(IScaleHortObj* pHortObj);
	BOOL			Init(DWORD outWidth,int initialWeight);
	void			Release();
private:
	DWORD		*m_ScaledRow1;
};

class ScaleVertObjDown : public IScaleVertObj
{
public:
	ScaleVertObjDown(DWORD vScaleFrom,DWORD vScaleTo);
	~ScaleVertObjDown();
	DWORD*			ScaleRow(IScaleHortObj* pHortObj);
	BOOL			Init(DWORD outWidth,int initialWeight);
	void			Release();
private:
	DWORD	m_scaleFrom;
	DWORD	m_scaleTo;
	int		m_weightA;
	DWORD	*m_buffer1;
	DWORD	*m_buffer2;
	DWORD	*m_buffer3;
	DWORD	*m_initRow;
	DWORD	*m_lastRead;
	DWORD	m_initWeight;
	DWORD	m_width;
};

IScaleVertObj* ScaleVertObj::GetObject(int vUpCode,DWORD vScaleFrom,DWORD vScaleTo,DWORD* phDiv)
{
	DWORD hDiv = *phDiv;
	IScaleVertObj* p = NULL;
	if (vUpCode==1)
	{
		p = new ScaleVertObj1to1();
	}
	else
	{
		if (vUpCode>=0)
		{
			p = new ScaleVertObj(vScaleFrom,vScaleTo);
			hDiv = (hDiv*vScaleTo)<<1;
		}
		else
		{
			p = new ScaleVertObjDown(vScaleFrom,vScaleTo);
			hDiv *= vScaleFrom;
		}
	}
	*phDiv = hDiv;
	return p;
}

ScaleVertObj1to1::ScaleVertObj1to1()
{
	m_ScaledRow1 = NULL;
}

ScaleVertObj1to1::~ScaleVertObj1to1()
{
	if (m_ScaledRow1) {delete[] m_ScaledRow1; m_ScaledRow1=NULL;}
}
void ScaleVertObj1to1::Release()
{
	delete this;
}

BOOL ScaleVertObj1to1::Init(DWORD outWidth,int initialWeight)
{
	if (m_ScaledRow1) {delete[] m_ScaledRow1; m_ScaledRow1=NULL;}
	m_ScaledRow1 = (new DWORD[outWidth]);
	if (m_ScaledRow1) return TRUE;
	return FALSE;
}


DWORD* ScaleVertObj1to1::ScaleRow(IScaleHortObj* pHortObj)
{
	pHortObj->ScaleRow(m_ScaledRow1);
	return	m_ScaledRow1;
}





ScaleVertObj::ScaleVertObj(DWORD vScaleFrom,DWORD vScaleTo)
{
	m_max2 = vScaleTo<<1;
	m_low2 = vScaleFrom<<1;
	m_weightA = vScaleTo-vScaleFrom;
	m_buffer = NULL;
	m_ScaledRow1 = NULL;
	m_ScaledRow2 = NULL;
	m_ScaledRow1Start = NULL;
	m_ScaledRow2Start = NULL;
	m_width = 0;
}

ScaleVertObj::~ScaleVertObj()
{
	if (m_buffer) {delete[] m_buffer; m_buffer=NULL;}
	if (m_ScaledRow1) {delete[] m_ScaledRow1; m_ScaledRow1=NULL;}
	if (m_ScaledRow2) {delete[] m_ScaledRow2; m_ScaledRow2=NULL;}
}
void ScaleVertObj::Release()
{
	delete this;
}

BOOL ScaleVertObj::Init(DWORD outWidth,int initialWeight)
{
	if (m_buffer) {delete[] m_buffer; m_buffer=NULL;}
	if (m_ScaledRow1) {delete[] m_ScaledRow1; m_ScaledRow1=NULL;}
	if (m_ScaledRow2) {delete[] m_ScaledRow2; m_ScaledRow2=NULL;}
	m_width = outWidth;
	m_weightA = initialWeight;

	m_buffer = new DWORD[outWidth];
	m_ScaledRow1 = new DWORD[outWidth];
	m_ScaledRow2 = new DWORD[outWidth];
	if (m_buffer && m_ScaledRow1 && m_ScaledRow2) return TRUE;
	return FALSE;
}

DWORD* ScaleVertObj::ScaleRow(IScaleHortObj* pHortObj)
{
	if (m_ScaledRow1Start==NULL)
	{
		pHortObj->ScaleRow(m_ScaledRow2);
		m_ScaledRow2Start = m_ScaledRow1Start = m_ScaledRow2;
		if (m_weightA<0)
		{
			m_weightA += m_max2;
			if (pHortObj->ScaleRow(m_ScaledRow1))
			{
				DWORD *temp = m_ScaledRow1;
				m_ScaledRow1 = m_ScaledRow2;
				m_ScaledRow2 = temp;
				m_ScaledRow1Start = m_ScaledRow1;
			}
			else
			{
				m_ScaledRow1Start = m_ScaledRow2;
			}
			m_ScaledRow2Start = m_ScaledRow2;
		}
	}
	else
	{
		m_weightA -= m_low2;
		if (m_weightA<0)
		{
			m_weightA += m_max2;
			if (pHortObj->ScaleRow(m_ScaledRow1))
			{
				DWORD *temp = m_ScaledRow1;
				m_ScaledRow1 = m_ScaledRow2;
				m_ScaledRow2 = temp;
				m_ScaledRow1Start = m_ScaledRow1;
			}
			else
			{
				m_ScaledRow1Start = m_ScaledRow2;
			}
			m_ScaledRow2Start = m_ScaledRow2;
		}
	}

	DWORD colsLeft = m_width;
	DWORD* dest = m_buffer;
	DWORD* ptr1 = m_ScaledRow1Start;
	DWORD* ptr2 = m_ScaledRow2Start;
	DWORD weightA = m_weightA;
	DWORD weightB = m_max2-weightA;
#ifdef NOASM
	while (colsLeft--)
	{
		*dest++ =  ((*ptr1++)*weightA) + ((*ptr2++)* weightB);
	}
#else
	__asm
	{
		pushf
		cld
		mov		edi,dest
		mov		esi,ptr1
		mov		ebx,ptr2
	sr1:
		lodsd
		mul		weightA
		mov		ecx,eax
		mov		eax,[ebx]
		mul		weightB
		add		ebx,4
		add		eax,ecx
		stosd
		dec		colsLeft
		jnz		sr1

		popf
	}
#endif

	return m_buffer;
}



ScaleVertObjDown::ScaleVertObjDown(DWORD vScaleFrom,DWORD vScaleTo)
{
	m_scaleFrom = vScaleFrom;
	m_scaleTo = vScaleTo;
	m_weightA = vScaleFrom-vScaleTo;
	m_buffer1 = NULL;
	m_buffer2 = NULL;
	m_buffer3 = NULL;
	m_initRow = NULL;
	m_lastRead = NULL;
	m_initWeight = 0;
	m_width = 0;
}

ScaleVertObjDown::~ScaleVertObjDown()
{
	if (m_buffer1) {delete[] m_buffer1; m_buffer1=NULL;}
	if (m_buffer2) {delete[] m_buffer2; m_buffer2=NULL;}
	if (m_buffer3) {delete[] m_buffer3; m_buffer3=NULL;}
}
void ScaleVertObjDown::Release()
{
	delete this;
}

BOOL ScaleVertObjDown::Init(DWORD outWidth,int initialWeight)
{
	if (m_buffer1) {delete[] m_buffer1; m_buffer1=NULL;}
	if (m_buffer2) {delete[] m_buffer2; m_buffer2=NULL;}
	if (m_buffer3) {delete[] m_buffer3; m_buffer3=NULL;}
	m_width = outWidth;
	m_weightA = initialWeight;

	m_buffer1 = new DWORD[outWidth];
	m_buffer2 = new DWORD[outWidth];
	m_buffer3 = new DWORD[outWidth];
	if (m_buffer1 && m_buffer2 && m_buffer3) return TRUE;
	return FALSE;
}
void CopyDword(DWORD* dest,DWORD* src,int count)
{
#ifdef NOASM
	while (count--) { *dest++ = *src++;}
#else
	__asm
	{
		push	esi
		push	edi
		pushf
		cld
		mov		esi,src
		mov		edi,dest
		mov		ecx,count
		rep		movsd
		popf
		pop		edi
		pop		esi
	}
#endif
}
DWORD* ScaleVertObjDown::ScaleRow(IScaleHortObj* pHortObj)
{
	DWORD scaleTo = m_scaleTo;
	DWORD weightAadd = m_scaleFrom-scaleTo;
	int weightA = m_weightA;

//Scaled values need divided by m_scaleFrom eventually
	DWORD *sumRow = NULL;
	DWORD *resultRow = NULL;
	if ((!m_initRow)&&(weightA<0))
	{
		if (!pHortObj->ScaleRow(m_buffer3))
		{
			//end of data, copy last line read to buffer3 if needed
			if (m_lastRead != m_buffer3)
			{
				CopyDword(m_buffer3,m_lastRead,m_width);
			}
		}
		m_lastRead = m_buffer3;

		m_initWeight = -weightA;
		m_initRow = m_buffer3;
		m_buffer3 = m_buffer1;
		m_buffer1 = m_initRow;
		weightA += weightAadd;
	}
	while (weightA>0)
	{
		if (!pHortObj->ScaleRow(m_buffer3))
		{
			//end of data, copy last line read to buffer3 if needed
			if (m_lastRead != m_buffer3)
			{
				CopyDword(m_buffer3,m_lastRead,m_width);
			}
		}
		m_lastRead = m_buffer3;
		if (sumRow)
		{
			DWORD *src = m_buffer3;
			DWORD destLeft= m_width;
#ifdef NOASM
			DWORD *sumPtr = sumRow;
			do {*sumPtr++ += *src++;} while (--destLeft);
#else
			__asm
			{
				pushf
				cld
				mov		ecx,destLeft
				mov		esi,src
				mov		edi,sumRow
			sum1:
				lodsd
				add		[edi],eax
				add		edi,4
				loop	sum1

				popf
			}
#endif
		}
		else
		{
			sumRow = m_buffer3;
			m_buffer3 = m_buffer2;
			m_buffer2 = sumRow;
		}
		weightA-=scaleTo;
	}

	
	if (!pHortObj->ScaleRow(m_buffer3))
	{
		//end of data, copy last line read to buffer3 if needed
		if (m_lastRead != m_buffer3)
		{
			CopyDword(m_buffer3,m_lastRead,m_width);
		}
	}
	m_lastRead = m_buffer3;

	DWORD initWeight = m_initWeight;
	DWORD *lastPtr = m_buffer3;
	DWORD *sumPtr = sumRow;
	DWORD *initPtr = m_initRow;
	DWORD destLeft= m_width;
	if  (weightA)
	{
		DWORD lastWeight = (weightA+scaleTo);
		if (initPtr)
		{
			if (sumPtr)
			{
#ifdef NOASM
				do 
				{
					*sumPtr = ((*initPtr++) * initWeight) + ((*lastPtr++)* lastWeight) + ((*sumPtr) * scaleTo);
					sumPtr++;
				} while (--destLeft);
#else
				__asm
				{
					pushf
					cld
					mov		edi,sumPtr
					mov		esi,initPtr
					mov		ebx,lastPtr
				sum2:
					lodsd
					mul		initWeight
					mov		ecx,eax

					mov		eax,[ebx]
					mul		lastWeight
					add		ebx,4
					add		ecx,eax

					mov		eax,[edi]
					mul		scaleTo
					add		eax,ecx
					stosd
					dec		destLeft
					jnz		sum2

					popf
				}
#endif
				resultRow = sumRow;
			}
			else
			{
#ifdef NOASM
				do
				{
					*initPtr = ((*lastPtr++)* lastWeight) + ((*initPtr) * initWeight);
					initPtr++;
				} while (--destLeft);
#else
				__asm
				{
					pushf
					cld
					mov		edi,initPtr
					mov		esi,lastPtr
					mov		ecx,destLeft
				sum3:
					lodsd
					mul		lastWeight
					mov		ebx,eax

					mov		eax,[edi]
					mul		initWeight
					add		eax,ebx
					stosd
					loop	sum3

					popf
				}
#endif
				resultRow = m_initRow;
			}
		}
		else
		{
#ifdef NOASM
			do
			{
				*sumPtr = ((*lastPtr++)* lastWeight) + ((*sumPtr) * scaleTo);
				sumPtr++;
			} while (--destLeft);
#else
			__asm
			{
				pushf
				cld
				mov		edi,sumPtr
				mov		esi,lastPtr
				mov		ecx,destLeft
			sum4:
				lodsd
				mul		lastWeight
				mov		ebx,eax

				mov		eax,[edi]
				mul		scaleTo
				add		eax,ebx
				stosd
				loop	sum4

				popf
			}
#endif
			resultRow = sumRow;
		}
		m_initWeight = -weightA;
		m_initRow = m_buffer3;
		m_buffer3 = m_buffer1;
		m_buffer1 = m_initRow;
		m_weightA = weightA + weightAadd;
	}
	else
	{
		if (initPtr)
		{
			if (sumPtr)
			{
#ifdef NOASM
				do
				{
					*sumPtr = ((*initPtr++) * initWeight) + (((*lastPtr++) + (*sumPtr)) * scaleTo);
					sumPtr++;
				} while (--destLeft);
#else
				__asm
				{
					pushf
					cld
					mov		edi,sumPtr
					mov		esi,initPtr
					mov		ebx,lastPtr
				sum5:
					lodsd
					mul		initWeight
					mov		ecx,eax

					mov		eax,[ebx]
					add		ebx,4
					add		eax,[edi]
					mul		scaleTo
					add		eax,ecx

					stosd
					dec		destLeft
					jnz		sum5

					popf
				}
#endif
				resultRow = sumRow;
			}
			else
			{
#ifdef NOASM
				do
				{
					*initPtr = ((*lastPtr++)* scaleTo) + ((*initPtr) * initWeight);
					initPtr++;
				} while (--destLeft);
#else
				__asm
				{
					pushf
					cld
					mov		edi,initPtr
					mov		esi,lastPtr
					mov		ecx,destLeft
				sum6:
					lodsd
					mul		scaleTo
					mov		ebx,eax

					mov		eax,[edi]
					mul		initWeight
					add		eax,ebx
					stosd
					loop	sum6

					popf
				}
#endif
				resultRow = m_initRow;
			}
		}
		else
		{
#ifdef NOASM
			do
			{
				*sumPtr = ((*lastPtr++) + (*sumPtr)) * scaleTo;
				sumPtr++;
			} while (--destLeft);
#else
			__asm
			{
				pushf
				cld
				mov		edi,sumPtr
				mov		esi,lastPtr
				mov		ecx,destLeft
				mov		ebx,scaleTo
			sum7:
				lodsd
				add		eax,[edi]
				mul		ebx
				stosd
				loop	sum7

				popf
			}
#endif
			resultRow = sumRow;
		}
		m_initRow = NULL;
		m_initWeight = 0;
		m_weightA = weightAadd;
	}
	return resultRow;
}
