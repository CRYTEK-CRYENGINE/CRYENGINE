// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DeepFilterProxyModel.h"

#include <QPersistentModelIndex>
#include <QHash>
#include "EditorStyleHelper.h"

struct QDeepFilterProxyModel::Implementation
{
	struct SFilterResult
	{
		SFilterResult()
			: matched(false)
			, partOfSourceRoot(true)
			, testedBehavior(BaseBehavior)
			, matchedBehavior(BaseBehavior)
		{}

		static SFilterResult createOutOfSourceRoot()
		{
			SFilterResult result;
			result.partOfSourceRoot = false;
			return result;
		}
		static SFilterResult createMatched()
		{
			SFilterResult result;
			result.matched = true;
			return result;
		}

		bool hasMatch(BehaviorFlags flags) const
		{
			return matched || (matchedBehavior & flags) != 0;
		}

		bool wasTested(BehaviorFlags flags) const
		{
			return matched || (testedBehavior & flags) != 0;
		}

		bool          matched          : 1;
		bool          partOfSourceRoot : 1;
		BehaviorFlags testedBehavior;
		BehaviorFlags matchedBehavior;
	};
	typedef QHash<QPersistentModelIndex, SFilterResult> IndexResultHash;
	typedef IndexResultHash::iterator                   IndexResult;

	Implementation(BehaviorFlags behavior, QDeepFilterProxyModel* pModel)
		: m_pModel(pModel)
		, m_behavior(behavior)
	{
		m_pModel->setFilterCaseSensitivity(Qt::CaseInsensitive); // this is meant to be a very permissive filter mainly used with wildcard
	}

	void DisconnectModel()
	{
		foreach(auto connection, m_connections)
		{
			disconnect(connection);
		}
		m_connections.clear();
	}

	void ConnectModel(QAbstractItemModel* sourceModel)
	{
		m_connections << connect(sourceModel, &QAbstractItemModel::modelReset, m_pModel, [this]()
		{
			ClearCaches();
		});
		m_connections << connect(sourceModel, &QAbstractItemModel::rowsInserted, m_pModel, [this](const QModelIndex& parent)
		{
			InsertSourceRows(parent);
		});
		m_connections << connect(sourceModel, &QAbstractItemModel::rowsRemoved, m_pModel, [this](const QModelIndex& parent)
		{
			RemoveSourceRows(parent);
			CleanupInvalidCache();
		});
		m_connections << connect(sourceModel, &QAbstractItemModel::rowsMoved, m_pModel, [this](const QModelIndex& sourceParent, int, int, const QModelIndex& destinationParent)
		{
			if (sourceParent != destinationParent)
			{
			  RemoveSourceRows(sourceParent);
			  InsertSourceRows(destinationParent);
			}
		});
		m_connections << connect(sourceModel, &QAbstractItemModel::dataChanged, m_pModel, [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
		{
			if (!topLeft.isValid() || !bottomRight.isValid())
			{
			  return;
			}
			auto sourceParent = topLeft.parent();
			CRY_ASSERT(sourceParent == bottomRight.parent());
			CRY_ASSERT(topLeft.row() <= bottomRight.row());
			UpdateSourceRows(sourceParent, topLeft.row(), bottomRight.row());
		});
	}

	void ClearCaches()
	{
		m_acceptedCache.clear();
	}

	void CleanupInvalidCache()
	{
		for (auto it = m_acceptedCache.begin(); it != m_acceptedCache.end(); )
		{
			if (!it.key().isValid())
			{
				it = m_acceptedCache.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	void InsertSourceRows(const QModelIndex& sourceParent)
	{
		if (!m_behavior.testFlag(AcceptIfChildMatches))
		{
			return; // nothing changed
		}
		auto cacheIt = m_acceptedCache.find(sourceParent);
		if (cacheIt == m_acceptedCache.end())
		{
			return; // was not published
		}
		if (!cacheIt->partOfSourceRoot)
		{
			return; // not affected by filter
		}
		if (cacheIt->hasMatch(AcceptIfChildMatches))
		{
			return; // other children already matched
		}
		bool hasAcceptedChildren = HasAcceptedChildren(sourceParent);
		if (hasAcceptedChildren)
		{
			PropagateAcceptedChildren(cacheIt);
		}
	}

	void PropagateAcceptedChildren(IndexResult it)
	{
		auto sourceIndex = it.key();
		auto sourceParent = sourceIndex.parent();

		it->testedBehavior |= AcceptIfChildMatches;
		it->matchedBehavior |= AcceptIfChildMatches;

		auto parentIt = m_acceptedCache.find(sourceParent);
		if (parentIt == m_acceptedCache.end() || parentIt.key() == m_sourceRootIndex)
		{
			m_pModel->sourceModel()->dataChanged(it.key(), it.key());
			return;
		}
		if (parentIt->hasMatch(AcceptIfChildMatches))
		{
			m_pModel->sourceModel()->dataChanged(it.key(), it.key());
			return;
		}
		PropagateAcceptedChildren(parentIt);
	}

	void RemoveSourceRows(const QModelIndex& sourceParent)
	{
		if (!m_behavior.testFlag(AcceptIfChildMatches))
		{
			return; // nothing changed
		}
		auto cacheIt = m_acceptedCache.find(sourceParent);
		if (cacheIt == m_acceptedCache.end())
		{
			return; // was not published
		}
		if (!cacheIt->partOfSourceRoot)
		{
			return; // not affected by filter
		}
		if (!cacheIt->hasMatch(AcceptIfChildMatches))
		{
			return; // no children matched
		}
		bool hasAcceptedChildren = HasAcceptedChildren(sourceParent);
		if (!hasAcceptedChildren)
		{
			PropagateRejectedChildren(cacheIt);
		}
	}

	void PropagateRejectedChildren(IndexResult it)
	{
		auto sourceIndex = it.key();
		auto sourceParent = sourceIndex.parent();

		it->testedBehavior |= AcceptIfChildMatches;
		it->matchedBehavior &= ~AcceptIfChildMatches;

		auto parentIt = m_acceptedCache.find(sourceParent);
		if (parentIt == m_acceptedCache.end() || parentIt.key() == m_sourceRootIndex)
		{
			m_pModel->sourceModel()->dataChanged(it.key(), it.key());
			return;
		}
		bool hasAcceptedChildren = HasAcceptedChildren(sourceParent);
		if (hasAcceptedChildren)
		{
			m_pModel->sourceModel()->dataChanged(it.key(), it.key());
			return;
		}
		PropagateRejectedChildren(parentIt);
	}

	void UpdateSourceRows(const QModelIndex& sourceParent, int firstRow, int lastRow)
	{
		if (m_behavior == 0)
		{
			return; // no deep filter
		}
		auto parentCacheIt = m_acceptedCache.find(sourceParent);
		if (parentCacheIt == m_acceptedCache.end())
		{
			return; // was not published
		}
		if (!parentCacheIt->partOfSourceRoot && parentCacheIt.key() != m_sourceRootIndex)
		{
			return; // not affected by filter
		}

		auto pSourceModel = m_pModel->sourceModel();
		for (auto sourceRow = firstRow; sourceRow <= lastRow; ++sourceRow)
		{
			auto sourceIndex = pSourceModel->index(sourceRow, 0, sourceParent);
			auto persistentIndex = QPersistentModelIndex(sourceIndex);
			auto cacheIt = m_acceptedCache.find(persistentIndex);
			if (cacheIt == m_acceptedCache.end())
			{
				continue; // not found
			}
			bool newMatched = m_pModel->rowMatchesFilter(sourceRow, sourceParent);
			if (cacheIt->matched == newMatched)
			{
				continue; // matched not changed
			}
			bool wasMatch = cacheIt->hasMatch(m_behavior);
			cacheIt->matched = newMatched;
			cacheIt->testedBehavior = 0;
			cacheIt->matchedBehavior = 0;
			UpdateFilterResult(*cacheIt, sourceIndex, sourceParent, m_behavior);
			bool nowMatch = cacheIt->hasMatch(m_behavior);

			if (nowMatch != wasMatch) // node changed visibility
			{
				if (m_behavior.testFlag(AcceptIfChildMatches) && parentCacheIt->hasMatch(AcceptIfChildMatches) != nowMatch)
				{
					if (nowMatch)
					{
						PropagateAcceptedChildren(parentCacheIt);
					}
					else
					{
						PropagateRejectedChildren(parentCacheIt);
					}
				}
			}
		}
	}

	const SFilterResult& FetchFilterResult(int sourceRow, const QModelIndex& sourceParent, BehaviorFlags behavior) const
	{
		auto pSourceModel = m_pModel->sourceModel();
		auto sourceIndex = pSourceModel->index(sourceRow, 0, sourceParent);
		if (!sourceIndex.isValid())
		{
			static SFilterResult s_invalidResult = SFilterResult::createOutOfSourceRoot();
			return s_invalidResult; // invalid index
		}
		QPersistentModelIndex persistentIndex(sourceIndex);
		auto it = m_acceptedCache.find(persistentIndex);
		if (it != m_acceptedCache.end())
		{
			UpdateFilterResult(it.value(), sourceIndex, sourceParent, behavior);
			return it.value(); // already cached
		}
		auto result = CreateFilterResult(sourceIndex, sourceParent, behavior);
		m_acceptedCache[persistentIndex] = result;
		return m_acceptedCache[persistentIndex];
	}

	SFilterResult CreateFilterResult(const QModelIndex& sourceIndex, const QModelIndex& sourceParent, BehaviorFlags behavior) const
	{
		if (!IsPartOfSourceRoot(sourceParent))
		{
			return SFilterResult::createOutOfSourceRoot();
		}
		if (m_pModel->rowMatchesFilter(sourceIndex.row(), sourceParent))
		{
			return SFilterResult::createMatched();
		}
		SFilterResult result;
		UpdateFilterResult(result, sourceIndex, sourceParent, behavior);
		return result;
	}

	void UpdateFilterResult(SFilterResult& result, const QModelIndex& sourceIndex, const QModelIndex& sourceParent, BehaviorFlags behavior) const
	{
		if (!result.partOfSourceRoot)
		{
			return; // no tests outside of root
		}
		if (result.hasMatch(behavior))
		{
			return; // one criteria matches (that's enough)
		}
		if (result.wasTested(behavior))
		{
			return; // everything was tested
		}
		if (behavior.testFlag(AcceptIfParentMatches) && !result.wasTested(AcceptIfParentMatches))
		{
			result.testedBehavior |= AcceptIfParentMatches;
			if (HasAcceptedParents(sourceParent))
			{
				result.matchedBehavior |= AcceptIfParentMatches;
				CRY_ASSERT(result.hasMatch(behavior));
				return;
			}
		}
		if (behavior.testFlag(AcceptIfChildMatches) /*&& !result.wasTested(AcceptIfChildMatches)*/)
		{
			result.testedBehavior |= AcceptIfChildMatches;
			if (HasAcceptedChildren(sourceIndex))
			{
				result.matchedBehavior |= AcceptIfChildMatches;
				CRY_ASSERT(result.hasMatch(behavior));
				return;
			}
		}
		return;
	}

	bool IsPartOfSourceRoot(const QModelIndex& sourceParent) const
	{
		if (!m_sourceRootIndex.isValid())
		{
			return true; // everything is in root
		}
		if (m_sourceRootIndex == sourceParent)
		{
			return true; // teminator
		}
		if (!sourceParent.isValid())
		{
			return false; // not found
		}
		// check parent
		const auto& result = FetchFilterResult(sourceParent.row(), sourceParent.parent(), BehaviorFlags());
		return result.partOfSourceRoot;
	}

	bool HasAcceptedChildren(const QModelIndex& sourceIndex) const
	{
		auto sourceModel = m_pModel->sourceModel();
		if (sourceModel->canFetchMore(sourceIndex))
		{
			sourceModel->fetchMore(sourceIndex);
		}
		if (!sourceIndex.model()->hasChildren(sourceIndex))
		{
			return false;
		}
		const int childRows = sourceIndex.model()->rowCount(sourceIndex);
		if (childRows == 0)
		{
			return false;
		}
		for (int childRow = 0; childRow < childRows; ++childRow)
		{
			auto result = FetchFilterResult(childRow, sourceIndex, AcceptIfChildMatches);
			if (result.hasMatch(AcceptIfChildMatches))
			{
				return true;
			}
		}
		return false;
	}

	bool HasAcceptedParents(const QModelIndex& sourceIndex) const
	{
		if (!sourceIndex.isValid())
		{
			return false;
		}
		if (m_sourceRootIndex == sourceIndex)
		{
			return false; // do not go higher than root index
		}
		auto sourceRow = sourceIndex.row();
		auto sourceParent = sourceIndex.parent();
		auto result = FetchFilterResult(sourceRow, sourceParent, AcceptIfParentMatches);
		return result.hasMatch(AcceptIfParentMatches);
	}

	bool MatchTokenSearch(const QString& string)
	{
		for (auto& token : m_searchTokens)
		{
			if (string.indexOf(token, 0, Qt::CaseInsensitive) == -1)
				return false;
		}

		return true;
	}

	QDeepFilterProxyModel*           m_pModel;
	BehaviorFlags                    m_behavior;
	QVector<QMetaObject::Connection> m_connections;
	QPersistentModelIndex            m_sourceRootIndex;
	mutable IndexResultHash          m_acceptedCache;
	QStringList						 m_searchTokens;
};

QDeepFilterProxyModel::QDeepFilterProxyModel(BehaviorFlags behavior, QObject* parent /*= nullptr*/)
	: QSortFilterProxyModel(parent)
	, p(new Implementation(behavior, this))
{}

QDeepFilterProxyModel::~QDeepFilterProxyModel()
{}

bool QDeepFilterProxyModel::IsRowMatched(const QModelIndex& sourceIndex) const
{
	if (!sourceIndex.isValid())
	{
		return false; // root never matches
	}
	auto sourceParent = sourceIndex.parent();
	auto result = p->FetchFilterResult(sourceIndex.row(), sourceParent, 0);
	return result.matched;
}

void QDeepFilterProxyModel::SetSourceRootIndex(const QModelIndex& sourceRoot)
{
	if (sourceRoot == p->m_sourceRootIndex)
	{
		return; // nothing changed
	}
	Q_ASSERT(!sourceRoot.isValid() || sourceRoot.model() == sourceModel());
	beginResetModel();
	p->ClearCaches();
	p->m_sourceRootIndex = sourceRoot;
	endResetModel();
}

QVariant QDeepFilterProxyModel::data(const QModelIndex& proxyIndex, int role) const
{
	if (role == Qt::TextColorRole)
	{
		auto sourceIndex = mapToSource(proxyIndex);
		if (!sourceIndex.isValid())
		{
			return sourceIndex.data(role);
		}
		auto result = p->FetchFilterResult(sourceIndex.row(), sourceIndex.parent(), p->m_behavior);
		if (result.matched)
		{
			return sourceIndex.data(role);
		}
		return GetStyleHelper()->disabledTextColor();
	}
	return QSortFilterProxyModel::data(proxyIndex, role);
}

void QDeepFilterProxyModel::setSourceModel(QAbstractItemModel* newSourceModel)
{
	auto oldSourceModel = QSortFilterProxyModel::sourceModel();
	if (oldSourceModel != newSourceModel)
	{
		p->DisconnectModel();
		p->ClearCaches();
		auto persistedIndexList = persistentIndexList();
		QVector<QModelIndex> clean(persistedIndexList.size());
		changePersistentIndexList(persistedIndexList, clean.toList());
		p->m_sourceRootIndex = QModelIndex();
		if (newSourceModel)
		{
			p->ConnectModel(newSourceModel);
		}
		QSortFilterProxyModel::setSourceModel(newSourceModel);
	}
}

void QDeepFilterProxyModel::setFilterRegExp(const QRegExp& regExp)
{
	if (filterRegExp() != regExp)
	{
		p->ClearCaches();
		QSortFilterProxyModel::setFilterRegExp(regExp);
	}
}

void QDeepFilterProxyModel::setFilterTokenizedString(const QString& filter)
{
	p->m_searchTokens = filter.split(' ', QString::SkipEmptyParts);
	invalidateFilter();
}

void QDeepFilterProxyModel::setFilterFixedString(const QString& filter)
{
	if (filterRegExp().patternSyntax() == QRegExp::FixedString && filterRegExp().pattern() == filter)
	{
		return; // nothing changed
	}
	p->ClearCaches();
	QSortFilterProxyModel::setFilterFixedString(filter);
}

void QDeepFilterProxyModel::setFilterRegExp(const QString& regexp)
{
	if (filterRegExp().patternSyntax() == QRegExp::RegExp && filterRegExp().pattern() == regexp)
	{
		return; // nothing changed
	}
	p->ClearCaches();
	QSortFilterProxyModel::setFilterRegExp(regexp);
}

void QDeepFilterProxyModel::setFilterWildcard(const QString& pattern)
{
	if (filterRegExp().patternSyntax() == QRegExp::Wildcard && filterRegExp().pattern() == pattern)
	{
		return; // nothing changed
	}
	p->ClearCaches();
	QSortFilterProxyModel::setFilterWildcard(pattern);
}

void QDeepFilterProxyModel::invalidate()
{
	p->ClearCaches();
	QSortFilterProxyModel::invalidate();
}

void QDeepFilterProxyModel::invalidateFilter()
{
	p->ClearCaches();
	QSortFilterProxyModel::invalidateFilter();
}

bool QDeepFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
	const auto& result = p->FetchFilterResult(sourceRow, sourceParent, p->m_behavior);
	return !result.partOfSourceRoot || result.hasMatch(p->m_behavior);
}

bool QDeepFilterProxyModel::rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const
{
	if (p->m_searchTokens.size())
	{
		auto model = sourceModel();
		if (filterKeyColumn() == -1) //filter on all columns
		{
			int columnCount = model->columnCount(sourceParent);
			for (int column = 0; column < columnCount; column++)
			{
				QModelIndex sourceIndex = model->index(sourceRow, column, sourceParent);
				QString data = sourceIndex.data(filterRole()).toString();
				if (!p->MatchTokenSearch(data))
					return false;
			}
			return true;
		}
		else
		{
			QModelIndex sourceIndex = model->index(sourceRow, filterKeyColumn(), sourceParent);
			QString data = sourceIndex.data(filterRole()).toString();
			return p->MatchTokenSearch(data);
		}
	}
	else
	{
		return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
	}
}

