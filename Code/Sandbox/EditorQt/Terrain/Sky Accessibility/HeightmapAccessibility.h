// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "HorizonTracker.h"           // CHorizonTracker

#include <math.h>                     // sin()
#include <vector>                     // STL vector<>
#include "QT/Widgets/QWaitProgress.h"

// you can use CHemisphereSolid
// or implement your own CHemisphereSink_Solid type (e.g. sum colors from given diffuse texture, sum circular spot for soft shadow, ... )

// Full Sky is 255
// brightness is equally distributed over the hemisphere
// sample precision is 8.8 fixpoint
// can be used as template argument (THemisphere)
class CHemisphereSink_Solid
{
public:

	typedef unsigned short SampleType;      //!< 8.8 fix point

	// ---------------------------------------------------------------------------------

	//! constructor
	//! \param indwAngleSteps [1..[
	CHemisphereSink_Solid(const unsigned int indwAngleSteps, const unsigned int, const unsigned int)
	{
		// to scale the input to the intermediate range

		m_fScale = (256.0f * 256.0f - 1.0f)         // 256 for 8bit fix point,
		           / ((float)indwAngleSteps);       // AddToIntermediate is called indwAngleSteps times
	}

	//!
	//!
	void InitSample(SampleType& inoutValue, const uint32, const uint32) const
	{
		inoutValue = 0;   // 8.8 fix point
	}

	//! \infAngle infAngle in rad (not needed here because every direction is equal)
	void SetAngle(const float infAngle)
	{
	}

	//! \param infSlope [0..[
	//! \param inoutValue result is added to this value
	void AddWedgeArea(const float, const float infSlope, SampleType& inoutValue, float slopeDist) const
	{
		float fWedgeHorizonAngle = (float)((gf_PI / 2) - atanf(-infSlope));     // [0..PI/2[

		// adjustment to get barely covered areas brighter (scan part of sphere instead of full hemisphere)
		fWedgeHorizonAngle = min(gf_PI * 0.5f, fWedgeHorizonAngle * 1.1f);

		inoutValue += (SampleType)(m_fScale * CurvedTriangleArea(fWedgeHorizonAngle));
	}

	const bool NeedsPostProcessing() const
	{
		return false;
	}

	//called when calculation has been finished to post process data if required
	void OnCalcEnd(SampleType&) {}

protected: // ---------------------------------------------------------------------------------

	float m_fScale;           //!<  to scale the input to the intermediate range

	static float CurvedTriangleArea(const float fAngle) { return 1.0f - cosf(fAngle); }

}; // CHemisphereSink_Solid

// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------

// only area between min and max angle is adding to the result
// useful for simulating a area light source
class CHemisphereSink_Slice : public CHemisphereSink_Solid
{
public:

	//! constructor
	CHemisphereSink_Slice(const unsigned int indwAngleSteps, const unsigned int indwWidth, const unsigned int indwHeight)
		: CHemisphereSink_Solid(indwAngleSteps, indwWidth, indwHeight)
	{
		m_fAngleAreaSubtract = 0;
		m_fFullAngleArea = 0;
	}

	void SetMinAndMax(const float infMinAngle, const float infMaxAngle)
	{
		m_fAngleAreaSubtract = CurvedTriangleArea(infMinAngle);

		m_fFullAngleArea = CurvedTriangleArea(infMaxAngle) - m_fAngleAreaSubtract;

		m_fFullAngleArea = max(m_fFullAngleArea, 0.0001f);   // to avoid divide by zero

		m_fScale /= m_fFullAngleArea;
	}

	void AddWedgeArea(const float, const float infSlope, SampleType& inoutValue, float slopeDist) const
	{
		float fWedgeHorizonAngle = (float)((gf_PI / 2) - atanf(-infSlope));     // [0..PI/2[

		float fAngleWedgeArea = CurvedTriangleArea(fWedgeHorizonAngle) - m_fAngleAreaSubtract;

		if (fAngleWedgeArea > 0)
		{
			if (fAngleWedgeArea < m_fFullAngleArea) inoutValue += (SampleType)(m_fScale * fAngleWedgeArea);
			else inoutValue += (SampleType)(m_fScale * m_fFullAngleArea);
		}
	}

private:

	float m_fAngleAreaSubtract;           //!<
	float m_fFullAngleArea;               //!<
};

// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------

template<class THemisphereSink>
class CHeightmapAccessibility
{
public:
	//typedef typename THemisphereSink::SampleType SampleType;
	typedef typename THemisphereSink::SampleType TSampleType;     //!< 8.8 fix point

	//! constructor
	//! \param indwAngleSteps try 10 for average quality or more for better quality
	//! \param indwWidth has to be power of two
	//! \param indwHeight has to be power of two
	//! !DO NOT MOVE THE m_ResultBuffer ALLOCATION OUTSIDE THE CTOR
	CHeightmapAccessibility(const unsigned int indwWidth, const unsigned int indwHeight, const unsigned int indwAngleSteps,
	                        const float infAngleStart = 0.0f, const float infAngleEnd = (float)(gf_PI2)) : m_Sink(indwAngleSteps, indwWidth, indwHeight), m_SamplesInitialized(false)
	{
		m_fAngleStart = infAngleStart;
		m_fAngleEnd = infAngleEnd;
		m_dwAngleSteps = indwAngleSteps;
		assert(indwWidth >= 0);
		assert(indwHeight >= 0);
		m_dwResultWidth = indwWidth;
		m_dwResultHeight = indwHeight;
		m_XShift = GetIntLog2(m_dwResultWidth);
		m_YShift = GetIntLog2(m_dwResultHeight);

		const uint32 cSingleAllocationSize = 4 * 1024 * 1024;//4 MB pieces
		m_PartMapping = cSingleAllocationSize / sizeof(TSampleType);//number of elements per segment
		m_AllocParts = m_dwResultWidth * m_dwResultHeight / m_PartMapping;
		m_AllocParts += (m_AllocParts * m_PartMapping == m_dwResultWidth * m_dwResultHeight) ? 0 : 1;
		m_ResultBuffer.resize(m_AllocParts);
		if (m_AllocParts == 1)
			m_ResultBuffer[0].resize(m_dwResultWidth * m_dwResultHeight);
		else
		{
			for (int i = 0; i < m_AllocParts - 1; ++i)
				m_ResultBuffer[i].resize(m_PartMapping);
			//allocate rest for last one
			assert(m_dwResultWidth * m_dwResultHeight - (m_AllocParts - 1) * m_PartMapping > 0);
			m_ResultBuffer[m_AllocParts - 1].resize(m_dwResultWidth * m_dwResultHeight - (m_AllocParts - 1) * m_PartMapping);
		}
		m_bTiling = false;
	}

	//! initializes samples
	//! exposed to give user the opportunity to determine the time of initialization to maybe further alter the sample data before calling Calc
	void InitSamples()
	{
		if (m_SamplesInitialized)
			return;
		for (unsigned int x = 0; x < m_dwResultWidth; x++)
			for (unsigned int y = 0; y < m_dwResultHeight; y++)
				m_Sink.InitSample(GetSampleAt(x, y), x, y);
		m_SamplesInitialized = true;
	}

	//! check out of memory with exception handling
	//! \param indwWidth has to be power of two
	//! \param indwHeight has to be power of two
	//! \return true=success, false= out of memory
	bool Calc(const float* inpHeightmap, const float fScale, const char* cpHeadLine = "Calculating Sky Accessibility")
	{
		assert(inpHeightmap);

		assert(m_fAngleEnd >= m_fAngleStart);

		m_fScale = fScale;

		// clear buffer
		InitSamples();

		bool bProgress = false;
		if (m_dwResultWidth >= 128)
			bProgress = true;

		CWaitProgress progress(cpHeadLine, false);
		if (bProgress)
			progress.Start();

		// the horizon is subdivided in wedges
		for (unsigned int dwWedge = 0; dwWedge < m_dwAngleSteps; dwWedge++)
		{
			if (bProgress)
			{
				if (!progress.Step((100 * dwWedge) / m_dwAngleSteps))
					return false;
			}

			float fAngle = (float)(dwWedge + 0.5f) / (float)m_dwAngleSteps * (m_fAngleEnd - m_fAngleStart) + m_fAngleStart;
			float fDx = cosf(fAngle), fDy = sinf(fAngle);

			if (fabs(fDx) < fabs(fDy))
			{
				// lines are mainly vertical
				float dx = fDx / fabsf(fDy);

				CalcHeightmapAccessWedge(inpHeightmap,    //
				                         fAngle,
				                         0, fDy > 0 ? 1 : -1,                           // line direction
				                         dx > 0 ? 1 : -1, 0,                            // line start direction (to fill area) = line step direction
				                         fabsf(dx), m_dwResultWidth, m_dwResultHeight); //
			}
			else
			{
				// lines are mainly horizontal
				float dy = fDy / fabsf(fDx);

				CalcHeightmapAccessWedge(inpHeightmap,    //
				                         fAngle,
				                         fDx > 0 ? 1 : -1, 0,                           // line direction
				                         0, dy > 0 ? 1 : -1,                            // line start direction (to fill area) = line step direction
				                         fabsf(dy), m_dwResultHeight, m_dwResultWidth); //
			}
		}

		if (m_Sink.NeedsPostProcessing())
		{
			for (int i = 0; i < m_dwResultWidth; ++i)
				for (int j = 0; j < m_dwResultHeight; ++j)
					m_Sink.OnCalcEnd(GetSampleAt(i, j));
		}

		if (bProgress)
			progress.Stop();

		m_fScale = 0.0f;

		return true;    // success
	}

	const TSampleType GetSampleAt(const unsigned int indwX, const unsigned int indwY) const
	{
		if (m_AllocParts == 1)
			return m_ResultBuffer[0][indwX + (indwY << m_XShift)];
		else
		{
			const unsigned int cIndex = (indwY * m_dwResultWidth + indwX);
			const unsigned int cBaseIndex = cIndex / m_PartMapping;
			const unsigned int cOffsetIndex = cIndex - cBaseIndex * m_PartMapping;
			assert(cBaseIndex < m_AllocParts);
			assert(cOffsetIndex < m_PartMapping);
			return(m_ResultBuffer[cBaseIndex][cOffsetIndex]);
		}
	}

	TSampleType& GetSampleAt(const unsigned int indwX, const unsigned int indwY)
	{
		if (m_AllocParts == 1)
			return m_ResultBuffer[0][indwX + (indwY << m_XShift)];
		else
		{
			const unsigned int cIndex = (indwY * m_dwResultWidth + indwX);
			const unsigned int cBaseIndex = cIndex / m_PartMapping;
			const unsigned int cOffsetIndex = cIndex - cBaseIndex * m_PartMapping;
			assert(cBaseIndex < m_AllocParts);
			assert(cOffsetIndex < m_PartMapping);
			return(m_ResultBuffer[cBaseIndex][cOffsetIndex]);
		}
	}

	// ---------------------------------------------------------------------------------

	// public to make if more convenient for the user

	THemisphereSink m_Sink;               //!< is
	bool            m_bTiling;            //!< true with tiling, false=faster

	// ---------------------------------------------------------------------------------

private:

	// properties (for a full hemisphere m_fAngleEnd-m_fAngleStart = gf_PI_DIV_2)
	float        m_fAngleStart;           //!< in rad
	float        m_fAngleEnd;             //!< in rad
	unsigned int m_dwAngleSteps;          //!< should be >4, more m_dwAngleSteps results in better quality and less speed
	float        m_fScale;                //!< Heightmap scale

	typedef std::vector<std::vector<TSampleType>> CSampleBuffer; //allocate in 4 MB pieces

	// result
	CSampleBuffer m_ResultBuffer;       //!<
	unsigned int  m_dwResultWidth;      //!< value is power of two
	unsigned int  m_dwResultHeight;     //!< value is power of two
	unsigned int  m_XShift;             //!< shift value corresponding to width
	unsigned int  m_YShift;             //!< shift value corresponding to height

	unsigned int  m_AllocParts;         //!< allocation parts, y is divided by that
	unsigned int  m_PartMapping;        //!< mapping to allocation part

	bool          m_SamplesInitialized;   //!< true if samples have been initialized

	// ---------------------------------------------------------------------------------

	//! U is the direction we go always one pixel in positive direction (start point of the lines)
	//! V is the line direction we go one pixel in negative or in positive or not (depending on iniFix8)
	//! uses CalcHeightmapAccessWedge()
	//! \param inpHeightmap must not be 0
	//! \param wedgeAngle current wedge angle, is azimuth and ranging from 0..2pi
	//! \param infStep 0..1 (0.0f=no steps, 1.0f=45 degrees)
	//! \param indwLineCount
	//! \param indwLineLength
	//! \param infResultScale
	void CalcHeightmapAccessWedge(const float* inpHeightmap,
	                              const float wedgeAngle,
	                              const int iLineDirX, const int iLineDirY,
	                              const int iLineStepX, const int iLineStepY,
	                              const float infStep, const unsigned int indwLineCount, const unsigned int indwLineLength)
	{
		assert(inpHeightmap);

		const float fLenStep = sqrtf(fabsf(infStep) + 1.0f);
		float fSlopeDist = 0.f;

		// this works only for power of two width and height
		const unsigned int dwXMask = m_dwResultWidth - 1;
		const unsigned int dwYMask = m_dwResultHeight - 1;

		CHorizonTracker HorizonTracker;
		// many lines fill the whole block
		for (unsigned int dwLine = 0; dwLine < indwLineCount; dwLine++)
		{
			HorizonTracker.Clear();
			int iX, iY;

			// start position of each line
			if (iLineDirY == 0)        // mainly horizonal lines
			{
				iX = iLineDirX > 0 ? 0 : m_dwResultWidth - 1;
				iY = dwLine;
			}
			else                    // mainly vertical lines
			{
				iX = dwLine;
				iY = iLineDirY > 0 ? 0 : m_dwResultHeight - 1;
			}

			float fFilterValue = 0.5f;    // 0..1
			float fLen = 0.0f;

			// one line
			if (!m_bTiling)
				for (unsigned int dwLinePos = 0; dwLinePos < indwLineLength; dwLinePos++, fLen += fLenStep)
				{
					assert(fFilterValue >= 0 && fFilterValue <= 1.0f);

					// get two height samples
					float fHeight1 = inpHeightmap[(iX & dwXMask) + ((iY & dwYMask) << m_XShift)];
					float fHeight2 = inpHeightmap[((iX + iLineStepX) & dwXMask) + (((iY + iLineStepY) & dwYMask) << m_XShift)];

					// get linear filtered height
					float fFilteredHeight = fHeight1 * (1.0f - fFilterValue) + fHeight2 * fFilterValue;

					if ((iX >> m_XShift) != 0 || (iY >> m_YShift) != 0)
					{
						HorizonTracker.Clear();
						iX &= dwXMask;
						iY &= dwYMask;
					}

					float fSlope = HorizonTracker.Insert(fLen, m_fScale * fFilteredHeight, fSlopeDist);

					m_Sink.AddWedgeArea(wedgeAngle, fSlope, GetSampleAt(iX, iY), fSlopeDist);

					// advance one pixel
					iX += iLineDirX;
					iY += iLineDirY;

					fFilterValue += infStep;

					if (fFilterValue > 1.0f)
					{
						fFilterValue -= 1.0f;
						iX += iLineStepX;
						iY += iLineStepY;
					}
				}

			// one line (two times longer to be sure the result is correct with tiling - optimizable)
			if (m_bTiling)
				for (unsigned int dwLinePos = 0; dwLinePos < indwLineLength * 2; dwLinePos++, fLen += fLenStep)
				{
					assert(fFilterValue >= 0 && fFilterValue <= 1.0f);

					// get two height samples
					float fHeight1 = inpHeightmap[(iX & dwXMask) + ((iY & dwYMask) << m_XShift)];
					float fHeight2 = inpHeightmap[((iX + iLineStepX) & dwXMask) + (((iY + iLineStepY) & dwYMask) << m_XShift)];

					// get linear filtered height
					float fFilteredHeight = fHeight1 * (1.0f - fFilterValue) + fHeight2 * fFilterValue;

					float fSlope = HorizonTracker.Insert(fLen, m_fScale * fFilteredHeight, fSlopeDist);

					if (dwLinePos >= indwLineLength) // first half of line length is necessary to make it tileable
						m_Sink.AddWedgeArea(wedgeAngle, fSlope, GetSampleAt((iX & dwXMask), (iY & dwYMask)), fSlopeDist);

					// advance one pixel
					iX += iLineDirX;
					iY += iLineDirY;

					fFilterValue += infStep;

					if (fFilterValue > 1.0f)
					{
						fFilterValue -= 1.0f;
						iX += iLineStepX;
						iY += iLineStepY;
					}
				}

		}
	}

}; // CHeightmapAccessibility

