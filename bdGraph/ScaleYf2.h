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

class ScaleYf2 : public Scale
{
public:
	ScaleYf2(ImageData* imageData) : Scale(imageData) {}
	void CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth);
	BOOL IsMulWidth3() {return TRUE;}
	Scale* GetSimilar(ImageData* imageData) {return new ScaleYf2(imageData);}
};

