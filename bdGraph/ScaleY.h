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

class ScaleY : public Scale
{
public:
	ScaleY(ImageData* imageData) : Scale(imageData) {}
	void CreateBitmap(ScaleUpObj* yObj,ScaleUpObj* cbObj,ScaleUpObj* crObj,DWORD srcWidth,DWORD rowCnt,BYTE* dest,DWORD rowWidth);
	BOOL IsMulWidth3() {return FALSE;}
	Scale* GetSimilar(ImageData* imageData) {return new ScaleY(imageData);}
};
