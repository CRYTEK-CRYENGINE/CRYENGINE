// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace Helper
{

template<class T>
T* FindFromSorted(T* pValues, uint32 count, T& valueToFind)
{
	T* pLast = &pValues[count];
	T* pValue = std::lower_bound(pValues, pLast, valueToFind);
	if (pValue == pLast || *pValue != valueToFind)
		return NULL;

	return pValue;
}

template<class T>
T* FindFromSorted(std::vector<T>& values, const T& valueToFind)
{
	return FindFromSorted(&values[0], values.size(), valueToFind);
}

} // namespace Helper
