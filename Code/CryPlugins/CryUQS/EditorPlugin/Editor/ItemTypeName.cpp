// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ItemTypeName.h"

#include "SerializationHelpers.h"
#include "DocSerializationContext.h"
#include "Settings.h"

//////////////////////////////////////////////////////////////////////////
// SItemTypeName
//////////////////////////////////////////////////////////////////////////

SItemTypeName::SItemTypeName()
	: m_typeGUID()
{}

SItemTypeName::SItemTypeName(const CryGUID& typeGUID)
	: m_typeGUID(typeGUID)
{}

SItemTypeName::SItemTypeName(const SItemTypeName& other)
	: m_typeGUID(other.m_typeGUID)
{}

SItemTypeName::SItemTypeName(SItemTypeName&& other)
	: m_typeGUID(std::move(other.m_typeGUID))
{}

SItemTypeName& SItemTypeName::operator=(const SItemTypeName& other)
{
	if (this != &other)
	{
		m_typeGUID = other.m_typeGUID;
	}
	return *this;
}

SItemTypeName& SItemTypeName::operator=(SItemTypeName&& other)
{
	if (this != &other)
	{
		m_typeGUID = std::move(other.m_typeGUID);
	}
	return *this;
}

void SItemTypeName::Serialize(Serialization::IArchive& archive)
{
	if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
	{
		const CryGUID oldTypeGUID = m_typeGUID;
		auto setTypeGUID = [this](const CryGUID& newTypeGUID)
		{
			m_typeGUID = newTypeGUID;
		};

		if (pContext->GetSettings().bUseSelectionHelpers)
		{
			CKeyValueStringList<CryGUID> itemTypeList;

			// hide item types that are only used as containers for shuttled items (they have ugly names and are not meant for being used by the user)
			std::function<bool (const UQS::Client::IItemFactory&)> filterItemType = [](const UQS::Client::IItemFactory& itemFactory) -> bool
			{
				const bool bKeep = !itemFactory.IsContainerForShuttledItems();
				return bKeep;
			};

			itemTypeList.FillFromFactoryDatabaseWithFilter(UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase(), true, filterItemType);
			itemTypeList.SerializeByData(archive, "typeGUID", "^", oldTypeGUID, setTypeGUID);
		}
		else
		{
			SerializeGUIDWithSetter(archive, "typeGUID", "^", oldTypeGUID, setTypeGUID);
		}
	}
}

const char* SItemTypeName::c_str() const
{
	if (const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(m_typeGUID))
	{
		return pItemFactory->GetName();
	}
	else
	{
		return "";
	}
}

const CryGUID& SItemTypeName::GetTypeGUID() const
{
	return m_typeGUID;
}

bool SItemTypeName::Empty() const
{
	return m_typeGUID.IsNull();
}

bool SItemTypeName::operator==(const SItemTypeName& other) const
{
	return m_typeGUID == other.m_typeGUID;
}
