#ifndef __PALETTE_H__
#define __PALETTE_H__ "$Id: palette.h,v 1.1 2004-04-07 12:45:21 ericn Exp $"

/*
 * palette.h
 *
 * This header file declares the palette_t class, which 
 * is used to create a palette of the specified depth 
 * based on the octree algorithm, more or less as described
 * in Graphics Gems.
 *
 *
 * Change History : 
 *
 * $Log: palette.h,v $
 * Revision 1.1  2004-04-07 12:45:21  ericn
 * -Initial import
 *
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */

class palette_t {
public:
   palette_t( void );
   ~palette_t( void );

   // --- construction methods
      // build tree by repeatedly calling this routine for each pixel
      void nextIn( unsigned char r, unsigned char g, unsigned char b );
      
      // call this when finished to build the palette
      void buildPalette();

   // --- processing methods
      unsigned numPaletteEntries( void ) const { return numLeaves_ ; }
      unsigned maxDepth( void ) const ;

      // use this after construction to get value for each pixel
      // guaranteed to return true for all values passed to nextIn during construction
      bool getIndex( unsigned char r, 
                     unsigned char g, 
                     unsigned char b,
                     unsigned char &index ) const ;
      
      // and this to get RGB values for each palette entry
      void getRGB( unsigned char  index,
                   unsigned char &r,
                   unsigned char &g,
                   unsigned char &b );

   // -- debug routine
   void dump( void ) const ;
   bool verifyPixels( char const *when );
   unsigned pixelsIn( void ) const { return pixelsIn_ ; }

private:
   struct octreeEntry_t {
      unsigned childPixels(void) const ;

      unsigned       r_ ;
      unsigned       g_ ; 
      unsigned       b_ ;
      unsigned       numPix_ ;         // <> 0 means leaf
      unsigned char  paletteIdx_ ;
      octreeEntry_t *sibling_ ;        // linked list at each level
      octreeEntry_t *children_[8];
   };

   struct nodeAndLevel_t {
      palette_t :: octreeEntry_t const *node_ ;
      unsigned                          level_ ;
   };
   
   struct chunk_t {
      chunk_t       *next_ ;
      octreeEntry_t  entries_[256];
   };

   void allocChunk();
   void reduce();
   octreeEntry_t *allocate();
   void free(octreeEntry_t *);

   unsigned              numLeaves_ ;
   unsigned char         palette_[3*256];
   unsigned              maxDepth_ ;
   unsigned              pixelsIn_ ;
   chunk_t              *chunks_ ;        // all entries (alloc result)
   octreeEntry_t        *levelHeads_[7];    // non-leaf entries at each level
   octreeEntry_t        *head_ ;
   octreeEntry_t        *freeHead_ ;       // list of all free entries (via sibling_)
};


#endif

