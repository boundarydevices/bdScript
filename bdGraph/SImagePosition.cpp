#include "ICommon.h"
#include "SImagePosition.h"


SImagePosition::SImagePosition(ImageData* imageData) : m_cRef(1)
{
	m_imageData = imageData;
}

SImagePosition::~SImagePosition()
{
	if (m_imageData) m_imageData->Release();
}
void SImagePosition::Release()
{
	if (!(--m_cRef)) delete this;
}
void SImagePosition::AddRef()
{
	m_cRef++;
}
void SImagePosition::GetRow()
{
	sampleRowsLeft = 0;
	if (m_imageData) m_parm = m_imageData->GetRow(this,m_colorIndex,m_parm);
}
BOOL SImagePosition::Reset(int colorIndex)
{
	m_colorIndex = colorIndex;
	sampleRowPos = NULL;
	sampleRowsLeft = 0;
	sampleRepeat = 0;
	m_withinRow = 0;
	m_f.hFact = 0;
	if (m_imageData) m_parm = m_imageData->Reset(&m_f,colorIndex);
	return (m_f.hFact!=0);
}
void SImagePosition::Close()
{
	if (m_imageData) m_imageData->Close();
}
BOOL SImagePosition::GetDimensions(int* pWidth,int* pHeight)
{
	if (m_imageData) return m_imageData->GetDimensions(pWidth,pHeight);
	return FALSE;
}
const SRFact* SImagePosition::GetSRFact()
{
	return &m_f;
}
void SImagePosition::SetPosition(const BYTE* sRowPos,DWORD sRowsLeft,DWORD sRepeat,DWORD sRowAdvance)
{
	sampleRowPos = sRowPos;
	sampleRowsLeft = sRowsLeft;
	sampleRepeat = sRepeat;
	rowAdvance = sRowAdvance;
}

/////////////////////////////////////////////////

void SImagePosition::Advance(DWORD picLeft,DWORD picTop)
{
	picLeft /= m_f.hMax;
	picTop /= m_f.vMax;
	AdvanceRows(picTop*m_f.vFact);
	AdvanceWithinRow(picLeft*m_f.hFact);
}

const BYTE* SImagePosition::GetNextRow()
{
	if (!sampleRowsLeft)
	{
		if (sampleRepeat) {sampleRepeat--; return NULL;}

		GetRow();
		if (!sampleRowsLeft) return NULL;
		sampleRowPos += m_withinRow;
	}
	sampleRowsLeft--;
	const BYTE* p = sampleRowPos;
	sampleRowPos += rowAdvance;
	return p;
}

void SImagePosition::AdvanceRows(DWORD n)
{
	do
	{
		if (sampleRowsLeft>n) {sampleRowPos += rowAdvance*n; sampleRowsLeft-=n; return;}
		n -= sampleRowsLeft;

		if (sampleRepeat>n)
		{
			sampleRepeat-=n;
			sampleRowPos += rowAdvance*(sampleRowsLeft-1);
			sampleRowsLeft = 1;
			sampleRepeat--;
			return;
		}
		n -= sampleRepeat;
		if (n)
		{
			GetRow();
			sampleRowPos += m_withinRow;
		}
		else
		{
			sampleRepeat = 0;
			sampleRowsLeft = 0;
			return;
		}
	} while (sampleRowsLeft);
}

void SImagePosition::AdvanceWithinRow(DWORD n)
{
	m_withinRow += n;
	sampleRowPos += n;
}
