#ifndef __SERIALPOLL_H__
#define __SERIALPOLL_H__ "$Id: serialPoll.h,v 1.1 2004-03-27 20:24:22 ericn Exp $"

/*
 * serialPoll.h
 *
 * This header file declares the serialPoll_t class, which is
 * used to handle I/O to a device connected to a serial port
 * in a polled fashion. This class comes pretty close to the 
 * interface described in jsSerial.h. In particular, it handles 
 * input in the same three modes:
 *
 *       Terminated
 *       Timed-out
 *       Character
 *
 *
 * Change History : 
 *
 * $Log: serialPoll.h,v $
 * Revision 1.1  2004-03-27 20:24:22  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

#include "pollHandler.h"
#include "pollTimer.h"

#include <string>

class serialPollTimer_t ;

class serialPoll_t : public pollHandler_t {
public:
   serialPoll_t( pollHandlerSet_t &set,
                 char const       *devName = "/dev/ttyS1",
                 int               baud = 115200,
                 int               databits = 8,
                 char              parity = 'N',
                 int               outDelay = 0,           // inter-character delay on output
                 char              terminator = '\0',      // end-of-line char
                 int               inputTimeout = 10 );    // ms 'til end of line
   ~serialPoll_t( void );

   // override this to perform processing of a received line
   virtual void onLineIn( void );
   
   // override this to perform processing of any input characters
   // (This routine is just a notification, use read() to extract
   // the input)
   virtual void onChar( void );

   struct line_t {
      line_t  *next_ ;
      unsigned length_ ;
      char     data_[1];
   };

   //
   // data access routines
   //
   //    - read a line of text (dequeues it as well)
   //    returns false when nothing is present
   //    terminators have been stripped
   bool readln( std::string & );

   //    - read (and dequeue) all available data
   //    returns false when nothing is present
   bool read( std::string & );

   bool         haveData( void ) const { return 0 != inLength_ ; }

   // general pollHandler input routine
   virtual void onDataAvail( void );
   void         timeout( void );

   bool         haveTerminator( void ) const { return '\0' != terminator_ ; }

   int          write( void const *data, int length ) const ;
   int          outputDelay( void ) const { return outDelay_ ; }
   void         setOutputDelay( int value ){ outDelay_ = value ; }

protected:
   void addLine( char const *data, unsigned len );
   serialPollTimer_t *timer_ ;
   int            outDelay_ ;
   char           inData_[512];
   unsigned       inLength_ ;
   unsigned       inputTimeout_ ;
   char const     terminator_ ;
   line_t        *lines_ ;
   line_t        *endLine_ ;
};


#endif

