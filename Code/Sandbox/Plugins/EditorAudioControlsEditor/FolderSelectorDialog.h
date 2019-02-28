// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

class CAdvancedFileSystemModel;
class QSearchBox;
class QPushButton;

namespace ACE
{
class CFolderSelectorFilterModel;
class CTreeView;

class CFolderSelectorDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	explicit CFolderSelectorDialog(QString const& assetFolderPath, QString const& targetPath, QWidget* const pParent);
	virtual ~CFolderSelectorDialog() override;

	CFolderSelectorDialog() = delete;

signals:

	void SignalSetTargetPath(QString const& targetPath);

private:

	void OpenFolderDialog();
	void OnContextMenu(QPoint const& pos);
	void OnCreateFolder(QString const& folderName);
	void OnSelectionChanged(QModelIndex const& index);
	void OnItemDoubleClicked(QModelIndex const& index);
	void OnAccept();

	CAdvancedFileSystemModel* const   m_pFileSystemModel;
	CFolderSelectorFilterModel* const m_pFilterProxyModel;
	QPushButton* const                m_pAddFolderButton;
	QSearchBox* const                 m_pSearchBox;
	CTreeView* const                  m_pTreeView;
	QString const                     m_assetFolderPath;
	QString                           m_targetPath;
};
} // namespace ACE
