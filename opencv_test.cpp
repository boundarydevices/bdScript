////////////////////////////////////////////////////////////////////////
//
// hello-world.cpp
//
// This is a simple, introductory OpenCV program. The program reads an
// image from a file, inverts it, and displays the result. 
//
////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <cv.h>
#include <highgui.h>

int main(int argc, char *argv[])
{
  IplImage* img = 0; 
  int height,width,step,channels;
  uchar *data;
  int i,j,k;

  if(argc<2){
    printf("Usage: main <image-file-name> [output-file-name]\n\7");
    exit(0);
  }

  // load an image  
  img=cvLoadImage(argv[1]);
  if(!img){
    printf("Could not load image file: %s\n",argv[1]);
    exit(0);
  }

  // get the image data
  height    = img->height;
  width     = img->width;
  step      = img->widthStep;
  channels  = img->nChannels;
  data      = (uchar *)img->imageData;
  printf("Processing a %dx%d image with %d channels\n",height,width,channels); 

  if(3 <= argc){
     IplImage* img2 = img2=cvCloneImage(img);
     cvConvertImage(img, img2, CV_CVTIMG_FLIP|CV_CVTIMG_SWAP_RB);

     char const *outFileName = argv[2];
     if(cvSaveImage(outFileName,img2)) 
        printf("saved to %s\n", outFileName);
     else
        printf("Could not save: %s:%m\n",outFileName);
     cvReleaseImage(&img2);
  }

  // release the image
  cvReleaseImage(&img);

  printf("Shutdown\n");
  return 0;
}

