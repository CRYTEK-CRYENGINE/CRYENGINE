// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeModel.h"

#include <CrySerialization/yasli/BinArchive.h>

namespace PropertyTree2
{

	CRowModel::CRowModel(const char* name, const char* label, CRowModel* parent)
		: m_name(name)
		, m_label(label)
		, m_widget(nullptr)
		, m_parent(parent)
		, m_dirty(DirtyFlag::Clean)
		, m_flags(0)
		, m_index(-1)
	{
		UpdateFlags();

		if (parent)
			parent->AddChild(this);

	}

	CRowModel::CRowModel(const char* name, const char* label, CRowModel* parent, const yasli::TypeID& type)
		: m_name(name)
		, m_label(label)
		, m_type(type)
		, m_widget(nullptr)
		, m_parent(parent)
		, m_dirty(DirtyFlag::Clean)
		, m_flags(0)
		, m_index(-1)
	{
		UpdateFlags();

		if (parent)
			parent->AddChild(this);
	}

	CRowModel::CRowModel(CRowModel* parent)
		: m_name("<root>")
		, m_parent(parent)
		, m_widget(nullptr)
		, m_dirty(DirtyFlag::Clean)
		, m_flags(0)
		, m_index(-1)
	{
	}

	CRowModel::~CRowModel()
	{
		if (m_widget)
		{
			m_widget->setParent(nullptr);
			m_widget->deleteLater();
		}
	}

	void CRowModel::SetWidgetAndType(const yasli::TypeID& type)
	{
		if (type && !m_widget)
		{
			m_type = type;
			m_widget = GetPropertyTreeWidgetFactory().Create(m_type.name());
			CRY_ASSERT_MESSAGE(m_widget, "No widget registered for type %s", m_type.name());

			auto ptWidget = GetPropertyTreeWidget();
			if (ptWidget)
				ptWidget->SetRowModel(this);

			if (m_widget && (m_flags & (Flags)Flag::ReadOnly))
				m_widget->setEnabled(false);

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
		if(label != m_label)
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
		return qobject_cast<IPropertyTreeWidget*>(m_widget);
	}

	void CRowModel::AddChild(CRowModel* child)
	{
 		m_children.push_back(child);
		child->m_index = m_children.size() - 1;

		if (m_children.size() == 1)
		{
			MarkDirty(DirtyFlag::DirtyFirstChildren);
			if (child->IsHidden())
				m_flags |= Flag::AllChildrenHidden;//TODO : flag is reset somehow
		}
		else if(m_flags & Flag::AllChildrenHidden)
		{
			if (!child->IsHidden())
				m_flags &= ~Flag::AllChildrenHidden;
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

		auto parent = m_parent;
		while (parent)
		{
			switch (parent->m_dirty)
			{
			case DirtyFlag::Clean:
				parent->m_dirty = DirtyFlag::DirtyChild;
				parent = parent->m_parent;
				break;
			case DirtyFlag::NotVisited:
				CRY_ASSERT(false); //This should not happen !
			case DirtyFlag::DirtyChild:
			case DirtyFlag::Dirty:
			case DirtyFlag::DirtyFirstChildren:
				parent = nullptr;
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
			const QChar c = m_label[i];
			switch (c.unicode())
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

		for (auto& child : m_children)
		{
			child->MarkNotVisitedRecursive();
		}
	}

	void CRowModel::PruneNotVisitedChildren()
	{
		auto it = std::remove_if(m_children.begin(), m_children.end(), [](const _smart_ptr<CRowModel>& row) { return row->m_dirty == DirtyFlag::NotVisited; });
		if (it != m_children.end())
		{
			MarkDirty();
			while (it != m_children.end())
			{
				if(*it)
					(*it)->m_parent = nullptr; //Detach this children from hierarchy
				it = m_children.erase(it);
			}
		}

		const int count = m_children.size();
		if(count > 0)
		{
			for (int i = 0; i < count; i++)
			{
				const auto& row = m_children[i];
				row->m_index = i;
				if (row->HasChildren())
					row->PruneNotVisitedChildren();
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

	void CRowModel::Intersect(const CRowModel* other)
	{
		//The root nodes are assumed equal or tested by the parent, here we will only test children
		if (other->HasChildren())
		{
			int i = 0;
			for (auto it = m_children.begin(); it != m_children.end();)
			{
				auto otherIt = std::find_if(other->m_children.begin(), other->m_children.end(), [&](const _smart_ptr<CRowModel>& row)
				{
					return row->m_name == (*it)->m_name && row->m_type == (*it)->m_type;
				});

				bool bRemoveChild = false;

				if (otherIt != other->m_children.end())
				{
					if ((*otherIt)->GetPropertyTreeWidget() && (*it)->GetPropertyTreeWidget())
					{
						if ((*it)->GetPropertyTreeWidget()->SupportsMultiEdit())
						{
							//Check that values are equal or set to a "multi-edit" placeholder value
							yasli::BinOArchive oar;
							(*otherIt)->GetPropertyTreeWidget()->Serialize(oar);
							yasli::BinOArchive oar2;
							(*it)->GetPropertyTreeWidget()->Serialize(oar2);
							if (oar.length() != oar2.length() || 0 != memcmp(oar.buffer(), oar2.buffer(), oar.length()))
							{
								(*it)->GetPropertyTreeWidget()->SetMultiEditValue();
							}
						}
						else if(!(*it)->HasChildren())
						{
							//Widget doesn't support multi-edit and no children, remove row
							bRemoveChild = true;
						}
						else
						{
							//Clear the invalid widget but leave row for children
							(*it)->m_widget = nullptr;
							(*it)->MarkDirty();
						}
					}

					if ((*it)->HasChildren())
					{
						(*it)->Intersect(*otherIt);
					}
				}
				else
				{
					//Remove children not present in the other
					bRemoveChild = true;
				}

				if(bRemoveChild)
				{
					MarkDirty();
					it = m_children.erase(it);
				}
				else
				{
					//Update cached index
					(*it)->m_index = i;

					++it;
					i++;
				}
			}
		}
		else if(HasChildren())
		{
			m_children.clear();
			m_flags &= ~Flag::AllChildrenHidden;
		}
	}
}


