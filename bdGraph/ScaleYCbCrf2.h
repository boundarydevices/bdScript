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

class ScaleYCbCrf2 : public Scale
{
public:
	ScaleYCbCrf2(ImageData* imageData) : Scale(imageData) {}
	void CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth);
	BOOL IsMulWidth3() {return TRUE;}
	Scale* GetSimilar(ImageData* imageData) {return new ScaleYCbCrf2(imageData);}
private:
	static void DoLineJPEGf2(BYTE* dest,DWORD* yPtr,DWORD* cbPtr,DWORD* crPtr,int colsLeft,
			DWORD yMult,DWORD cbBlueMult,DWORD cbGreenMult,DWORD crGreenMult,DWORD crRedMult);

};

