// CScale.cpp : implementation file
// CScale
//
#include "ICommon.h"
#include "ScaleHortObj.h"
#include "ScaleVertObj.h"
#include "ScaleUpObj.h"

//upcode		! is starting point for edges
//1		1 -> 1  !next
//2		1 -> 2  (3a+b)/4, !(a+3b)/4,						!a=b, a=next
//3		1 -> 3  (2a+b)/3, !(a+2b)/3, b,						!a=b, a=next
//4		1 -> 4  (7a+b)/8, (5a+3b)/8, !(3a+5b)/8, (a+7b)/8,	!a=b, b=next
//1		2 -> 2  !next
//5		2 -> 3  (a+b)/2, (5b+c)/6, !(b+5c)/6,				a=c, !b=next, c=next
//2		2 -> 4 = 1 -> 2 = (3a+b)/4, (a+3b)/4
//1		3 -> 3  !next
//6		3 -> 4  (3a+5b)/8, (5b+3c)/8, (7c+d)/8, !(c+7d)/8,	a=d, b=next,c=next,!d=next
//0		4 -> 4  !next

//unoptimized
//		1 -> 2  (3a+b)/4, !(a+3b)/4,						!a=b, a=next
//		1 -> 3  (4a+2b)/6, !(2a+4b)/6, 6b/6				!a=b, a=next
//		2 -> 3  (3a+3b)/6, (5b+c)/6, !(b+5c)/6,			a=c, !b=next, c=next
//		1 -> 4  (7a+b)/8, (5a+3b)/8, !(3a+5b)/8, (a+7b)/8,!a=b, b=next
//		2 -> 4  (6a+2b)/8, (2a+6b)/8, (6b+2c)/8, !(2b+6c)/8, a=c, b=next, !c=next,
//		3 -> 4  (3a+5b)/8, (5b+3c)/8, (7c+d)/8, !(c+7d)/8, a=d, b=next,c=next,!d=next
const signed char code[16]= {    1,    2,    3,    4,     -2,    1,    5,    2,     -3,   -5,    1,    6,     -4,   -2,   -6,    1 };
								//1->1, 1->2, 1->3, 1->4    2->1, 2->2, 2->3, 2->4,   3->1, 3->2, 3->3, 3->4,   4->1, 4->2, 4->3, 4->4
int UpCode(DWORD scaleFrom,DWORD scaleTo)
{
	scaleFrom--;
	scaleTo--;
	if ((scaleFrom<=3) && (scaleTo<=3))
	{
			return code[(scaleFrom<<2)+scaleTo];
	}
	if (scaleFrom<=scaleTo)
	{
		return 100;
	}
	return -100;
}

ScaleUpObj::~ScaleUpObj()
{
	if (m_vertObj) { m_vertObj->Release(); m_vertObj = NULL;} //just an interface
	if (m_hortObj) { m_hortObj->Release(); m_hortObj = NULL;}
}


ScaleUpObj::ScaleUpObj()
{
	m_hortObj = NULL;
	m_vertObj = NULL;
	m_div = 1;
}

DWORD gcm(DWORD a, DWORD b)
{
	//find greatest common multiple
	while (TRUE)
	{
		b %= a;
		if (b==0)return a;
		a %= b;
		if (a==0)return b;
	}
}

BOOL ScaleUpObj::Init(ImagePosition* pSample,DWORD picWidth,DWORD picHeight,DWORD drawWidth,DWORD drawHeight,
		DWORD outLeft,DWORD outTop,DWORD outWidth)
{
	DWORD gf;
	const SRFact* pRF = pSample->GetSRFact();

	DWORD hScaleFrom = picWidth*pRF->hFact;
	DWORD hScaleTo = drawWidth*pRF->hMax;
	gf = gcm(hScaleFrom,hScaleTo);
	hScaleFrom /= gf;
	hScaleTo /= gf;

	DWORD vScaleFrom = picHeight*pRF->vFact;
	DWORD vScaleTo = drawHeight*pRF->vMax;
	gf = gcm(vScaleFrom,vScaleTo);
	vScaleFrom /= gf;
	vScaleTo /= gf;

	int hUpCode = UpCode(hScaleFrom,hScaleTo);
	int vUpCode = UpCode(vScaleFrom,vScaleTo);
	if (pSample && hUpCode && vUpCode)
	{
		DWORD hDiv = hScaleTo<<1;
		if (!m_hortObj)
		{
			m_hortObj = ScaleHortObj::GetObject(hUpCode,hScaleFrom,hScaleTo,&hDiv);
		}
		if (!m_vertObj)
		{
			m_vertObj = ScaleVertObj::GetObject(vUpCode,vScaleFrom,vScaleTo,&hDiv);
			m_div = hDiv;
		}
		if (m_hortObj && m_vertObj)
		{
			DWORD relaventWidth=0;
			int vInitWeight = vScaleTo-vScaleFrom;
			int hInitWeight = hScaleTo-hScaleFrom;
			if (vInitWeight>0)
			{
				int t = (vScaleFrom<<1)*outTop -vInitWeight;
				if (t>0)
				{
					DWORD n = ((DWORD)t)/(vScaleTo<<1);
					t -= (vScaleTo<<1)*n;
					pSample->AdvanceRows(n);
				}
				vInitWeight = -t;
			}
			else
			{
				vInitWeight = vScaleFrom*outTop;
				DWORD n = ((DWORD)vInitWeight)/vScaleTo;
				vInitWeight -= vScaleTo*n;
				pSample->AdvanceRows(n);
				if (vInitWeight==0)
				{
					vInitWeight = vScaleFrom;
				}
				vInitWeight-=vScaleTo;
			}
			if (!(m_vertObj->Init(outWidth,vInitWeight))) {return FALSE;}


			if (hInitWeight>0)
			{
				int t = (hScaleFrom<<1)*outLeft -hInitWeight;
				DWORD n =0;
				if (t>0)
				{
					n = ((DWORD)t)/(hScaleTo<<1);
					t -= (hScaleTo<<1)*n;
					pSample->AdvanceWithinRow(n);
				}

				int tt = (hScaleFrom<<1)*(outLeft+outWidth) -hInitWeight;
				DWORD m=1;
				if (tt>0)
				{
					m = ((DWORD)tt)/(hScaleTo<<1);
					tt -= (hScaleTo<<1)*m;
					if (m<picWidth) m++;
				}
				hInitWeight = -t;
				relaventWidth = m-n;
			}
			else
			{
				hInitWeight = hScaleFrom*outLeft;
				DWORD n = ((DWORD)hInitWeight)/hScaleTo;
				hInitWeight -= hScaleTo*n;
				pSample->AdvanceWithinRow(n);
				if (hInitWeight==0)
				{
					hInitWeight = hScaleFrom;
				}
				hInitWeight-=hScaleTo;

				DWORD t = hScaleFrom*(outLeft+outWidth);
				DWORD m = t/hScaleTo;
				t -= hScaleTo*m;
				if (t!=0)
				{
					m++;
				}
				relaventWidth = m-n;
			}
			if (m_hortObj->Init(pSample,relaventWidth,outWidth,hScaleFrom,hScaleTo,hInitWeight))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}


DWORD* ScaleUpObj::SampleRow()
{
	return m_vertObj->ScaleRow(m_hortObj);
}
DWORD ScaleUpObj::GetDiv()
{
	return m_div;
}
