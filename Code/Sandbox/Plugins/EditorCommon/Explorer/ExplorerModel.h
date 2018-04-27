// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <QtCore/QAbstractItemModel>
#include "EditorCommonAPI.h"

namespace Explorer
{

struct ExplorerEntry;
struct ExplorerEntryModifyEvent;
class ExplorerData;

class EDITOR_COMMON_API ExplorerModel : public QAbstractItemModel
{
	Q_OBJECT

public:
	ExplorerModel(ExplorerData* explorerData, QObject* parent);

	void                  SetRootByIndex(int index);
	int                   GetRootIndex() const;
	ExplorerEntry*        GetActiveRoot() const;

	static ExplorerEntry* GetEntry(const QModelIndex& index);

	// QAbstractItemModel
	QModelIndex index(int row, int column, const QModelIndex& parent) const override;
	int         rowCount(const QModelIndex& parent) const override;
	int         columnCount(const QModelIndex& parent) const override;

	QVariant    headerData(int section, Qt::Orientation orientation, int role) const override;
	bool        hasChildren(const QModelIndex& parent) const override;
	QVariant    data(const QModelIndex& index, int role) const override;
	QModelIndex ModelIndexFromEntry(ExplorerEntry* entry, int column) const;
	QModelIndex NewModelIndexFromEntry(ExplorerEntry* entry, int column) const;
	QModelIndex parent(const QModelIndex& index) const override;

public slots:
	void OnEntryModified(ExplorerEntryModifyEvent& ev);
	void OnBeginAddEntry(ExplorerEntry* entry);
	void OnEndAddEntry();
	void OnBeginRemoveEntry(ExplorerEntry* entry);
	void OnEndRemoveEntry();
protected:

	bool          m_addWithinActiveRoot;
	bool          m_removeWithinActiveRoot;
	int           m_rootIndex;
	int           m_rootSubtree;
	ExplorerData* m_explorerData;
};

}

