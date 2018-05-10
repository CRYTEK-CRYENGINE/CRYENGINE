// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2013.
// -------------------------------------------------------------------------
//  File name:   FIR-Weights.cpp
//  Version:     v1.00
//  Created:     24/05/2013 by nielsf.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include <assert.h>                         // assert()

#include <math.h>
#include "FIR-Weights.h"


#define	roundToInt(x) (int)(((x) >= 0) ? floor((x) + 0.5) : ceil((x) - 0.5))

/* ####################################################################################################################
 */

void calculateFilterRange(unsigned int srcFactor, int &srcFirst, int &srcLast,
                          unsigned int dstFactor, int  dstFirst, int  dstLast, 
                          double blurFactor, class IWindowFunction<double> *windowFunction) 
{
	double s, t, u, scaleFactor;  /* scale factors */
	double srcRadius, srcCenter;  /* window position and size */

#define s0      0
#define s1      srcFactor
#define d0      0
#define d1      dstFactor

	/* the mapping from discrete destination coordinates to continuous source coordinates: */
#define MAP(b, scaleFactor, offset)     ((b) + (offset)) / (scaleFactor)

	/* relation of dstFactor to srcFactor */
	s = (double)dstFactor / srcFactor;
	t = d0 - s * (s0 - 0.5) - 0.5;

	/* compute offsets for MAP */
	u = d0 - s * (s0 - 0.5) - t;

	/* find scale of filter
	 * when minifying, scaleFactor = 1/s, but when magnifying, scaleFactor = 1
	 */
	scaleFactor = (blurFactor == 0.0 ? 1.0 : (blurFactor > 0.0 ? (1.0 + blurFactor) : 1.0 / (1.0 - blurFactor))) * maximum(1., 1. / s);

	/* find support radius of scaled filter
	 * if the window's length is <= 0.5 then we've got point sampling.
	*/
	srcRadius = maximum(0.5, scaleFactor * windowFunction->getLength());

	/* sample the continuous filter, scaled by scaleFactor and
	 * positioned at continuous source coordinate srcCenter
	 */
	{
		srcCenter = MAP(dstFirst + 0, s, u);

		/* find the source coordinate range of this positioned filter window */
		srcFirst = int(floor(srcCenter - srcRadius + 0.5));
	}

	{
		srcCenter = MAP(dstLast - 1, s, u);

		/* find the source coordinate range of this positioned filter window */
		srcLast = int(floor(srcCenter + srcRadius + 0.5));
	}
}

template<>
FilterWeights<signed short int> *calculateFilterWeights<signed short int>(unsigned int srcFactor, int srcFirst, int srcLast,
                                                                          unsigned int dstFactor, int dstFirst, int dstLast, signed short int numRepetitions,
                                                                          double blurFactor, class IWindowFunction<double> *windowFunction,
                                                                          bool peaknorm, bool &plusminus)

{
#define WEIGHTBITS  15
#define WEIGHTONE   (1 << WEIGHTBITS)       /* filter weight of one */

	double s, t, u, scaleFactor;  /* scale factors */
	double srcRadius;  /* support radius of scaled filter */
	double sumfWeights, neg, pos, nrmWeights, fWeight;    /* window position and size */
	int i;
	int dstPosition;
	signed short int n;
	bool trimZeros = true;
	int lastnonzero, hWeight, highest;
	signed int sumiWeights, iWeight;
	signed short int* weightsPtr, *weightsMem;
	FilterWeights<signed short int>* weightsObjs;
	bool pm, pma = false;

	/* pre-calculate filter window solutions for all rows
	 */
	weightsObjs = new FilterWeights<signed short int>[dstLast - dstFirst];

#define s0      0
#define s1      srcFactor
#define d0      0
#define d1      dstFactor

	/* relation of dstFactor to srcFactor */
	s = (double)dstFactor / srcFactor;
	t = d0 - s * (s0 - 0.5) - 0.5;

	/* compute offsets for MAP */
	u = d0 - s * (s0 - 0.5) - t;

	/* find scale of filter
	 * when minifying, scaleFactor = 1/s, but when magnifying, scaleFactor = 1
	*/
	scaleFactor = (blurFactor == 0.0 ? 1.0 : (blurFactor > 0.0 ? (1.0 + blurFactor) : 1.0 / (1.0 - blurFactor))) * maximum(1., 1. / s);

	/* find support radius of scaled filter
	 * if the window's length is <= 0.5 then we've got point sampling.
	 */
	srcRadius = maximum(0.5, scaleFactor * windowFunction->getLength());

	/* sample the continuous filter, scaled by ap->scaleFactor and
	 * positioned at continuous source coordinate srcCenter, for source coordinates in
	 * the range [0..len-1], writing the weights into wtab.
	 * Scale the weights so they sum up to WEIGHTONE, and trim leading and trailing
	 * zeros if trimZeros is true.
	 */
#undef  NORMALIZE_SUMMED_PEAK
#define NORMALIZE_MAXXED_PEAK
	for (dstPosition = dstFirst, pm = false; dstPosition < dstLast; dstPosition++)
	{
		double srcCenter = MAP(dstPosition, s, u);

		/* find the source coordinate range of this positioned filter window */
		int i0 = int(floor(srcCenter - srcRadius + 0.5));
		int i1 = int(floor(srcCenter + srcRadius + 0.5));

		/* clip against the source-range */
		if (i0 < srcFirst)
		{
			i0 = srcFirst;
		}
		if (i1 > srcLast)
		{
			i1 = srcLast;
		}

		/* this is possible if we hit the final line */
		if (i1 <= i0)
		{
			if (i1 >= srcLast)
			{
				i0 = i1 - 1;
			}
			else
			{
				i1 = i0 + 1;
			}
		}

		assert(i0 >= srcFirst);
		assert(i1 <= srcLast);
		assert(i0 < i1);

		/* find maximum peak to normalize the filter */
		for (sumfWeights = 0, pos = 0, neg = 0, i = i0; i < i1; i++)
		{
			/* evaluate the filter function: */
			fWeight = (*windowFunction)((i + 0.5 - srcCenter) / scaleFactor);

#if   defined(NORMALIZE_SUMMED_PEAK)
			/* get positive and negative summed peaks */
			if (fWeight >= 0)
			{
				pos += fWeight;
			}
			else
			{
				neg += fWeight;
			}
#elif defined(NORMALIZE_MAXXED_PEAK)
			/* get positive and negative maximum peaks */
			minmax(fWeight, neg, pos);
#endif

			sumfWeights += fWeight;
		}

		/* the range of source samples to buffer: */
		weightsMem = new signed short int[(i1 - i0) * abs(numRepetitions)];

		/* set nrmWeights so that sumWeights of windowFunction() is approximately WEIGHTONE
		 * this needs to be adjusted because the maximum weight-coefficient
		 * is NOT allowed to leave [-32768,32767]
		 * a case like {+1.25,-0.25} does produce a sumWeights of 1.0 BUT
		 * produced a weight much too high (-40000)
		 */
#if   defined(NORMALIZE_SUMMED_PEAK)
		sumfWeights =                      maximum(-neg, pos) ;
#elif defined(NORMALIZE_MAXXED_PEAK)
		sumfWeights = maximum(sumfWeights, maximum(-neg, pos));
#endif

		if (!peaknorm)
		{
			nrmWeights = (sumfWeights == 0. ? WEIGHTONE : (-neg > pos ? WEIGHTONE - 1 : WEIGHTONE) / sumfWeights);
		}
		else
		{
			nrmWeights = (sumfWeights == 0. ? WEIGHTONE : (-neg > pos ? WEIGHTONE - 1 : WEIGHTONE) / maximum(-neg, pos));
		}

		/* compute the discrete, sampled filter coefficients */
		bool stillzero = trimZeros;
		for (sumiWeights = 0, hWeight = -WEIGHTONE, weightsPtr = weightsMem, i = i0; i < i1; i++)
		{
			/* evaluate the filter function: */
			fWeight = (*windowFunction)((i + 0.5 - srcCenter) / scaleFactor);

			/* normalize against the peak sumWeights, because the sums are not allowed to leave -32768/32767 */
			fWeight = fWeight * nrmWeights;
			iWeight = roundToInt(fWeight);

			/* find first nonzero */
			if (stillzero && (iWeight == 0))
			{
				i0++;
			}
			else
			{
				assert((-fWeight >= -32768.5) && (-fWeight <= 32767.5));

				if (!peaknorm)
				{
					sumiWeights += iWeight;
				}
				else
				{
					sumiWeights  = maximum(sumiWeights, iWeight);
				}

#define sgnextend(n, iWeight)   (((n) & 1) ? ((iWeight) < 0 ? -1 : 0) : (iWeight))
				if (numRepetitions < 0)
				{
					/* add weight to table, interleaved sign */
					for (n = 0; n < -numRepetitions; n++)
					{
						*weightsPtr++ = sgnextend(n, -iWeight);
					}
				}
				else
				{
					/* add weight to table */
					for (n = 0; n <  numRepetitions; n++)
					{
						*weightsPtr++ =              -iWeight;
					}
				}

				stillzero = false;

				/* find last nonzero */
				if (iWeight != 0)
				{
					lastnonzero = i;
				}

				/* check for negative values */
				if (iWeight < 0)
				{
					pm = pma = true;
				}

				/* find most influential value */
				if (iWeight >= hWeight)
				{
					highest = i;
					hWeight = iWeight;
				}
			}
		}

		if (sumiWeights == 0)
		{
			i0 = (i0 + i1) >> 1;
			i1 = (i0 +  1);

			for (n = 0, weightsPtr = weightsMem; n < numRepetitions; n++)
			{
				*weightsPtr++ = -WEIGHTONE;
			}
		}
		else
		{
			/* skip leading and trailing zeros */
			if (trimZeros)
			{
				/* set i0 and i1 to the nonzero support of the filter (note: i0 stays the same) */
				i1 = lastnonzero + 1;
			}

			if (sumiWeights != WEIGHTONE)
			{
				/* Fudge with the highest value */
				i = highest;

				/* fudge srcCenter sample */
				iWeight = WEIGHTONE - sumiWeights;

				for (n = 0, weightsPtr = weightsMem + (i - i0) * numRepetitions; n < numRepetitions; n++)
				{
					*weightsPtr++ -= iWeight;
				}
			}
		}

		/* the new adjusted range of source samples to buffer: */
		weightsObjs[dstPosition].first     = i0;
		weightsObjs[dstPosition].last      = i1;
		weightsObjs[dstPosition].hasNegativeWeights = pm;
		weightsObjs[dstPosition].weights   = weightsMem;
	}

	plusminus = pma;
	return weightsObjs;
}
