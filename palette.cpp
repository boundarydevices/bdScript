/*
 * Module palette.cpp
 *
 * This module defines the methods of the palette_t
 * class as declared in palette.h
 *
 *
 * Change History : 
 *
 * $Log: palette.cpp,v $
 * Revision 1.1  2004-04-07 12:45:21  ericn
 * -Initial import
 *
 *
 * Copyright Boundary Devices, Inc. 2004
 */


#include "palette.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

// #define __DEBUG 1

inline void DEBUG( FILE *f, char const *fmt, ... )
{
#ifdef __DEBUG
   va_list ap;
   va_start(ap, fmt);

   vfprintf( f, fmt, ap );

   va_end(ap);
#endif
}

palette_t :: palette_t( void )
   : numLeaves_( 0 )
   , maxDepth_( 7 )
   , pixelsIn_( 0 )
   , chunks_( 0 )
   , head_( 0 )
   , freeHead_( 0 )
{
   memset( levelHeads_, 0, sizeof(levelHeads_) );
   allocChunk();
   levelHeads_[0] = head_ = allocate();
}

palette_t :: ~palette_t( void )
{
   while( chunks_ )
   {
      chunk_t *const ch = chunks_ ;
      chunks_ = ch->next_ ;
      delete ch ;
   }

DEBUG( stderr, "return from destructor\n" );
}

static bool          _tracing = false ;
static unsigned char traceR_ = 0x3d ;
static unsigned char traceG_ = 0x3d ;
static unsigned char traceB_ = 0x3d ;

static void doTrace( unsigned char r, unsigned char g, unsigned char b )
{
   traceR_ = r ;
   traceG_ = g ;
   traceB_ = b ;
   _tracing = true ;
}

static bool shouldTrace( unsigned char r, unsigned char g, unsigned char b )
{
   return _tracing 
          && 
          ( traceR_ == r )
          && 
          ( traceG_ == g )
          && 
          ( traceB_ == b );
}

void palette_t :: allocChunk()
{
   palette_t :: chunk_t *ch = new chunk_t ;

   memset( ch->entries_, 0, sizeof(ch->entries_) );

   // build free list
   for( unsigned i = 0 ; i < 255 ; i++ )
      ch->entries_[i].sibling_ = ch->entries_ + i + 1 ;
   ch->entries_[255].sibling_ = 0 ;

   assert( 0 == freeHead_ ); // or why bother?
   freeHead_ = ch->entries_ ;
   ch->next_ = chunks_ ;
   chunks_ = ch ;
}

inline unsigned char CHILDIDX( unsigned char __m, unsigned char __r, unsigned char __g, unsigned char __b )
{
   bool rSet = 0 != ( __m & __r );
   bool gSet = 0 != ( __m & __g );
   bool bSet = 0 != ( __m & __b );
   unsigned char const rval = ( rSet << 2 ) | ( gSet << 1 ) | bSet ;
   assert( 8 > rval );
   return rval ;
}

// build tree by repeatedly calling this routine
void palette_t :: nextIn( unsigned char r, unsigned char g, unsigned char b )
{
   bool const trace = shouldTrace( r, g, b );
   if( trace )
      fprintf( stderr, "\ntracing walk for %02x/%02x/%02x\n", r, g, b );

   //
   // Look for a matching node, keeping track of parent
   // for insertion if necessary.
   //
   // There are two outcomes here:
   //    1. We encounter a leaf node. Add our pixel value and return.
   //    2. We reach the end of the tree. This means we're on a 
   //       different branch than anybody else.
   //    3. We run out of input bits. This should never happen.
   // 
   unsigned char mask = 0x80 ;
   octreeEntry_t *parent = 0 ;
   octreeEntry_t *next = head_ ;

   unsigned char depth = 0 ;
   while( 0 != next )
   {
if( trace )
fprintf( stderr, "--> %p/%u\n", next, depth );
      if( 0 == next->numPix_ ) // no leaves
      {
         parent = next ;
         unsigned char const childId = CHILDIDX(mask,r,g,b);
if( trace )
fprintf( stderr, "childId %u\n", childId );
         next = next->children_[childId];
         if( next )
         {
            mask >>= 1 ;
            depth++ ;
         }
      }
      else
      {
if( trace )
fprintf( stderr, "--> %p/%u (leaf)\n", next, depth );
         ++pixelsIn_ ;
         next->numPix_++ ;
         next->r_ += r ;
         next->g_ += g ;
         next->b_ += b ;
DEBUG( stderr, "found leaf at level %u\n", depth );
         return ;
      }
   }

   if( trace )
      fprintf( stderr, "--> parent %p/next %p, depth %u\n", parent, next, depth );

   // not found. add a leaf and reduce if necessary

   // go around again because our parent may be a leaf
   DEBUG( stderr, "insert:%u:%u\n", numLeaves_, depth );

   assert( 0 != parent ); // head is pre-allocated. should never happen
   if( 0 == mask )
   {
      fprintf( stderr, "\nran out of bits!!!! tracing to see how this happened!\n"
                       "(should have found a leaf node before now)\n" 
                       "next: %p, parent %p, depth %u, mask %02x\n", next, parent, depth, mask );
      if( !trace )
      {
         doTrace( r,g,b );
         nextIn( r, g, b );
      }
      exit( 0 );
   }

   //
   // build any intermediate nodes
   //
   while( ( 1 < mask ) && ( maxDepth_ > depth ) )
   {
      unsigned idx = CHILDIDX(mask,r,g,b);

if( trace )
fprintf( stderr, "--> childId %u\n", idx );
      if( 0 == parent->children_[idx] )
      {
         next = allocate();
if( trace )
fprintf( stderr, "--> allocate %p/%u\n", next, idx );
         DEBUG( stderr, "alloc %p/%u/%u\n", next, depth, next->numPix_ );
         // attach to parent and level chain
         parent->children_[idx] = next ;
         
         next->sibling_ = levelHeads_[depth];
         levelHeads_[depth] = next ;
   
         parent = next ; // for next iteration
      }
      else
      {
         parent = parent->children_[idx];
         DEBUG( stderr, "same parent: %u\n", depth );
if( trace )
fprintf( stderr, "--> same parent %p/%u\n", parent, idx );
      }
      mask >>= 1 ;
      depth++ ;
   }

   assert( 1 <= mask );

   //
   // build or add to leaf node
   // 
   unsigned const idx = CHILDIDX(mask,r,g,b);
   if( 0 != parent->numPix_ )
   {
fprintf( stderr, "!!!!!! parent is a leaf !!!!!!\n" );
fprintf( stderr, "\nran out of bits!!!! tracing to see how this happened!\n"
                 "(should have found a leaf node before now)\n" 
                 "next: %p, parent %p, depth %u, mask %02x\n", next, parent, depth, mask );
exit( 0 );
      ++pixelsIn_ ;
      parent->r_ += r ;
      parent->g_ += g ;
      parent->b_ += b ;
      parent->numPix_++ ;
   } // non-leaf parent
   else if( 0 == parent->children_[idx] )
   {
      DEBUG( stderr, "add leaf at level %u, parent %p\n", depth, parent );
      ++pixelsIn_ ;
      octreeEntry_t *next = allocate();
      parent->children_[idx] = next ;
      next->r_ = r ;
      next->g_ = g ;
      next->b_ = b ;
      next->numPix_ = 1 ;
      ++numLeaves_ ;
   }
   else
   {
      ++pixelsIn_ ;
      octreeEntry_t *next = parent->children_[idx];
      next->r_ += r ;
      next->g_ += g ;
      next->b_ += b ;
      next->numPix_++ ;
      DEBUG( stderr, "---> merge %p/%u/%p\n", parent, idx, next );
   } // merging here and now
   
   reduce();
}

unsigned palette_t::octreeEntry_t::childPixels( void ) const 
{
   unsigned count = 0 ; 
   for( unsigned i = 0 ; i < 8 ; i++ )
   {
      octreeEntry_t const *const child = children_[i];
      if( child )
      {
         assert( 0 < child->numPix_ );
         count += child->numPix_ ;
      }
   }
   return count ;
}

void palette_t :: reduce()
{
   while( 256 <= numLeaves_ )
   {
      DEBUG( stderr, "reduce:%u\n", numLeaves_ );
      unsigned depth = 6 ;

      while( ( 0 < depth ) && ( 0 == levelHeads_[depth] ) )
         --depth ;
      if( 0 == depth )
      {
         DEBUG( stderr, "Error: depth reached zero\n" );
         dump();
         exit(0);
      }
      else
         DEBUG( stderr, "reduce from level %u\n", depth );

      octreeEntry_t *rear = 0 ;
      octreeEntry_t *front = levelHeads_[depth];

      // find best candidate for merge (most pixels in children)
      octreeEntry_t *bestPred = 0 ;
      octreeEntry_t *best = 0 ;
   
      unsigned maxPixels = 0 ;
      while( front )
      {
         if( 0 != front->numPix_ ) // this should be a non-leaf node
         {
            DEBUG( stderr, "leaf node still in level chain: %p -> %p, level %u\n", rear, front, depth );
         }
         assert( 0 == front->r_ );
         assert( 0 == front->g_ );
         assert( 0 == front->b_ );
         unsigned const childPixels = front->childPixels();

         // children of the lowest level must be leaves
         assert( 0 < childPixels );

         if( childPixels > maxPixels )
         {
            best = front ;
            bestPred = rear ;
            maxPixels = childPixels ;
         }
         rear = front ;
         front = rear->sibling_ ;
      }

      assert( 0 != best );
      assert( 0 < maxPixels );
   
DEBUG( stderr, "remove %u pixels from children of %p, pred %p\n", maxPixels, best, bestPred );
      // remove from non-leaf list for this level
      if( bestPred )
      {
DEBUG( stderr, "not levelHead\n" );
         assert( bestPred->sibling_ == best );
         bestPred->sibling_ = best->sibling_ ;
      }
      else
      {
DEBUG( stderr, "levelHead: %u\n", depth );
         assert( levelHeads_[depth] == best );
         levelHeads_[depth] = best->sibling_ ;
         if( 0 == levelHeads_[depth] )
            maxDepth_ = depth ;
      }

      best->sibling_ = 0 ;
      best->numPix_  = 0 ; // added below

      // make best the sum of its' children
      for( unsigned i = 0 ; i < 8 ; i++ )
      {
         octreeEntry_t *const child = best->children_[i];
         if( child )
         {
            best->children_[i] = 0 ;
            assert( 0 < child->numPix_ );
            best->numPix_ += child->numPix_ ;
            best->r_ += child->r_ ;
            best->g_ += child->g_ ;
            best->b_ += child->b_ ;
            free( child );
DEBUG( stderr, "release %p, numPix %u\n", child, best->numPix_ );
            numLeaves_-- ;
         }
      }
DEBUG( stderr, "parent %u\n", depth );
      assert( maxPixels == best->numPix_ );
      numLeaves_++ ;
   }
}

#include <list>

void palette_t :: dump( void ) const 
{
   std::list<palette_t :: nodeAndLevel_t> nodeStack ;
   if( head_ )
   {
      fprintf( stderr, "--> dumping tree from %p, %u pixels input\n", head_, pixelsIn_ );
      nodeAndLevel_t nl ;
      nl.node_  = head_ ;
      nl.level_ = 0 ;
      nodeStack.push_back( nl );

      while( 0 < nodeStack.size() )
      {
         nl = nodeStack.front();
         nodeStack.pop_front();
         fprintf( stderr, "%u:%p", nl.level_, nl.node_ );
         if( 0 < nl.node_->numPix_ )
         {
            fprintf( stderr, "  %u pixels, %u/%u/%u\n", nl.node_->numPix_, nl.node_->r_, nl.node_->g_, nl.node_->b_ );
         } // leaf
         else
         {
            for( unsigned i = 0 ; i < 8 ; i++ )
            {
               octreeEntry_t const *child = nl.node_->children_[i];
               fprintf( stderr, "   %p", child );
               if( child )
               {
                  nodeAndLevel_t nl2 ;
                  nl2.node_  = child ;
                  nl2.level_ = nl.level_ + 1 ;
                  nodeStack.push_back( nl2 );
               }
            }
            fprintf( stderr, "   --> %p\n", nl.node_->sibling_ );
         } // non-leaf
      }

      fprintf( stderr, "---> by levelHeads_\n" );
      for( unsigned i = 0 ; i < 7 ; i++ )
      {
         octreeEntry_t const *p = levelHeads_[i];
         fprintf( stderr, "lh[%u] == %p\n", i, p );
         while( p )
         {
            nodeAndLevel_t nl ;
            nl.node_  = p ;
            nl.level_ = i ;
            nodeStack.push_back( nl );
            p = p->sibling_ ;
         }
      }

      while( 0 < nodeStack.size() )
      {
         nl = nodeStack.front();
         nodeStack.pop_front();
         fprintf( stderr, "%u:%p", nl.level_, nl.node_ );
         
         if( 0 != nl.node_->numPix_ )
         {
            fprintf( stderr, "!!!!!!!!! non-leaf made it here !!!!!!!!!!!!!\n" );
            fprintf( stderr, "rgb: %u/%u/%u, numPix %u\n", nl.node_->r_, nl.node_->g_, nl.node_->b_, nl.node_->numPix_ );
         } // how did a non-leaf make it here

         for( unsigned i = 0 ; i < 8 ; i++ )
         {
            octreeEntry_t const *child = nl.node_->children_[i];
            fprintf( stderr, "   %p", child );
         }
         fprintf( stderr, "   --> %p\n", nl.node_->sibling_ );
      }
   }
   else
      fprintf( stderr, "nothing to dump: head is NULL\n" );
}

bool palette_t :: getIndex
   ( unsigned char r, 
     unsigned char g, 
     unsigned char b,
     unsigned char &index ) const 
{
   index = 0 ;
   unsigned char mask = 0x80 ;
   octreeEntry_t *next = head_ ;
   
   while( ( 0 != next ) && ( 0 != mask ) )
   {
      if( 0 == next->numPix_ ) // no leaves
      {
         unsigned childId = CHILDIDX(mask,r,g,b);
         octreeEntry_t *child = next->children_[childId];
/*
         if( 0 == child )
         {
            if( 0 < childId )
               child = next->children_[childId-1];
            if( 0 == child )
            {
               if( 7 > childId )
                  child = next->children_[childId+1];
            }
         }
*/         
         next = child ;
         mask >>= 1 ;
      }
      else
         break;
   }

   if( next )
   {
      index = next->paletteIdx_ ;
      return true ;
   }
   else
   {
#define __TRACE 1
#ifdef __TRACE
      fprintf( stderr, "error finding rgb: %02x/%02x/%02x\n", r, g, b );
      mask = 0x80 ;
      next = head_ ;
      unsigned depth = 0 ;
      while( ( 0 != next ) && ( 0 != mask ) )
      {
         fprintf( stderr, "%u %p: %02x\n", depth, next, mask );
         if( 0 == next->numPix_ )
         {
            unsigned const idx = CHILDIDX(mask,r,g,b);
            fprintf( stderr, "  childId: %x\n", idx );
            next = next->children_[CHILDIDX(mask,r,g,b)];
            mask >>= 1 ;
            depth++ ;
         }
         else
         {
            fprintf( stderr, "leaf!\n" );
         }
      }
      fprintf( stderr, "[end of chain]\n" );
#endif
      return false ;
   }
}

void palette_t :: getRGB
   ( unsigned char  index,
     unsigned char &r,
     unsigned char &g,
     unsigned char &b )
{
   unsigned char const *rgb = palette_ + (index*3);
   r = *rgb++ ;
   g = *rgb++ ;
   b = *rgb++ ;
}

palette_t :: octreeEntry_t *palette_t :: allocate()
{
   if( 0 == freeHead_ )
      allocChunk();
   assert( 0 != freeHead_ );
   palette_t :: octreeEntry_t *rval = freeHead_ ;
   freeHead_ = rval->sibling_ ;
   memset( rval, 0, sizeof( *rval ) );
// DEBUG( stderr, "alloc %p\n", rval );
   assert( 0 == rval->numPix_ );
   return rval ;
}

void palette_t :: free( octreeEntry_t *entry )
{
   entry->sibling_ = freeHead_ ;
   freeHead_ = entry ;
}


unsigned palette_t :: maxDepth() const 
{
   unsigned i = 6 ;
   while( 0 == levelHeads_[i] )
      --i ;
   return i ;
}

void palette_t :: buildPalette()
{
   int depth = 6 ;
   unsigned numEntries = 0 ;
   unsigned char *nextOut = palette_ ;
   unsigned numPixels = 0 ;

   //
   // walk from bottom level head, looking for leaves
   //
   while( ( 0 <= depth ) && ( numEntries < numLeaves_ ) )
   {
      octreeEntry_t *level = levelHeads_[depth];
      while( 0 != level )
      {
         for( unsigned i = 0 ; i < 8 ; i++ )
         {
            octreeEntry_t *child = level->children_[i];
            if( child && ( 0 < child->numPix_ ) )
            {
               numPixels += child->numPix_ ;
               child->paletteIdx_ = numEntries ;
               numEntries++ ;
// fprintf( stderr, "%3u %5u: %06x/%06x/%06x\n", child->paletteIdx_, child->numPix_, child->r_, child->g_, child->b_ );
               unsigned const halfPix = ( child->numPix_ / 2 );
               *nextOut++ = (unsigned char)( ( child->r_+halfPix) /child->numPix_ );
               *nextOut++ = (unsigned char)( ( child->g_+halfPix) /child->numPix_ );
               *nextOut++ = (unsigned char)( ( child->b_+halfPix) /child->numPix_ );

               if( numEntries == numLeaves_ )
                  return ;
            }
         }
         level = level->sibling_ ;
      }
      depth-- ;
   }

   // should never get here
   fprintf( stderr, "found %u of %u leaves at level %u (%u pixels)\n", numEntries, numLeaves_, ++depth, numPixels );
//   __assert_fail( "Missing leaves building palette", __FILE__, __LINE__, __PRETTY_FUNCTION__ );

}

bool palette_t :: verifyPixels( char const *when )
{
   unsigned counted = 0 ;
   for( unsigned level = 0 ; level < 7 ; level++ )
   {
      octreeEntry_t *node = levelHeads_[level];
      while( node )
      {
         assert( 0 == node->numPix_ );
         for( unsigned i = 0 ; i < 8 ; i++ )
         {
            octreeEntry_t *child = node->children_[i];
            if( child && ( 0 < child->numPix_ ) )
               counted += child->numPix_ ;
         }
         node = node->sibling_ ;
      }
   }
   if( counted != pixelsIn_ )
   {
      fprintf( stderr, "pixel mismatch (%s): %u out of %u\n", when, counted, pixelsIn_ );
      return false ;
   }
   else
      return true ;
}

#ifdef MODULETEST

//
// Note that most testing has nothing to do with image quality.
// Test based on a more or less random input file
//


#include <stdio.h>
#include "hexDump.h"

extern "C" {
#include <jpeglib.h>
};

#define BYTESPERMEMCHUNK 4096

struct memChunk_t {
   unsigned long  length_ ; // bytes used so far
   memChunk_t    *next_ ;   // next chunk
   JOCTET         data_[BYTESPERMEMCHUNK-8];
};

struct memDest_t {
   struct       jpeg_destination_mgr pub; /* public fields */
   memChunk_t  *chunkHead_ ;
   memChunk_t  *chunkTail_ ;
};

typedef memDest_t  *memDestPtr_t ;

static void dumpcinfo( jpeg_compress_struct const &cinfo )
{
   hexDumper_t dump( &cinfo, sizeof( cinfo ) );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
   fflush( stdout );
}

static void dumpMemDest( memDest_t const &md )
{
   hexDumper_t dump( &md, sizeof( md ) );
   while( dump.nextLine() )
      printf( "%s\n", dump.getLine() );
   fflush( stdout );
}

/*
 * Initialize destination --- called by jpeg_start_compress
 * before any data is actually written.
 */
static void init_destination (j_compress_ptr cinfo)
{
   memDestPtr_t dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 == dest->chunkHead_ );

   /* Allocate first memChunk */
   dest->chunkHead_ = dest->chunkTail_ = new memChunk_t ;
   memset( dest->chunkHead_, 0, sizeof( *dest->chunkHead_ ) );
   dest->pub.next_output_byte = dest->chunkHead_->data_ ;
   dest->pub.free_in_buffer   = sizeof( dest->chunkHead_->data_ );
}


/*
 * Empty the output buffer --- called whenever buffer fills up.
 */

static boolean empty_output_buffer (j_compress_ptr cinfo)
{
   memDestPtr_t const dest = (memDestPtr_t) cinfo->dest;
   
   assert( 0 != dest->chunkTail_ );

   //
   // free_in_buffer member doesn't seem to be filled in
   //
   dest->chunkTail_->length_ = sizeof( dest->chunkTail_->data_ ); //  - dest->pub.free_in_buffer ;

   memChunk_t * const next = new memChunk_t ;
   memset( next, 0, sizeof( *next ) );
   dest->chunkTail_->next_ = next ;
   dest->chunkTail_ = next ;
   
   dest->pub.next_output_byte = next->data_ ;
   dest->pub.free_in_buffer   = sizeof( next->data_ );

   return TRUE;
}


/*
 * Terminate destination --- called by jpeg_finish_compress
 * after all data has been written.  Usually needs to flush buffer.
 *
 * NB: *not* called by jpeg_abort or jpeg_destroy; surrounding
 * application must deal with any cleanup that should happen even
 * for error exit.
 */
static void term_destination (j_compress_ptr cinfo)
{
   //
   // just account for data used
   //
   memDest_t  *const dest = (memDestPtr_t) cinfo->dest ;
   memChunk_t  *tail = dest->chunkTail_ ;
   assert( 0 != tail );   
   assert( sizeof( tail->data_ ) >= dest->pub.free_in_buffer );
   assert( tail->data_ <= dest->pub.next_output_byte );
   assert( tail->data_ + sizeof( tail->data_ ) >= dest->pub.next_output_byte + dest->pub.free_in_buffer );

   tail->length_ = sizeof( tail->data_ ) - dest->pub.free_in_buffer ;
}

/*
 * Prepare for output to a chunked memory stream.
 */
void jpeg_mem_dest( j_compress_ptr cinfo )
{
   assert( 0 == cinfo->dest );
   cinfo->dest = (struct jpeg_destination_mgr *)
                 (*cinfo->mem->alloc_small)
                     ( (j_common_ptr) cinfo, 
                       JPOOL_IMAGE,
		       sizeof(memDest_t)
                     );
   memDest_t *dest = (memDest_t *) cinfo->dest ;
   dest->pub.init_destination    = init_destination ;
   dest->pub.empty_output_buffer = empty_output_buffer ;
   dest->pub.term_destination    = term_destination ;
   dest->chunkHead_ = 
   dest->chunkTail_ = 0 ;
}

//
// used differently than pngData_t.
//
// libjpeg keeps track of the ptr and length, but may want to rewind,
// so this structure is kept constant and the internals of jpeg_source_mgr
// are changed to point somewhere within.
// 
//
typedef struct {
   JOCTET const *data_ ;
   size_t        length_ ;
   size_t        numRead_ ;
} jpegSrc_t ;

static void jpg_init_source( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   pSrc->numRead_ = 0 ;
//printf( "init_source\n" );
}

static boolean jpg_fill_input_buffer( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   cinfo->src->bytes_in_buffer = pSrc->length_ - pSrc->numRead_ ;
   cinfo->src->next_input_byte = pSrc->data_ + pSrc->numRead_ ;
   pSrc->numRead_ = cinfo->src->bytes_in_buffer ;
//printf( "fill input\n" );
   return ( 0 < cinfo->src->bytes_in_buffer );
}

static void jpg_skip_input_data( j_decompress_ptr cinfo, long num_bytes )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   unsigned left = pSrc->length_ - pSrc->numRead_ ;
   if( left > num_bytes )
      num_bytes = left ;
   pSrc->numRead_ += num_bytes ;
//printf( "skip input : %u/%ld\n", pSrc->numRead_, num_bytes );
}

static boolean jpg_resync_to_restart( j_decompress_ptr cinfo, int desired )
{
//printf( "resync to restart\n" );
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   pSrc->numRead_ = 0 ;
   return true ;
}

static void jpg_term_source( j_decompress_ptr cinfo )
{
   jpegSrc_t *pSrc = (jpegSrc_t *)cinfo->client_data ;
   // nothing to do
}

bool imageJPEG( void const    *inData,     // input
                unsigned long  inSize,     // input
                void const    *&pixData,   // output
                unsigned short &width,     // output
                unsigned short &height )   // output
{
   pixData = 0 ; width = height = 0 ;
   
   JOCTET const *jpegData = (JOCTET *)inData ;
   unsigned const jpegLength = inSize ;
   
   struct jpeg_decompress_struct cinfo;
   struct jpeg_error_mgr jerr;
   cinfo.err = jpeg_std_error(&jerr);
   jpeg_create_decompress(&cinfo);
   
   jpegSrc_t jpgSrc ;
   jpgSrc.data_   = jpegData ;
   jpgSrc.length_ = jpegLength ;
   jpgSrc.numRead_ = 0 ;

   cinfo.client_data = &jpgSrc ;

   jpeg_source_mgr srcMgr ;
   memset( &srcMgr, 0, sizeof( srcMgr ) );
   srcMgr.next_input_byte  = jpgSrc.data_ ; /* => next byte to read from buffer */
   srcMgr.bytes_in_buffer  = jpgSrc.length_ ;	/* # of bytes remaining in buffer */
   
   srcMgr.init_source         = jpg_init_source ;
   srcMgr.fill_input_buffer   = jpg_fill_input_buffer ;
   srcMgr.skip_input_data     = jpg_skip_input_data ;
   srcMgr.resync_to_restart   = jpg_resync_to_restart ;
   srcMgr.term_source         = jpg_term_source ;
   cinfo.src = &srcMgr ;

   jpeg_read_header(&cinfo, TRUE);
   cinfo.out_color_space = JCS_RGB ;
   cinfo.dct_method = JDCT_IFAST;   
   
   //
   // per documentation, sample array should be allocated before
   // start_decompress() call. In order to get the image dimensions,
   // we need to call calc_output_dimensions().
   //
   jpeg_calc_output_dimensions(&cinfo);
   
   int row_stride = cinfo.output_width * cinfo.output_components;

   JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo,
                                                   JPOOL_IMAGE,
                                                   row_stride, 1);
   jpeg_start_decompress(&cinfo);

   unsigned char * const pixMap = new unsigned char[ cinfo.output_height*cinfo.output_width*3 ];

   // read the scanlines
   for( unsigned row = 0 ; row < cinfo.output_height ; row++ )
   {
      unsigned numRead = jpeg_read_scanlines( &cinfo, buffer, 1);
      unsigned char const *nextOut = buffer[0];

      for( unsigned column = 0; column < cinfo.output_width; ++column ) 
      {
         unsigned char red, green, blue ;
         if( cinfo.output_components == 1 ) 
         {   
            red = green = blue = *nextOut++ ;
         }
         else
         {
            red   = *nextOut++ ;
            green = *nextOut++ ;
            blue  = *nextOut++ ;
         }
         unsigned outIndex = 3*(row*cinfo.output_width+column);
         pixMap[outIndex++] = red ;
         pixMap[outIndex++] = green ;
         pixMap[outIndex++] = blue ;
      }
   }

   pixData = pixMap ;
   width   = cinfo.output_width ;
   height  = cinfo.output_height ;

   jpeg_finish_decompress(&cinfo);

   jpeg_destroy_decompress(&cinfo);

   return true ;
}

#include "memFile.h"

struct bmpHeader_t {
   unsigned char startJunk_[0x12];
   unsigned long width_ ;
   unsigned long height_ ;
   unsigned char endJunk_[0x36-0x12-8];
   unsigned char rgbData_[1];
} __attribute__ ((packed));


int main( int argc, char const * const argv[] )
{
   if( 2 <= argc )
   {
      bool firstError = true ;

      time_t const st = time(0);
      for( int arg = 1 ; arg < argc ; arg++ )
      {
         memFile_t fIn( argv[arg] );
         if( fIn.worked() )
         {
            fprintf( stderr, "read file '%s'\n", argv[arg] );
            void const *imgData = 0 ;
            unsigned short width, height ;
            if( imageJPEG( fIn.getData(), fIn.getLength(),
                           imgData, width, height ) )
            {
/*
bmpHeader_t const &header = *( bmpHeader_t const *)fIn.getData();
fprintf( stderr, "%u x %u pixels\n", header.width_, header.height_ );
hexDumper_t dumpHeader( &header, sizeof( header ) );
while( dumpHeader.nextLine() )
   fprintf( stderr, "%s\n", dumpHeader.getLine() );
width = header.width_ ;
height = header.height_ ;
unsigned const numImgBytes = width*height*3 ;
imgData = new unsigned char [numImgBytes];
memcpy( (void *)imgData, header.rgbData_, numImgBytes );
*/
               fprintf( stderr, "   %u x %u pixels\n", width, height );
               unsigned char *imgBytes = (unsigned char *)imgData ;
               palette_t palette ;
      
               for( unsigned row = 0 ; row < height ; row++ )
               {
                  for( unsigned col = 0 ; col < width ; col++ )
                  {
                     palette.nextIn( imgBytes[0], imgBytes[1], imgBytes[2] );
                     imgBytes += 3 ;
                  }
               }
               palette.buildPalette();
               printf( "   %u palette entries\n", palette.numPaletteEntries() );

               unsigned char * const palettized = new unsigned char [ height * width ];
               unsigned char *nextPal = palettized ;

               imgBytes = (unsigned char *)imgData ;
               for( unsigned row = 0 ; row < height ; row++ )
               {
                  for( unsigned col = 0 ; col < width ; col++ )
                  {
                     unsigned char index ;
                     if( palette.getIndex( imgBytes[0], imgBytes[1], imgBytes[2], index ) )
                     {
                        *nextPal++ = index ;
                        imgBytes += 3 ;
                     }
                     else
                     {
                        fprintf( stderr, "Error getting palette index for imgBytes %02x/%02x/%02x\n", imgBytes[0], imgBytes[1], imgBytes[2] );
                        return 0 ;
                     }
                  }
               }
               printf( "   converted all pixels\n" );

               nextPal = palettized ;
               imgBytes = (unsigned char *)imgData ;
               
               for( unsigned row = 0 ; row < height ; row++ )
               {
                  for( unsigned col = 0 ; col < width ; col++ )
                  {
                     unsigned char r, g, b ;
                     palette.getRGB( *nextPal++, r, g, b );
                     imgBytes[0] = r ;
                     imgBytes[1] = g ;
                     imgBytes[2] = b ;
                     imgBytes += 3 ;
                  }
               }
               printf( "   converted back\n" );

               struct jpeg_compress_struct cinfo;
               struct jpeg_error_mgr jerr;
               cinfo.err = jpeg_std_error(&jerr);
               jpeg_create_compress( &cinfo );
               cinfo.in_color_space = JCS_RGB; /* arbitrary guess */
               jpeg_set_defaults(&cinfo);
               cinfo.dct_method = JDCT_IFAST;
               cinfo.in_color_space = JCS_RGB;
               cinfo.input_components = 3;
               cinfo.data_precision = 8;
               cinfo.image_width = (JDIMENSION) width;
               cinfo.image_height = (JDIMENSION) height;
               jpeg_default_colorspace(&cinfo);
               jpeg_mem_dest( &cinfo );
               jpeg_start_compress( &cinfo, TRUE );
   
               unsigned const row_stride = 3*sizeof(JSAMPLE)*width; // RGB
               JSAMPARRAY const buffer = (*cinfo.mem->alloc_sarray)( (j_common_ptr)&cinfo,
                                                                     JPOOL_IMAGE,
                                                                     row_stride, 1 );
               //
               // loop through scan lines here
               //
               imgBytes = (unsigned char *)imgData ;
               for( unsigned row = 0 ; row < height ; row++ )
               {
                  jpeg_write_scanlines( &cinfo, &imgBytes, 1 );
                  imgBytes += 3*width ;
               } // for each row

               fprintf( stderr, "   finished compressing\n" );
               jpeg_finish_compress( &cinfo );

               char outFileName[256];
               sprintf( outFileName, "%u.jpg", arg );
               FILE *fOut = fopen( outFileName, "wb" );
               if( !fOut )
               {
                  perror( outFileName );
                  return 0 ;
               }
               memChunk_t const *const firstChunk = ( (memDest_t *)cinfo.dest )->chunkHead_ ;
               memChunk_t const *ch = firstChunk ;
               for( ch = firstChunk ; 0 != ch ;  )
               {
                  fwrite( ch->data_, 1, ch->length_, fOut );
                  memChunk_t const *temp = ch ;
                  ch = ch->next_ ;
                  delete temp ;
               }
               
               fprintf( stderr, "   created %s\n", outFileName );
               fclose( fOut );

               delete [] palettized ;
            }
            else
               fprintf( stderr, "Error parsing JPEG image %s\n", argv[arg] );

/*
*/               
            if( 0 != imgData )
               delete [] (unsigned char *)imgData ;
         }
         else
            fprintf( stderr, "Error <%s> reading %s\n", fIn.getError(), argv[arg] );
      }
      time_t const end = time(0);
      printf( "%u files in %u seconds\n", argc-1, end-st );
   }
   else
      fprintf( stderr, "Usage: %s inputFile\n", argv[0] );

   return 0 ;
}

#if 0
         FILE *fIn = fopen( argv[arg], "rb" );
         if( fIn )
         {
            fprintf( stderr, "reading from file '%s'\n", argv[arg] );
            palette_t palette ;
   
            unsigned char imgBytes[3];
            unsigned numRGB = 0 ;
            while( sizeof( imgBytes ) == fread( imgBytes, 1, sizeof( imgBytes ), fIn ) )
            {
/*
               if( 0 == ( numRGB & 7 ) )
                  fprintf( stderr, "%4u ->", numRGB );
               fprintf( stderr, "   %02x/%02x/%02x", imgBytes[0], imgBytes[1], imgBytes[2] );
*/
               palette.nextIn( imgBytes[0], imgBytes[1], imgBytes[2] );
/*
               if( 7 == ( numRGB & 7 ) )
                  fprintf( stderr, "\n", numRGB );
*/                  
               ++numRGB ;
            }
            fprintf( stderr, "\n" );

            palette.buildPalette();

//            palette.dump();
//            palette.verifyPixels("after building palette");

            fprintf( stderr, "%u pixels, %u palette entries, depth %u, %u pixels\n", numRGB, palette.numPaletteEntries(), palette.maxDepth(), palette.pixelsIn() );
            
            // dump palette
            unsigned char const numEntries = palette.numPaletteEntries();
            for( unsigned char i = 0 ; i < numEntries ; i++ )
            {
/*
               if( 0 == ( i & 7 ) )
                  fprintf( stderr, "%3u", i );
*/                  
               unsigned char r, g, b ;
               palette.getRGB( i, r, g, b );
/*
               fprintf( stderr, "   %02x.%02x.%02x", r, g, b );
               if( 7 == ( i & 7 ) )
                  fprintf( stderr, "\n" );
*/                  
            }
//            fprintf( stderr, "\n" );

            fseek( fIn, 0, SEEK_SET );
            unsigned numRGB2 = 0 ;
            unsigned char matchMask = (unsigned char)( ((signed char)'\x80' ) >> ( palette.maxDepth()-1 ));
            while( sizeof( imgBytes ) == fread( imgBytes, 1, sizeof( imgBytes ), fIn ) )
            {
               ++numRGB2 ;
if( 880 == numRGB2 )
{
   traceR_ = imgBytes[0];
   traceG_ = imgBytes[1];
   traceB_ = imgBytes[2];
}
               unsigned char index ;
               if( palette.getIndex( imgBytes[0], imgBytes[1], imgBytes[2], index ) )
               {
                  unsigned char r, g, b ;
                  palette.getRGB( index, r, g, b );
/*
                  if( 200 > numRGB2 )
                  {
                     fprintf( stderr, "   %02x.%02x.%02x ==> %02x.%02x.%02x\n", imgBytes[0], imgBytes[1], imgBytes[2], r, g, b );
                  }
                  if( (imgBytes[0] & matchMask) != ( r & matchMask ) )
                     fprintf( stderr, "mismatch r: %02x/%02x\n", imgBytes[0], r );
                  if( (imgBytes[1] & matchMask) != ( g & matchMask ) )
                     fprintf( stderr, "mismatch g: %02x/%02x\n", imgBytes[1], g );
                  if( (imgBytes[2] & matchMask) != ( b & matchMask ) )
                     fprintf( stderr, "mismatch b: %02x/%02x\n", imgBytes[2], b );
*/                     
               }
               else
               {
                  fprintf( stderr, "Error finding index for element %u [%02x/%02x/%02x]\n", numRGB2, imgBytes[0], imgBytes[1], imgBytes[2] );
                  palette.dump();
                  palette.verifyPixels( "after imgBytes mismatch" );
if( firstError )
{
   traceR_ = imgBytes[0];
   traceG_ = imgBytes[1];
   traceB_ = imgBytes[2];
   arg-- ;
   firstError = false ;
}
                  break;
               }
            }
fprintf( stderr, "matchMask : %02x\n", matchMask );

            if( numRGB != numRGB2 )
               fprintf( stderr, "Read mismatch: %u/%u\n", numRGB, numRGB2 );

            fclose( fIn );
         }
         else
            perror( argv[arg] );
#endif

#endif
