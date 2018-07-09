// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySerialization/StringList.h>

#include "Settings.h"
#include "ItemTypeName.h"

class CQueryListProvider;
class CQueryBlueprint;

namespace UQS
{
namespace Core
{
struct ITextualQueryBlueprint;
} // namespace Core
} // namespace UQS

//////////////////////////////////////////////////////////////////////////

class CUqsDatabaseSerializationCache
{
public:
	const Serialization::StringList& GetQueryFactoryNamesList() const;
	const Serialization::StringList& GetItemTypeNamesList() const;
	const Serialization::StringList& GetGeneratorNamesList() const;
	const Serialization::StringList& GetFunctionNamesList() const;
	const Serialization::StringList& GetFunctionNamesList(const SItemTypeName& typeToFilter) const;

	const Serialization::StringList& GetEvaluatorNamesList() const;

	const Serialization::StringList& GetScoreTransformNamesList() const;

	SItemTypeName                    GetItemTypeNameFromType(const UQS::Shared::CTypeInfo& typeInfo) const;

private:
	template<typename TFactoryDb>
	static void BuildNameStringList(const TFactoryDb& factoryDb, Serialization::StringList& outNamesList);

	template<typename TFactoryDb, typename TFilterFunc>
	static void BuildNameStringListWithFilter(const TFactoryDb& factoryDb, TFilterFunc filterFunc, Serialization::StringList& outNamesList);

	void        BuildTypeInfoToNameMap();

private:

	Serialization::StringList                       m_queryFactoryNamesList;
	Serialization::StringList                       m_generatorNamesList;
	Serialization::StringList                       m_functionNamesList;
	Serialization::StringList                       m_itemTypeNamesList;
	Serialization::StringList                       m_instantEvaluatorNamesList;
	Serialization::StringList                       m_deferredEvaluatorNamesList;
	Serialization::StringList                       m_evaluatorNamesList;
	Serialization::StringList                       m_scoreTransformNamesList;

	Serialization::StringList                       m_filteredFunctionNamesList;
	SItemTypeName                                   m_lastTypeToFilterFunctionNames;

	std::map<UQS::Shared::CTypeInfo, SItemTypeName> m_typeInfoToName;
};

//////////////////////////////////////////////////////////////////////////

class CUqsEditorContext
{
public:
	CUqsEditorContext();

	CQueryListProvider&                   GetQueryListProvider() const;
	const SDocumentSettings&              GetSettings() const;
	SDocumentSettings&                    GetSettings();
	const CUqsDatabaseSerializationCache& GetSerializationCache() const;

private:
	std::unique_ptr<CQueryListProvider> m_pQueryListProvider;
	SDocumentSettings                   m_docSettings;
	CUqsDatabaseSerializationCache      m_uqsDatabaseSerializationCache;
};
