// IA_Unilever.cpp : Defines the exported functions for the DLL application.
//

#include "IA_Unilever.h"

// IA_Unilever.cpp : Defines the exported functions for the DLL application.
//

//#include "stdafx.h"


/*-------------------------------------------------------------------------------
	PURPOSE: To calculate spots and texture
	PROGRAMMER: Jomer dela Cruz

  ------------------------------------------------------------------------------*/
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>

#include <vector>


#include <cmath>
//#include "math.h"
//#include <ctime>

//Third party headers with implementation
#include "connected.h"
#include "CImg.h"


//#include <stdlib.h>
//#include <iostream>


//time
//#include <time.h>
#include <stdio.h>
//#include <sys/types.h>
//#include <sys/timeb.h>
//#include <string.h>
#include <string> //c++ string class
//


using namespace cimg_library;
using namespace std;
//using namespace blepo;


// The lines below are necessary when using a non-standard compiler such as visualcpp6.
#ifdef cimg_use_visualcpp6
#define std
#endif
#ifdef min
#undef min
#undef max
#endif

//define version
//#define MyRegistryKey "8.8.2008.1"

//#define cimg_imagemagick_path "C:\Program Files\ImageMagick-6.4.0-Q8\convert"
//#define cimg_imagemagick_path "convert" 

//Jomer Type definition
typedef cimg_library::CImg<float> CIMGF ;// list float
typedef cimg_library::CImgList<float> CIMGLF; //float list
typedef  unsigned char UCHAR_2;


struct comp_handler	
{
		long complabel;
		long count;
		vector<int> xcoor;
		vector<int> ycoor;

};

double MinCoor(vector<int> Numbers, const int Count)
{
	double Minimum = Numbers[0];

	for(int i = 0; i < Count; i++)
		if( Minimum  > Numbers[i] )
			Minimum  = Numbers[i];

	return Minimum;
}
//template<class Numbers>
double MaxCoor(vector<int> Numbers, const int Count)
{
	double Maximum = Numbers[0];

	for(int i = 0; i < Count; i++)
		if( Maximum < Numbers[i] )
			Maximum = Numbers[i];

	return Maximum;
}

CImg<float> removeComponents(CImg<float> image, double aspectRatio, int lowerSize, int upperSize, int MHPorW)//unsigned int
/*
PURPOSE: Perform connected component and remove small components and criteria aspect ratio
INPUT: Aspect ratio, criteria for removing circular or ellipse components
	   image - index image
	   MhporW -> 1 for wrinkle eveything else for MHP
	   lowerSize-> smallest component to remove
	   upperSize-> largest component to remove.
OUTPUT: Clean mask image
PROGRAMMER: Jomer dela Cruz
*/
{
	CImg<unsigned char> image_inb(image);//unsigned char
	

	int Xsize = image_inb.dimx();
	int Ysize =  image_inb.dimy();
	
	CImg<unsigned int> image_inlabel(Xsize,Ysize,1,1,0); // unsigned int
	
	unsigned char *img = (unsigned char *)malloc(Xsize*Ysize);
	unsigned int *out = (unsigned int *)malloc(sizeof(*out)*Xsize*Ysize); //int
	//img =image_inb;
	//out =image_inlabel;
	
	int x=0;
	int y=0;
	int xy=0;

	//dilate
	image_inb=image_inb.get_dilate(1);

	for(x=0;x<Xsize;++x)
	{
		for(y=0;y<Ysize;++y)
		{
			img[xy]=image_inb(x,y);			
		     xy=xy+1;
		}

	}	
	ConnectedComponents cc(30); 
	cc.connected(img, out, Ysize, Xsize, std::equal_to<unsigned char>(), true);//unsigned char
	xy=0;	
	for(x=0;x<Xsize;++x)
	{
		for(y=0;y<Ysize;++y)
		{
			image_inlabel(x,y)=out[xy];
			xy=xy+1;
		}

	}	
   //get max -> translated into total number of objects
	unsigned int max=0;
	for(x=0;x<Xsize;++x)
	{
		for(y=0;y<Ysize;++y)
		{
			if (image_inlabel(x,y)> max)
			{
				max = image_inlabel(x,y);
			}
		}
	}
	//create array of handlers
	std::vector<comp_handler> compH;

	for(unsigned int c=0;c<=max;++c)
	{
		compH.push_back(comp_handler());			 
		compH[c].complabel=c;
		compH[c].count=0;		
	}
	//Place components into array
	for(x=0;x<Xsize;++x)
	{
		for(y=0;y<Ysize;++y)
		{
			compH[image_inlabel(x,y)].xcoor.push_back(compH[image_inlabel(x,y)].count);
			compH[image_inlabel(x,y)].xcoor[compH[image_inlabel(x,y)].count] = x;  //enter x coordinate into vector
			compH[image_inlabel(x,y)].ycoor.push_back(compH[image_inlabel(x,y)].count);
			compH[image_inlabel(x,y)].ycoor[compH[image_inlabel(x,y)].count] = y; //enter y coordinate into vector
			compH[image_inlabel(x,y)].count=compH[image_inlabel(x,y)].count +1;  //increase count
		}
	}
	
	//Remove components based on Aspect ratio criteria
	double distance;
	double maxx=0;
	double maxy=0;
	double minx=0;
	double miny=0;
	double area;
    const double PI =3.14;
	double temp=0.0;
	int ic;
	//remove wrinkle
	if (MHPorW==1)
	{
		for(unsigned int c=0;c<=max;++c)
		{
			area=0.0;
			maxx=(double)MaxCoor(compH[c].xcoor,compH[c].count);
			maxy=(double)MaxCoor(compH[c].ycoor,compH[c].count);
			minx=(double)MinCoor(compH[c].xcoor,compH[c].count);
			miny=(double)MinCoor(compH[c].ycoor,compH[c].count);
			temp=((maxx-minx)*(maxx-minx)) + ((maxy-miny)*(maxy-miny));
			distance=sqrt(temp);
			area = PI * (distance/2) * (distance/2);
	
			if ((area/compH[c].count)<= aspectRatio) //remove everything not slender, 3-remove circle
			{
				for(ic=0;ic<compH[c].count;++ic)
				{
					image_inlabel(compH[c].xcoor[ic],compH[c].ycoor[ic])=255;
				
				}
			}
			else //include
			{
				//but make sure size is good for wrinkles
				if(compH[c].count <= lowerSize || compH[c].count >= upperSize )
				{
					//remove small
					for(ic=0;ic<compH[c].count;++ic)
					{
						image_inlabel(compH[c].xcoor[ic],compH[c].ycoor[ic])=255;				
					}
				}
				else
				{
					for(ic=0;ic<compH[c].count;++ic)
					{
						image_inlabel(compH[c].xcoor[ic],compH[c].ycoor[ic])=0;
			
					}
				}//else

			}//else
		}//for
	}//if
	else
	{
		for(unsigned int c=0;c<=max;++c)
		{
			area=0.0;
			maxx=(double)MaxCoor(compH[c].xcoor,compH[c].count);
			maxy=(double)MaxCoor(compH[c].ycoor,compH[c].count);
			minx=(double)MinCoor(compH[c].xcoor,compH[c].count);
			miny=(double)MinCoor(compH[c].ycoor,compH[c].count);
			temp=((maxx-minx)*(maxx-minx)) + ((maxy-miny)*(maxy-miny));
			distance=sqrt(temp);
			area = PI * (distance/2) * (distance/2);
	
			if ((area/compH[c].count)>= aspectRatio) //remove everything slender, 3-remove circle
			{
				for(ic=0;ic<compH[c].count;++ic)
				{
					image_inlabel(compH[c].xcoor[ic],compH[c].ycoor[ic])=255;
				
				}
			}
			else //include
			{
				//but make sure size is good for hyperpigmentation not blob
				if(compH[c].count <= lowerSize || compH[c].count >= upperSize )
				{
					//remove small
					for(ic=0;ic<compH[c].count;++ic)
					{
						image_inlabel(compH[c].xcoor[ic],compH[c].ycoor[ic])=255;				
					}
				}
				else
				{
					for(ic=0;ic<compH[c].count;++ic)
					{
						image_inlabel(compH[c].xcoor[ic],compH[c].ycoor[ic])=0;
			
					}
				}//else

			}//else
		}//for
	
	}

	//image_inlabel.save("maskLabel.bmp");
	
	//free memory
	free(img); 
	free(out); 
	image_inb.assign();

	CImg<float> image_out(image_inlabel);//unsigned char
   image_inlabel.assign(); //cleat memory
	return image_out;
}

//Functions declaration

//CImg<float> meanN(CImg<float> image);
float ImStddev(CImg<float> image) ;
CImg<float> imThresh(CImg<float> image,float imStd, float imMean, float numThresh);
CImg<float> imDiff(CImg<float> image,CImg<float> image2, int R, int G, int B);
CImg<float> avgFilter(CImg<float> image);
float calc_hyper(CImg<float> mask, CImg<float> rawimage) ;
CImg<float>  norm_mean(CImg<float> image);
CImg<float>  im_div(CImg<float> image1,CImg<float> image2);
//CImg<float> multIM(CImgList<float> image1, CImg<float> image2);


//PRATEEK FUNCTIONS
CImg<float> sqrim(CImg<float> image);
CImg<float> DirectionalWT(CImg<float> image);
CImg<float> psi_1(int side, double sigma1);
CImgList<float> psi_2(int side, double theta, double sigma2, int awidth,CImgList<float> iml);
CImg<float> imThreshp(CImg<float> image, float numThresh, double& wrinkleScore,int R, int G, int B); //for thresholding DWT image

CImg<float>  im_div(CImg<float> image1,CImg<float> image2)
{
		cimg_library::CImg<float> imdiv(image1);
		
		cimg_forXY(imdiv,x,y)
		{
			  imdiv(x,y)= image1(x,y)/image2(x,y);
		}
	return imdiv.get_normalize(0,255);
}
CImg<float>  norm_mean(CImg<float> image)
{
		cimg_library::CImg<float> norm_im(image);
		float immean =  norm_im.mean();
		cimg_forXY(norm_im,x,y)
		{
			 norm_im(x,y)= norm_im(x,y)/immean;
		}
		printf("Size: %ld\n", norm_im.size());
		return norm_im.get_normalize(0,255);
}
CImg<float>   psi_1(int side, double sigma1)
//NOTE: TESTED
{
	//Variables has to be in vector to accept varrying size
	int N=side;

	float* arrW1;
	float* arrW2;
	float* arrW3;
	float* arrW4;
    arrW1= new float [N];
	arrW2= new float [N];
	arrW3= new float [N];
	arrW4= new float [N];
   


	cimg_library::CImg<float> dist(N,N);
	int i;
	int idx=0;
	double temp;
	for (i=0; i<=N-1; ++i)
	{

		arrW1[i]=i;
		arrW2[i]=arrW1[i] -((N/2)-1);
	}
	for (i=0; i<=N-1; ++i)
	{
	    temp=(((N/2)-1)+i)%N;
		idx=(int)floor(temp);
		arrW3[i]=arrW2[idx]; //circular shift 
	    //cout<<arrW3[i]<<"\n";
	    arrW4[i] = arrW3[i]/sigma1;
		arrW4[i]= pow(arrW4[i],2);// + pow(arrW4[i],2);
		//cout<<arrW4[i];
	}
	
	cimg_forXY(dist,x,y)
	{
		{

			dist(x,y)= arrW4[x] + arrW4[y];
			dist(x,y) = 1- exp(-1*dist(x,y));
		//	cout<<dist(x,y) <<" ";
		}
		//cout<<"\n";
	}

	delete[] arrW1;
	delete[] arrW2;
	delete[] arrW3;
	delete[] arrW4;
    

	return 	dist;
}


CImgList<float>  psi_2(int side, double theta, double sigma2, int awidth, CImgList<float> iml)
{
	//Variables has to be in vector to accept varrying size
	//sigma2=1;  //just for testing
	int N=side;
	int im_Xsize = N;
	int im_Ysize = N;	
	

    float* arrW1;
	float* arrW2;
	float* arrW3;
	float* arrX;
    float* arrY;

	//Dynamical Allocate memory
    arrW1= new float [N];
	arrW2= new float [N];
	arrW3= new float [N];
	arrX= new float [N];
    arrY= new float [N];

	CIMGF wx(N,N);
	CIMGF wy(N,N);


	cimg_library::CImgList<float> psi_2_out(N,N);
	int i;
	for (i=0; i<=N-1; ++i)
	{
	
		arrW1[i]=i;
		arrW2[i]=arrW1[i] -((N/2)-1);
	}
	for (i=0; i<=N-1; ++i)
	{

		arrW3[i]=arrW2[(((N/2)-1)+i)%N]; //circular shift - tested
		arrX[i]= arrW3[i] * cos((double)theta);
		arrY[i]= arrW3[i] * sin((double)theta);

	}
	
	double k = (-1) * (log(sigma2)/pow(tan((double)awidth),2)); //tested

	cimg_forXY(wy,x,y)
	{
			wx(x,y)= arrX[y] + arrY[x];  //tested the arrangement don't change			
			wy(x,y)= arrX[x] + arrY[y]; //tested			
			if(wx(x,y)< 0.0)
			{
				if((-1*wx(x,y))<0.01)
				{
					wx(x,y)=(float)0.0100;
				}
			}
			else
			{
				if(wx(x,y)<0.01)
				{
				
					wx(x,y)=(float)0.0100;
				}
			}		
			wx(x,y)= pow(wx(x,y),2);
			wy(x,y)= pow(wy(x,y),2); //tested			
			wy(x,y) = k*wy(x,y);
	

	}

 
	//psi_2_out = exp(wy * wx.get_inverse());  
	//psi_2_out = wy / wx;

	for (int x=0; x<=im_Xsize-1;++x)
	{
		for (int y=0; y<=im_Ysize-1;++y)
		{
			psi_2_out(x,y)= exp(wy(x,y) / wx(x,y));

			psi_2_out(x,y)= psi_2_out(x,y)* iml[0](x,y);
		}
	}

	//free up memory
	delete[] arrW1;
	delete[] arrW2;
	delete[] arrW3;
	delete[] arrX;
    delete[] arrY;
	wx.assign(); //equivalent to  img=CImg <float>
	wy.assign();

	return psi_2_out;
}

float  calc_hyper(CImg<float> mask, CImg<float> rawimage) 
{

	float sumSPOT=0.0;
	float sumBACK=0.0;
	double ctrSPOT=0.0;
	double ctrBACK=0.0;


	int im_Xsize = rawimage.dimx();
	int im_Ysize = rawimage.dimy();	
	
	for (int x=0; x<=im_Xsize-1; x++)
	{
		for (int y=0; y<=im_Ysize-1; y++)
		{
			if (mask(x,y)==0)
			{
				sumSPOT=sumSPOT + rawimage(x,y);
				ctrSPOT = ctrSPOT + 1.0;
			}
			else
			{

				sumBACK=sumBACK + rawimage(x,y);
				ctrBACK = ctrBACK + 1.0;
			}
		}

	}
	return ctrSPOT/(im_Ysize*im_Xsize); // ((sumBACK/ctrBACK ) - (sumSPOT/ctrSPOT));

}


float   ImStddev(CImg<float> image) 
{

	float temp=0.0;
	float vari=0.0;

	float imMean = image.mean();
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	
	for (int x=0; x<=im_Xsize-1; x++)
	{
		for (int y=0; y<=im_Ysize-1; y++)
		{
			temp=image(x,y)-imMean;
			vari += temp*temp;

		}

	}
	return sqrt( vari / ((im_Xsize*im_Xsize)-1) ); 

}

CImg<float> avgFilter(CImg<float> image)
//void __stdcall avgFilter()
{
	
	//3x3 neigborhood to get average, circular implementation 
    float sumSPOT=0;
	
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	cimg_library::CImg<float> imAvg(im_Xsize,im_Ysize ,1,1,255); 
    
	for (int x=0; x<=im_Xsize-1; x++)
	{
		sumSPOT=0.0;
		for (int y=0; y<=im_Ysize-1; y++)
		{
			//3x3 neigborhood			
			sumSPOT=image((im_Xsize+(x-1))%im_Xsize,(im_Ysize+(y-1))%im_Ysize)+ //diagonal
					image((x+1)%im_Xsize,(y+1)%im_Ysize)+
					 //center
					image(x%im_Xsize,y%im_Ysize)+					
					//0
					image((im_Xsize+(x-1))%im_Xsize,y)+
					image((x+1)%im_Xsize,y)+
					//90
					image(x,(im_Ysize+(y-1))%im_Ysize)+
					image(x,(y+1)%im_Ysize)+
					//45
					image((im_Xsize+(x-1))%im_Xsize,(y+1)%im_Ysize)+
					image((x+1)%im_Xsize,(im_Ysize+(y-1))%im_Ysize);

		
			imAvg(x,y)=(float)sumSPOT/9.0;
		
			

		}
	}
	
	return imAvg;
	
//	cout<<"TEST";
}


CImg<float> imThresh(CImg<float> image,float imStd, float imMean, float numThresh)
{
	//3x3 neigborhood to get average, circular implementation 
    float sumSPOT=0.0;

	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	cimg_library::CImg<float> imSpot(im_Xsize,im_Ysize ,1,1,255); 

	for (int x=0; x<=im_Xsize-1; x++)
	{
		
		for (int y=0; y<=im_Ysize-1; y++)
		{
			//3x3 neigborhood
			
			sumSPOT=image((im_Xsize+(x-1))%im_Xsize,(im_Ysize+(y-1))%im_Ysize)+ //diagonal
					image((x+1)%im_Xsize,(y+1)%im_Ysize)+
					 //center
					image(x%im_Xsize,y%im_Ysize)+					
					//0
					image((im_Xsize+(x-1))%im_Xsize,y)+
					image((x+1)%im_Xsize,y)+
					//90
					image(x,(im_Ysize+(y-1))%im_Ysize)+
					image(x,(y+1)%im_Ysize)+
					//45
					image((im_Xsize+(x-1))%im_Xsize,(y+1)%im_Ysize)+
					image((x+1)%im_Xsize,(im_Ysize+(y-1))%im_Ysize);

			if((sumSPOT/9.0) <= imMean-(imStd * numThresh))
			{
				imSpot(x,y)=0;
			}
		
			sumSPOT=0.0;

		}
	}
	return imSpot;
}

CImg<float>  imDiff(CImg<float> image,CImg<float> image2, int R, int G, int B)
{
    //NOTE OUTPUT COLOR
	
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	cimg_library::CImg<float> imDiff(im_Xsize,im_Ysize ,1,3,255); 

	for (int x=0; x<=im_Xsize-1; x++)
	{
		for (int y=0; y<=im_Ysize-1; y++)
		{
			
		
			imDiff(x,y,2)=image(x,y)-image2(x,y);
			
			if (imDiff(x,y,2)==0)
			{
				imDiff(x,y,0)=255;
				imDiff(x,y,1)=255;
				imDiff(x,y,2)=255;
			}
			else
			{
				imDiff(x,y,0)=R;
				imDiff(x,y,1)=G;
				imDiff(x,y,2)=B;
			}
			

		}
	}
	return imDiff;
}

void bdimDiff(CImg<float> image,
		CImg<float> image2,
		unsigned char *alpha)
{
    //NOTE OUTPUT COLOR
	
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	

	for (int x=0; x<=im_Xsize-1; x++)
	{
		for (int y=0; y<=im_Ysize-1; y++)
		{
			if ((image(x,y)-image2(x,y)))
				alpha[x + y * im_Xsize] = 255;
		}
	}
}

CImg<float>   multIM(CImgList<float> image1, CImg<float> image2)
//PURPOSE: Multiple Image
//INPUT:grayscale images to multiply
{
	int im_Xsize = image2.dimx();
	int im_Ysize = image2.dimy();	
	int x=0;
	int y=0;

	cimg_library::CImg<float> imMult; 

	for (x=0;x<=im_Xsize;x++)
	{
		for (y=0;y<=im_Ysize;y++)
		{
			imMult(x,y)=image1(x,y)* image2(x,y);
		}
	}
	return imMult;
	
}


//PRATEEK IMPLEMENTATION TEXTURE
CImg<float>  sqrim(CImg<float> image)
//NOTE: TESTED TO CREATE DIADIC SQUARE IMAGE
{
	//INPUT : gray image
	//PURPOSE: To take care of corners
	//OUTPUT: square image that contain the input image
	//PROGRAMMER: Prateek Shrivastava
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	int side=0;
//	double temp=2.0;

	if(im_Ysize>im_Xsize)
	{
		side=im_Ysize;
	}
	else
	{
		side=im_Xsize;
	}
	//diadic
	side= (int)pow(2,ceil((log((double)side)/log(2.0))));  //GO BACK HERE *******************************

	//side=(int)temp;

	//create blank square image
	cimg_library::CImg<float> sqr_im(side,side ,1,1,0); 
	
	int left=(int)floor((side-im_Ysize)/2.0);  //correspond to y-axis
	int low =(int)floor((side-im_Xsize)/2.0); //x

	//Copy image to empty square
	int x;
	int y;
	for (y=0;y<=im_Ysize-1;++y)
	{
		for (x=0;x<=im_Xsize-1;++x)
		{
			sqr_im(x+low,y+left)=image(x,y);
		}
	}
	int im_newXsize = sqr_im.dimx();
	int im_newYsize = sqr_im.dimy();
	//mirror the image into the border for taking care of the corners
	//X - LOW=X
	for (x=0;x<=low-1;x++)
	{
		for (y=0;y<=im_newYsize-1;y++)
		{
			sqr_im(x,y)=sqr_im(2*low-x,y);
		}
	}
	//X
	
	for (x=(low)+(im_Xsize-1);x<=side-1;x++)
	{
		for (y=0;y<=im_newYsize-1;y++)
		{
			sqr_im(x,y)=sqr_im((2*(im_Xsize+low-1))-x,y);
		}
	}
	
	//Y
	for (x=0;x<=im_newXsize-1;x++)
	{
		for (y=0;y<=left-1;y++)
		{
			sqr_im(x,y)=sqr_im(x,2*left-y);
		}
	}
	
	//Y
	for (x=0;x<=im_newXsize-1;x++)
	{
		for (y=(left)+(im_Ysize-1);y<=side-1;y++)
		{
			sqr_im(x,y)=sqr_im(x,2*(im_Ysize+left-1)-y);
		}
	}
	
	//sqr_im.save("sqim.bmp");
	
	return sqr_im;

}


CImg<float>   DirectionalWT(CImg<float> image)
//INPUT : gray image
//PURPOSE: To take care of corners
//OUTPUT: DirectionalWT of input image
//PROGRAMMER: Prateek Shrivastava
{
	int im_oldXsize = image.dimx();
	int im_oldYsize = image.dimy();
	
	const double PI=3.1415926535897932384626433832795;
	//square image holder
	CImg<float> sqr_im; //declare empty image of unknown dimension

	//put input image into square image
	sqr_im = sqrim(image);
	int im_Xsize = sqr_im.dimx();
	int im_Ysize = sqr_im.dimy();
	int side=im_Ysize;

    //sqr_im=image;

	// set up contant
	int N_Angle =4;
	double awidth=PI/N_Angle;
	double bwidth=(double)side/30;
	double Sigma = bwidth/sqrt((double)log(2.0));
	double Sigma2=2.0;

	cimg_library::CImgList<> Fp1(im_Xsize,im_Ysize);  //declare empty image of unknown dimension
	cimg_library::CImg<float> result(side,side,1,1,0);

	//Element-wise multiplication
	Fp1 = sqr_im.get_FFT();
  
	CImgList<> f(Fp1);
	f[0].mul(psi_1(side, Sigma));
    
 
	double theta;
	for (int isector=0; isector<=N_Angle-1;++isector)
	{
		theta = isector * awidth;	    
		CImgList<> holder(f);		
		holder[0]<<psi_2(side,theta,Sigma2,(int)awidth,f);	//this is the time-consuming part	

		holder.FFT(true)[0];  //just get inverse of the real part

		//Maximum implementation - with absolute value
		//N^2 * 8
		cimg_forXY(sqr_im,x,y)
		{
				
				if (holder[0](x,y) < 0.0)
				{
					holder[0](x,y) = holder[0](x,y)  * -1;

					if (result(x,y)>holder[0](x,y))
					{
						result(x,y) =result(x,y);
					}
					else
					{
						result(x,y) =holder[0](x,y);
					}
				}
				else
				{
					if (result(x,y)>holder[0](x,y))
					{
						result(x,y) =result(x,y);
					}
					else
					{
						result(x,y) =holder[0](x,y);
					}

				}
		}
	 
	} 
	
	//cut image
	int left = (int)floor((side-im_oldYsize)/2); //y
	int low = (int)floor((side-im_oldXsize)/2); //x

	cimg_library::CImg<float> cut_im(im_oldXsize,im_oldYsize);
	int x,y;
	for(x=0;x<=im_oldXsize-1;++x)
	{
		for(y=0;y<=im_oldYsize-1; ++y)
		{
			cut_im(x,y)=result(x+low-1,y+left-1);
	
		}
	
	}
	//freeup memory
    Fp1.assign();
	f.assign();
	sqr_im.assign();
	result.assign();

	return cut_im; //.normalize(0,255); 
}


CImg<float>   imThreshWrinkle(CImg<float> image, float numThresh)
/*******************************************************
PROGRAMMER: Jomer dela Cruz

*********************************************************/
{
	
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	cimg_library::CImg<unsigned char> imWrinkle(im_Xsize,im_Ysize ,1,1,255); 

	cimg_forXY(image,x,y)
	{		

	   if(image(x,y) > numThresh)
		{
			//color mask
			imWrinkle(x,y)=0;			
	
		}	
	}
	//clear memory   
	//wrinkleScore= sumWrinkle /(im_Xsize * im_Ysize);	

	return imWrinkle;
}
void calcWrinkleScore(CImg<float> image, double& wrinkleScore)
/*******************************************************
PROGRAMMER: Jomer dela Cruz
Threshold: 7 - Empirical
*********************************************************/
{
	float sumWrinkle=0.0;
	
	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();	
	//cimg_library::CImg<float> imWrinkle(im_Xsize,im_Ysize ,1,1,255); 

	cimg_forXY(image,x,y)
	{		

	   if(image(x,y)==0)
		{
			sumWrinkle=sumWrinkle+1.0;		
		}	
	}
	//clear memory   
	wrinkleScore= sumWrinkle /(im_Xsize * im_Ysize);	

	//return imWrinkle;
}

void bdcalcMHP(cimg_library::CImg<float> &image,
		cimg_library::CImg<float>& rawim,
		double& mhpScore,
		double threshold,
		unsigned char *alpha)
{
	try
	{
		int im_Xsize = image.dimx();
		int im_Ysize = image.dimy();	

		cimg_library::CImg<float> bIm = rawim; 

		cimg_library::CImg<float> imDil(im_Xsize,im_Ysize ,1,1,255); 

		rawim=norm_mean(rawim);

		bIm=norm_mean(bIm);
	
		bIm= im_div(rawim,bIm.get_blur(60,true));

		bIm = bIm.blur(2,true); //remover furrows was

		float imMean = (float)bIm.mean();
		float imStd= (float)ImStddev(bIm);
	
		bIm=imThresh(bIm,imStd, imMean, threshold);

		bIm=removeComponents(bIm, 2.5,35,2000,0);  //(image,aspectratio, lowersize, uppersize, 0-mhp)
		double hyper_score= calc_hyper(bIm,rawim) ;
		mhpScore=hyper_score;

		imDil = bIm.get_dilate(3);

		bdimDiff(bIm, imDil, alpha);

		//clean up
		bIm.assign();	
		imDil.assign();
	}
	catch (CImgInstanceException &e)
	{
		std::fprintf(stderr,"CImg Library Error : %s",e.message); 
	}
	catch (CImgArgumentException &e)
	{
		std::fprintf(stderr,"CImg Library Error : %s",e.message); 
	}
	catch (CImgIOException &e)
	{
		std::fprintf(stderr,"CImg Library Error : %s",e.message); 
	}
}

void bdcalcWrinkle(cimg_library::CImg<float> &image,
			cimg_library::CImg<float>& rawim,
			double& wrinkleScore,
			double threshold,
			unsigned char *alpha)
{
	try
	{
	//Wrinkles

	int im_Xsize = image.dimx();
	int im_Ysize = image.dimy();

	cimg_library::CImg<float> raw = image;
	cimg_library::CImg<float> imDil(im_Xsize,im_Ysize ,1,1,255); 

	image=norm_mean(image); //normalize

	//remove uneven
	image= im_div(image,raw.get_blur(60,true)); //devide and normalize

	rawim=DirectionalWT(image);

	//threshold
	rawim=imThreshWrinkle(rawim, threshold);

	//remove components
	rawim=removeComponents(rawim, 4,25,(int)((im_Xsize*im_Xsize)*0.05),1);//(image,aspectratio, lowersize, uppersize, 1-wrinkle)
	calcWrinkleScore(rawim,wrinkleScore);

	imDil = rawim.get_dilate(3);
	bdimDiff(rawim, imDil, alpha);

	raw.assign();
	imDil.assign();
	}
	catch (CImgInstanceException &e)
	{
		std::fprintf(stderr,"CImg Library Error : %s",e.message); 
	}
	catch (CImgArgumentException &e)
	{
		std::fprintf(stderr,"CImg Library Error : %s",e.message); 
	}
	catch (CImgIOException &e)
	{
		std::fprintf(stderr,"CImg Library Error : %s",e.message); 
	}
}

void createHistImg(cimg_library::CImg<float> &image, cimg_library::CImg<float> &hist)
{
  int width = image.dimx();
  int height = image.dimy();
  //int layers = image.dimz(); NEEDS EXAMINATION
  int r = 0;
  int c = 0;

  for (r = 0; r < height; r++)
  {
    c = 0;
    hist(c, r, 0) = image(c, r, 0);
    hist(c, r, 1) = image(c, r, 1);
    hist(c, r, 2) = image(c, r, 2);

    for (c = 1; c < width; c++)
    {
      hist(c, r, 0) = hist((c - 1), r, 0) + image(c, r, 0);
      hist(c, r, 1) = hist((c - 1), r, 1) + image(c, r, 1);
      hist(c, r, 2) = hist((c - 1), r, 2) + image(c, r, 2);
    }
  }

  for (c = 0; c < width; c++)
  {
    for (r = 1; r < height; r++)
    {
      hist(c, r, 0) = hist(c, (r - 1), 0) + hist(c, r, 0);
      hist(c, r, 1) = hist(c, (r - 1), 1) + hist(c, r, 1);
      hist(c, r, 2) = hist(c, (r - 1), 2) + hist(c, r, 2);
    }
  }
}

float getMean(cimg_library::CImg<float> &image, int r, int c, int fhs, int layer)
{
  float sum = image((c + fhs), (r + fhs), layer);
  sum -= image((c + fhs), (r - fhs - 1), layer);
  sum -= image((c - fhs -1), (r + fhs), layer);
  sum += image((c - fhs - 1), (r - fhs - 1), layer);

  return sum / ((2 * fhs + 1) * (2 * fhs + 1));
}

void performBLT(cimg_library::CImg<float> &image, float evenPct, int locSz)
{
  float pct = evenPct;
  int width = image.dimx();
  int height = image.dimy();
  int planes = image.dimz();
  cimg_library::CImg<float> output(width, height, planes);
  cimg_library::CImg<float> histImg(width, height, planes);

  createHistImg(image, histImg);

  for (int r = locSz + 1; r < height - locSz; r++)
  {
    for (int c = locSz + 1; c < width - locSz; c++)
    {
      float rMean = getMean(histImg, r, c, locSz, 0);
      float gMean = getMean(histImg, r, c, locSz, 1);
      float bMean = getMean(histImg, r, c, locSz, 2);
      float xG = 1 - rMean / gMean * pct;
      float xB = 1 - rMean / bMean * pct;

      output(c, r, 0) = image(c, r, 0);
      output(c, r, 1) = image(c, r, 0) * pct + image(c, r, 1) * xG;
      output(c, r, 2) = image(c, r, 0) * pct + image(c, r, 2) * xB;

      output(c, r, 0) = (output(c, r, 0) >= 0) ? output(c, r, 0) : 0;
      output(c, r, 1) = (output(c, r, 1) >= 0) ? output(c, r, 1) : 0;
      output(c, r, 2) = (output(c, r, 2) >= 0) ? output(c, r, 2) : 0;

      output(c, r, 0) = (output(c, r, 0) <= 255) ? output(c, r, 0) : 255;
      output(c, r, 1) = (output(c, r, 1) <= 255) ? output(c, r, 1) : 255;
      output(c, r, 2) = (output(c, r, 2) <= 255) ? output(c, r, 2) : 255;
    }
  }

  image = output;
}

void clipImg(cimg_library::CImg<float> &image, int locSz)
{
  int width = image.dimx();
  int height = image.dimy();
  printf("width: %d height: %d\n", width, height);

  for (int r = 0; r < locSz; r++)
  {
    for (int c = 0; c < width; c++)
    {
      image(c, r, 0) = 0;
      image(c, r, 1) = 0;
      image(c, r, 2) = 0;
    }
  }

  for (int r = height - locSz; r < height; r++)
  {
    for (int c = 0; c < width; c++)
    {
      image(c, r, 0) = 0;
      image(c, r, 1) = 0;
      image(c, r, 2) = 0;
    }
  }

  for (int r = 0; r < height; r++)
  {
    for (int c = 0; c < locSz; c++)
    {
      image(c, r, 0) = 0;
      image(c, r, 1) = 0;
      image(c, r, 2) = 0;
    }
  }

  for (int r = 0; r < height; r++)
  {
    for (int c = width - locSz; c < width; c++)
    {
      image(c, r, 0) = 0;
      image(c, r, 1) = 0;
      image(c, r, 2) = 0;
    }
  }
}

float *matrixMultiply(float *mat1, int x1, int y1,
                        float *mat2, int x2, int y2)
{
  float *newMat = (float *) calloc(x1*y2, sizeof(float));
  int x = 0, y = 0, z = 0;
  for(x = 0; x < x1; x++)
  {
    for(z = 0; z < y2; z++)
    {
      for(y = 0; y < y1; y++)
        newMat[x * y2 + z] += mat1[x * y1 + y] * mat2[y * y2 + z];
    }
  }
  return newMat;
}

/*
 * M is a matrix of size 3x3
 */
void RGB_To_LAB_For_NonLinear_Images(cimg_library::CImg<float> &image, float *M)
{
  double invGamma = 2.2;
  int width = image.dimx();
  int height = image.dimy();
  int planes = image.dimz();
  float k = 903.3;
  cimg_library::CImg<float> output(width, height, planes);
  double e = 0.008856;

  for (int r = 0; r < height; r++)
  {
    for (int c = 0; c < width; c++)
    {
      double R = image(c, r, 0);
      double G = image(c, r, 1);
      double B = image(c, r, 2);

      R = pow(R / 255, invGamma);
      G = pow(G / 255, invGamma);
      B = pow(B / 255, invGamma);

      float mRGB[] = { R, G, B };

      float *mXYZ = matrixMultiply(M, 3, 3, mRGB, 3, 1);

      double xr = mXYZ[0] / 0.950456;
      double yr = mXYZ[1];
      double zr = mXYZ[2] / 1.088754;

      free(mXYZ);

      double fx = 0;
      double fy = 0;
      double fz = 0;

      if (xr > e)
        fx = pow(xr, 1.0 / 3.0);
      else
        fx = (k * xr + 16) / 116;

      if (yr > e)
        fy = pow(yr, 1.0 / 3.0);
      else
        fy = (k * yr + 16) / 116;

      if (zr > e)
        fz = pow(zr, 1.0 / 3.0);
      else
        fz = (k * zr + 16) / 116;

      double L = 116 * fy - 16;
      double a = 500 * (fx - fy);
      double b = 200 * (fy - fz);

      output(c, r, 0) = L;
      output(c, r, 1) = a;
      output(c, r, 2) = b;
    }
  }
  image = output;
}

void Apply_Delta_Lab(cimg_library::CImg<float> &image, float Delta_L, float Delta_a, float Delta_b)
{
  int width = image.dimx();
  int height = image.dimy();
  int planes = image.dimz();
  cimg_library::CImg<float> output(width, height, planes);

  for (int r = 0; r < height; r++)
  {
    for (int c = 0; c < width; c++)
    {
      output(c, r, 0) = image(c, r, 0) + Delta_L;
      output(c, r, 1) = image(c, r, 1) + Delta_a;
      output(c, r, 2) = image(c, r, 2) + Delta_b;
    }
  }

  image=output;
}

void LAB_To_RGB_For_NonLinear_Images(cimg_library::CImg<float> &image, float *InvM)
{
  double gamma = 1 / 2.2;
  int width = image.dimx();
  int height = image.dimy();
  int planes = image.dimz();
  cimg_library::CImg<float> output(width, height, planes);
  double e = 0.008856;
  float k = 903.3;

  for (int r = 0; r < height; r++)
  {
    for (int c = 0; c < width; c++)
    {
      double L = image(c, r, 0);
      double a = image(c, r, 1);
      double b = image(c, r, 2);
      double fx = 0, fy = 0, fz = 0;
      double xr = 0, yr = 0, zr = 0;

      if (L > k * e)
        yr = pow((L + 16) / 116, 3);
      else
        yr = L / k;

      if (yr > e)
        fy = (L + 16) / 116;
      else
        fy = (k * yr + 16) / 116;

      fx = a / 500 + fy;
      fz = fy - b / 200;

      if (pow(fx, 3) > e)
        xr = pow(fx, 3);
      else
        xr = (116 * fx - 16) / k;

      if (pow(fz, 3) > e)
        zr = pow(fz, 3);
      else
        zr = (116 * fz - 16) / k;

      float mXYZ[] = { xr * 0.950456, yr, zr * 1.088754 };

      float *mRGB = matrixMultiply(InvM, 3, 3, mXYZ, 3, 1);

      float R = (float)(pow((double)mRGB[0], gamma) * 255);
      float G = (float)(pow((double)mRGB[1], gamma) * 255);
      float B = (float)(pow((double)mRGB[2], gamma) * 255);

      R = (R >= 0) ? R : 0;
      R = (R <= 255) ? R : 255;
      G = (G >= 0) ? G : 0;
      G = (G <= 255) ? G : 255;
      B = (B >= 0) ? B : 0;
      B = (B <= 255) ? B : 255;

      output(c, r, 2) = B;
      output(c, r, 1) = G;
      output(c, r, 0) = R;
    }
  }
  image = output;
}

void bdPinkishWhite(cimg_library::CImg<float> &image, float perBLT, float delta_L, float delta_a, float delta_b, int bltSize)
{
	int locSz = bltSize / 2;
	float M[] = {0.412453, 0.35758, 0.180423, 0.212671, 0.71516, 0.072169, 0.019334, 0.119193, 0.950227};
	float invM[] = { 3.2405, -1.5372, -0.4985, -0.9693, 1.876, 0.0416, 0.0556, -0.204, 1.0573};

	clipImg(image, locSz);
	performBLT(image, perBLT, locSz);
	RGB_To_LAB_For_NonLinear_Images(image, M);
	Apply_Delta_Lab(image, delta_L, delta_a, delta_b);
	LAB_To_RGB_For_NonLinear_Images(image, invM);
	return;
}
