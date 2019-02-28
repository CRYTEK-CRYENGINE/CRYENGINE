// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DynamicArray2D.h"
#include <CryMemory/CrySizer.h>             // ICrySizer

//////////////////////////////////////////////////////////////////////
// Construction / destruction
//////////////////////////////////////////////////////////////////////

CDynamicArray2D::CDynamicArray2D(unsigned int iDimension1, unsigned int iDimension2)
{
	////////////////////////////////////////////////////////////////////////
	// Declare a 2D array on the free store
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	// Save the position of the array dimensions
	m_Dimension1 = iDimension1;
	m_Dimension2 = iDimension2;

	// First dimension
	m_Array = new float*[m_Dimension1];
	VERIFY(m_Array);

	// Second dimension
	for (i = 0; i < m_Dimension1; ++i)
	{
		m_Array[i] = new float[m_Dimension2];

		// Init all fields with 0
		memset(&m_Array[i][0], 0, m_Dimension2 * sizeof(float));
	}
}

CDynamicArray2D::~CDynamicArray2D()
{
	////////////////////////////////////////////////////////////////////////
	// Remove the 2D array and all its sub arrays from the free store
	////////////////////////////////////////////////////////////////////////

	unsigned int i;

	for (i = 0; i < m_Dimension1; ++i)
		delete[] m_Array[i];

	delete[] m_Array;
	m_Array = 0;
}

void CDynamicArray2D::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->Add((char*)this, m_Dimension1 * m_Dimension2 * sizeof(float) + sizeof(*this));
}

void CDynamicArray2D::ScaleImage(CDynamicArray2D* pDestination)
{
	////////////////////////////////////////////////////////////////////////
	// Scale an image stored (in an array class) to a new size
	////////////////////////////////////////////////////////////////////////

	unsigned int i, j, iOldWidth;
	int iXSrcFl, iXSrcCe, iYSrcFl, iYSrcCe;
	float fXSrc, fYSrc;
	float fHeight[4];
	float fHeightWeight[4];
	float fHeightBottom;
	float fHeightTop;

	assert(pDestination);
	assert(pDestination->m_Dimension1 > 1);

	// Width has to be zero based, not a count
	iOldWidth = m_Dimension1 - 1;

	// Loop trough each field of the new image and interpolate the value
	// from the source heightmap
	for (i = 0; i < pDestination->m_Dimension1; i++)
	{
		// Calculate the average source array position
		fXSrc = i / (float) pDestination->m_Dimension1 * iOldWidth;
		assert(fXSrc >= 0.0f && fXSrc <= iOldWidth);

		// Precalculate floor and ceiling values. Use fast asm integer floor and
		// fast asm float / integer conversion
		iXSrcFl = (int)floor(fXSrc);
		iXSrcCe = pos_directed_rounding((float) ceil(fXSrc));

		// Distribution between left and right height values
		fHeightWeight[0] = (float) iXSrcCe - fXSrc;
		fHeightWeight[1] = fXSrc - (float) iXSrcFl;

		// Avoid error when floor() and ceil() return the same value
		if (fHeightWeight[0] == 0.0f && fHeightWeight[1] == 0.0f)
		{
			fHeightWeight[0] = 0.5f;
			fHeightWeight[1] = 0.5f;
		}

		for (j = 0; j < pDestination->m_Dimension1; j++)
		{
			// Calculate the average source array position
			fYSrc = j / (float) pDestination->m_Dimension1 * iOldWidth;
			assert(fYSrc >= 0.0f && fYSrc <= iOldWidth);

			// Precalculate floor and ceiling values. Use fast asm integer floor and
			// fast asm float / integer conversion
			iYSrcFl = (int)floor(fYSrc);
			iYSrcCe = pos_directed_rounding((float) ceil(fYSrc));

			// Get the four nearest height values
			fHeight[0] = m_Array[iXSrcFl][iYSrcFl];
			fHeight[1] = m_Array[iXSrcCe][iYSrcFl];
			fHeight[2] = m_Array[iXSrcFl][iYSrcCe];
			fHeight[3] = m_Array[iXSrcCe][iYSrcCe];

			// Calculate how much weight each height value has

			// Distribution between top and bottom height values
			fHeightWeight[2] = (float) iYSrcCe - fYSrc;
			fHeightWeight[3] = fYSrc - (float) iYSrcFl;

			// Avoid error when floor() and ceil() return the same value
			if (fHeightWeight[2] == 0.0f && fHeightWeight[3] == 0.0f)
			{
				fHeightWeight[2] = 0.5f;
				fHeightWeight[3] = 0.5f;
			}

			// Interpolate between the four nearest height values

			// Get the height for the given X position trough interpolation between
			// the left and the right height
			fHeightBottom = (fHeight[0] * fHeightWeight[0] + fHeight[1] * fHeightWeight[1]);
			fHeightTop = (fHeight[2] * fHeightWeight[0] + fHeight[3] * fHeightWeight[1]);

			// Set the new value in the destination heightmap
			pDestination->m_Array[i][j] = fHeightBottom * fHeightWeight[2] + fHeightTop * fHeightWeight[3];
		}
	}
}

