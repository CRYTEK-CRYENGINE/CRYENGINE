// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CGlyphBitmap
{
public:
	CGlyphBitmap();
	~CGlyphBitmap();

	int            Create(int iWidth, int iHeight);
	int            Release();

	unsigned char* GetBuffer() { return m_pBuffer; };

	int            Blur(int iIterations);
	int            Scale(float fScaleX, float fScaleY);

	int            Clear();

	int            BlitTo8(unsigned char* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth);
	int            BlitTo32(unsigned int* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth);

	int            BlitScaledTo8(unsigned char* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth, int iDestHeight, int iDestBufferWidth);
	int            BlitScaledTo32(unsigned char* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth, int iDestHeight, int iDestBufferWidth);

	int            GetWidth() const  { return m_iWidth; }
	int            GetHeight() const { return m_iHeight; }

	void           GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_pBuffer, m_iWidth * m_iHeight);
	}

private:

	unsigned char* m_pBuffer;
	int            m_iWidth;
	int            m_iHeight;
};
