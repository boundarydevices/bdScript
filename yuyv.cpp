/*
 * yuyv.c
 * Copyright (C) 2000-2003 Michel Lespinasse <walken@zoy.org>
 * Copyright (C) 2003      Regis Duchesne <hpreg@zoy.org>
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 * See http://libmpeg2.sourceforge.net/ for updates.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <inttypes.h>
#include <mpeg2dec/mpeg2.h>
#include <mpeg2dec/mpeg2convert.h>

typedef struct {
    int width;
    int stride;
    int chroma420;
    uint8_t * out;
} convert_yuyv_t;

static void yuyv_start (void * _id, const mpeg2_fbuf_t * fbuf,
                        const mpeg2_picture_t * picture,
                        const mpeg2_gop_t * gop)
{
    convert_yuyv_t * instance = (convert_yuyv_t *) _id;

    instance->out = fbuf->buf[0];
    instance->stride = instance->width;
    if (picture->nb_fields == 1) {
        if (! (picture->flags & PIC_FLAG_TOP_FIELD_FIRST))
            instance->out += 2 * instance->stride;
        instance->stride <<= 1;
    }
}

#define PACK(a,b,c,d) (((d) << 24) | ((c) << 16) | ((b) << 8) | (a))

static void yuyv_line (uint8_t * py, uint8_t * pu, uint8_t * pv, void * _dst,
                       int width)
{
    uint32_t * dst = (uint32_t *) _dst;

    __asm__ volatile (
       "  pld   [%0, #0]\n"
       "  pld   [%0, #32]\n"
       "  pld   [%1, #0]\n"
       "  pld   [%1, #32]\n"
       "  pld   [%2, #0]\n"
       "  pld   [%2, #32]\n"
       "  pld   [%3, #0]\n"
       "  pld   [%3, #32]\n"
       "  pld   [%0, #64]\n"
       "  pld   [%0, #96]\n"
       "  pld   [%1, #64]\n"
       "  pld   [%1, #96]\n"
       "  pld   [%2, #64]\n"
       "  pld   [%2, #96]\n"
       "  pld   [%3, #64]\n"
       "  pld   [%3, #96]\n"
       :
       : "r" (py), "r" (pu), "r" (pv), "r" (dst)
    );


    width >>= 4;
    do {
        dst[0] = PACK ( py[0], pu[0],  py[1], pv[0]);
        dst[1] = PACK ( py[2], pu[1],  py[3], pv[1]);
        dst[2] = PACK ( py[4], pu[2],  py[5], pv[2]);
        dst[3] = PACK ( py[6], pu[3],  py[7], pv[3]);
        dst[4] = PACK ( py[8], pu[4],  py[9], pv[4]);
        dst[5] = PACK (py[10], pu[5], py[11], pv[5]);
        dst[6] = PACK (py[12], pu[6], py[13], pv[6]);
        dst[7] = PACK (py[14], pu[7], py[15], pv[7]);
        py += 16;
        pu += 8;
        pv += 8;
        dst += 8;
    } while (--width);
}

static void yuyv_copy (void * id, uint8_t * const * src, unsigned int v_offset)
{
    const convert_yuyv_t * instance = (convert_yuyv_t *) id;
    uint8_t * py;
    uint8_t * pu;
    uint8_t * pv;
    uint8_t * dst;
    int height;

    dst = instance->out + 2 * instance->stride * v_offset;
    py = src[0]; pu = src[1]; pv = src[2];

    height = 16;
    do {
        yuyv_line (py, pu, pv, dst, instance->width);
        dst += 2 * instance->stride;
        py += instance->stride;
        if (! (--height & instance->chroma420)) {
            pu += instance->stride >> 1;
            pv += instance->stride >> 1;
        }
    } while (height);
}

int mpeg2convert_yuyv (int stage, void * _id, const mpeg2_sequence_t * seq,
                       int stride, uint32_t accel, void * arg,
                       mpeg2_convert_init_t * result)
{
    convert_yuyv_t * instance = (convert_yuyv_t *) _id;

    if (seq->chroma_width == seq->width)
        return 1;

    if (instance) {
        instance->width = seq->width;
        instance->chroma420 = (seq->chroma_height < seq->height);
        result->buf_size[0] = seq->width * seq->height * 2;
        result->buf_size[1] = result->buf_size[2] = 0;
        result->start = yuyv_start;
        result->copy = yuyv_copy;
    } else {
        result->id_size = sizeof (convert_yuyv_t);
    }

    return 0;
}
