// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IItemModel.h>
#include <SharedData.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
class CItem;

class CItemModel final : public IItemModel
{
public:

	enum class EColumns
	{
		Notification,
		Connected,
		PakStatus,
		InPak,
		OnDisk,
		Name,
		Count,
	};

	explicit CItemModel(CItem const& rootItem);

	CItemModel() = delete;

	// IItemModel
	virtual int                          GetNameColumn() const override         { return static_cast<int>(EColumns::Name); }
	virtual ColumnResizeModes const&     GetColumnResizeModes() const override  { return m_columnResizeModes; }
	virtual QString const                GetTargetFolderName(QModelIndex const& index) const override;
	virtual QStringList const&           GetSupportedFileTypes() const override { return s_supportedFileTypes; }
	virtual ExtensionFilterVector const& GetExtensionFilters() const override   { return m_extensionFilters; }
	// ~IItemModel

	void Reset();

	static QStringList const s_supportedFileTypes;

protected:

	// QAbstractItemModel
	virtual int             rowCount(QModelIndex const& parent) const override;
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual QModelIndex     index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(QModelIndex const& index) const override;
	virtual bool            canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	virtual bool            dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	virtual Qt::DropActions supportedDragActions() const override;
	virtual QStringList     mimeTypes() const override;
	virtual QMimeData*      mimeData(QModelIndexList const& indexes) const override;
	// ~QAbstractItemModel

private:

	CItem*      ItemFromIndex(QModelIndex const& index) const;
	QModelIndex IndexFromItem(CItem const* const pItem) const;

	CItem const&                m_rootItem;
	ColumnResizeModes const     m_columnResizeModes;
	ExtensionFilterVector const m_extensionFilters;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
