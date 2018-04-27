// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AbstractVariableTypesModel.h"

#include <CrySchematyc/Reflection/TypeDesc.h>
#include <CrySchematyc/Utils/SharedString.h>

const CDataTypeItem& CDataTypeItem::Empty()
{
	static CDataTypeItem empty("Unknown", QColor(255, 55, 100), CryGUID());
	return empty;
}

CDataTypeItem::CDataTypeItem(QString name, const QColor& color, const CryGUID& guid)
	: m_name(name)
	, m_color(color)
	, m_guid(guid)
{

}

CDataTypeItem::~CDataTypeItem()
{

}

CDataTypesModel& CDataTypesModel::GetInstance()
{
	static CDataTypesModel instance;
	return instance;
}

CDataTypesModel::CDataTypesModel()
{
	GenerateTypeInfo();
}

CDataTypeItem* CDataTypesModel::GetTypeItemByIndex(uint32 index) const
{
	if (index < m_typesByIndex.size())
	{
		return m_typesByIndex[index];
	}
	return nullptr;
}

CDataTypeItem* CDataTypesModel::GetTypeItemByGuid(const CryGUID& guid) const
{
	TypesByGuid::const_iterator result = m_typesByGuid.find(guid);
	if (result != m_typesByGuid.end())
	{
		return result->second;
	}
	return nullptr;
}

CDataTypesModel::~CDataTypesModel()
{
	for (CDataTypeItem* pTypeItem : m_typesByIndex)
		delete pTypeItem;
}

void CDataTypesModel::GenerateTypeInfo()
{
#define CREATE_TYPE_ITEM(_type_, _color_)                                                                    \
  {                                                                                                          \
    const Schematyc::CTypeDesc<_type_>& typeDesc = Schematyc::GetTypeDesc<_type_>();                         \
    CDataTypeItem* pTypeItem = new CDataTypeItem(typeDesc.GetName().c_str(), (_color_), typeDesc.GetGUID()); \
    m_typesByIndex.emplace_back(pTypeItem);                                                                  \
    m_typesByGuid.emplace(typeDesc.GetGUID(), pTypeItem);                                                    \
  }

	CREATE_TYPE_ITEM(bool, QColor(53, 213, 22));
	CREATE_TYPE_ITEM(int32, QColor(255, 72, 29));
	CREATE_TYPE_ITEM(uint32, QColor(255, 72, 29));
	CREATE_TYPE_ITEM(float, QColor(68, 249, 183));
	CREATE_TYPE_ITEM(Vec3, QColor(205, 151, 23));
	CREATE_TYPE_ITEM(CryGUID, QColor(192, 192, 98));
	CREATE_TYPE_ITEM(Schematyc::CSharedString, QColor(210, 42, 252));
	CREATE_TYPE_ITEM(Schematyc::ObjectId, QColor(0, 156, 255));
	//CREATE_TYPE_ITEM(Schematyc::ExplicitEntityId, QColor(110, 180, 160));
}

