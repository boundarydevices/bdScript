// scaleUpObj.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif


interface IScaleHortObj;

interface IScaleVertObj
{
	virtual DWORD*	ScaleRow(IScaleHortObj* pHortObj) =0;
	virtual BOOL	Init(DWORD outWidth,int initialWeight) =0;
	virtual void	Release() =0;
};


class ScaleVertObj : public IScaleVertObj
{
protected:
	ScaleVertObj(DWORD vScaleFrom,DWORD vScaleTo);
	~ScaleVertObj();
public:
	static IScaleVertObj* GetObject(int vUpCode,DWORD vScaleFrom,DWORD vScaleTo,DWORD* phDiv);
	DWORD*			ScaleRow(IScaleHortObj* pHortObj);
	BOOL			Init(DWORD outWidth,int initialWeight);
	void			Release();
private:
	DWORD	m_max2;
	DWORD	m_low2;
	int		m_weightA;
	DWORD	*m_buffer;
	DWORD	*m_ScaledRow1;
	DWORD	*m_ScaledRow2;
	DWORD	*m_ScaledRow1Start;
	DWORD	*m_ScaledRow2Start;
	DWORD	m_width;
};

