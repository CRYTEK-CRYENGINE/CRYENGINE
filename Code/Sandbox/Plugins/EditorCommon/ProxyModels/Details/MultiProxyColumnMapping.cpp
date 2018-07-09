// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "MultiProxyColumnMapping.h"

CMultiProxyColumnMapping::CMultiProxyColumnMapping(const ColumnValues& columnValues, int valueRole)
{
	Reset(columnValues, valueRole);
}

void CMultiProxyColumnMapping::UpdateMountData(const QAbstractItemModel* sourceModel, CMultiProxyColumnMapping::MountData& mountData) const
{
	auto sourceColumnCount = sourceModel->columnCount();

	mountData.m_proxyToSource.fill(-1, m_proxyColumns.size());
	mountData.m_sourceToProxy.resize(sourceColumnCount);

	for (auto sourceColumn = 0; sourceColumn < sourceColumnCount; ++sourceColumn)
	{
		auto value = sourceModel->headerData(sourceColumn, Qt::Horizontal, m_valueRole);

		auto it = m_valueColumn.find(value);
		if (it != m_valueColumn.end())
		{
			auto proxyColumn = it.value();
			mountData.m_proxyToSource[proxyColumn] = sourceColumn;
			mountData.m_sourceToProxy[sourceColumn] = proxyColumn;
		}
		else
		{
			mountData.m_sourceToProxy[sourceColumn] = -1; // mark as not mapped
		}
	}
}

void CMultiProxyColumnMapping::Reset(const ColumnValues& columnValues, int valueRole)
{
	m_valueRole = valueRole;
	auto proxyColumnCount = columnValues.size();

	m_proxyColumns.clear();
	m_proxyColumns.reserve(proxyColumnCount);

	for (auto proxyColumn = 0; proxyColumn < proxyColumnCount; ++proxyColumn)
	{
		auto value = columnValues[proxyColumn];

		auto columnIt = m_valueColumn.insert(value, proxyColumn);
		m_proxyColumns << columnIt;
	}
}

void CMultiProxyColumnMapping::InsertColumns(int firstProxyColumn, const ColumnValues& columnValues)
{
	CRY_ASSERT(firstProxyColumn >= 0 && firstProxyColumn <= m_proxyColumns.size());
	if (columnValues.isEmpty())
	{
		return; // nothing to insert
	}
	auto oldProxyColumnCount = m_proxyColumns.size();
	auto difference = columnValues.size();
	auto newProxyColumnCount = oldProxyColumnCount + difference;
	m_proxyColumns.resize(newProxyColumnCount);

	// make space for new columns
	for (auto oldProxyColumn = oldProxyColumnCount - 1; oldProxyColumn >= firstProxyColumn; --oldProxyColumn)
	{
		auto columnIt = m_proxyColumns[oldProxyColumn];
		auto newProxyColumn = oldProxyColumn + difference;
		columnIt.value() = newProxyColumn;
		m_proxyColumns[newProxyColumn] = columnIt;
	}

	// insert new columns
	for (auto newIndex = 0; newIndex < difference; ++newIndex)
	{
		auto value = columnValues[newIndex];
		auto proxyColumn = firstProxyColumn + newIndex;

		auto columnIt = m_valueColumn.insert(value, proxyColumn);
		m_proxyColumns[proxyColumn] = columnIt;
	}
}

void CMultiProxyColumnMapping::RemoveColumns(int firstProxyColumn, int count)
{
	CRY_ASSERT(firstProxyColumn >= 0);
	CRY_ASSERT(count >= 0);
	CRY_ASSERT(firstProxyColumn + count < m_proxyColumns.size());
	if (0 <= count)
	{
		return; // nothing to remove
	}

	// remove values
	for (auto proxyColumn = firstProxyColumn; proxyColumn < firstProxyColumn + count; ++proxyColumn)
	{
		auto columnIt = m_proxyColumns[proxyColumn];
		m_valueColumn.erase(columnIt);
	}

	// move values
	auto oldProxyColumnCount = m_proxyColumns.size();
	for (auto oldProxyColumn = firstProxyColumn + count; oldProxyColumn < oldProxyColumnCount; ++oldProxyColumn)
	{
		auto columnIt = m_proxyColumns[oldProxyColumn];
		auto newProxyColumn = oldProxyColumn - count;
		columnIt.value() = newProxyColumn;
		m_proxyColumns[newProxyColumn] = columnIt;
	}

	// cut array
	auto newProxyColumnCount = oldProxyColumnCount - count;
	m_proxyColumns.resize(newProxyColumnCount);
}

