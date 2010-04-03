#include <stdlib.h>
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/poll.h>
#include <fcntl.h>
#include "tickMs.h"

int main(int argc, char *argv[])
{
	if ( SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO) < 0 ) {
		fprintf(stderr, "Unable to init SDL: %s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);

	SDL_Surface *screen = SDL_SetVideoMode(800, 600, 16, SDL_HWSURFACE|SDL_DOUBLEBUF);
	if ( screen == NULL ) {
		fprintf(stderr, "Unable to set 640x480 video: %s\n", SDL_GetError());
		exit(1);
	}

	int const fd = socket( AF_INET, SOCK_DGRAM, 0 );
	sockaddr_in local ;
	local.sin_family      = AF_INET ;
	local.sin_addr.s_addr = INADDR_ANY ;
	local.sin_port        = 0xBDBD;
	bind( fd, (struct sockaddr *)&local, sizeof( local ) );
	int doit = 1 ;
	int result = setsockopt( fd, SOL_SOCKET, SO_BROADCAST, &doit, sizeof( doit ) );
	if ( 0 != result )
		perror( "SO_BROADCAST" );
	doit = 1 ;
	result = setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &doit, sizeof( doit ) );
	if ( 0 != result )
		fprintf( stderr, "SO_REUSEADDR:%d:%m\n", result );

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL,0 )|O_NONBLOCK);

	bool done = false ;

	long long start = tickMs();
	unsigned numFrames = 0 ;
	pollfd fds;
	fds.events = POLLIN ;
	fds.fd = fd ;
	while (!done) {
		SDL_Event event;
		if (SDL_PollEvent(&event)) {
			if ( (event.type==SDL_KEYDOWN) && (SDLK_ESCAPE == event.key.keysym.sym) ) {
				done = true ;
			}
		}

		if ( 1 == poll(&fds,1,100) ) {
			char inBuf[32768];
			int numRead ;
			sockaddr_in rem ;
			socklen_t remSize = sizeof(rem);
			if ( 0 < ( numRead = recvfrom( fd, inBuf, sizeof(inBuf), 0, (struct sockaddr *)&rem, &remSize ) ) ) {
				SDL_RWops *rw = SDL_RWFromMem(inBuf,numRead);
				//				printf( "rx %u bytes from %s, port 0x%x\n", numRead, inet_ntoa(rem.sin_addr), ntohs(rem.sin_port) );
				if (IMG_isJPG(rw)) {
					SDL_Surface *image = IMG_LoadJPG_RW(rw);
					if (image) {
						++numFrames ;
						int err = SDL_BlitSurface(image,0,screen,0);
						SDL_FreeSurface(image);
						SDL_UpdateRect(screen,0,0,0,0);
					}
					else {
						printf("IMG_LoadJPG_RW: %s\n", IMG_GetError());
					}
				}
				else {
					printf("not JPEG\n");
				}
				SDL_FreeRW(rw);
				// fwrite( inBuf, numRead, 1, stdout );
				//				printf( "\n" );
			}
		}
	}
	long long end = tickMs();
	unsigned elapsed = (unsigned)(end-start);
	printf("%u ms elapsed, %u frames loaded, (%u fps)\n", elapsed, numFrames,(numFrames*1000)/elapsed);

	return 0 ;
}

