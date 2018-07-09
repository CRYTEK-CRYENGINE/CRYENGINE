// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>

class CItemModelAttribute;

namespace ACE
{
class CAsset;
class CLibrary;

class CSystemSourceModel : public QAbstractItemModel
{
public:

	enum class EColumns
	{
		Notification,
		Type,
		Placeholder,
		NoConnection,
		NoControl,
		PakStatus,
		InPak,
		OnDisk,
		Scope,
		Name,
		Count,
	};

	CSystemSourceModel(QObject* const pParent);
	virtual ~CSystemSourceModel() override;

	void                        DisconnectSignals();

	static CItemModelAttribute* GetAttributeForColumn(EColumns const column);
	static QVariant             GetHeaderData(int const section, Qt::Orientation const orientation, int const role);

	static CAsset*              GetAssetFromIndex(QModelIndex const& index, int const column);
	static QMimeData*           GetDragDropData(QModelIndexList const& list);

	static bool                 CanDropData(QMimeData const* const pData, CAsset const& parent);
	static bool                 DropData(QMimeData const* const pData, CAsset* const pParent);

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
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	// ~QAbstractItemModel

private:

	void ConnectSignals();

	bool      m_ignoreLibraryUpdates;
	int const m_nameColumn;
};
} // namespace ACE
