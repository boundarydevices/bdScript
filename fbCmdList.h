#ifndef __FBCMDLIST_H__
#define __FBCMDLIST_H__ "$Id: fbCmdList.h,v 1.2 2006-10-16 22:36:34 ericn Exp $"

/*
 * fbCmdList.h
 *
 * This header file declares the fbCommand_t base class
 * and the fbCommandList_t collection for use in constructing
 * command strings for the SM-501 command-list interpreter.
 *
 *
 * Change History : 
 *
 * $Log: fbCmdList.h,v $
 * Revision 1.2  2006-10-16 22:36:34  ericn
 * -make fbJump_t a full-fledged fbCommand_t
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

#include <vector>

class fbCommand_t {
public:
   fbCommand_t( unsigned size ): size_( size ){}
   virtual ~fbCommand_t( void ){}

   unsigned size( void ) const { return size_ ; }
   virtual void const *data( void ) const = 0 ;
   virtual void retarget( void *data );

private:
   fbCommand_t( fbCommand_t const & ); // no copies
   unsigned size_ ;
};


class fbCommandList_t {
public:
   fbCommandList_t( void );
   ~fbCommandList_t( void );

   // by pushing a command, you're giving ownership to the 
   // command list. The command will be delete'd in the destructor.
   void push( fbCommand_t *cmd );

   // return the total size (in bytes) of the command list
   unsigned size( void ) const { return size_ ; }

   // copy the command list (generally to frame-buffer memory)
   void copy( void *where );

   // 
   void dump();

private:
   unsigned                   size_ ;
   std::vector<fbCommand_t *> commands_ ;
};


class fbJump_t : public fbCommand_t {
public:
   fbJump_t( unsigned numBytes );

   virtual void const *data( void ) const ;
   virtual void retarget( void *data );

   // used to implement conditional jumps
   void setLength( unsigned bytes );

   unsigned long  cmdData_[2];
   unsigned long *cmdPtr_ ;
};


#endif

