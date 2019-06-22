// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>

namespace ACE
{
class CListenerSelectorModel final : public QAbstractItemModel
{
	Q_OBJECT

public:

	CListenerSelectorModel() = delete;
	CListenerSelectorModel(CListenerSelectorModel const&) = delete;
	CListenerSelectorModel(CListenerSelectorModel&&) = delete;
	CListenerSelectorModel& operator=(CListenerSelectorModel const&) = delete;
	CListenerSelectorModel& operator=(CListenerSelectorModel&&) = delete;

	explicit CListenerSelectorModel(QObject* const pParent)
		: QAbstractItemModel(pParent)
	{}

	virtual ~CListenerSelectorModel() override = default;

	// QAbstractItemModel
	virtual int             rowCount(QModelIndex const& parent) const override;
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual QModelIndex     index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(QModelIndex const& index) const override;
	virtual bool            canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	virtual bool            dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	// ~QAbstractItemModel

	static string GetListenerNameFromIndex(QModelIndex const& index);
};
} // namespace ACE
