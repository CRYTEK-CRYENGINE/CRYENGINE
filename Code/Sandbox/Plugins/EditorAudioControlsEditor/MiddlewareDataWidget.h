// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>
#include <SharedData.h>

class QFilteringPanel;

namespace ACE
{
class CControl;
class CMiddlewareDataModel;
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

	void ClearFilters();

	CMiddlewareFilterProxyModel* const m_pMiddlewareFilterProxyModel;
	CMiddlewareDataModel* const        m_pMiddlewareDataModel;
	QFilteringPanel*                   m_pFilteringPanel;
	CTreeView* const                   m_pTreeView;
	int const                          m_nameColumn;
};
} // namespace ACE

