#include "ICommon.h"
#include "SImageData16.h"
#include "SImagePosition.h"


SImageData16::SImageData16(DWORD width,DWORD height,const unsigned short* pix)
{
	m_cRef = 1;
	m_width = width;
	m_height = height;
	m_pix = pix;
	m_pixEnd = pix +(width*height);
	m_tempBuf = NULL;
}

SImageData16::~SImageData16()
{
	BYTE* p = m_tempBuf;
	if (p)
	{
		m_tempBuf = NULL;
		delete[] p;
	}
}

ImagePosition* SImageData16::GetImagePosition()
{
	ImagePosition* p = new SImagePosition(this);
	if (p) AddRef();	//they keep a reference to us
	return p;
}
int SImageData16::GetRow(ImagePosition* pSample,int colorIndex,int parm)
{
	const unsigned short* rr = reinterpret_cast<const unsigned short*>(parm);
	if (rr< m_pixEnd) {
		int i = m_width;
		BYTE* p = m_tempBuf;
		if (colorIndex==0) {
			do {
				*p++ = ((*rr++) << 3);
			} while (--i);
		} else if (colorIndex==1) {
			do {
				*p++ = ((*rr++) >> 3) & 0xfc;
			} while (--i);
		} else {
			do {
				*p++ = ((*rr++) >> 8) & 0xf8;
			} while (--i);
		}
		pSample->SetPosition(m_tempBuf,1,0,m_width);
		return reinterpret_cast<int>(rr);
	}
	pSample->SetPosition(NULL,0,0xffffffff,0);
	return parm;
}

int SImageData16::Reset(SRFact* pFact,int colorIndex)
{
	pFact->hFact = 1;
	pFact->vFact = 1;
	pFact->hMax = 1;
	pFact->vMax = 1;
	if (!m_tempBuf) m_tempBuf = (new BYTE[m_width]);
	return reinterpret_cast<int>(m_pix);
}
void SImageData16::Close()
{
}

BOOL SImageData16::GetDimensions(int* pWidth, int* pHeight)
{
	if (pWidth) *pWidth = m_width;
	if (pHeight) *pHeight = m_height;
	if (m_width && m_height) return TRUE;
	return FALSE;
}

void SImageData16::Release()
{
	if (!(--m_cRef)) delete this;
}
void SImageData16::AddRef()
{
	m_cRef++;
}

