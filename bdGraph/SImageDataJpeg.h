#ifndef interface
#define interface struct
#endif
interface ImagePosition;

class SImageDataJpeg : public ImageData
{
public:
	SImageDataJpeg(DWORD	hortMCUs,DWORD	mcuRowCnt,DWORD samplesPerLine,DWORD numberOfLines,BYTE f0,BYTE f1,BYTE f2);
	~SImageDataJpeg();
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
	DWORD	m_hortMCUs;
	DWORD	m_mcuRowCnt;
	DWORD	m_samplesPerLine;
	DWORD	m_numberOfLines;
	BYTE	m_compSamplingFactors[3];
	BYTE    spare;
public:
	BYTE*	m_yCbCrSamples[3];
};
