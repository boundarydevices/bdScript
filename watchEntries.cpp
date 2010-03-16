#include <fcntl.h>
#include <time.h>
#include <sys/mount.h>
#include <linux/types.h>
#include <syscall.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "tickMs.h"
#include <ctype.h>
#include "entryFlash.h"

// #define DEBUGPRINT
#include "debugPrint.h"

class ioBuffer_t {
public:
    ioBuffer_t( void );
    ~ioBuffer_t( void );

    bool wait( void );
    void post( char const *inData, unsigned len );

    bool read( bool &high );

    sem_t sem_ ;
    char inData_[1024];
    unsigned add_ ;
    unsigned take_ ;
};

ioBuffer_t::ioBuffer_t( void )
    : add_(0)
    , take_(0)
{
    sem_init(&sem_,0,1);
}

ioBuffer_t::~ioBuffer_t( void )
{
    sem_post(&sem_);
}

void ioBuffer_t::post( char const *inData, unsigned len )
{
    while( 0 < len ){
        unsigned space = sizeof(inData_)-add_ ;
        if( space > len )
            space = len ;
        memcpy(inData_+add_,inData,space);
        add_ = (add_ + space)%sizeof(inData_);
        len -= space ;
    }
    sem_post(&sem_);
}

bool ioBuffer_t::read( bool &high )
{
    if( add_ != take_ ){
        high = (0 != (inData_[take_]&1));
        take_ = (take_+1)%sizeof(inData_);
        return true ;
    }
    else
        return false ;
}

int const billion = 1000000000 ;
int const interval = billion/10 ;      // 1/10th of a second

bool ioBuffer_t::wait(void) {
    if( add_ == take_ ){
        struct timespec ts;
        if(0 <= clock_gettime(CLOCK_REALTIME, &ts)){
            ts.tv_nsec += interval ;
            if( ts.tv_nsec > billion ){
                ts.tv_sec++ ;
                ts.tv_nsec -= billion ;
            }
            sem_timedwait(&sem_,&ts);
        }
    }

    return add_ != take_ ;
}

struct threadParams_t {
    mtdCircular_t *mtd_ ;
    ioBuffer_t     buf_ ;
    unsigned long  timeout_ms_ ;
    unsigned long  numEntries_ ;
    bool           die_ ;
};

static void *timerRtn( void *arg )
{
    long long prevTick = 0LL ;
    long long startTick = 0LL ;
    unsigned transitions = 0 ;
    bool prevHigh = false ;
    struct threadParams_t *tp = (struct threadParams_t *)arg ;
    while( !tp->die_ ){
        if( tp->buf_.wait() ){
            bool high ;
            while( tp->buf_.read(high) ){
                if(high != prevHigh){
                    if( 1 == ++transitions )
                        startTick = tickMs();
#ifdef DEBUGPRINTx
                    fprintf(stderr, "%u", high );
                    fflush(stderr);
#endif
                    prevHigh = high ;
                    if( !prevHigh ){
                        prevTick = tickMs();
                    }
                }
            }
        } else if( (0 < transitions) && !prevHigh ){
            long long const nowTick = tickMs();
            unsigned long elapsed = (unsigned long)(nowTick-prevTick);
            if( tp->timeout_ms_ <= elapsed ){
                time_t now ;
                now = time(&now);
#ifdef DEBUGPRINTx
                struct tm tm ;
                localtime_r(&now, &tm);
                char time_buf[80];
                unsigned time_len = strftime(time_buf,sizeof(time_buf),"%Y-%m-%d %H:%M:%S", &tm);
                debugPrint("entry:%s\n", time_buf );
                elapsed = prevTick-startTick ;
                fprintf(stderr, "elapsed: %lu\n", elapsed );
                elapsed = nowTick-prevTick ;
                fprintf(stderr, "elapsed2: %lu\n", elapsed );
#endif
                ++tp->numEntries_ ;
                char flashBuf[256];
                unsigned len = snprintf(flashBuf,sizeof(flashBuf),"%lx,%lu,%lu\t%lu\n"
                                        ,now
                                        , (unsigned long)(prevTick-startTick)
                                        , (unsigned long)(nowTick-prevTick)
                                        , tp->numEntries_ );
                transitions = 0 ;
//                fwrite(flashBuf,1,len,stderr);
                tp->mtd_->write(flashBuf,len);
            }
        }
    }
    return 0 ;
}

int main( int argc, char const * const argv[] ){
    if( 3 <= argc ){
        char const * const gpName = argv[1];
        char const * const mtdName = argv[2];
        unsigned long timeout_ms = (3 < argc) ? strtoul(argv[3],0,0) : 200 ;
        struct tagAndCount_t tc = {0};
        mtdCircular_t mtdDev(mtdName,&tc,validate_flash_entry_line);
        if( mtdDev.isOpen() ){
            struct tm tm ;
            localtime_r(&tc.when_, &tm);
            char time_buf[80];
            strftime(time_buf,sizeof(time_buf),"%Y-%m-%d %H:%M:%S", &tm);
            fprintf( stderr, "%s: %s, count %u\n", time_buf, tc.tag_, tc.numEntries_ );
            int const fdGP = open(gpName, O_RDONLY);
            if( 0 <= fdGP ){
                pthread_t timerThread = 0 ;
                struct threadParams_t tp ;
                tp.mtd_ = &mtdDev ;
                tp.timeout_ms_ = timeout_ms ;
                tp.numEntries_ = tc.numEntries_ ;
                if( 0 == pthread_create(&timerThread, 0, timerRtn, &tp) ){
                    while(1){
                        char inBuf[256];
                        int numRead ;
                        while( 0 < (numRead = read(fdGP, inBuf, sizeof(inBuf)) )){
                            inBuf[numRead] = '\0' ;
                            fwrite(inBuf,1,numRead,stdout);
                            fflush(stdout);
                            tp.buf_.post(inBuf,numRead);
                        }
                    }
                    tp.die_ = true ;
                    tp.buf_.post("",0);
                    void *exitStat ;
                    pthread_join( timerThread, &exitStat );
                }
                else 
                    perror( "threadcreate" );

                close(fdGP);
            } else
                perror(gpName);
        } else
            perror(mtdName);
    }
    else
        fprintf( stderr, "Usage: %s /dev/gpix /dev/mtdx timeout_ms=200\n", argv[0]);
    return 0 ;
}
