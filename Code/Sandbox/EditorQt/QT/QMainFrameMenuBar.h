// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QMenuBar;
class QStandardItemModel;

class QMainFrameMenuBar : public QWidget
{
	Q_OBJECT
public:
	QMainFrameMenuBar(QMenuBar* pMenuBar = 0, QWidget* pParent = 0);
	void ShowSearchResults();

protected:
	void                OnItemSelected(const QModelIndex& index);

	virtual void        paintEvent(QPaintEvent* pEvent) override;

	QStandardItemModel* CreateMainMenuModel();
	void                AddActionsToModel(QMenu* pParentMenu, QStandardItemModel* pModel);
	void                OnFilter();

protected:
	QMenuBar* m_pMenuBar;
};

