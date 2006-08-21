#ifndef __YUYV_H__
#define __YUYV_H__

/*
 * yuyv.h
 *
 * libmpeg2 conversion routine to SM-501 YUYV form.
 *
 */

extern bool volatile convertingYUYV_ ;

int mpeg2convert_yuyv (int stage, void * _id, const mpeg2_sequence_t * seq,
                       int stride, uint32_t accel, void * arg,
                       mpeg2_convert_init_t * result);

#endif
