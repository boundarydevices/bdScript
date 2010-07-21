/*
 * Module entryFlash.cpp
 *
 * This module defines the methods of the mtdCircular_t class and the
 * validate_flash_entry_line() routine as declared in entryFlash.h
 *
 * Change History : 
 *
 * $Log$
 *
 * Copyright Boundary Devices, Inc. 2010
 */


#include "entryFlash.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "debugPrint.h"

extern bool validate_flash_entry_line (void *opaque, char const *line)
{
    char const *sep = strchr(line,'\t');
    if( opaque && sep && isdigit(sep[1]) ){
        struct tagAndCount_t *tc = (struct tagAndCount_t *)opaque ;
        tc->numEntries_ = strtoul(sep+1,0,0);
        char const *endOfTime = strchr(line,',');
        if(endOfTime && (endOfTime<(sep-1)) && (endOfTime <= line+8)){
            int len = endOfTime-line ;
            char cTime[10];
            memcpy(cTime,line,len);
            cTime[len] = '\0' ;
            unsigned when ;
            unsigned numParsed = sscanf(cTime,"%x", &when);
            if( 1 == numParsed ){
                tc->when_ = when ;
                endOfTime++ ;
                len = sep-endOfTime ;
                if( (3 <= len) && ((int)sizeof(tc->tag_) > len) ){
                    memcpy(tc->tag_,endOfTime,len);
                    if( isdigit(*endOfTime) ){
                        if( 2 == sscanf(tc->tag_,"%lu,%lu", &tc->pulseTime_, &tc->debounceTime_ ) ){
                            strcpy(tc->tag_, "entry" );
                            return true ;
                        } else
                            fprintf(stderr, "invalid pulse times in %s\n", line );
                    } else {
                        return true ; 
                    } 
                } else {
                    fprintf(stderr, "error parsing times or tag in %s\n", line );
                }
            }
            else
                fprintf(stderr, "unable to parse time_t field in %s\n", line);
        }
        else {
            fprintf(stderr, "missing time separator in <%s>\n", line );
            fprintf(stderr, "endOfTime %p, sep %p, line %p\n", endOfTime, sep, line );
        }
    }
    else
        fprintf(stderr, "Invalid opaque %p or missing tab in <%s>\n", opaque, line );
    return false ;
}


mtdCircular_t::mtdCircular_t( char const *devName, void *opaque, validate_line_t validator )
    : fd_( open(devName, O_RDWR) )
    , curSector_(0)
    , curSize_(0)
    , curSequence_(-1)
{
    lastLine_[0] = '\0' ;
    if( isOpen() ){
        if(ioctl(fd_, MEMGETINFO, (unsigned long)&meminfo_)){
            perror( devName );
            close();
            return ;
        }
        debugPrint( "flags       0x%lx\n", meminfo_.flags ); 
        debugPrint( "size        0x%lx\n", meminfo_.size ); 
        debugPrint( "erasesize   0x%lx\n", meminfo_.erasesize ); 
        //	    debugPrint( "oobblock    0x%lx\n", meminfo_.oobblock ); 
        debugPrint( "oobsize     0x%lx\n", meminfo_.oobsize ); 
        debugPrint( "ecctype     0x%lx\n", meminfo_.ecctype ); 
        debugPrint( "eccsize     0x%lx\n", meminfo_.eccsize ); 
        unsigned numSectors = meminfo_.size / meminfo_.erasesize ;
        debugPrint( "numSectors  0x%lx\n", numSectors );
        int prevSector = -1 ;
        curSequence_ = -1 ;
        for( unsigned i = 0 ; i < numSectors ; i++ ){
            unsigned offset = i*meminfo_.erasesize ;
            if( offset == (unsigned)lseek( fd_, offset, SEEK_SET ) ){
                int sequence ;
                if( sizeof(sequence) == read(fd_,&sequence,sizeof(sequence))){
                    debugPrint("sector[%u] == %d\n", i, sequence );
                    if( sequence >= curSequence_ ){
                        if( i != curSector_ ){
                            prevSector = curSector_ ;
                        }
			debugPrint( "best: %d->%d in sector %u\n",curSequence_,sequence, i);
                        curSector_ = i ;
                        curSequence_ = sequence ;
                    }
                }
                else {
                    perror("read(mtd)");
                    close();
                    return ;
                }
            }
            else {
                perror("lseek(mtd)");
                close();
                return ;
            }
        }
        debugPrint( "current sector == %u\n", curSector_ );
        curSize_ = 4 ;
        unsigned offset = curSector_*meminfo_.erasesize+curSize_ ;
        if( offset != (unsigned)lseek( fd_, offset, SEEK_SET ) ){
            perror( "seek(curSector)");
            close();
            return ;
        }

        signed char lastChar = -1 ;
        while( curSize_ < meminfo_.erasesize ){
            signed char c ;
            if( sizeof(c) == read(fd_,&c,sizeof(c))) {
// debugPrint("offset %ld: char %02x\n",lseek(fd_,0,SEEK_CUR), (unsigned char)c);
                if( 0 > c )
                    break;
                lastChar = c ;
                ++curSize_ ;
            } else {
                perror("readData(mtd)");
                close();
                return ;
            }
        }
        debugPrint("curSize: %u\n", curSize_ );

        if( curSize_ >= meminfo_.erasesize ){
            advance();
        } else if( '\n' != lastChar ){
            lseek(fd_,-1,SEEK_CUR);
            ::write(fd_,"\n",1);
        }

        if( (0 > curSequence_) && (0 == curSector_) && (sizeof(curSequence_) == curSize_) ){
            debugPrint("initializing device\n");
            // first time through
            curSequence_ = 0 ;
            if( 0 != (unsigned)lseek( fd_, 0, SEEK_SET ) ){
                perror( "seek(curSector)");
                close();
                return ;
            }
            if(sizeof(curSequence_) != ::write(fd_,&curSequence_,sizeof(curSequence_))) {
                perror( "write(initial sequence)\n");
                close();
                return ;
            }
        }
        getLastLine(curSector_,prevSector,opaque,validator);
    }
}

void mtdCircular_t::close(void)
{
    if( isOpen() ){
        ::close(fd_);
        fd_ = -1 ;
    }
}

mtdCircular_t::~mtdCircular_t( void )
{
    close();
}

void mtdCircular_t::advance(void){
    ++curSequence_ ;
    unsigned numSectors = meminfo_.size / meminfo_.erasesize ;
    curSector_ = (curSector_+1) % numSectors ;
    debugPrint("sector(%u)= sequence(%d)\n", curSector_, curSequence_ );

    unsigned offset = curSector_*meminfo_.erasesize ;
    erase_info_t erase;
    erase.start = offset ;
    erase.length = meminfo_.erasesize;
    if( 0 == ioctl( fd_, MEMERASE, (unsigned long)&erase) ){
        if( offset == (unsigned)lseek( fd_, offset, SEEK_SET ) ){
            if (sizeof(curSequence_) == ::write(fd_,&curSequence_,sizeof(curSequence_))) {
                curSize_ = sizeof(curSequence_);
            } else {
                perror("write(curSequence)");
            }
        } else
            perror( "seek after erase");
    } else {
        perror("MEMERASE");
    }
}

void mtdCircular_t::write(char const *data, unsigned len ){
    unsigned space = meminfo_.erasesize - curSize_ ;
    if( space < len )
        advance();
    unsigned offset = curSector_*meminfo_.erasesize + curSize_ ;
    if( offset == (unsigned)lseek( fd_, offset, SEEK_SET ) ){
        if( len == (unsigned)::write(fd_,data,len)) {
            curSize_ += len ;
        } else {
            perror("write(data)");
        }
    }
    else
        perror("lseek(write)");
}

static void strrev(char *line,int len)
{
    int i = 0 ;
    while(i < --len){
        char tmp  = line[i];
        line[i]   = line[len];
        line[len] = tmp ;
        i++ ;
    }
}

void mtdCircular_t::getLastLine
    ( int sector, 
      int  prevsect, 
      void *opaque,
      validate_line_t validator)
{
    debugPrint("%s:%d:%d\n", __func__,sector,prevsect );
    
    int sectors[] = {
        sector,
        prevsect,
        -1
    };
    unsigned i = 0 ;
    int cursect ;
    int curPos = curSize_ ;
    int lineLen = 0 ;
    while( 0 <= (cursect=sectors[i++]) ){
        debugPrint( "process sector %d, size %d(0x%x)\n", cursect, curPos, curPos );
        while( (int)sizeof(curSequence_) < curPos ){
            unsigned offset = cursect*meminfo_.erasesize + --curPos ;
            if( offset != (unsigned)lseek( fd_, offset, SEEK_SET ) ){
                perror("seek()");
                continue;
            }
            signed char c ;
            int numRead = read(fd_,&c,sizeof(c));
            if( 1 == numRead ){
                if( '\n' == c ){
                    if( lineLen ){
                        lastLine_[lineLen] = 0 ;
                        strrev(lastLine_,lineLen);
                        if( validator(opaque,lastLine_) ){
                            return ;
                        }
                    }
                    lineLen = 0 ;
                } else if( 0 <= c ){
                    lastLine_[lineLen++] = c ;
                    lineLen %= sizeof(lastLine_);
                } else 
                    lineLen = 0 ; // skip chars with high bit set
            }
            else
                perror("read()");
        }
        if( 0 < lineLen ){
            lastLine_[lineLen] = 0 ;
            strrev(lastLine_,lineLen);
            if( validator(opaque,lastLine_) )
                return ;
        }
        curPos = meminfo_.erasesize ;
    }
}


