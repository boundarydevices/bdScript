#ifndef __ENTRYFLASH_H__
#define __ENTRYFLASH_H__ "$Id$"

/*
 * entryFlash.h
 *
 * This header file declares a class and a utility routine
 * for use in storing and reading entry counts from an MTD 
 * device. Data is stored in sequential sectors circularly, 
 * with a sequence number placed in the first four bytes, 
 * followed by text lines of one of these two forms:
 *
 *      normal entry:       time_t,pulsetime,debouncetime\tnumentries
 *      clear               time_t,text\tnumentries
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include <time.h>
#include <linux/types.h>
#include <mtd/mtd-user.h>

struct tagAndCount_t {
    time_t          when_ ;
    char            tag_[80];
    unsigned long   pulseTime_ ;
    unsigned long   debounceTime_ ;
    unsigned        numEntries_ ;
};

extern bool validate_flash_entry_line (void *opaque, char const *line);

typedef bool (*validate_line_t)(void *opaque, char const *line);

class mtdCircular_t {
public:
    mtdCircular_t( char const     *devName, 
                   void           *opaque,
                   validate_line_t validator );
    ~mtdCircular_t( void );

    bool isOpen(void) const { return 0 <= fd_ ; }

    void write(char const *data, unsigned len );

private:
    void close(void);
    void advance(void);
    void getLastLine( int sector, 
                      int  prevsect, 
                      void *opaque,
                      validate_line_t validator);

    int             fd_ ;
    mtd_info_t      meminfo_ ;
    unsigned        curSector_ ;
    unsigned        curSize_ ;
    int             curSequence_ ;
    char            lastLine_[256];
};

#endif

