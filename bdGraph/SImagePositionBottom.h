class SImagePositionBottom : public SImagePosition
{
private:
	int		m_state;
	int		m_linesSkipped;
	const BYTE*	bsampleRowPos;
	DWORD	bsampleRowsLeft;
	DWORD	bsampleRepeat;
	DWORD	browAdvance;
//////////////////
	BYTE*	m_zeros;
	DWORD	m_width;
///////////////////
public:
	SImagePositionBottom(ImageData* imageData);
	~SImagePositionBottom();
private:
//////////////////
	void GetRow();
	BOOL Reset(int colorIndex);
};
