// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<typename TValue, typename TAllocator>
void RadixSort(
  /*OUT*/ uint32* pRanksBegin, uint32* pRanksEnd,
  /*IN*/ const TValue* pValuesBegin, const TValue* pValuesEnd,
  /*IN*/ TAllocator& allocator = TAllocator());

#include "RadixSortImpl.h"
