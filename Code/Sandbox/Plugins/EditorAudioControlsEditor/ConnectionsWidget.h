// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QPropertyTree;
class QDeepFilterProxyModel;

namespace ACE
{
class CSystemControl;
class CConnectionModel;
class CAudioTreeView;

class CConnectionsWidget final : public QWidget
{
	Q_OBJECT

public:

	CConnectionsWidget(QWidget* pParent = nullptr);
	virtual ~CConnectionsWidget() override;

	void SetControl(CSystemControl* pControl);
	void Reload();
	void BackupTreeViewStates();
	void RestoreTreeViewStates();

private slots:

	void OnContextMenu(QPoint const& pos);

private:

	// QObject
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	// ~QObject

	void RemoveSelectedConnection();
	void RefreshConnectionProperties();

	CSystemControl*              m_pControl;
	QPropertyTree* const         m_pConnectionProperties;
	QDeepFilterProxyModel* const m_pFilterProxyModel;
	CConnectionModel* const      m_pConnectionModel;
	CAudioTreeView* const        m_pTreeView;
};
} // namespace ACE
