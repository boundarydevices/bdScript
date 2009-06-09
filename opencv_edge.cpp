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

static int const edge_thresh = 16;

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

  // Convert to grayscale
  IplImage *gray = cvCreateImage(cvSize(img->width,img->height), IPL_DEPTH_8U, 1);
  cvCvtColor(img, gray, CV_BGR2GRAY);
  
  // Create the output image
  IplImage *cedge = cvCreateImage(cvSize(img->width,img->height), IPL_DEPTH_8U, 3);
  cvZero( cedge );

  IplImage *edge = cvCreateImage(cvSize(img->width,img->height), IPL_DEPTH_8U, 1);

  cvSmooth( gray, edge, CV_BLUR, 3, 3, 0, 0 );
  cvNot( gray, edge );

  // Run the edge detector on grayscale
  cvCanny(gray, edge, (float)edge_thresh, (float)edge_thresh*3, 3);

  cvZero( cedge );
    
  // copy edge points
  cvCopy( img, cedge, edge );

  // get the image data
  height    = img->height;
  width     = img->width;
  step      = img->widthStep;
  channels  = img->nChannels;
  data      = (uchar *)img->imageData;
  printf("Processing a %dx%d image with %d channels\n",height,width,channels); 

  if(3 <= argc){
     char const *outFileName = argv[2];
     if(cvSaveImage(outFileName,cedge)) 
        printf("saved to %s\n", outFileName);
     else
        printf("Could not save: %s:%m\n",outFileName);
  }

  // release the image
  cvReleaseImage(&gray);
  cvReleaseImage(&edge);
  cvReleaseImage(&cedge);
  cvReleaseImage(&img);

  printf("Shutdown\n");
  return 0;
}

