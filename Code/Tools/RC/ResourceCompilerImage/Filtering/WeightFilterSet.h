// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SimpleBitmap.h"            // SimpleBitmap<>
#include <vector>                    // STL vector<>
#include "SummedAreaFilterKernel.h"  // CSummedAreaTableFilterKernel

class CWeightFilter
{
public:
	CWeightFilter();

	void CreateAverage2x2();

	void CreateNearest2x2(const float sharpness);

	void CreateSharpen3x3(const float sharpness);

	// Arguments:
	//   bCenter == true:
	//     filter width is even (1, 3, 5...): ceil(fRadius + 0.5) * 2 - 1
	//     Center point is at the center of the middle pixel.
	//   bCenter == false:
	//     filter width is odd (2, 4, 6...): ceil(fRadius) * 2
	//     Center point is between four middle pixels.
	bool CreateFromSatFilterKernel(const CSummedAreaTableFilterKernel& satFilterKernel, const float fRadius, const bool bCenter);

	void FreeData();

	bool IsValid() const
	{
		return m_FilterKernelBlock.IsValid();
	}

	//! call this function once prior to calling GetBlockWithFilter_2D()
	template <class TInputImage>
	void PrepareBlockWithFilter_2D(const TInputImage& inSrc)
	{
		const int imageW = (int)inSrc.GetWidth();
		const int imageH = (int)inSrc.GetHeight();

		const int filterW = (int)m_FilterKernelBlock.GetWidth();
		const int filterH = (int)m_FilterKernelBlock.GetHeight();

		m_startX = -filterW / 2;
		do
		{
			m_startX += imageW;
		} while (m_startX < 0);

		m_startY = -filterH / 2;
		do
		{
			m_startY += imageH;
		} while (m_startY < 0);
	}

	// You must call PrepareBlockWithFilter_2D() prior to calling this function!
	//! optimizable
	//! weight is 1.0
	//! /param iniX x position in inSrc
	//! /param iniY y position in inSrc
	//! /param TInputImage typically CSimpleBitmap<TElement>
	//! /return weight
	template <class TElement, class TInputImage >
	float GetBlockWithFilter_2D(const TInputImage& inSrc, const int iniX, const int iniY, TElement& outResult) const
	{
		float fWeightSum=0.0f;
		
		const int imageW = (int)inSrc.GetWidth();
		const int imageH = (int)inSrc.GetHeight();

		const int filterW = (int)m_FilterKernelBlock.GetWidth();
		const int filterH = (int)m_FilterKernelBlock.GetHeight();

		const float* pfWeights=m_FilterKernelBlock.GetPointer(0,0);

		const int x0 = m_startX + iniX;
		const int y0 = m_startY + iniY;

		for(int y=0; y<filterH; ++y)
		{
			const int tiledY = (y0 + y) % imageH;

			for(int x=0; x<filterW; ++x, ++pfWeights)
			{
				const int tiledX = (x0 + x) % imageW;

				const TElement* const pValue = inSrc.GetForFiltering_2D(tiledX, tiledY);

				if(pValue)
				{
					const float fWeight = *pfWeights;
	
					outResult += (*pValue) * fWeight;
					fWeightSum += fWeight;
				}
				else assert(0);
			}
		}

//		assert(fWeightSum<=1.0f);		// less is better - more can introduce in amplifying the data
		return fWeightSum;
	}

	//! optimizable
	//! weight is 1.0
	//! /param iniX x position in inSrc
	//! /param iniY y position in inSrc
	//! /param TInputImage typically CSimpleBitmap<TElement>
	template <class TElement, class TInputImage >
	void GetBlockWithFilter4x4_Pow2_2D( const float* pWeights4x4, const TInputImage &inSrc, const int iniX, const int iniY, TElement &outResult, uint32 dwXMask, uint32 dwYMask) const
	{
		assert((dwXMask & (dwXMask + 1)) == 0);
		assert((dwYMask & (dwYMask + 1)) == 0);
		const uint32 x = (uint32)(iniX - 2);
		for (int y = 0; y < 4; ++y, pWeights4x4 += 4)
		{
			const TElement* const pRow = &inSrc.GetRef(0, ((uint32)(y + iniY - 2)) & dwYMask);
			outResult += 
				(pRow[ x      & dwXMask] * pWeights4x4[0]) +
				(pRow[(x + 1) & dwXMask] * pWeights4x4[1]) +
				(pRow[(x + 2) & dwXMask] * pWeights4x4[2]) +
				(pRow[(x + 3) & dwXMask] * pWeights4x4[3]);
		}
	}

	//! optimizable
	//! weight is 1.0
	//! /param iniX x position in inoutDest
	//! /param iniY y position in inoutDest
	//! /param TInputImage typically CSimpleBitmap<TElement>
	//! /return weight
	template <class TElement, class TInputImage >
	float GetBlockWithFilter_Cubemap( const TInputImage &inSrc, const int iniX, const int iniY, TElement &outResult ) const
	{
		float fWeightSum=0.0f;
		const CSimpleBitmap<float> &rBitmap=m_FilterKernelBlock;

		const int W=(int)rBitmap.GetWidth();
		const int H=(int)rBitmap.GetHeight();

		const float* pfWeights=rBitmap.GetPointer(0,0);

		for(int y=0; y<H; ++y)
		{
			const int iDestY = y+iniY-H/2;

			for(int x=0; x<W; ++x, ++pfWeights)
			{
				const int iDestX = x+iniX-W/2;

				const TElement* const pValue = inSrc.GetForFiltering_Cubemap(iDestX, iDestY, iniX, iniY);

				if(pValue)
				{
					const float fWeight = *pfWeights;

					outResult += (*pValue) * fWeight;
					fWeightSum += fWeight;
				}
				else assert(0);
			}
		}

		//		assert(fWeightSum<=1.0f);		// less is better - more can introduce in amplifying the data
		return fWeightSum;
	}

	float ComputeSum() const;

	int GetWidth() const
	{
		return m_FilterKernelBlock.GetWidth();
	}

	int GetHeight() const
	{
		return m_FilterKernelBlock.GetHeight();
	}

	const float* GetWeights() const
	{
		return m_FilterKernelBlock.GetPointer(0, 0);
	}

	void Print(bool bDetailed) const;

private: // -------------------------------------------------------------

	CSimpleBitmap<float> m_FilterKernelBlock;

	int m_startX;  // set and used by XXXBlockWithFilter_2D()
	int m_startY;

	char m_description[80];
};
