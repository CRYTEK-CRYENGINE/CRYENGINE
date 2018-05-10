// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SimpleBitmap.h"      // SimpleBitmap<>

// squared of any size (summed area tables limit the size and/or values)
// normalized(sum=1)

// optimized for high quality, not speed
// for faster filter kernels extract the necessary size and use this 1:1

// based on summed area tables

class CSummedAreaTableFilterKernel
	: public CSimpleBitmap<int>
{
public:
	// Initializes as an empty table. You should use CreateXXX() function to form a needed kernel.
	CSummedAreaTableFilterKernel();

	//! load 8 bit photoshop 256x256 raw image - slow
	//! typical filtersize for a gaussian filter kernel is 1.44
	//! /param iniMidValue [0..255[ this enables sharpening - sharpening may expand the result range
	bool CreateFromRawFile(const char* filename, const uint32 indwSize=256, const int iniMidValue=0);

	//! /param indwSize >4
	//! /param sincLobeCount - lobe count ('a' in http://en.wikipedia.org/wiki/Lanczos_resampling),
	//  could be non-integer, for example:
	//  1 - positive; 2 - positive and negative; 3 - positive, negative, positive;
	//  1.7 - positive, short negative, 2.4 - positive, negative, short positive.
	// Typically alpha is 2.0 or 3.0.
	// See also: http://en.wikipedia.org/wiki/Window_function
	bool CreateFromLanczos(const uint32 indwSize, const float sincLobeCount);

	//! /param indwSize >4
	//! /param pi_mul_alpha - pi * alpha, typical value is 3 or 4.
	// See also: http://en.wikipedia.org/wiki/Window_function
	bool CreateFromKaiser(const uint32 indwSize, const float sincLobeCount, const float pi_mul_alpha);

	//! shttp://www.sixsigma.de/english/sixsigma/6s_e_gauss.htm
	//! can also do sharpening filter, set fInnerDiameter=~0.8, fNegativeLobe=0.2
	//! could be optimized
	//! /param indwSize >4
	//! /param fInnerDiameter ]0..1] to limit the positive Lobe radius/diameter
	//! /param fNegativeLobe [0..1[, 0 means not used
	bool CreateFromGauss(const uint32 indwSize, const float fInnerDiameter, const float fNegativeLobe);

	// weight for the whole resulting block is 1.0
	// Arguments:
	//    See CWeightFilter::CreateFromSatFilterKernel()
	bool ComputeWeightFilterBlock(
		CSimpleBitmap<float>& outFilter, 
		const float infR, 
		const bool bCenter) const;

	//!
	//! /return e.g. "Kaizer alpha=2.0"
	const char* GetDescription() const;

private: // --------------------------------------------------------------------
	//! all coordinates are in [0;1] coordinate space.
	//! returns sum of the values in the specified area.
	float GetAreaBoxAA(float minX, float minY, float maxX, float maxY) const;

	//! with user filter kernel
	//! /param infR >0, radius
	void AddWeights(CSimpleBitmap<float>& inoutFilter, const float infR) const;

	//! bokeh is in the range ([0..255],[0..255])
	//! /param infX
	//! /param infY
	//! /return not normalized result
	float GetBilinearFiltered(const float infX, const float infY) const;

	//! sum the stored values in the bitmap together
	//! calculate m_fCorrectionFactor
	void SumUpTableAndNormalize();

	bool IsSymmetrical() const;

	//! mostly for debugging purpose
	bool SaveToRAW(const char* szFileName) const;

private: // --------------------------------------------------------------------

	float       m_fCorrectionFactor;  //!< to get the normalized (whole kernel has sum of 1) result
	char        m_description[64];
};
