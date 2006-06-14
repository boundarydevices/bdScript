#ifndef __DICTIONARY_H__
#define __DICTIONARY_H__ "$Id: dictionary.h,v 1.1 2006-06-14 13:54:05 ericn Exp $"

/*
 * dictionary.h
 *
 * This header file declares the dictionary_t template,
 * which is used to consolidate storage of a set of unique
 * values by index (usually to reduce the storage size).
 *
 * Change History : 
 *
 * $Log: dictionary.h,v $
 * Revision 1.1  2006-06-14 13:54:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <map>
#include <vector>

template<typename T>
class dictionary_t {
public:
   dictionary_t( void ){}
   ~dictionary_t( void ){}
   
   // add or locate an item, return index
   unsigned operator+=( T const &v );
   
   // access an item by index
   T const &operator[]( unsigned idx ) const ;
   
   unsigned size(void) const { return items_.size(); }

   typedef T value_t ;
   typedef std::map<T,unsigned>          vtoi_t ;
   typedef typename vtoi_t::iterator     iterator ;

private:
   dictionary_t( dictionary_t const & ); // no copies
   
   std::vector<T>       items_ ;
   vtoi_t               byValue_ ;
};


template<typename T>
unsigned dictionary_t<T>::operator+=( T const &v )
{
   iterator iter = byValue_.find(v);
   if( iter == byValue_.end() )
   {
      unsigned const idx = items_.size();
      byValue_[v] = idx ;
      items_.push_back(v);
      return idx ;
   }
   else
      return (*iter).second ;   
}
   
   // access an item by index
template<typename T>
T const &dictionary_t<T>::operator[]( unsigned idx ) const 
{
   return items_[idx];
}

#endif

