// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <QSearchBox.h>
#include "QAdvancedTreeView.h"

class QPopupWidget;
class QMenuTreeView;
class QDeepFilterProxyModel;
class QStandardItemModel;

class CTraySearchBox : public QSearchBox
{
	Q_OBJECT
	// Need to override the mouse move event so we can change the current item on move (like a menu does)
	class QMenuTreeView : public QAdvancedTreeView
	{
	protected:
		virtual void mouseMoveEvent(QMouseEvent* pEvent) override;
	};

public:
	CTraySearchBox(QWidget* pParent = nullptr);
	~CTraySearchBox();

	void                OnItemSelected(const QModelIndex& index);
	void                ShowSearchResults();
	void                OnFilter();

	QStandardItemModel* CreateMainMenuModel();
	void                AddActionsToModel(QMenu* pParentMenu, QStandardItemModel* pModel);

protected:
	virtual void keyPressEvent(QKeyEvent* pKeyEvent) override;

	void UpdatePlaceHolderText();

private:
	QPopupWidget*        m_pPopupWidget;
	QMenuTreeView*         m_pTreeView;
	QDeepFilterProxyModel* m_pFilterProxy;
};

#pragma once

