#ifndef __METER_H__
#define __METER_H__ "$Id: meter.h,v 1.1 2003-12-01 04:55:49 ericn Exp $"

/*
 * meter.h
 *
 * This header file declares a simple meter_t class, 
 * which is used to project a number onto a different
 * scale using fixed-point (1/256ths) math.
 *
 * The reason it's called meter_t reflects its' 
 * usefulness in producing progress bars and the like.
 * 
 * To use this class, provide the input and output range 
 * to the constructor, and call the project() method with
 * each input point.
 * 
 *
 * Change History : 
 *
 * $Log: meter.h,v $
 * Revision 1.1  2003-12-01 04:55:49  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2003
 */

class meter_t {
public:
   inline meter_t( int inputMin,
                   int inputMax,
                   int outputMin,
                   int outputMax );

   inline int project( int input ) const ;

private:
   int const      inputMin_ ;
   int const      inputMax_ ;
   unsigned const inputRange_ ;
   int const      outputMin_ ;
   int const      outputMax_ ;
   unsigned const outputRange_ ;
   bool const     inputBigger_ ;
   unsigned       ratio_ ;
};

meter_t :: meter_t
   ( int inputMin,
     int inputMax,
     int outputMin,
     int outputMax )
   : inputMin_( inputMin )
   , inputMax_( inputMax )
   , inputRange_( inputMax-inputMin+1 )
   , outputMin_( outputMin ) 
   , outputMax_( outputMax )
   , outputRange_( outputMax-outputMin+1 )
   , inputBigger_( inputRange_ > outputRange_ )
   , ratio_( inputBigger_ ? ( ( inputRange_ * 256 ) / ( outputRange_ ? outputRange_ : 1 ) )
                          : ( ( outputRange_ * 256 ) / ( inputRange_ ? inputRange_ : 1 ) ) )
{
   if( 0 == ratio_ )
      ratio_ = 1 ;
}

int meter_t :: project( int input ) const 
{
   //
   // scale input
   // 
   if( input > inputMax_ )
   {
      return outputMax_ ;
   }
   else if( input < inputMin_ )
   {
      return outputMin_ ;
   }

   unsigned offset = input - inputMin_ ;

   if( inputBigger_ )
   {
      offset *= 256 ;
      offset /= ratio_ ;
   }
   else
   {
      offset *= ratio_ ;
      offset /= 256 ;
   }

   int output = offset + outputMin_ ;
   if( output < outputMin_ )
      output = outputMin_ ;
   else if( output > outputMax_ )
      output = outputMax_ ;

   return output ;
}

#endif

