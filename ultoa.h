#ifndef __ULTOA_H__
#define __ULTOA_H__ "$Id: ultoa.h,v 1.1 2002-11-11 04:30:46 ericn Exp $"

/*
 * ultoa.h
 *
 * This header file declares the ultoa_t class, which is
 * used to convert unsigned numerics to strings.
 *
 *
 * Change History : 
 *
 * $Log: ultoa.h,v $
 * Revision 1.1  2002-11-11 04:30:46  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

class ultoa_t {
public:
   typedef enum {
      binary  = 2,
      decimal = 10,
      hex     = 16
   } base_e ;

   ultoa_t( unsigned long value,
            signed char   minWidth,       // < 0 to left-justify, asserts <= 32
            char          padChar = ' ',
            base_e        radix = decimal );

   char const *getValue( void ) const { return charValue_ ; }

private:
   char charValue_[ 33 ]; // worst case 32 bits + null
};

//
// this macro will return a normal hex value for any unsigned numeric type
//

#define HEX_VALUE( __v ) ultoa_t( (unsigned long)(__v), sizeof(__v)*2, '0', ultoa_t::hex ).getValue()
#define DECIMAL_VALUE( __v ) ultoa_t( (__v), sizeof(__v)*3, ' ', ultoa_t::decimal ).getValue()
#define DECIMAL_VALUE_NOPAD( __v ) ultoa_t( (__v), 1, '0', ultoa_t::decimal ).getValue()
#define BINARY_VALUE( __v ) ultoa_t( (__v), sizeof(__v)*8, '0', ultoa_t::binary ).getValue()

#endif

