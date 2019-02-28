// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2013-2014.
// -------------------------------------------------------------------------
//  File name:   FIR-Weights.h
//  Version:     v1.00
//  Created:     24/05/2013 by nielsf.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef FIR_WEIGHTS_H
#define FIR_WEIGHTS_H

#include "FIR-Windows.h"

/* ####################################################################################################################
 */
template<class DataType>
static inline DataType abs    (const DataType& ths                            ) { return (ths < 0 ? -ths : ths); }
template<class DataType>
static inline void     minmax (const DataType& ths, DataType& mn, DataType& mx) { mn = (mn > ths ? ths : mn); mx = (mx < ths ? ths : mx); }
template<class DataType>
static inline DataType minimum(const DataType& ths, const DataType& tht       ) { return (ths < tht ? ths : tht); }
template<class DataType>
static inline DataType maximum(const DataType& ths, const DataType& tht       ) { return (ths > tht ? ths : tht); }

/* ####################################################################################################################
 */

template<class T>
class FilterWeights
{

public:
	FilterWeights()
		: first(0)
		, last(0)
		, hasNegativeWeights(false)
		, weights(nullptr)
	{
	}

	~FilterWeights()
	{
		delete[] weights;
	}

public:
	// window-position
	int first, last;

	// do we encounter positive as well as negative weights
	bool hasNegativeWeights;

	/* weights, summing up to -(1 << 15),
	 * means weights are given negative
	 * that enables us to use signed short
	 * multiplication while occupying 0x8000
	 */
	T* weights;
};

/* ####################################################################################################################
 */

void               calculateFilterRange (unsigned int srcFactor, int &srcFirst, int &srcLast,
                                         unsigned int dstFactor, int  dstFirst, int  dstLast,                                  
                                         double blurFactor, class IWindowFunction<double> *windowFunction);

template<typename T>
FilterWeights<T> *calculateFilterWeights(unsigned int srcFactor, int  srcFirst, int  srcLast,
                                         unsigned int dstFactor, int  dstFirst, int  dstLast, signed short int numRepetitions, 
                                         double blurFactor, class IWindowFunction<double> *windowFunction,
                                         bool peaknorm, bool &plusminus);

template<>
FilterWeights<signed short int> *calculateFilterWeights<signed short int>(unsigned int srcFactor, int srcFirst, int srcLast,
                                         unsigned int dstFactor, int dstFirst, int dstLast, signed short int numRepetitions,
                                         double blurFactor, class IWindowFunction<double> *windowFunction,
                                         bool peaknorm, bool &plusminus);
#endif
