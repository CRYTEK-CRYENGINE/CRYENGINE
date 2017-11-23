// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include <SystemTypes.h>

namespace ACE
{
class CSystemAssetsManager;
class CSystemControl;
struct IEditorImpl;

class CConnectionModel final: public QAbstractItemModel
{
public:

	CConnectionModel();
	virtual ~CConnectionModel() override;

	void Init(CSystemControl* const pControl);

	enum class EConnectionModelRoles
	{
		Id = Qt::UserRole + 1,
	};

	enum class EConnectionModelColumns
	{
		Name = 0,
		Path,
		Size,
	};

	// QAbstractTableModel
	virtual int             rowCount(QModelIndex const& parent) const override;
	virtual int             columnCount(QModelIndex const& parent) const override;
	virtual QVariant        data(QModelIndex const& index, int role) const override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags   flags(QModelIndex const& index) const override;
	virtual QModelIndex     index(int row, int column, QModelIndex const& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(QModelIndex const& index) const override;
	virtual bool            canDropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) const override;
	virtual QStringList     mimeTypes() const override;
	virtual bool            dropMimeData(QMimeData const* pData, Qt::DropAction action, int row, int column, QModelIndex const& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual bool            setData(QModelIndex const& index, QVariant const& value, int role) override;
	// ~QAbstractTableModel

private:

	void ResetCache();
	void DecodeMimeData(QMimeData const* pData, std::vector<CID>& ids) const;

	CSystemAssetsManager* const m_pAssetsManager;
	CSystemControl*             m_pControl;
	IEditorImpl*                m_pEditorImpl;
	std::vector<ConnectionPtr>  m_connectionsCache;
	std::vector<QString>        m_platformNames;
};
} // namespace ACE
