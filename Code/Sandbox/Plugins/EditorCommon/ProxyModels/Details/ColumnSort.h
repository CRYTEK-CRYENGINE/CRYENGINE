// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <Qt>

struct CColumnSort
{
	inline CColumnSort()
		: m_column(-1)
		, m_order(Qt::AscendingOrder)
	{}

	inline CColumnSort(int column, Qt::SortOrder order)
		: m_column(column)
		, m_order(order)
	{}

	inline bool          isValid() const { return 0 <= m_column; }

	inline int           column() const  { return m_column; }
	inline Qt::SortOrder order() const   { return m_order; }

	inline bool          operator==(const CColumnSort& other)
	{
		return (m_column == other.m_column) && (m_order == other.m_order);
	}

	inline bool operator!=(const CColumnSort& other)
	{
		return !(*this == other);
	}

private:
	int           m_column;
	Qt::SortOrder m_order;
};

