// scaleUpObj.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif

interface ImagePosition;

class ScaleHortObj;
interface IScaleVertObj;

class ScaleUpObj
{
public:
	BOOL Init(ImagePosition* pSample,DWORD picWidth,DWORD picHeight,DWORD drawWidth,DWORD drawHeight,
			DWORD outLeft,DWORD outTop,DWORD outWidth);
	DWORD* SampleRow();
	DWORD GetDiv();
	ScaleUpObj();
	~ScaleUpObj();
private:
	ScaleHortObj	*m_hortObj;
	IScaleVertObj	*m_vertObj;
	DWORD		m_div;
};

