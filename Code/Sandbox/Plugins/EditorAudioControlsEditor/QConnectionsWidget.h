// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QVBoxLayout;
class QFrame;
class QPropertyTree;

namespace ACE
{
class CAudioControl;
class QConnectionModel;
class CAdvancedTreeView;

class QConnectionsWidget : public QWidget
{
public:

	QConnectionsWidget(QWidget* pParent = nullptr);
	virtual ~QConnectionsWidget() override;

	void Init();
	void SetControl(CAudioControl* pControl);
	void Reload();
	void BackupTreeViewStates();
	void RestoreTreeViewStates();

private:

	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	void RemoveSelectedConnection();
	void RefreshConnectionProperties();

	CAudioControl*          m_pControl;
	QFrame*                 m_pConnectionPropertiesFrame;
	QPropertyTree*          m_pConnectionProperties;
	QConnectionModel*       m_pConnectionModel;
	CAdvancedTreeView*      m_pConnectionsView;
};
} // namespace ACE
