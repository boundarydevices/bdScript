#include "ICommon.h"
#include "SImageDataJpeg.h"
#include "SJpeg.h"
#include "mathLib.h"

#include "coefficients.h"
#undef ZeroMemory
#define ZeroMemory(dest,count)  ZeroMem(count,dest)
void __fastcall ZeroMem(int count,void* dest);

/////////////////////////////////////////////////////////////////////////////
// SJpeg

SJpeg::SJpeg()
{
	int i;
	m_IData = NULL;
	basePtr=NULL;
	for (i=0;i<=15;i++) {dqt_tables[i]=NULL;}
	for (i=0;i<=7;i++) {dht_tables[i]=NULL;}
	mcuRow=NULL;
	mcuRowCnt=0;
}

void SJpeg::FreeWorkArea()
{
	for (int i=0;i<=7;i++)
	{
		if (dht_tables[i])
		{
			delete dht_tables[i];
			dht_tables[i]=NULL;
		}
	}

}

void SJpeg::FreeDataArea()
{
	for (int i=0;i<=15;i++)
	{
		if (dqt_tables[i])
		{
			delete[] dqt_tables[i];
			dqt_tables[i]=NULL;
		}
	}
	lpMCURowStruct p1;
	lpMCURowStruct p2;
	p1 = mcuRow;
	mcuRow =NULL;
	while (p1)
	{
		p2 = (p1->nextMCURow);
		delete[] (BYTE*)p1;
		p1 = p2;
	}

}



SJpeg::~SJpeg()
{
	FreeWorkArea();
	FreeDataArea();
}



/////////////////////////////////////////////////////////////////////////////
// SJpeg message handlers


void SJpeg::GetMCURow(StartOfScan* sos)
{
	DWORD MCUsLeftInRow;
	if (sos->numberOfComponents>1)
	{
		MCUsLeftInRow = sof_hortMCUs;
	}
	else
	{
		MCUsLeftInRow = 1;
	}

	short* curMCUBase = (sos->curMCURow->data);
	while (MCUsLeftInRow--)
	{
		//get entire MCU data, or all of 1 component of MCU Row
		DWORD curComp=0;
		while (curComp<sos->numberOfComponents)
		{
			short* dct;
			short* dctComponentBase;
			DWORD compDataCnt;
			DWORD lastCompDataCnt;
			DWORD skipOtherComponents;
			DWORD dctComponentAdvance;
			DWORD compHortMCUs;
			DWORD compLinesLeft;
			int compLastDCVal;
			BYTE compHuffDC;
			BYTE compHuffAC;
			BYTE csj;
			BYTE compSS;

			BYTE compSE;
			BYTE compVSampling;
			BYTE compHSampling;
			BYTE spare=0;
	
			compSE=sos->se;
			compLastDCVal = sos->LastDCVal[curComp];
			csj = sos->component[curComp];
			compHuffDC = sos->huffDC[curComp];
			compHuffAC = sos->huffAC[curComp];

			compHSampling = sof_componentSamplingFactors[csj];
			compVSampling = compHSampling &15;
			compHSampling >>= 4;
			dctComponentBase = (short*)(((BYTE*)curMCUBase)+(sof_componentOffset[csj]<<7));
			if (sos->numberOfComponents>1)
			{
				compDataCnt = compHSampling*compVSampling; //parameters for 1 MCU component
				lastCompDataCnt = compDataCnt;
				compHortMCUs = 1;
				compLinesLeft = 1;
				skipOtherComponents=0;
				dctComponentAdvance=0;
			}
			else
			{
				DWORD rem;
				compHortMCUs= sof_hortMCUs;
				compDataCnt = compHSampling;				//parameters for 1 componet of MCU Row.
				lastCompDataCnt = ((uDivRem(sof_samplesPerLine*compHSampling+sof_maxHFactor-1,sof_maxHFactor,&rem)+7)>>3)- ((compHortMCUs-1)*compHSampling);
				compLinesLeft = ((uDivRem(sof_numberOfLines*compVSampling+sof_maxVFactor-1,sof_maxVFactor,&rem)+7)>>3)- ((sos->curRowNum)*compVSampling);
				if ((compLinesLeft>compVSampling)||(compLinesLeft<=0)) compLinesLeft = compVSampling;
				skipOtherComponents=(sof_dataUnitCnt-compHSampling)<<7;
				dctComponentAdvance=compHSampling<<7; 
			}
			while (compLinesLeft--)
			{
				dct = dctComponentBase;
				DWORD hortMCUs = compHortMCUs;
				while (hortMCUs--)
				{
					DWORD contigDCTs = lastCompDataCnt;
					if (hortMCUs) contigDCTs = compDataCnt;
					while (contigDCTs--)
					{
						//ReadDCT
						BYTE codeByte;
						BYTE rrrr,ssss;
						compSS = sos->ss;
						dct += compSS;
						int val;
						if (!compSS)
						{
							if (!sos->bitCnt)
							{
								//first stage
								codeByte = GetHuffByte(compHuffDC);
								if (codeByte)
								{
									if (codeByte<16) val = GetBits(codeByte);
									else val = 32768;
								}
								else val=0;
								compLastDCVal += (val<<sos->al);
								val = compLastDCVal; 
							}
							else
							{
								val = (GetUnsignedBits(sos->bitCnt)<<sos->al);
							}
							*dct++ += (short)val;
							compSS++;
						}

						while (compSS<=compSE)
						{
							if (!sos->eobCnt)
							{
								codeByte = GetHuffByte(compHuffAC);
								ssss = codeByte &15;
								rrrr = codeByte >>4;
							}
							else	
							{
								ssss = 0;
								rrrr = 63;	//skip all AC coefficients
								sos->eobCnt--;
							}
							if (ssss || (rrrr>=15))	//not an EOBn
							{
								val = GetBits(ssss);
								if (!sos->bitCnt)
								{
									//first stage
									dct += rrrr;	//skip rrrr coefficients
									compSS += rrrr;
								}
								else
								{
									while (compSS<=compSE)
									{
										if (*dct)
										{
											short sVal = (short)(GetUnsignedBits(sos->bitCnt)<<sos->al);
											if (*dct>0)
											{
												*dct++ += sVal;
											}
											else
											{
												*dct++ -= sVal;
											}
										}
										else
										{
											if (rrrr==0) break;
											rrrr--;
											dct++;
										}
										compSS++;
									}
								}
								if (compSS<=compSE)
								{
									*dct++ += (short)(val<<sos->al);
									compSS++;
								}
							}
							else
							{
								//EOB0-EOB14
								sos->eobCnt = GetEOBCnt(rrrr);
							}

						}//while (compSS<=compSE)
						dct += (64-compSS);							   //advance to next DCT
						if (sos->nonInterleavedRestartCnt)
						{
							if ( !(--sos->nonInterleavedRestartCnt))
							{
								sos->nonInterleavedRestartCnt=sos->restartInterval;
								if (skipRST(sos)){compLastDCVal=0;}
							}
						}
					}//while (contigDCTs--) 
					dct = (short*)( ((BYTE*)dct)+skipOtherComponents); //next DCT group in row
				}//while (hortMCUs--)
				dctComponentBase = (short*)( ((BYTE*)dctComponentBase)+dctComponentAdvance);	   //next line of DCTs
			}//while (compLinesLeft--)
			sos->LastDCVal[curComp++]=compLastDCVal;
		}//while (curComp<sos->numberOfComponents)
		curMCUBase += (sof_dataUnitCnt<<6);
		if (sos->interleavedRestartCnt)
		{
			if ( !(--sos->interleavedRestartCnt))
			{
				sos->interleavedRestartCnt=sos->restartInterval;
				skipRST(sos);
			}
		}
	}//while (MCUsLeftInRow--)
}

#ifdef MICROSOFT_EMBEDDED
extern "C" int __umull18(unsigned valueA,unsigned valueB);

#else
#define __umull18(a,b) \
({  register int __rTempLo,__rTempHi;	register int __val=a;	\
  __asm__ ("%@ Inlined smull				\n\
	umull	%1,%2,%3,%0				\n\
	mov	%0,%2,LSL # (32-18)			\n\
	add	%0,%0,%1,LSR # (18)"			\
           : "=&r" (__val), 				\
             "=&r" (__rTempLo), "=&r" (__rTempHi)	\
           : "r" (b), "0" (__val) );			\
	     __val;})
#endif

#define fracBits 16

#define MultCCDiv4(a,b) __umull18(a,b)

#define A00 0
#define A01 1		//Aij, row, then column, zig-zag sequence
#define A02 5
#define A03 6
#define A04 14
#define A05 15
#define A06 27
#define A07 28
#define A10 2
#define A11 4
#define A12 7
#define A13 13
#define A14 16
#define A15 26
#define A16 29
#define A17 42
#define A20 3
#define A21 8
#define A22 12
#define A23 17
#define A24 25
#define A25 30
#define A26 41
#define A27 43
#define A30 9
#define A31 11
#define A32 18
#define A33 24
#define A34 31
#define A35 40
#define A36 44
#define A37 53
#define A40 10
#define A41 19
#define A42 23
#define A43 32
#define A44 39
#define A45 45
#define A46 52
#define A47 54
#define A50 20
#define A51 22
#define A52 33
#define A53 38
#define A54 46
#define A55 51
#define A56 55
#define A57 60
#define A60 21
#define A61 34
#define A62 37
#define A63 47
#define A64 50
#define A65 56
#define A66 59
#define A67 61
#define A70 35
#define A71 36
#define A72 48
#define A73 49
#define A74 57
#define A75 58
#define A76 62
#define A77 63
/*
static const int qscale_normal[64] __attribute__ ((aligned (32))) =
		{ RD4(CE00),
		  RD4(CE01), RD4(CE10),
		  RD4(CE20), RD4(CE11), RD4(CE02),
		  RD4(CE03), RD4(CE12), RD4(CE21), RD4(CE30),
		  RD4(CE40), RD4(CE31), RD4(CE22), RD4(CE13), RD4(CE04),
		  RD4(CE05), RD4(CE14), RD4(CE23), RD4(CE32), RD4(CE41), RD4(CE50),
		  RD4(CE60), RD4(CE51), RD4(CE42), RD4(CE33), RD4(CE24), RD4(CE15), RD4(CE06),
		  RD4(CE07), RD4(CE16), RD4(CE25), RD4(CE34), RD4(CE43), RD4(CE52), RD4(CE61), RD4(CE70),
		             RD4(CE71), RD4(CE62), RD4(CE53), RD4(CE44), RD4(CE35), RD4(CE26), RD4(CE17),
			                RD4(CE27), RD4(CE36), RD4(CE45), RD4(CE54), RD4(CE63), RD4(CE72),
					           RD4(CE73), RD4(CE64), RD4(CE55), RD4(CE46), RD4(CE37),
						              RD4(CE47), RD4(CE56), RD4(CE65), RD4(CE74),
							                 RD4(CE75), RD4(CE66), RD4(CE57),
									            RD4(CE67), RD4(CE76),
										               RD4(CE77)
		};
*/


void SJpeg::do_DQT()
{
	int* qt_entry;
	int tempTable[64];
	DWORD size;
	BYTE byte;

	DWORD length=readWord(); //length
	length-=2;
	while (length>0)
	{
		byte=readByte();
		length--;
		int i = byte >> 4;
		byte &= 15;
		qt_entry = dqt_tables[byte];
		if (!qt_entry)
		{
			dqt_tables[byte] = qt_entry = new int[64];
		}
		if (i)
		{
			size=128;
			if (size>length) size=(length&0x7e);
			length -=size;
			size >>=1;
			i=0;
			while (size)
			{
				tempTable[i++] = readWord();
				size--;
			}
		}
		else
		{
			size=64;
			if (size>length) size=length;
			length -=size;
			i=0;
			while (size)
			{
				tempTable[i++] = readByte();
				size--;
			}
		}
		while (i<64) {tempTable[i++]=1;}
//the dqt table is stored in the order required for easy access
//in the dequantization, this is the order after the permutation matrix.
//include Q/4 in quantization table

//Q'[] = |c4   0   0   0   0   0   0   0 |
//       | 0 -c1   0   0   0   0   0   0 |
//       | 0   0 -c2   0   0   0   0   0 |
//       | 0   0   0 -c3   0   0   0   0 |
//       | 0   0   0   0 -c4   0   0   0 |
//       | 0   0   0   0   0 -c5   0   0 |
//       | 0   0   0   0   0   0 -c6   0 |
//       | 0   0   0   0   0   0   0  c7 |

//Q' xo Q' = |c4Q  0   0   0   0   0   0   0  |
//           | 0 -c1Q  0   0   0   0   0   0  |
//           | 0   0 -c2Q  0   0   0   0   0  |
//           | 0   0   0 -c3Q  0   0   0   0  |
//           | 0   0   0   0 -c4Q  0   0   0  |
//           | 0   0   0   0   0 -c5Q  0   0  |
//           | 0   0   0   0   0   0 -c6Q  0  |
//           | 0   0   0   0   0   0   0  c7Q |

//0
#if 1
//row order
		*qt_entry++ =  (tempTable[A00])<<(fracBits-3); //c4*c4=.5  , dct 0
		*qt_entry++ = -(tempTable[A40])<<(fracBits-3); //c4*c4=.5	//dct 32
		*qt_entry++ = -MultCCDiv4(tempTable[A20],c2c4);				//dct 16
		*qt_entry++ = -MultCCDiv4(tempTable[A60],c6c4);				//dct 48
		*qt_entry++ = -MultCCDiv4(tempTable[A50],c5c4);				//dct 40
		*qt_entry++ = -MultCCDiv4(tempTable[A10],c1c4);				//dct 8
		*qt_entry++ =  MultCCDiv4(tempTable[A70],c7c4);				//dct 56
		*qt_entry++ = -MultCCDiv4(tempTable[A30],c3c4);				//dct 24
//4
		*qt_entry++ = -(tempTable[A04])<<(fracBits-3); //c4*c4=.5
		*qt_entry++ =  (tempTable[A44])<<(fracBits-3); //c4*c4=.5
		*qt_entry++ =  MultCCDiv4(tempTable[A24],c2c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A64],c6c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A54],c5c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A14],c1c4);
		*qt_entry++ = -MultCCDiv4(tempTable[A74],c7c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A34],c3c4);
//2
		*qt_entry++ = -MultCCDiv4(tempTable[A02],c4c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A42],c4c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A22],c2c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A62],c6c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A52],c5c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A12],c1c2);
		*qt_entry++ = -MultCCDiv4(tempTable[A72],c7c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A32],c3c2);
//6
		*qt_entry++ = -MultCCDiv4(tempTable[A06],c4c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A46],c4c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A26],c2c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A66],c6c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A56],c5c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A16],c1c6);
		*qt_entry++ = -MultCCDiv4(tempTable[A76],c7c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A36],c3c6);
//5
		*qt_entry++ = -MultCCDiv4(tempTable[A05],c4c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A45],c4c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A25],c2c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A65],c6c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A55],c5c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A15],c1c5);
		*qt_entry++ = -MultCCDiv4(tempTable[A75],c7c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A35],c3c5);
//1
		*qt_entry++ = -MultCCDiv4(tempTable[A01],c4c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A41],c4c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A21],c2c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A61],c6c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A51],c5c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A11],c1c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A71],c7c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A31],c3c1);
//7
		*qt_entry++ =  MultCCDiv4(tempTable[A07],c4c7);
		*qt_entry++ = -MultCCDiv4(tempTable[A47],c4c7);
		*qt_entry++ = -MultCCDiv4(tempTable[A27],c2c7);
		*qt_entry++ = -MultCCDiv4(tempTable[A67],c6c7);
		*qt_entry++ = -MultCCDiv4(tempTable[A57],c5c7);
		*qt_entry++ = -MultCCDiv4(tempTable[A17],c1c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A77],c7c7);				//dct 63
		*qt_entry++ = -MultCCDiv4(tempTable[A37],c3c7);
//3
		*qt_entry++ = -MultCCDiv4(tempTable[A03],c4c3);
		*qt_entry++ =  MultCCDiv4(tempTable[A43],c4c3);
		*qt_entry++ =  MultCCDiv4(tempTable[A23],c2c3);
		*qt_entry++ =  MultCCDiv4(tempTable[A63],c6c3);
		*qt_entry++ =  MultCCDiv4(tempTable[A53],c5c3);
		*qt_entry++ =  MultCCDiv4(tempTable[A13],c1c3);
		*qt_entry++ = -MultCCDiv4(tempTable[A73],c7c3);
		*qt_entry++ =  MultCCDiv4(tempTable[A33],c3c3);
#else
//column order
		*qt_entry++ =  (tempTable[A00])<<(fracBits-3); //c4*c4=.5  , dct 0
		*qt_entry++ = -(tempTable[A04])<<(fracBits-3); //c4*c4=.5
		*qt_entry++ = -MultCCDiv4(tempTable[A02],c4c2);
		*qt_entry++ = -MultCCDiv4(tempTable[A06],c4c6);
		*qt_entry++ = -MultCCDiv4(tempTable[A05],c4c5);
		*qt_entry++ = -MultCCDiv4(tempTable[A01],c4c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A07],c4c7);
		*qt_entry++ = -MultCCDiv4(tempTable[A03],c4c3);
//4
		*qt_entry++ = -(tempTable[A40])<<(fracBits-3); //c4*c4=.5	//dct 32
		*qt_entry++ =  (tempTable[A44])<<(fracBits-3); //c4*c4=.5
		*qt_entry++ =  MultCCDiv4(tempTable[A42],c4c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A46],c4c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A45],c4c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A41],c4c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A47],c4c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A43],c4c3);
//2
		*qt_entry++ = -MultCCDiv4(tempTable[A20],c2c4);				//dct 16
		*qt_entry++ =  MultCCDiv4(tempTable[A24],c2c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A22],c2c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A26],c2c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A25],c2c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A21],c2c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A27],c2c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A23],c2c3);
//6
		*qt_entry++ = -MultCCDiv4(tempTable[A60],c6c4);				//dct 48
		*qt_entry++ =  MultCCDiv4(tempTable[A64],c6c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A62],c6c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A66],c6c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A65],c6c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A61],c6c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A67],c6c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A63],c6c3);
//5
		*qt_entry++ = -MultCCDiv4(tempTable[A50],c5c4);				//dct 40
		*qt_entry++ =  MultCCDiv4(tempTable[A54],c5c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A52],c5c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A56],c5c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A55],c5c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A51],c5c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A57],c5c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A53],c5c3);
//1
		*qt_entry++ = -MultCCDiv4(tempTable[A10],c1c4);				//dct 8
		*qt_entry++ =  MultCCDiv4(tempTable[A14],c1c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A12],c1c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A16],c1c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A15],c1c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A11],c1c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A17],c1c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A13],c1c3);
//7
		*qt_entry++ =  MultCCDiv4(tempTable[A70],c7c4);				//dct 56
		*qt_entry++ = -MultCCDiv4(tempTable[A74],c7c4);
		*qt_entry++ = -MultCCDiv4(tempTable[A72],c7c2);
		*qt_entry++ = -MultCCDiv4(tempTable[A76],c7c6);
		*qt_entry++ = -MultCCDiv4(tempTable[A75],c7c5);
		*qt_entry++ = -MultCCDiv4(tempTable[A71],c7c1);
		*qt_entry++ =  MultCCDiv4(tempTable[A77],c7c7);				//dct 63
		*qt_entry++ = -MultCCDiv4(tempTable[A73],c7c3);
//3
		*qt_entry++ = -MultCCDiv4(tempTable[A30],c3c4);				//dct 24
		*qt_entry++ =  MultCCDiv4(tempTable[A34],c3c4);
		*qt_entry++ =  MultCCDiv4(tempTable[A32],c3c2);
		*qt_entry++ =  MultCCDiv4(tempTable[A36],c3c6);
		*qt_entry++ =  MultCCDiv4(tempTable[A35],c3c5);
		*qt_entry++ =  MultCCDiv4(tempTable[A31],c3c1);
		*qt_entry++ = -MultCCDiv4(tempTable[A37],c3c7);
		*qt_entry++ =  MultCCDiv4(tempTable[A33],c3c3);
#endif
	}
}


void SJpeg::Decode()
{
	DWORD length;
	BOOL bCont;

	FreeDataArea();

	bCont=TRUE;
	m_IData->Reset();
	if (m_IData->GetData(&basePtr,&length))
	{
		endPtr=basePtr+length-1;
		curPtr=basePtr;
	}
	else return;

	int marker = readMarker();
	if (marker==SOI)
	{
		dri_RestartInterval=0;
		marker = readMarker();

		while (marker>0)
		{
			switch (marker)
			{
			case DQT :
			{
				do_DQT();
				break;
			}
			case SOF2 :
			case SOF1 :
			case SOF0 :
			{
				int i;
				length = readWord();
				length -= 2;
				for (i=0;i<=255;i++)
				{
					sof_componentSamplingFactors[i]=0;
					sof_componentQuantizationTable[i]=0;
					sof_componentOffset[i]=0; //number of data units before component in MCU
				}
				if (length>0)
				{
					sof_precision=readByte();
					length--;
				}
				if (length>1)
				{
					sof_numberOfLines = readWord();
					length-=2;
				}

				if (length>1)
				{
					sof_samplesPerLine = readWord();
					length-=2;
				}
				if (length>0)
				{
					i = sof_numberOfComponents=readByte();
					length--;
				}
				int j=0;

				sof_maxHFactor=1;
				sof_maxVFactor=1;
				sof_dataUnitCnt = 0;
				while ((length>0)&&(i>0))
				{
					BYTE component;
					BYTE factor;
					BYTE qTable;
					component = readByte();
					length--;
					sof_component[j]=component;
					if (length>0) {factor = readByte();length--;}
					else factor=0;
					if (length>0) {qTable = readByte();length--;}
					else qTable=0;
					if (sof_componentSamplingFactors[component]==0)
					{
						sof_componentSamplingFactors[component]=factor;
						sof_componentQuantizationTable[j]=qTable&3;
						sof_componentOffset[component] = (WORD)sof_dataUnitCnt;
						BYTE hFact,vFact;
						hFact=(factor>>4);
						vFact=(factor&15);
						sof_dataUnitCnt += (vFact*hFact);
						if (sof_maxHFactor<hFact) sof_maxHFactor=hFact;
						if (sof_maxVFactor<vFact) sof_maxVFactor=vFact;
					}
					j++;
					i--;
				}
				DWORD rem;
				sof_hortMCUs = uDivRem(((sof_samplesPerLine+7)>>3) + sof_maxHFactor-1,sof_maxHFactor,&rem);
				sof_mcuRowByteCnt = ((sof_hortMCUs*sof_dataUnitCnt)<<7)+4;
				break;
			}
			case DHT :
			{
				huffStruct* hs;
				DWORD size;
				DWORD code;
				DWORD shift;
				DWORD count;
				BYTE th;
				BYTE tc;

				length = readWord();
				length -= 2;
				while (length>0)
				{
					th=readByte();
					length--;
					tc = th >> 4;
					th &= 3;
					if (tc) th += 4;

					hs = dht_tables[th];
					if (!hs)
					{
						dht_tables[th] = hs = new huffStruct;
					}
					size = 16;
					if (size>length) size=length;
					length -=size;
					int i=0;
					count =0;
					code = 0;
					shift=15;
					while (size)
					{
						th = readByte();
						DWORD c1 = code+(th<<shift);
						if (c1>0x10000)
						{
							c1=0x10000;
							th=(BYTE)((c1-code)>>shift);
						}
						if ((count+th)>256) th = (BYTE)(256-count);
						if (th)
						{
							hs->huffCode[i]=(WORD)code;
							hs->huffIndex[i]=(BYTE)count;
							hs->huffLength[i]=(BYTE)(16-shift);
							i++;
							count += th;
							code = c1;
						}
						shift--;
						size--;
					}
					if (i<16)
					{
						hs->huffCode[i]=(WORD)code;
						hs->huffIndex[i]=0;
						hs->huffLength[i]=0;
						i++;
					}
					while (i<16)
					{
						hs->huffCode[i]=0x0ffff;
						hs->huffIndex[i]=0;
						hs->huffLength[i]=0;
						i++;
					}


					if (count>length) count=length;
					length -=count;
					i=0;
					while (count)
					{
						hs->huffVal[i++]= readByte();
						count--;
					}
				}
				break;
			}
			case SOS:
			{
				StartOfScan sos;
				DWORD num;

				length = readWord();
				length -= 2;
				if (length>0) { num=readByte();length--; if (num>4)num=4;}
				else num=0;
				sos.numberOfComponents=(BYTE)num;
				if (num>(length>>1)) num = (length>>1);
				length -= (num+num);
				int i=0;

				BYTE tdta;
				while (num>0)
				{
					sos.LastDCVal[i] = 0;
					sos.component[i] = readByte();
					tdta = readByte();
					sos.huffDC[i] = (tdta>>4)&3;
					sos.huffAC[i++] = (tdta&3)+4;
					num--;
				}
				if (length>0) {sos.ss = readByte(); length--;}
				if (length>0) {sos.se = readByte(); length--;}
				if (length>0)
				{
					tdta = readByte(); length--;
					sos.ah=tdta>>4;
					sos.al=tdta&15;
					if (sos.ah>sos.al) sos.bitCnt = sos.ah-sos.al;
					else sos.bitCnt=0;
					initHuff();
					sos.eobCnt = 0;
					lpMCURowStruct ptr;
					lpMCURowStruct prevLink;
					prevLink = (lpMCURowStruct)&mcuRow;
					sos.restartInterval=dri_RestartInterval;
					if (sos.numberOfComponents>1)
					{
						sos.interleavedRestartCnt=sos.restartInterval;
						sos.nonInterleavedRestartCnt=0;
					}
					else
					{
						sos.interleavedRestartCnt=0;
						sos.nonInterleavedRestartCnt=sos.restartInterval;
					}
					sos.curRowNum = 0;
					while (!IsMarker())
					{
						//get entire MCU row of data or all  of 1 component of MCU row
						ptr = prevLink->nextMCURow;
						if (ptr==NULL)
						{
							ptr = (lpMCURowStruct) new BYTE[sof_mcuRowByteCnt];
							prevLink->nextMCURow = ptr;
							mcuRowCnt++;
							int cnt = sof_mcuRowByteCnt>>2;
							DWORD* p1;
							p1 = (DWORD*)ptr;
							while (cnt--) {*p1++ = 0;}
						}
						sos.curMCURow = ptr;
						GetMCURow(&sos);
						sos.curRowNum++;
						prevLink = ptr;
					}

				}//if (length>0)

				break;
			}//case SOS:
			case DNL:
			{
				length = readWord();
				length -= 2;
				if (length>1)
				{
					sof_numberOfLines = readWord();
					length-=2;
				}
				if (length>0) MovePtr(length);
				break;
			}
			case DRI:
			{
				length = readWord();
				length -= 2;
				if (length>1)
				{
					dri_RestartInterval = readWord();
					length-=2;
				}
				if (length>0) MovePtr(length);
				break;

			}
			default:
				if (marker>=APP0)
				{
					length=readWord();
					if (length>=2) MovePtr(length-2);
				}
				else
				{
					bCont=FALSE;
				}
				break;
			}
			if (bCont) marker = readMarker();
			else marker =-1;
		}
	}

	FreeWorkArea();
}

int SJpeg::readByte()
{
	if (curPtr<=endPtr)
	{
		return *curPtr++;
	}
	else
	{
		if (curPtr==(BYTE*)-1) { curPtr = basePtr; return 255;} //marker has been split between read segments
		if (endPtr)
		{
			DWORD lastRead;
			if (m_IData->GetData(&basePtr,&lastRead))
			{
				curPtr = basePtr;
				endPtr=basePtr+lastRead-1;
				return *curPtr++;
			}
			else
			{
				endPtr=NULL;
			}
		}
	}
	return -1;
}

int SJpeg::readWord()
{
	int i,j;
	if (curPtr<endPtr)
	{
		i= (*curPtr++<<8);
		i+= *curPtr++;
	}
	else
	{
		i = readByte();
		if (i!=-1)
		{
			j = readByte();
			if (j!=-1) {i= (i<<8)+j;}
			else i=-1;
		}
	}
	return i;
}

int SJpeg::readMarker()
{
	int i = readByte();
	if (i!=255) {curPtr--;i=-1;}
	while (i==255) {i = readByte();}
	return i;
}

void SJpeg::MovePtr(int offset)
{
	if (offset)
	{
		if (curPtr==(BYTE*)-1) curPtr = basePtr-1;  //marker has been split between read segments
		DWORD lastRead = endPtr+1-curPtr;
		while (lastRead <= (DWORD)offset)
		{
			offset -= lastRead;
			if (m_IData->GetData(&basePtr,&lastRead))
			{
				curPtr = basePtr;
				endPtr=basePtr+lastRead-1;
			}
			else
			{
				endPtr=NULL;
				offset = 0;
				break;
			}
		}
		curPtr += offset;
	}
}

BYTE SJpeg::GetHuffByte(BYTE compHuff)
{
	huffStruct* hsp;
	int i;
	DWORD diff;
	WORD huffWord;

	hsp = dht_tables[compHuff];
	if (hsp)
	{
		while (bitsOverWord<0)
		{
			nextBits <<= 8;
			nextBits += readByteSkipStuffed();
			bitsOverWord+=8;
		}
		huffWord = (WORD)(nextBits >> bitsOverWord);
		i=1;
		while (huffWord>=hsp->huffCode[i]){i++;if (i>15)break;}; //huffCode[0]=0
		i--;
		diff = huffWord - hsp->huffCode[i];
		diff >>= (16-hsp->huffLength[i]);

		bitsOverWord -= hsp->huffLength[i];

		return hsp->huffVal[hsp->huffIndex[i]+diff];
	}
	return 0x0ff;

}

int SJpeg::GetBits(BYTE bitcnt)
{
	int data;
	if (bitcnt)
	{
		while (bitsOverWord<0)
		{
			nextBits <<= 8;
			nextBits += readByteSkipStuffed();
			bitsOverWord+=8;
		}
		data = (int)(WORD)(nextBits >> bitsOverWord);
		bitsOverWord-=bitcnt;
		if (data>=0x08000)
		{
			data >>= (16-bitcnt); //positive
		}
		else
		{
			data += 0x0ffff0000;
			data >>= (16-bitcnt);
			data++;				//negative, (1's complement to 2's complement)
		}
	}
	else
	{
		data = 0;
	}
	return data;
}

DWORD SJpeg::GetEOBCnt(BYTE bitcnt)
{
	DWORD data;
	if (bitcnt)
	{
		while (bitsOverWord<0)
		{
			nextBits <<= 8;
			nextBits += readByteSkipStuffed();
			bitsOverWord+=8;
		}
		data = (DWORD)(WORD)(nextBits >> bitsOverWord);
		bitsOverWord-=bitcnt;
		data += 0x010000;
		data >>= (16-bitcnt); //positive
	}
	else
	{
		data = 1;
	}
	return data;

}


DWORD SJpeg::GetUnsignedBits(BYTE bitcnt)
{
	DWORD data;
	if (bitcnt)
	{
		while (bitsOverWord<0)
		{
			nextBits <<= 8;
			nextBits += readByteSkipStuffed();
			bitsOverWord+=8;
		}
		data = (DWORD)(WORD)(nextBits >> bitsOverWord);
		bitsOverWord-=bitcnt;
		data >>= (16-bitcnt);
	}
	else
	{
		data = 0;
	}
	return data;

}
void SJpeg::initHuff()
{
	bitsOverWord=-16;
	nextBits = 0;
	markerReached1=0;
	markerReached2=0;
}

BYTE SJpeg::readByteSkipStuffed()
{
	BYTE val,val2;
	val = readByte();
	if (val==255)
	{
		val2 = readByte();
		if (val2)
		{
			curPtr -=2;
			if (curPtr<basePtr) curPtr = (BYTE*)(-1);  //marker has been split between read segments
			markerReached2=markerReached1;
			markerReached1=val2;
		}
	}
	return val;
}


BOOL SJpeg::skipRST(StartOfScan* sos)
{
	DWORD data;
	while (bitsOverWord<0)
	{
		nextBits <<= 8;
		nextBits += readByteSkipStuffed();
		bitsOverWord+=8;
	}
	if (markerReached2)
	{
		data = (0x010000<<bitsOverWord)-1;
		if ((nextBits&data)==data)
		{
			//all bits queued are 1's
			if (curPtr==(BYTE*)-1) curPtr = basePtr-1;  //marker has been split between read segments
			curPtr+=2;
			while (markerReached1==255) {markerReached1 = readByte();}
			if ((markerReached1&0x0f8)==RST0)
			{
				initHuff();
				sos->eobCnt=0;

				BYTE i=sos->numberOfComponents;
				while (i){ sos->LastDCVal[--i]=0;}
				return TRUE;

			}
			else
			{
				curPtr-=2;
				if (curPtr<basePtr) curPtr = (BYTE*)(-1);  //marker has been split between read segments
			}
		}
	}
	return FALSE;
}

BOOL SJpeg::IsMarker()
{
	DWORD data;
	while (bitsOverWord<0)
	{
		nextBits <<= 8;
		nextBits += readByteSkipStuffed();
		bitsOverWord+=8;
	}
	if (markerReached2)
	{
		data = (0x010000<<bitsOverWord)-1;
		if ((nextBits&data)==data)
		{
			return TRUE;
		}
	}
	return FALSE;
}

//*************************************************

// *****************************
ImageData* SJpeg::LoadYCbCr(IFileData* data)
{
	SImageDataJpeg* pS = NULL;
	if (data)
	{
		m_IData = data;
	}
	if (m_IData)
	{
		Decode();
		pS = new SImageDataJpeg(sof_hortMCUs,mcuRowCnt,sof_samplesPerLine,sof_numberOfLines,sof_componentSamplingFactors[sof_component[0]],sof_componentSamplingFactors[sof_component[1]],sof_componentSamplingFactors[sof_component[2]]);
		if (pS) if (!IDCT(pS->m_yCbCrSamples)) {pS->Release(); pS = NULL;}
		FreeDataArea();
	}
	return pS;
}


#define GETIDCT GetIDCT_ARM_JQ

extern "C"
{
	void __stdcall GETIDCT(short *srcDCT, BYTE *dest, DWORD rowAdvance, const int* qTable);
}

BOOL SJpeg::IDCT(BYTE** ppSamples)
{
	BOOL bGood = FALSE;
	if (sof_samplesPerLine && sof_numberOfLines && mcuRow)
	{
		DWORD offset = 0;
		bGood=ConvertDCTComponent(0,&offset,ppSamples);
		if (bGood)
		{
			if (sof_numberOfComponents>=3)
			{
				bGood = ConvertDCTComponent(1,&offset,ppSamples);
				if (bGood) bGood = ConvertDCTComponent(2,&offset,ppSamples);
			}
		}
	}
	return bGood;
}

//extern void GETIDCT(short *srcDCT, BYTE *dest, DWORD rowAdvance, const int* qTable);
BOOL SJpeg::ConvertDCTComponent(DWORD index,DWORD* pSrcOffset,BYTE** ppSamples)
{
	const int	factor = sof_componentSamplingFactors[sof_component[index]];
	const BYTE	hFact = factor>>4;
	const BYTE	vFact = factor&15;

	const DWORD	hortMCUs = sof_hortMCUs;
	const DWORD	sampleColumns = (hFact*hortMCUs<<3);
	const DWORD	sampleRows = (vFact*mcuRowCnt<<3);
	const int	*qTable = dqt_tables[sof_componentQuantizationTable[index]];

	const DWORD	sampleRowAdvance = sampleColumns;
	const DWORD	srcOffset = *pSrcOffset;
	const DWORD	vertDCTAdvance = (hFact<<6);
	const DWORD	mcuAdvance = (sof_dataUnitCnt-hFact)<<6;

	lpMCURowStruct curMCURow = mcuRow;
	if (!curMCURow) return FALSE;
	short	*verticalDCTPosition = &curMCURow->data[srcOffset];
	DWORD	vCnt = vFact;
	BYTE* sampleData = new BYTE[sampleColumns*sampleRows];
	if (!sampleData) return FALSE;

	*(ppSamples+index)  = sampleData;


	while (curMCURow)
	{
		BYTE *destSamples = sampleData;
		short *srcDCT = verticalDCTPosition;
		DWORD mcusLeft = hortMCUs;
		while (mcusLeft--)
		{
			BYTE dctsLeft = hFact;
			while (dctsLeft--)
			{
				GETIDCT(srcDCT,destSamples,sampleRowAdvance,qTable);
				srcDCT += 64;
				destSamples+= 8;
			}
			srcDCT += mcuAdvance;
		}
		vCnt--;
		if (vCnt)
		{
			verticalDCTPosition +=  vertDCTAdvance;
		}
		else
		{
			vCnt = vFact;
			curMCURow = curMCURow->nextMCURow;
			if (curMCURow) {verticalDCTPosition = &curMCURow->data[srcOffset];}
		}
		sampleData += sampleColumns<<3;		//just did 8 rows
	}
	*pSrcOffset += (vFact*hFact)<<6;
	return TRUE;
}


/*
BOOL SJpeg::Load(HDC hdc,IFileData* data,DWORD drawWidth,DWORD drawHeight)
{
	if (LoadYCbCr(data))
	{
		if (!drawWidth) drawWidth = sof_samplesPerLine;
		if (!drawHeight) drawHeight = sof_numberOfLines;
		FreeBitmap();
		HBITMAP hbm;
		hbm = ::CreateCompatibleBitmap(hdc,drawWidth,drawHeight);

		if (hbm)
		{
			BYTE *pDib = NULL;
			if (pDib=GetDibBits(drawWidth,drawHeight,0,0,drawWidth,drawHeight,
									0,0,sof_samplesPerLine,sof_numberOfLines))
			{
				DWORD rowWidth = (((drawWidth*3)+3)& ~3);
				BITMAPINFO info;                       // pointer to DIB bitmap
				::ZeroMemory( &info, sizeof(BITMAPINFOHEADER) );
				info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
				info.bmiHeader.biWidth = drawWidth;
				info.bmiHeader.biHeight = - ((int)drawHeight);
				info.bmiHeader.biPlanes = 1;
				info.bmiHeader.biBitCount = 24;
				info.bmiHeader.biCompression = BI_RGB;
				info.bmiHeader.biSizeImage = rowWidth*drawHeight;
				::SetDIBits(hdc, hbm, 0, drawHeight, pDib, &info,DIB_RGB_COLORS);
				delete[] pDib;
				m_bitmap = hbm;
				m_drawWidth = drawWidth;
				m_drawHeight = drawHeight;
			}
			else
			{
				::DeleteObject(hbm);
			}
		}
	}

	if (m_bitmap) return TRUE;
	return FALSE;

}
*/
