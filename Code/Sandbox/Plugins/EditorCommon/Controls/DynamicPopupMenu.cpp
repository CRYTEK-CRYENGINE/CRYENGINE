// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DynamicPopupMenu.h"

#include "Commands/ICommandManager.h"
#include "Commands/QCommandAction.h"
#include "EditorCommonInit.h"

#include <IEditor.h>
#include <CryIcon.h>

#include <QCursor>
#include <QMenu>

#include <algorithm>

//////////////////////////////////////////////////////////////////////////
class CPopupMenuItemCommand : public CPopupMenuItem
{
public:
	CPopupMenuItemCommand(const char* text, const char* command)
		: CPopupMenuItem(text)
		, m_command(command)
		, m_shouldUseText(true)
	{
		m_function = [&]()
		{
			GetIEditor()->GetICommandManager()->Execute(m_command);
		};
	}

	CPopupMenuItemCommand(const char* command)
		: CPopupMenuItem(command)
		, m_command(command)
		, m_shouldUseText(false)
	{
		m_function = [&]()
		{
			GetIEditor()->GetICommandManager()->Execute(m_command);
		};
	}

	virtual bool IsEditorCommand()  { return true; }
	const char*  GetCommand() const { return m_command; }
	//We are using the item text instead of editor command description for the generated qaction text
	bool ShouldUseText() const { return m_shouldUseText; }
private:
	string m_command;
	bool m_shouldUseText;
};

/////////////////////////////////////////////////////////////////////////
class QPopupMenuItemAction : public QAction
{
public:
	QPopupMenuItemAction(const std::shared_ptr<CPopupMenuItem>& item, QObject* parent);
	~QPopupMenuItemAction();

	void OnTriggered();
	void OnHovered();

private:
	std::shared_ptr<CPopupMenuItem> m_item;
};

QPopupMenuItemAction::QPopupMenuItemAction(const std::shared_ptr<CPopupMenuItem>& item, QObject* parent)
	: QAction(QString(item->Text()), parent)
	, m_item(item)
{
	//TODO : handle hotkeys through commands or having hotkeys as a KeyboardShortcut embedded here
	if (m_item->IsChecked())
	{
		setCheckable(true);
		setChecked(true);
	}
	setEnabled(m_item->IsEnabled());
	if (m_item->IsDefault())
	{
		QFont boldFont = font();
		boldFont.setBold(true);
		setFont(boldFont);
	}

	connect(this, &QPopupMenuItemAction::triggered, this, &QPopupMenuItemAction::OnTriggered);
	connect(this, &QPopupMenuItemAction::hovered, this, &QPopupMenuItemAction::OnHovered);
}

QPopupMenuItemAction::~QPopupMenuItemAction()
{
}

void QPopupMenuItemAction::OnTriggered()
{
	m_item->Call();
}

void QPopupMenuItemAction::OnHovered()
{
	if (m_item->m_hoverFunc)
		m_item->m_hoverFunc();
}

CDynamicPopupMenu::CDynamicPopupMenu()
	: m_root(CPopupMenuItem(""))
{
}

void CDynamicPopupMenu::Clear()
{
	m_root.GetChildren().clear();
}

CPopupMenuItem& CPopupMenuItem::Add(const char* text)
{
	CPopupMenuItem* item = new CPopupMenuItem(text);
	AddChildren(item);
	return *item;
}

CPopupMenuItem& CPopupMenuItem::Add(const char* text, const std::function<void()>& function)
{
	CPopupMenuItem* item = new CPopupMenuItem(text);
	item->m_function = function;
	AddChildren(item);
	return *item;
}

CPopupMenuItem& CPopupMenuItem::Add(const char* text, const char* icon, const std::function<void()>& function)
{
	CPopupMenuItem* item = new CPopupMenuItem(text);
	item->m_function = function;
	item->m_icon = icon;
	AddChildren(item);
	return *item;
}

CPopupMenuItem& CPopupMenuItem::AddSeparator()
{
	return Add("-");
}

CPopupMenuItem& CPopupMenuItem::AddCommand(const char* text, string commandToExecute)
{
	CPopupMenuItemCommand* item = new CPopupMenuItemCommand(text, commandToExecute);
	AddChildren(item);
	return *item;
}

CPopupMenuItem& CPopupMenuItem::AddCommand(string commandToExecute)
{
	CPopupMenuItemCommand* item = new CPopupMenuItemCommand(commandToExecute);
	AddChildren(item);
	return *item;
}

CPopupMenuItem* CPopupMenuItem::Find(const char* text)
{
	for (Children::iterator it = m_children.begin(); it != m_children.end(); ++it)
	{
		CPopupMenuItem* item = it->get();
		if (strcmp(item->Text(), text) == 0)
			return item;
	}
	return 0;
}

QMenu* CDynamicPopupMenu::CreateQMenu()
{
	QMenu* menu = new QMenu();
	PopulateQMenu(menu);
	if (m_onHide)
	{
		QObject::connect(menu, &QMenu::aboutToHide, m_onHide);
	}
	return menu;
}

void CDynamicPopupMenu::PopulateQMenu(class QMenu* menu)
{
	if (m_root.GetChildren().empty())
		return;
	else
		PopulateQMenu(menu, &m_root);
}

void CDynamicPopupMenu::PopulateQMenu(class QMenu* menu, CPopupMenuItem* parentItem)
{
	for (auto& childItem : parentItem->GetChildren())
	{
		if (childItem->GetChildren().empty())
		{
			string text = childItem->Text();
			if (text == "-")
				menu->addSeparator();
			else if (childItem->IsEditorCommand())
			{

				CPopupMenuItemCommand* pPopupMenuItem = static_cast<CPopupMenuItemCommand*>(childItem.get());
				const char* cmd = pPopupMenuItem->GetCommand();
				
				//If we want to use all default ui data from this command or override with our own text
				if (!pPopupMenuItem->ShouldUseText())
				{
					QAction* action = GetIEditor()->GetICommandManager()->GetAction(cmd);
					action->setEnabled(childItem->IsEnabled());
					menu->addAction(action);
				}
				else
				{
					QAction* action = GetIEditor()->GetICommandManager()->CreateNewAction(cmd);
					action->setText(QString(text));
					action->setEnabled(childItem->IsEnabled());
					menu->addAction(action);
				}
			}
			else
			{
				QAction* action = new QPopupMenuItemAction(childItem, menu);
				if (!childItem->m_icon.empty())
				{
					action->setIcon(CryIcon(childItem->m_icon.c_str()));
				}
				menu->addAction(action);
			}
		}
		else
		{
			QMenu* subMenu = menu->addMenu(childItem->Text());
			PopulateQMenu(subMenu, childItem.get());
		}
	}
}

void CDynamicPopupMenu::Spawn(int x, int y)
{
	QMenu* menu = CreateQMenu();
	menu->exec(QPoint(x, y)); // Should already trigger an action
}

void CDynamicPopupMenu::SpawnAtCursor()
{
	Spawn(QCursor::pos().x(), QCursor::pos().y());
}
