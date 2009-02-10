#include <stdio.h>
#include <stdlib.h>
#include "inputDevs.h"
#include "pxaCursorTest.h"

pxaCursorTest_t::pxaCursorTest_t(pollHandlerSet_t &set,
				char const       *devName,
				unsigned          mode,
				char const       *cursorImg)
				: inputMouse_t(set,devName),
					pcursor(mode),
					minx(0),
					miny(0),
					curx(0),
					cury(0)
{
	image_t image;
	fbDevice_t &fb = getFB();

	maxx = fb.getWidth() - 1;
	maxy = fb.getHeight() - 1;
	imageFromFile(cursorImg, image);
	pcursor.setImage(image);

	pcursor.getPos(curx, cury);

	onMove();
}

void pxaCursorTest_t::enforceBounds()
{
	if(curx < minx)
		curx = minx;
	else if(curx > maxx)
		curx = maxx;

	if(cury < miny)
		cury = miny;
	else if(cury > maxy)
		cury = maxy;
}

void pxaCursorTest_t::onMove( void )
{
	enforceBounds();
	pcursor.setPos(curx, cury);
}

void pxaCursorTest_t::onData( struct input_event const &event )
{
	switch(event.type) {
		case EV_REL:
			if(REL_X == event.code)
				curx += event.value;
			else if(REL_Y == event.code)
				cury += event.value;
			else
				printf( "rel: %x %d\n", event.code, event.value );
			break;
		case EV_SYN:
			onMove();
		case EV_KEY:
			break;
		default:
			printf("type %d, code 0x%x, value %d\n",
				event.type, 
				event.code,
				event.value);
			break;
	}
}
int main(int argc, char const * const argv[])
{
	inputDevs_t inputDevs ;

	for( unsigned i = 0 ; i < inputDevs.count(); i++ ){
		unsigned evIdx ;
		inputDevs_t::type_e type ;
		inputDevs.getInfo(i,type,evIdx);

		if(inputDevs_t::MOUSE == type){
			char devName[512];
			pollHandlerSet_t handlers ;
			snprintf(devName, sizeof(devName),
				"/dev/input/event%u", evIdx);

			if(2 > argc)
			{
				printf("usage: pxaCursorTest [cursor img file]\n");
				exit(-1);
			}

			pxaCursorTest_t mouse(handlers, devName,
					DEFAULT_CURSOR_MODE, argv[1]);

			while(true)
				handlers.poll(-1);

			break;
		}
	}
}
