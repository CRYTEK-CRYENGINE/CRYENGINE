// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QVBoxLayout;
class QFrame;
class QPropertyTree;
class QAdvancedTreeView;

namespace ACE
{
class CAudioControl;
class QConnectionModel;

class QConnectionsWidget : public QWidget
{
public:
	QConnectionsWidget(QWidget* pParent = nullptr);
	void Init();
	void SetControl(CAudioControl* pControl);
	void Reload();

private:
	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	void RemoveSelectedConnection();
	void RefreshConnectionProperties();

	CAudioControl*     m_pControl;
	QFrame*            m_pConnectionPropertiesFrame;
	QPropertyTree*     m_pConnectionProperties;
	QConnectionModel*  m_pConnectionModel;
	QAdvancedTreeView* m_pConnectionsView;
};
}
