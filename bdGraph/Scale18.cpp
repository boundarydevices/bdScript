#include "ICommon.h"
#include "Scale.h"
#include "Scale18.h"
#include "SImageData16.h"
#include "ScaleRGB.h"
//*******************************************************************************
#define DEST_BYTES_PER_PIXEL 3
#define SOURCE_BYTES_PER_PIXEL 3
static inline int min(int x,int y)
{
   return (x<y)? x : y;
}
static inline unsigned char * getPixel(unsigned char *fbMem, int fbWidth,int fbLeft, int fbTop)
{
    return fbMem + (((fbTop*fbWidth) + fbLeft)*DEST_BYTES_PER_PIXEL);
}
static void ConvertRgb24Line(unsigned char* fbMem, unsigned char const *video,int cnt)
{
    do {
    	unsigned int val = (video[0]>>2) | ((video[1]&0xfc)<<(6-2)) | ((video[2]&0xfc)<<(6+6-2));
    if(1) {
		fbMem[0] = (unsigned char)val;
		fbMem[1] = (unsigned char)(val>>8);
		fbMem[2] = (unsigned char)(val>>16);
    } else {
		fbMem[0] = (unsigned char)~val;
		fbMem[1] = (unsigned char)~(val>>8);
		fbMem[2] = (unsigned char)~(val>>16);
    }
		fbMem += DEST_BYTES_PER_PIXEL;
		video += SOURCE_BYTES_PER_PIXEL;
    } while ((--cnt)>0);
}
void Scale18::render(unsigned char *fbMem,int fbWidth, int fbHeight,
     int fbLeft,				//placement on screen
     int fbTop,
     unsigned char const *imgMem,
     int imgWidth,
     int imgHeight,	 			// width and height of image
     int imageDisplayLeft,			//offset within image to start display
     int imageDisplayTop,
     int imageDisplayWidth,		//portion of image to display
     int imageDisplayHeight,
	 ConvertRgb24Line_to18 convertLineFunc
   )
{
   int minWidth;
   int minHeight;
   int imgWidthInBytes = ((imgWidth*SOURCE_BYTES_PER_PIXEL)+3)&~3;
   if (fbLeft<0)
   {
      imageDisplayLeft -= fbLeft;		//increase offset
      imageDisplayWidth += fbLeft;	//reduce width
      fbLeft = 0;
   }
   if (fbTop<0)
   {
      imageDisplayTop -= fbTop;
      imageDisplayHeight += fbTop;
      fbTop = 0;
   }
   if ((imageDisplayWidth <=0)||(imageDisplayWidth >(imgWidth-imageDisplayLeft))) imageDisplayWidth = imgWidth-imageDisplayLeft;
   if ((imageDisplayHeight<=0)||(imageDisplayHeight>(imgHeight-imageDisplayTop))) imageDisplayHeight = imgHeight-imageDisplayTop;

   imgMem += (imgWidthInBytes*imageDisplayTop)+(imageDisplayLeft*SOURCE_BYTES_PER_PIXEL);

   minWidth = min(fbWidth-fbLeft,imageDisplayWidth);	// << 1;	//2 bytes/pixel
   minHeight= min(fbHeight-fbTop,imageDisplayHeight);

   if (convertLineFunc==NULL)convertLineFunc = ConvertRgb24Line;
   if ((minWidth > 0) && (minHeight > 0))
   {
      unsigned char *fbPix = getPixel(fbMem,fbWidth,fbLeft,fbTop);
//      printf("fbWidth:%d, fbLeft:%d, fbTop:%d\n",fbWidth,fbLeft,fbTop);

      do
      {
         convertLineFunc(fbPix,imgMem,minWidth);
		 fbPix += (fbWidth*DEST_BYTES_PER_PIXEL);
         imgMem += imgWidthInBytes;
      } while (--minHeight);
   }
}
//*******************************************************************************
ImageData* Scale18::GetImageData(const unsigned short *img, int imgWidth, int imgHeight)
{
	return new SImageData16(imgWidth,imgHeight,img);
}

void Scale18::scale(unsigned char *dest, int destWidth, int destHeight,
	 unsigned short const *img, int imgWidth, int imgHeight,
	 int picLeft, int picTop, int picWidth,int picHeight)
{
	ImageData* pImageData = GetImageData(img,imgWidth,imgHeight);
	if (pImageData)
	{
		Scale* pScale = new ScaleRGB(pImageData);	//give my reference to pImageData if successful
		if (pScale) {
			BYTE* dib = pScale->GetDibBits(destWidth,destHeight,0,0, destWidth,destHeight,
				picLeft,picTop,picWidth,picHeight, NULL,0);
			if (dib) {
				render(dest,destWidth,destHeight,0,0,   dib,destWidth,destHeight, 0,0,destWidth,destHeight);
				delete [] dib;
			}
			pScale->Release();
		}
		else pImageData->Release();
	}
}
#define destWidth srcHeight
#define destHeight srcWidth
void Scale18::rotate90(unsigned char *dest, unsigned char const *src, int srcWidth, int srcHeight)
{
	unsigned char * dest1 = dest + ((destWidth - 1)*DEST_BYTES_PER_PIXEL);
	while (dest1 >=dest)
	{
		unsigned char * dest2 = dest1;
		int i = destHeight;
		while (i)
		{
			dest2[0] = src[0];
			dest2[1] = src[1];
			dest2[2] = src[2];
			src += DEST_BYTES_PER_PIXEL;	//rotate same # of bytes/pixel
			dest2 += (destWidth*DEST_BYTES_PER_PIXEL);
			i--;
		}
		dest1--;
	}	
}

/*
extern "C" void scale18(unsigned char *dest, int destWidth, int destHeight,
		unsigned char const *img, int imgWidth, int imgHeight,
		int picLeft, int picTop, int picWidth,int picHeight)
{
	Scale18::scale(dest,destWidth,destHeight,
		img,imgWidth,imgHeight,
		picLeft,picTop,picWidth,picHeight);
}
*/


