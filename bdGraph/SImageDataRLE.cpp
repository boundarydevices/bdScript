#include "ICommon.h"
#include "SImageDataRLE.h"
#include "SImagePosition.h"

typedef struct tagRowRepeat
{
	const BYTE* rowPos;
	DWORD rowsLeft;
	DWORD repeat;
} RowRepeat;


SImageDataRLE::SImageDataRLE(DWORD width,DWORD height,DWORD maxSize)
{
	m_cRef = 1;
	m_width = width;
	m_height = height;
	m_maxSize = maxSize;
	m_yCbCrSamples[0] = NULL;
	m_yCbCrSamples[1] = NULL;
	m_yCbCrSamples[2] = NULL;
}

SImageDataRLE::~SImageDataRLE()
{
	int i = 0;
	while (i<3)
	{
		BYTE* p = m_yCbCrSamples[i];
		if (p)
		{
			m_yCbCrSamples[i] = NULL;
			delete[] p;
		}
		i++;
	}
}

ImagePosition* SImageDataRLE::GetImagePosition()
{
	ImagePosition* p = new SImagePosition(this);
	if (p) AddRef();
	return p;
}

int SImageDataRLE::GetRow(ImagePosition* pSample,int colorIndex,int parm)
{
	RowRepeat* rr = reinterpret_cast<RowRepeat*>(parm);

	const BYTE* pB = rr->rowPos;
	if (pB)
	{
		pSample->SetPosition(pB,rr->rowsLeft,rr->repeat,m_width);
		return (int)(rr-1);
	}
	pSample->SetPosition(NULL,0,0xffffffff,0);
	return parm;
}

int SImageDataRLE::Reset(SRFact* pFact,int colorIndex)
{
	pFact->hFact = 1;
	pFact->vFact = 1;
	pFact->hMax = 1;
	pFact->vMax = 1;
	return reinterpret_cast<int>((reinterpret_cast<RowRepeat*>(m_yCbCrSamples[colorIndex]+m_maxSize))-1);
}
void SImageDataRLE::Close()
{
}

BOOL SImageDataRLE::GetDimensions(int* pWidth, int* pHeight)
{
	if (pWidth) *pWidth = m_width;
	if (pHeight) *pHeight = m_height;
	if (m_width && m_height) return TRUE;
	return FALSE;
}

void SImageDataRLE::Release()
{
	if (!(--m_cRef)) delete this;
}
void SImageDataRLE::AddRef()
{
	m_cRef++;
}

