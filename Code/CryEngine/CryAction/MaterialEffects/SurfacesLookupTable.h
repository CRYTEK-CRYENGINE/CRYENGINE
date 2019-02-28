// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// ----------------------------------------------------------------------------------------
//  File name:   SurfacesLookupTable.h
//  Description: 2D look up table for surfaces
//
////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _SURFACES_LOOKUP_TABLE_H_
#define _SURFACES_LOOKUP_TABLE_H_

#pragma once

namespace MaterialEffectsUtils
{
template<typename T>
struct SSurfacesLookupTable
{
	void Create(size_t rows, size_t cols)
	{
		SAFE_DELETE_ARRAY(m_pData);

		m_nRows = rows;
		m_nCols = cols;
		m_pData = new T[m_nRows * m_nCols];

		memset(m_pData, 0, m_nRows * m_nCols * sizeof(T));
	}

	SSurfacesLookupTable()
		: m_pData(0)
		, m_nRows(0)
		, m_nCols(0)
	{
	}

	~SSurfacesLookupTable()
	{
		SAFE_DELETE_ARRAY(m_pData);
	}

	void Free()
	{
		SAFE_DELETE_ARRAY(m_pData);
	}

	T* operator[](const size_t row) const
	{
		CRY_ASSERT(m_pData);
		CRY_ASSERT((row >= 0) && (row < m_nRows));
		return &m_pData[row * m_nCols];
	}

	T& operator()(const size_t row, const size_t col) const
	{
		CRY_ASSERT(m_pData);
		CRY_ASSERT((row >= 0) && (row < m_nRows) && (col >= 0) && (col < m_nCols));
		return m_pData[row * m_nCols + col];
	}

	T GetValue(const size_t row, const size_t col, const T& defaultValue) const
	{
		return (m_pData && row >= 0 && row < m_nRows && col >= 0 && col < m_nCols) ? m_pData[row * m_nCols + col] : defaultValue;
	}
	void GetMemoryUsage(ICrySizer* s) const
	{
		s->AddObject(m_pData, sizeof(T) * m_nRows * m_nCols);
	}

	T*     m_pData;
	size_t m_nRows;
	size_t m_nCols;
};
}

#endif //_SURFACES_LOOKUP_TABLE_H_
