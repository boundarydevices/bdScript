// Scale.cpp : implementation file
//
//
#include "ICommon.h"
#include "ScaleUpObj.h"
#include "Scale.h"
#include "ScaleRGB.h"
#include "ScaleYCbCr.h"
#include "ScaleYCbCrf1.h"
#include "ScaleYCbCrf2.h"
#include "ScaleY.h"
#include "ScaleYf1.h"
#include "ScaleYf2.h"
#include "SRle.h"
#include "SJpeg.h"
#include "SImagePosition.h"
#include "SImagePositionBottom.h"

extern "C" unsigned long uDivRem(unsigned long value,unsigned long divisor,unsigned long * rem);
//return (1<<32)/value and remainder
extern "C" unsigned long uReciprocal(unsigned long value,unsigned long * rem);
//#define _LCD
/////////////////////////////////////////////////////////////////////////////

DWORD Scale::GetMult(DWORD val,DWORD cDiv)
{
	DWORD result;
#ifdef NOASM
#if 0
	result = val/cDiv;
	DWORD rem = val%cDiv;
#else
	DWORD rem;
	result = uDivRem(val,cDiv,&rem);
#endif
	rem += rem;
	if (rem>cDiv) {result++;}
#else
	__asm
	{
		xor		edx,edx
		mov		eax,val
		mov		ecx,cDiv

		div		ecx
		add		edx,edx
		cmp		ecx,edx
		adc		eax,0
		sbb		eax,0
		mov		result,eax
	}
#endif
	return result;

}

//about 2.5% of time
//BOOL tBitBlt( HDC hdcDest, int nXDest, int nYDest, int nWidth, int nHeight, HDC hdcSrc, 
//	int nXSrc, int nYSrc, DWORD dwRop ) 
//{
//	return ::BitBlt(hdcDest, nXDest, nYDest, nWidth, nHeight,hdcSrc, nXSrc, nYSrc, dwRop);
//}

DWORD Scale::GetReciprocal(DWORD yDiv)
{
	DWORD result;
	if (yDiv>1)
	{
#ifdef NOASM
#if 0
		ULONGLONG u = static_cast<ULONGLONG>(1)<<32;
		result = static_cast<DWORD>(u/yDiv);
		DWORD rem = static_cast<DWORD>(u % yDiv);
#else
		DWORD rem;
		result = uReciprocal(yDiv,&rem);
#endif
		rem += rem;
		if (rem>yDiv) result++;
#else
		__asm
		{
			mov		edx,1
			xor		eax,eax
			mov		ecx,yDiv
			div		ecx
			add		edx,edx
			cmp		ecx,edx
			adc		eax,0
			mov		result,eax
		}
#endif
	}
	else
	{
		result = 0;
	}
	return result;

}

/////
//drawXXX specifies how the bitmap is scaled
//srcXXX specifies the portion of the scaled bitmap wanted
//
//picXXX specifiies the portion of bitmap used to construct the image
//picLeft && picTop are usually 0
//picWidth && picHeight are usually 0 meaning set to samplesPerLine && numberOfLines
//Just a part of the bitmap is used if a subImage is to be drawn (like shrink function)
//
//inDibBits is usually 0 which means to allocate space which will be returned by the function
//if (!inDibBits) inDibWidth specifies the width of the previously allocated bitmap
//

BYTE* Scale::GetDibBits(DWORD drawWidth,const DWORD drawHeight,
		DWORD srcLeft,const DWORD srcTop,DWORD srcWidth,const DWORD srcHeight,
		DWORD picLeft, DWORD picTop, DWORD picWidth,DWORD picHeight,
		BYTE* inDibBits,DWORD inDibWidth)
{
	if (!m_imageData) return NULL;

	DWORD width,height;
	m_imageData->GetDimensions((int*)&width,(int*)&height);
	if (!picWidth) picWidth = width;
	if (!picHeight) picHeight = height;
	SRFact rf;
	m_imageData->Reset(&rf,0);
	DWORD rem;
	uDivRem(picLeft,rf.hMax,&rem);
	picLeft -= rem;
	picWidth +=rem;

	uDivRem(picTop,rf.vMax,&rem);
	picTop -= rem;
	picHeight +=rem;

	if (picWidth>(width-picLeft)) picWidth = (width-picLeft);
	if (picHeight>(height-picTop)) picHeight = (height-picTop);


	DWORD rowWidth;
	BYTE* dibBits=NULL;

	if (inDibBits)
	{
		rowWidth = inDibWidth;
	}
	else
	{
		rowWidth = (((srcWidth*3)+3)& ~3);
		DWORD nSizeImage = rowWidth*srcHeight;
		dibBits = (new BYTE[nSizeImage]);
		if (!dibBits) return NULL;
		inDibBits = dibBits;
	}
	

	if (IsMulWidth3())
	{
		drawWidth += (drawWidth<<1);
		srcLeft += (srcLeft<<1);
		srcWidth += (srcWidth<<1);
	}

	ImagePosition*  imagePos[3];
	imagePos[0] = NULL;
	imagePos[1] = NULL;
	imagePos[2] = NULL;
	ScaleUpObj	upObj[3];
	int i = 0;
	while (i<3)
	{
		imagePos[i] = m_imageData->GetImagePosition();
		if (!imagePos[i]->Reset(i)) break;
		imagePos[i]->Advance(picLeft,picTop);
		if (!upObj[i].Init(imagePos[i],picWidth,picHeight,drawWidth,drawHeight,  srcLeft,srcTop,srcWidth))
		{
			if (dibBits) delete[] dibBits;
			return NULL;
		}
		i++;
	}

	CreateBitmap(&upObj[0],&upObj[1],&upObj[2],srcWidth,srcHeight,inDibBits,rowWidth);
	if (imagePos[0]) imagePos[0]->Release();
	if (imagePos[1]) imagePos[1]->Release();
	if (imagePos[2]) imagePos[2]->Release();
	return inDibBits;
}


Scale::Scale(ImageData* imageData) : m_cRef(1),m_imageData(imageData)
{
}

Scale::~Scale()
{
	if (m_imageData) {m_imageData->Release(); m_imageData = NULL;}
}
ULSTD Scale::AddRef ()
{
    return ++m_cRef;
}

ULSTD Scale::Release ()
{
	DWORD i = --m_cRef;
    if ( i== 0)
	{
		delete this;
	}
    return i;
}

class CImageDataBottom : public ImageData
{
public:
	CImageDataBottom(ImageData* imageData) : m_cRef(1),m_imageData(imageData) {}
	~CImageDataBottom()
	{
		if (m_imageData) {m_imageData->Release(); m_imageData = NULL;}
	}
private:

	//ImageData
public:
	void Release() { if ((--m_cRef)==0) {delete this;} }
	void AddRef() {++m_cRef;}
	int GetRow(ImagePosition* pSample,int colorIndex,int parm) {return m_imageData->GetRow(pSample,colorIndex,parm);}
	int Reset(SRFact* pFact,int colorIndex) {return m_imageData->Reset(pFact,colorIndex);}
	void Close() {m_imageData->Close();}
	BOOL GetDimensions(int* pWidth,int* pHeight) {return m_imageData->GetDimensions(pWidth,pHeight);}
	ImagePosition* GetImagePosition();
private:
	volatile ULONG m_cRef;
	ImageData* m_imageData;
};

ImagePosition* CImageDataBottom::GetImagePosition()
{
	ImagePosition* p = NULL;
	if (m_imageData)
	{
		p = new SImagePositionBottom(m_imageData);
		if (p) m_imageData->AddRef();
	}
	return p;
}

Scale* Scale::GetBottom()
{
	ImageData* pBase = m_imageData;
	Scale* pScale = NULL;
	if (pBase)
	{
		CImageDataBottom* pI = new CImageDataBottom(pBase);
		if (pI)
		{
			pBase->AddRef();
			pScale = GetSimilar(pI);
			if (pScale == NULL) pI->Release();
		}
	}
	return pScale;
}


//rle or jpeg
Scale* Scale::GetScalableImage(IFileData* data,int flags)
{
	Scale* pScale = NULL;
	const BYTE* buf=NULL;
	DWORD bytesRead=0;
	if (data->GetData(&buf,&bytesRead))
	{
		BYTE b0 = buf[0];
		BYTE b1 = 0;
		if (bytesRead>1) b1 = buf[1];
		else
		{
			if (data->GetData(&buf,&bytesRead)) b1 = buf[0];
		}
		data->Reset();

		if ((b0 == 0xf5) && (b1== 0xc0))
		{
			SRle pic;
			ImageData* pImageData = pic.LoadYCbCr(data);
			if (pImageData)
			{
//				if (flags==0)
					pScale = new ScaleRGB(pImageData);
//				else
//				{
//					if (flags==2) pScale = new ScaleRGBf2(pImageData);
//					else pScale = new ScaleRGBf1(pImageData);
//				}
				if (pScale==NULL) pImageData->Release();
			}
		}
		else
		{
			SJpeg pic;
			ImageData* pImageData = pic.LoadYCbCr(data);
			if (pImageData)
			{
				SRFact rf;
				pImageData->Reset(&rf,2);
				if (rf.hFact)
				{
#ifdef _LCD
					if (flags==0)
						pScale = new ScaleYCbCr(pImageData);
					else
					{
						if (flags==2) pScale = new ScaleYCbCrf2(pImageData);
						else pScale = new ScaleYCbCrf1(pImageData);
					}
#else
					pScale = new ScaleYCbCr(pImageData);
#endif
				}
				else
				{
					//lcd display of Black and White JPEG image
#ifdef _LCD
					if (flags==0)
						pScale = new ScaleY(pImageData);
					else
					{
						if (flags==2) pScale = new ScaleYf2(pImageData);			//BGR order
						else pScale = new ScaleYf1(pImageData);
					}
#else
					pScale = new ScaleY(pImageData);
#endif
				}
				if (pScale==NULL) pImageData->Release();
			}
		}
	}
	return pScale;
}

BOOL Scale::GetDimensions(int* pWidth, int* pHeight)
{
	if (m_imageData) return m_imageData->GetDimensions(pWidth,pHeight);
	*pWidth = 0;
	*pHeight = 0;
	return FALSE;
}



