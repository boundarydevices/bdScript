/* WARNING : this function only works when stride_U == stride_V (I use some hacks to
	     not have to do too many computations at line's end)...

   C-like prototype :
	void convert_yuv420_rgb565(AVPicture *picture, unsigned char *results, int w, int h) ;
   void yuv2rgb_c_16( unsigned char const *py_1, 
                      unsigned char const *py_2,
                      unsigned char const *pu, 
                      unsigned char const *pv,
                      void * _dst_1, 
                      void * _dst_2, 
                      int width )

*/
	.text
	.align
	
	.global yuv2rgb_c_16
yuv2rgb_c_16:
	mov	ip, sp
	stmdb   sp!, { r4 - r12, lr }   @ all callee saved regs
	mov	fp, ip

/*
   r0       - py_1      - passed in register
   r1       - py_2      - passed in register
   r2       - pu        - passed in register
   r3       - pv        - passed in register
   r4       - dst_1     - [fp,#0]
   r5       - dst_2     - [fp,#4]
   r6       - width     - [fp,#8]
*/   
   
   ldr      r4, [fp,#0]
   ldr      r5, [fp,#4]
   ldr      r6, [fp,#8]
	add r7, pc, #(const_storage-.-8)   @ r0 = base pointer to the constants array
	ldr r8, [r7, #(4*4)]               @ r8 = multy	

	stmdb   sp!, { r0-r10 }          // save some room on the stack
   
   @ Stack description :
	@ (sp+ 0)      py_1
	@ (sp+ 4)      py_2
	@ (sp+ 8)      pu
	@ (sp+12)      pv
	@ (sp+16)      dst_1
	@ (sp+20)      dst_2
	@ (sp+24)      width
	@ (sp+28) 
	@ (sp+32) 
	@ (sp+36) 
	@ (sp+40) 

   ldr r9, .inYUVOffs
   mov   r10, #1
   str   r10, [r9,#0]

   add   r0, pc, #(const_storage-.+8)     // r0 = base pointer to the constants array+16 (so ldm works)
   ldm   r0,{r8-r12}                      // r8  = crv
                                          // r9  = cbu
                                          // r10 = cgu
                                          // r11 = cgv
                                          // r12 = multy
   mov r7, #32768                         // r7 = 32768 (for additions in MLA)

yuv_loop:
   //
   // load U, V and get r,g,b components from each
   //
   // process Y1 and Y2 twice each, 
   //    generating R = y + rconst*v
   //               B = y + bconst*u
   //               G = y - gvconst*v - guconst*u
   //    clip each
   //    shift into RGB triplet
   //
	ldr   r1, [sp, #8]                     // r1 = pu 
   ldr   r2, [sp, #12]                    // r2 = pv
   ldrb  r4, [r1,#0]                      // r4 = *pu
   ldrb  r5, [r2,#0]                      // r5 = *pv
   add   r1, r1, #1                       // r1 = pu++
   add   r2, r2, #1                       // r2 = pv++
   sub   r4, r4, #128                     // r4 = *pu-128
   sub   r5, r5, #128                     // r5 = *pv-128

   str   r1, [sp, #8]                     // save pu on stack
   str   r2, [sp, #12]                    // save pu on stack

	mla r1, r4, r8, r7                     // r1 = almost red = crv * (*V - 128) + 32768
	mla r2, r5, r9, r7                     // r2 = almost blue = cbu * (*U - 128) + 32768
   mul r6, r4, r10                        // r4 = uGreen = - cgu * (*pu - 128)
   mla r4, r5, r11, r7                    // r5 = vGreen = - cgv * (*pv - 128) + 32768
   add r3, r4, r6                         // r3 = almost green (just add Y)

   ldr   r4, [sp, #0]                     // r4 = py_1 
   ldrb  r5, [r4,#0]                      // r5 = *py_1
   add   r4, r4, #1                       // py_1++
   str   r4, [sp,#0]                      // save back to stack
   sub   r5, r5, #16                      // r6 = ( *py_1-16 )
   mul   r6, r5, r12                      //      * multy
   add   r4, r6, r1                       // r4 = non-clipped red = (*py_1 - 16) * multy + almost red
   add   r0, pc, #(rb_clip-.-8)           // r0 = red/blue clip array
   ldrb  r4, [r0, r4, asr #(16+3)]        //    = clipped red 
   add   r5, r6, r2                       // r5 = non-clipped blue = (*py_1 - 16) * multy + almost blue
   ldrb  r5, [r0, r5, asr #(16+3)]        //    = clipped blue
   add   r5, r4, r5, lsl #11              // r5 = red and blue in position
   add   r4, r6, r3                       // r4 = non-clipped green = (*py_1 - 16) * multy + almost green
	add   r0, pc, #(g_clip-.-8)            // r0 = green clip array
   ldrb  r6, [r0, r4, asr #(16+2)]        // r6 = clipped green
   add   r4, r5, r6, lsl #5               // r4 = RGB triplet

   ldr   r5, [sp,#16]                     // dst_1
   strh  r4, [r5,#0]                      // *dst_1 is RGB triplet
   add   r5, r5, #2                       //
   str   r5, [sp,#16]                     // save back on stack

   ldr   r4, [sp, #0]                     // r4 = py_1 
   ldrb  r5, [r4,#0]                      // r5 = *py_1
   add   r4, r4, #1                       // py_1++
   str   r4, [sp,#0]                      // save back to stack
   sub   r5, r5, #16                      // r6 = ( *py_1-16 )
   mul   r6, r5, r12                      //      * multy
   add   r4, r6, r1                       // r4 = non-clipped red = (*py_1 - 16) * multy + almost red
   add   r0, pc, #(rb_clip-.-8)           // r0 = red/blue clip array
   ldrb  r4, [r0, r4, asr #(16+3)]        //    = clipped red 
   add   r5, r6, r2                       // r5 = non-clipped blue = (*py_1 - 16) * multy + almost blue
   ldrb  r5, [r0, r5, asr #(16+3)]        //    = clipped blue
   add   r5, r4, r5, lsl #11              // r5 = red and blue in position
   add   r4, r6, r3                       // r4 = non-clipped green = (*py_1 - 16) * multy + almost green
	add   r0, pc, #(g_clip-.-8)            // r0 = green clip array
   ldrb  r6, [r0, r4, asr #(16+2)]        // r6 = clipped green
   add   r4, r5, r6, lsl #5               // r4 = RGB triplet

   ldr   r5, [sp,#16]                     // dst_1
   strh  r4, [r5,#0]                      // *dst_1 is RGB triplet
   add   r5, r5, #2                       //
   str   r5, [sp,#16]                     // save back on stack

   ldr   r4, [sp, #4]                     // r4 = py_2 
   ldrb  r5, [r4,#0]                      // r5 = *py_2
   add   r4, r4, #1                       // py_2++
   str   r4, [sp,#4]                      // save back to stack
   sub   r5, r5, #16                      // r6 = ( *py_2-16 )
   mul   r6, r5, r12                      //      * multy
   add   r4, r6, r1                       // r4 = non-clipped red = (*py_2 - 16) * multy + almost red
   add   r0, pc, #(rb_clip-.-8)           // r0 = red/blue clip array
   ldrb  r4, [r0, r4, asr #(16+3)]        //    = clipped red 
   add   r5, r6, r2                       // r5 = non-clipped blue = (*py_2 - 16) * multy + almost blue
   ldrb  r5, [r0, r5, asr #(16+3)]        //    = clipped blue
   add   r5, r4, r5, lsl #11              // r5 = red and blue in position
   add   r4, r6, r3                       // r4 = non-clipped green = (*py_2 - 16) * multy + almost green
	add   r0, pc, #(g_clip-.-8)            // r0 = green clip array
   ldrb  r6, [r0, r4, asr #(16+2)]        // r6 = clipped green
   add   r4, r5, r6, lsl #5               // r4 = RGB triplet

   ldr   r5, [sp,#20]                     // dst_2
   strh  r4, [r5,#0]                      // *dst_2 is RGB triplet
   add   r5, r5, #2                       //
   str   r5, [sp,#20]                     // save back on stack

   ldr   r4, [sp, #4]                     // r4 = py_2 
   ldrb  r5, [r4,#0]                      // r5 = *py_2
   add   r4, r4, #1                       // py_2++
   str   r4, [sp,#4]                      // save back to stack
   sub   r5, r5, #16                      // r6 = ( *py_2-16 )
   mul   r6, r5, r12                      //      * multy
   add   r4, r6, r1                       // r4 = non-clipped red = (*py_2 - 16) * multy + almost red
   add   r0, pc, #(rb_clip-.-8)           // r0 = red/blue clip array
   ldrb  r4, [r0, r4, asr #(16+3)]        //    = clipped red 
   add   r5, r6, r2                       // r5 = non-clipped blue = (*py_2 - 16) * multy + almost blue
   ldrb  r5, [r0, r5, asr #(16+3)]        //    = clipped blue
   add   r5, r4, r5, lsl #11              // r5 = red and blue in position
   add   r4, r6, r3                       // r4 = non-clipped green = (*py_2 - 16) * multy + almost green
	add   r0, pc, #(g_clip-.-8)            // r0 = green clip array
   ldrb  r6, [r0, r4, asr #(16+2)]        // r6 = clipped green
   add   r4, r5, r6, lsl #5               // r4 = RGB triplet

   ldr   r5, [sp,#20]                     // dst_2
   strh  r4, [r5,#0]                      // *dst_2 is RGB triplet
   add   r5, r5, #2                       //
   str   r5, [sp,#20]                     // save back on stack

   ldr   r6, [sp,#24]                     // width -= 2
   subs  r6, r6, #2
   str   r6, [sp,#24]

   bne yuv_loop

	@ Exit cleanly :-)
	add sp, sp, #(11*4)             @ remove all custom things from stack
   
   ldr r9, .inYUVOffs
   mov   r10, #0
   str   r10, [r9,#0]
	ldmia   sp!, { r4 - r12, pc }   @ restore callee saved regs and return


/* VR[i] = ((i - 128)*  91881) >> 16) + 2048 */
/* UB[i] = ((i - 128)* 116130) >> 16) + 2048 */
/* UG[i] = ((i - 128)* -22554) */
/* VG[i] = ((i - 128)* -46802) */
/* 
   from http://www.fourcc.org/yuv2ppm.c
   
   r = y + (1.370705 * (v-128));
   g = y - (0.698001 * (v-128)) - (0.337633 * (u-128));
   b = y + (1.732446 * (u-128));
   
*/
const_storage:

//          crv,        cbu,        - cgu,    - cgv,       multy
//	.word 0x00019895, 0x00020469, 0xffff9bb5, 0xffff2fe1, 0x00012A15
	.word 0x00019895, 0x00020469, 0xffff9bb5, 0xffff2fe1, 0x00012A15

.inYUVOffs:
	.word	inYUV2RGB

rb_clip_dummy:	
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
rb_clip:
        .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
        .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
        .byte 0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f
        .byte 0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f
        .byte 0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f
g_clip_dummy:	
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
        .byte 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
g_clip:	
        .byte 0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
        .byte 0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f
        .byte 0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f
        .byte 0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f
        .byte 0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f
        .byte 0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f
        .byte 0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f
        .byte 0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f
        .byte 0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f
        .byte 0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f,0x3f


.data
	.global inYUV2RGB

inYUV2RGB:
   .long 0
