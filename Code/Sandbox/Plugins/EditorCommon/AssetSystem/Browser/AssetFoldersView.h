// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once


#include "EditorFramework/StateSerializable.h"
#include "ProxyModels/DeepFilterProxyModel.h"

#include <CrySandbox/CrySignal.h>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QWidget>

class CFilteredFolders;
class CFoldersViewProxyModel;
class QAdvancedTreeView;

class EDITOR_COMMON_API CAssetFoldersView : public QWidget, public IStateSerializable
{
	friend class CAssetBrowser;
public:
	CAssetFoldersView(bool bHideEngineFolder = false, QWidget* parent = nullptr);

	virtual QVariantMap GetState() const override;
	virtual void        SetState(const QVariantMap& state) override;

	void                SelectFolder(const QString& folder);
	void                SelectFolders(const QStringList& folders);
	void                ClearSelection();
	void                RenameFolder(const QString& folder);

	void                SetFilteredFolders(CFilteredFolders* pFilteredFolders);

	//! Index must belong to CAssetFoldersModel instance
	void SelectFolder(const QModelIndex& folderIndex);
	void SelectFolders(const QModelIndexList& foldersIndices);
	void RenameFolder(const QModelIndex& folderIndex);

	//! List of folder paths which content should be visible in other views.
	//! Other views showing both folders and assets (i.e., details view and thumbnails view) should
	//! display the content of the folders returned by GetSelectedFolders.
	//! There must be at least one visible folder, so the returned list is never empty.
	//! When no folder is selected (the view's selection is empty), the returned list contains
	//! a single string for the top-level asset folder.
	const QStringList&       GetSelectedFolders() const;

	const QAdvancedTreeView* GetInternalView() const;
	void                     setSelectionMode(QAbstractItemView::SelectionMode mode);
	//! Emitted when the selection has changed, but also if a folder is renamed
	CCrySignal<void(const QStringList&)> signalSelectionChanged;

private:
	class CFoldersViewProxyModel;

	void    OnSelectionChanged();
	void    OnCreateFolder(const QString& parentFolder);
	void    OnDeleteFolder(const QString& folder);
	void    OnOpenInExplorer(const QString& folder);
	void    OnDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

	QString ToFolder(const QModelIndex& index);

	QAdvancedTreeView*                     m_treeView;
	QStringList                            m_selectedFolders;
	std::unique_ptr<QDeepFilterProxyModel> m_pProxyModel;
};
