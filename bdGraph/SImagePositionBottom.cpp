#include "ICommon.h"
#include "SImagePosition.h"
#include "SImagePositionBottom.h"


SImagePositionBottom::SImagePositionBottom(ImageData* imageData)
 : SImagePosition(imageData), m_zeros(NULL), m_state(0)
{
}
SImagePositionBottom::~SImagePositionBottom()
{
	if (m_zeros) delete[] m_zeros;
}

BOOL SImagePositionBottom::Reset(int colorIndex)
{
	m_state = 0;
	if (m_imageData)
	{
		int width,height;
		if (GetDimensions(&width,&height))
		{
			m_width = width;
			if (m_zeros) delete[] m_zeros;
			if (width)
			{
				BYTE* p = m_zeros = new BYTE[width];
				if (p)
				{
					do {*p++ = 0;} while (--width);
					return SImagePosition::Reset(colorIndex);
				}
			}
		}
	}
	return FALSE;
}

#define BOTVAL (480+31)
#define TOPCNT 1
//BOTVAL zero rows, followed by
//topRow, followed by
//TOPCNT topRows again, repeating the very first row, followed by
//bottom rows
void SImagePositionBottom::GetRow()
{
	switch (m_state)
	{
		case -1 :
			SImagePosition::GetRow();
			break;
		case 0:
			m_state = 1;
			SImagePosition::GetRow();
			bsampleRowPos   = sampleRowPos;
			bsampleRowsLeft = sampleRowsLeft;
			bsampleRepeat   = sampleRepeat;
			browAdvance     = rowAdvance;

			sampleRowPos = m_zeros;		//return zeros
			if (sampleRowsLeft) sampleRowsLeft =1;
			sampleRepeat = BOTVAL-1;
			rowAdvance = m_width;
			break;
		case 1:
			m_state = 2;
			sampleRowPos   = bsampleRowPos;	//return 1st line
			sampleRowsLeft = (bsampleRowsLeft) ? 1 : 0;
			sampleRepeat   = 0;
			rowAdvance     = browAdvance;
			m_linesSkipped = 0;
			break;
		case 4:
			m_state = 2;
			SImagePosition::GetRow();
			bsampleRowPos   = sampleRowPos;
			bsampleRowsLeft = sampleRowsLeft;
			bsampleRepeat   = sampleRepeat;
			browAdvance     = rowAdvance;

		case 2:
			{
				sampleRowPos   = bsampleRowPos;	//return 1st line
				sampleRowsLeft = bsampleRowsLeft;
				sampleRepeat   = bsampleRepeat;
				rowAdvance     = browAdvance;

				DWORD n = sampleRowsLeft + sampleRepeat;
				n += m_linesSkipped;
				if (n<TOPCNT)
				{
					m_linesSkipped = n;	//return all the lines
					m_state = 4;
					break;
				}
				else
				{
					m_state = 3;
					n-=TOPCNT;		//n has # of lines too many to return
					m_linesSkipped = n;
					if (sampleRepeat >= n)
					{
						sampleRepeat -= n;
						break;
					}
					else
					{
						n -= sampleRepeat;
						sampleRepeat = 0;
						sampleRowsLeft -= n;
						if (sampleRowsLeft != 0) break;
					}
				}
			}
		case 3:
			m_state=-1;
			int n = m_linesSkipped-(BOTVAL+1);	//# of lines too many skipped
											//when need to recover the last n lines
			while ((n<=0) && sampleRowsLeft)
			{
				SImagePosition::GetRow();
				n+=sampleRowsLeft + sampleRepeat;
			}
			if (n>0)
			{
				if (((DWORD)n)<=sampleRepeat)
				{
					//all data in repeat line
					sampleRowPos += rowAdvance*(sampleRowsLeft-1);
					sampleRowsLeft = 1;
					sampleRepeat = n-1;
				}
				else
				{
					n -= sampleRepeat;
					sampleRowPos += rowAdvance*(sampleRowsLeft-n);
					sampleRowsLeft = n;
				}
			}
	}
}
