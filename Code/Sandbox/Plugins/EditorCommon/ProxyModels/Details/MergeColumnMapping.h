// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QVariant>

struct CMergeColumns
{
	typedef const QAbstractItemModel SourceModel;

	typedef int                      SourceColumn;
	typedef int                      ProxyColumn;

	typedef QVector<ProxyColumn>     SourceToProxy;
	typedef QVector<SourceColumn>    ProxyToSource;

	struct MountData
	{
		friend struct CMergeColumns;
	public:
		SourceColumn GetColumnFromProxy(ProxyColumn proxyColumn) const
		{
			return m_proxyToSource.value(proxyColumn, -1);
		}

		ProxyColumn GetColumnFromSource(SourceColumn sourceColumn) const
		{
			return m_sourceToProxy.value(sourceColumn, -1);
		}

	private:
		SourceToProxy m_sourceToProxy; // [source column] = proxy column
		ProxyToSource m_proxyToSource; // [proxy column] = source column (value -1 means no source)
	};

private:
	struct Column
	{
		ProxyColumn m_proxyColumn;
		mutable int m_referenceCount;

		Column(ProxyColumn proxyColumn)
			: m_proxyColumn(proxyColumn)
			, m_referenceCount(1)
		{}

		operator ProxyColumn() const { return m_proxyColumn; }

		void AddRef() const    { ++m_referenceCount; }
		void RemoveRef() const { --m_referenceCount; }
		bool HasRef() const    { return 0 != m_referenceCount; }
	};

	typedef int                     Role;
	typedef QMap<QVariant, Column>  ValueToColumn;
	typedef ValueToColumn::iterator ColumnIterator;
	typedef QVector<ColumnIterator> ProxyColumnToIterator;

	Role                  m_headerDataRole;     // if -1, columns are merged by index only
	ValueToColumn         m_mergeValueToColumn; //Key is column index if headerDataRole == -1, otherwise key is the return value of getHeaderData
	ProxyColumnToIterator m_columns;

public:
	CMergeColumns()
		: m_headerDataRole(-1)
	{}

	void SetHeaderDataRole(Role role)
	{
		m_headerDataRole = role;
		Reset();
	}

	bool HasMapping() const
	{
		return m_headerDataRole != -1;
	}

	int ColumnCount() const
	{
		return m_columns.count();
	}

	void Reset()
	{
		m_mergeValueToColumn.clear();
		m_columns.clear();
	}

	struct MountDataAndNewColumns
	{
		MountData         mountData;
		QVector<QVariant> newColumns;
	};

	// references a new sourceModel, but does not insert new columns
	// newColums are defered to allow proper beginInsertColumns()
	MountDataAndNewColumns Mount(SourceModel* sourceModel) const
	{
		MountDataAndNewColumns result;
		auto sourceColumnCount = sourceModel->columnCount();
		if (!HasMapping())
		{
			auto proxyColumnCount = ColumnCount();

			if (sourceColumnCount > proxyColumnCount)
			{
				result.newColumns = QVector<QVariant>(sourceColumnCount - proxyColumnCount);
				proxyColumnCount = sourceColumnCount;
			}

			result.mountData.m_sourceToProxy.reserve(sourceColumnCount);
			result.mountData.m_proxyToSource = ProxyToSource(proxyColumnCount, -1);

			for (int i = 0; i < sourceColumnCount; i++)
			{
				result.mountData.m_sourceToProxy << i;
				result.mountData.m_proxyToSource[i] = i;
			}

			const int addRefCount = std::min(ColumnCount(), sourceColumnCount);
			for (int i = 0; i < addRefCount; i++)
			{
				m_columns[i]->AddRef();
			}

			return result;
		}
		else
		{
			//fill source to proxy
			auto proxyColumnCount = m_mergeValueToColumn.size();
			result.mountData.m_sourceToProxy.reserve(sourceColumnCount);

			//for all source columns
			for (auto sourceColumn = 0; sourceColumn < sourceColumnCount; ++sourceColumn)
			{
				QVariant mergeValue = sourceModel->headerData(sourceColumn, Qt::Horizontal, m_headerDataRole);
				auto it = m_mergeValueToColumn.constFind(mergeValue);
				if (it == m_mergeValueToColumn.constEnd())
				{
					//column not found
					auto proxyColumn = proxyColumnCount;
					++proxyColumnCount;
					result.mountData.m_sourceToProxy << proxyColumn;
					result.newColumns << mergeValue;
					//AddRef() is not necessary here because it will be constructed with reference count to 1 in InsertNewColumns
				}
				else
				{
					//column already used
					auto& column = it.value();
					result.mountData.m_sourceToProxy << column;
					column.AddRef();
				}
			}

			//fill proxy to source
			result.mountData.m_proxyToSource = ProxyToSource(proxyColumnCount, -1);

			for (auto sourceColumn = 0; sourceColumn < sourceColumnCount; ++sourceColumn)
			{
				auto proxyColumn = result.mountData.m_sourceToProxy[sourceColumn];
				result.mountData.m_proxyToSource[proxyColumn] = sourceColumn;
			}

			return result;
		}
	}

	// use this in beginInsertColumns()
	void InsertNewColumns(const QVector<QVariant>& newColumns)
	{
		for (auto& value : newColumns)
		{
			auto proxyColumn = m_mergeValueToColumn.size();
			auto valueToInsert = HasMapping() ? value : proxyColumn;
			if (!m_mergeValueToColumn.contains(valueToInsert))  // other models might have inserted the column already
			{
				auto it = m_mergeValueToColumn.insert(valueToInsert, proxyColumn);
				m_columns << it;
			}
		}
	}

	// unreferences a sourceModel, but does not insert new columns
	// use the result to call proper beginRemoveColumns()
	QVector<ProxyColumn> Unmount(const MountData& mountData)
	{
		QVector<ProxyColumn> result;

		for (auto proxyColumn : mountData.m_sourceToProxy)
		{
			auto it = m_columns[proxyColumn];
			it->RemoveRef();
			if (!it->HasRef())
			{
				result << proxyColumn;
			}
		}
		return result;
	}

	// use this in beginRemoveColumns()
	void RemoveColumns(ProxyColumn proxyColumn, int columnCount)
	{
		for (int i = 0; i < columnCount; ++i)
		{
			auto it = m_columns[proxyColumn + i];
			m_mergeValueToColumn.erase(it);
		}
		m_columns.remove(proxyColumn, columnCount);
	}
};

