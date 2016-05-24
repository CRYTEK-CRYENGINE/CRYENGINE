// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QAbstractItemModel>
#include "ATLControlsModel.h"

class QVBoxLayout;
class QFrame;
class QPropertyTree;
class QTreeView;

namespace ACE
{
class CATLControl;
class IAudioSystemEditor;

class QConnectionModel : public QAbstractItemModel, public IATLControlModelListener
{
public:
	QConnectionModel();
	~QConnectionModel();
	void Init(CATLControl* pControl);

	enum EConnectionModelRoles
	{
		eConnectionModelRoles_Id = Qt::UserRole + 1,
	};

	enum EConnectionModelColumns
	{
		eConnectionModelColumns_Name = 0,
		eConnectionModelColumns_Path,
		eConnectionModelColumns_Size,
	};

	//////////////////////////////////////////////////////////
	// QAbstractTableModel implementation
	virtual int             rowCount(const QModelIndex& parent) const override;
	virtual int             columnCount(const QModelIndex& parent) const override;
	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	virtual bool            canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	virtual QStringList     mimeTypes() const override;
	virtual bool            dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual bool            setData(const QModelIndex& index, const QVariant& value, int role) override;
	//////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////
	// IATLControlModelListener implementation
	virtual void OnConnectionAdded(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl);
	virtual void OnConnectionRemoved(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl);
	virtual void OnControlModified(CATLControl* pControl);
	//////////////////////////////////////////////////////////

private:

	void ResetCache();
	void DecodeMimeData(const QMimeData* pData, std::vector<CID>& ids) const;

	CATLControl*               m_pControl;
	IAudioSystemEditor*        m_pAudioSystem;
	std::vector<ConnectionPtr> m_connectionsCache;
	std::vector<QString>       m_platformNames;
};

}
