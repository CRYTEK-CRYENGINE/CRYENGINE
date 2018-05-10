// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QSortFilterProxyModel>

#include "EditorCommonAPI.h"

#include <memory>
#include <functional>

/**
 * @brief allows to filter even nested trees
 */
class EDITOR_COMMON_API QDeepFilterProxyModel
	: public QSortFilterProxyModel
{
	Q_OBJECT

public:
	enum Behavior
	{
		BaseBehavior          = 0, ///< only direct matches (same behavior as QSortFilterProxyModel)
		AcceptIfChildMatches  = 1, ///< if any child of a row matches include this row
		AcceptIfParentMatches = 2, ///< include rows if the parent matches
	};
	typedef QFlags<Behavior>                                             BehaviorFlags;

public:
	QDeepFilterProxyModel(BehaviorFlags behavior = AcceptIfChildMatches, QObject* parent = nullptr);
	~QDeepFilterProxyModel();

	/// \return true if row is directly matched
	/// \note to check if row was matched through behavior use mapFromSource and check for invalid index
	bool IsRowMatched(const QModelIndex& sourceIndex) const;

	/// \brief sets the source index as the filter root
	/// \note all source indices outside sourceRoot are never filtered out
	void SetSourceRootIndex(const QModelIndex& sourceRoot);

	// interface QAbstracItemModel
public:
	/// \brief Qt::TextColorRole will be disabled if row does not match directly
	virtual QVariant data(const QModelIndex& index, int role) const override;

	virtual void     setSourceModel(QAbstractItemModel* sourceModel) override;

	/// \warning this methods masks a public method of the base class.
	void setFilterRegExp(const QRegExp& regExp);

public slots:
	/// \brief filter is tokenized and each token must be found, in any order
	void setFilterTokenizedString(const QString& filter);

	//TODO : if those slots ever become virtual in the base class we should use inheritance
	/// \warning this methods mask public methods of the base class.
	void setFilterFixedString(const QString& filter);

	/// \warning this methods mask public methods of the base class.
	void setFilterRegExp(const QString& regexp);

	/// \warning this methods mask public methods of the base class.
	void setFilterWildcard(const QString& pattern);

	/// \warning this methods mask public methods of the base class.
	void invalidate();

	/// \warning this methods mask public methods of the base class.
	void invalidateFilter();

protected:
	/// \note Derived classes should implement another method, this one provides the "deep filtering functionnality"
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const final;

protected:
	/// \returns true only if the row's own data matches the filter, regardless of children or other factors
	/// \note Override to customize the filter's behavior as you would usually with filterAcceptsRow
	virtual bool rowMatchesFilter(int sourceRow, const QModelIndex& sourceParent) const;

private:
	struct Implementation;
	std::unique_ptr<Implementation> p;
};

