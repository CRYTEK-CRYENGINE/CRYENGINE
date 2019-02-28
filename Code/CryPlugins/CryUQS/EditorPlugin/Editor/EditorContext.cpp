// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorContext.h"

#include "QueryListProvider.h"

//////////////////////////////////////////////////////////////////////////

template<typename TFactoryDb, typename TFilterFunc>
static void CUqsDatabaseSerializationCache::BuildNameStringListWithFilter(const TFactoryDb& factoryDb, TFilterFunc filterFunc, Serialization::StringList& outNamesList)
{
	outNamesList.clear();

	const size_t count = factoryDb.GetFactoryCount();
	outNamesList.reserve(count + 1);

	outNamesList.push_back("");

	for (size_t i = 0; i < count; ++i)
	{
		const auto& factory = factoryDb.GetFactory(i);
		if (filterFunc(factory))
		{
			outNamesList.push_back(factory.GetName());
		}
	}
}

struct SEmptyFilterFunc
{
	template<typename T>
	bool operator()(const T&) { return true; }
};

template<typename TFactoryDb>
static void CUqsDatabaseSerializationCache::BuildNameStringList(const TFactoryDb& factoryDb, Serialization::StringList& outNamesList)
{
	BuildNameStringListWithFilter(factoryDb, SEmptyFilterFunc(), outNamesList);
}

SItemTypeName CUqsDatabaseSerializationCache::GetItemTypeNameFromType(const UQS::Shared::CTypeInfo& typeInfo) const
{
	if (m_typeInfoToName.empty())
	{
		// TODO pavloi 2015.12.11: fix const_cast inside lazy getter
		const_cast<CUqsDatabaseSerializationCache*>(this)->BuildTypeInfoToNameMap();
	}

	auto iter = m_typeInfoToName.find(typeInfo);
	if (iter != m_typeInfoToName.end())
	{
		return iter->second;
	}
	else
	{
		if (const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(typeInfo))
		{
			return SItemTypeName(pItemFactory->GetGUID());
		}
		else
		{
			// FIXME: is this OK?
			return SItemTypeName(CryGUID::Null());
		}
	}
}

void CUqsDatabaseSerializationCache::BuildTypeInfoToNameMap()
{
	m_typeInfoToName.clear();

	const auto& db = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase();
	const size_t itemFactoryCount = db.GetFactoryCount();
	for (size_t i = 0; i < itemFactoryCount; ++i)
	{
		const auto& factory = db.GetFactory(i);
		const UQS::Shared::CTypeInfo& typeInfo = factory.GetItemType();

		m_typeInfoToName[typeInfo] = SItemTypeName(factory.GetGUID());
	}
}

//////////////////////////////////////////////////////////////////////////

CUqsEditorContext::CUqsEditorContext()
{
	m_pQueryListProvider.reset(
	  new CQueryListProvider(*this));
	m_pQueryListProvider->Populate();
}

CQueryListProvider& CUqsEditorContext::GetQueryListProvider() const
{
	if (!m_pQueryListProvider)
	{
		CryFatalError("m_pQueryListProvider is not created");
	}
	return *m_pQueryListProvider;
}

const SDocumentSettings& CUqsEditorContext::GetSettings() const
{
	return m_docSettings;
}

SDocumentSettings& CUqsEditorContext::GetSettings()
{
	return m_docSettings;
}

const CUqsDatabaseSerializationCache& CUqsEditorContext::GetSerializationCache() const
{
	return m_uqsDatabaseSerializationCache;
}
