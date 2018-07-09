// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  primitive curves for delta state compression
   -------------------------------------------------------------------------
   History:
   - 02/09/2004   12:34 : Created by Craig Tiller
   - 14/11/2005		10:45 : 32 Bit probability implementation by Jan Müller
*************************************************************************/
#include "StdAfx.h"
#include "ArithPrimitives.h"

#if USE_MEMENTO_PREDICTORS
// this function scales nValue0, nValue1 such that their sum is
// less than 65536
static ILINE uint32 ScaleBinaryValue(
  uint32& nValue0,
  uint32& nValue1)
{
	uint32 nSum = nValue0 + nValue1;
	if (nSum > 65536)
	{
		int nSumBits = IntegerLog2(nSum);
		NET_ASSERT(nSumBits >= 16);
		int nShiftBits = nSumBits - 15;
		uint32 nLow = 1 << nShiftBits;
		nValue0 = (nValue0 >> nShiftBits) + (nValue0 < nLow);
		nValue1 = (nValue1 >> nShiftBits) + (nValue1 < nLow);
		nSum = nValue0 + nValue1;
	}
	NET_ASSERT(nSum <= 65536);
	return nSum;
}

// this function scales nValue0, nValue1, nValue2 such that their sum is
// less than 65536
static ILINE uint32 ScaleTernaryValue(
  uint32& nValue0,
  uint32& nValue1,
  uint32& nValue2)
{
	uint32 nSum = nValue0 + nValue1 + nValue2;
	if (nSum > 65536)
	{
		int nSumBits = IntegerLog2(nSum);
		NET_ASSERT(nSumBits >= 16);
		int nShiftBits = nSumBits - 15;
		uint32 nLow = 1 << nShiftBits;
		nValue0 = (nValue0 >> nShiftBits) + (nValue0 < nLow);
		nValue1 = (nValue1 >> nShiftBits) + (nValue1 < nLow);
		nValue2 = (nValue2 >> nShiftBits) + (nValue2 < nLow);
		nSum = nValue0 + nValue1 + nValue2;
	}
	NET_ASSERT(nSum <= 65536);
	return nSum;
}

static ILINE uint32 VerifyHeight(
  uint32 nHeight,
  uint32 nLeft,
  uint32 nRight)
{
	if ((uint64(nHeight) * (nRight - nLeft)) >= (uint64(1) << 32))
	{
		nHeight = static_cast<uint32>((uint64(1) << 32) / (nRight - nLeft));
		NET_ASSERT(nHeight != 0);
	}

	return nHeight;
}

/*
 * SECTION: SquareWaveProbability
 *
 * This section provides a model for the distribution of numbers that looks
 * like a square pulse:
 *
 *          nCenter -+
 *                   v
 *              +---------+                ^
 *              |         |                | nHeight
 *              |         |                |
 * -------------+         +-------         v
 * 0                             nRange
 *              <--------->
 *               2*nRadius
 *
 * We expect nValue to be between nCenter-nRadius and nCenter+nRadius
 */

// this structure assists the calculation of square wave probability curves
struct SSquareWaveProbabilityCalculation
{
	int32          nCenter;
	int32          nRadius;
	uint32         nRange;
	int32          nLeft;
	int32          nRight;
	uint32         nWeight0;
	uint32         nWeight1;
	uint32         nWeight2;
	int32          nOffset;
	uint32         nTot;
	uint32         nLow;
	uint32         nSym;
	uint32         nHeight;
	bool           bHit;

	ILINE unsigned GetCode()
	{
		return
		  // bit 0: does the pulse extend past the left
		  (nCenter < nRadius) |
		  // bit 1: does the pulse extend past the right
		  ((nCenter + nRadius > (int)nRange) << 1) |
		  // bit 2: does the pulse exist completely outside of the range
		  (((int)nRange + nRadius < nCenter || nCenter + nRadius < 0) << 2);
	}

	ILINE void Values0()
	{
		nLeft = nCenter - nRadius;
		nRight = nCenter + nRadius;
		nWeight0 = 1;
		nWeight1 = nHeight;
		nWeight2 = 1;
		nTot = ScaleTernaryValue(nWeight0, nWeight1, nWeight2);
	}

	ILINE void Prob0(uint32 nDecide, uint32 nDecideLeft, uint32 nDecideRight)
	{
		if (nDecide < nDecideLeft)
		{
			nSym = nWeight0;
			nLow = 0;
			nRange = nLeft;
			nOffset = 0;
			bHit = false;
		}
		else if (nDecide < nDecideRight)
		{
			nSym = nWeight1;
			nLow = nWeight0;
			nRange = 2 * nRadius;
			nOffset = nLeft;
			bHit = true;
		}
		else
		{
			nSym = nWeight2;
			nLow = nWeight0 + nWeight1;
			nRange -= nRight;
			nOffset = nRight;
			bHit = false;
		}
	}

	ILINE void Values1()
	{
		nLeft = nCenter + nRadius;
		nWeight0 = nHeight;
		nWeight1 = 1;
		nTot = ScaleBinaryValue(nWeight0, nWeight1);
	}

	ILINE void Prob1(uint32 nDecide, uint32 nDecideLeft)
	{
		if (nDecide < nDecideLeft)
		{
			nSym = nWeight0;
			nLow = 0;
			nRange = nLeft;
			nOffset = 0;
			bHit = true;
		}
		else
		{
			nSym = nWeight1;
			nLow = nWeight0;
			nRange -= nLeft;
			nOffset = nLeft;
			bHit = false;
		}
	}

	ILINE void Values2()
	{
		nLeft = nCenter - nRadius;
		nWeight0 = 1;
		nWeight1 = nHeight;
		nTot = ScaleBinaryValue(nWeight0, nWeight1);
	}

	ILINE void Prob2(uint32 nDecide, uint32 nDecideLeft)
	{
		if (nDecide < nDecideLeft)
		{
			nSym = nWeight0;
			nLow = 0;
			nRange = nLeft;
			nOffset = 0;
			bHit = false;
		}
		else
		{
			nSym = nWeight1;
			nLow = nWeight0;
			nRange -= nLeft;
			nOffset = nLeft;
			bHit = true;
		}
	}
};

void HalfSquareProbabilityWrite(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nDropPoint,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 recurseLimit)
{
	NET_ASSERT(nRange >= nDropPoint);
	NET_ASSERT(nValue < nRange);
	// encode hit or miss
	bool hit = nValue < nDropPoint;
	if (recurseLimit)
	{
		if (hit)
			stm.EncodeShift(7, 0, nInRangePercentage);
		else
			stm.EncodeShift(7, nInRangePercentage, 128 - nInRangePercentage);
	}
	else
	{
		nDropPoint = nRange;
	}
	if (hit || !recurseLimit)
	{
		stm.Encode(nDropPoint, nValue, 1);
	}
	else if (recurseLimit)
	{
		HalfSquareProbabilityWrite(
		  stm,
		  nValue - nDropPoint,
		  std::min(nDropPoint * 2, uint32(nRange - nDropPoint)),
		  std::max(1u, uint32(nHeight / 2)),
		  nRange - nDropPoint,
		  nInRangePercentage,
		  recurseLimit - 1);
	}
}

bool HalfSquareProbabilityRead(
  uint32& value,
  CCommInputStream& stm,
  uint32 nDropPoint,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 recurseLimit)
{
	NET_ASSERT(nRange >= nDropPoint);
	bool hit = !recurseLimit;
	if (!hit)
	{
		hit = (stm.DecodeShift(7) < nInRangePercentage) ? true : false;

		if (hit)
			stm.UpdateShift(7, 0, nInRangePercentage);
		else
			stm.UpdateShift(7, nInRangePercentage, 128 - nInRangePercentage);
	}
	else
	{
		nDropPoint = nRange;
	}
	if (hit)
	{
		if (!nDropPoint)
			return false;
		value = stm.Decode(nDropPoint);
		stm.Update(nDropPoint, value, 1);
		return true;
	}
	else
	{
		uint32 tail;
		if (!HalfSquareProbabilityRead(
		      tail,
		      stm,
		      std::min(nDropPoint * 2, uint32(nRange - nDropPoint)),
		      std::max(1u, uint32(nHeight / 2)),
		      nRange - nDropPoint,
		      nInRangePercentage,
		      recurseLimit - 1))
			return false;
		value = nDropPoint + tail;
		return true;
	}
}

/************************************************************************/
/* This function writes nValue to the stream for which a range was
   predicted where the value could probably be in.
   If the prediction was correct, only that range has to be transmitted.
   If it was not, the range is increased recursively until I either give up or hit.
   /************************************************************************/
void SquarePulseProbabilityWriteImproved(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 nMaxBits,
  bool* bHit)
{
	//debug
	NET_ASSERT(nLeft <= nRight);
	NET_ASSERT(nRange > 0);
	NET_ASSERT(nLeft >= 0);
	NET_ASSERT(nRight <= nRange);

	nHeight = VerifyHeight(nHeight, nLeft, nRight);

	//most important: was the value correctly predicted?
	bool isInRange = (nValue >= nLeft && nValue <= nRight) ? true : false;

	//sym is nHeight if the value is inside the pulse, 1 else
	//low is the summed probabilities of all values beLOW the value
	//tot is all summed probabilities
	uint32 tot = 0;
	uint32 low = 0;
	uint32 sym = 0;

	if (bHit)
		*bHit = isInRange;

	//submit whether the value was inRange
	if (isInRange)
		stm.EncodeShift(7, 0, nInRangePercentage);
	else
		stm.EncodeShift(7, nInRangePercentage, 128 - nInRangePercentage);

	//if inRange - transmit only the area or the single value
	if (isInRange)
	{
		if (nRight - nLeft < 3)  //if we know the value exactly - send it
		{
			sym = 1;
			tot = ((nRight + 1) - nLeft);
			low = (nValue - nLeft);

			NET_ASSERT(low < tot);
			NET_ASSERT(tot > 0);

			stm.Encode(tot, low, sym);
		}
		else  //if we have predicted a larger area, this can additionally be compressed by giving centric values a higher probability
		{
			uint32 valueRange = nRight - nLeft;
			uint32 value = nValue;
			ThreeSquarePulseProbabilityWrite(stm, value - nLeft, 0, valueRange, nHeight, valueRange + 1, bHit);
		}
	}
	else if (nRight == nLeft) //we have no idea - probably start of prediction ?
	{
		stm.WriteBits(nValue, nMaxBits);
	}
	else  //the range was not predicted correctly
	{
		uint32 midPoint = (nLeft + nRight) / 2;
		bool isRight = nValue >= midPoint || !nLeft;
		stm.WriteBits(isRight, 1);
		if (isRight)
			HalfSquareProbabilityWrite(stm, nValue - nRight, std::min(nRight - midPoint, nRange + 1 - nRight), std::max(1u, nHeight / 2), nRange + 1 - nRight, nInRangePercentage, 4);
		else
		{
			NET_ASSERT(nLeft);
			HalfSquareProbabilityWrite(stm, nLeft - nValue - 1, std::min(uint32(nRight - midPoint), nLeft), std::max(1u, nHeight / 2), nLeft, nInRangePercentage, 4);
		}
	}
}

//see write - routine for comments (reading is just a reflection of writing)
bool SquarePulseProbabilityReadImproved(
  uint32& value,
  CCommInputStream& stm,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 nMaxBits,
  bool* bHit)
{
	//debug
	NET_ASSERT(nLeft <= nRight);
	NET_ASSERT(nRange > 0);
	NET_ASSERT(nLeft >= 0);
	NET_ASSERT(nRight <= nRange);

	bool isInRange = (stm.DecodeShift(7) < nInRangePercentage) ? true : false;

	if (isInRange)
		stm.UpdateShift(7, 0, nInRangePercentage);
	else
		stm.UpdateShift(7, nInRangePercentage, 128 - nInRangePercentage);

	uint32 low = 0;
	uint32 sym = 0;
	value = 0;
	uint32 tot = 0;

	if (bHit)
		*bHit = false;

	if (isInRange)
	{
		if (nRight - nLeft < 3)
		{
			tot = ((nRight + 1) - nLeft);
			low = stm.Decode(tot);
			sym = 1;

			value = nLeft + low;

			if (bHit)
				*bHit = true;

			stm.Update(tot, low, sym);
		}
		else
		{
			uint32 valueRange = nRight - nLeft;
			value = nLeft + ThreeSquarePulseProbabilityRead(stm, 0, valueRange, nHeight, valueRange + 1, bHit);
		}
	}
	else if (nRight == nLeft) //we have no idea - probably start of prediction ?
	{
		value = stm.ReadBits(nMaxBits);
	}
	else
	{
		uint32 midPoint = (nLeft + nRight) / 2;
		bool isRight = stm.ReadBits(1) != 0;
		if (isRight)
		{
			uint32 tail;
			if (!HalfSquareProbabilityRead(tail, stm, std::min(nRight - midPoint, nRange + 1 - nRight), std::max(1u, nHeight / 2), nRange + 1 - nRight, nInRangePercentage, 4))
				return false;
			value = nRight + tail;
		}
		else
		{
			if (!nLeft)
			{
				NET_ASSERT(false);
				return false;
			}
			uint32 tail;
			if (!HalfSquareProbabilityRead(tail, stm, std::min(uint32(nRight - midPoint), nLeft), std::max(1u, nHeight / 2), nLeft, nInRangePercentage, 4))
				return false;
			value = nLeft - tail - 1;
		}
	}

	return true;
}

//traditional probability coding (basic and unreliable)
void SquarePulseProbabilityWrite(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit)
{

	//debug
	NET_ASSERT(nLeft <= nRight);
	NET_ASSERT(nRange > 0);
	NET_ASSERT(nLeft >= 0);
	NET_ASSERT(nRight <= nRange);

	nHeight = VerifyHeight(nHeight, nLeft, nRight);

	//sym is nHeight if the value is inside the pulse, 1 else
	uint32 sym = (nValue >= nLeft && nValue <= nRight) ? nHeight : 1;

	uint32 low = 0;

	if (bHit)
		*bHit = false;

	//low - summed probabilities of all predecessors
	if (nValue < nLeft)
		low = nValue;
	else if (nValue <= nRight)
	{
		low = nLeft + (nValue - nLeft) * nHeight;
		if (bHit)
			*bHit = true;
	}
	else
		low = nLeft + ((nRight - nLeft + 1) * nHeight) + (nValue - nRight - 1);

	//tot - summed probabilities
	uint32 tot = nLeft + (((nRight + 1) - nLeft) * nHeight) + (nRange - nRight);

	NET_ASSERT(tot >= low);

	stm.Encode(tot, low, sym);
}

float SquarePulseProbabilityEstimate(
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange)
{
	uint32 sym = (nValue >= nLeft && nValue <= nRight) ? nHeight : 1;
	uint32 tot = nLeft + (((nRight + 1) - nLeft) * nHeight) + (nRange - nRight);

	return CCommOutputStream::EstimateArithSizeInBits(tot, sym);
}

uint32 SquarePulseProbabilityRead(
  CCommInputStream& stm,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit)
{
	//debug
	NET_ASSERT(nLeft <= nRight);
	NET_ASSERT(nRange > 0);
	NET_ASSERT(nLeft >= 0);
	NET_ASSERT(nRight <= nRange);

	uint32 tot = nLeft + (((nRight + 1) - nLeft) * nHeight) + (nRange - nRight);

	uint32 low = stm.Decode(tot);
	uint32 sym = 0;
	uint32 value = 0;

	if (bHit)
		*bHit = false;

	if (low < nLeft)
	{
		value = low;
		sym = 1;
	}
	else if (low < nLeft + ((nRight - nLeft) + 1) * nHeight)
	{
		int32 tempLow = low - nLeft;
		tempLow = tempLow % nHeight;
		low -= tempLow;
		value = nLeft + ((low - nLeft) / (uint32)nHeight);
		sym = nHeight;

		if (bHit)
			*bHit = true;
	}
	else
	{
		value = low - ((nRight - nLeft) + 1) * (nHeight - 1);
		sym = 1;
	}

	stm.Update(tot, low, sym);

	return value;
}

/**
 * This function writes nValue to the stream by simplifying its probability.
 * The predicted probability curve is approximated by 3 rectangles.
 */

const uint32 MEDHEIGHTDIV = 4;
const uint32 MAXHEIGHTRANGEDIV = 2;

void ThreeSquarePulseProbabilityWrite(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit)
{
	NET_ASSERT(nLeft <= nRight);
	NET_ASSERT(nLeft >= 0);
	NET_ASSERT(nRight <= nRange);

	nHeight = VerifyHeight(nHeight, nLeft, nRight);

	// determine x% of the total delta to be the maximum height
	// and the rest to be y% of the maximum height
	// (percentages determined by empiric evaluation)
	uint32 maxHeightDelta = (nRight - nLeft + 1) / MAXHEIGHTRANGEDIV;  // 65% of total delta
	uint32 medHeightDelta = ((nRight - nLeft + 1) - maxHeightDelta) / (uint32)2;
	uint32 medHeight = nHeight / MEDHEIGHTDIV; // 35% of height
	if (!medHeight)
		medHeight = 1;

	// Truncation could make the summed delta smaller than the original delta. Error correction:
	maxHeightDelta += (nRight - nLeft + 1) - maxHeightDelta - 2 * medHeightDelta;

	// tot = total accumulated probabilities
	uint32 tot = nLeft + 2 * medHeightDelta * medHeight + maxHeightDelta * nHeight + (nRange - nRight - 1);

	// calculate height and low according to position
	uint32 sym = 1;
	uint32 low = 0;
	if (nValue >= nLeft && nValue <= nRight)
	{
		if (bHit)
			*bHit = true;

		// if nValue is inside the delta, determine which block it's in
		if (nValue < nLeft + medHeightDelta)
		{
			// nValue is in the lower medium square
			sym = medHeight;
			low = nLeft + (nValue - nLeft) * medHeight;
		}
		else if (nValue > nRight - medHeightDelta)
		{
			// nValue is in the upper medium square
			sym = medHeight;
			low = nLeft + medHeightDelta * medHeight + maxHeightDelta * nHeight + (nValue - (nRight - medHeightDelta) - 1) * medHeight;
		}
		else
		{
			// nValue is inside the maxHeight square
			sym = nHeight;
			low = nLeft + medHeightDelta * medHeight + (nValue - (nLeft + medHeightDelta)) * nHeight;
		}
	}
	else
	{
		if (bHit)
			*bHit = false;

		if (nValue > nRight)
		{
			low = nLeft + 2 * medHeightDelta * medHeight + maxHeightDelta * nHeight + (nValue - nRight - 1);
		}
		else
			low = nValue;
	}

	stm.Encode(tot, low, sym);
}

float ThreeSquarePulseProbabilityEstimate(
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange)
{
	uint32 maxHeightDelta = (nRight - nLeft + 1) / MAXHEIGHTRANGEDIV;  // 65% of total delta
	uint32 medHeightDelta = ((nRight - nLeft + 1) - maxHeightDelta) / (uint32)2;
	uint32 medHeight = nHeight / MEDHEIGHTDIV; // 35% of height
	if (!medHeight)
		medHeight = 1;

	// Truncation could make the summed delta smaller than the original delta. Error correction:
	maxHeightDelta += (nRight - nLeft + 1) - maxHeightDelta - 2 * medHeightDelta;

	// calculate total accumulated probabilities
	uint32 tot = nLeft + 2 * medHeightDelta * medHeight + maxHeightDelta * nHeight + (nRange - nRight - 1);
	uint32 sym = 1;
	if (nValue >= nLeft && nValue <= nRight)
	{
		// if nValue is inside the delta, determine which block it's in
		if ((nValue < nLeft + medHeightDelta) || (nValue > nRight - medHeightDelta))
		{
			// nValue is in the lower medium square
			sym = medHeight;
		}
		else
		{
			// nValue is inside the maxHeight square
			sym = nHeight;
		}
	}

	return CCommOutputStream::EstimateArithSizeInBits(tot, sym);
}

/**
 * Reads and returns a value from the stream by it's probability.
 * The probability curve is approximated as in ::ThreeSquarePulseProbabilityWrite()
 * @see ThreeSquarePulseProbabilityWrite()
 */
uint32 ThreeSquarePulseProbabilityRead(
  CCommInputStream& stm,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit)
{
	NET_ASSERT(nLeft <= nRight);
	//NET_ASSERT ( nRange > 0 );
	NET_ASSERT(nLeft >= 0);
	NET_ASSERT(nRight <= nRange);

	nHeight = VerifyHeight(nHeight, nLeft, nRight);

	// determine x% of the total delta to be the maximum height
	// and the rest to be y% of the maximum height
	// (percentages determined by empiric evaluation)
	uint32 maxHeightDelta = (nRight - nLeft + 1) / MAXHEIGHTRANGEDIV;  // 65% of total delta
	uint32 medHeightDelta = ((nRight - nLeft + 1) - maxHeightDelta) / (uint32)2;
	uint32 medHeight = nHeight / MEDHEIGHTDIV; // 35% of height
	if (!medHeight)
		medHeight = 1;

	// Truncation could make the summed delta smaller than the original delta. Error correction:
	maxHeightDelta += (nRight - nLeft + 1) - maxHeightDelta - 2 * medHeightDelta;

	// calculate total accumulated probabilities
	uint32 tot = nLeft + 2 * medHeightDelta * medHeight + maxHeightDelta * nHeight + (nRange - nRight - 1);

	uint32 low = stm.Decode(tot);
	uint32 sym = 0;
	uint32 value = 0;

	if (bHit)
		*bHit = false;

	// determine the rectangle the value is in
	if (low < nLeft)
	{
		sym = 1;
		value = low;
	}
	else if (low < nLeft + medHeightDelta * medHeight)
	{
		sym = medHeight;
		uint32 tempLow = low - nLeft;
		tempLow = tempLow % medHeight;
		low -= tempLow;
		value = nLeft + ((low - nLeft) / medHeight);
	}
	else if (low < nLeft + medHeightDelta * medHeight + maxHeightDelta * nHeight)
	{
		sym = nHeight;
		uint32 tempLow = low - nLeft - medHeightDelta * medHeight;
		tempLow = tempLow % (uint32)nHeight;
		low -= tempLow;
		value = nLeft + medHeightDelta + uint32((low - nLeft - medHeightDelta * medHeight) / nHeight);
	}
	else if (low < nLeft + 2 * medHeightDelta * medHeight + maxHeightDelta * nHeight)
	{
		sym = medHeight;
		uint32 tempLow = low - nLeft - medHeightDelta * medHeight - maxHeightDelta * nHeight;
		tempLow = tempLow % medHeight;
		low -= tempLow;
		value = nLeft + medHeightDelta + maxHeightDelta + uint32((low - nLeft - medHeightDelta * medHeight - maxHeightDelta * nHeight) / medHeight);
	}
	else
	{
		sym = 1;
		value = nRight + 1 + low - nLeft - 2 * medHeightDelta * medHeight - maxHeightDelta * nHeight;
	}

	stm.Update(tot, low, sym);

	return value;
}
#endif
