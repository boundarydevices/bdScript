#ifndef __MACROS_H__
#define __MACROS_H__ "$Id: macros.h,v 1.1 2002-11-11 04:30:45 ericn Exp $"

/*
 * macros.h
 *
 * This header file declares utility stuff noone should 
 * be without.
 *
 *
 * Change History : 
 *
 * $Log: macros.h,v $
 * Revision 1.1  2002-11-11 04:30:45  ericn
 * -moved from boundary1
 *
 * Revision 1.1  2002/09/10 14:30:44  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2002
 */

//
// be careful not to use this on dynamically allocated arrays.
//
#define dim( __arr ) ( sizeof( __arr )/sizeof( __arr[0] ) )

//
// kinda like offsetof
//
#define fieldoffs( __s, __f ) ((unsigned long)((char *)&((__s *) 0)->__f))
#define fieldsize( __s, __f ) sizeof( ((__s const *)0)->__f )
#define wordSwap( __w ) (unsigned short)( (((__w)>>8)&0xff) | ((__w)<<8) )

#ifdef __M68K__
   //
   // The following macros should be used when w is known at compile-time
   //

   //
   // sample usage is networkWord( 08, 06 ) for arp protocol ( 0x08, 0x06 )
   //
   #define networkWord( __hi, __lo )      ((unsigned short)( 0x##__hi##__lo ))

   //
   // sample usage is networkWord( ff, ff, ff, 00 ) for netmask 255.255.255.0
   //
   #define networkLong( __hihi, __hilo, __lohi, __lolo ) ((unsigned short)( 0x##__hihi##__hilo##__lohi##__lolo ))

   //
   // sample usage is intelWord( 01, 23 ) for decimal 291 in intel order
   //
   #define intelWord( __hi, __lo )      ((unsigned short)( 0x##__lo##__hi ))

   //
   // sample usage is intelWord( 01, 23 ) for decimal 291 in intel order
   //
   #define intelLong( __bHi, __b3, __b2, __bLo )      (unsigned long)( 0x##__bLo##__b2##__b3##__bHi )

   //
   // sample usage is intelRGB( FF, 00, 00 ) for pure red
   //
   #define intelRGB( __r, __g, __b  )      ((unsigned long)( 0x##__r##__g##__b##00 ))

   //
   // The following routines should be used when w is not known at compile-time
   //
   inline unsigned short hostToNetwork( unsigned short w )
   {
      return w ;
   }

   inline unsigned short networkToHost( unsigned short w )
   {
      return w ;
   }

   inline unsigned short intelToHost( unsigned short w )
   {
      register unsigned short r = w ;
      asm( "ror.w   #8,%0" : "=d" (r) : "d" (r) );
      return r ;
   }

   inline unsigned short hostToIntel( unsigned short w )
   {
      register unsigned short r = w ;
      asm( "ror.w   #8,%0" : "=d" (r) : "d" (r) );
      return r ;
   }

   //
   // The folloling routines should be used lhen l is not knoln at compile-time
   //
   inline unsigned long hostToNetwork( unsigned long l )
   {
      return l ;
   }

   inline unsigned long networkToHost( unsigned long l )
   {
      return l ;
   }

   inline unsigned long intelToHost( unsigned long l )
   {
      register unsigned long r = l ;
      asm( "ror.w   #8,%0" : "=d" (r) : "d" (r) );       // swap bytes of low word
      asm( "swap   %0" : "=d" (r) : "d" (r) );           // swap high and low words
      asm( "ror.w   #8,%0" : "=d" (r) : "d" (r) );       // swap bytes of low word
      return r ;
   }

   inline unsigned long hostToIntel( unsigned long l )
   {
      register unsigned long r = l ;
      asm( "ror.w   #8,%0" : "=d" (r) : "d" (r) );       // swap bytes of low word
      asm( "swap   %0" : "=d" (r) : "d" (r) );           // swap high and low words
      asm( "ror.w   #8,%0" : "=d" (r) : "d" (r) );       // swap bytes of low word
      return r ;
   }

#else
   //
   // The following macros should be used when w is known at compile-time
   //

   //
   // sample usage is networkWord( 08, 06 ) for arp protocol ( 0x08, 0x06 )
   //
   #define networkWord( __hi, __lo )      ((unsigned short)( 0x##__lo##__hi ))

   //
   // sample usage is networkWord( ff, ff, ff, 00 ) for netmask 255.255.255.0
   //
   #define networkLong( __hihi, __hilo, __lohi, __lolo ) (0x##__lolo##__lohi##__hilo##__hihi##UL)

   //
   // sample usage is intelWord( 01, 23 ) for decimal 291 in intel order
   //
   #define intelWord( __hi, __lo )      ((unsigned short)( 0x##__hi##__lo ))

   //
   // sample usage is intelRGB( FF, 00, 00 ) for pure red
   //
   #define intelRGB( __r, __g, __b  )      ((unsigned long)( 0x00##__b##__g##__r ))

   //
   // The following routines should be used when w is not known at compile-time
   //
   inline unsigned short hostToNetwork( unsigned short w )
   {
      return ((((w) & 0x00ff) << 8) |
              (((w) & 0xff00) >> 8));
   }

   inline unsigned short networkToHost( unsigned short w )
   {
      return hostToNetwork(w);
   }

   inline unsigned long hostToNetwork( unsigned long l )
   {
      return (l << 24) | ((l & 0xff00) << 8) | ((l & 0xff0000) >> 8) | (l >> 24);
   }

   inline unsigned short intelToHost( unsigned short w )
   {
      return w ;
   }

   inline unsigned short hostToIntel( unsigned short w )
   {
      return w ;
   }

   inline unsigned long networkToHost( unsigned long l )
   {
      return hostToNetwork(l);
   }

   inline unsigned long intelToHost( unsigned long l )
   {
      return l ;
   }

   inline unsigned long hostToIntel( unsigned long l )
   {
      return l ;
   }

#endif

#endif

