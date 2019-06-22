// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "PropertyTreeIArchive.h"

#include <CrySandbox/ScopedVariableSetter.h>
#include <CrySerialization/yasli/Callback.h>
#include "IPropertyTreeWidget.h"

namespace PropertyTree2
{

template<typename ValueType>
bool PropertyTreeIArchive::ProcessSimpleRow(const char* name, ValueType& value)
{
	const CRowModel* pRow = FindRowInScope<ValueType>(name);
	if (pRow)
	{
		pRow->GetPropertyTreeWidget()->GetValue(value);
		return true;
	}
	else
	{
		return false;
	}
}

//////////////////////////////////////////////////////////////////////////

PropertyTreeIArchive::PropertyTreeIArchive(const CRowModel& root)
	: yasli::Archive(INPUT | EDIT)
	, m_pCurrentScope(&root)
	, m_lastVisitedIndex(-1)
{
	root.MarkClean();
}

PropertyTreeIArchive::~PropertyTreeIArchive()
{

}

bool PropertyTreeIArchive::operator()(yasli::StringInterface& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::WStringInterface& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(bool& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(char& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::i8& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::i16& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::i32& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::i64& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::u8& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::u16& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::u32& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(yasli::u64& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(float& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(double& value, const char* name, const char* label)
{
	return ProcessSimpleRow(name, value);
}

bool PropertyTreeIArchive::operator()(const yasli::Serializer& ser, const char* name, const char* label)
{
	const CRowModel* pRow = FindRowInScope(name, ser.type());
	if (pRow)
	{
		bool serializeChildren = true;
		if (pRow->GetPropertyTreeWidget())
		{
			pRow->GetPropertyTreeWidget()->GetValue(ser);
			serializeChildren = pRow->GetPropertyTreeWidget()->AllowChildSerialization();
		}

		if (ser && serializeChildren)
		{
			CScopedVariableSetter<const CRowModel*> resetScope(m_pCurrentScope, pRow);
			CScopedVariableSetter<int> resetIndex(m_lastVisitedIndex, -1);
			ser(*this);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool PropertyTreeIArchive::operator()(yasli::PointerInterface& pointerInterface, const char* name, const char* label)
{
	//using the fully qualified pointer type as row type id
	const CRowModel* pRow = FindRowInScope(name, pointerInterface.pointerType());
	if (pRow)
	{
		pRow->GetPropertyTreeWidget()->GetValue(pointerInterface);

		if (yasli::Serializer ser = pointerInterface.serializer())
		{
			CScopedVariableSetter<const CRowModel*> resetScope(m_pCurrentScope, pRow);
			CScopedVariableSetter<int> resetIndex(m_lastVisitedIndex, -1);

			ser(*this);
		}

		return true;
	}
	else
	{
		return false;
	}
}

bool PropertyTreeIArchive::operator()(yasli::ContainerInterface& container, const char* name, const char* label)
{
	const CRowModel* pRow = FindRowInScope(name, container.containerType());
	if (pRow)
	{
		if (container.size() > 0)
		{
			CScopedVariableSetter<const CRowModel*> resetScope(m_pCurrentScope, pRow);
			CScopedVariableSetter<int> resetIndex(m_lastVisitedIndex, -1);

			int i = 1;
			container.begin();
			do
			{
				const string itemString = string().Format("Item %d", i);
				container(*this, itemString, itemString);
				i++;
			}
			while (container.next());
		}

		//Action is applied on the array after its children have been serialized, otherwise the behavior would not be what we expect
		container.begin();
		pRow->GetPropertyTreeWidget()->GetValue(container);

		return true;
	}
	else
	{
		return false;
	}
}

bool PropertyTreeIArchive::operator()(yasli::CallbackInterface& callback, const char* name, const char* label)
{
	return callback.serializeValue(*this, name, label);
}

bool PropertyTreeIArchive::operator()(yasli::Object& obj, const char* name, const char* label)
{
	return (*this)(obj.serializer(), name, label);
}

bool PropertyTreeIArchive::openBlock(const char* name, const char* label, const char* icon /*= 0*/)
{
	const CRowModel* pRow = FindRowInScope(name, yasli::TypeID() /*intentional*/);
	if (pRow && pRow->HasChildren())
	{
		m_pCurrentScope = pRow;
		m_lastVisitedIndex = -1;
		return true;
	}
	else
	{
		return false;
	}
}

void PropertyTreeIArchive::closeBlock()
{
	m_pCurrentScope = m_pCurrentScope->GetParent();
	CRY_ASSERT_MESSAGE(m_pCurrentScope, "Cannot close a block at root scope");
	m_lastVisitedIndex = -1;
}

const PropertyTree2::CRowModel* PropertyTreeIArchive::FindRowInScope(const char* name, const yasli::TypeID& type)
{
	if (!m_pCurrentScope || !m_pCurrentScope->HasChildren())
	{
		return nullptr;
	}

	using namespace PropertyTree2;

	const std::vector<_smart_ptr<CRowModel>>& childArray = m_pCurrentScope->GetChildren();
	const int count = childArray.size();

	//First test the next index
	if (m_lastVisitedIndex != -1)
	{
		m_lastVisitedIndex++;

		if (m_lastVisitedIndex >= 0 && m_lastVisitedIndex < count)
		{
			const CRowModel* pRow = childArray[m_lastVisitedIndex];
			if (pRow->GetName() == name)
			{
				if (!type || pRow->GetType() == type)
				{
					return pRow;
				}
			}
		}
	}

	//Otherwise do a full search
	for (int i = 0; i < count; i++)
	{
		const CRowModel* pRow = childArray[i];
		if (pRow->GetName() == name)
		{
			if (!type || pRow->GetType() == type)
			{
				m_lastVisitedIndex = i;
				return pRow;
			}
		}
	}

	return nullptr;
}
}
