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

class ScaleYCbCrf1 : public Scale
{
public:
	ScaleYCbCrf1(ImageData* imageData) : Scale(imageData) {}
	void CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth);
	BOOL IsMulWidth3() {return TRUE;}
	Scale* GetSimilar(ImageData* imageData) {return new ScaleYCbCrf1(imageData);}
private:
	static void DoLineJPEGf1(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD yMult,DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult);
};
