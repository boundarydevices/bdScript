#include "ICommon.h"
#include "Scale.h"
#include "Scale16.h"
#include "SImageData16.h"
#include "ScaleRGB.h"
//*******************************************************************************

static inline int min(int x,int y)
{
   return (x<y)? x : y;
}
static inline unsigned short * getPixel(unsigned short *fbMem, int fbWidth,int fbLeft, int fbTop)
{
    return fbMem + ((fbTop*fbWidth) + fbLeft);
}
static void ConvertRgb24Line(unsigned short* fbMem, unsigned char const *video,int cnt)
{
    do {
	*fbMem++ = (video[0]>>3) | ((video[1]&0xfc)<<(5-2)) | ((video[2]&0xf8)<<(5+6-3));
	video += 3;
    } while ((--cnt)>0);
}
void Scale16::render(unsigned short *fbMem,int fbWidth, int fbHeight,
     int fbLeft,				//placement on screen
     int fbTop,
     unsigned char const *imgMem,
     int imgWidth,
     int imgHeight,	 			// width and height of image
     int imageDisplayLeft,			//offset within image to start display
     int imageDisplayTop,
     int imageDisplayWidth,		//portion of image to display
     int imageDisplayHeight,
	 ConvertRgb24Line_t convertLineFunc=NULL
   )
{
   int minWidth;
   int minHeight;
//   int imgWidth3 = imgWidth*3;
   int imgWidth3 = ((imgWidth*3)+3)&~3;
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

   imgMem += (imgWidth3*imageDisplayTop)+(imageDisplayLeft*3);

   minWidth = min(fbWidth-fbLeft,imageDisplayWidth);	// << 1;	//2 bytes/pixel
   minHeight= min(fbHeight-fbTop,imageDisplayHeight);

   if (convertLineFunc==NULL)convertLineFunc = ConvertRgb24Line;
   if ((minWidth > 0) && (minHeight > 0))
   {
      unsigned short *fbPix = getPixel(fbMem,fbWidth,fbLeft,fbTop);
//      printf("fbWidth:%d, fbLeft:%d, fbTop:%d\n",fbWidth,fbLeft,fbTop);

      do
      {
         convertLineFunc(fbPix,imgMem,minWidth);
		 fbPix += fbWidth;
         imgMem += imgWidth3;
      } while (--minHeight);
   }
}
//*******************************************************************************
ImageData* Scale16::GetImageData(const unsigned short *img, int imgWidth, int imgHeight)
{
	return new SImageData16(imgWidth,imgHeight,img);
}

void Scale16::scale(unsigned short *dest, int destWidth, int destHeight,
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
void Scale16::rotate90(unsigned short *dest, unsigned short const *src, int srcWidth, int srcHeight)
{
	unsigned short * dest1 = dest + (destWidth - 1);
	while (dest1 >=dest)
	{
		unsigned short * dest2 = dest1;
		int i = destHeight;
		while (i)
		{
			*dest2 = *src++;
			dest2 += destWidth;
			i--;
		}
		dest1--;
	}	
}

/*
extern "C" void scale16(unsigned short *dest, int destWidth, int destHeight,
		unsigned short const *img, int imgWidth, int imgHeight,
		int picLeft, int picTop, int picWidth,int picHeight)
{
	Scale16::scale(dest,destWidth,destHeight,
		img,imgWidth,imgHeight,
		picLeft,picTop,picWidth,picHeight);
}
*/


