// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PropertyTreeModel.h"

#include <CrySerialization/yasli/BinArchive.h>

#include <QWidget>

namespace PropertyTree2
{
CRowModel::CRowModel(CRowModel* pParent)
	: m_name("<root>")
	, m_pParent(pParent)
	, m_pWidget(nullptr)
	, m_dirty(DirtyFlag::Clean)
	, m_flags(0)
	, m_index(-1)
	, m_pFactory(nullptr)
{
}

CRowModel::CRowModel(const char* name, const char* label, CRowModel* pParent)
	: m_name(name)
	, m_label(label)
	, m_pWidget(nullptr)
	, m_pParent(pParent)
	, m_dirty(DirtyFlag::Clean)
	, m_flags(0)
	, m_index(-1)
{
	UpdateFlags();

	if (pParent)
	{
		pParent->AddChild(this);
	}

}

CRowModel::CRowModel(const char* name, const char* label, CRowModel* pParent, const yasli::TypeID& type)
	: m_name(name)
	, m_label(label)
	, m_type(type)
	, m_pWidget(nullptr)
	, m_pParent(pParent)
	, m_dirty(DirtyFlag::Clean)
	, m_flags(0)
	, m_index(-1)
{
	UpdateFlags();

	if (pParent)
	{
		pParent->AddChild(this);
	}
}

CRowModel::~CRowModel()
{
	if (m_pWidget)
	{
		m_pFactory->Release(m_pWidget);
	}
}

void CRowModel::SetWidgetAndType(const yasli::TypeID& type)
{
	if (type && !m_pWidget)
	{
		m_type = type;
		//create the widget mapped to m_type and the factory used to create it (will use that to return the widget to the pool on model destruction)
		m_pFactory = GetPropertyTreeWidgetFactory().GetFactory(m_type.name());

		CRY_ASSERT_MESSAGE(m_pFactory, "No factory registered for type %s", m_type.name());

		if (m_pFactory)
		{
			m_pWidget = m_pFactory->Create();
		}

		CRY_ASSERT_MESSAGE(m_pWidget, "No widget created for registered factory of type %s", m_type.name());

		IPropertyTreeWidget* pPropertyTreeWidget = GetPropertyTreeWidget();
		if (pPropertyTreeWidget)
		{
			pPropertyTreeWidget->SetRowModel(this);
		}

		if (m_pWidget && (m_flags & (Flags)Flag::ReadOnly))
		{
			m_pWidget->setEnabled(false);
		}

		MarkDirty();
	}
}

void CRowModel::SetType(const yasli::TypeID& type)
{
	m_type = type;
	MarkDirty();
}

void CRowModel::SetLabel(const QString& label)
{
	//We'll assume here that the only use cases are for containers, which may not have inlined children
	//Therefore the parent's AllChildrenHidden state will not be handled here
	if (label != m_label)
	{
		m_label = label;
		UpdateFlags();
	}
}

int CRowModel::GetIndex() const
{
	return m_index;
}

IPropertyTreeWidget* CRowModel::GetPropertyTreeWidget() const
{
	return qobject_cast<IPropertyTreeWidget*>(m_pWidget);
}

void CRowModel::AddChild(CRowModel* pChild)
{
	m_children.push_back(pChild);
	pChild->m_index = m_children.size() - 1;

	if (m_children.size() == 1)
	{
		MarkDirty(DirtyFlag::DirtyFirstChildren);
		if (pChild->IsHidden())
		{
			m_flags |= Flag::AllChildrenHidden;  //TODO : flag is reset somehow
		}
	}
	else if (m_flags & Flag::AllChildrenHidden)
	{
		if (!pChild->IsHidden())
		{
			m_flags &= ~Flag::AllChildrenHidden;
		}
	}

	if (!IsDirty())
	{
		MarkDirty();
	}
}

void CRowModel::MarkDirty(DirtyFlag flag /*= DirtyFlag::Dirty*/)
{
	if (!IsDirty())
	{
		m_dirty = flag;
	}

	PropertyTree2::CRowModel* pParent = m_pParent;
	while (pParent)
	{
		switch (pParent->m_dirty)
		{
		case DirtyFlag::Clean:
			pParent->m_dirty = DirtyFlag::DirtyChild;
			pParent = pParent->m_pParent;
			break;
		case DirtyFlag::NotVisited:
			CRY_ASSERT(false);   //This should not happen !
		case DirtyFlag::DirtyChild:
		case DirtyFlag::Dirty:
		case DirtyFlag::DirtyFirstChildren:
			pParent = nullptr;
			break;
		}
	}
}

void CRowModel::UpdateFlags()
{
	m_flags = 0;

	int i = 0;
	while (i < m_label.length())
	{
		const QChar labelDecorator = m_label[i];
		switch (labelDecorator.unicode())
		{
		case '+':
			m_flags |= Flag::ExpandByDefault;
			break;
		case '-':
			m_flags |= Flag::CollapseByDefault;
			break;
		case '!':
			m_flags |= Flag::ReadOnly;
			break;
		case '^':
			m_flags |= (Flag::Inline | Flag::Hidden);
			break;
		case '>':
		case '<':
			//Do nothing but skip these characters
			break;
		default:
			m_label = m_label.mid(i);
			return;
		}

		i++;
	}
}

void CRowModel::MarkClean() const
{
	m_dirty = DirtyFlag::Clean;
}

void CRowModel::MarkNotVisitedRecursive()
{
	m_dirty = DirtyFlag::NotVisited;

	for (_smart_ptr<CRowModel>& pChild : m_children)
	{
		pChild->MarkNotVisitedRecursive();
	}
}

void CRowModel::PruneNotVisitedChildren()
{
	auto childrenIterator = std::remove_if(m_children.begin(), m_children.end(), [](const _smart_ptr<CRowModel>& row) { return row->m_dirty == DirtyFlag::NotVisited; });
	if (childrenIterator != m_children.end())
	{
		MarkDirty();
		while (childrenIterator != m_children.end())
		{
			if (*childrenIterator)
			{
				(*childrenIterator)->m_pParent = nullptr;   //Detach this children from hierarchy
			}
			childrenIterator = m_children.erase(childrenIterator);
		}
	}

	const int count = m_children.size();
	if (count > 0)
	{
		for (int i = 0; i < count; i++)
		{
			const _smart_ptr<CRowModel>& pRow = m_children[i];
			pRow->m_index = i;

			if (pRow->HasChildren())
			{
				pRow->PruneNotVisitedChildren();
			}
		}
	}
	else
	{
		m_flags &= ~Flag::AllChildrenHidden;
	}
}

bool CRowModel::IsClean() const
{
	return m_dirty == DirtyFlag::Clean;
}

bool CRowModel::HasDirtyChildren() const
{
	switch (m_dirty)
	{
	case DirtyFlag::Dirty:
	case DirtyFlag::DirtyChild:
	case DirtyFlag::DirtyFirstChildren:
		return true;
	default:
		return false;
	}
}

bool CRowModel::HasFirstTimeChildren() const
{
	return m_dirty == DirtyFlag::DirtyFirstChildren;
}

bool CRowModel::IsDirty() const
{
	switch (m_dirty)
	{
	case DirtyFlag::Dirty:
	case DirtyFlag::DirtyFirstChildren:
		return true;
	default:
		return false;
	}
}

void CRowModel::Intersect(const CRowModel* pOther)
{
	//The root nodes are assumed equal or tested by the parent, here we will only test children
	if (pOther->HasChildren())
	{
		int i = 0;
		for (auto modelIterator = m_children.begin(); modelIterator != m_children.end();)
		{
			auto intersectableModelIterator = std::find_if(pOther->m_children.begin(), pOther->m_children.end(), [&](const _smart_ptr<CRowModel>& pRow)
				{
					return pRow->m_name == (*modelIterator)->m_name && pRow->m_type == (*modelIterator)->m_type;
				});

			bool removeChild = false;

			if (intersectableModelIterator != pOther->m_children.end())
			{
				if ((*intersectableModelIterator)->GetPropertyTreeWidget() && (*modelIterator)->GetPropertyTreeWidget())
				{
					if ((*modelIterator)->GetPropertyTreeWidget()->SupportsMultiEdit())
					{
						//Check that values are equal or set to a "multi-edit" placeholder value
						yasli::BinOArchive oar;
						(*intersectableModelIterator)->GetPropertyTreeWidget()->Serialize(oar);
						yasli::BinOArchive oar2;
						(*modelIterator)->GetPropertyTreeWidget()->Serialize(oar2);
						if (oar.length() != oar2.length() || 0 != memcmp(oar.buffer(), oar2.buffer(), oar.length()))
						{
							(*modelIterator)->GetPropertyTreeWidget()->SetMultiEditValue();
						}
					}
					else if (!(*modelIterator)->HasChildren())
					{
						//Widget doesn't support multi-edit and no children, remove row
						removeChild = true;
					}
					else
					{
						//Clear the invalid widget but leave row for children
						(*modelIterator)->m_pWidget = nullptr;
						(*modelIterator)->MarkDirty();
					}
				}

				if ((*modelIterator)->HasChildren())
				{
					(*modelIterator)->Intersect(*intersectableModelIterator);
				}
			}
			else
			{
				//Remove children not present in the other
				removeChild = true;
			}

			if (removeChild)
			{
				MarkDirty();
				modelIterator = m_children.erase(modelIterator);
			}
			else
			{
				//Update cached index
				(*modelIterator)->m_index = i;

				++modelIterator;
				i++;
			}
		}
	}
	else if (HasChildren())
	{
		m_children.clear();
		m_flags &= ~Flag::AllChildrenHidden;
	}
}
}
