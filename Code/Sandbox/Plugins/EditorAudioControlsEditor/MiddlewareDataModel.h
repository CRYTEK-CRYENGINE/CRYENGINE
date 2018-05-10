// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>

class CItemModelAttribute;

namespace ACE
{
namespace Impl
{
struct IItem;
} // namespace Impl

class CMiddlewareDataModel final : public QAbstractItemModel
{
public:

	enum class EColumns
	{
		Notification,
		Connected,
		Localized,
		Name,
		Count,
	};

	explicit CMiddlewareDataModel(QObject* const pParent);
	virtual ~CMiddlewareDataModel() override;

	CMiddlewareDataModel() = delete;

	static CItemModelAttribute* GetAttributeForColumn(EColumns const column);
	static QVariant             GetHeaderData(int const section, Qt::Orientation const orientation, int const role);

	void                        DisconnectSignals();
	void                        Reset();

protected:

	// QAbstractItemModel
	virtual int             rowCount(QModelIndex const& parent) const override;
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual QModelIndex     index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(QModelIndex const& index) const override;
	virtual Qt::DropActions supportedDragActions() const override;
	virtual QStringList     mimeTypes() const override;
	virtual QMimeData*      mimeData(QModelIndexList const& indexes) const override;
	// ~QAbstractItemModel

private:

	void         ConnectSignals();
	Impl::IItem* ItemFromIndex(QModelIndex const& index) const;
	QModelIndex  IndexFromItem(Impl::IItem const* const pIItem) const;
};
} // namespace ACE

