/*
 * Module touchCalibrate.cpp
 *
 * This module defines the methods of the touchCalibration_t
 * class as declared in touchCalibrate.h
 *
 *
 * Change History : 
 *
 * $Log: inputCalibrate.cpp,v $
 * Revision 1.1  2008-08-23 22:04:48  ericn
 * [import]
 *
 * Revision 1.2  2006/08/29 01:07:59  ericn
 * -clamp to screen bounds
 *
 * Revision 1.1  2006/08/28 18:24:43  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <stdio.h>
#include <math.h>
#include <linux/fb.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <linux/input.h>
#include "flashVar.h"
#include <assert.h>

struct point_t {
   unsigned x ;
   unsigned y ;
};

static double coef_[6];
static bool haveData_ = false ;

static void setCalibration( char const *data )
{
   double a[6];
   if( 6 == sscanf( data, "%lf,%lf,%lf,%lf,%lf,%lf", a, a+1, a+2, a+3, a+4, a+5 ) )
   {
      haveData_ = true ;
      for( unsigned i = 0 ; i < 6 ; i++ )
         coef_[i] = a[i];
   }
   else
      fprintf( stderr, "Invalid calibration settings\n" );
}

class fbDevice_t {
public:
   static fbDevice_t &get(void);
   void clear( unsigned char r, unsigned char g, unsigned char b );
   unsigned getWidth();
   unsigned getHeight();
   void line( unsigned x1, 
              unsigned y1, 
              unsigned x2, 
              unsigned y2, 
              unsigned      penWidth,
              unsigned char r, 
              unsigned char g, 
              unsigned char b );
private:   
   fbDevice_t( char const *devName );
   int                        fd_ ;
   struct fb_fix_screeninfo   fix_ ;
   struct fb_var_screeninfo   var_ ;
   void                      *mem_ ;
};

fbDevice_t &fbDevice_t::get(void)
{
   static fbDevice_t *dev_ = 0 ;
   if( 0 == dev_ )
      dev_ = new fbDevice_t( "/dev/fb0" );
   return *dev_ ;
}

unsigned fbDevice_t::getWidth()
{
   return var_.xres ;
}

unsigned fbDevice_t::getHeight()
{
   return var_.yres ;
}

void fbDevice_t::clear( unsigned char r, unsigned char g, unsigned char b )
{
   if( 16 == var_.bits_per_pixel ){
      unsigned short rgb = (((unsigned)r>>3) << 11)
                         | (((unsigned)g>>2) << 5)
                         | (((unsigned)b>>3));
      unsigned short *out = (unsigned short *)mem_ ;
      unsigned count = fix_.smem_len / sizeof(rgb);
      while( count-- ){
         *out++ = rgb ;
      }
   }
   else
      fprintf( stderr, "Only know how to clear 16bpp displays\n" );
}
   
void fbDevice_t::line
   ( unsigned x1, 
     unsigned y1, 
     unsigned x2, 
     unsigned y2, 
     unsigned      penWidth,
     unsigned char r, 
     unsigned char g, 
     unsigned char b )
{
   if( 16 == var_.bits_per_pixel ){
      unsigned short rgb = (((unsigned)r>>3) << 11)
                         | (((unsigned)g>>2) << 5)
                         | (((unsigned)b>>3));
      unsigned char *bytes = (unsigned char *)mem_ ;
      bytes += y1*fix_.line_length + x1*sizeof(rgb);
      unsigned short *out = (unsigned short *)bytes ;
      if( x1 == x2 ){
         unsigned shortsPerRow = fix_.line_length/sizeof(rgb);
         int count = y2-y1 ;
         while( 0 < count-- ){
            *out = rgb ;
            out += shortsPerRow ;
         }
      } else if( y1 == y2 ) {
         int count = x2-x1 ;
         while( 0 < count-- ){
            *out++ = rgb ;
         }
      } else {
         printf( "only horizontal or vertical lines supported\n" );
      }
   }
   else
      fprintf( stderr, "Only know how to clear 16bpp displays\n" );
}

fbDevice_t::fbDevice_t( char const *devName )
   : fd_( open( devName, O_RDWR ) )
   , mem_( 0 )
{
   if( 0 > fd_ ){
      perror( devName );
      exit(-1);
   }

   int err = ioctl( fd_, FBIOGET_FSCREENINFO, &fix_);
   if( 0 != err ){
      perror( "FBIOGET_FSCREENINFO" );
      exit(-1);
   }
   err = ioctl( fd_, FBIOGET_VSCREENINFO, &var_ );
   if( 0 != err ){
      perror( "FBIOGET_VSCREENINFO" );
      exit(-1);
   }
   mem_ = mmap( 0, fix_.smem_len, PROT_READ|PROT_WRITE, MAP_SHARED, fd_, 0 );
   if( MAP_FAILED == mem_ ){
      perror( "mmap(fbMem)" );
      mem_ = 0 ;
   }
}

void translate
   ( point_t const &input,
     point_t       &translated )
{
   if( haveData_ ){
      fbDevice_t &fb = fbDevice_t::get();
      long int x = (long)round(coef_[0]*input.x + coef_[1]*input.y + coef_[2]);
      if( 0 > x )
         x = 0 ;
      if( (unsigned)x >= fb.getWidth() )
         x = fb.getWidth()-1 ;
      long int y = (long)round(coef_[3]*input.x + coef_[4]*input.y + coef_[5]);
      if( 0 > y )
         y = 0 ;
      if( (unsigned) y >= fb.getHeight() )
         y = fb.getHeight()-1 ;
      translated.x = x ;
      translated.y = y ;
   }
   else
      translated = input ;
}

#define DIM(__arr) (sizeof(__arr)/sizeof(__arr[0]))
#include <vector>
#include <algorithm>

struct pointData_t {
   unsigned x_ ;
   unsigned y_ ;
   unsigned median_i_ ;
   unsigned median_j_ ;
};
struct minMaxMed_t {
   unsigned min_ ;
   unsigned max_ ;
   unsigned median_ ;
};

typedef std::vector<point_t> pointVector_t ;

class calibratePoll_t {
public:
   calibratePoll_t( void );
   ~calibratePoll_t( void );

   inline bool done( void ){ return released_ ; }
   inline unsigned numSamples(void) const { return points_.size(); }
   inline pointVector_t const &getSamples(void) const { return points_ ; }

   // override this to perform processing of a touch
   void onTouch( int x, int y, unsigned pressure, timeval const &tv );
   // override this to perform processing of a release
   void onRelease( timeval const &tv );

   void poll(unsigned ms);

private:
   int                  fd_ ;
   bool                 wasDown_ ;
   bool                 pressed_ ;
   bool                 released_ ;
   pointVector_t        points_ ;
};

static char const *getTouchDev( void )
{
   char const *devName = 0 ;
   char const *envDev = getenv( "TSDEV" );
   if( 0 != envDev )
      devName = envDev ;
   if( 0 == devName )
      devName = "/dev/input/event0" ;
   return devName ;
}

calibratePoll_t::calibratePoll_t( void )
   : fd_( open( getTouchDev(), O_RDONLY ) )
   , wasDown_( false )
   , pressed_( false )
   , released_( false )
{
   if( 0 > fd_ ){
      perror( getTouchDev() );
      exit(-1);
   }
   fcntl( fd_, F_SETFD, FD_CLOEXEC );
   fcntl( fd_, F_SETFL, O_NONBLOCK );
}

calibratePoll_t::~calibratePoll_t( void )
{
}

static char const * const eventTypeNames[] = {
	"EV_SYN"
,	"EV_KEY"
,	"EV_REL"
,	"EV_ABS"
,	"EV_MSC"
};

void calibratePoll_t::poll(unsigned ms)
{
   pollfd pfd ; 
   pfd.fd = fd_ ;
   pfd.events = POLLIN | POLLERR ;

   int const numReady = ::poll( &pfd, 1, ms );
   if( 0 < numReady ){
      unsigned x ; unsigned y ;
      unsigned mask = 0 ;
      struct input_event event ;
      bool isDown = wasDown_ ;

      int numRead ;

      while( sizeof( event ) == ( numRead = read( fd_, &event, sizeof( event ) ) ) )
      {
         if( EV_ABS == event.type ){
            if( ABS_X == event.code ){
               x = event.value ;
               mask |= 1<<ABS_X ;
            } else if( ABS_Y == event.code ){
               y = event.value ;
               mask |= 1<<ABS_Y ;
            } else if( ABS_PRESSURE == event.code ){
               isDown = (0 != event.value);
	       if( !isDown )
		       mask = 0 ;
            }
         } else if( EV_SYN == event.type ){
            struct timeval now ; gettimeofday(&now, 0);
            if( (isDown != wasDown_) && (!isDown) ){
               printf( "%s\n", isDown ? "press" : "release" );
               wasDown_ = isDown ;
               onRelease(now);
            } else if( mask == (1<<ABS_X)|(1<<ABS_Y) ){
		if( isDown )
			onTouch(x,y,1,now);
                wasDown_ = isDown ;
            }
         } else {
            printf( "type %d (%s), code 0x%x, value %d\n",
                  event.type, 
                  event.type <= EV_MSC ? eventTypeNames[event.type] : "other",
                  event.code,
                  event.value );
         }
      }
   }
}

void calibratePoll_t::onTouch( int x, int y, unsigned pressure, timeval const &tv )
{
   pressed_ = true ;
   point_t pt = { x, y };
   points_.push_back( pt );
   printf( "touched %u:%u\n", x, y );
}

void calibratePoll_t::onRelease( timeval const &tv )
{
   printf( "released\n" );
   released_ = pressed_ ;
}

static void cross( fbDevice_t &fb, point_t const &pt, unsigned char r, unsigned char g, unsigned char b ){
   fb.line( pt.x-8, pt.y, pt.x+8, pt.y, 1, r, g, b );
   fb.line( pt.x, pt.y-8, pt.x, pt.y+8, 1, r, g, b );
}

static bool compareY( point_t const &lhs, point_t const &rhs)
{
   return lhs.y < rhs.y ;
}

static bool compareX( point_t const &lhs, point_t const &rhs)
{
   return lhs.x < rhs.x ;
}

void colinear( char dir,
               pointData_t const &point1,
               pointData_t const &point2,
               pointData_t const &point3 )
{
      fprintf( stderr, "points %u:%u, %u:%u, and %u:%u colinear in %c\n", 
               point1.x_, point1.y_,
               point2.x_, point2.y_,
               point3.x_, point3.y_,
	       dir );
      exit(-1);
}

/*
x = a1 i  +  a2 j + a3
y = b1 i  +  b2 j + b3

where x,y are screen coordinates and i,j are measured touch screen readings
| x |  =  | a1  a2  a3 | | i |
| y |     | b1  b2  b3 | | j |
                         | 1 |

| x1  x2  x3 |  =  | a1  a2  a3 | | i1 i2 i3 |
| y1  y2  y3 |     | b1  b2  b3 | | j1 j2 j3 |
                                  | 1   1  1 |
where xn,yn are screen coordinates of where touched
and   in,jn are measured readings when touched

so

| x1  x2  x3 |  | i1  i2  i3 | -1  =  | a1  a2  a3 |
| y1  y2  y3 |  | j1  j2  j3 |        | b1  b2  b3 |
                | 1   1    1 |

any three non-linear points will determine a1,a2,a3, b1.b2,b3
*/

static void SwapRows(double* w1,double* w2)
{
	double t;
	int i=6;
	while (i) {
		t = *w1; 
		*w1++ = *w2;
		*w2++ = t;	//swap
		i--;
	}
}
#if 0
static void PrintMatrix(double *w)
{
	int j;
	for (j=0; j<3; j++) {
		printf("%f %f %f %f %f %f\n",w[0],w[1],w[2],w[3],w[4],w[5]);
		w+=6;
	}
	printf("\n");
}
#endif

/*
x = a1 i  +  a2 j + a3
y = b1 i  +  b2 j + b3

where x,y are screen coordinates and i,j are measured touch screen readings
| x |  =  | a1  a2  a3 | | i |
| y |     | b1  b2  b3 | | j |
                         | 1 |

| x1  x2  x3 |  =  | a1  a2  a3 | | i1 i2 i3 |
| y1  y2  y3 |     | b1  b2  b3 | | j1 j2 j3 |
                                  | 1   1  1 |
where xn,yn are screen coordinates of where touched
and   in,jn are measured readings when touched

so

| x1  x2  x3 |  | i1  i2  i3 | -1  =  | a1  a2  a3 |
| y1  y2  y3 |  | j1  j2  j3 |        | b1  b2  b3 |
                | 1   1    1 |

any three non-linear points will determine a1,a2,a3, b1,b2,b3
*/
#define rc(r,c) (r*6+c)
int calcCoef( double *coef,
                pointData_t const* point1,
                pointData_t const* point2,
                pointData_t const* point3 )
{
	int i,j;
	double w[rc(3,6)];
	double t;

	w[rc(0,0)] = point1->median_i_;
	w[rc(1,0)] = point1->median_j_;
	w[rc(2,0)] = 1.0;

	w[rc(0,1)] = point2->median_i_;
	w[rc(1,1)] = point2->median_j_;
	w[rc(2,1)] = 1.0;

	w[rc(0,2)] = point3->median_i_;
	w[rc(1,2)] = point3->median_j_;
	w[rc(2,2)] = 1.0;

	//find Inverse of touched
	for (j=0; j<3; j++) {
		for (i=3; i<6; i++) {
			w[rc(j,i)] = ((j+3)==i) ? 1.0 : 0;	//identity matrix tacked on end
		}
	}

//	PrintMatrix(w);
	if (w[rc(0,0)] < w[rc(1,0)]) SwapRows(&w[rc(0,0)],&w[rc(1,0)]);
	if (w[rc(0,0)] < w[rc(2,0)]) SwapRows(&w[rc(0,0)],&w[rc(2,0)]);
	t = w[rc(0,0)];
	if (t==0) { printf("error w[0][0]=0\n"); return -1;}
	w[rc(0,0)] = 1.0;
	for (i=1; i<6; i++) w[rc(0,i)] = w[rc(0,i)]/t;

	for (j=1; j<3; j++) {
		t = w[rc(j,0)];
		w[rc(j,0)] = 0;
		for (i=1; i<6; i++) w[rc(j,i)] -= w[rc(0,i)] * t;
	}

//	PrintMatrix(w);
	if (w[rc(1,1)] < w[rc(2,1)]) SwapRows(&w[rc(1,0)],&w[rc(2,0)]);

	t = w[rc(1,1)];
	if (t==0) { printf("error w[1][1]=0\n"); return -1;}
	w[rc(1,1)] = 1.0;
	for (i=2; i<6; i++) w[rc(1,i)] = w[rc(1,i)]/t;

	t = w[rc(2,1)];
	w[rc(2,1)] = 0;
	for (i=2; i<6; i++) w[rc(2,i)] -= w[rc(1,i)] * t;

//	PrintMatrix(w);
	t = w[rc(2,2)];
	if (t==0) { printf("error w[2][2]=0\n"); return -1;}
	w[rc(2,2)] = 1.0;
	for (i=3; i<6; i++) w[rc(2,i)] = w[rc(2,i)]/t;

	for (j=0; j<2; j++) {
		t = w[rc(j,2)];
		w[rc(j,2)] = 0;
		for (i=3; i<6; i++) w[rc(j,i)] -= w[rc(2,i)] * t;
	}

//	PrintMatrix(w);
	t = w[rc(0,1)];
	w[rc(0,1)] = 0;
	for (i=3; i<6; i++) w[rc(0,i)] -= w[rc(1,i)] * t;
//	PrintMatrix(w);

/*
| x1  x2  x3 |  | i1  i2  i3 | -1  =  | a1  a2  a3 |
| y1  y2  y3 |  | j1  j2  j3 |        | b1  b2  b3 |
                | 1   1    1 |
*/
	coef[0] = (point1->x_ * w[rc(0,3)]) + (point2->x_ * w[rc(1,3)]) + (point3->x_ * w[rc(2,3)]);
	coef[1] = (point1->x_ * w[rc(0,4)]) + (point2->x_ * w[rc(1,4)]) + (point3->x_ * w[rc(2,4)]);
	coef[2] = (point1->x_ * w[rc(0,5)]) + (point2->x_ * w[rc(1,5)]) + (point3->x_ * w[rc(2,5)]);
	coef[3] = (point1->y_ * w[rc(0,3)]) + (point2->y_ * w[rc(1,3)]) + (point3->y_ * w[rc(2,3)]);
	coef[4] = (point1->y_ * w[rc(0,4)]) + (point2->y_ * w[rc(1,4)]) + (point3->y_ * w[rc(2,4)]);
	coef[5] = (point1->y_ * w[rc(0,5)]) + (point2->y_ * w[rc(1,5)]) + (point3->y_ * w[rc(2,5)]);
	return 0;
}



static unsigned colors[4] = {
   0xFDE64F,  // yellow
   0xFF0000,  // red 
   0x00FF00,  // green
   0x0000FF   // blue
};

int main( void )
{
   fbDevice_t &fb = fbDevice_t::get();
   fb.clear(0,0,0);

   point_t points[5] = {
      { fb.getWidth()>>3, fb.getHeight()>>3 }
   ,  { fb.getWidth()-(fb.getWidth()>>3), fb.getHeight()>>3 }
   ,  { fb.getWidth()-(fb.getWidth()>>3), fb.getHeight()-(fb.getHeight()>>3) }
   ,  { fb.getWidth()>>3, fb.getHeight()-(fb.getHeight()>>3) }
   ,  { fb.getWidth()>>1, fb.getHeight()>>1 }
   };

   pointVector_t samples[5];

   for( unsigned i = 0 ; i < DIM(points); i++ ){
      calibratePoll_t poller ;
      
      cross(fb,points[i],0xff,0xff,0xff);
      while( !poller.done() ){
         poller.poll(-1U);
      }
      cross(fb,points[i],0,0,0);
      if( 50 > poller.numSamples() ){
         printf( "Not enough points: %u\n", poller.numSamples() );
         --i ;
      }
      else {
         printf( "%u samples\n", poller.numSamples() );
         samples[i] = poller.getSamples();
      }
   }

   minMaxMed_t stats_[5*2];
   pointData_t pointData[5];

   for( unsigned i = 0 ; i < DIM(points); i++ ){
      pointVector_t &ptSamples = samples[i];
      std::sort(ptSamples.begin(), ptSamples.end(), compareX );
      minMaxMed_t &xStat = stats_[2*i];
      xStat.min_ = ptSamples[0].x ;
      xStat.max_ = ptSamples[ptSamples.size()-1].x ;
      xStat.median_ = ptSamples[ptSamples.size()/2].x ;
      
      std::sort(ptSamples.begin(), ptSamples.end(), compareY );
      minMaxMed_t &yStat = stats_[2*i+1];
      yStat.min_ = ptSamples[0].y ;
      yStat.max_ = ptSamples[ptSamples.size()-1].y ;
      yStat.median_ = ptSamples[ptSamples.size()/2].y ;

      printf( "%4u:%-4u    i: %u..%u (median %u) j: %u..%u (median %u)\n", 
              points[i].x,
              points[i].y,
              xStat.min_, xStat.max_, xStat.median_, 
              yStat.min_, yStat.max_, yStat.median_ );
      pointData[i].x_ = points[i].x ;
      pointData[i].y_ = points[i].y ;
      pointData[i].median_i_ = xStat.median_ ;
      pointData[i].median_j_ = yStat.median_ ;
   }

   for( unsigned i = 0 ; i < DIM(pointData); i++ ){
      printf( "%4u:%-4u  i: %u, j: %u\n", 
              pointData[i].x_,
              pointData[i].y_,
              pointData[i].median_i_,
              pointData[i].median_j_ );
   }

   for( unsigned p = 0 ; p < 4 ; p++ )
   {
      unsigned char r = colors[p]>>16; 
      unsigned char g = (colors[p]&0xFF00)>>8;
      unsigned char b = colors[p]&0xFF;
      double coef[6];
      char temp[256];
      calcCoef(coef, &pointData[p],&pointData[(p+1)&3],&pointData[(p+2)&3]);
      snprintf( temp, sizeof(temp), "%lf,%lf,%lf,%lf,%lf,%lf", coef[0], coef[1], coef[2], coef[3], coef[4], coef[5] );
      printf( "%s\n", temp );
      setCalibration(temp);

      for( unsigned i = 0 ; i < DIM(samples); i++ ){
         pointVector_t &ptSamples = samples[i];
         for( unsigned j = 0 ; j < ptSamples.size(); j++ ){
            point_t pt ;
//            printf( "translating %u:%u\n", ptSamples[j].x, ptSamples[j].y );
            translate(ptSamples[j],pt);
//            if( (0==i)&&(0==j) ){
//               printf( "i:%u,j:%u --> %u:%u\n", ptSamples[j].x, ptSamples[j].y, pt.x, pt.y );
//            }
            cross(fb,pt,r,g,b);
         }
      }
   }

static char const tsTypeFileName[] = {
   "/proc/tstype"
};

   unsigned wires = 0 ;
   FILE *fWires = fopen( tsTypeFileName, "rt" );
   if( fWires ){
      char cWires[256];
      if( fgets(cWires,sizeof(cWires)-1,fWires) ){
         unsigned wires = cWires[0]-'0' ;
         if( (4 == wires) || (5 == wires) ){
         }
         else
            fprintf( stderr, "%s: Invalid data <%s>\n", tsTypeFileName, cWires );
      }
      else
         fprintf( stderr, "%s: No data\n", tsTypeFileName );
      fclose( fWires );
   }
   else {
      perror( tsTypeFileName );
      wires = 4 ;
   }

   if( wires ){
            char flashVariable[80];
            snprintf( flashVariable, sizeof(flashVariable), "t%ux%u-%uwa", fb.getWidth(), fb.getHeight(), wires );
            
            char flashData[512];
            char *nextOut = flashData ;
            for( unsigned i = 0 ; i < DIM(pointData); i++ ){
               nextOut += snprintf( nextOut, flashData+sizeof(flashData)-nextOut-1, 
                                    "%u:%u.%d.%d ", 
                                    pointData[i].x_,
                                    pointData[i].y_,
                                    pointData[i].median_i_,
                                    pointData[i].median_j_ );
            }
            printf( "---------------> saving flash variable %s==%s\n", flashVariable, flashData );
            writeFlashVar( flashVariable, flashData );
   }
   return 0 ;
}

