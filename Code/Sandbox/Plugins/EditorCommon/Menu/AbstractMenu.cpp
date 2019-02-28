// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractMenu.h"
#include "QtUtil.h"
#include "IEditor.h"
#include "ICommandManager.h"

struct CAbstractMenu::SMenuItem
{
	SMenuItem(int priority, int section)
		: m_priority(priority)
		, m_section(section)
	{
	}

	virtual void Build(IWidgetBuilder& widgetBuilder) const = 0;

	std::pair<int, int> MakeKey() const { return std::make_pair(m_section, m_priority); }

	int m_priority;
	int m_section;
};

struct CAbstractMenu::SActionItem : SMenuItem
{
	SActionItem(QAction* pAction, int priority, int section)
		: SMenuItem(priority, section)
		, m_pAction(pAction)
	{
	}

	virtual void Build(IWidgetBuilder& widgetBuilder) const
	{
		widgetBuilder.AddAction(m_pAction);
	}

	QAction* const m_pAction;
};

struct CAbstractMenu::SSubMenuItem : SMenuItem
{
	SSubMenuItem(CAbstractMenu* pMenu, int priority, int section)
		: SMenuItem(priority, section)
		, m_pMenu(pMenu)
	{
	}

	virtual void Build(IWidgetBuilder& widgetBuilder) const
	{
		std::unique_ptr<IWidgetBuilder> pSubMenuBuilder = widgetBuilder.AddMenu(m_pMenu);
		if (pSubMenuBuilder)
		{
			m_pMenu->Build(*pSubMenuBuilder);
		}
	}

	CAbstractMenu* const m_pMenu;
};

struct CAbstractMenu::SNamedSection
{
	explicit SNamedSection(int section)
		: m_section(section)
	{
	}

	const int m_section;
	string m_name;
};

CAbstractMenu::CAbstractMenu(const char* szName)
	: m_name(szName)
	, m_bEnabled(true)
{
}

CAbstractMenu::CAbstractMenu()
	: m_bEnabled(true)
{
}

CAbstractMenu::~CAbstractMenu()
{
}

bool CAbstractMenu::IsEmpty() const
{
	return m_sortedItems.empty();
}

int CAbstractMenu::GetNextEmptySection() const
{
	return IsEmpty() ? 0 : (GetMaxSection() + 1);
}

int CAbstractMenu::GetMaxSection() const
{
	CRY_ASSERT(!IsEmpty());
	return m_sortedItems.back()->m_section;
}

void CAbstractMenu::Clear()
{
	m_sortedItems.clear();

	m_subMenuItems.clear();
	m_actionItems.clear();

	m_subMenus.clear();
	m_actions.clear();
}

int CAbstractMenu::GetMaxPriority(int section) const
{
	CRY_ASSERT(!IsEmpty());
	CRY_ASSERT(section >= eSections_Min);
	const std::pair<int, int> adj(section + 1, ePriorities_Min);
	auto maxItemInSection = --std::lower_bound(m_sortedItems.begin(), m_sortedItems.end(), adj, [](const auto& lhp, const auto& adj)
	{
		return lhp->MakeKey() < adj;
	});
	return (*maxItemInSection)->m_priority;
}

int CAbstractMenu::GetDefaultSection() const
{
	return IsEmpty() ? 0 : GetMaxSection();
}

int CAbstractMenu::GetSectionFromHint(int sectionHint) const
{
	return sectionHint == eSections_Default ? GetDefaultSection() : sectionHint;
}

int CAbstractMenu::GetPriorityFromHint(int priorityHint, int section) const
{
	if (priorityHint == ePriorities_Append)
	{
		return !IsEmpty() ? GetMaxPriority(section) : 0;
	}
	else
	{
		return priorityHint;
	}
}

void CAbstractMenu::AddAction(QAction* pAction, int sectionHint, int priorityHint)
{
	const int section = GetSectionFromHint(sectionHint);
	m_actionItems.emplace_back(new SActionItem(pAction, GetPriorityFromHint(priorityHint, section), section));
	InsertItem(m_actionItems.back().get());
	signalActionAdded();
}

void CAbstractMenu::AddCommandAction(const char* szCommand, int sectionHint /*= eSections_Default*/, int priorityHint /*= ePriorities_Append*/)
{
	QAction* pAction = GetIEditor()->GetICommandManager()->GetAction(szCommand);

	if (!pAction)
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR_DBGBRK, "Command not found");

	AddAction(pAction, sectionHint, priorityHint);
}

QAction* CAbstractMenu::CreateAction(const QString& name, int sectionHint, int priorityHint)
{
	QAction* const pAction = new QAction(name, nullptr);
	m_actions.emplace_back(pAction);
	AddAction(pAction, sectionHint, priorityHint);
	return pAction;
}

QAction* CAbstractMenu::CreateAction(const QIcon& icon, const QString& name, int sectionHint, int priorityHint)
{
	QAction* const pAction = new QAction(icon, name, nullptr);
	m_actions.emplace_back(pAction);
	AddAction(pAction, sectionHint, priorityHint);
	return pAction;
}

CAbstractMenu* CAbstractMenu::CreateMenu(const char* szName, int sectionHint, int priorityHint)
{
	m_subMenus.emplace_back(new CAbstractMenu(szName));
	CAbstractMenu* const pSubMenu = m_subMenus.back().get();

	const int section = GetSectionFromHint(sectionHint);
	const int priority = GetPriorityFromHint(priorityHint, section);

	std::unique_ptr<SSubMenuItem> pSubMenuItem(new SSubMenuItem(pSubMenu, priority, section));

	InsertItem(pSubMenuItem.get());
	m_subMenuItems.push_back(std::move(pSubMenuItem));

	signalMenuAdded(pSubMenu);

	return pSubMenu;
}

CAbstractMenu* CAbstractMenu::CreateMenu(const QString& name, int sectionHint, int priorityHint)
{
	return CreateMenu(QtUtil::ToString(name).c_str(), sectionHint, priorityHint);
}

CAbstractMenu* CAbstractMenu::FindMenu(const char* szName)
{
	auto it = std::find_if(m_subMenus.begin(), m_subMenus.end(), [szName](const auto& other)
	{
		return other->m_name == szName;
	});
	return it != m_subMenus.end() ? it->get() : nullptr;
}

CAbstractMenu* CAbstractMenu::FindMenuRecursive(const char* szName)
{
	std::deque<CAbstractMenu*> queue;
	queue.insert(queue.end(), m_subMenus.begin(), m_subMenus.end());
	while (!queue.empty())
	{
		if (queue.front()->m_name == szName)
		{
			return queue.front();
		}
		queue.insert(queue.end(), queue.front()->m_subMenus.begin(), queue.front()->m_subMenus.end());
		queue.pop_front();
	}
	return nullptr;
}

void CAbstractMenu::SetSectionName(int section, const char* szName)
{
	auto it = std::find_if(m_sortedNamedSections.begin(), m_sortedNamedSections.end(), [section](const auto& other)
	{
		return other->m_section == section;
	});
	if (it != m_sortedNamedSections.end())
	{
		(*it)->m_name = szName;
	}
	else
	{
		std::unique_ptr<SNamedSection> namedSection(new SNamedSection(section));
		namedSection->m_name = szName;
		InsertNamedSection(std::move(namedSection));
	}
}

bool CAbstractMenu::IsNamedSection(int section) const
{
	return std::find_if(m_sortedNamedSections.begin(), m_sortedNamedSections.end(), [section](const auto& other)
	{
		return other->m_section == section;
	}) != m_sortedNamedSections.end();
}

const char* CAbstractMenu::GetSectionName(int section) const
{
	auto it = std::find_if(m_sortedNamedSections.begin(), m_sortedNamedSections.end(), [section](const auto& other)
	{
		return other->m_section == section;
	});
	if (it == m_sortedNamedSections.end())
	{
		return "";
	}
	else
	{
		return (*it)->m_name.c_str();
	}
}

int CAbstractMenu::FindSectionByName(const char* szName) const
{
	for (const auto& section : m_sortedNamedSections)
	{
		if (section.get()->m_name.Compare(szName) == 0)
		{
			return section.get()->m_section;
		}
	}
	return eSections_Default;
}

bool CAbstractMenu::ContainsAction(const QAction* pAction) const
{
	return m_actionItems.end() != std::find_if(m_actionItems.begin(), m_actionItems.end(), [pAction](const auto& other)
	{
		return other->m_pAction == pAction;
	});
}

const char* CAbstractMenu::GetName() const
{
	return m_name.c_str();
}

void CAbstractMenu::Build(IWidgetBuilder& widgetBuilder) const
{
	const size_t N = m_sortedItems.size();
	size_t i = 0;
	while (i < N)
	{
		const int section = m_sortedItems[i]->m_section;
		if (i > 0)
		{
			widgetBuilder.AddSection(IWidgetBuilder::SSection { section, GetSectionName(section) });
		}
		while (i < N && m_sortedItems[i]->m_section == section)
		{
			m_sortedItems[i]->Build(widgetBuilder);
			i++;
		}
	}

	//SetEnabled must be called last in order to iterate over all the actions and disabled them
	widgetBuilder.SetEnabled(m_bEnabled);
}

void CAbstractMenu::InsertItem(SMenuItem* pItem)
{
	m_sortedItems.insert(std::upper_bound(m_sortedItems.begin(), m_sortedItems.end(), pItem, [](const SMenuItem* lhp, const SMenuItem* rhp)
	{
		return lhp->MakeKey() < rhp->MakeKey();
	}), pItem);
}

void CAbstractMenu::InsertNamedSection(std::unique_ptr<SNamedSection>&& pNamedSection)
{
	const SNamedSection* const needle = pNamedSection.get();
	m_sortedNamedSections.insert(std::upper_bound(m_sortedNamedSections.begin(), m_sortedNamedSections.end(), needle, [](const auto& lhp, const auto& rhp)
	{
		return lhp->m_section < rhp->m_section;
	}), std::move(pNamedSection));
}

