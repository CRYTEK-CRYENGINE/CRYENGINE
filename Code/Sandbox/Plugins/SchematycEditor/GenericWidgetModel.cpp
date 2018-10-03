// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GenericWidgetModel.h"

#include <Controls/DictionaryWidget.h>

#include <ProxyModels/MergingProxyModel.h>
#include <ProxyModels/ItemModelAttribute.h>

#include <CrySchematyc2/IFramework.h>
#include <CrySchematyc2/Script/IScriptGraph.h>
#include <CrySchematyc2/Script/IScriptRegistry.h>

#include "Util.h"

namespace Cry {
namespace SchematycEd {

//////////////////////////////////////////////////////////////////////////
CGenericWidgetDictionaryEntry::CGenericWidgetDictionaryEntry(CGenericWidgetDictionaryModel& dictionaryModel)
	: m_node(Type_Undefined)
	, m_dictionaryModel(dictionaryModel)
{
}

//////////////////////////////////////////////////////////////////////////
uint32 CGenericWidgetDictionaryEntry::GetType() const
{
	return m_node;
}

//////////////////////////////////////////////////////////////////////////
int32 CGenericWidgetDictionaryEntry::GetNumChildEntries() const
{
	return m_childs.size();
}

//////////////////////////////////////////////////////////////////////////
const CAbstractDictionaryEntry* CGenericWidgetDictionaryEntry::GetChildEntry(int32 index) const
{
	if ((index >= 0) && (index < m_childs.size()))
		return m_dictionaryModel.GetEntryByGUID(m_childs[index]);

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
const CAbstractDictionaryEntry* CGenericWidgetDictionaryEntry::GetParentEntry() const
{
	return m_dictionaryModel.GetEntryByGUID(m_parent);
}

//////////////////////////////////////////////////////////////////////////
QString  CGenericWidgetDictionaryEntry::GetToolTip(int32 columnIndex) const
{
	return m_description;
}

//////////////////////////////////////////////////////////////////////////
QString CGenericWidgetDictionaryEntry::GetName() const
{
	return m_name;
}

//////////////////////////////////////////////////////////////////////////
QString CGenericWidgetDictionaryEntry::GetFullName() const
{
	return m_fullName;
}

//////////////////////////////////////////////////////////////////////////
SGUID CGenericWidgetDictionaryEntry::GetTypeGUID()
{
	return m_guid;
}

//////////////////////////////////////////////////////////////////////////
QVariant CGenericWidgetDictionaryEntry::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
		case CGenericWidgetDictionaryModel::Column_Name:
		{
			return QVariant::fromValue(m_name);
		}
		case CGenericWidgetDictionaryModel::Column_Type:
		{
			return QVariant::fromValue(m_type);
		}
		default:
		{
			break;
		}
	}

	return QVariant();
}

//////////////////////////////////////////////////////////////////////////
void CGenericWidgetDictionaryModel::ClearScriptElementModel()
{
	m_entryGuid.clear();
	m_entryPool.clear();
	m_entryDict.clear();

	m_entryPool.reserve(128);
}

//////////////////////////////////////////////////////////////////////////
CGenericWidgetDictionaryEntry* CGenericWidgetDictionaryModel::BuildScriptElementEntry(const IScriptElement& element, const char* typeName)
{
	m_entryPool.emplace_back(*this);
	CGenericWidgetDictionaryEntry& entry = m_entryPool.back();

	entry.m_name = element.GetName();
	entry.m_guid = element.GetGUID();
	entry.m_node = CAbstractDictionaryEntry::Type_Entry;
	entry.m_type = typeName;

	m_entryDict.insert(std::make_pair(entry.m_guid, &entry));
	return &entry;
}

void CGenericWidgetDictionaryModel::BuildScriptElementParent(const IScriptElement& element, CGenericWidgetDictionaryEntry* childEntry, const std::vector<EScriptElementType>& parentType)
{
	const IScriptElement* parent = element.GetParent();
	for (auto& type : parentType)
	{
		if (parent && (parent->GetElementType() == type))
		{
			CGenericWidgetDictionaryEntry* parentEntry = GetEntryByGUID(parent->GetGUID());
			if (!parentEntry)
				break;

			parentEntry->m_childs.emplace_back(childEntry->m_guid);
			parentEntry->m_node = CAbstractDictionaryEntry::Type_Folder;

			childEntry->m_parent = parent->GetGUID();
			return;
		}
	}
	
	m_entryGuid.push_back(childEntry->m_guid);
	return;
}

//////////////////////////////////////////////////////////////////////////
int32 CGenericWidgetDictionaryModel::GetNumEntries() const
{
	return m_entryGuid.size();
}

//////////////////////////////////////////////////////////////////////////
const CAbstractDictionaryEntry* CGenericWidgetDictionaryModel::GetEntry(int32 index) const
{
	if (index < m_entryGuid.size())
	{
		return GetEntryByGUID(m_entryGuid[index]);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
int32 CGenericWidgetDictionaryModel::GetNumColumns() const
{
	return Column_Count;
};

//////////////////////////////////////////////////////////////////////////
QString CGenericWidgetDictionaryModel::GetColumnName(int32 index) const
{	
	auto pAttribute = CGenericWidgetDictionaryModel::GetColumnAttributeInternal(index);
	if (pAttribute)
	{
		return QString(pAttribute->GetName());
	}

	return QString();
}

//////////////////////////////////////////////////////////////////////////
int32 CGenericWidgetDictionaryModel::GetDefaultFilterColumn() const
{
	return Column_Type;
}

//////////////////////////////////////////////////////////////////////////
int32 CGenericWidgetDictionaryModel::GetDefaultSortColumn() const
{
	return Column_Name;
}

//////////////////////////////////////////////////////////////////////////
const CItemModelAttribute* CGenericWidgetDictionaryModel::GetColumnAttribute(int32 index) const
{
	return GetColumnAttributeInternal(index);
}

//////////////////////////////////////////////////////////////////////////
CGenericWidgetDictionaryEntry* CGenericWidgetDictionaryModel::GetEntryByGUID(const Schematyc2::SGUID& guid) const
{
	auto it = m_entryDict.find(guid);
	if (it != m_entryDict.end())
	{
		return it->second;
	}

	return nullptr;
}

const CItemModelAttribute* CGenericWidgetDictionaryModel::GetColumnAttributeInternal(int column)
{
	static CItemModelAttribute Name("Name", &Attributes::s_stringAttributeType);
	static CItemModelAttribute Type("Type", &Attributes::s_stringAttributeType);
	static CItemModelAttribute None("    ", &Attributes::s_stringAttributeType);

	switch (column)
	{
		case Column_Name: return &Name;
		case Column_Type: return &Type;
	}

	return &None;
}

} //namespace SchematycEd
} //namespace Cry
