#include "bdGraph/ICommon.h"
#include "bdGraph/Scale.h"
#include "bdGraph/Scale16.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

//*******************************************************************************

class CDataObj : public IFileData
{
public:
	CDataObj();
	~CDataObj();

	BOOL GetData(const BYTE** ppBuffer,DWORD* bytesRead,DWORD maxNeeded=0);

	void Reset();
	void ResetBounds(int backup,int length);
	void Advance();
	BOOL Init(const char* fName);
	FILE* m_hFile;
	BYTE*  m_buffer;
};

CDataObj::CDataObj()
{
	m_hFile = NULL;
	m_buffer = NULL;
}

CDataObj::~CDataObj()
{
	if (m_hFile)
	{
		fclose(m_hFile);
		m_hFile=NULL;
	}
	if (m_buffer)
	{
		delete[] (m_buffer);
		m_buffer=NULL;
	}
}

#define BufferSize 4096
void CDataObj::Reset()
{
	if (m_hFile)
	{
		fseek(m_hFile,0L,SEEK_SET);
	}
}
BOOL CDataObj::Init(const char* fName)
{
	if (m_hFile)
	{
		fclose(m_hFile);
		m_hFile=NULL;
	}
	if (!m_buffer)
	{
		int i = BufferSize;
		m_buffer = new BYTE[i+4];
		if (!m_buffer) return FALSE;
		*((DWORD*)m_buffer) = 0x0ffffffff;
	}
//NOTE: !!!!!!!!!!!!!!
//first DWORD of buffer  is -1 so that if a marker flag
//is split between the read of two segments, no special checks are needed
//NOTE: !!!!!!!!!!!!!!

	m_hFile = ::fopen(fName,"r");
	if (m_hFile) return TRUE;
	return FALSE;
}

BOOL CDataObj::GetData(const BYTE** ppBuffer,DWORD* pBytesRead,DWORD maxNeeded)
{
	int cnt = BufferSize;
	if (maxNeeded>0) if (maxNeeded<BufferSize) cnt = maxNeeded;
	*ppBuffer = m_buffer+4;
	*pBytesRead = 0;
	if (m_hFile)
	{
		*pBytesRead = fread((m_buffer+4), 1,cnt,m_hFile);
	}
	if (*pBytesRead) return TRUE;
	return FALSE;
}
void CDataObj::ResetBounds(int backup,int length)
{
}
void CDataObj::Advance()
{
}

//*******************************************************************************
//*******************************************************************************

int main(int argc, char *argv[])
{
	unsigned short *fbMem = NULL;
	int fbWidth,fbHeight,memSize;
	int picWidth,picHeight;
	int left=0;
	int top=0;
	char* filename = argv[1];
	char* opts = argv[2];
	int flags=0;
	int stretch=0;

	if (argc == 3) {
		if ((*opts != '-') && (*filename == '-')) {
			filename = argv[2];
			opts = argv[1];
		}
		if (*opts == '-') {
			opts++;
			if ((*opts == 'r') || (*opts == 'R')) flags = 1;
			if ((*opts == 'b') || (*opts == 'B')) flags = 2;
			if ((*opts == 's') || (*opts == 'S')) stretch = 1;
			if (flags || stretch) argc = 2;
		}
    }
	if (argc != 2) {
		printf(" *** Usage ***\n");
		printf("./jpegview file [-r|-b|-s]\n");
		exit(1);
	}

	CDataObj data;
	data.Init(filename);
	Scale* pScaleObj = Scale::GetScalableImage(&data,flags);

//  pScaleObj->m_flags = flags;
	if (pScaleObj) if (pScaleObj->GetDimensions(&picWidth,&picHeight)) {
		int fbDev = open( "/dev/fb0", O_RDWR );
		if (fbDev) {
			struct fb_fix_screeninfo fixed_info;
			int err = ioctl( fbDev, FBIOGET_FSCREENINFO, &fixed_info);
			if( 0 == err ) {
				struct fb_var_screeninfo variable_info;
				err = ioctl( fbDev, FBIOGET_VSCREENINFO, &variable_info );
				if( 0 == err ) {
					fbWidth   = variable_info.xres ;
					fbHeight  = variable_info.yres ;
					memSize = fixed_info.smem_len ;
					printf("screen width:%d, Screen height:%d\n",fbWidth,fbHeight);
					printf("picture width:%d, picture height:%d\n",picWidth,picHeight);

					BYTE *pDib = NULL;
					if (stretch) pDib = pScaleObj->GetDibBits(fbWidth,fbHeight,	0,0,fbWidth,fbHeight,		0,0,0,0);
					else 		 pDib = pScaleObj->GetDibBits(picWidth,picHeight,	0,0,picWidth,picHeight,		0,0,0,0);
					if (pDib) {
						fbMem = (unsigned short *) mmap( 0, memSize, PROT_WRITE, MAP_SHARED, fbDev, 0 );
						if (fbMem) {
							if (stretch) {
//								ResourceView::RenderStretch(fbMem,fbWidth,fbHeight,flags,resource,length);
								Scale16::render(fbMem,fbWidth,fbHeight,0,0,pDib,fbWidth,fbHeight,0,0,fbWidth,fbHeight);
							} else {
								left = (fbWidth-picWidth)>>1;
								top = (fbHeight-picHeight)>>1;
//								ResourceView::RenderCenter(fbMem,fbWidth,fbHeight,flags,resource,length);
								Scale16::render(fbMem,fbWidth,fbHeight,left,top,pDib,picWidth,picHeight,0,0,picWidth,picHeight);
//  Scale16::scale( (unsigned short *)fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
//  std::scale16( (unsigned short *)fbMem, fbWidth, fbHeight,(unsigned short *)pDib, picWidth,picHeight, 0,0,picWidth,picHeight);
							}
				    	}
					}
				}
			}
			close(fbDev);
		}
	}
	exit(1);
}
