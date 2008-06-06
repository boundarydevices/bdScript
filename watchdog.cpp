/*
 * Program watchdog.cpp
 *
 * This program defines a watchdog program that
 * expects to read a control file of the form:
 *
 *	command line (program to start & monitor)
 *	port number (decimal or 0xHEX)
 *	[1 or more] of
 *	name timeout(ms)
 *
 * Change History : 
 *
 * $Log: watchdog.cpp,v $
 * Revision 1.1  2008-06-06 21:48:10  ericn
 * -import
 *
 *
 * Copyright Boundary Devices, Inc. 2008
 */

#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

inline long long timeValToMs( struct timeval const &tv )
{
   return ((long long)tv.tv_sec*1000)+((long long)tv.tv_usec / 1000 );
}

inline long long tickMs()
{
   struct timeval now ; gettimeofday( &now, 0 );
   return timeValToMs( now );
}

typedef struct _watchpoint_t {
	char const     *name ;
	unsigned    	timeout ;
	long long   	lastAlive ;
	unsigned long   minInterval ;
	unsigned long   maxInterval ;
	unsigned long   count ;
	struct _watchpoint_t *next ;
} watchPoint_t ;

// a.la. fgets, but skipping blank lines and comments
static char *getLine( char *inBuf, unsigned inSize, FILE *fIn ){
        char *out ;
	do {
		out = fgets( inBuf, inSize, fIn );
		if( !out )
			break;
		else if( ( '#' == *out ) || ( '\n' == *out ) )
			continue;
		else {
			out[strlen(out)-1] = '\0' ; // strip newline
			break;
		}
	} while( 1 );

	return out ;
}

static void parse( char *buf, char **args )
{
    while( *buf ){
        /*
         * Strip whitespace.  Use nulls, so
         * that the previous argument is terminated
         * automatically.
         */
        while ((*buf == ' ') || (*buf == '\t'))
            *buf++ = '\0' ;

        /*
         * Save the argument.
         */
        *args++ = buf;

        /*
         * Skip over the argument.
         */
        while (*buf && (*buf != ' ') && (*buf != '\t'))
            buf++;
    }

    *buf = 0 ; // terminating null
    *args = NULL;
}

static void showArgs( char **args )
{
	while( *args ){
		printf( "--> %s\n", *args++ );
	}
}

static int volatile childPid_ = -1 ;

static void childHandler
   ( int             sig, 
     struct siginfo *info, 
     void           *context )
{
   if( SIGCHLD == sig )
   {
	int exitCode ;
        struct rusage usage ;
	int pid ;
	while( 0 < ( pid = wait3( &exitCode, WNOHANG, &usage ) ) ){
		if( pid == childPid_ ){
			printf( "%lld:child %d died\n", tickMs(), pid );
			childPid_ = -1 ;
		}
	}
   }
}

static void startChild( char **args, watchPoint_t *wp )
{
	pid_t pid = fork();
	if( 0 == pid ){
		// child
		printf( "child process %d\n", getpid() );
		execvp( args[0], args );
		exit(-1);
	}
	else {
		printf( "parent of %d\n", pid );
		childPid_ = pid ;
	}
	
	sleep( 2 );
	int rval = kill(pid,SIGCONT);
	printf( "SIGCONT: %d\n", rval );
	if( 0 == rval ){
		long long now = tickMs();
		for( ; wp ; wp = wp->next ){
			wp->lastAlive = now ;
			wp->minInterval = 0xFFFFFFFF ;
			wp->maxInterval = 0 ;
		}
	}
}

static unsigned numWP = 0 ;
static watchPoint_t *firstWP = 0 ;
static watchPoint_t **watchpoints = 0 ;

static void dumpStats( void ){
}


int main( int argc, char const * const argv[] )
{
	if( 2 != argc ){
		fprintf( stderr, "Usage: %s /path/to/controlFile\n", argv[0] );
		return -1 ;
	}
	FILE *fIn = fopen( argv[1], "rt" );
	if( !fIn ){
		perror( argv[1] );
		return -1 ;
	}
	char cmdLine[512];
	if( !getLine( cmdLine, sizeof(cmdLine)-1, fIn ) ){
		perror( "cmdLine" );
		return -1 ;
	}
	while( getLine( inBuf,sizeof(inBuf)-1, fIn ) ){
		char name[80];
		unsigned timeout ;
		int numRead ;
		if( 2 == (numRead = sscanf( inBuf, "%s %u", &name, &timeout ) )){
			watchPoint_t *newOne = new watchPoint_t ;
			memset( newOne, 0, sizeof(*newOne) );
			newOne->name = strdup( name );
			newOne->timeout = timeout ;
			newOne->next = firstWP ;
			firstWP = newOne ;
                        ++numWP ;
		}
	} // until EOF

	if( 0 == numWP ){
		fprintf( stderr, "no watchpoints defined\n" );
		return -1 ;
	}
	fclose( fIn );

	int const sockFd = socket( AF_INET, SOCK_DGRAM, 0 );
	if( 0 > sockFd ){
		perror( "socket" );
		return -1 ;
	}

        int const doit = 1;
	int rval = setsockopt(sockFd, SOL_SOCKET, SO_REUSEADDR, (void *)&doit, sizeof(doit));
	if( 0 != rval ){
		perror( "setsockopt" );
		return -1 ;
	}

	int flags = flags = fcntl( sockFd, F_GETFL, 0 );
	rval = fcntl( sockFd, F_SETFL, flags | O_NONBLOCK );
	if( 0 != rval ){
		perror( "O_NONBLOCK" );
		return -1 ;
	}

	watchpoints = new watchPoint_t *[numWP];
	printf( "command line: %s\n", cmdLine );
	printf( "port %u (0x%x)\n", port, port );
	unsigned idx = 0 ;
       	for( watchPoint_t *next = firstWP ; next ; next = next->next ){
		unsigned wdNum = numWP-1-idx ;
		idx++ ;
		printf( "%u:%s: %u\n", wdNum, next->name, next->timeout );
		watchpoints[wdNum] = next ;
	}

	char *args[80];
	parse( cmdLine, args );
	showArgs( args );

	struct sigaction sa;

	/* Initialize the sa structure */
	sa.sa_handler = (__sighandler_t)childHandler ;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO ; // pass info
	
	//   int result = 
	sigaction( SIGCHLD, &sa, 0 );

	do {
		// start program
		startChild( args, firstWP );

		// now monitor program
		pollfd rdPoll ;
		rdPoll.fd = sockFd ;
		while( 0 < childPid_ ){
			rdPoll.events = POLLIN | POLLERR ;
			int const numEvents = poll( &rdPoll, 1, 1000 );
			long long const now = tickMs();
			if( 1 == numEvents )
			{
				sockaddr_in remote ;
				socklen_t   remSize = sizeof(remote);
				unsigned char in ;
				int numRead ;
				do {
					numRead = recvfrom( sockFd, &in, 1, 0, (struct sockaddr *)&remote, &remSize );
					if( 1 == numRead ){
						if( in < numWP ){
                                                        watchPoint_t &wp = *watchpoints[in];
							unsigned long elapsed = now-wp.lastAlive ;
							wp.lastAlive = now ;
							wp.count++ ;
							if( elapsed < wp.minInterval )
                                                                wp.minInterval = elapsed ;
							if( elapsed > wp.maxInterval )
								wp.maxInterval = elapsed ;
						}
						else
							fprintf( stderr, "Invalid watchpoint %u, max %u\n", in, numRead );
					}
				} while( 1 == numRead );
			}
			bool killit = false ;
			for( unsigned i = 0 ; i < numWP ; i++ ){
				watchPoint_t &wp = *watchpoints[i];
				unsigned long elapsed = now-wp.lastAlive ;
				if( elapsed > wp.timeout ){
					printf( "event %u expired: %u elapsed, max %u\n", i, elapsed, wp.timeout );
					killit = true ;
				}
			}
			if( killit ){
				pid_t pid = childPid_ ;
				printf( "killing child process %d\n", pid );
				if( 0 < pid ){
					kill( pid, SIGKILL );
				}
				while( 0 > childPid_ ){
					printf( "waiting for child\n" );
					sleep(1);
				}
			}
		} // while child is stilll alive
		printf( "%lld:child died. restarting\n", tickMs() );
	} while( 1 );

	return 0 ;
}
