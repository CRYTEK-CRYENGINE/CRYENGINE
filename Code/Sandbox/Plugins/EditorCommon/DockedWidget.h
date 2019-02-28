// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

template<class Widget>
class DockedWidget
{
public:
	DockedWidget(QMainWindow* main, cstr title, Qt::DockWidgetArea area, Qt::WindowFlags flags = 0)
		: m_pDock(new QDockWidget(title, main, flags))
		, m_pWidget(new Widget(m_pDock))
	{
		m_pDock->setWidget(m_pWidget);
		main->addDockWidget(area, m_pDock);
	}

	DockedWidget(QMainWindow* main, Widget* pWidget, cstr title, Qt::DockWidgetArea area, Qt::WindowFlags flags = 0)
		: m_pDock(new QDockWidget(title, main, flags))
		, m_pWidget(pWidget)
	{
		m_pWidget->setParent(m_pDock);
		m_pDock->setWidget(m_pWidget);
		main->addDockWidget(area, m_pDock);
	}

	Widget*      widget() const { return m_pWidget; }
	QDockWidget* dock() const   { return m_pDock; }

private:
	QDockWidget* m_pDock;
	Widget*      m_pWidget;
};

