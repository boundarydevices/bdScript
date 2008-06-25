#ifndef __UBOOTPARAMS_H__
#define __UBOOTPARAMS_H__ "$Id: uBootParams.h,v 1.1 2008-06-25 22:07:24 ericn Exp $"

/*
 * uBootParams.h
 *
 * This header file declares the uBootParams_t class,
 * which attempts to read and parse a set of U-Boot
 * parameters saved in flash, building a linked list
 * of name/value pairs.
 *
 * Change History : 
 *
 * $Log: uBootParams.h,v $
 * Revision 1.1  2008-06-25 22:07:24  ericn
 * -Import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

class uBootParams_t {
public:
   uBootParams_t( char const *fileName );
   ~uBootParams_t( void );

   bool worked( void ) const { return worked_ ; }

   // returns null if not found
   char const *find( char const *name ) const ;

   // returns true if it finds enough space for the item
   bool set( char const *name, char const *value );

   // returns true if it wrote the params to the specified file or device
   bool save( char const *fileName );

private:
   friend class uBootParamIter_t ;
   typedef struct _listItem {
      struct _listItem *next_ ;
      char const *name_ ;
      char const *value_ ;
   } listItem_t ;

   bool         worked_ ;
   listItem_t  *head_ ;
   unsigned     dataMax_ ;
   unsigned     dataLen_ ;
   char        *dataBuf_ ;
};

class uBootParamIter_t {
public:
   uBootParamIter_t( uBootParams_t const &params ){ next_ = params.head_ ; }
   ~uBootParamIter_t( void ){}

   inline bool valid( void ) const { return 0 != next_ ; }
   inline char const *name( void ) const { return valid() ? next_->name_ : "" ; }
   inline char const *value( void ) const { return valid() ? next_->value_ : "" ; }
   inline void operator++( void ){ next_ = next_ ? next_->next_ : 0 ; }

   // used when relocating values during uBootParams_t::set()
   inline void changeValue( char const *newValue ){ if( next_ ){ next_->value_ = newValue ; } }
private:
   uBootParams_t::listItem_t *next_ ;
};

#endif

