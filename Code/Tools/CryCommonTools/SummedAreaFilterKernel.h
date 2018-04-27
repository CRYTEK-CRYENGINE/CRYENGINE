// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <string>											// STL string
#include "SimpleBitmap.h"							// SimpleBitmap<>

// squared of any size (summed area tables limit the size and/or values)
// normalized(sum=1)

// optimized for high quality, not speed
// for faster filter kernels extract the neccessary size and use this 1:1

// based on summed area tables

class CSummedAreaFilterKernel :public CSimpleBitmap<int>
{
public:

	//! constructor init is eEmpty
	CSummedAreaFilterKernel();

	//! load 8 bit photoshop 256x256 raw image - slow
	//! typical filtersize for a gaussian filter kernel is 1.44
	//! /param iniMidValue [0..255[ this enables sharpening - sharpening may expand the result range
	bool CreateFromRAWFile( const char *filename, const unsigned long indwSize=256, const int iniMidValue=0 );

	//! sharpest possible result - filter diameter size has to be 16*pixelsize (256 samples per pixel)
	//! theory: http://home.no.net/dmaurer/~dersch/interpolator/interpolator.html
	//! /param indwSize >2
	bool CreateFromSincCalc( const unsigned long indwSize=256 );

	//! shttp://www.sixsigma.de/english/sixsigma/6s_e_gauss.htm
	//! /param indwSize >2
	bool CreateFromGauss( const unsigned long indwSize=256 );

	//! optimizable O(k*1) with high k
	//! bokeh is in the range ([-1..1],[-1..1])
	//! return normalized result
	float GetAreaAA( float infAx, float infAy, float infDx, float infDy ) const;

	//! O(k*1) with low k
	//! bokeh is in the range ([-1..1],[-1..1])
	//! return normalized result
	float GetAreaNonAA( float infAx, float infAy, float infDx, float infDy ) const;

	//!
	//! /return e.g. "FilterKernel(Sinc16x16)"
	std::string GetInfoString( void ) const;

	//! /param infX [0..1[
	//! /param infY [0..1[
	//! /param infWeight [0..[
	//! /param infR >0, radius
	bool CreateWeightFilter( CSimpleBitmap<float> &outFilter, const float infX, const float infY,
		const float infWeight, const float infR  ) const;

	//! weight for the whole block is 1.0
	//! /param indwSideLength [1,..[ e.g. 3 for 3x3 block
	//! /param infR >0, radius
	bool CreateWeightFilterBlock( CSimpleBitmap<float> &outFilter, const unsigned long indwSideLength, const float infR ) const;

	//! with user filter kernel
	//! /param infX
	//! /param infY
	//! /param infWeight [0..[
	//! /param infR >0, radius
	void AddWeights( CSimpleBitmap<float> &inoutFilter, const float infX, const float infY,
		const float infWeight, const float infR ) const;

private: // --------------------------------------------------------------------

	enum EFilterState
	{
		eEmpty,						//!< after calling constructor
		eSinc,						//!< from CreateFromSincCalc
		eRAW,							//!< from CreateFromRAWFile
		eGaussBlur,				//!< from CreateFromGauss
		eDisc,						//!< not implemented
		eGaussSharp				//!< not implemented
	};

	EFilterState		m_eFilterType;					//!< for error checks and GetInfoString()
	float						m_fCorrectionFactor;		//!< to get the normalized (whole kernel has sum of 1) result

	//! optimizable
	//! bokeh is in the range ([0..255],[0..255])
	//! /param infX
	//! /param infY
	//! /return not normalized result
	float _GetBilinearFiltered( const float infX, const float infY ) const;

	//! sum the stored values in the bitmap together
	//! calculate m_fCorrectionFactor
	void _SumUpTableAndNormalize( void );
};
