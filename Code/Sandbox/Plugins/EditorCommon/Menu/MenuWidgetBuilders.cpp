// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MenuWidgetBuilders.h"
#include "QtUtil.h"
#include "QControls.h"

#include <QMenu>
#include <QMenuBar>

namespace MenuWidgetBuilders
{

namespace Private_MenuWidgetBuilders
{

void InitMenu(QMenu* pMenu, const CAbstractMenu* pAbstractMenu)
{
	const string name = pAbstractMenu->GetName();
	pMenu->setObjectName(QtUtil::ToQString(name));
	QObject::connect(pMenu, &QMenu::aboutToShow, [pAbstractMenu, pMenu]()
	{
		pAbstractMenu->signalAboutToShow(const_cast<CAbstractMenu*>(pAbstractMenu));
		pAbstractMenu->Build(CMenuBuilder(pMenu));
	});
}

} // namespace Private_MenuWidgetBuilders

// ==================================================

CMenuBuilder::CMenuBuilder(QMenu* pMenu, bool bClearMenu /*= true*/)
	: m_pMenu(pMenu)
{
	if(bClearMenu)
		m_pMenu->clear();
}

void CMenuBuilder::AddAction(QAction* pAction)
{
	m_pMenu->addAction(pAction);
}

void CMenuBuilder::AddSection(const SSection& sec)
{
	if (!sec.m_name.empty())
	{
		QAction* const pAction = new QMenuLabelSeparator(sec.m_name.c_str());
		m_pMenu->addAction(pAction);
		pAction->setParent(m_pMenu); // Transfer the ownership of the action to the menu.
	}
	else
	{
		m_pMenu->addSeparator();
	}
}

std::unique_ptr<CAbstractMenu::IWidgetBuilder> CMenuBuilder::AddMenu(const CAbstractMenu* pMenu)
{
	using namespace Private_MenuWidgetBuilders;
	const QString name = pMenu->GetName();
	QMenu* const pSubMenu = m_pMenu->addMenu(name);
	InitMenu(pSubMenu, pMenu);
	return std::unique_ptr<IWidgetBuilder>(new CMenuBuilder(pSubMenu));
}

void CMenuBuilder::SetEnabled(bool enabled)
{
	if (!enabled)
	{
		for (auto& action : m_pMenu->actions())
		{
			action->setEnabled(false);
		}
	}
}

void CMenuBuilder::FillQMenu(QMenu* pMenu, CAbstractMenu* abstractMenu)
{
	CMenuBuilder builder(pMenu, false);
	abstractMenu->Build(builder);
}

// ==================================================

CMenuBarBuilder::CMenuBarBuilder(QMenuBar* pMenuBar)
	: m_pMenuBar(pMenuBar)
{
	m_pMenuBar->clear();
}

void CMenuBarBuilder::AddAction(QAction* pAction)
{
	m_pMenuBar->addAction(pAction);
}

void CMenuBarBuilder::AddSection(const SSection&)
{
	m_pMenuBar->addSeparator();
}

std::unique_ptr<CAbstractMenu::IWidgetBuilder> CMenuBarBuilder::AddMenu(const CAbstractMenu* pMenu)
{
	using namespace Private_MenuWidgetBuilders;
	const QString name = pMenu->GetName();
	QMenu* const pSubMenu = m_pMenuBar->addMenu(name);
	InitMenu(pSubMenu, pMenu);
	return std::unique_ptr<IWidgetBuilder>(new CMenuBuilder(pSubMenu));
}

void CMenuBarBuilder::SetEnabled(bool enabled)
{
	if (!enabled)
	{
		for (auto& action : m_pMenuBar->actions())
		{
			action->setEnabled(false);
		}
	}
}

} // namespace MenuWidgetBuilders

