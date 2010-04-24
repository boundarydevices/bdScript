/*
 * Module imx_mjpeg_decoder.cpp
 *
 * This module defines the methods of the mjpeg_decoder_t class
 * as declared in imx_mjpeg_decoder.h
 *
 * Copyright Boundary Devices, Inc. 2010
 */

#include "imx_mjpeg_decoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <zlib.h>

#define DEBUGPRINT
#include "debugPrint.h"

#define STREAM_BUF_SIZE		0x80000
#define PS_SAVE_SIZE		0x80000

static DecReportInfo userData = {0};

mjpeg_decoder_t::mjpeg_decoder_t(vpu_t &vpu)
	: w_(0)
	, h_(0)
	, ystride_(0)
	, uvstride_(0)
	, imgSize_(0)
	, handle_(0)
	, phy_bsbuf_addr(0)
	, phy_ps_buf(0)
	, phy_slice_buf(0)
	, phy_slicebuf_size(0)
	, virt_bsbuf_addr(0)
	, bsbuf_end(0)
	, fbcount(0)
	, fb(0)
	, pfbpool(0)
	, mvcol_memdesc(0)
	, startedDecode_(false)
	, app_fbs(0)
	, decoder_fbs(0)
	, state_(ERR)
{
	vpu_mem_desc mem_desc = {0};
	mem_desc.size = STREAM_BUF_SIZE;
	int ret = IOGetPhyMem(&mem_desc);
	if (ret) {
		fprintf(stderr,"Unable to obtain physical mem\n");
		return;
	}

	if (IOGetVirtMem(&mem_desc) <= 0) {
		fprintf(stderr,"Unable to obtain virtual mem\n");
		IOFreePhyMem(&mem_desc);
		return;
	}

	vpu_mem_desc slice_mem_desc = {0};

	phy_bsbuf_addr = mem_desc.phy_addr;
	virt_bsbuf_addr = mem_desc.virt_uaddr;
        bsbuf_end = virt_bsbuf_addr + mem_desc.size ;

	DecOpenParam oparam ; memset(&oparam,0,sizeof(oparam));

	oparam.bitstreamFormat = STD_MJPG ;
	oparam.bitstreamBuffer = phy_bsbuf_addr;
	oparam.bitstreamBufferSize = STREAM_BUF_SIZE;
	oparam.reorderEnable = 1 ;
	oparam.mp4DeblkEnable = 0 ;

	oparam.psSaveBuffer = 0 ;
	oparam.psSaveBufferSize = PS_SAVE_SIZE;

	ret = vpu_DecOpen(&handle_, &oparam);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr,"vpu_DecOpen failed: %d\n", ret);
		return ;
	}

	debugPrint("vpu_DecOpen success\n" );
	ret = vpu_DecGiveCommand(handle_,DEC_SET_REPORT_USERDATA, &userData);
debugPrint("vpu_DecGiveCommand: %d(DEC_SET_REPORT_USERDATA)/%d\n", DEC_SET_REPORT_USERDATA,ret);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr, "Failed to set user data report, ret %d\n", ret );
		return ;
	} else
		printf( "disabled userdata\n" );
	/* Parse bitstream and get width/height/framerate etc */
	ret = vpu_DecSetEscSeqInit(handle_, 1);
	if (ret != RETCODE_SUCCESS) {
		fprintf(stderr, "Failed to set Esc Seq, ret %d\n", ret );
		return ;
	} else
                debugPrint("vpu_DecSetEscSeqInit(1): %d\n", ret);
	state_ = INIT ;
}

mjpeg_decoder_t::~mjpeg_decoder_t(void) {
	if(startedDecode_){
		vpu_WaitForInt(0);
	}
	debugPrint( "vpu_DecClose\n" );
	RetCode ret = vpu_DecClose(handle_);
	if (ret == RETCODE_FRAME_NOT_COMPLETE) {
		debugPrint("close with stale decode data\n");
		DecOutputInfo outinfo = {0};
		vpu_DecGetOutputInfo(handle_, &outinfo);
		ret = vpu_DecClose(handle_);
		if (ret != RETCODE_SUCCESS)
			fprintf(stderr,"vpu_DecClose failed\n");
	} else if( RETCODE_SUCCESS != ret ) {
		fprintf(stderr, "Error %d closing decoder\n",ret );
	}
}

bool mjpeg_decoder_t::space_avail( unsigned &length )
{
	RetCode ret;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	unsigned long space;
	ret = vpu_DecGetBitstreamBuffer(handle_,&pa_read_ptr, &pa_write_ptr,&space);
	if( RETCODE_SUCCESS != ret ){
		fprintf(stderr, "Error %d from vpu_DecGetBitstreamBuffer\n", ret );
	}
	else if( 0 < space ) {
		debugPrint( "%s: %u bytes available\n", __func__, space );
		length = space ;
		return true ;
	} else {
		debugPrint( "%s: no space\n", __func__ );
	}
	return false ;
}

bool mjpeg_decoder_t::fill_buf( void const *data, unsigned length, void *opaque )
{
	RetCode ret;
	PhysicalAddress pa_read_ptr, pa_write_ptr;
	unsigned long space;
	ret = vpu_DecGetBitstreamBuffer(handle_,&pa_read_ptr, &pa_write_ptr,&space);
debugPrint("vpu_DecGetBitstreamBuffer: %d, %lx.%lx %u bytes avail\n", ret,pa_read_ptr, pa_write_ptr,space);
	if( RETCODE_SUCCESS != ret ){
		fprintf(stderr, "Error %d from vpu_DecGetBitstreamBuffer\n", ret );
		return false ;
	}
	else if( space < length ) {
		debugPrint( "Not enough room (%lu) for %u bytes of input\n", space, length );
		ret = vpu_DecUpdateBitstreamBuffer(handle_,0);
		if( RETCODE_SUCCESS != ret ){
			fprintf(stderr, "Error %d from flagging end of stream vpu_DecUpdateBitstreamBuffer\n", ret );
		}
		return false ;
	}
	else {
		debugPrint( "fill in %u bytes of data here (max %lu)\n", length, space );
		debugPrint( "readFrom %lx, writePtr %lx\n", pa_read_ptr, pa_write_ptr );
		unsigned long addr = (virt_bsbuf_addr + pa_write_ptr - phy_bsbuf_addr);
		if (addr+length<=bsbuf_end) {
			memcpy((void *)addr,data,length);
		} else {
			space = bsbuf_end-addr ;
			memcpy((void*)addr,data,space);
			memcpy((void*)virt_bsbuf_addr,data,length-space);
		}
		ret = vpu_DecUpdateBitstreamBuffer(handle_,length);
debugPrint("vpu_DecUpdateBitstreamBuffer: %d, %u\n", ret, length );
		if( RETCODE_SUCCESS != ret ){
			fprintf(stderr, "Error %d from vpu_DecUpdateBitstreamBuffer\n", ret );
			return false ;
		}
		if( INIT == state_ ) {
			DecInitialInfo initinfo = {0};
			ret = vpu_DecGetInitialInfo(handle_, &initinfo);
debugPrint("vpu_DecGetInitialInfo: %d\n", ret);
			if (ret != RETCODE_SUCCESS) {
				fprintf(stderr, "vpu_DecGetInitialInfo failed %d\n", ret);
				return false ;
			}
			else {
				vpu_DecSetEscSeqInit(handle_, 0);
debugPrint("vpu_DecSetEscSeqInit(0): %d\n", ret);
				if (initinfo.streamInfoObtained) {
					printf( "have stream info\n" );
					printf("Decoder: width = %d, height = %d, fps = %lu, minfbcount = %u\n",
							initinfo.picWidth, initinfo.picHeight,
							initinfo.frameRateInfo,
							initinfo.minFrameBufferCount);
					assert(0 == fbcount);
					assert(0 == fb);
					assert(0 == pfbpool);
					fbcount = initinfo.minFrameBufferCount + 2 ;
					fb = (FrameBuffer *)calloc(fbcount, sizeof(fb[0]));
					if (fb == NULL) {
						fprintf(stderr,"Failed to allocate fb\n");
						return false ;
					}
					pfbpool = (frame_buf *)calloc(fbcount, sizeof(pfbpool[0]));
					if (pfbpool == NULL) {
						fprintf(stderr, "Failed to allocate pfbpool\n");
						return false ;
					}
printf("fb == %p, pfbpool == %p, malloc == %p\n", fb, pfbpool, malloc(0x10000) );
					w_ = initinfo.picWidth ;
					h_ = initinfo.picHeight ;
					ystride_ = (initinfo.picWidth + 15)&~15;
					uvstride_ = ((initinfo.picWidth/2)+15)&~15;
printf( "strides: %u/%u\n", ystride_, uvstride_ );
					unsigned paddedHeight = (h_+15)&~15 ; 		// Why???
					unsigned ySize = ystride_ * paddedHeight ;
					unsigned uvSize = uvstride_ * (paddedHeight/2);

					imgSize_ = paddedHeight*(ystride_+uvstride_);
printf( "imgSize == %u, ySize %u, uvSize %u\n", imgSize_, ySize, uvSize );
					imgSize_ += (ystride_*paddedHeight) / 4 ; 	// Why?
printf( "padded == %u\n", imgSize_ );

					for (unsigned i = 0; i < fbcount ; i++) {
                                                decoder_fbs |= (1 <<i);
						struct frame_buf *f = pfbpool + i ;
debugPrint( "pfbpool[%u] == %p\n", i, f);
						f->desc.size = imgSize_ ;
						int err = IOGetPhyMem(&f->desc);
						if (err) {
							printf("Frame buffer allocation failure: %d\n", err);
							return false ;
						}
						debugPrint( "allocated frame buffer %u == 0x%x\n", i, f->desc.phy_addr );
						f->addrY = f->desc.phy_addr;
						f->addrCb = f->addrY + ySize ;
						f->addrCr = f->addrCb + uvSize ;
						f->mvColBuf = f->addrCr + uvSize ;
						f->desc.virt_uaddr = IOGetVirtMem(&(f->desc));
						debugPrint( "virt == %p\n", f->desc.virt_uaddr );
						if (f->desc.virt_uaddr <= 0) {
							fprintf(stderr, "Error mapping frame buffer memory\n" );
							return false ;
						}

						fb[i].bufY = f->addrY;
						fb[i].bufCb = f->addrCb;
						fb[i].bufCr = f->addrCr;
						fb[i].bufMvCol = f->mvColBuf ;
					}
debugPrint( "allocated frame buffers\n" );
	for( unsigned i = 0 ; i < fbcount ; i++ ) {
		debugPrint("fb[%d].bufY = 0x%lx\n", i, fb[i].bufY );
		debugPrint("fb[%d].bufCb = 0x%lx\n", i, fb[i].bufCb );
		debugPrint("fb[%d].bufCr = 0x%lx\n", i, fb[i].bufCr );
		debugPrint("fb[%d].bufMvCol = 0x%lx\n", i, fb[i].bufMvCol );
	}
					DecBufInfo bufinfo = {0};
					ret = vpu_DecRegisterFrameBuffer(handle_, fb, fbcount, ystride_, &bufinfo);
debugPrint("vpu_DecRegisterFrameBuffer: %d, %u fbs, stride %u\n", ret, fbcount,ystride_);
					if (ret != RETCODE_SUCCESS) {
						fprintf(stderr,"Register frame buffer failed\n");
						return false ;
					}
debugPrint( "registered frame buffers\n");
					int rot_angle = 0 ;
					ret = vpu_DecGiveCommand(handle_, SET_ROTATION_ANGLE, &rot_angle);
debugPrint("vpu_DecGiveCommand: %d(SET_ROTATION_ANGLE)/%d, angle %d\n", SET_ROTATION_ANGLE,ret, rot_angle);
					if (ret != RETCODE_SUCCESS)
						fprintf(stderr,"rot_angle %d\n", ret);
					int mirror = 0 ;
					ret = vpu_DecGiveCommand(handle_, SET_MIRROR_DIRECTION,&mirror);
debugPrint("vpu_DecGiveCommand: %d(SET_MIRROR_DIRECTION)/%d, mirror %d\n", SET_MIRROR_DIRECTION,ret, mirror);
					if (ret != RETCODE_SUCCESS)
						fprintf(stderr,"mirror %d\n", ret);
					int rot_stride = ystride_ ;
					ret = vpu_DecGiveCommand(handle_, SET_ROTATOR_STRIDE, &rot_stride);
debugPrint("vpu_DecGiveCommand: %d(SET_ROTATOR_STRIDE)/%d, stride %d\n", SET_ROTATOR_STRIDE,ret, rot_stride);
					if (ret != RETCODE_SUCCESS)
						fprintf(stderr,"rotstride %d\n", ret);
					state_ = DECODING ;
debugPrint("before decode: image size %u, ySize %u, uvSize %u\n", yuvSize(), this->ySize(), this->uvSize() );
					startDecode();
				}
			}
		}
		return true ;
	}
}

void mjpeg_decoder_t::startDecode(void)
{
	if( !startedDecode_&& (DECODING == state_) ){
		unsigned idx = ffs(decoder_fbs);
		if (0 < idx) {
			idx-- ;
#if 1
			RetCode ret = vpu_DecGiveCommand(handle_, SET_ROTATOR_OUTPUT, (void *)&fb[idx]);
debugPrint("vpu_DecGiveCommand: %d(SET_ROTATOR_OUTPUT)/%d, fb0 (%p)\n", SET_ROTATOR_OUTPUT,ret, &fb[idx]);
			if (ret != RETCODE_SUCCESS) {
				fprintf(stderr,"rotout %d\n", ret);
				return ;
			}
#endif
			DecParam decparam = {0};
			ret = vpu_DecStartOneFrame(handle_, &decparam);
					debugPrint("vpu_DecStartOneFrame: %d: %d,%d,%d,%d,%d,%d,%d,%d,%lx\n",
						ret,
						decparam.prescanEnable,
						decparam.prescanMode,
						decparam.dispReorderBuf,
						decparam.iframeSearchEnable,
						decparam.skipframeMode,
						decparam.skipframeNum,
						decparam.chunkSize,
						decparam.picStartByteOffset,
						decparam.picStreamBufferAddr
					);
			if (ret == RETCODE_SUCCESS) {
				startedDecode_ = true ;
			} else {
				fprintf(stderr, "DecStartOneFrame failed: %d\n", ret);
			}
		} else {
			debugPrint( "%s: no fbs available\n", __func__ );
		}
	}
	else
		debugPrint( "%s: not ready\n", __func__ );
}

bool mjpeg_decoder_t::decode_complete(void const *&y, void const *&u, void const *&v, void *&opaque, unsigned &displayIdx, int &picType, bool &errors )
{
        displayIdx = 0 ;
	errors = false ;
	if( startedDecode_ && !vpu_IsBusy() ){
                DecOutputInfo outinfo = {0};
		RetCode ret = vpu_DecGetOutputInfo(handle_, &outinfo);
debugPrint("vpu_DecGetOutputInfo(%d): %d display, %d decode\n", ret, outinfo.indexFrameDisplay, outinfo.indexFrameDecoded);
		if (ret == RETCODE_SUCCESS) {
			startedDecode_ = false ;
			debugPrint("success %d\n", outinfo.decodingSuccess);
			debugPrint("indexFrame decoded %d (0x%x)\n", outinfo.indexFrameDecoded, outinfo.indexFrameDecoded);
			debugPrint("indexFrame display %d (0x%x)\n", outinfo.indexFrameDisplay, outinfo.indexFrameDisplay);
			debugPrint("picType %d\n", outinfo.picType);
			picType = outinfo.picType ;
			if( 0 != outinfo.numOfErrMBs) {
				debugPrint("%d errMBs\n", outinfo.numOfErrMBs);
				errors = true ;
			}
			debugPrint("qpInfo %p\n", outinfo.qpInfo);
			debugPrint("hscale %d\n", outinfo.hScaleFlag);
			debugPrint("vscale %d\n", outinfo.vScaleFlag);
			debugPrint("index rangemap %d\n", outinfo.indexFrameRangemap);
			debugPrint("prescan result %d\n", outinfo.prescanresult);
			debugPrint("picstruct %d\n", outinfo.pictureStructure);
			debugPrint("topfield first %d\n", outinfo.topFieldFirst);
			debugPrint("repeat first %d\n", outinfo.repeatFirstField);
			debugPrint("field seq %d\n", outinfo.fieldSequence);
			debugPrint("size %dx%d\n", outinfo.decPicWidth, outinfo.decPicHeight);
			debugPrint("nsps %d\n", outinfo.notSufficientPsBuffer);
			debugPrint("nsslice %d\n", outinfo.notSufficientSliceBuffer);
			debugPrint("success %d\n", outinfo.decodingSuccess);
			debugPrint("interlaced %d\n", outinfo.interlacedFrame);
			debugPrint("mp4packed %d\n", outinfo.mp4PackedPBframe);
			debugPrint("h264Npf %d\n", outinfo.h264Npf);
			debugPrint("prog/repeat %d\n", outinfo.progressiveFrame);
			debugPrint("crop %d:%d->%d:%d\n", outinfo.decPicCrop.left,outinfo.decPicCrop.top, outinfo.decPicCrop.right, outinfo.decPicCrop.bottom);
			if( (0 <= outinfo.indexFrameDisplay) && (fbcount > outinfo.indexFrameDisplay) ){
				unsigned mask = 1<<outinfo.indexFrameDisplay ;
				assert(0 != (decoder_fbs&mask));
				decoder_fbs &= ~mask ;
				assert(0 == (app_fbs & mask));
				app_fbs |= mask ;

				frame_buf *pfb = pfbpool+outinfo.indexFrameDisplay ;
				y = (void *)(pfb->addrY + pfb->desc.virt_uaddr - pfb->desc.phy_addr);
				u = (char *)y + ySize();
				v = (char *)u + uvSize();
                                displayIdx = outinfo.indexFrameDisplay ;
				debugPrint("returning display index %u:%p\n", displayIdx,y);
				return true ;
			} else {
				fprintf(stderr, "Invalid display fb index: %d\n", outinfo.indexFrameDisplay);
				startDecode();
			}
		} else {
			fprintf( stderr, "Error %d from vpu_DecGetOutputInfo\n", ret );
		}
	} else {
		debugPrint( "VPU is busy\n" );
                vpu_WaitForInt(0);
	}

	return false ;
}

void mjpeg_decoder_t::releaseBuffer(unsigned displayIdx)
{
	unsigned mask = 1<<displayIdx ;
	assert(displayIdx < fbcount);
	assert(0 == (decoder_fbs & mask));
	assert(0 != (app_fbs & mask));
	app_fbs &= ~mask ;
	decoder_fbs |= mask ;

        RetCode err = vpu_DecClrDispFlag(handle_, displayIdx);
	if( RETCODE_SUCCESS == err ){
		debugPrint("%s: released buffer %u\n", __func__, displayIdx );
	}
	else
		fprintf(stderr, "%s: Error %d releasing buffer %u\n", __func__, err, displayIdx );
	startDecode();
}

#ifdef MODULETEST_MPEG4_DECODER

static unsigned long adlerYUV
	( unsigned char const *y, 
	  unsigned char const *u, 
	  unsigned char const *v,
	  unsigned w,
	  unsigned h,
	  unsigned yStride,
	  unsigned uvStride )
{
	unsigned long rval = 0 ;
	for( unsigned i = 0 ; i < h ; i++ ){
		rval = adler32(rval,y,w);
		y += yStride ;
	}
	h /= 2 ;
	for( unsigned i = 0 ; i < h ; i++ ){
		rval = adler32(rval,u,w);
		u += uvStride ;
	}
	for( unsigned i = 0 ; i < h ; i++ ){
		rval = adler32(rval,v,w);
		v += uvStride ;
	}
	return rval ;
}

extern int vpu_debug ;

#include "v4l_overlay.h"
#include <linux/videodev2.h>
#include "tickMs.h"
#include <ctype.h>

unsigned xPos = 0 ;
unsigned yPos = 0 ;
unsigned width = 640 ;
unsigned height = 360 ;
unsigned debugLevel = 
#ifdef DEBUGPRINT        
	7 ;
#else
	0 ;
#endif

static void parseArgs( int &argc, char const **argv )
{
	for( unsigned arg = 1 ; arg < argc ; arg++ ){
		if( '-' == *argv[arg] ){
			char const *param = argv[arg]+1 ;
			if( 'x' == tolower(*param) ){
            			xPos = strtoul(param+1,0,0);
			}
			else if( 'y' == tolower(*param) ){
            			yPos = strtoul(param+1,0,0);
			}
			else if( 'w' == tolower(*param) ){
            			width = strtoul(param+1,0,0);
			}
			else if( 'h' == tolower(*param) ){
            			height = strtoul(param+1,0,0);
			}
			else if( 'd' == tolower(*param) ){
            			debugLevel = strtoul(param+1,0,0);
			}
			else
				printf( "unknown option %s\n", param );

			// pull from argument list
			for( int j = arg+1 ; j < argc ; j++ ){
				argv[j-1] = argv[j];
			}
			--arg ;
			--argc ;
		}
	}
}

int main(int argc, char const **argv){

	parseArgs(argc,argv);

	if( 2 > argc ){
		fprintf(stderr, "Usage: %s inFile\n", argv[0]);
		return -1 ;
	}
	int rv = -1 ;
	vpu_debug = debugLevel ;
        vpu_t vpu ;

	v4l_overlay_t *overlay = 0 ;

	if(vpu.worked()) {
		mjpeg_decoder_t decoder(vpu);
		if( decoder.initialized() ){
			printf( "ready for data\n" );
			int fdIn = open(argv[1], O_RDONLY);
			if( 0 <= fdIn ){
				bool first = true ;
				bool eof = false ;
				long long start = tickMs();
				unsigned numDecoded = 0 ;
				unsigned numSkipped = 0 ;
				bool skippin = false ;
				while( !eof ){
					unsigned how_much ;
					if( decoder.space_avail(how_much) ){
						debugPrint( "%u bytes available\n", how_much );
						unsigned char input[16384];
						unsigned max = how_much < sizeof(input) ? how_much : sizeof(input);
						int numRead ;
						if( 0 < (numRead = read(fdIn,input,max)) ){
							if( decoder.fill_buf(input,numRead,0) ){
								debugPrint("%u bytes filled\n", max);
							} else {
								fprintf(stderr, "Error feeding decoder\n" );
								return -1 ;
							}
						}
						else
							eof = true ;
					} // 
					else
						debugPrint("no space\n" );
					for( unsigned i = 0 ; i < 1 ; i++ ){
						void const *y, *u, *v ;
						void *opaque ;
						unsigned displayIdx ;
						int picType ;
						bool errors ;
						printf( "decoding\n" );
						if( decoder.decode_complete(y,u,v,opaque,displayIdx,picType,errors) ) {
							printf( "decoded\n" );
							++numDecoded ;
                                                        if(errors){
								skippin = true ;
							} else if(skippin && (decoder.PICTYPE_IFRAME == picType) ){
								skippin = false ;
							}
							if( 0 == overlay ){
								overlay = new v4l_overlay_t(decoder.width(), decoder.height(), xPos, yPos, width, height, 0, V4L2_PIX_FMT_YUV420);
								if( !overlay->isOpen() ){
									fprintf(stderr, "Error opening overlay: %ux%u\n", decoder.width(), decoder.height());
								}
							}
							if( 0 ) printf( "have decoded data: adler 0x%lx\n",
								adlerYUV((unsigned char *)y,
									 (unsigned char *)u,
									 (unsigned char *)v,
									 decoder.width(),
									 decoder.height(),
									 decoder.yStride(),
									 decoder.uvStride() ) );
							if( !skippin && overlay && (overlay->imgSize() <= decoder.yuvSize()) ){
								printf( "putting picture to display\n" );
								unsigned idx ;
								unsigned char *yOut, *uOut, *vOut ;
								if( overlay->getBuf(idx,yOut,uOut,vOut) ){
									memcpy(yOut,y,overlay->ySize());
									memcpy(uOut,u,overlay->uvSize());
									memcpy(vOut,v,overlay->uvSize());
									overlay->putBuf(idx);
									if(first){
										first = false ;
										unsigned i = 0 ; 
										while( (++i < 4) && overlay->getBuf(idx,yOut,uOut,vOut) ){
											memcpy(yOut,y,overlay->ySize());
											memcpy(uOut,u,overlay->uvSize());
											memcpy(vOut,v,overlay->uvSize());
											overlay->putBuf(idx);
										}
									}
								} else
									fprintf(stderr, "No output buf\n" );
char temp[80];
if( !fgets(temp,sizeof(temp),stdin) ){
	return -1;
}							} else if(skippin) {
								fprintf(stderr, "MB errors: picType %d\n", picType );
                                                                numSkipped++ ;
							} else if(overlay) {
								fprintf(stderr, "overlay size mismatch: %u/%u\n", decoder.yuvSize(), overlay->imgSize());
								fprintf(stderr, "overlay sizes: %u/%u\n", overlay->ySize(), overlay->uvSize() );
								fprintf(stderr, "decoder sizes: %u/%u\n", decoder.ySize(), decoder.uvSize() );
							} else {
								fprintf(stderr, "no overlay\n" );
							}
							decoder.releaseBuffer(displayIdx);
							break;
						}
						else {
							debugPrint( "not yet: %d\n", i );
						}
					} // try to decode
				} // until end-of-file
				long long end = tickMs();
				unsigned elapsed = end-start ;
				printf( "%u frames decoded in %u ms (%u fps)\n", numDecoded, elapsed, (numDecoded*1000)/elapsed );
				printf( "%u frames skipped\n", numSkipped);
			} else 
				perror(argv[1]);
		} else
			fprintf(stderr, "Error initializing decoder\n" );
	} else
		fprintf(stderr, "Error initializing VPU\n" );
	return rv ;
}

#endif
