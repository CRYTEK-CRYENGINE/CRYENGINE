// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>

namespace ACE
{
class CAsset;
class CLibrary;

class CSystemLibraryModel : public QAbstractItemModel
{
public:

	CSystemLibraryModel(CLibrary* const pLibrary, QObject* const pParent);
	virtual ~CSystemLibraryModel() override;

	void DisconnectSignals();

protected:

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
	virtual QMimeData*      mimeData(QModelIndexList const& indexes) const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	// ~QAbstractItemModel

private:

	void        ConnectSignals();
	QModelIndex IndexFromItem(CAsset const* pAsset) const;

	CLibrary* const m_pLibrary;
	int const       m_nameColumn;
};
} // namespace ACE
