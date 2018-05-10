// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include <QSortFilterProxyModel>

namespace Internal
{
	class CNotification;
}

class CNotificationFilterModel : public QSortFilterProxyModel
{
public:
	CNotificationFilterModel(QObject* pParent = nullptr);
	void SetFilterTypeState(unsigned type, bool bEnable);

protected:
	virtual bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

protected:
	unsigned m_typeMask;
};

//////////////////////////////////////////////////////////////////////////

class CNotificationModel : public QAbstractItemModel
{
public:
	enum class Roles : int
	{
		Type = Qt::UserRole,
		SortRole,
	};

	CNotificationModel(QObject* pParent = nullptr);

	void OnNotificationAdded(int id);

	void CopyHistory() const;
	void CopyHistory(QModelIndexList indices) const;
	void ClearAll();
	void ShowCompleteHistory();

	void AddFilterType(int type);
	void RemoveFilterType(int type);

	//QAbstractItemModel implementation begin
	virtual int           columnCount(const QModelIndex& parent = QModelIndex()) const override { return s_ColumnCount; }
	virtual QVariant      data(const QModelIndex& index, int role /* = Qt::DisplayRole */) const override;
	virtual bool          hasChildren(const QModelIndex& parent /* = QModelIndex() */) const override;
	virtual QVariant      headerData(int section, Qt::Orientation orientation, int role /* = Qt::DisplayRole */) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	virtual QModelIndex   index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex   parent(const QModelIndex& index) const override;
	virtual bool          setData(const QModelIndex& index, const QVariant& value, int role /*= Qt::EditRole*/) override;
	virtual int           rowCount(const QModelIndex& parent /* = QModelIndex() */) const override;
	//QAbstractItemModel implementation end

	Internal::CNotification* NotificationFromIndex(const QModelIndex& index) const;

protected:
	// Determines the offset into the notifications array. Since we can clear the list we want to only show new notifications
	int                m_startIdx;
	int                m_startSize;
	int                m_currIdx;
	static const int   s_ColumnCount = 4;
	static const char* s_ColumnNames[s_ColumnCount];
};

