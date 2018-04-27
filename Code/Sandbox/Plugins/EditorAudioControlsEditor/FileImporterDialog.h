// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

#include <FileImportInfo.h>

class QAttributeFilterProxyModel;
class QLineEdit;

namespace ACE
{
class CTreeView;
class CFileImporterModel;

class CFileImporterDialog final : public CEditorDialog
{
	Q_OBJECT

public:

	explicit CFileImporterDialog(FileImportInfos const& fileInfos, QString const& assetFolderPath, QString const& targetPath, QWidget* const pParent);

	CFileImporterDialog() = delete;

	// QDialog
	virtual QSize sizeHint() const override;
	// ~QDialog

	// QWidget
	virtual void closeEvent(QCloseEvent* pEvent) override;
	// ~QWidget

signals:

	void SignalImporterClosed();

private slots:

	void OnCreateDirectorySelector();
	void OnTargetPathChanged(QString const& targetPath);
	void OnActionChanged(Qt::CheckState const isChecked);
	void OnSetImportAll();
	void OnSetIgnoreAll();
	void OnApplyImport();

private:

	CTreeView* const                  m_pTreeView;
	CFileImporterModel* const         m_pFileImporterModel;
	QAttributeFilterProxyModel* const m_pAttributeFilterProxyModel;
	QLineEdit* const                  m_pTargetDirLineEdit;
	FileImportInfos                   m_fileImportInfos;
	QString const                     m_assetFolderPath;
	QString                           m_targetPath;
};
} // namespace ACE
