#ifndef __ODOMGRAPHICS_H__
#define __ODOMGRAPHICS_H__ "$Id: odomGraphics.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomGraphics.h
 *
 * This header file declares the odomGraphics_t class, which 
 * is used to load a set of graphics info from a certain directory
 * and the odomGraphicsByName_t class, used to keep track of 
 * a set of 
 *
 * Files read include:
 * 
 *    digitStrip.png
 *    dollarSign.png
 *    decimalPt.png
 *    comma.png
 *
 *    vshadow.dat
 *    vhighlight.dat
 *
 * Refer to odometer.pdf for a description of how these are used.
 *
 *
 * Change History : 
 *
 * $Log: odomGraphics.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include "fbImage.h"
#include "odomMode.h"

struct odomGraphics_t {
   odomGraphics_t(char const    *dirname,
                  odometerMode_e mode);
   ~odomGraphics_t(void);

   inline bool worked( void ) const { return worked_ ; }

   fbImage_t             digitStrip_ ;  // placed in video RAM
   fbImage_t             dollarSign_ ;
   fbImage_t             decimalPoint_ ;
   fbImage_t             comma_ ;

   fbPtr_t               digitAlpha_ ;
   fbPtr_t               dollarAlpha_ ;
   fbPtr_t               decimalAlpha_ ;
   fbPtr_t               commaAlpha_ ;

private:
   bool  worked_ ;
   odomGraphics_t(odomGraphics_t const &); // no copies
};

class odomGraphicsByName_t {
public:
   static odomGraphicsByName_t &get(void); // get singleton

   void add( char const *name, odomGraphics_t *graphics );
   odomGraphics_t const *get(char const *name);

private:
   odomGraphicsByName_t( void ); // only through singleton
};

#endif

