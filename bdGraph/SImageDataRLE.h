//
//comment next line to remove from program
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif
interface ImagePosition;

class SImageDataRLE : public ImageData
{
public:
	SImageDataRLE(DWORD width,DWORD height,DWORD maxSize);
	~SImageDataRLE();
//ImageData
	ImagePosition* GetImagePosition();
	int GetRow(ImagePosition* pSample,int colorIndex,int parm);
	int Reset(SRFact* pFact,int colorIndex);
	void Close();
	BOOL GetDimensions(int* pWidth,int* pHeight);
	void Release();
	void AddRef();
private:
	DWORD	m_cRef;
	DWORD	m_width;
	DWORD	m_height;
	DWORD	m_maxSize;
public:
	BYTE*	m_yCbCrSamples[3];
};
