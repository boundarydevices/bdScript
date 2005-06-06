#include "ICommon.h"
#include "Scale.h"
#include "Scale16.h"
#include "Scale18.h"
#include "ResourceView.h"
//#include <unistd.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include <fcntl.h>
//#include <linux/fb.h>
//#include <linux/videodev.h>
//#include <sys/ioctl.h>
//#include <sys/mman.h>

//*******************************************************************************

class CResourceObj : public IFileData
{
public:
	CResourceObj();
	~CResourceObj();

	BOOL GetData(const BYTE** ppBuffer,DWORD* bytesRead,DWORD maxNeeded=0);

	void Reset();
	void ResetBounds(int backup,int length);
	void Advance();
	BOOL Init(const BYTE* data,DWORD length);
	const BYTE* m_data;
	DWORD  m_length;
	DWORD  m_pos;
};

CResourceObj::CResourceObj()
{
	m_data = NULL;
	m_length = 0;
	m_pos = 0;
}

CResourceObj::~CResourceObj()
{
}

void CResourceObj::Reset()
{
	m_pos = 0;
}

BOOL CResourceObj::Init(const BYTE* data,DWORD length)
{
	m_data = data;
	m_length = length;
	m_pos = 0;
	return (m_data && length);
}

BOOL CResourceObj::GetData(const BYTE** ppBuffer,DWORD* pBytesRead,DWORD maxNeeded)
{
	DWORD cnt = m_length-m_pos;
	if (maxNeeded>0) if (maxNeeded<cnt) cnt = maxNeeded;
	*ppBuffer = m_data+m_pos;
	*pBytesRead = cnt;
	m_pos += cnt;
	return (cnt);
}
void CResourceObj::ResetBounds(int backup,int length)
{
}
void CResourceObj::Advance()
{
}

//*******************************************************************************
//*******************************************************************************
ResourceView::ResourceView()
{
}
void ResourceView::RenderCenter(unsigned short *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length)
{
    int picWidth,picHeight;
    int left,top;


    CResourceObj data;
    data.Init(resource,length);
    Scale* pScaleObj = Scale::GetScalableImage(&data,flags);

//  pScaleObj->m_flags = flags;
    if (pScaleObj)
    {
		if (pScaleObj->GetDimensions(&picWidth,&picHeight))
		{
			BYTE *pDib = NULL;
			if (pDib= pScaleObj->GetDibBits(picWidth,picHeight,
					0,0,picWidth,picHeight,
					0,0,0,0))
			{
				left = (fbWidth-picWidth)>>1;
				top = (fbHeight-picHeight)>>1;
				Scale16::render(fbMem,fbWidth,fbHeight,left,top,pDib,picWidth,picHeight,0,0,picWidth,picHeight);
//	Scale16::scale( fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
//  std::scale16( fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
				delete[] pDib;
			}
		}
	}
}

void ResourceView::RenderStretch(unsigned short *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length)
{
    CResourceObj data;
    data.Init(resource,length);
    Scale* pScaleObj = Scale::GetScalableImage(&data,flags);

//  pScaleObj->m_flags = flags;
    if (pScaleObj)
    {
		BYTE *pDib = NULL;
		if (pDib= pScaleObj->GetDibBits(fbWidth,fbHeight,
				0,0,fbWidth,fbHeight,
				0,0,0,0))
		{
			Scale16::render(fbMem,fbWidth,fbHeight,0,0,pDib,fbWidth,fbHeight,0,0,fbWidth,fbHeight);
//	Scale16::scale( fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
//  std::scale16( fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
			delete[] pDib;
		}
	}
}
void ResourceView::RenderStretch18(unsigned char *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length)
{
    CResourceObj data;
    data.Init(resource,length);
    Scale* pScaleObj = Scale::GetScalableImage(&data,flags);

//  pScaleObj->m_flags = flags;
    if (pScaleObj)
    {
		BYTE *pDib = NULL;
		if (pDib= pScaleObj->GetDibBits(fbWidth,fbHeight,
				0,0,fbWidth,fbHeight,
				0,0,0,0))
		{
			Scale18::render(fbMem,fbWidth,fbHeight,0,0,pDib,fbWidth,fbHeight,0,0,fbWidth,fbHeight);
//	Scale16::scale( fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
//  std::scale16( fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
			delete[] pDib;
		}
	}
}

void ResourceView::GetData16(const unsigned char* resource, unsigned int length,
	unsigned short **pfbMem,int* ppicWidth,int* ppicHeight,
	ConvertRgb24Line_t convertLineFunc)
{
	int flags=0;
	unsigned short* fbMem = NULL;
	int picWidth=0;
	int picHeight=0;
    CResourceObj data;
    data.Init(resource,length);
    Scale* pScaleObj = Scale::GetScalableImage(&data,flags);

    if (pfbMem) if (pScaleObj)
    {
		if (pScaleObj->GetDimensions(&picWidth,&picHeight))
		{
			BYTE *pDib = NULL;
			if (pDib= pScaleObj->GetDibBits(picWidth,picHeight,
					0,0,picWidth,picHeight,
					0,0,0,0))
			{
				fbMem = new unsigned short[picWidth*picHeight];
				Scale16::render(fbMem,picWidth,picHeight,0,0,pDib,picWidth,picHeight,0,0,picWidth,picHeight,convertLineFunc);
				delete[] pDib;
			}
		}
	}
	if (pfbMem) *pfbMem = fbMem;
	if (ppicWidth) *ppicWidth = picWidth;
	if (ppicHeight) *ppicHeight = picHeight;
}
