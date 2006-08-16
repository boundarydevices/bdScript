#ifndef __ODOMVQ_H__
#define __ODOMVQ_H__ "$Id: odomVQ.h,v 1.1 2006-08-16 17:31:05 ericn Exp $"

/*
 * odomVQ.h
 *
 * This header file declares the odometerVideoQueue_t
 * class, which is used for buffering video in an odometer
 * application.
 *
 *
 * Change History : 
 *
 * $Log: odomVQ.h,v $
 * Revision 1.1  2006-08-16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class odometerVideoQueue_t {
public:
   odometerVideoQueue_t();
   ~odometerVideoQueue_t( void );

   enum {
      MAXFRAMES = 32       // should be power of 2
   };

   void init( unsigned w, unsigned h ); // clear between videos
   unsigned frameSize( void ) const { return frameSize_ ; }
   unsigned numDropped( void ) const { return dropped_ ; }

   // ------ filler interface -------
   
   // returns 0 if queue is full
   unsigned char *pullEmpty( void );
   void putFull( unsigned char *, long long pts );
   bool started( void ) const ;
   void start( void ); // first pts is calculated here
   bool isFull( void ) const { return ((addFull_+1)%MAXFRAMES) == takeFull_ ; }

   // ------ emptier interface ------

   // returns 0 if empty or not yet time to play
   unsigned char *pull( unsigned vsync, long long *pts );
   void putEmpty( unsigned char * );
   bool isEmpty( void ) const { return addFull_ == takeFull_ ; }

   inline long long startPTS( void ) const { return firstPTS_ ; }
   void dumpHeap(void);

private:
   odometerVideoQueue_t( odometerVideoQueue_t const & ); // no copies

   unsigned char *pullHeap( long long now, long long *pts );

   void          *frameMem_ ;
   unsigned       frameSize_ ;

   typedef struct entry_t {
      long long      pts_ ;
      unsigned char *data_ ;
   };

   unsigned long  dropped_ ;

   // free list is a circular queue
   unsigned volatile addFree_ ;
   unsigned volatile takeFree_ ;
   entry_t           freeFrames_[MAXFRAMES];

   // so is full list
   unsigned volatile addFull_ ;
   unsigned volatile takeFull_ ;
   entry_t           fullFrames_[MAXFRAMES];

   long long         firstPTS_ ;

   // flags prevent pulling while heap is 
   // being manipulated
   enum {
      HEAPINUSE   = 1
   };

   unsigned long volatile flags_ ;
};

#endif

