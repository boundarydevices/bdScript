extern "C" {
#include <vpu_lib.h>
#include <vpu_io.h>
};

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>	      /* low-level i/o */
#include <assert.h>
#include <sys/time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <malloc.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <asm/types.h>	  /* for videodev2.h */

#include <linux/fb.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>

#include "bayerToYUV.h"
#include "imx_vpu.h"
#include "imx_mjpeg_encoder.h"
#include "camera.h"
#include "cameraParams.h"
#include "v4l_overlay.h"
#include "fourcc.h"

#define DEBUGPRINT
#include "debugPrint.h"

int main(int argc, const char **argv) {
	vpu_t vpu ;
	if(vpu.worked()) {
		cameraParams_t params(argc,argv);
		params.dump();
		int udpSocket = -1 ;
		sockaddr_in remote ;
		remote.sin_family = AF_INET ;
                remote.sin_addr.s_addr = 0 ;
		remote.sin_port = 0; 
		if(params.getBroadcastAddr() && params.getBroadcastPort()){
			udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
			if(0 <= udpSocket){
				remote.sin_addr.s_addr = params.getBroadcastAddr();
				remote.sin_port = htons(params.getBroadcastPort());
				int doit = 1 ;
                                int result = setsockopt(udpSocket, SOL_SOCKET, SO_BROADCAST, &doit, sizeof( doit ) );
                                if( 0 != result )
                                        perror( "SO_BROADCAST" );
			}
			else
				perror("socket");
		}
		debugPrint( "%ux%u: %u iterations\n", params.getCameraWidth(), params.getCameraHeight(), params.getIterations());
		v4l_overlay_t overlay(params.getCameraWidth(), params.getCameraHeight(),
				      params.getPreviewX(),params.getPreviewY(),
				      params.getPreviewWidth(),params.getPreviewHeight(),
				      params.getPreviewTransparency(), params.getCameraFourcc());
		if( overlay.isOpen() ){
			struct timeval tenc_begin,tenc_end, total_start, total_end;
			int sec, usec;
			float tenc_time = 0, total_time=0;
	
			camera_t camera(params.getCameraDeviceName(),
					params.getCameraWidth(), 
					params.getCameraHeight(),
					params.getCameraFPS(),
					params.getCameraFourcc(),
					params.getCameraRotation());
			if( !camera.isOpen() ) {
				fprintf(stderr, "Error opening %s\n", params.getCameraDeviceName() );
				return -1 ;
			}
			if( !camera.startCapture() ) {
				fprintf(stderr, "Error starting capture on %s\n", params.getCameraDeviceName() );
				return -1 ;
			}
			mjpeg_encoder_t encoder(vpu,params.getCameraWidth(), params.getCameraHeight(),params.getCameraFourcc(),camera.getFd(),camera.numBuffers(),camera.getBuffers());
			if( encoder.initialized() ){
				printf( "encoder: %ux%u (strides %ux%u) y%u, uv%u\n", encoder.width(), encoder.height(), encoder.yuvSize() );
				gettimeofday(&total_start, NULL);
		
				unsigned maxSize = 0 ;
	
				/* The main encoding loop */
				unsigned frame_id = 0;
				for( frame_id = 0 ; frame_id < (unsigned)params.getIterations() ; frame_id++ ){
					void const *cameraBuf ; 
					int camera_index ;
					while( !camera.grabFrame(cameraBuf,camera_index) )
						;
	
					if( params.getSaveFrameNumber() == frame_id ) {
						FILE *fOut = fopen("/tmp/imx_vpu_test.bayer", "wb");
						if(fOut){
							fwrite(cameraBuf,((params.getCameraWidth()+31)&~31)*params.getCameraHeight(),1,fOut);
							fclose(fOut);
						} else
							perror( "outFile");
					}
					unsigned ovIndex ;
					unsigned char *y, *u, *v ;
					if( overlay.getBuf(ovIndex,y,u,v) ){
					    memcpy(y,cameraBuf,overlay.imgSize());
					    overlay.putBuf(ovIndex);
					} else
					    printf( "output frame drop\n" );
	
					gettimeofday(&tenc_begin, NULL);
	
					void const *outData ;
					unsigned    outLength ;
					if( encoder.encode(camera_index, outData,outLength) ){
						if( maxSize < outLength )
							maxSize = outLength ;
						gettimeofday(&tenc_end, NULL);
						sec = tenc_end.tv_sec - tenc_begin.tv_sec;
						usec = tenc_end.tv_usec - tenc_begin.tv_usec;
	
						if (usec < 0) {
							sec--;
							usec = usec + 1000000;
						}
		
						tenc_time += (sec * 1000000) + usec;
	
						if( params.getSaveFrameNumber() == frame_id ) {
							printf("save frame\n");
							FILE *fOut = fopen("/tmp/imx_vpu_test.mjpeg", "wb");
							if(fOut){
								fwrite(outData,outLength,1,fOut);
								fclose(fOut);
							} else
								perror( "outFile");
						}
						if(0 <= udpSocket) {
							int err = sendto(udpSocket, outData, outLength, MSG_DONTROUTE|MSG_DONTWAIT,  (struct sockaddr *)&remote, sizeof( remote ) );
							if( 0 > err ){
								perror("sendto");
							}
						}
					}
					else {
						fprintf(stderr, "encode error\n" );
						break;
					}
					camera.returnFrame(cameraBuf,camera_index);
				}
		
				gettimeofday(&total_end, NULL);
				sec = total_end.tv_sec - total_start.tv_sec;
				usec = total_end.tv_usec - total_start.tv_usec;
				if (usec < 0) {
					sec--;
					usec = usec + 1000000;
				}
				total_time = (sec * 1000000) + usec;
	/*
				stop_capturing ();
				uninit_device ();
				close_device ();
	*/			
				if( !camera.stopCapture() ) {
					fprintf(stderr, "Error stopping capture on %s\n", params.getCameraDeviceName() );
				}
				printf("%ux%u: %d frames, %.2f fps %.2f fps encoding\n", 
				       params.getCameraWidth(), params.getCameraHeight(), 
				       frame_id, 
				       (frame_id / (tenc_time / 1000000)),
				       (frame_id / (total_time / 1000000)));
				printf("max size: %u bytes\n", maxSize );
			} else
				fprintf(stderr, "Error initializing MJPEG decoder\n" );
		} else
			fprintf(stderr, "Error opening preview window\n" );
	}

	return 0 ;
}
