#ifndef __FBMEM_H__
#define __FBMEM_H__ "$Id: fbMem.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * fbMem.h
 *
 * This header file declares the fbMemory_t class,
 * which is used to allocate memory from the SM-501
 * graphics controller memory
 *
 *
 * Change History : 
 *
 * $Log: fbMem.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class fbMemory_t {
   static fbMemory_t &get( void ); // get singleton
   static void destroy( void ); // clean up


   // returns an offset to the memory object 
   //    or zero if the allocation fails
   unsigned alloc( unsigned size );

   // free an object
   void free( unsigned offs );

   fbMemory_t( void );
   fbMemory_t( fbMemory_t const & );
   ~fbMemory_t( void );

   int fd_ ;
   
   friend class fbPtrImpl_t ;
   friend class fbPtr_t ;
};

// don't look here (used by fbPtr_t)  
class fbPtrImpl_t {
   fbPtrImpl_t( void )
      : count_( 0 )
      , ptr_( 0 )
      , size_( 0 ){}

   fbPtrImpl_t( unsigned offs, void *ptr, unsigned size )
      : count_( 1 )
      , offs_( offs )
      , ptr_( ptr )
      , size_( size ){}

   ~fbPtrImpl_t( void );

   void addRef( void ){ ++count_ ; }
   void releaseRef( void );

   unsigned count_ ;
   unsigned offs_ ;
   void    *ptr_ ;
   unsigned size_ ;
   friend class fbPtr_t ;
};

class fbPtr_t {
public:
   fbPtr_t( void );
   fbPtr_t( fbPtr_t const &rhs );
   fbPtr_t( unsigned size );

   ~fbPtr_t( void );

   fbPtr_t &operator=( fbPtr_t const &rhs );

   void *getPtr(void) const { return inst_ ? inst_->ptr_ : 0 ; }
   unsigned getOffs(void) const { return inst_ ? inst_->offs_ : 0 ; }
   unsigned size( void ) const ;

private:
   fbPtrImpl_t *inst_ ;
};

#endif

