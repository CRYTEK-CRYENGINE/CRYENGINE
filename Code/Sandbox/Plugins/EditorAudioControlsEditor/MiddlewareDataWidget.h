// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include <SharedData.h>
#include <FileImportInfo.h>

class QFilteringPanel;
class QPushButton;

namespace ACE
{
namespace Impl
{
struct IItemModel;
} // namespace Impl

class CControl;
class CMiddlewareFilterProxyModel;
class CTreeView;

class CMiddlewareDataWidget final : public QWidget
{
	Q_OBJECT

public:

	explicit CMiddlewareDataWidget(QWidget* const pParent);
	virtual ~CMiddlewareDataWidget() override;

	CMiddlewareDataWidget() = delete;

	void Reset();
	void BackupTreeViewStates();
	void RestoreTreeViewStates();
	void SelectConnectedImplItem(ControlId const itemId);

signals:

	void SignalSelectConnectedSystemControl(CControl& sytemControl, ControlId const itemId);

private slots:

	void OnContextMenu(QPoint const& pos);

private:

	void SetDragDropMode();
	void SetColumnResizeModes();
	void ClearFilters();
	void OpenFileImporter(FileImportInfos const& fileInfos, QString const& targetFolderName);
	void OnImportFiles();

	CMiddlewareFilterProxyModel* const m_pMiddlewareFilterProxyModel;
	Impl::IItemModel*                  m_pImplItemModel;
	QPushButton* const                 m_pImportButton;
	QFilteringPanel*                   m_pFilteringPanel;
	CTreeView* const                   m_pTreeView;
	int                                m_nameColumn;
};
} // namespace ACE
