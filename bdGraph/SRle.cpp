#include "ICommon.h"
#include "SRle.h"
#include "SImageDataRLE.h"

extern "C" unsigned long uDivRem(unsigned long value,unsigned long divisor,unsigned long * rem);
extern "C" unsigned long uShiftRight(ULONGLONG* m_nextBitsBuffer,int shift);

//#undef ZeroMemory
//#define ZeroMemory(dest,count)  ZeroMem(count,dest)
//void __fastcall ZeroMem(int count,void* dest);
//BOOL CmpBytes(const void* dest,const void* src,int count);

#include <string.h>
#define ZeroMemory(dest,count) memset(dest,0,count)
#define CmpBytes(s1,s2,count) !memcmp(s1,s2,count)
static BYTE* CopyBytes(void* dest,const void* src,int count)
{
	memcpy(dest,src,count);
	return ((BYTE*)dest) + count;
}

/////////////////////////////////////////////////////////////////////////////

void SRle::DecodeRow(BYTE* pBlue,BYTE* pGreen,BYTE* pRed,DWORD* dA)
{
	BYTE* pBlueStart = pBlue;
	int widthLeft = m_width;
	int lastWidth=widthLeft-1;
	int firstCnt=lastWidth+m_height;	//horizontalRepeat
	int vertRepeat=0;
	int hortRepeat=0;
	int nextVertRepeat=0;

	BYTE* pb=NULL;
	BYTE* pg;
	BYTE* pr;
	int rgb;
	BYTE blue;
	BYTE green;
	BYTE red;
	do
	{
		while (*dA)
		{
			(*dA)--;
			dA++;
			int rgb = *dA++;
			if (rgb>=0)
			{
				*pBlue++ = (BYTE)rgb; rgb>>=8;
				*pGreen++ = (BYTE)rgb; rgb>>=8;
				*pRed++ = (BYTE)rgb;
			}
			else
			{
				int offset = pBlue-pBlueStart;
				BYTE* pb;
				BYTE* pg;
				BYTE* pr;
				if ((-rgb)>lastWidth)
				{
					rgb+=lastWidth;
					pb = (m_ppBlueRows[rgb]+offset);
					pg = (m_ppGreenRows[rgb]+offset);
					pr = (m_ppRedRows[rgb]+offset);
				}
				else
				{
					pb = pBlue+rgb;
					pg = pGreen+rgb;
					pr = pRed+rgb;
				}
				*pBlue++ = *pb;
				*pGreen++ = *pg;
				*pRed++ = *pr;
			}
			widthLeft--;
			if (!widthLeft) goto done1;
			if (pb) {pb++; pg++; pr++;}	//this source position is skipped
		}
		if (!hortRepeat)
		{
			vertRepeat=nextVertRepeat;
			nextVertRepeat=0;
			hortRepeat=1;
samecase:
			rgb = GetHuffColor();
			if (rgb>=0)
			{
try3:
				blue = (BYTE)rgb; 
				green = (BYTE)(rgb>>8);
				red = (BYTE)(rgb>>16);
				pb=NULL;
			}
			else
			{
				if ((-rgb)<firstCnt)
				{
c1:
					int offset = pBlue-pBlueStart;
					int distance;
					if ((-rgb)>lastWidth)
					{
						distance = rgb+lastWidth;
						if ((-distance)>m_rowCnt)
						{
							distance=-m_rowCnt;
							rgb = distance-lastWidth;
							if (!distance) {rgb=-1;goto try1;}
						}
try2:
						pb = (m_ppBlueRows[distance]+offset);
						pg = (m_ppGreenRows[distance]+offset);
						pr = (m_ppRedRows[distance]+offset);
					}
					else
					{
try1:
						if ((-rgb)>offset)
						{
							rgb=-offset;
							if (!rgb)
							{
								if (m_rowCnt) { rgb=-1-lastWidth; distance=-1; goto try2;}
								goto try3;
							}
						}
						pb = pBlue+rgb;
						pg = pGreen+rgb;
						pr = pRed+rgb;
					}
				}
				else
				{
					DWORD cnt= (-rgb)-firstCnt+2;
					if (cnt>m_width)
					{
c2:
						vertRepeat = cnt - m_width-1;	//vertical repeat count
						rgb = GetHuffColor();
						if (rgb>=0) goto try3;
						if ((-rgb)<firstCnt) goto c1;
						cnt= (-rgb)-firstCnt+2;
						if (cnt>m_width) goto c2;
						nextVertRepeat=vertRepeat;
					}
					hortRepeat=cnt;
					if (hortRepeat>widthLeft) hortRepeat=widthLeft;
					goto samecase;
				}
			}
		}
		*dA++ = vertRepeat;
		*dA++ = rgb;
		if (pb)
		{
			*pBlue++ = *pb++;
			*pGreen++ = *pg++;
			*pRed++ = *pr++;
		}
		else
		{
			*pBlue++ = blue;
			*pGreen++ = green;
			*pRed++ = red;
		}
		hortRepeat--;
	} while (--widthLeft);
done1:
	int rowInc = m_rowInc;
	if (rowInc)
	{
		blue = *(pBlue-1);
		green = *(pGreen-1);
		red = *(pRed-1);
		do
		{
			*pBlue++ = blue;
			*pGreen++ = green;
			*pRed++ = red;
		} while (--rowInc);
	}
}



void AddRep(DWORD* &pCnt,BYTE* pCur,int count,int width)
{
	BYTE* last = ((BYTE*)pCnt[0]) + (width)*(pCnt[1]-1);
	if (last==pCur) {pCnt[2]+=count;}
	else
	{
		last+=width;
		if ((!pCnt[2])&&(last==pCur))
		{
			pCnt[1]++; pCnt[2]=(count-1);
		}
		else
		{
			pCnt-=3;
			pCnt[0]=(DWORD)pCur;
			pCnt[1]=1;
			pCnt[2]=count-1;
		}
	}
}

//moving data
void AddDataPS(DWORD* &pCnt,BYTE* &p,BYTE** &ppRows,int rowWidth,int compressedWidth)
{
	BYTE* pPrev=(BYTE*)pCnt[0];
	if (pPrev)
	{
		pPrev+=((pCnt[1]-1)*rowWidth);
		if (CmpBytes(p,pPrev,compressedWidth))
		{
			//match
			(pCnt[2])++;
			*ppRows++ = pPrev;
			return;
		}
		else
		{
			//non-match
			pPrev+=rowWidth;
			if ((!pCnt[2])&&(pPrev==p))
			{
				(pCnt[1])++;
				*ppRows++ = pPrev;
				p+=rowWidth;
				return;
			}
			else
			{
				pCnt-=3;
			}
		}
	}
	*ppRows++ = p;
	pCnt[0]=(DWORD)p;
	pCnt[1]=1;
	pCnt[2]=0;
	p+=rowWidth;
}

int min1(int a, int b)
{
	if (a<b) return a;
	return b;
}
DWORD SRle::ReadVal(DWORD max)
{
	DWORD val=0;

	switch(max)
	{
	case 2:
			val =GetUnsignedBits(1);
			if (val)
			{
				val=GetUnsignedBits(1)+1;
			}
			break;
	case 1:
			val =GetUnsignedBits(1);
	case 0: break;
	case 3:
			val =GetUnsignedBits(2);
			break;
	default:
			int powerBitCount=2;
			int firstPower=0;
			int ss = 0x7;
			if (max>14)
			{
				if (max>254) {ss = 0x7fff; powerBitCount=4;}
				else {ss = 0x7f; powerBitCount=3;}
			}
			while ( (max<=(DWORD)(ss+firstPower)) && ss) {firstPower++;ss>>=1;}
			val =GetUnsignedBits(powerBitCount);
			int cnt=val-firstPower;
			if (cnt>0)
			{
				val = ((1<<cnt)+(firstPower-1))+GetUnsignedBits(cnt);
			}
	}
	if (val>max)
	{
#ifdef _DEBUG
		::MessageBox(NULL,"error","val>max",MB_OK);
#endif
		val=max;
	}
	return val;
}

DWORD* SRle::ReadMap(DWORD &full, DWORD& compressed, DWORD& minEntries)
{
	DWORD* pMap=NULL;
	full = ReadVal(0x10000);
	DWORD maxNewEntryCnt = ReadVal(full-1);
	compressed=0;
	minEntries=0;
	if (maxNewEntryCnt)
	{
		DWORD maxPrevEntryCnt = ReadVal(full-1-maxNewEntryCnt);
		pMap = new DWORD[(full+2-maxNewEntryCnt-maxPrevEntryCnt)<<1];
		DWORD* pM = pMap;
		int prev=-1;
		int t = full-1;
		DWORD same = ~0;
		DWORD type=0;
		DWORD prevType;
		goto entry1;
		while (t>=0)
		{
			prevType=type;
			type = ReadVal(prev);
			if (type>=prevType)type++;
			int cnt;
			if (type)
			{
				cnt = ReadVal(min1(t,maxPrevEntryCnt))+1;
				if (same!=type)
				{
					same=type;
					minEntries++;
				}
			}
			else
			{
entry1:
				minEntries++;
				same=1;
				
				cnt = ReadVal(min1(t,maxNewEntryCnt));
				if (!cnt) cnt = t+1;
				compressed+=cnt;
				prev+=cnt;
			}
			*pM++ = type;
			*pM++ = cnt;
			t-=cnt;
		}
		*pM++ = 0;
		*pM++ = 0;
	}
	else
	{
		pMap = new DWORD[(1+1)<<1];
		pMap[0]=0;
		pMap[1]=full;
		pMap[2]=0;
		pMap[3]=0;
		minEntries++;
		compressed=full;
	}
	return pMap;
}

void SRle::ReadColorTable()
{
	DWORD code = 0;
	DWORD shift = 32;
	DWORD count = 0;
	m_huffSpecial[0] = 0;
	int i=0;
	int max=1;
	while (max)
	{
		if (i>=32) return;
		DWORD cnt= ReadVal(max);
		m_huffSpecial[i] = ReadVal(cnt);
		if (cnt)
		{
			m_huffCode[i]=code;
			m_huffIndex[i]=count;
			BYTE len = (BYTE)(32-shift);
			m_huffLength[i]=len;
			i++;
			count += cnt;
			code = code+(cnt<<shift);
			max-=cnt;
		}
		max<<=1;
		shift--;
	}
	while (i<32)
	{
		m_huffCode[i]=0x0ffffffff;
		m_huffIndex[i]=0;
		m_huffLength[i]=0;
		m_huffSpecial[i] = 0;
		i++;
	}
	i = m_bitsOverDword&7;
	GetUnsignedBits(i);		//align on byte boundary

	int jj=count+count+count;
	m_pColorTbl = new BYTE[jj];
	if (m_pColorTbl)
	{
		BYTE* dest = m_pColorTbl;
		if (jj)
		{
			int i = m_bitsOverDword+32;
			if (i)
			{
				i>>=3;
				BYTE* pnb=((BYTE*)&m_nextBitsBuffer)+i;
				if (i>jj) i=jj;
				jj-=i;
				do
				{
					*dest++ = (*--pnb);
					m_bitsOverDword-=8;
				} while (--i);
			}
			while (jj)
			{
				int cc = m_endPtr+1-m_curPtr;
				if (cc>jj) cc=jj;
				dest = CopyBytes(dest,m_curPtr,cc);
				m_curPtr+= cc;
				jj-=cc;
				if (jj)
				{
					*dest++ = readByte();
					jj--;
				}
			}
		}
	}
}

void ExpandCols(BYTE* pData,DWORD width,DWORD compressedWidth,int rowCnt,DWORD* columnMap)
{
	while (rowCnt--)
	{
		BYTE* pSrc=pData+compressedWidth;
		BYTE* pDest=pData+width;
		DWORD* pColMap = columnMap;
		while (pDest!=pSrc)
		{
			DWORD cnt = *(--pColMap);
			DWORD type = *(--pColMap);
			if (type)
			{
				BYTE val = *(pSrc-type);
				while (cnt--) {*(--pDest) = val;}
			}
			else
			{
				while (cnt--) {*(--pDest) = *(--pSrc);}
			}
		}
		pData+=width;
	}
}

ImageData* SRle::Decode()
{
	SImageDataRLE* pS=NULL;

	DWORD length;

	m_IData->Reset();
	if (m_IData->GetData(&m_curPtr,&length))
	{
		m_endPtr=m_curPtr+length-1;

		if (readWord()!=0xf5c0) return NULL;
		initHuff();
		DWORD width;
		DWORD height;
		DWORD compressedWidth;
		DWORD compressedHeight;
		DWORD minEntriesWidth;
		DWORD minEntriesHeight;
		DWORD* colMap = ReadMap(width,compressedWidth,minEntriesWidth);
		if (!colMap) return NULL;
		DWORD* rowMap = ReadMap(height,compressedHeight,minEntriesHeight);
		if (!rowMap) {delete[] colMap; return NULL;}


		m_width = compressedWidth;
		m_height= compressedHeight;
		m_rowInc = 0;


		BYTE* pBlue = NULL;
		BYTE* pGreen = NULL;
		BYTE* pRed = NULL;
		BYTE** ppBaseRow = NULL;

		int mapSpace= compressedHeight*width;
		int maxSize = (((minEntriesHeight+2)*3)<<2)+ mapSpace;
		if (width<24) maxSize+= (24-width)*((compressedHeight+1)>>1);
		maxSize = (maxSize+3)&(~3);

		pS = new SImageDataRLE(width,height,maxSize);
		if (!pS) return NULL;

		pBlue  = new BYTE[maxSize];
		pGreen = new BYTE[maxSize];
		pRed   = new BYTE[maxSize];
		ppBaseRow = m_ppBlueRows = new BYTE*[compressedHeight+compressedHeight+compressedHeight];
		m_ppGreenRows = m_ppBlueRows+compressedHeight;
		m_ppRedRows = m_ppGreenRows+compressedHeight;


		pS->m_yCbCrSamples[0]= pBlue;
		pS->m_yCbCrSamples[1]= pGreen;
		pS->m_yCbCrSamples[2]= pRed;
		if (pBlue&&pGreen&&pRed) ReadColorTable();
		else m_pColorTbl=NULL;

		if (!m_pColorTbl)
		{
			pS->Release();
			delete[] colMap;
			delete[] rowMap;
			return NULL;
		}


		int heightLeft = height;
		DWORD*  pBlueCnt = (DWORD*)(pBlue+maxSize-12);
		DWORD*  pGreenCnt = (DWORD*)(pGreen+maxSize-12);
		DWORD*  pRedCnt = (DWORD*)(pRed+maxSize-12);
		pBlueCnt[0] = 0;//  pBlueCnt[1] = 0;  pBlueCnt[2] = 0;
		pGreenCnt[0] = 0;// pGreenCnt[1] = 0; pGreenCnt[2] = 0;
		pRedCnt[0] = 0;//   pRedCnt[1] = 0;   pRedCnt[2] = 0;

		DWORD* pMap = rowMap;
		DWORD* depthArray = new DWORD[compressedWidth<<1];
		ZeroMemory(depthArray,compressedWidth<<3);

		m_rowCnt = 0;
		while (heightLeft>0)
		{
			while (pMap[0] && (heightLeft>0))
			{
				int type = pMap[0];
				int count = pMap[1];
				AddRep(pBlueCnt, m_ppBlueRows[-type], count,width);
				AddRep(pGreenCnt,m_ppGreenRows[-type],count,width);
				AddRep(pRedCnt,  m_ppRedRows[-type],  count,width);
				heightLeft-=count;
				pMap+=2;
			}
					
			if (!pMap[0])
			{
				int cnt = pMap[1];
				heightLeft-=cnt;
				if (heightLeft<0) cnt += heightLeft;

				while (cnt--)
				{
					DecodeRow(pBlue,pGreen,pRed,depthArray);
					AddDataPS(pBlueCnt, pBlue, m_ppBlueRows, width,compressedWidth);
					AddDataPS(pGreenCnt,pGreen,m_ppGreenRows,width,compressedWidth);
					AddDataPS(pRedCnt,  pRed,  m_ppRedRows,  width,compressedWidth);
					m_rowCnt++;
				}
				pMap+=2;
			}
		}
		delete[] depthArray;
		pBlueCnt-=3;  pBlueCnt[0]=0;  pBlueCnt[1]=0;  pBlueCnt[2]= ~0;
		pGreenCnt-=3;  pGreenCnt[0]=0;  pGreenCnt[1]=0;  pGreenCnt[2] = ~0;
		pRedCnt-=3;  pRedCnt[0]=0;  pRedCnt[1]=0;  pRedCnt[2]= ~0;

		delete[] ppBaseRow;
		delete[] m_pColorTbl;
		DWORD* p = colMap+1;
		while (*p) p+=2;
		p--;		//0,0 (last) entry of column map
		DWORD rem;
		DWORD rowCnt = uDivRem(((DWORD)(pBlue-pS->m_yCbCrSamples[0])),width,&rem);
		ExpandCols(pS->m_yCbCrSamples[0],width,compressedWidth,rowCnt,p);

		rowCnt = uDivRem(((DWORD)(pGreen-pS->m_yCbCrSamples[1])),width,&rem);
		ExpandCols(pS->m_yCbCrSamples[1],width,compressedWidth,rowCnt,p);

		rowCnt = uDivRem(((DWORD)(pRed-pS->m_yCbCrSamples[2])),width,&rem);
		ExpandCols(pS->m_yCbCrSamples[2],width,compressedWidth,rowCnt,p);
		delete[] rowMap;
		delete[] colMap;
	}
	return pS;
}

int SRle::readByte()
{
	if (m_curPtr<=m_endPtr)
	{
		return *m_curPtr++;
	}
	else
	{
		if (m_endPtr)
		{
			DWORD lastRead;
			if (m_IData->GetData(&m_curPtr,&lastRead))
			{
				m_endPtr=m_curPtr+lastRead-1;
				return *m_curPtr++;
			}
			else
			{
				m_curPtr=m_endPtr;
				m_endPtr=NULL;
			}
		}
	}
	return -1;
}

int SRle::readWord()
{
	int i;
	if (m_curPtr<m_endPtr)
	{
		i= (*m_curPtr++<<8);
		i+= *m_curPtr++;
	}
	else
	{
		i = readByte()<<8;
		i += readByte();
	}
	return i;
}

static inline int Get3Bytes(BYTE* p)
{
	return *p | (*(p+1) << 8) | (*(p+2) << 16);
}

int SRle::GetHuffColor()
{
	BYTE* p = (BYTE*)&m_nextBitsBuffer;
	while (m_bitsOverDword<0)
	{
		DWORD d = *((DWORD*)p);
		*(p+4) = (BYTE)(d>>24);
		*((DWORD*)p) = (d<<8) | readByte();	//new byte is least significant
		m_bitsOverDword+=8;
	}
#ifdef NOASM
//	DWORD huffWord=(DWORD)(m_nextBitsBuffer>>m_bitsOverDword);
	DWORD huffWord= uShiftRight(&m_nextBitsBuffer,m_bitsOverDword);
#else
	DWORD huffWord;
	BYTE shift = (BYTE)m_bitsOverDword;
	__asm
	{
		mov ebx,p
		mov	eax,[ebx]
		mov	bl,[ebx+4]
		mov	cl,shift
		shrd eax,ebx,cl
		mov	huffWord,eax
	}
#endif
	int i=1;
	while (huffWord>=m_huffCode[i]){i++;if (i>31)break;}; //huffCode[0]=0
	i--;
	DWORD diff = huffWord - m_huffCode[i];
	int len = m_huffLength[i];
	diff >>= (32-len);

	m_bitsOverDword -= len;
	int index = diff + m_huffIndex[i];
	index += (index<<1);//3 bytes per entry
	index = Get3Bytes(m_pColorTbl+index);
	if (diff<m_huffSpecial[i])
	{
//0                           - color follows immediately
//1:(width-1)                 - prior color this step count back on same row
//width:(height+width-2)  - prior color this step count up on previous row
		if (index==0)
		{
			index= GetUnsignedBits(24);
		}
		else if (index==1)
		{
			index= ~GetUnsignedBits(16);
		}
		else index=-(index-1);
//		else index=0x00ffffff;
	}
	return index;
}


DWORD SRle::GetUnsignedBits(BYTE bitcnt)
{
	DWORD data;
	if (bitcnt)
	{
		BYTE* p = (BYTE*)&m_nextBitsBuffer;
		while (m_bitsOverDword<0)
		{
			DWORD d = *((DWORD*)p);
			*(p+4) = (BYTE)(d>>24);
			*((DWORD*)p) = (d<<8) | readByte();	//new byte is least significant
			m_bitsOverDword+=8;
		}
#ifdef NOASM
//		data = (DWORD)(m_nextBitsBuffer>>m_bitsOverDword);
		data = uShiftRight(&m_nextBitsBuffer,m_bitsOverDword);
#else
		BYTE shift = (BYTE)m_bitsOverDword;
		__asm
		{
			mov ebx,p
			mov	eax,[ebx]
			mov	bl,[ebx+4]
			mov	cl,shift
			shrd eax,ebx,cl
			mov	data,eax
		}
#endif
		m_bitsOverDword-=bitcnt;
		data >>= (32-bitcnt); //positive
	}
	else
	{
		data = 0;
	}
	return data;
}



void SRle::initHuff()
{
	m_bitsOverDword=-32;
	m_nextBitsBuffer=0;
}


//*************************************************

SRle::SRle()
{
	m_IData = NULL;
}


SRle::~SRle()
{
}

ImageData* SRle::LoadYCbCr(IFileData* data)
{
	ImageData* pS = NULL;
	if (data)
	{
		m_IData = data;
		pS = Decode();
	}
	return pS;
}
