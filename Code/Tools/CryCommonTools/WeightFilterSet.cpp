// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <assert.h>										// assert()
#include "weightfilterset.h"					// CWeightFilterSet



void CWeightFilterSet::FreeData()
{
	m_FilterKernelBlock.FreeData();
}


bool CWeightFilterSet::Create( const unsigned long indwSideLength, const CSummedAreaFilterKernel &inFilter, const float infR )
{
	assert(indwSideLength>=1);

	FreeData();

	// 32 Baustelle
	inFilter.CreateWeightFilterBlock(m_FilterKernelBlock,1,infR*indwSideLength);
	return(true);
}

