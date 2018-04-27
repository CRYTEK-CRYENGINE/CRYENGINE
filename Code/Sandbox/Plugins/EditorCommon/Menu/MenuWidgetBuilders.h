// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "AbstractMenu.h"

class QMenu;
class QMenuBar;

namespace MenuWidgetBuilders
{

class EDITOR_COMMON_API CMenuBuilder : public CAbstractMenu::IWidgetBuilder
{
public:
	explicit CMenuBuilder(QMenu* pMenu, bool bClearMenu = true);

	virtual void AddAction(QAction* pAction) override;
	virtual void AddSection(const SSection& sec) override;
	virtual std::unique_ptr<IWidgetBuilder> AddMenu(const CAbstractMenu* pMenu) override;
	virtual void SetEnabled(bool enabled) override;

	//! Fills the QMenu with the actions of the abstract menu. This does not clear the QMenu.
	static void FillQMenu(QMenu* pMenu, CAbstractMenu* abstractMenu);

private:
	QMenu* const m_pMenu;
};

class EDITOR_COMMON_API CMenuBarBuilder : public CAbstractMenu::IWidgetBuilder
{
public:
	explicit CMenuBarBuilder(QMenuBar* pMenuBar);

	virtual void AddAction(QAction* pAction) override;
	virtual void AddSection(const SSection& sec) override;
	virtual std::unique_ptr<IWidgetBuilder> AddMenu(const CAbstractMenu* pMenu) override;
	virtual void SetEnabled(bool enabled) override;

private:
	QMenuBar* const m_pMenuBar;
};

} // namespace MenuWidgetBuilders
