// Scale16.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif
interface ImageData;

class Scale16
{
// Construction
public:
	Scale16();
	~Scale16();

	static ImageData* GetImageData(const unsigned short *img, int imgWidth, int imgHeight);
	static void render(unsigned short *fbMem,int fbWidth, int fbHeight,
		int fbLeft, int fbTop,
		unsigned char const *imgMem, int imgWidth, int imgHeight,
		int imageDisplayLeft,int imageDisplayTop,int imageDisplayWidth,int imageDisplayHeight);

	static void scale(unsigned short *dest, int destWidth, int destHeight,
		unsigned short const *img, int imgWidth, int imgHeight,
		int picLeft, int picTop, int picWidth,int picHeight);

};

//extern "C" void scale16(unsigned short *dest, int destWidth, int destHeight,
//		unsigned short const *img, int imgWidth, int imgHeight,
//		int picLeft, int picTop, int picWidth,int picHeight);
