// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  primitive curve integrators used for delta state
               compression on an arithmetic stream
   -------------------------------------------------------------------------
   History:
   - 02/09/2004   12:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __ARITHPRIMITIVES_H__
#define __ARITHPRIMITIVES_H__

#pragma once

#include "Config.h"

#if USE_MEMENTO_PREDICTORS

	#include "Streams/CommStream.h"

void SquarePulseProbabilityWrite(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit = NULL
  );
uint32 SquarePulseProbabilityRead(
  CCommInputStream& stm,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit = NULL
  );
float SquarePulseProbabilityEstimate(
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange
  );

void HalfSquareProbabilityWrite(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nDropPoint,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 recurseLimit = 5
  );
float HalfSquareProbabilityEstimate(
  uint32 nValue,
  uint32 nDropPoint,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 recurseLimit = 5
  );
bool HalfSquareProbabilityRead(
  uint32& value,
  CCommInputStream& stm,
  uint32 nDropPoint,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePercentage,
  uint8 recurseLimit = 5
  );

void SquarePulseProbabilityWriteImproved(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePrecentage,
  uint8 nMaxBits,
  bool* bHit = NULL
  );
bool SquarePulseProbabilityReadImproved(
  uint32& value,
  CCommInputStream& stm,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePrecentage,
  uint8 nMaxBits,
  bool* bHit = NULL
  );
float SquarePulseProbabilityEstimateImproved(
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  uint8 nInRangePrecentage,
  uint8 nMaxBits
  );

void ThreeSquarePulseProbabilityWrite(
  CCommOutputStream& stm,
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit = NULL
  );
uint32 ThreeSquarePulseProbabilityRead(
  CCommInputStream& stm,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange,
  bool* bHit = NULL
  );
float ThreeSquarePulseProbabilityEstimate(
  uint32 nValue,
  uint32 nLeft,
  uint32 nRight,
  uint32 nHeight,
  uint32 nRange
  );

void CliffProbabilityWrite(
  CCommOutputStream& stm,
  int32 nValue,
  int32 nLowPoint,
  int32 nHighPoint,
  uint16 nHeight,
  int32 nRange
  );
float CliffProbabilityEstimate(
  int32 nValue,
  int32 nLowPoint,
  int32 nHighPoint,
  uint16 nHeight,
  int32 nRange
  );
int32 CliffProbabilityRead(
  CCommOutputStream& stm,
  int32 nLowPoint,
  int32 nHighPoint,
  uint16 nHeight,
  int32 nRange
  );

#endif

#endif
