// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

class CAdvancedFileSystemModel;
class QSearchBox;

namespace ACE
{
class CDirectorySelectorFilterModel;
class CTreeView;

class CDirectorySelectorDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	explicit CDirectorySelectorDialog(QString const& assetFolderPath, QString const& targetPath, QWidget* const pParent);
	virtual ~CDirectorySelectorDialog() override;

	CDirectorySelectorDialog() = delete;

signals:

	void SignalSetTargetPath(QString const& targetPath);

private slots:

	void OnSelectionChanged(QModelIndex const& index);
	void OnItemDoubleClicked(QModelIndex const& index);
	void OnAccept();

private:

	CAdvancedFileSystemModel* const      m_pFileSystemModel;
	CDirectorySelectorFilterModel* const m_pFilterProxyModel;
	QSearchBox* const                    m_pSearchBox;
	CTreeView* const                     m_pTreeView;
	QString const                        m_assetFolderPath;
	QString                              m_targetPath;
};
} // namespace ACE

