// scaleUpObj.h : header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef interface
#define interface struct
#endif

interface ImagePosition;

interface IScaleHortObj
{
	virtual BOOL	ScaleRow(DWORD* destScaledRow) =0;
};


class ScaleHortObj : public IScaleHortObj
{
protected:
	ScaleHortObj();
	~ScaleHortObj();
public:
	static ScaleHortObj* GetObject(int hUpCode,DWORD hScaleFrom,DWORD hScaleTo,DWORD* phDiv);
	void Release();
	virtual BOOL ScaleRow(DWORD* destScaledRow);
	BOOL Init(ImagePosition* pSample,DWORD rowWidth,DWORD outWidth,DWORD hScaleFrom,DWORD hScaleTo,int initialWeight);
protected:
	ImagePosition* m_pSample;

	DWORD	m_rowWidth;
	DWORD	m_outWidth;
	DWORD	m_hScaleFrom;
	DWORD	m_hScaleTo;
	int		m_initialWeight;

};

