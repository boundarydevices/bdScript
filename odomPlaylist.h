#ifndef __ODOMPLAYLIST_H__
#define __ODOMPLAYLIST_H__ "$Id: odomPlaylist.h,v 1.4 2006-09-04 15:16:51 ericn Exp $"

/*
 * odomPlaylist.h
 *
 * This header file declares the odomPlaylist_t class,
 * which is a container for a set of media playback instructions.
 *
 * Currently, Macromedia Flash (.swf) and MPEG-1/2 movies are
 * supported, as are still images (.png,.gif,.jpg).
 *
 * Synchronization is performed based on the vertical sync count.
 *
 * Change History : 
 *
 * $Log: odomPlaylist.h,v $
 * Revision 1.4  2006-09-04 15:16:51  ericn
 * -add audio interfaces
 *
 * Revision 1.3  2006/08/26 16:05:16  ericn
 * -allow access to YUV without geometry
 *
 * Revision 1.2  2006/08/20 18:16:54  ericn
 * -fix speeling
 *
 * Revision 1.1  2006/08/16 17:31:05  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2006
 */

class odomCmdInterp_t ;

typedef enum playlistType_t {
   PLAYLIST_NONE       = -1,
   PLAYLIST_STILLIMAGE = 0,
   PLAYLIST_MPEG       = 1,
   PLAYLIST_FLASH      = 2,
   PLAYLIST_CMD        = 3,        // fileName_ is interpreted as a command
   PLAYLIST_STREAM     = 4
};

typedef struct playlistEntry_t {
   playlistType_t    type_ ;
   bool              repeat_   ; // add to end of list on completion?
   unsigned          numTicks_ ; // duration for still images, ignored otherwise
   unsigned          x_ ;
   unsigned          y_ ;
   unsigned          w_ ;
   unsigned          h_ ;
   char              fileName_[512];
};

class odomPlaylist_t {
public:
   odomPlaylist_t( void );
   ~odomPlaylist_t( void );

   enum {
      MAXENTRIES  = 16     // must be power of 2
   };

   void add( playlistEntry_t const &entry );
   void stop( void ); // stop current animation, move to next
   void purge( void ); // clear everything

   bool dispatch( odomCmdInterp_t   &interp,
                  char const * const params[],
                  unsigned           numParams,
                  char              *errorMsg,
                  unsigned           errorMsgLen );

   //
   // playback interface
   //
   void play( unsigned long syncCount );

   // 
   // interface for media players (flash, MPEG)
   //
   int fdYUV( void );
   int fdYUV( unsigned inw,
              unsigned inh,
              unsigned outx, 
              unsigned outy, 
              unsigned outw, 
              unsigned outh );
   void closeYUV( void );

   void vsyncHandler( void );

   int fdDsp( void );
   int fdDsp( unsigned speed, unsigned channels );
   void closeDsp( void );

   void setVolume( unsigned volume );

   void dump( void );
private:
   void next( unsigned long syncCount );

   void             *playing_ ;     // handle to flash or MPEG data, start tick for stills
   playlistEntry_t   current_ ;
   playlistEntry_t   entries_[MAXENTRIES];
   unsigned          add_ ;
   unsigned          take_ ;
   int               fdYUV_ ;
   unsigned          yuvInW_ ;
   unsigned          yuvInH_ ;
   unsigned          yuvOutX_ ;
   unsigned          yuvOutY_ ;
   unsigned          yuvOutW_ ;
   unsigned          yuvOutH_ ;
   int               fdDsp_ ;
   unsigned          channels_ ;
   unsigned          speed_ ;
};


extern odomPlaylist_t *lastPlaylistInst_ ;

#endif

