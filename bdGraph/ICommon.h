#ifndef interface
#define interface struct
#endif

#define NOASM

typedef unsigned char BYTE;
typedef int BOOL;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef unsigned long ULONG;
//typedef unsigned __int64 ULONGLONG;
typedef unsigned long long ULONGLONG;
typedef long LONG;
#define __stdcall

#ifndef NULL
//#define NULL ((void *)0)
#define NULL 0
#endif

#define TRUE 1
#define FALSE 0

#define __fastcall
#define uint64_t ULONGLONG


class SRFact
{
public:
	BYTE	hFact;
	BYTE	vFact;
	BYTE	hMax;
	BYTE	vMax;
};

interface ImagePosition
{
	virtual void Release() =0;
	virtual void AddRef() =0;
	virtual void GetRow() =0;
	virtual BOOL Reset(int colorIndex) =0;
	virtual void Close() =0;
	virtual BOOL GetDimensions(int* pWidth,int* pHeight) =0;
	virtual void Advance(DWORD picLeft,DWORD picTop) = 0;
	virtual const BYTE* GetNextRow() = 0;
	virtual void AdvanceRows(DWORD n) = 0;
	virtual void AdvanceWithinRow(DWORD n) = 0;
	virtual const SRFact* GetSRFact() = 0;
	virtual void SetPosition(const BYTE* sampleRowPos,DWORD sampleRowsLeft,DWORD sampleRepeat,DWORD rowAdvance) = 0;
};

interface ImageData
{
	virtual void Release() =0;
	virtual void AddRef() =0;
	virtual int GetRow(ImagePosition* pSample,int colorIndex,int parm) =0;
	virtual int Reset(SRFact* pFact,int colorIndex) =0;
	virtual void Close() =0;
	virtual BOOL GetDimensions(int* pWidth,int* pHeight) =0;
	virtual ImagePosition* GetImagePosition() =0;
};


interface IFileData
{
	virtual BOOL GetData(const BYTE** ppBuffer,DWORD* bytesRead,DWORD maxNeeded=0) =0;
	virtual void Reset() =0;
	virtual void ResetBounds(int backup,int length) =0;
	virtual void Advance() =0;
};

class CJpegRegion;
class CBEntry
{
public:
	CJpegRegion* m_srcImage;
	BYTE m_bmState;
	BYTE m_bmLastState;
	BYTE m_bmType;
	BYTE spare1;

	WORD left;
	WORD top;
	WORD right;
	WORD bottom;
	WORD srcLeft;
	WORD srcTop;
	WORD srcRight;
	WORD srcBottom;
	BYTE m_command;
	BYTE m_scaleIndex;
	BYTE m_stringIndex[2];
};
