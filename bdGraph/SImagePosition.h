class SImagePosition : public ImagePosition
{
private:
	DWORD	m_cRef;
protected:
	ImageData* m_imageData;
//////////////////
	const BYTE*	sampleRowPos;
	DWORD	sampleRowsLeft;
	DWORD	sampleRepeat;
	DWORD	rowAdvance;
//above are outputs from GetRow
//////////////////
private:
	DWORD	m_withinRow;
	SRFact   m_f;
//////////////////
public:
	SImagePosition(ImageData* imageData);
	virtual ~SImagePosition();
protected:
private:
//////////////////

	void Release();
	void AddRef();
protected:
	virtual void GetRow();
	virtual BOOL Reset(int colorIndex);
private:
	void Close();
protected:
	BOOL GetDimensions(int* pWidth,int* pHeight);
private:
///////////////////
	void Advance(DWORD picLeft,DWORD picTop);
	const BYTE* GetNextRow();
	void AdvanceRows(DWORD n);
	void AdvanceWithinRow(DWORD n);
	const SRFact* GetSRFact();
	void SetPosition(const BYTE* sampleRowPos,DWORD sampleRowsLeft,DWORD sampleRepeat,DWORD rowAdvance);

//below are state info
public:
	int	m_parm;
	int	m_colorIndex;
};
