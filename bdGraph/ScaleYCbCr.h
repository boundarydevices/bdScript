// scaleRGB.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif
#ifndef FALSE
#define FALSE               0
#endif
#ifndef TRUE
#define TRUE                1
#endif

class ScaleYCbCr : public Scale
{
public:
	ScaleYCbCr(ImageData* imageData) : Scale(imageData) {}
	void CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth);
	BOOL IsMulWidth3() {return FALSE;}
	Scale* GetSimilar(ImageData* imageData) {return new ScaleYCbCr(imageData);}
private:
	static void DoLine1JPEG(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult);
	static void DoLineJPEG(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD yMult,DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult);
};
