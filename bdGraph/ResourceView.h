// ResourceView.h : header file
//
/////////////////////////////////////////////////////////////////////////////
typedef void (*ConvertRgb24Line_t)(unsigned short* fbMem, unsigned char const *video,int cnt);

class ResourceView
{
	ResourceView();
	~ResourceView();
public:
	static void RenderCenter(unsigned short *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length);
	static void RenderStretch(unsigned short *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length);
	static void GetData16(const unsigned char* resource, unsigned int length,unsigned short **pfbMem,int* ppicWidth,int* ppicHeight,
		ConvertRgb24Line_t convertLineFunc=NULL);
};

