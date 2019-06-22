// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeOArchive.h"

#include <CrySerialization/yasli/Callback.h>
#include "IPropertyTreeWidget.h"

namespace PropertyTree2
{
template<typename ValueType>
void PropertyTreeOArchive::ProcessSimpleRow(const ValueType& value, const char* name, const char* label)
{
	CRowModel* pRow = FindOrCreateRowInScope<ValueType>(name, label);
	pRow->SetWidgetAndType<ValueType>();
	pRow->GetPropertyTreeWidget()->SetValue(value, *this);
}

//////////////////////////////////////////////////////////////////////////

PropertyTreeOArchive::PropertyTreeOArchive(CRowModel& root)
	: yasli::Archive(OUTPUT | EDIT | DOCUMENTATION)
	, m_currentScope(&root)
{
	//If the root object has no children we can optimize populating it
	m_firstPopulate = !root.HasChildren();

	root.MarkClean();
}

PropertyTreeOArchive::~PropertyTreeOArchive()
{

}

bool PropertyTreeOArchive::operator()(yasli::StringInterface& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::WStringInterface& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(bool& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(char& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::i8& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::i16& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::i32& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::i64& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::u8& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::u16& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::u32& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(yasli::u64& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(float& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(double& value, const char* name, const char* label)
{
	ProcessSimpleRow(value, name, label);
	return true;
}

bool PropertyTreeOArchive::operator()(const yasli::Serializer& ser, const char* name, const char* label)
{
	CRowModel* pRow = FindOrCreateRowInScope(name, label, ser.type());

	if (!pRow->IsWidgetSet() && GetPropertyTreeWidgetFactory().IsRegistered(ser.type().name()))
	{
		pRow->SetWidgetAndType(ser.type());
	}

	bool serializeChildren = true;
	if (pRow->GetPropertyTreeWidget())
	{
		pRow->GetPropertyTreeWidget()->SetValue(ser, *this);
		serializeChildren = pRow->GetPropertyTreeWidget()->AllowChildSerialization();
	}

	if (ser && serializeChildren)
	{
		EnterScope(pRow);
		ser(*this);
		ExitScope();
	}

	return true;
}

bool PropertyTreeOArchive::operator()(yasli::PointerInterface& ptr, const char* name, const char* label)
{
	//using the fully qualified pointer type as row type id
	CRowModel* pRow = FindOrCreateRowInScope(name, label, ptr.pointerType());
	if (!pRow->IsWidgetSet())
	{
		pRow->SetWidgetAndType<yasli::PointerInterface>();
		pRow->SetType(ptr.pointerType());
	}

	pRow->GetPropertyTreeWidget()->SetValue(ptr, *this);

	if (yasli::Serializer ser = ptr.serializer())
	{
		EnterScope(pRow);
		ser(*this);
		ExitScope();
	}

	return true;
}

bool PropertyTreeOArchive::operator()(yasli::ContainerInterface& container, const char* name, const char* label)
{
	CRowModel* pRow = FindOrCreateRowInScope(name, label, container.containerType());

	if (!pRow->IsWidgetSet())
	{
		//We are setting back to ContainerInterface and not to the container type because it is the  typename that creates the CArrayWidget widget
		//The	REGISTER_PROPERTY_WIDGET(ContainerInterface,	PropertyTree2::CArrayWidget) macro does the factory mapping
		pRow->SetWidgetAndType<yasli::ContainerInterface>();
		//set back to the actual container type
		pRow->SetType(container.containerType());

		if (container.isFixedSize())
		{
			pRow->SetLabel(QString("%1 (%2 Items)").arg(label).arg(container.size()));
		}
	}

	if (!container.isFixedSize())
	{
		pRow->SetLabel(QString("%1 (%2 Items)").arg(label).arg(container.size()));
	}

	pRow->GetPropertyTreeWidget()->SetValue(container, *this);

	if (container.size() > 0)
	{
		EnterScope(pRow);

		int i = 1;
		container.begin();
		do
		{
			const string itemString = string().Format("Item %d", i);
			container(*this, itemString, itemString);
			i++;
		}
		while (container.next());

		ExitScope();
	}

	return true;
}

bool PropertyTreeOArchive::operator()(yasli::CallbackInterface& callback, const char* name, const char* label)
{
	CRowModel* pRow = FindRowInScope(name);
	bool shouldConnect = pRow ? !pRow->IsWidgetSet() : true;

	if (!callback.serializeValue(*this, name, label))
	{
		return false;
	}

	//attach the callback otherwise
	pRow = GetLastVisitedRow();
	if (shouldConnect && pRow->GetPropertyTreeWidget())
	{
		yasli::CallbackInterface* pClone = callback.clone();
		pRow->GetPropertyTreeWidget()->signalChanged.Connect([pClone, pRow]()
			{
				auto applyLambda = [pRow](void* arg, const yasli::TypeID& type)
				                   {
				                     pRow->GetPropertyTreeWidget()->GetValue(arg, type);
													 };
				pClone->call(applyLambda);
			});
	}

	return true;
}

bool PropertyTreeOArchive::operator()(yasli::Object& obj, const char* name, const char* label)
{
	return (*this)(obj.serializer(), name, label);
}

bool PropertyTreeOArchive::openBlock(const char* name, const char* label, const char* icon /*= 0*/)
{
	CRowModel* pRow = FindOrCreateRowInScope(name, label, yasli::TypeID());
	EnterScope(pRow);
	return true;
}

void PropertyTreeOArchive::closeBlock()
{
	ExitScope();
}

void PropertyTreeOArchive::EnterScope(CRowModel* pScope)
{
	m_scopeStack.push_back(m_currentScope);
	m_currentScope = Scope(pScope);
}

void PropertyTreeOArchive::ExitScope()
{
	m_currentScope = m_scopeStack.back();
	m_scopeStack.pop_back();
}

PropertyTree2::CRowModel* PropertyTreeOArchive::FindOrCreateRowInScope(const char* name, const char* label, const yasli::TypeID& type)
{
	CRY_ASSERT(m_currentScope.m_pScopeRow);

	if (!m_firstPopulate)  //First populate will always create a row for every row visited
	{
		if (m_currentScope.m_pScopeRow->HasChildren())
		{
			const std::vector<_smart_ptr<CRowModel>>& childArray = m_currentScope.m_pScopeRow->GetChildren();
			const int count = childArray.size();

			//first test index
			if (m_currentScope.m_lastVisitedIndex != -1)
			{
				m_currentScope.m_lastVisitedIndex++;

				if (m_currentScope.m_lastVisitedIndex >= 0 && m_currentScope.m_lastVisitedIndex < count)
				{
					CRowModel* pRow = childArray[m_currentScope.m_lastVisitedIndex];
					if (pRow->GetName() == name)
					{
						if (!type || pRow->GetType() == type)
						{
							pRow->MarkClean();
							return pRow;
						}
					}
				}
			}

			//otherwise search for it
			for (int i = 0; i < count; i++)
			{
				CRowModel* pRow = childArray[i];
				if (pRow->GetName() == name)
				{
					if (!type || pRow->GetType() == type)
					{
						m_currentScope.m_lastVisitedIndex = i;
						pRow->MarkClean();
						return pRow;
					}
				}
			}
		}
	}

	//create if not found
	m_currentScope.m_lastVisitedIndex = m_currentScope.m_pScopeRow->GetChildren().size();
	return new CRowModel(name, label, m_currentScope.m_pScopeRow, type);
}

PropertyTree2::CRowModel* PropertyTreeOArchive::FindRowInScope(const char* name)
{
	CRY_ASSERT(m_currentScope.m_pScopeRow);

	if (m_currentScope.m_pScopeRow->HasChildren())
	{
		const std::vector<_smart_ptr<CRowModel>>& childArray = m_currentScope.m_pScopeRow->GetChildren();
		const int count = childArray.size();

		for (int i = 0; i < count; i++)
		{
			CRowModel* pRow = childArray[i];
			if (pRow->GetName() == name)
			{
				return pRow;
			}
		}
	}

	return nullptr;
}

PropertyTree2::CRowModel* PropertyTreeOArchive::GetLastVisitedRow()
{
	CRY_ASSERT(m_currentScope.m_pScopeRow);
	if (m_currentScope.m_lastVisitedIndex >= 0 && m_currentScope.m_lastVisitedIndex < m_currentScope.m_pScopeRow->GetChildren().size())
	{
		return m_currentScope.m_pScopeRow->GetChildren()[m_currentScope.m_lastVisitedIndex];
	}
	else
	{
		return nullptr;
	}
}

void PropertyTreeOArchive::documentLastField(const char* docString)
{
	CRowModel* pRow = GetLastVisitedRow();
	if (pRow)
	{
		pRow->SetTooltip(docString);
	}
	else
	{
		CRY_ASSERT_MESSAGE(false, "Could not resolve last visited row");
	}
}

}
