#include "ICommon.h"
#include "SImageDataJpeg.h"
#include "SImagePosition.h"

/////////////////////////////////////////////////////////////////////////////

SImageDataJpeg::SImageDataJpeg(DWORD hortMCUs,DWORD mcuRowCnt,DWORD samplesPerLine,DWORD numberOfLines,BYTE f0,BYTE f1,BYTE f2)
{
	m_cRef = 1;
	m_hortMCUs = hortMCUs;
	m_mcuRowCnt = mcuRowCnt;
	m_samplesPerLine = samplesPerLine;
	m_numberOfLines = numberOfLines;
	m_compSamplingFactors[0] = f0;
	m_compSamplingFactors[1] = f1;
	m_compSamplingFactors[2] = f2;
	m_yCbCrSamples[0] = NULL;
	m_yCbCrSamples[1] = NULL;
	m_yCbCrSamples[2] = NULL;
}

SImageDataJpeg::~SImageDataJpeg()
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

ImagePosition* SImageDataJpeg::GetImagePosition()
{
	ImagePosition* p = new SImagePosition(this);
	if (p) AddRef();
	return p;
}

int SImageDataJpeg::GetRow(ImagePosition* pSample,int colorIndex,int parm)
{
	if (parm >=0)
	{
		BYTE factor = m_compSamplingFactors[colorIndex];
		BYTE hFact = (factor>>4);
		BYTE vFact = (factor&15);
		pSample->SetPosition(m_yCbCrSamples[colorIndex],(vFact*m_mcuRowCnt<<3),0,(hFact*m_hortMCUs<<3) );
	}
	else
	{
		pSample->SetPosition(NULL,0,-1,0);
	}
	return -1;
}

int SImageDataJpeg::Reset(SRFact* pFact,int colorIndex)
{
	int i = 0;
	BYTE hMax=1;
	BYTE vMax=1;
	BYTE hFact,vFact, factor;
	while (i<3)
	{
		factor = m_compSamplingFactors[i];
		hFact=(factor>>4);
		vFact=(factor&15);
		if (hMax<hFact) hMax=hFact;
		if (vMax<vFact) vMax=vFact;
		i++;
	}
	factor = m_compSamplingFactors[colorIndex];
	hFact = (factor>>4);
	vFact = (factor&15);

	pFact->hFact = hFact;
	pFact->vFact = vFact;
	pFact->hMax = hMax;
	pFact->vMax = vMax;
	return 0;
}
void SImageDataJpeg::Close()
{
}

BOOL SImageDataJpeg::GetDimensions(int* pWidth, int* pHeight)
{
	if (pWidth) *pWidth = m_samplesPerLine;
	if (pHeight) *pHeight = m_numberOfLines;
	if (m_samplesPerLine && m_numberOfLines) return TRUE;
	return FALSE;
}

void SImageDataJpeg::Release()
{
	if (!(--m_cRef)) delete this;
}
void SImageDataJpeg::AddRef()
{
	m_cRef++;
}

