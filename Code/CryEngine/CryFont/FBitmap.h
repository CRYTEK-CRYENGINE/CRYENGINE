// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CFBitmap
{
public:
	CFBitmap();
	~CFBitmap();

	int Blur(int iIterations);
	int Scale(float fScaleX, float fScaleY);

	int BlitFrom(CFBitmap* pSrc, int iSX, int iSY, int iDX, int iDY, int iW, int iH);
	int BlitTo(CFBitmap* pDst, int iDX, int iDY, int iSX, int iSY, int iW, int iH);

	int Create(int iWidth, int iHeight);
	int Release();

	int SaveBitmap(const string& szFileName);
	int Get32Bpp(unsigned int** pBuffer)
	{
		(*pBuffer) = new unsigned int[m_iWidth * m_iHeight];

		if (!(*pBuffer))
		{
			return 0;
		}

		int iDataSize = m_iWidth * m_iHeight;

		for (int i = 0; i < iDataSize; i++)
		{
			(*pBuffer)[i] = (m_pData[i] << 24) | (m_pData[i] << 16) | (m_pData[i] << 8) | (m_pData[i]);
		}

		return 1;
	}

	int   GetWidth()                       { return m_iWidth; }
	int   GetHeight()                      { return m_iHeight; }

	void  SetRenderData(void* pRenderData) { m_pIRenderData = pRenderData; };
	void* GetRenderData()                  { return m_pIRenderData; };

	void GetMemoryUsage(class ICrySizer* pSizer);

	unsigned char* GetData() { return m_pData; }

public:

	int            m_iWidth;
	int            m_iHeight;

	unsigned char* m_pData;
	void*          m_pIRenderData;
};
