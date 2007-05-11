// Scale18.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif
interface ImageData;
typedef void (*ConvertRgb24Line_to18)(unsigned char* fbMem, unsigned char const *video,int cnt);

class Scale18
{
// Construction
public:
	Scale18();
	~Scale18();

	static ImageData* GetImageData(const unsigned short *img, int imgWidth, int imgHeight);
	static void render(unsigned char *fbMem,int fbWidth, int fbHeight,int fbStride,
		int fbLeft, int fbTop,
		unsigned char const *imgMem, int imgWidth, int imgHeight,
		int imageDisplayLeft,int imageDisplayTop,int imageDisplayWidth,int imageDisplayHeight,
		ConvertRgb24Line_to18 convertLineFunc=NULL);

	static void scale(unsigned char *dest, int destWidth, int destHeight, int destStride,
		unsigned short const *img, int imgWidth, int imgHeight,
		int picLeft, int picTop, int picWidth,int picHeight);

	static void rotate90(unsigned char *dest, unsigned char const *src,
		int srcWidth, int srcHeight);
};

//extern "C" void scale18(unsigned char *dest, int destWidth, int destHeight,
//		unsigned char const *img, int imgWidth, int imgHeight,
//		int picLeft, int picTop, int picWidth,int picHeight);
