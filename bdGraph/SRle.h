// SRle.h : header file
//
//comment next line to remove from program
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif
interface IFileData;

class SRle
{
// Construction
public:
	SRle();
	~SRle();

	ImageData* LoadYCbCr(IFileData* data);
private:
	ImageData* Decode();
	void DecodeRow(BYTE* pBlue,BYTE* pGreen,BYTE* pRed,DWORD* dA);
	void initHuff();
	int GetVariableBits(BOOL &bMustBeGtr0,int firstPower,BYTE bitcnt);
	DWORD GetUnsignedBits(BYTE bitcnt);
	int GetHuffColor();
	int readWord();
	int readByte();
	DWORD ReadVal(DWORD max);

	DWORD* ReadMap(DWORD &full, DWORD& compressed, DWORD& minEntries);
	void ReadColorTable();

	DWORD m_huffCode[32];	//lowest code which starts length
	DWORD m_huffIndex[32]; //index into huffval of lowest code of bitsize.
	DWORD m_huffSpecial[32]; //# of special codes at beginning of group
	BYTE m_huffLength[32];
	BYTE* m_pColorTbl;
	BYTE** m_ppBlueRows;
	BYTE** m_ppGreenRows;
	BYTE** m_ppRedRows;
	int m_rowInc;
	DWORD m_width;
	DWORD m_height;
	int m_rowCnt;

	IFileData* m_IData;
	const BYTE* m_curPtr;
	const BYTE* m_endPtr;
	int	 m_bitsOverDword;
	ULONGLONG m_nextBitsBuffer;
};
