// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GlyphBitmap.h"
#include <math.h>

//-------------------------------------------------------------------------------------------------
CGlyphBitmap::CGlyphBitmap()
	: m_iWidth(0), m_iHeight(0), m_pBuffer(0)
{
}

//-------------------------------------------------------------------------------------------------
CGlyphBitmap::~CGlyphBitmap()
{
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::Create(int iWidth, int iHeight)
{
	Release();

	m_pBuffer = new unsigned char[iWidth * iHeight];

	if (!m_pBuffer)
	{
		return 0;
	}

	m_iWidth = iWidth;
	m_iHeight = iHeight;

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::Release()
{
	if (m_pBuffer)
	{
		delete[] m_pBuffer;
	}
	m_pBuffer = 0;
	m_iWidth = m_iHeight = 0;

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::Blur(int iIterations)
{
	int cSum;
	int yOffset;
	int yupOffset;
	int ydownOffset;

	for (int i = 0; i < iIterations; i++)
	{
		for (int y = 0; y < m_iHeight; y++)
		{
			yOffset = y * m_iWidth;

			if (y - 1 >= 0)
			{
				yupOffset = (y - 1) * m_iWidth;
			}
			else
			{
				yupOffset = (y) * m_iWidth;
			}

			if (y + 1 < m_iHeight)
			{
				ydownOffset = (y + 1) * m_iWidth;
			}
			else
			{
				ydownOffset = (y) * m_iWidth;
			}

			for (int x = 0; x < m_iWidth; x++)
			{
				cSum = m_pBuffer[yupOffset + x] + m_pBuffer[ydownOffset + x];

				if (x - 1 >= 0)
				{
					cSum += m_pBuffer[yOffset + x - 1];
				}
				else
				{
					cSum += m_pBuffer[yOffset + x];
				}

				if (x + 1 < m_iWidth)
				{
					cSum += m_pBuffer[yOffset + x + 1];
				}
				else
				{
					cSum += m_pBuffer[yOffset + x];
				}

				m_pBuffer[yOffset + x] = cSum >> 2;
			}
		}
	}

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::Scale(float fScaleX, float fScaleY)
{
	int iNewWidth = (int)(m_iWidth * fScaleX);
	int iNewHeight = (int)(m_iHeight * fScaleY);

	unsigned char* pNewBuffer = new unsigned char[iNewWidth * iNewHeight];

	if (!pNewBuffer)
	{
		return 0;
	}

	float xFactor = m_iWidth / (float)iNewWidth;
	float yFactor = m_iHeight / (float)iNewHeight;

	float xFractioned, yFractioned, xFraction, yFraction, oneMinusX, oneMinusY, fR0, fR1;
	int xCeil, yCeil, xFloor, yFloor, yNewOffset;

	unsigned char c0, c1, c2, c3;

	for (int y = 0; y < iNewHeight; ++y)
	{
		yFractioned = y * yFactor;
		yFloor = (int)floor_tpl(yFractioned);
		yCeil = yFloor + 1;

		if (yCeil >= m_iHeight)
		{
			yCeil = yFloor;
		}

		yFraction = yFractioned - yFloor;
		oneMinusY = 1.0f - yFraction;

		yNewOffset = y * iNewWidth;

		for (int x = 0; x < iNewWidth; ++x)
		{
			xFractioned = x * xFactor;
			xFloor = (int)floor_tpl(xFractioned);
			xCeil = xFloor + 1;

			if (xCeil >= m_iWidth)
			{
				xCeil = xFloor;
			}

			xFraction = xFractioned - xFloor;
			oneMinusX = 1.0f - xFraction;

			c0 = m_pBuffer[yFloor * m_iWidth + xFloor];
			c1 = m_pBuffer[yFloor * m_iWidth + xCeil];
			c2 = m_pBuffer[yCeil * m_iWidth + xFloor];
			c3 = m_pBuffer[yCeil * m_iWidth + xCeil];

			fR0 = (oneMinusX * c0 + xFraction * c1);
			fR1 = (oneMinusX * c2 + xFraction * c3);

			pNewBuffer[yNewOffset + x] = (unsigned char)((oneMinusY * fR0) + (yFraction * fR1));
		}
	}

	m_iWidth = iNewWidth;
	m_iHeight = iNewHeight;

	delete[] m_pBuffer;
	m_pBuffer = pNewBuffer;

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::Clear()
{
	memset(m_pBuffer, 0, m_iWidth * m_iHeight);

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::BlitTo8(unsigned char* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth)
{
	int ySrcOffset, yDestOffset;

	for (int y = 0; y < iSrcHeight; y++)
	{
		ySrcOffset = (iSrcY + y) * m_iWidth;
		yDestOffset = (iDestY + y) * iDestWidth;

		for (int x = 0; x < iSrcWidth; x++)
		{
			pBuffer[yDestOffset + iDestX + x] = m_pBuffer[ySrcOffset + iSrcX + x];
		}
	}

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::BlitTo32(unsigned int* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth)
{
	int ySrcOffset, yDestOffset;
	char cColor;

	for (int y = 0; y < iSrcHeight; y++)
	{
		ySrcOffset = (iSrcY + y) * m_iWidth;
		yDestOffset = (iDestY + y) * iDestWidth;

		for (int x = 0; x < iSrcWidth; x++)
		{
			cColor = m_pBuffer[ySrcOffset + iSrcX + x];

			pBuffer[yDestOffset + iDestX + x] = (cColor << 24) | (255 << 16) | (255 << 8) | 255;
		}
	}

	return 1;
}

//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::BlitScaledTo8(unsigned char* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth, int iDestHeight, int iDestBufferWidth)
{
	int iNewWidth = (int)iDestWidth;
	int iNewHeight = (int)iDestHeight;

	unsigned char* pNewBuffer = pBuffer;

	float xFactor = iSrcWidth / (float)iNewWidth;
	float yFactor = iSrcHeight / (float)iNewHeight;

	float xFractioned, yFractioned, xFraction, yFraction, oneMinusX, oneMinusY, fR0, fR1;
	int xCeil, yCeil, xFloor, yFloor, yNewOffset;

	unsigned char c0, c1, c2, c3;

	for (int y = 0; y < iNewHeight; ++y)
	{
		yFractioned = y * yFactor;
		yFloor = (int)floor_tpl(yFractioned);
		yCeil = yFloor + 1;

		yFraction = yFractioned - yFloor;
		oneMinusY = 1.0f - yFraction;

		yNewOffset = y * iDestBufferWidth;

		yFloor += iSrcY;
		yCeil += iSrcY;

		if (yCeil >= m_iHeight)
		{
			yCeil = yFloor;
		}

		for (int x = 0; x < iNewWidth; ++x)
		{
			xFractioned = x * xFactor;
			xFloor = (int)floor_tpl(xFractioned);
			xCeil = xFloor + 1;

			xFraction = xFractioned - xFloor;
			oneMinusX = 1.0f - xFraction;

			xFloor += iSrcY;
			xCeil += iSrcY;

			if (xCeil >= m_iWidth)
			{
				xCeil = xFloor;
			}

			c0 = m_pBuffer[yFloor * m_iWidth + xFloor];
			c1 = m_pBuffer[yFloor * m_iWidth + xCeil];
			c2 = m_pBuffer[yCeil * m_iWidth + xFloor];
			c3 = m_pBuffer[yCeil * m_iWidth + xCeil];

			fR0 = (oneMinusX * c0 + xFraction * c1);
			fR1 = (oneMinusX * c2 + xFraction * c3);

			pNewBuffer[yNewOffset + x + iDestX] = (unsigned char)((oneMinusY * fR0) + (yFraction * fR1));
		}
	}

	return 1;
}

#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic ignored "-Woverflow"
	#endif
#endif
//-------------------------------------------------------------------------------------------------
int CGlyphBitmap::BlitScaledTo32(unsigned char* pBuffer, int iSrcX, int iSrcY, int iSrcWidth, int iSrcHeight, int iDestX, int iDestY, int iDestWidth, int iDestHeight, int iDestBufferWidth)
{
	int iNewWidth = (int)iDestWidth;
	int iNewHeight = (int)iDestHeight;

	unsigned char* pNewBuffer = pBuffer;

	float xFactor = iSrcWidth / (float)iNewWidth;
	float yFactor = iSrcHeight / (float)iNewHeight;

	float xFractioned, yFractioned, xFraction, yFraction, oneMinusX, oneMinusY, fR0, fR1;
	int xCeil, yCeil, xFloor, yFloor, yNewOffset;

	unsigned char c0, c1, c2, c3, cColor;

	for (int y = 0; y < iNewHeight; ++y)
	{
		yFractioned = y * yFactor;
		yFloor = (int)floor_tpl(yFractioned);
		yCeil = yFloor + 1;

		yFraction = yFractioned - yFloor;
		oneMinusY = 1.0f - yFraction;

		yNewOffset = y * iDestBufferWidth;

		yFloor += iSrcY;
		yCeil += iSrcY;

		if (yCeil >= m_iHeight)
		{
			yCeil = yFloor;
		}

		for (int x = 0; x < iNewWidth; ++x)
		{
			xFractioned = x * xFactor;
			xFloor = (int)floor_tpl(xFractioned);
			xCeil = xFloor + 1;

			xFraction = xFractioned - xFloor;
			oneMinusX = 1.0f - xFraction;

			xFloor += iSrcY;
			xCeil += iSrcY;

			if (xCeil >= m_iWidth)
			{
				xCeil = xFloor;
			}

			c0 = m_pBuffer[yFloor * m_iWidth + xFloor];
			c1 = m_pBuffer[yFloor * m_iWidth + xCeil];
			c2 = m_pBuffer[yCeil * m_iWidth + xFloor];
			c3 = m_pBuffer[yCeil * m_iWidth + xCeil];

			fR0 = (oneMinusX * c0 + xFraction * c1);
			fR1 = (oneMinusX * c2 + xFraction * c3);

			cColor = (unsigned char)((oneMinusY * fR0) + (yFraction * fR1));

			pNewBuffer[yNewOffset + x + iDestX] = 0xffffff | (cColor << 24);
		}
	}

	return 1;
}
#if defined(__GNUC__)
	#if __GNUC__ >= 4 && __GNUC__MINOR__ < 7
		#pragma GCC diagnostic error  "-Woverflow"
	#endif
#endif
//-------------------------------------------------------------------------------------------------
