// scale.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif

interface IFileData;
interface ImageData;
#define ULSTD ULONG __stdcall
class ScaleUpObj;


class Scale
{
protected:
	Scale(ImageData* imageData);
	DWORD GetMult(DWORD val,DWORD cDiv);
	DWORD GetReciprocal(DWORD yDiv);
private:
	virtual void CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth) = 0;
	virtual BOOL IsMulWidth3() = 0;
	virtual Scale* GetSimilar(ImageData* imageData) = 0;
public:
    ULSTD AddRef();
    ULSTD Release();
	Scale* GetBottom();
	static Scale* GetScalableImage(IFileData* data,int flags);
	BOOL GetDimensions(int* pWidth, int* pHeight);

	BYTE* GetDibBits(DWORD drawWidth,const DWORD drawHeight,
		DWORD srcLeft,const DWORD srcTop,DWORD srcWidth,const DWORD srcHeight,
		DWORD picLeft, DWORD picTop, DWORD picWidth, DWORD picHeight,
		BYTE* inDibBits=NULL,DWORD inDibWidth=0);
	~Scale();
private:
    volatile ULONG	  m_cRef;             // Object reference count

	ImageData* m_imageData;
};

