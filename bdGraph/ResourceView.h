// ResourceView.h : header file
//
/////////////////////////////////////////////////////////////////////////////
class ResourceView
{
	ResourceView();
	~ResourceView();
public:
	static void RenderCenter(unsigned short *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length);
	static void RenderStretch(unsigned short *fbMem,int fbWidth,int fbHeight,int flags,const unsigned char* resource,unsigned int length);
};

