// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "QToolWindowManagerCommon.h"
#include <QTabBar>
#include <QMenu>

class QToolButton;
class QToolWindowTabBar;

class QTabSelectionMenu : public QMenu
{
	Q_OBJECT
public:
	QTabSelectionMenu(QToolButton* pToolButton, QToolWindowTabBar* pParentTabbar);

private:
	QToolWindowTabBar* m_pParentTabbar;
};

class QTOOLWINDOWMANAGER_EXPORT QToolWindowTabBar : public QTabBar
{
	Q_OBJECT;
public:
	explicit QToolWindowTabBar(QWidget* parent = 0);
	~QToolWindowTabBar();

protected:
	void paintEvent(QPaintEvent *e);

private:
	void onSelectionMenuClicked();

	QToolButton* m_tabSelectionButton;
	QTabSelectionMenu* m_tabSelectionMenu;

	// Re-implement some qt5 changes for earlier versions.
#if QT_VERSION <= 0x050000
protected:
	bool bAutoHide;

public:

	void tabCloseRequested(int index)
	{
		QTabBar::tabCloseRequested(index);
	}

	bool autoHide() const
	{
		return bAutoHide;
	}

	void setAutoHide(bool hide)
	{
		bAutoHide = hide;
		updateHidden();
	}

private:
	void updateHidden()
	{
		if (autoHide())
		{
			setHidden(count() < 2);
		}
	}

protected:
	virtual void tabInserted (int index) Q_DECL_OVERRIDE
	{
		QTabBar::tabInserted(index);
		updateHidden();
	}

	virtual void tabRemoved(int index) Q_DECL_OVERRIDE
	{
		QTabBar::tabRemoved(index);
		updateHidden();
	}
#endif

};
