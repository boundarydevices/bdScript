#ifndef __ODOMGRAPHICS_H__
#define __ODOMGRAPHICS_H__ "$Id: odomGraphics.h,v 1.3 2002-12-15 05:46:44 ericn Exp $"

/*
 * odomGraphics.h
 *
 * This header file declares the odomGraphics_t class, which 
 * is used to load a set of graphics info from a certain directory
 * and the odomGraphicsByName_t class, used to keep track of 
 * a set of odomGraphics_t objects by name.
 *
 * Files read include:
 * 
 *    digitStrip.png
 *    dollarSign.png
 *    decimalPt.png
 *    comma.png
 *
 *    scale.dat   (defines the number of scaled copies of the graphics created)
 *
 * When using 
 *    vshadow.dat
 *    vhighlight.dat
 *
 * Refer to odometer.pdf for a description of how these are used.
 *
 *
 * Change History : 
 *
 * $Log: odomGraphics.h,v $
 * Revision 1.3  2002-12-15 05:46:44  ericn
 * -Added support for horizontally scaled graphics based on input width
 *
 * Revision 1.2  2006/10/16 22:26:19  ericn
 * -added validate() method
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
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
                  odometerMode_e mode,
                  unsigned       maxDigits,     // # of digits. used to build scaled copies
                  unsigned       maxWidth );    // in pixels
   ~odomGraphics_t(void);

   inline bool worked( void ) const { return worked_ ; }
   inline unsigned height( void ) const { return decimalPoint_[0]->height(); }

   bool validate() const ;

   inline unsigned numScaled( void ) const { return numScaled_ ; }
   inline unsigned const *sigToIndex( void ) const { return sigToIndex_ ; }
   inline unsigned sigToIndex(unsigned sig ) const { return (sig <= maxDigits_) ? sigToIndex_[sig] : numScaled_-1 ; }

   //
   // Use these routines to get scaled graphic images based
   // on the number of significant digits
   //
   fbImage_t const &getStrip(unsigned numSig) const { return *digitStrip_[sigToIndex(numSig)]; }
   fbImage_t const &getDollar(unsigned numSig) const { return *dollarSign_[sigToIndex(numSig)]; }
   fbImage_t const &getDecimal(unsigned numSig) const { return *decimalPoint_[sigToIndex(numSig)]; }
   fbImage_t const &getComma(unsigned numSig) const { return *comma_[sigToIndex(numSig)]; }

   //
   // Use this routine to determine digit positions based on the
   // number of significant digits (due to scaled graphics)
   //
   // Note that digit zero is the least significant digit and that
   // the offsets assume right-alignment.
   // 
   unsigned digitOffset( unsigned numSig, unsigned digitPos ) const { return digitPositions_[sigToIndex(numSig)][digitPos]; }
   unsigned dollarOffset( unsigned numSig ) const { return dollarPositions_[numSig]; }
   unsigned comma1Offset( unsigned numSig ) const { return comma1Positions_[sigToIndex(numSig)]; }
   unsigned comma2Offset( unsigned numSig ) const { return comma2Positions_[sigToIndex(numSig)]; }
   unsigned decimalOffset( unsigned numSig ) const { return decimalPositions_[sigToIndex(numSig)]; }

private:
   unsigned const         maxDigits_ ;
   unsigned               numScaled_ ;
   unsigned const        *sigToIndex_ ;
   unsigned const *const *digitPositions_ ; // [numScaled_][maxDigits_]
   unsigned const *const  dollarPositions_ ; // [numScaled_]
   unsigned const *const  comma1Positions_ ; // [numScaled_]
   unsigned const *const  comma2Positions_ ; // [numScaled_]
   unsigned const *const  decimalPositions_ ; // [numScaled_]
   fbImage_t            **digitStrip_ ;  // placed in video RAM
   fbImage_t            **dollarSign_ ;
   fbImage_t            **decimalPoint_ ;
   fbImage_t            **comma_ ;

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

