//comment next line to remove from program
// SJpeg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// SJpeg window
typedef struct taghuffStruct
{
	WORD huffCode[16];	//lowest code which starts length
	BYTE huffLength[16];
	BYTE huffIndex[16]; //index into huffval of lowest code of bitsize.
	BYTE huffVal[256];
} huffStruct;
typedef struct tagMCURowStruct
{
	struct tagMCURowStruct *nextMCURow;
	short data[1];
} MCURowStruct,*lpMCURowStruct;

typedef struct
{
	DWORD eobCnt;
	lpMCURowStruct curMCURow;
	WORD LastDCVal[4];
	DWORD nonInterleavedRestartCnt;
	DWORD interleavedRestartCnt;
	DWORD curRowNum;
//values below don't change once set
	DWORD restartInterval;
	BYTE component[4];
	BYTE huffDC[4];
	BYTE huffAC[4];
	BYTE ss;
	BYTE se;
	BYTE ah;
	BYTE al;

	BYTE numberOfComponents;
	BYTE bitCnt;
} StartOfScan;


class SJpeg
{
// Construction
public:
	SJpeg();
	~SJpeg();

//	BOOL Load(HDC hdc,IFileData* data,DWORD drawWidth=0,DWORD drawHeight=0);
	ImageData* LoadYCbCr(IFileData* data);


// Attributes
private:
	enum Markers
	{
		TEM=0x01,		//0x02-0x0bf reserved

		SOF0=0x0c0,	//baseline DCT
		SOF1=0x0c1,	//Extended sequential DCT
		SOF2=0x0c2,	//Progressive DCT
		SOF3=0x0c3,	//Lossless (sequential)

		DHT=0x0c4,	//define Huffman tables

		SOF5=0x0c5,	//Huffman Differential sequential DCT
		SOF6=0x0c6,	//Huffman Differential progressive DCT
		SOF7=0x0c7,	//Huffman Differential lossless

		JPG=0x0c8,	//reserved for JPEG extension

		SOF9=0x0c9,	//arithmetic sequential DCT
		SOF10=0x0ca,	//arithmetic progressive DCT
		SOF11=0x0cb,	//arithmetic lossless(sequential)

		DAC=0x0cc,	//define arithmetic conditioning

		SOF13=0x0cd,	//arithmetic Differential sequential DCT
		SOF14=0x0ce,	//arithmetic Differential progressive DCT
		SOF15=0x0cf,	//arithmetic Differential lossless

		RST0=0x0d0,	//-0x0d7 restart modulo 8 counter
		SOI=0x0d8,	//Start of image
		EOI=0x0d9,	//End of image
		SOS=0x0da,	//Start of scan
		DQT=0x0db,	//define quantization tables
		DNL=0x0dc,	//define number of lines
		DRI=0x0dd,	//define restart interval
		DHP=0x0de,	//define hierarchial progression
		EXP=0x0df,	//expand reference image
		APP0=0x0e0,	//-0x0ef
		JPG0=0x0f0, //-0x0fd, reserved for JPEG extension
		COM=0x0fe
	};

// Operations
public:

// Implementation

private:
	void FreeWorkArea();
	void FreeDataArea();
	void Decode();
	BOOL IDCT(BYTE** ppSamples);
	BOOL ConvertDCTComponent(DWORD index,DWORD* pSrcOffset,BYTE** ppSamples);
	BOOL IsMarker();
	void initHuff();
	BOOL skipRST(StartOfScan* sos);
	void GetMCURow(StartOfScan* sos);
	DWORD GetEOBCnt(BYTE bitcnt);
	DWORD GetUnsignedBits(BYTE bitcnt);
	int GetBits(BYTE bitcnt);
	BYTE readByteSkipStuffed();
	BYTE GetHuffByte(BYTE compHuff);

	void do_DQT();
	int readWord();
	int readByte();
	int readMarker();
	void MovePtr(int offset);


	
	int* dqt_tables[16];

	huffStruct* dht_tables[8]; //0-3 DC or lossless, 4-7 AC
	IFileData* m_IData;
	lpMCURowStruct mcuRow;//ptr to next row, short[64]*sof_dataUnitCnt*sof_hortMCUs
	DWORD mcuRowCnt;

protected:
	DWORD sof_numberOfLines;
	DWORD sof_samplesPerLine;
	BYTE sof_precision;
	BYTE sof_numberOfComponents;
	BYTE sof_maxHFactor;
	BYTE sof_maxVFactor;
	DWORD sof_hortMCUs;
	BYTE sof_component[256];
	BYTE sof_componentSamplingFactors[256];
private:

	DWORD sof_dataUnitCnt;
	DWORD sof_mcuRowByteCnt;
	BYTE sof_componentQuantizationTable[256];
	WORD sof_componentOffset[256]; //number of data units before component in MCU
	DWORD dri_RestartInterval;
	const BYTE *basePtr,*curPtr,*endPtr;
	DWORD nextBits;
	int bitsOverWord;
	BYTE markerReached1,markerReached2;
	BYTE spare1,spare2;
////protected:

};
