//
//comment next line to remove from program
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif
interface ImagePosition;

class SImageData16 : public ImageData
{
public:
	SImageData16(DWORD width,DWORD height,const unsigned short* pix);
	~SImageData16();
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
	const unsigned short*	m_pix;
	const unsigned short*	m_pixEnd;
	BYTE* m_tempBuf;
};
