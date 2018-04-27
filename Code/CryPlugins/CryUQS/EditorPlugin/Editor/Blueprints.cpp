// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Blueprints.h"
#include "DocSerializationContext.h"
#include "Settings.h"
#include "CentralEventManager.h"

#include <CrySerialization/CryStrings.h>
#include <CrySerialization/Enum.h>
#include <CrySerialization/STL.h>

#include "SerializationHelpers.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

//////////////////////////////////////////////////////////////////////////

template<typename TSetFunc>
bool SerializeStringWithSetter(Serialization::IArchive& archive, const char* szName, const char* szLabel, const char* const szOldValue, TSetFunc setFunc)
{
	stack_string str(szOldValue);
	const bool res = archive(str, szName, szLabel);
	if (res && archive.isInput() && str != szOldValue)
	{
		setFunc(str);
	}
	return res;
}

static bool IsStringEmpty(const char* szStr)
{
	return !szStr || !szStr[0];
}

template<typename TString>
static bool IsStringEmpty(const TString& str)
{
	return str.empty();
}

namespace UQSEditor
{

// TODO pavloi 2015.12.09: we shouldn't expose undefined type to UI, but right now I'm going to use it for debug purposes.
SERIALIZATION_ENUM_BEGIN(EEvaluatorType, "EvaluatorType")
SERIALIZATION_ENUM(EEvaluatorType::Instant, "Instant", "Instant")
SERIALIZATION_ENUM(EEvaluatorType::Deferred, "Deferred", "Deferred")
SERIALIZATION_ENUM(EEvaluatorType::Undefined, "Undefined", "Undefined")
SERIALIZATION_ENUM_END()

SERIALIZATION_ENUM_BEGIN(EEvaluatorCost, "EvaluatorCost")
SERIALIZATION_ENUM(EEvaluatorCost::Cheap, "Cheap", "Cheap")
SERIALIZATION_ENUM(EEvaluatorCost::Expensive, "Expensive", "Expensive")
SERIALIZATION_ENUM(EEvaluatorCost::Undefined, "Undefined", "Undefined")
SERIALIZATION_ENUM_END()

//////////////////////////////////////////////////////////////////////////

class CSyntaxErrorCollector : public UQS::DataSource::ISyntaxErrorCollector
{
public:
	explicit CSyntaxErrorCollector(const std::shared_ptr<CErrorCollector>& pErrorCollector)
		: m_pErrorCollector(pErrorCollector)
	{}

	// ISyntaxErrorCollector
	virtual void AddErrorMessage(const char* szFormat, ...) PRINTF_PARAMS(2, 3) override
	{
		va_list args;
		va_start(args, szFormat);
		string msg;
		msg.FormatV(szFormat, args);
		m_pErrorCollector->AddErrorMessage(std::move(msg));
		va_end(args);
	}
	// ~ISyntaxErrorCollector

private:
	std::shared_ptr<CErrorCollector> m_pErrorCollector;
};

//////////////////////////////////////////////////////////////////////////

static void DeleteSyntaxErrorCollector(UQS::DataSource::ISyntaxErrorCollector* pSyntaxErrorCollectorToDelete)
{
	delete pSyntaxErrorCollectorToDelete;
}

static UQS::DataSource::SyntaxErrorCollectorUniquePtr MakeNewSyntaxErrorCollectorUniquePtr(const std::shared_ptr<CErrorCollector>& pErrorCollector)
{
	return UQS::DataSource::SyntaxErrorCollectorUniquePtr(new CSyntaxErrorCollector(pErrorCollector), UQS::DataSource::CSyntaxErrorCollectorDeleter(&DeleteSyntaxErrorCollector));
}

//////////////////////////////////////////////////////////////////////////

class CEvaluatorFactoryHelper
{
public:
	CEvaluatorFactoryHelper()
		: m_pInstantFactory(nullptr)
		, m_pDeferredFactory(nullptr)
	{}

	CEvaluatorFactoryHelper(const CUqsDocSerializationContext& context, const CryGUID& evaluatorGUID)
		: m_pInstantFactory(nullptr)
		, m_pDeferredFactory(nullptr)
	{
		// FIXME: let's hope that there's never a duplicate GUID between Instant- and Deferred-Evaluators
		m_pInstantFactory = UQS::Core::IHubPlugin::GetHub().GetInstantEvaluatorFactoryDatabase().FindFactoryByGUID(evaluatorGUID);
		if (!m_pInstantFactory)
		{
			m_pDeferredFactory = UQS::Core::IHubPlugin::GetHub().GetDeferredEvaluatorFactoryDatabase().FindFactoryByGUID(evaluatorGUID);
		}
	}

	const UQS::Client::IInputParameterRegistry& GetInputParameterRegistry() const
	{
		assert(IsValid());
		if (m_pInstantFactory)
		{
			return m_pInstantFactory->GetInputParameterRegistry();
		}
		else
		{
			assert(m_pDeferredFactory);
			return m_pDeferredFactory->GetInputParameterRegistry();
		}
	}

	bool IsValid() const
	{
		assert(!(m_pInstantFactory && m_pDeferredFactory));
		return m_pInstantFactory || m_pDeferredFactory;
	}

	EEvaluatorType GetType() const
	{
		if (m_pInstantFactory)
		{
			return EEvaluatorType::Instant;
		}
		else if (m_pDeferredFactory)
		{
			return EEvaluatorType::Deferred;
		}
		else
		{
			return EEvaluatorType::Undefined;
		}
	}

	EEvaluatorCost GetCost() const
	{
		if (m_pInstantFactory)
		{
			switch (m_pInstantFactory->GetCostCategory())
			{
			case UQS::Client::IInstantEvaluatorFactory::ECostCategory::Cheap:
				return EEvaluatorCost::Cheap;

			case UQS::Client::IInstantEvaluatorFactory::ECostCategory::Expensive:
				return EEvaluatorCost::Expensive;

			default:
				assert(0);
				return EEvaluatorCost::Undefined;
			}
		}
		else if (m_pDeferredFactory)
		{
			// deferred evaluators are implicitly always expensive
			return EEvaluatorCost::Expensive;
		}
		else
		{
			return EEvaluatorCost::Undefined;
		}
	}

	const char* GetDescription() const
	{
		if (m_pInstantFactory)
		{
			return m_pInstantFactory->GetDescription();
		}
		else if (m_pDeferredFactory)
		{
			return m_pDeferredFactory->GetDescription();
		}
		else
		{
			return "";
		}
	}

private:
	UQS::Client::IInstantEvaluatorFactory*  m_pInstantFactory;
	UQS::Client::IDeferredEvaluatorFactory* m_pDeferredFactory;
};

//////////////////////////////////////////////////////////////////////////
// CErrorCollector
//////////////////////////////////////////////////////////////////////////

CErrorCollector::CErrorCollector()
	: m_messages()
	, m_pExternalCollector()
{}

CErrorCollector::CErrorCollector(CErrorCollector&& other)
	: m_messages(std::move(other.m_messages))
	, m_pExternalCollector(std::move(other.m_pExternalCollector))
{}

CErrorCollector& CErrorCollector::operator=(CErrorCollector&& other)
{
	if (this != &other)
	{
		m_messages = std::move(other.m_messages);
		m_pExternalCollector = std::move(other.m_pExternalCollector);
	}
	return *this;
}

void CErrorCollector::AddErrorMessage(string&& message)
{
	m_messages.push_back(message);
}

void CErrorCollector::Clear()
{
	m_messages.clear();
	m_pExternalCollector.reset();
}

void CErrorCollector::Serialize(Serialization::IArchive& archive, const SValidatorKey& validatorKey)
{
	if (archive.isValidation())
	{
		for (const string& message : m_messages)
		{
			archive.error(validatorKey.pHandle, validatorKey.typeId, "%s", message.c_str());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CItemUniquePtr
//////////////////////////////////////////////////////////////////////////

CItemUniquePtr::CItemUniquePtr()
	: m_pItem(nullptr)
	, m_pItemFactory(nullptr)
{}

CItemUniquePtr::~CItemUniquePtr()
{
	if (m_pItem)
	{
		assert(m_pItemFactory);
		m_pItemFactory->DestroyItems(m_pItem);
		m_pItem = nullptr;
	}
}

CItemUniquePtr::CItemUniquePtr(CItemUniquePtr&& other)
	: m_pItem(other.m_pItem)
	, m_pItemFactory(other.m_pItemFactory)
{
	other.m_pItem = nullptr;
	other.m_pItemFactory = nullptr;
}

CItemUniquePtr& CItemUniquePtr::operator=(CItemUniquePtr&& other)
{
	m_pItem = other.m_pItem;
	m_pItemFactory = other.m_pItemFactory;
	other.m_pItem = nullptr;
	other.m_pItemFactory = nullptr;
	return *this;
}

CItemUniquePtr::CItemUniquePtr(UQS::Client::IItemFactory* pItemFactory)
	: m_pItem(nullptr)
	, m_pItemFactory(pItemFactory)
{
	if (pItemFactory && pItemFactory->CanBePersistantlySerialized())
	{
		m_pItem = pItemFactory->CreateItems(1, UQS::Client::IItemFactory::EItemInitMode::UseUserProvidedFunction);
	}
}

bool CItemUniquePtr::IsSerializable() const
{
	return m_pItem && m_pItemFactory && m_pItemFactory->CanBePersistantlySerialized();
}

bool CItemUniquePtr::SetFromStringLiteral(const string& stringLiteral)
{
	if (m_pItem && m_pItemFactory)
	{
		return UQS::Core::IHubPlugin::GetHub().GetItemSerializationSupport().DeserializeItemFromCStringLiteral(m_pItem, *m_pItemFactory, stringLiteral, nullptr);
	}
	return false;
}

void CItemUniquePtr::ConvertToStringLiteral(string& outString) const
{
	outString.clear();
	if (m_pItem && m_pItemFactory)
	{
		UQS::Shared::CUqsString literalString;
		if (UQS::Core::IHubPlugin::GetHub().GetItemSerializationSupport().SerializeItemToStringLiteral(m_pItem, *m_pItemFactory, literalString))
		{
			outString = literalString.c_str();
		}
	}
}

bool Serialize(Serialization::IArchive& archive, CItemUniquePtr& value, const char* szName, const char* szLabel)
{
	if (value.m_pItem && value.m_pItemFactory)
	{
		if (archive.isInput())
		{
			return value.m_pItemFactory->TryDeserializeItem(value.m_pItem, archive, szName, szLabel);
		}
		else
		{
			assert(archive.isOutput());
			return value.m_pItemFactory->TrySerializeItem(value.m_pItem, archive, szName, szLabel);
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// CItemLiteral
//////////////////////////////////////////////////////////////////////////

CItemLiteral::CItemLiteral()
	: m_item()
	, m_typeName()
{}

CItemLiteral::CItemLiteral(const SItemTypeName& typeName, const CUqsDocSerializationContext& context)
	: m_item(UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(typeName.GetTypeGUID()))
	, m_typeName(typeName)
{}


bool CItemLiteral::SetFromStringLiteral(const string& stringLiteral)
{
	return m_item.SetFromStringLiteral(stringLiteral);
}

void CItemLiteral::ConvertToStringLiteral(string& outString) const
{
	m_item.ConvertToStringLiteral(outString);
}

bool Serialize(Serialization::IArchive& archive, CItemLiteral& value, const char* szName, const char* szLabel)
{
	return archive(value.m_item, szName, szLabel);
}


//////////////////////////////////////////////////////////////////////////
// CFunctionSerializationHelper
//////////////////////////////////////////////////////////////////////////

void CFunctionSerializationHelper::CFunctionList::Build(const SItemTypeName& typeName, const CUqsDocSerializationContext& context)
{
	std::vector<SFunction> allFunctions;

	{
		const UQS::Core::IFunctionFactoryDatabase& functionFactoryDB = UQS::Core::IHubPlugin::GetHub().GetFunctionFactoryDatabase();
		for (size_t i = 0; i < functionFactoryDB.GetFactoryCount(); i++)
		{
			UQS::Client::IFunctionFactory& functionFactory = functionFactoryDB.GetFactory(i);
			const UQS::Client::IItemFactory* pItemFactoryOfReturnType = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(functionFactory.GetReturnType());
			assert(pItemFactoryOfReturnType);
			if (pItemFactoryOfReturnType->GetGUID() == typeName.GetTypeGUID() || !context.GetSettings().bFilterAvailableInputsByType)
			{
				allFunctions.emplace_back();
				SFunction& func = allFunctions.back();

				func.guid = functionFactory.GetGUID();
				func.pFactory = &functionFactory;
				func.leafFunctionKind = functionFactory.GetLeafFunctionKind();

				if (const UQS::Client::IItemFactory* pItemFactoryOfReturnType = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(functionFactory.GetReturnType()))
				{
					func.returnType = SItemTypeName(pItemFactoryOfReturnType->GetGUID());
				}
				else
				{
					func.returnType = SItemTypeName(CryGUID::Null());
				}
			}
		}
	}

	const bool bApplyTypeFilter = !typeName.Empty() && context.GetSettings().bFilterAvailableInputsByType;

	std::vector<SFunction> functions;
	functions.reserve(allFunctions.size());

	// iterated params
	if (CSelectedGeneratorContext* pSelectedGeneratorContext = context.GetSelectedGeneratorContext())
	{
		if (pSelectedGeneratorContext->GetGeneratorProcessed())
		{
			std::vector<SFunction> functionsForIteratedItem;
			std::copy_if(allFunctions.begin(), allFunctions.end(), std::back_inserter(functionsForIteratedItem),
				[](const SFunction& func) { return func.leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::IteratedItem; });

			pSelectedGeneratorContext->BuildFunctionListForAvailableGenerators(typeName, context, functionsForIteratedItem, functions);

			for (SFunction& func : functions)
			{
				if (bApplyTypeFilter)
				{
					// TODO pavloi 2016.04.26: change, if we're going to support multiple generators
					func.prettyName = "ITER";
					// TODO pavloi 2016.04.26: is it possible to register several iterated item functions for same type?
					break;
				}
				else
				{
					func.prettyName.Format("ITER: %s", func.returnType.c_str());
				}
			}
		}
	}

	// Global Params
	if (CParametersListContext* pParams = context.GetParametersListContext())
	{
		std::vector<SFunction> globalParams;
		{
			std::vector<SFunction> functionsForGlobalParams;
			std::copy_if(allFunctions.begin(), allFunctions.end(), std::back_inserter(functionsForGlobalParams),
				[](const SFunction& func) { return func.leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::GlobalParam; });

			pParams->BuildFunctionListForAvailableParameters(typeName, context, functionsForGlobalParams, globalParams);
		}

		for (SFunction& func : globalParams)
		{
			assert(func.leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::GlobalParam);
			assert(!func.param.empty());

			functions.emplace_back(std::move(func));
			SFunction& iteratedItemFunc = functions.back();

			iteratedItemFunc.prettyName.Format("PARAM: %s", iteratedItemFunc.param.c_str());
		}
		globalParams.clear();
	}

	// Literals
	{
		for (const SFunction& func : allFunctions)
		{
			if (func.leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::Literal)
			{
				functions.emplace_back(func);
				SFunction& iteratedItemFunc = functions.back();

				iteratedItemFunc.prettyName.Format("LITERAL: %s", iteratedItemFunc.returnType.c_str());

				if (bApplyTypeFilter)
				{
					// TODO pavloi 2016.04.26: is it possible to register several literal functions for same type?
					break;
				}
			}
		}
	}

	// Shuttled Items
	// TODO: the expectedShuttleType already gets properly serialized and such, but we also need a way for the user to specify the type somehow (preferably through a drop-down-list?)
	for (const SFunction& func : allFunctions)
	{
		if (func.leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::ShuttledItems)
		{
			functions.emplace_back(func);
			SFunction& shuttledItemsFunc = functions.back();

			const UQS::Shared::CTypeInfo* pContainedType = shuttledItemsFunc.pFactory->GetContainedType();
			assert(pContainedType);
			const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(*pContainedType);
			assert(pItemFactory);
			shuttledItemsFunc.prettyName.Format("SHUTTLE: %s", pItemFactory->GetName());

			if (bApplyTypeFilter)
			{
				// TODO christianw 2016.09.05: is it possible to register several shuttled-items functions for same type?
				break;
			}
		}
	}

	// All other functions
	{
		for (const SFunction& func : allFunctions)
		{
			auto alreadyListedFuncIter = std::find_if(functions.begin(), functions.end(), [&func](const SFunction& alreadyListedFunc) { return alreadyListedFunc.pFactory == func.pFactory; });
			if (alreadyListedFuncIter == functions.end())
			{
				functions.emplace_back(func);
				SFunction& iteratedItemFunc = functions.back();

				iteratedItemFunc.prettyName = UQS::Core::IHubPlugin::GetHub().GetFunctionFactoryDatabase().FindFactoryByGUID(iteratedItemFunc.guid)->GetName();
			}
		}
	}

	// Build string list
	{
		m_functionsGuidList.Clear();
		m_functionsGuidList.AddEntry("", CryGUID::Null());
		for (const SFunction& func : functions)
		{
			m_functionsGuidList.AddEntry(func.prettyName.c_str(), func.guid);
		}
	}

	m_functions = std::move(functions);
}

int CFunctionSerializationHelper::CFunctionList::SerializeGUID(
	Serialization::IArchive& archive,
	const char* szName,
	const char* szLabel,
	const CUqsDocSerializationContext& context,
	const SValidatorKey& validatorKey,
	const int oldFunctionIdx)
{
	const bool bIsNpos = (oldFunctionIdx == CFunctionSerializationHelper::npos);
	const string oldPrettyName = bIsNpos ? "" : m_functions[oldFunctionIdx].prettyName;

	const CKeyValueStringList<CryGUID>::SSerializeResult serializeResult = m_functionsGuidList.SerializeByLabelForDisplayInDropDownList(archive, szName, szLabel, &validatorKey, oldPrettyName);
	const int newFunctionIdx = serializeResult.newIndex - 1;	// careful: could become -2 if the drop-down-list has no entries ("I GUESS"!)
	const bool bChanged = newFunctionIdx != oldFunctionIdx;

	int resultFunctionIndex = oldFunctionIdx;

	if (bChanged)
	{
		if (serializeResult.newIndex == -1 || serializeResult.newIndex == 0)
		{
			resultFunctionIndex = CFunctionSerializationHelper::npos;
		}
		else
		{
			resultFunctionIndex = newFunctionIdx;
		}
	}

	// description of the function
	if (resultFunctionIndex != CFunctionSerializationHelper::npos)
	{
		archive.doc(GetByIdx(resultFunctionIndex).pFactory->GetDescription());
	}

	return resultFunctionIndex;

	// ORIGINAL CODE FOR REFERENCE
	/*
	const bool bIsNpos = (oldFunctionIdx == Serialization::StringList::npos);

	Serialization::StringListValue strValue(m_functionsStringList, !bIsNpos ? (oldFunctionIdx + 1) : 0, validatorKey.pHandle, validatorKey.typeId);
	const int oldStrValueIndex = strValue.index();
	const bool bRes = archive(strValue, szName, szLabel);

	const int newStrValueIndex = strValue.index();
	int resultFunctionIndex = oldFunctionIdx;

	const bool bChanged = oldStrValueIndex != newStrValueIndex;
	if (bChanged)
	{
		if (newStrValueIndex == Serialization::StringList::npos || newStrValueIndex == 0)
		{
			resultFunctionIndex = CFunctionSerializationHelper::npos;
		}
		else
		{
			resultFunctionIndex = newStrValueIndex - 1;
		}
	}

	// description of the function
	if (resultFunctionIndex != CFunctionSerializationHelper::npos)
	{
		archive.doc(GetByIdx(resultFunctionIndex).pFactory->GetDescription());
	}

	return resultFunctionIndex;
	*/
}

const CFunctionSerializationHelper::SFunction& CFunctionSerializationHelper::CFunctionList::GetByIdx(const int idx) const
{
	if (idx < 0 || idx > m_functions.size())
	{
		CryFatalError("Wrong index");
	}

	return m_functions[idx];
}

int CFunctionSerializationHelper::CFunctionList::FindByGUIDAndParam(const CryGUID& guid, const string& param) const
{
	for (int i = 0; i < m_functions.size(); ++i)
	{
		if (m_functions[i].guid == guid)
		{
			if (m_functions[i].leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::GlobalParam)
			{
				if (m_functions[i].param == param)
				{
					return i;
				}

				// TODO pavloi 2016.04.26: remember, that the function was a GlobalParam leaf. If no params are found, stop search (list is sorted) and use
				// this info to show good warning message.
			}
			else
			{
				return i;
			}
		}
	}
	return CFunctionSerializationHelper::npos;
}

void CFunctionSerializationHelper::CFunctionList::Clear()
{
	m_functions.clear();
}

CFunctionSerializationHelper::CFunctionSerializationHelper()
	: m_funcGUID()
	, m_funcParam()
	, m_itemLiteral()
	, m_typeName()
	, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
	, m_functionsList()
	, m_selectedFunctionIdx(npos)
{}

CFunctionSerializationHelper::CFunctionSerializationHelper(const CryGUID& functionGUID, const char* szParamOrReturnValue, bool bAddReturnValueToDebugRenderWorldUponExecution)
	: m_funcGUID(functionGUID)
	, m_funcParam(szParamOrReturnValue)
	, m_itemLiteral()
	, m_typeName()
	, m_bAddReturnValueToDebugRenderWorldUponExecution(bAddReturnValueToDebugRenderWorldUponExecution)
	, m_functionsList()
	, m_selectedFunctionIdx(npos)
{}

CFunctionSerializationHelper::CFunctionSerializationHelper(const UQS::Client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context)
	: m_funcGUID(functionFactory.GetGUID())
	, m_funcParam()
	, m_itemLiteral()
	, m_typeName(context.GetItemTypeNameFromType(functionFactory.GetReturnType()))
	, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
	, m_functionsList()
	, m_selectedFunctionIdx(npos)
{}

void CFunctionSerializationHelper::RebuildList(const CUqsDocSerializationContext& context)
{
	m_functionsList.Clear();
	if (context.GetSettings().bUseSelectionHelpers && !m_typeName.Empty())
	{
		m_functionsList.Build(m_typeName, context);
	}
}

void CFunctionSerializationHelper::UpdateSelectedFunctionIndex()
{
	m_selectedFunctionIdx = m_functionsList.FindByGUIDAndParam(m_funcGUID, m_funcParam);
}

void CFunctionSerializationHelper::Prepare(const CUqsDocSerializationContext& context)
{
	RebuildList(context);
	UpdateSelectedFunctionIndex();
}

void CFunctionSerializationHelper::Reset(const SItemTypeName& typeName, const CUqsDocSerializationContext& context)
{
	bool bShouldResetList = context.ShouldForceSelectionHelpersEvaluation();

	if (m_typeName != typeName)
	{
		m_typeName = typeName;
		m_itemLiteral = CItemLiteral();

		bShouldResetList = true;
	}

	if (!bShouldResetList && context.GetParametersListContext())
	{
		bShouldResetList = context.GetParametersListContext()->GetParamsChanged();
	}

	if (bShouldResetList)
	{
		Prepare(context);

		if (m_selectedFunctionIdx != npos)
		{
			const SFunction& function = m_functionsList.GetByIdx(m_selectedFunctionIdx);
			const bool bIsLiteral = (function.leafFunctionKind == UQS::Client::IFunctionFactory::ELeafFunctionKind::Literal);

			if (bIsLiteral)
			{
				if (!m_itemLiteral.IsExist() || m_itemLiteral.GetTypeName() != typeName)
				{
					m_itemLiteral = CItemLiteral(typeName, context);
				}

				// TODO pavloi 2016.09.01: this should happen when we read everything from textual blueprint, 
				// but the code and initialization order I wrote is such a dense spaghetti, that it doesn't seem easy anymore.
				ReserializeFunctionLiteralFromParam();
			}
		}
	}
}

void CFunctionSerializationHelper::ReserializeFunctionLiteralFromParam()
{
	if (m_itemLiteral.IsSerializable() && !IsStringEmpty(m_funcParam))
	{
		if (m_itemLiteral.SetFromStringLiteral(m_funcParam))
		{
			m_funcParam.clear();
		}
	}
}

bool CFunctionSerializationHelper::SerializeFunctionGUID(
	Serialization::IArchive& archive,
	const char* szName,
	const char* szLabel,
	const CUqsDocSerializationContext& context)
{
	bool bGUIDChanged = false;

	if (context.GetSettings().bUseSelectionHelpers)
	{
		const int oldFunctionIdx = m_selectedFunctionIdx;
		const SValidatorKey validatorKey = SValidatorKey::FromObject(*this);
		const int functionIdx = m_functionsList.SerializeGUID(archive, szName, szLabel, context, validatorKey, oldFunctionIdx);

		if (functionIdx != oldFunctionIdx)
		{
			bGUIDChanged = true;
			m_selectedFunctionIdx = functionIdx;
			if (functionIdx != npos)
			{
				const SFunction& function = m_functionsList.GetByIdx(functionIdx);
				m_funcGUID = function.guid;

				switch (function.leafFunctionKind)
				{
				case UQS::Client::IFunctionFactory::ELeafFunctionKind::GlobalParam:
					m_funcParam = function.param;
					m_itemLiteral = CItemLiteral();
					break;

				case UQS::Client::IFunctionFactory::ELeafFunctionKind::Literal:
					if (!m_itemLiteral.IsExist())
					{
						m_itemLiteral = CItemLiteral(function.returnType, context);

						if (oldFunctionIdx == npos)
						{
							ReserializeFunctionLiteralFromParam();
						}
					}
					break;

				default:
					m_funcParam.clear();
					m_itemLiteral = CItemLiteral();
					break;
				}
			}
			else
			{
				m_funcGUID = CryGUID::Null();
				m_funcParam.clear();
				m_itemLiteral = CItemLiteral();
			}
		}

		if (m_selectedFunctionIdx == npos && !m_funcGUID.IsNull())
		{
			// TODO pavloi 2016.04.26: there is no way to pass validator key handle and type to the warning() like to error()

			if (m_funcParam.empty())
			{
				archive.warning(*this, "Function '%s' is not a valid input.", GetFunctionInternalName());
			}
			else
			{
				archive.warning(*this, "Function '%s' with param '%s' is not a valid input.", GetFunctionInternalName(), m_funcParam.c_str());
			}
		}
	}
	else
	{
		const CryGUID oldGUID = m_funcGUID;
		const auto setGUID = [this, &bGUIDChanged](const CryGUID& newGUID)
		{
			m_funcGUID = newGUID;
			bGUIDChanged = true;
		};

		SerializeGUIDWithSetter(archive, szName, szLabel, oldGUID, setGUID);
	}

	return bGUIDChanged;
}

void CFunctionSerializationHelper::SerializeFunctionParam(Serialization::IArchive& archive, const CUqsDocSerializationContext& context)
{
	if (context.GetSettings().bUseSelectionHelpers)
	{
		if (m_selectedFunctionIdx != npos)
		{
			const SFunction& function = m_functionsList.GetByIdx(m_selectedFunctionIdx);

			switch (function.leafFunctionKind)
			{
			case UQS::Client::IFunctionFactory::ELeafFunctionKind::Literal:
				if (m_itemLiteral.IsExist())
				{
					archive(m_itemLiteral, "returnValue", "^");
				}
				else
				{
					archive(m_funcParam, "returnValue", "^");
				}
				break;

			default:
				// do nothing
				break;
			}
		}
	}
	else
	{
		stack_string tmp("Not supported");
		archive(tmp, "returnValue", "!Return Value");
	}
}

void CFunctionSerializationHelper::SerializeAddReturnValueToDebugRenderWorldUponExecution(Serialization::IArchive& archive)
{
	bool bItemCanBeRepresentedInDebugRenderWorld = false;

	// check for whether the item can be represented in the debug render world
	if (const SFunction* pFunction = GetSelectedFunction())
	{
		if (const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(pFunction->pFactory->GetReturnType()))
		{
			if (pItemFactory->CanBeRepresentedInDebugRenderWorld())
			{
				bItemCanBeRepresentedInDebugRenderWorld = true;
			}
		}
	}

	if (bItemCanBeRepresentedInDebugRenderWorld)
	{
		archive(m_bAddReturnValueToDebugRenderWorldUponExecution, "addReturnValueToDebugRenderWorldUponExecution", "^DebugRender");
		archive.doc("If enabled, the return value is added to DebugRenderWorld upon execution. Then you can view the value in the query history.");
	}
}

const char* CFunctionSerializationHelper::GetFunctionInternalName() const
{
	if (const UQS::Client::IFunctionFactory* pFunctionFactory = UQS::Core::IHubPlugin::GetHub().GetFunctionFactoryDatabase().FindFactoryByGUID(m_funcGUID))
	{
		return pFunctionFactory->GetName();
	}
	else
	{
		return "";
	}
}

const string& CFunctionSerializationHelper::GetFunctionParamOrReturnValue() const
{
	if (m_itemLiteral.IsExist())
	{
		m_itemLiteral.ConvertToStringLiteral(m_itemCachedLiteral);
		return m_itemCachedLiteral;
	}
	else
	{
		return m_funcParam;
	}
}

UQS::Client::IFunctionFactory* CFunctionSerializationHelper::GetFunctionFactory() const
{
	if (const SFunction* pFunction = GetSelectedFunction())
	{
		return pFunction->pFactory;
	}
	return nullptr;
}

const CFunctionSerializationHelper::SFunction* CFunctionSerializationHelper::GetSelectedFunction() const
{
	if (m_selectedFunctionIdx == npos)
	{
		return nullptr;
	}

	return &m_functionsList.GetByIdx(m_selectedFunctionIdx);
}

//////////////////////////////////////////////////////////////////////////
// CInputBlueprint
//////////////////////////////////////////////////////////////////////////

CInputBlueprint::CInputBlueprint()
	: m_paramName()
	, m_paramID(UQS::Client::CInputParameterID::CreateEmpty())
	, m_paramDescription()
	, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
	, m_functionHelper()
	, m_children()
	, m_pErrorCollector(new CErrorCollector)
{}

CInputBlueprint::CInputBlueprint(const char* szParamName, const UQS::Client::CInputParameterID& paramID, const char* szParamDescription, const CryGUID& funcGUID, const char* szFuncReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution)
	: m_paramName(szParamName)
	, m_paramID(paramID)
	, m_paramDescription(szParamDescription)
	, m_bAddReturnValueToDebugRenderWorldUponExecution(bAddReturnValueToDebugRenderWorldUponExecution)
	, m_functionHelper(funcGUID, szFuncReturnValueLiteral, bAddReturnValueToDebugRenderWorldUponExecution)
	, m_children()
	, m_pErrorCollector(new CErrorCollector)
{}

CInputBlueprint::CInputBlueprint(const char* szParamName, const UQS::Client::CInputParameterID& paramID, const char* szParamDescription)
	: m_paramName(szParamName)
	, m_paramID(paramID)
	, m_paramDescription(szParamDescription)
	, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
	, m_functionHelper()
	, m_children()
	, m_pErrorCollector(new CErrorCollector)
{}

CInputBlueprint::CInputBlueprint(const UQS::Client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context)
	: m_paramName()
	, m_paramID(UQS::Client::CInputParameterID::CreateEmpty())
	, m_paramDescription()
	, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
	, m_functionHelper(functionFactory, context)
	, m_children()
	, m_pErrorCollector(new CErrorCollector)
{
	ResetChildrenFromFactory(functionFactory, context);
}

CInputBlueprint::CInputBlueprint(CInputBlueprint&& other)
	: m_paramName(std::move(other.m_paramName))
	, m_paramID(std::move(other.m_paramID))
	, m_bAddReturnValueToDebugRenderWorldUponExecution(std::move(other.m_bAddReturnValueToDebugRenderWorldUponExecution))
	, m_functionHelper(std::move(other.m_functionHelper))
	, m_children(std::move(other.m_children))
	, m_pErrorCollector(std::move(other.m_pErrorCollector))
{}

CInputBlueprint& CInputBlueprint::operator=(CInputBlueprint&& other)
{
	if (this != &other)
	{
		m_paramName = std::move(other.m_paramName);
		m_paramID = std::move(other.m_paramID);
		m_paramDescription = std::move(other.m_paramDescription);
		m_bAddReturnValueToDebugRenderWorldUponExecution = std::move(other.m_bAddReturnValueToDebugRenderWorldUponExecution);
		m_functionHelper = std::move(other.m_functionHelper);
		m_children = std::move(other.m_children);
		m_pErrorCollector = std::move(other.m_pErrorCollector);
	}
	return *this;
}

void CInputBlueprint::ResetChildrenFromFactory(const UQS::Client::IGeneratorFactory& generatorFactory, const CUqsDocSerializationContext& context)
{
	m_children.clear();
	SetChildrenFromFactoryInputRegistry(generatorFactory, context);
}

void CInputBlueprint::ResetChildrenFromFactory(const UQS::Client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context)
{
	m_children.clear();
	SetChildrenFromFactoryInputRegistry(functionFactory, context);
}

void CInputBlueprint::ResetChildrenFromFactory(const CEvaluatorFactoryHelper& evaluatorFactory, const CUqsDocSerializationContext& context)
{
	m_children.clear();
	SetChildrenFromFactoryInputRegistry(evaluatorFactory, context);
}

void CInputBlueprint::EnsureChildrenFromFactory(const UQS::Client::IGeneratorFactory& generatorFactory, const CUqsDocSerializationContext& context)
{
	SetChildrenFromFactoryInputRegistry(generatorFactory, context);
}

void CInputBlueprint::EnsureChildrenFromFactory(const UQS::Client::IFunctionFactory& functionFactory, const CUqsDocSerializationContext& context)
{
	SetChildrenFromFactoryInputRegistry(functionFactory, context);
}

void CInputBlueprint::EnsureChildrenFromFactory(const CEvaluatorFactoryHelper& evaluatorFactory, const CUqsDocSerializationContext& context)
{
	SetChildrenFromFactoryInputRegistry(evaluatorFactory, context);
}

template<typename TFactory>
void CInputBlueprint::SetChildrenFromFactoryInputRegistry(const TFactory& factory, const CUqsDocSerializationContext& context)
{
	const UQS::Client::IInputParameterRegistry& inputRegistry = factory.GetInputParameterRegistry();
	const size_t paramCount = inputRegistry.GetParameterCount();
	for (size_t i = 0; i < paramCount; ++i)
	{
		const UQS::Client::IInputParameterRegistry::SParameterInfo paramInfo = inputRegistry.GetParameter(i);

		CInputBlueprint* pParam = FindChildByParamID(paramInfo.id);
		if (!pParam)
		{
			m_children.emplace_back(paramInfo.szName, paramInfo.id, paramInfo.szDescription);
			pParam = &m_children.back();
		}
		else
		{
			// patch the parameter name such that it equals the one in the InputParameterRegistry (remember: it's currently still the one provided by the ITextualInputBlueprint)
			pParam->m_paramName = paramInfo.szName;
		}
		pParam->SetAdditionalParamInfo(paramInfo, context);
	}

	CRY_ASSERT(paramCount <= m_children.size());

	if (paramCount < m_children.size())
	{
		std::set<string> paramNames;
		for (size_t i = 0; i < paramCount; ++i)
		{
			const UQS::Client::IInputParameterRegistry::SParameterInfo paramInfo = inputRegistry.GetParameter(i);
			paramNames.insert(paramInfo.szName);
		}

		auto removeIter = std::remove_if(m_children.begin(), m_children.end(),
			[&](const CInputBlueprint& param) { return paramNames.find(param.m_paramName) == paramNames.end(); });
		m_children.erase(removeIter, m_children.end());
	}

	CRY_ASSERT(paramCount == m_children.size());
}

void CInputBlueprint::PrepareHelpers(const UQS::Client::IGeneratorFactory* pGeneratorFactory, const CUqsDocSerializationContext& context)
{
	if (pGeneratorFactory)
	{
		DeriveChildrenInfoFromFactoryInputRegistry(*pGeneratorFactory, context);
	}

	PrepareHelpersRoot(context);
}

void CInputBlueprint::PrepareHelpers(const UQS::Client::IFunctionFactory* pFunctionFactory, const CUqsDocSerializationContext& context)
{
	if (pFunctionFactory)
	{
		DeriveChildrenInfoFromFactoryInputRegistry(*pFunctionFactory, context);
	}

	PrepareHelpersRoot(context);
}

void CInputBlueprint::PrepareHelpers(const CEvaluatorFactoryHelper* pEvaluatorFactory, const CUqsDocSerializationContext& context)
{
	if (pEvaluatorFactory)
	{
		DeriveChildrenInfoFromFactoryInputRegistry(*pEvaluatorFactory, context);
	}

	PrepareHelpersRoot(context);
}

void CInputBlueprint::PrepareHelpersRoot(const CUqsDocSerializationContext& context)
{
	for (CInputBlueprint& child : m_children)
	{
		child.PrepareHelpersChild(context);
	}
}

void CInputBlueprint::PrepareHelpersChild(const CUqsDocSerializationContext& context)
{
	UQS::Client::IFunctionFactory* pFunctionFactory = m_functionHelper.GetFunctionFactory();
	PrepareHelpers(pFunctionFactory, context);
}

template<typename TFactory>
void CInputBlueprint::DeriveChildrenInfoFromFactoryInputRegistry(const TFactory& factory, const CUqsDocSerializationContext& context)
{
	const UQS::Client::IInputParameterRegistry& inputRegistry = factory.GetInputParameterRegistry();

	const size_t paramCount = inputRegistry.GetParameterCount();
	std::map<string, size_t> nameToParameterInfoIdx;
	for (size_t i = 0; i < paramCount; ++i)
	{
		const UQS::Client::IInputParameterRegistry::SParameterInfo paramInfo = inputRegistry.GetParameter(i);
		nameToParameterInfoIdx[string(paramInfo.szName)] = i;
	}

	for (CInputBlueprint& child : m_children)
	{
		auto iter = nameToParameterInfoIdx.find(child.m_paramName);
		if (iter != nameToParameterInfoIdx.end())
		{
			child.SetAdditionalParamInfo(inputRegistry.GetParameter(iter->second), context);
		}
	}
}

void CInputBlueprint::SetAdditionalParamInfo(const UQS::Client::IInputParameterRegistry::SParameterInfo& paramInfo, const CUqsDocSerializationContext& context)
{
	m_paramID = paramInfo.id;
	m_paramDescription = paramInfo.szDescription;
	m_functionHelper.Reset(context.GetItemTypeNameFromType(paramInfo.type), context);
}

void CInputBlueprint::Serialize(Serialization::IArchive& archive)
{
	assert(m_pErrorCollector);

	CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>();
	if (!pContext)
	{
		assert(false);
		return;
	}

	m_pErrorCollector->Serialize(archive, *this);

	const bool bUseSelectionHelpers = pContext->GetSettings().bUseSelectionHelpers;
	const bool bShowInputParamTypes = pContext->GetSettings().bShowInputParamTypes;

	if (bUseSelectionHelpers)
	{
		// Prepare param (and type) name and use it as an extra UI element to show the parameter description as tooltip
		if (bShowInputParamTypes)
		{
			m_paramLabel.Format("^!>0>%s (%s)", m_paramName.c_str(), m_functionHelper.GetExpectedType().c_str());
		}
		else
		{
			m_paramLabel.Format("^!>0>%s", m_paramName.c_str());
		}

		// dummy field just for displaying the parameter's name and its description
		string dummyValue = "";
		archive(dummyValue, "paramLabel", m_paramLabel.c_str());
		archive.doc(m_paramDescription.c_str());

		SerializeFunction(archive, *pContext, "^");
	}
	else
	{
		archive(m_paramName, "paramName", "^Param");
		SerializeFunction(archive, *pContext, "^Func");
	}
}

void CInputBlueprint::SerializeRoot(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context, const SValidatorKey& validatorKey)
{
	assert(m_pErrorCollector);
	m_pErrorCollector->Serialize(archive, validatorKey);
	SerializeChildren(archive, szName, szLabel, context);
}

void CInputBlueprint::SerializeFunction(Serialization::IArchive& archive, const CUqsDocSerializationContext& context, const char* szParamLabel)
{
	UQS::Client::IFunctionFactory* pFactory = nullptr;

	const bool bGUIDChanged = m_functionHelper.SerializeFunctionGUID(archive, "funcGUID", szParamLabel, context);

	bool bIsLeafUndecided = true;
	UQS::Client::IFunctionFactory::ELeafFunctionKind leafFunctionKind =
	  UQS::Client::IFunctionFactory::ELeafFunctionKind::None;

	if (context.GetSettings().bUseSelectionHelpers)
	{
		pFactory = m_functionHelper.GetFunctionFactory();

		if (pFactory)
		{
			bIsLeafUndecided = false;
			leafFunctionKind = pFactory->GetLeafFunctionKind();
		}
		else
		{
			return;
		}
	}

	if (pFactory && bGUIDChanged)
	{
		ResetChildrenFromFactory(*pFactory, context);
	}

	if (bIsLeafUndecided)
	{
		m_functionHelper.SerializeFunctionParam(archive, context);

		const bool bIsLeafFunc = !IsStringEmpty(m_functionHelper.GetFunctionParamOrReturnValue());
		if (!bIsLeafFunc)
		{
			SerializeChildren(archive, "inputs", "Inputs", context);
		}
	}
	else
	{
		switch (leafFunctionKind)
		{
		case UQS::Client::IFunctionFactory::ELeafFunctionKind::None:
			SerializeChildren(archive, "inputs", "Inputs", context);
			break;

		case UQS::Client::IFunctionFactory::ELeafFunctionKind::IteratedItem:
			// do nothing
			break;

		case UQS::Client::IFunctionFactory::ELeafFunctionKind::GlobalParam:
			// do nothing
			break;

		case UQS::Client::IFunctionFactory::ELeafFunctionKind::Literal:
			m_functionHelper.SerializeFunctionParam(archive, context);
			break;
		}
	}

	if (pFactory)
	{
		EnsureChildrenFromFactory(*pFactory, context);
	}

	// bAddReturnValueToDebugRenderWorldUponExecution
	{
		m_functionHelper.SerializeAddReturnValueToDebugRenderWorldUponExecution(archive);

		if (archive.isInput())
		{
			m_bAddReturnValueToDebugRenderWorldUponExecution = m_functionHelper.GetAddReturnValueToDebugRenderWorldUponExecution();
		}
	}
}

void CInputBlueprint::SerializeChildren(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context)
{
	if (context.GetSettings().bUseSelectionHelpers)
	{
		if (m_children.size() == 1)
		{
			// NOTE pavloi 2016.04.26: special case for functions with one child - it will be serialized not as container to
			// remove some visual noise from UI
			archive(m_children[0], szName, "<0.");   // label looks like a container's element
		}
		else
		{
			archive(FixedContainerSTL(m_children), szName, szLabel);
		}
	}
	else
	{
		archive(m_children, szName, szLabel);
	}
}

void CInputBlueprint::ClearErrors()
{
	assert(m_pErrorCollector);
	m_pErrorCollector->Clear();

	for (CInputBlueprint& input : m_children)
	{
		input.ClearErrors();
	}
}

//////////////////////////////////////////////////////////////////////////

const char* CInputBlueprint::GetParamName() const
{
	return m_paramName.c_str();
}

const UQS::Client::CInputParameterID& CInputBlueprint::GetParamID() const
{
	return m_paramID;
}

const char* CInputBlueprint::GetFuncName() const
{
	return m_functionHelper.GetFunctionInternalName();
}

const CryGUID& CInputBlueprint::GetFuncGUID() const
{
	if (const UQS::Client::IFunctionFactory* pFunctionFactory = m_functionHelper.GetFunctionFactory())
	{
		return pFunctionFactory->GetGUID();
	}
	else
	{
		static const CryGUID nullGUID = CryGUID::Null();
		return nullGUID;
	}
}

const char* CInputBlueprint::GetFuncReturnValueLiteral() const
{
	return m_functionHelper.GetFunctionParamOrReturnValue().c_str();
}

bool CInputBlueprint::GetAddReturnValueToDebugRenderWorldUponExecution() const
{
	return m_bAddReturnValueToDebugRenderWorldUponExecution;
}

CInputBlueprint& CInputBlueprint::AddChild(const char* szParamName, const UQS::Client::CInputParameterID& paramID, const CryGUID& funcGUID, const char* szFuncReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution)
{
	const char* szParamDescription = "";  // FIXME: where to get the description of the parameter from?? ... nevermind, the description eventually ends up in the UI somehow
	m_children.emplace_back(szParamName, paramID, szParamDescription, funcGUID, szFuncReturnValueLiteral, bAddReturnValueToDebugRenderWorldUponExecution);
	return m_children.back();
}

size_t CInputBlueprint::GetChildCount() const
{
	return m_children.size();
}

const CInputBlueprint& CInputBlueprint::GetChild(size_t index) const
{
	return m_children[index];
}

CInputBlueprint* CInputBlueprint::FindChildByParamID(const UQS::Client::CInputParameterID& paramID)
{
	return const_cast<CInputBlueprint*>(FindChildByParamIDConst(paramID));
}

const CInputBlueprint* CInputBlueprint::FindChildByParamIDConst(const UQS::Client::CInputParameterID& paramID) const
{
	auto iter = std::find_if(m_children.begin(), m_children.end(),
	                         [paramID](const CInputBlueprint& bp) { return bp.m_paramID == paramID; });
	return iter != m_children.end() ? &*iter : nullptr;
}

//////////////////////////////////////////////////////////////////////////
// CConstParamBlueprint
//////////////////////////////////////////////////////////////////////////

CConstParamBlueprint::SConstParam::SConstParam()
	: bAddToDebugRenderWorld(false)
	, pErrorCollector(new CErrorCollector)
{}

CConstParamBlueprint::SConstParam::SConstParam(const char* szName, const CryGUID& typeGUID, CItemLiteral&& value, bool _bAddToDebugRenderWorld)
	: name(szName)
	, type(typeGUID)
	, value(std::move(value))
	, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
	, pErrorCollector(new CErrorCollector)
{}

CConstParamBlueprint::SConstParam::SConstParam(SConstParam&& other)
	: name(std::move(other.name))
	, type(std::move(other.type))
	, value(std::move(other.value))
	, bAddToDebugRenderWorld(std::move(other.bAddToDebugRenderWorld))
	, pErrorCollector(std::move(other.pErrorCollector))
{}

CConstParamBlueprint::SConstParam& CConstParamBlueprint::SConstParam::operator=(CConstParamBlueprint::SConstParam&& other)
{
	if (this != &other)
	{
		name = std::move(other.name);
		type = std::move(other.type);
		value = std::move(other.value);
		bAddToDebugRenderWorld = std::move(other.bAddToDebugRenderWorld);
		pErrorCollector = std::move(other.pErrorCollector);
	}
	return *this;
}

void CConstParamBlueprint::SConstParam::Serialize(Serialization::IArchive& archive)
{
	if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
	{
		if (archive.isInput())
		{
			string oldName = name;
			SItemTypeName oldType = type;

			SerializeImpl(archive, *pContext);

			if (CParametersListContext* pParamsContext = pContext->GetParametersListContext())
			{
				if ((name != oldName) || (type != oldType))
				{
					pParamsContext->SetParamsChanged();
				}
			}
		}
		else
		{
			SerializeImpl(archive, *pContext);
		}
	}
	else
	{
		assert(false); // not supported
	}
}

void CConstParamBlueprint::SConstParam::SerializeImpl(Serialization::IArchive& archive, CUqsDocSerializationContext& context)
{
	pErrorCollector->Serialize(archive, *this);

	archive(name, "name", "^Name");
	archive(type, "type", "^Type");

	if (archive.isInput())
	{
		if (type != value.GetTypeName())
		{
			value = CItemLiteral(type, context);
		}
	}

	// description of the type
	if (const UQS::Client::IItemFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(type.GetTypeGUID()))
	{
		archive.doc(pFactory->GetDescription());
	}

	archive(value, "value", "^Value");

	bool bItemCanBeRepresentedInDebugRenderWorld = false;
	if (const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(type.GetTypeGUID()))
	{
		bItemCanBeRepresentedInDebugRenderWorld = pItemFactory->CanBeRepresentedInDebugRenderWorld();
	}

	if (bItemCanBeRepresentedInDebugRenderWorld)
	{
		archive(bAddToDebugRenderWorld, "bAddToDebugRenderWorld", "^DebugRender");
		archive.doc("If enabled, the return value is added to DebugRenderWorld upon execution. Then you can view the value in the query history.");
	}
}

void CConstParamBlueprint::SConstParam::ClearErrors()
{
	assert(pErrorCollector);
	pErrorCollector->Clear();
}

bool CConstParamBlueprint::SConstParam::IsValid() const
{
	return !name.empty() && !type.Empty();
}

void CConstParamBlueprint::AddParameter(const char* szName, const CryGUID& typeGUID, CItemLiteral&& value, bool bAddToDebugRenderWorld)
{
	m_params.emplace_back(szName, typeGUID, std::move(value), bAddToDebugRenderWorld);
}

size_t CConstParamBlueprint::GetParameterCount() const
{
	return m_params.size();
}

void CConstParamBlueprint::GetParameterInfo(size_t index, const char*& szName, const char*& szType, CryGUID& typeGUID, string& value, bool& bAddToDebugRenderWorld, std::shared_ptr<CErrorCollector>& pErrorCollector) const
{
	assert(index < m_params.size());

	const SConstParam& param = m_params[index];
	szName = param.name.c_str();
	szType = param.type.c_str();
	typeGUID = param.type.GetTypeGUID();
	param.value.ConvertToStringLiteral(*&value);
	bAddToDebugRenderWorld = param.bAddToDebugRenderWorld;
	pErrorCollector = param.pErrorCollector;
}

void CConstParamBlueprint::Serialize(Serialization::IArchive& archive)
{
	const size_t oldSize = m_params.size();
	archive(m_params, "params", "^");

	if (m_params.size() != oldSize)
	{
		if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
		{
			if (CParametersListContext* pParamsContext = pContext->GetParametersListContext())
			{
				pParamsContext->SetParamsChanged();
			}
		}
	}
}

void CConstParamBlueprint::PrepareHelpers(CUqsDocSerializationContext& context)
{
	// nothing
}

void CConstParamBlueprint::ClearErrors()
{
	for (SConstParam& param : m_params)
	{
		param.ClearErrors();
	}
}

//////////////////////////////////////////////////////////////////////////
// CRuntimeParamBlueprint
//////////////////////////////////////////////////////////////////////////

CRuntimeParamBlueprint::SRuntimeParam::SRuntimeParam()
	: bAddToDebugRenderWorld(false)
	, pErrorCollector(new CErrorCollector)
{}

CRuntimeParamBlueprint::SRuntimeParam::SRuntimeParam(const char* szName, const CryGUID& typeGUID, bool _bAddToDebugRenderWorld)
	: name(szName)
	, type(typeGUID)
	, bAddToDebugRenderWorld(_bAddToDebugRenderWorld)
	, pErrorCollector(new CErrorCollector)
{}

CRuntimeParamBlueprint::SRuntimeParam::SRuntimeParam(CRuntimeParamBlueprint::SRuntimeParam&& other)
	: name(std::move(other.name))
	, type(std::move(other.type))
	, bAddToDebugRenderWorld(std::move(other.bAddToDebugRenderWorld))
	, pErrorCollector(std::move(other.pErrorCollector))
{}

CRuntimeParamBlueprint::SRuntimeParam& CRuntimeParamBlueprint::SRuntimeParam::operator=(CRuntimeParamBlueprint::SRuntimeParam&& other)
{
	if (this != &other)
	{
		name = std::move(other.name);
		type = std::move(other.type);
		bAddToDebugRenderWorld = std::move(other.bAddToDebugRenderWorld);
		pErrorCollector = std::move(other.pErrorCollector);
	}
	return *this;
}

void CRuntimeParamBlueprint::SRuntimeParam::Serialize(Serialization::IArchive& archive)
{
	if (archive.isInput())
	{
		string oldName = name;
		SItemTypeName oldType = type;

		SerializeImpl(archive);

		if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
		{
			if (CParametersListContext* pParamsContext = pContext->GetParametersListContext())
			{
				if ((name != oldName) || (type != oldType))
				{
					pParamsContext->SetParamsChanged();
				}
			}
		}
	}
	else
	{
		SerializeImpl(archive);
	}
}

void CRuntimeParamBlueprint::SRuntimeParam::SerializeImpl(Serialization::IArchive& archive)
{
	pErrorCollector->Serialize(archive, *this);

	archive(name, "name", "^Name");
	archive(type, "type", "^Type");

	// description of the type
	if (const UQS::Client::IItemFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(type.GetTypeGUID()))
	{
		archive.doc(pFactory->GetDescription());
	}

	bool bItemCanBeRepresentedInDebugRenderWorld = false;
	if (const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetItemFactoryDatabase().FindFactoryByGUID(type.GetTypeGUID()))
	{
		bItemCanBeRepresentedInDebugRenderWorld = pItemFactory->CanBeRepresentedInDebugRenderWorld();
	}

	if (bItemCanBeRepresentedInDebugRenderWorld)
	{
		archive(bAddToDebugRenderWorld, "bAddToDebugRenderWorld", "^DebugRender");
		archive.doc("If enabled, the return value is added to DebugRenderWorld upon execution. Then you can view the value in the query history.");
	}
}

void CRuntimeParamBlueprint::SRuntimeParam::ClearErrors()
{
	assert(pErrorCollector);
	pErrorCollector->Clear();
}

bool CRuntimeParamBlueprint::SRuntimeParam::IsValid() const
{
	return !name.empty() && !type.Empty();
}

void CRuntimeParamBlueprint::AddParameter(const char* szName, const CryGUID& typeGUID, bool bAddToDebugRenderWorld)
{
	m_params.emplace_back(szName, typeGUID, bAddToDebugRenderWorld);
}

size_t CRuntimeParamBlueprint::GetParameterCount() const
{
	return m_params.size();
}

void CRuntimeParamBlueprint::GetParameterInfo(size_t index, const char*& szName, const char*& szType, CryGUID& typeGUID, bool& bAddToDebugRenderWorld, std::shared_ptr<CErrorCollector>& pErrorCollector) const
{
	assert(index < m_params.size());

	const SRuntimeParam& param = m_params[index];
	szName = param.name.c_str();
	szType = param.type.c_str();
	typeGUID = param.type.GetTypeGUID();
	bAddToDebugRenderWorld = param.bAddToDebugRenderWorld;
	pErrorCollector = param.pErrorCollector;
}

void CRuntimeParamBlueprint::Serialize(Serialization::IArchive& archive)
{
	const size_t oldSize = m_params.size();
	archive(m_params, "params", "^");

	if (m_params.size() != oldSize)
	{
		if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
		{
			if (CParametersListContext* pParamsContext = pContext->GetParametersListContext())
			{
				pParamsContext->SetParamsChanged();
			}
		}
	}
}

void CRuntimeParamBlueprint::PrepareHelpers(CUqsDocSerializationContext& context)
{
	// nothing
}

void CRuntimeParamBlueprint::ClearErrors()
{
	for (SRuntimeParam& param : m_params)
	{
		param.ClearErrors();
	}
}


//////////////////////////////////////////////////////////////////////////
// CGeneratorBlueprint
//////////////////////////////////////////////////////////////////////////

CGeneratorBlueprint::CGeneratorBlueprint()
	: m_pErrorCollector(new CErrorCollector)
{}

CGeneratorBlueprint::CGeneratorBlueprint(const UQS::Client::IGeneratorFactory& factory, const CUqsDocSerializationContext& context)
	: m_guid(factory.GetGUID())
	, m_pErrorCollector(new CErrorCollector)
{
	m_inputs.ResetChildrenFromFactory(factory, context);
}

void CGeneratorBlueprint::SetGeneratorGUID(const CryGUID& generatorGUID)
{
	m_guid = generatorGUID;
}

CInputBlueprint& CGeneratorBlueprint::GetInputRoot()
{
	return m_inputs;
}

const CInputBlueprint& CGeneratorBlueprint::GetInputRoot() const
{
	return m_inputs;
}

const char* CGeneratorBlueprint::GetGeneratorName() const
{
	if (const UQS::Client::IGeneratorFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase().FindFactoryByGUID(m_guid))
	{
		return pFactory->GetName();
	}
	else
	{
		return "";
	}
}

const CryGUID& CGeneratorBlueprint::GetGeneratorGUID() const
{
	return m_guid;
}

void CGeneratorBlueprint::Serialize(Serialization::IArchive& archive)
{
	if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
	{
		assert(m_pErrorCollector);

		const bool bGUIDChanged = SerializeGUID(archive, "guid", "^", *pContext);

		// description of the generator
		if (!m_guid.IsNull())
		{
			if (const UQS::Client::IGeneratorFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase().FindFactoryByGUID(m_guid))
			{
				archive.doc(pFactory->GetDescription());
			}
		}

		// type of items to generate
		string itemTypeName;
		if (!m_guid.IsNull())
		{
			if (const UQS::Client::IGeneratorFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase().FindFactoryByGUID(m_guid))
			{
				if (const UQS::Client::IItemFactory* pItemFactory = UQS::Core::IHubPlugin::GetHub().GetUtils().FindItemFactoryByType(pFactory->GetTypeOfItemsToGenerate()))
				{
					itemTypeName.Format("(%s)", pItemFactory->GetName());
				}
				else
				{
					itemTypeName.Format("(%s)", pFactory->GetTypeOfItemsToGenerate().name());
				}
			}
		}
		archive(itemTypeName, "itemTypeName", "^!");

		m_pErrorCollector->Serialize(archive, *this);

		if (!m_guid.IsNull())
		{
			SerializeInputs(archive, "inputs", "Inputs", bGUIDChanged, *pContext);
		}

		if (bGUIDChanged)
		{
			if (pContext->GetSelectedGeneratorContext())
			{
				pContext->GetSelectedGeneratorContext()->SetGeneratorChanged(true);
			}
		}
	}
}

bool CGeneratorBlueprint::SerializeGUID(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context)
{
	bool bGUIDChanged = false;

	const CryGUID oldGeneratorGUID = m_guid;
	auto setGUID = [this, &bGUIDChanged](const CryGUID& newGUID)
	{
		m_guid = newGUID;
		bGUIDChanged = true;
	};

	if (context.GetSettings().bUseSelectionHelpers)
	{
		CKeyValueStringList<CryGUID> generatorGuidList;
		generatorGuidList.FillFromFactoryDatabase(UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase(), true);
		generatorGuidList.SerializeByData(archive, szName, szLabel, oldGeneratorGUID, setGUID);
	}
	else
	{
		SerializeGUIDWithSetter(archive, szName, szLabel, oldGeneratorGUID, setGUID);
	}
	return bGUIDChanged;
}

void CGeneratorBlueprint::SerializeInputs(Serialization::IArchive& archive, const char* szName, const char* szLabel, const bool bGUIDChanged, const CUqsDocSerializationContext& context)
{
	UQS::Client::IGeneratorFactory* pFactory = nullptr;

	if (context.GetSettings().bUseSelectionHelpers)
	{
		pFactory = UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase().FindFactoryByGUID(m_guid);
	}

	if (pFactory && bGUIDChanged)
	{
		m_inputs.ResetChildrenFromFactory(*pFactory, context);
	}

	m_inputs.SerializeRoot(archive, szName, szLabel, context, SValidatorKey::FromObject(*this));

	if (pFactory)
	{
		m_inputs.EnsureChildrenFromFactory(*pFactory, context);
	}
}

void CGeneratorBlueprint::PrepareHelpers(CUqsDocSerializationContext& context)
{
	UQS::Client::IGeneratorFactory* pFactory = nullptr;

	if (context.GetSettings().bUseSelectionHelpers)
	{
		pFactory = UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase().FindFactoryByGUID(m_guid);
	}

	m_inputs.PrepareHelpers(pFactory, context);
}

void CGeneratorBlueprint::ClearErrors()
{
	assert(m_pErrorCollector);
	m_pErrorCollector->Clear();
	m_inputs.ClearErrors();
}

//////////////////////////////////////////////////////////////////////////
// CInstantEvaluatorBlueprint
//////////////////////////////////////////////////////////////////////////

void CInstantEvaluatorBlueprint::SetEvaluatorGUID(const CryGUID& evaluatorGUID)
{
	return Owner().SetEvaluatorGUID(evaluatorGUID);
}

void CInstantEvaluatorBlueprint::SetWeight(float weight)
{
	Owner().SetWeight(weight);
}

void CInstantEvaluatorBlueprint::SetScoreTransformGUID(const CryGUID& scoreTransformGUID)
{
	Owner().SetScoreTransformGUID(scoreTransformGUID);
}

void CInstantEvaluatorBlueprint::SetNegateDiscard(bool bNegateDiscard)
{
	Owner().SetNegateDiscard(bNegateDiscard);
}

float CInstantEvaluatorBlueprint::GetWeight() const
{
	return Owner().GetWeight();
}

const char* CInstantEvaluatorBlueprint::GetScoreTransformName() const
{
	return Owner().GetScoreTransformName();
}

const CryGUID& CInstantEvaluatorBlueprint::GetScoreTransformGUID() const
{
	return Owner().GetScoreTransformGUID();
}

bool CInstantEvaluatorBlueprint::GetNegateDiscard() const
{
	return Owner().GetNegateDiscard();
}

CInputBlueprint& CInstantEvaluatorBlueprint::GetInputRoot()
{
	return Owner().GetInputRoot();
}

const CInputBlueprint& CInstantEvaluatorBlueprint::GetInputRoot() const
{
	return Owner().GetInputRoot();
}

const char* CInstantEvaluatorBlueprint::GetEvaluatorName() const
{
	return Owner().GetEvaluatorName();
}

const CryGUID& CInstantEvaluatorBlueprint::GetEvaluatorGUID() const
{
	return Owner().GetEvaluatorGUID();
}

//////////////////////////////////////////////////////////////////////////
// CDeferredEvaluatorBlueprint
//////////////////////////////////////////////////////////////////////////

void CDeferredEvaluatorBlueprint::SetEvaluatorGUID(const CryGUID& evaluatorGUID)
{
	return Owner().SetEvaluatorGUID(evaluatorGUID);
}

void CDeferredEvaluatorBlueprint::SetWeight(float weight)
{
	Owner().SetWeight(weight);
}

void CDeferredEvaluatorBlueprint::SetScoreTransformGUID(const CryGUID& scoreTransformGUID)
{
	Owner().SetScoreTransformGUID(scoreTransformGUID);
}

void CDeferredEvaluatorBlueprint::SetNegateDiscard(bool bNegateDiscard)
{
	Owner().SetNegateDiscard(bNegateDiscard);
}

float CDeferredEvaluatorBlueprint::GetWeight() const
{
	return Owner().GetWeight();
}

const char* CDeferredEvaluatorBlueprint::GetScoreTransformName() const
{
	return Owner().GetScoreTransformName();
}

const CryGUID& CDeferredEvaluatorBlueprint::GetScoreTransformGUID() const
{
	return Owner().GetScoreTransformGUID();
}

bool CDeferredEvaluatorBlueprint::GetNegateDiscard() const
{
	return Owner().GetNegateDiscard();
}

CInputBlueprint& CDeferredEvaluatorBlueprint::GetInputRoot()
{
	return Owner().GetInputRoot();
}

const CInputBlueprint& CDeferredEvaluatorBlueprint::GetInputRoot() const
{
	return Owner().GetInputRoot();
}

const char* CDeferredEvaluatorBlueprint::GetEvaluatorName() const
{
	return Owner().GetEvaluatorName();
}

const CryGUID& CDeferredEvaluatorBlueprint::GetEvaluatorGUID() const
{
	return Owner().GetEvaluatorGUID();
}

//////////////////////////////////////////////////////////////////////////

CEvaluator& SEvaluatorBlueprintAdapter::Owner() const
{
	assert(m_pOwner);
	return *m_pOwner;
}


//////////////////////////////////////////////////////////////////////////
// CEvaluator
//////////////////////////////////////////////////////////////////////////

void CEvaluator::SetEvaluatorGUID(const CryGUID& evaluatorGUID)
{
	m_evaluatorGUID = evaluatorGUID;
}

void CEvaluator::SetWeight(float weight)
{
	m_weight = weight;
}

void CEvaluator::SetScoreTransformGUID(const CryGUID& scoreTransformGUID)
{
	m_scoreTransformGUID = scoreTransformGUID;
}

void CEvaluator::SetNegateDiscard(bool bNegateDiscard)
{
	m_bNegateDiscard = bNegateDiscard;
}

float CEvaluator::GetWeight() const
{
	return m_weight;
}

const char* CEvaluator::GetScoreTransformName() const
{
	if (const UQS::Core::IScoreTransformFactory* pScoreTransformFactory = UQS::Core::IHubPlugin::GetHub().GetScoreTransformFactoryDatabase().FindFactoryByGUID(m_scoreTransformGUID))
	{
		return pScoreTransformFactory->GetName();
	}
	else
	{
		return "";
	}
}

const CryGUID& CEvaluator::GetScoreTransformGUID() const
{
	return m_scoreTransformGUID;
}

bool CEvaluator::GetNegateDiscard() const
{
	return m_bNegateDiscard;
}

CInputBlueprint& CEvaluator::GetInputRoot()
{
	return m_inputs;
}

const CInputBlueprint& CEvaluator::GetInputRoot() const
{
	return m_inputs;
}

const char* CEvaluator::GetEvaluatorName() const
{
	const char* szEvaluatorName = "";

	if (m_evaluatorType == EEvaluatorType::Instant)
	{
		if (const UQS::Client::IInstantEvaluatorFactory* pInstantEvaluatorFactory = UQS::Core::IHubPlugin::GetHub().GetInstantEvaluatorFactoryDatabase().FindFactoryByGUID(m_evaluatorGUID))
		{
			szEvaluatorName = pInstantEvaluatorFactory->GetName();
		}
	}
	else if (m_evaluatorType == EEvaluatorType::Deferred)
	{
		if (const UQS::Client::IDeferredEvaluatorFactory* pDeferredEvaluatorFactory = UQS::Core::IHubPlugin::GetHub().GetDeferredEvaluatorFactoryDatabase().FindFactoryByGUID(m_evaluatorGUID))
		{
			szEvaluatorName = pDeferredEvaluatorFactory->GetName();
		}
	}

	return szEvaluatorName;
}

const CryGUID& CEvaluator::GetEvaluatorGUID() const
{
	return m_evaluatorGUID;
}

CEvaluator CEvaluator::CreateInstant()
{
	return CEvaluator(EEvaluatorType::Instant);
}

CEvaluator CEvaluator::CreateDeferred()
{
	return CEvaluator(EEvaluatorType::Deferred);
}

CInstantEvaluatorBlueprint& CEvaluator::AsInstant()
{
	if (m_evaluatorType != EEvaluatorType::Instant)
	{
		CryFatalError("Can't cast evaluator to instant evaluator");
	}

	return *static_cast<CInstantEvaluatorBlueprint*>(m_interfaceAdapter.get());
}

CDeferredEvaluatorBlueprint& CEvaluator::AsDeferred()
{
	if (m_evaluatorType != EEvaluatorType::Deferred)
	{
		CryFatalError("Can't cast evaluator to deferred evaluator");
	}

	return *static_cast<CDeferredEvaluatorBlueprint*>(m_interfaceAdapter.get());
}

EEvaluatorType CEvaluator::GetType() const
{
	return m_evaluatorType;
}

CEvaluator::CEvaluator(const EEvaluatorType type)
	: m_evaluatorGUID()
	, m_weight(1.0f)
	, m_scoreTransformGUID()
	, m_bNegateDiscard(false)
	, m_inputs()
	, m_pErrorCollector(new CErrorCollector)
	, m_evaluatorType(EEvaluatorType::Undefined)
	, m_evaluatorCost(EEvaluatorCost::Undefined)
{
	SetType(type);
}

void CEvaluator::SetType(EEvaluatorType type)
{
	m_evaluatorType = type;

	switch (type)
	{
	case EEvaluatorType::Instant:
		m_interfaceAdapter.reset(new CInstantEvaluatorBlueprint(*this));
		break;

	case EEvaluatorType::Deferred:
		m_interfaceAdapter.reset(new CDeferredEvaluatorBlueprint(*this));
		break;

	case EEvaluatorType::Undefined:
	default:
		CryFatalError("Attempt to create an evaluator of unknown type");
		break;
	}
}

CEvaluator::CEvaluator(CEvaluator&& other)
	: m_evaluatorGUID(std::move(other.m_evaluatorGUID))
	, m_weight(other.m_weight)
	, m_inputs(std::move(other.m_inputs))
	, m_scoreTransformGUID(std::move(other.m_scoreTransformGUID))
	, m_bNegateDiscard(other.m_bNegateDiscard)
	, m_pErrorCollector(std::move(other.m_pErrorCollector))
	, m_evaluatorType(other.m_evaluatorType)
	, m_evaluatorCost(other.m_evaluatorCost)
	, m_interfaceAdapter(std::move(other.m_interfaceAdapter))
{
	if (m_interfaceAdapter)
	{
		m_interfaceAdapter->SetOwner(*this);
	}
}

CEvaluator::CEvaluator()
	: m_evaluatorGUID()
	, m_weight(1.0f)
	, m_scoreTransformGUID()
	, m_bNegateDiscard(false)
	, m_inputs()
	, m_pErrorCollector(new CErrorCollector)
	, m_evaluatorType(EEvaluatorType::Undefined)
	, m_evaluatorCost(EEvaluatorCost::Undefined)
	, m_interfaceAdapter()
{

}

CEvaluator& CEvaluator::operator=(CEvaluator&& other)
{
	if (this != &other)
	{
		m_evaluatorGUID = std::move(other.m_evaluatorGUID);
		m_weight = other.m_weight;
		m_inputs = std::move(other.m_inputs);
		m_scoreTransformGUID = std::move(other.m_scoreTransformGUID);
		m_bNegateDiscard = other.m_bNegateDiscard;
		m_pErrorCollector = std::move(other.m_pErrorCollector);
		m_evaluatorType = other.m_evaluatorType;
		m_evaluatorCost = other.m_evaluatorCost;
		m_interfaceAdapter = std::move(other.m_interfaceAdapter);
		if (m_interfaceAdapter)
		{
			m_interfaceAdapter->SetOwner(*this);
		}
	}
	return *this;
}

void CEvaluator::Serialize(Serialization::IArchive& archive)
{
	if (CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>())
	{
		assert(m_pErrorCollector);

		const bool bGUIDChanged = SerializeEvaluatorGUID(archive, "evaluatorGUID", "^", *pContext);

		m_pErrorCollector->Serialize(archive, *this);

		if (!m_evaluatorGUID.IsNull())
		{
			CEvaluatorFactoryHelper factory;

			const bool bUseSelectionHelpers = pContext->GetSettings().bUseSelectionHelpers;

			if (bUseSelectionHelpers)
			{
				factory = CEvaluatorFactoryHelper(*pContext, m_evaluatorGUID);
			}

			if (factory.IsValid())
			{
				SetType(factory.GetType());
				m_evaluatorCost = factory.GetCost();
				if (bGUIDChanged)
				{
					m_inputs.ResetChildrenFromFactory(factory, *pContext);
				}

				// description of the evaluator
				archive.doc(factory.GetDescription());
			}

			// Always set type from factory (if available), and only show it on output as read-only field
			if (!bUseSelectionHelpers || archive.isOutput())
			{
				const char* const szEvaluatorTypeLabel = bUseSelectionHelpers
					? "!Evaluator type"
					: "Evaluator type";
				archive(m_evaluatorType, "evaluatorType", szEvaluatorTypeLabel);
			}

			if (m_evaluatorType == EEvaluatorType::Undefined)
			{
				archive.warning(m_evaluatorType, "Undefined evaluator type");
			}

			// Evaluator cost (read-only)
			archive(m_evaluatorCost, "evaluatorCost", "!Evaluator cost");

			archive(m_weight, "weight", "Weight");

			SerializeScoreTransformGUID(archive, "scoreTransformGUID", "^Score Transform", *pContext);

			archive(m_bNegateDiscard, "negateDiscard", "^Negate Discard");

			m_inputs.SerializeRoot(archive, "inputs", "Inputs", *pContext, SValidatorKey::FromObject(*this));

			if (factory.IsValid())
			{
				m_inputs.EnsureChildrenFromFactory(factory, *pContext);
			}
		}
	}
}

bool CEvaluator::SerializeEvaluatorGUID(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context)
{
	bool bGUIDChanged = false;

	const CryGUID oldGUID = m_evaluatorGUID;
	auto setGUID = [this, &bGUIDChanged](const CryGUID& newGUID)
	{
		m_evaluatorGUID = newGUID;
		bGUIDChanged = true;
	};

	if (context.GetSettings().bUseSelectionHelpers)
	{
		CKeyValueStringList<CryGUID> evaluatorGuidList;
		evaluatorGuidList.FillFromFactoryDatabase(UQS::Core::IHubPlugin::GetHub().GetInstantEvaluatorFactoryDatabase(), true);
		evaluatorGuidList.FillFromFactoryDatabase(UQS::Core::IHubPlugin::GetHub().GetDeferredEvaluatorFactoryDatabase(), false);
		evaluatorGuidList.SerializeByData(archive, szName, szLabel, oldGUID, setGUID);
	}
	else
	{
		SerializeGUIDWithSetter(archive, szName, szLabel, oldGUID, setGUID);
	}
	return bGUIDChanged;
}

void CEvaluator::SerializeScoreTransformGUID(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext& context)
{
	const CryGUID oldScoreTransformGUID = m_scoreTransformGUID;
	auto setScoreTransformGUID = [this](const CryGUID& newGUID)
	{
		m_scoreTransformGUID = newGUID;
	};

	if (context.GetSettings().bUseSelectionHelpers)
	{
		CKeyValueStringList<CryGUID> scoreTransformsList;
		scoreTransformsList.FillFromFactoryDatabase(UQS::Core::IHubPlugin::GetHub().GetScoreTransformFactoryDatabase(), true);
		scoreTransformsList.SerializeByData(archive, szName, szLabel, oldScoreTransformGUID, setScoreTransformGUID);
	}
	else
	{
		SerializeGUIDWithSetter(archive, szName, szLabel, oldScoreTransformGUID, setScoreTransformGUID);
	}

	// description of the score-transform
	if (const UQS::Core::IScoreTransformFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetScoreTransformFactoryDatabase().FindFactoryByGUID(m_scoreTransformGUID))
	{
		archive.doc(pFactory->GetDescription());
	}
}

void CEvaluator::PrepareHelpers(CUqsDocSerializationContext& context)
{
	CEvaluatorFactoryHelper factory;
	CEvaluatorFactoryHelper* pFactory = nullptr;

	if (context.GetSettings().bUseSelectionHelpers)
	{
		factory = CEvaluatorFactoryHelper(context, m_evaluatorGUID);
	}

	if (factory.IsValid())
	{
		pFactory = &factory;
	}

	m_inputs.PrepareHelpers(pFactory, context);
}

void CEvaluator::ClearErrors()
{
	assert(m_pErrorCollector);
	m_pErrorCollector->Clear();
	m_inputs.ClearErrors();
}

//////////////////////////////////////////////////////////////////////////
// CEvaluators
//////////////////////////////////////////////////////////////////////////

CInstantEvaluatorBlueprint& CEvaluators::AddInstantEvaluator()
{
	m_evaluators.emplace_back(CEvaluator::CreateInstant());
	return m_evaluators.back().AsInstant();
}

size_t CEvaluators::GetInstantEvaluatorCount() const
{
	return CalcCountOfType(EEvaluatorType::Instant);
}

CInstantEvaluatorBlueprint& CEvaluators::GetInstantEvaluator(size_t index)
{
	Evaluators::iterator iter = FindByIndexOfType(EEvaluatorType::Instant, index);
	if (iter != m_evaluators.end())
	{
		return iter->AsInstant();
	}

	CryFatalError("Index out of bounds");
	return *static_cast<CInstantEvaluatorBlueprint*>(nullptr);
}

const CInstantEvaluatorBlueprint& CEvaluators::GetInstantEvaluator(size_t index) const
{
	return const_cast<CEvaluators*>(this)->GetInstantEvaluator(index);
}

void CEvaluators::RemoveInstantEvaluator(size_t index)
{
	m_evaluators.erase(FindByIndexOfType(EEvaluatorType::Instant, index));
}

CDeferredEvaluatorBlueprint& CEvaluators::AddDeferredEvaluator()
{
	m_evaluators.emplace_back(CEvaluator::CreateDeferred());
	return m_evaluators.back().AsDeferred();
}

size_t CEvaluators::GetDeferredEvaluatorCount() const
{
	return CalcCountOfType(EEvaluatorType::Deferred);
}

CDeferredEvaluatorBlueprint& CEvaluators::GetDeferredEvaluator(size_t index)
{
	Evaluators::iterator iter = FindByIndexOfType(EEvaluatorType::Deferred, index);
	if (iter != m_evaluators.end())
	{
		return iter->AsDeferred();
	}

	CryFatalError("Index out of bounds");
	return *reinterpret_cast<CDeferredEvaluatorBlueprint*>(nullptr);
}

const CDeferredEvaluatorBlueprint& CEvaluators::GetDeferredEvaluator(size_t index) const
{
	return const_cast<CEvaluators*>(this)->GetDeferredEvaluator(index);
}

void CEvaluators::RemoveDeferredEvaluator(size_t index)
{
	m_evaluators.erase(FindByIndexOfType(EEvaluatorType::Deferred, index));
}

CEvaluators::Evaluators::iterator CEvaluators::FindByIndexOfType(const EEvaluatorType type, const size_t index)
{
	size_t idx = 0;

	const Evaluators::iterator iterEnd = m_evaluators.end();

	for (Evaluators::iterator iter = m_evaluators.begin(); iter != iterEnd; ++iter)
	{
		CEvaluator& eval = *iter;
		if (eval.GetType() == type)
		{
			if (idx == index)
			{
				return iter;
			}
			++idx;
		}
	}

	return iterEnd;
}

size_t CEvaluators::CalcCountOfType(const EEvaluatorType type) const
{
	return std::count_if(m_evaluators.begin(), m_evaluators.end(),
	                     [type](const CEvaluator& eval) { return eval.GetType() == type; });
}

void CEvaluators::Serialize(Serialization::IArchive& archive)
{
	archive(m_evaluators, "evaluators", "^");
}

void CEvaluators::PrepareHelpers(CUqsDocSerializationContext& context)
{
	for (CEvaluator& evaluator : m_evaluators)
	{
		evaluator.PrepareHelpers(context);
	}
}

void CEvaluators::ClearErrors()
{
	for (CEvaluator& evaluator : m_evaluators)
	{
		evaluator.ClearErrors();
	}
}

//////////////////////////////////////////////////////////////////////////
// SQueryFactoryType::CTraits
//////////////////////////////////////////////////////////////////////////

SQueryFactoryType::CTraits::CTraits()
	: m_pQueryFactory(nullptr)
{}

SQueryFactoryType::CTraits::CTraits(const UQS::Core::IQueryFactory& queryFactory)
	: m_pQueryFactory(&queryFactory)
{}

bool SQueryFactoryType::CTraits::operator==(const CTraits& other) const
{
	if (IsUndefined() || other.IsUndefined())
		return false;

	return (RequiresGenerator() == other.RequiresGenerator())
		&& (SupportsParameters() == other.SupportsParameters())
		&& (GetMinRequiredChildren() == other.GetMinRequiredChildren())
		&& (GetMaxAllowedChildren() == other.GetMaxAllowedChildren());
}

bool SQueryFactoryType::CTraits::IsUndefined() const
{
	return (m_pQueryFactory == nullptr);
}

bool SQueryFactoryType::CTraits::RequiresGenerator() const
{
	if (m_pQueryFactory) return m_pQueryFactory->RequiresGenerator();
	return true;
}

bool SQueryFactoryType::CTraits::SupportsEvaluators() const
{
	if (m_pQueryFactory) return m_pQueryFactory->SupportsEvaluators();
	return true;
}

bool SQueryFactoryType::CTraits::SupportsParameters() const
{
	if (m_pQueryFactory) return m_pQueryFactory->SupportsParameters();
	return true;
}

size_t SQueryFactoryType::CTraits::GetMinRequiredChildren() const
{
	if (m_pQueryFactory) return m_pQueryFactory->GetMinRequiredChildren();
	return 0;
}

size_t SQueryFactoryType::CTraits::GetMaxAllowedChildren() const
{
	if (m_pQueryFactory) return m_pQueryFactory->GetMaxAllowedChildren();
	return UQS::Core::IQueryFactory::kUnlimitedChildren;
}

bool SQueryFactoryType::CTraits::IsUnlimitedChildren() const
{
	return GetMaxAllowedChildren() == UQS::Core::IQueryFactory::kUnlimitedChildren;
}

bool SQueryFactoryType::CTraits::CanHaveChildren() const
{
	return GetMaxAllowedChildren() > 0;
}

//////////////////////////////////////////////////////////////////////////
// SQueryFactoryType
//////////////////////////////////////////////////////////////////////////

SQueryFactoryType::SQueryFactoryType(const CryGUID& queryFactoryGUID)
	: m_queryFactoryGUID(queryFactoryGUID)
{}

bool SQueryFactoryType::Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel, const CUqsDocSerializationContext* pContext)
{
	bool bGUIDChanged = false;

	const CryGUID oldGUID = m_queryFactoryGUID;
	auto setGUID = [this, &bGUIDChanged](const CryGUID& newGUID)
	{
		m_queryFactoryGUID = newGUID;
		bGUIDChanged = true;
	};

	if (pContext && pContext->GetSettings().bUseSelectionHelpers)
	{
		CKeyValueStringList<CryGUID> queryGuidList;
		queryGuidList.FillFromFactoryDatabase(UQS::Core::IHubPlugin::GetHub().GetQueryFactoryDatabase(), true);
		queryGuidList.SerializeByData(archive, szName, szLabel, oldGUID, setGUID);
	}
	else
	{
		SerializeGUIDWithSetter(archive, szName, szLabel, oldGUID, setGUID);
	}

	const UQS::Core::IQueryFactory* pFactory = GetFactory(pContext);

	// description of the query type
	if (pFactory)
	{
		archive.doc(pFactory->GetDescription());
	}

	if (bGUIDChanged || m_queryTraits.IsUndefined())
	{
		UpdateTraits(pContext);
	}

	if (!m_queryFactoryGUID.IsNull() && !pFactory)
	{
		char guidAsString[37];
		m_queryFactoryGUID.ToString(guidAsString);
		archive.warning(oldGUID, "Query factory type with GUID = '%s' is unknown to the editor, helpers might be wrong.", guidAsString);
	}

	return bGUIDChanged;
}

const char* SQueryFactoryType::GetFactoryName() const
{
	if (const UQS::Core::IQueryFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetQueryFactoryDatabase().FindFactoryByGUID(m_queryFactoryGUID))
	{
		return pFactory->GetName();
	}
	else
	{
		return "";
	}
}

const UQS::Core::IQueryFactory* SQueryFactoryType::GetFactory(const CUqsDocSerializationContext* pContext) const
{
	if (pContext && pContext->GetSettings().bUseSelectionHelpers)
	{
		return UQS::Core::IHubPlugin::GetHub().GetQueryFactoryDatabase().FindFactoryByGUID(m_queryFactoryGUID);
	}
	return nullptr;
}

void SQueryFactoryType::UpdateTraits(const CUqsDocSerializationContext* pContext)
{
	if (pContext && pContext->GetSettings().bUseSelectionHelpers)
	{
		if (const UQS::Core::IQueryFactory* pQueryFactory = UQS::Core::IHubPlugin::GetHub().GetQueryFactoryDatabase().FindFactoryByGUID(m_queryFactoryGUID))
		{
			m_queryTraits = CTraits(*pQueryFactory);
			return;
		}
	}
	m_queryTraits = CTraits();
}

//////////////////////////////////////////////////////////////////////////
// CQueryBlueprint
//////////////////////////////////////////////////////////////////////////

CQueryBlueprint::CQueryBlueprint()
	: m_name()
	, m_queryFactory(UQS::Core::IHubPlugin::GetHub().GetUtils().GetDefaultQueryFactory().GetGUID())
	, m_maxItemsToKeepInResultSet(1)
	, m_constParams()
	, m_runtimeParams()
	, m_generator()
	, m_evaluators()
	, m_queryChildren()
	, m_pErrorCollector(new CErrorCollector)
{
}

void CQueryBlueprint::BuildSelfFromITextualQueryBlueprint(const UQS::Core::ITextualQueryBlueprint& source, CUqsDocSerializationContext& context)
{
	// name
	{
		m_name = source.GetName();
	}

	// query factory
	{
		m_queryFactory = SQueryFactoryType(source.GetQueryFactoryGUID());
	}

	// query children
	{
		m_queryChildren.reserve(source.GetChildCount());
		for (size_t i = 0; i < source.GetChildCount(); ++i)
		{
			const UQS::Core::ITextualQueryBlueprint& childQueryBP = source.GetChild(i);
			m_queryChildren.emplace_back();
			m_queryChildren.back().BuildSelfFromITextualQueryBlueprint(childQueryBP, context);
		}
	}

	// max. items to keep in result set
	{
		m_maxItemsToKeepInResultSet = source.GetMaxItemsToKeepInResultSet();
	}

	// global constant-params
	{
		const UQS::Core::ITextualGlobalConstantParamsBlueprint& sourceGlobalConstParams = source.GetGlobalConstantParams();

		for (size_t i = 0, n = sourceGlobalConstParams.GetParameterCount(); i < n; ++i)
		{
			const UQS::Core::ITextualGlobalConstantParamsBlueprint::SParameterInfo pi = sourceGlobalConstParams.GetParameter(i);
			CItemLiteral value(pi.typeGUID, context);
			value.SetFromStringLiteral(CONST_TEMP_STRING(pi.szValue));
			m_constParams.AddParameter(pi.szName, pi.typeGUID, std::move(value), pi.bAddToDebugRenderWorld);
		}
	}

	// global runtime-params
	{
		const UQS::Core::ITextualGlobalRuntimeParamsBlueprint& sourceGlobalRuntimeParams = source.GetGlobalRuntimeParams();

		for (size_t i = 0, n = sourceGlobalRuntimeParams.GetParameterCount(); i < n; ++i)
		{
			const UQS::Core::ITextualGlobalRuntimeParamsBlueprint::SParameterInfo pi = sourceGlobalRuntimeParams.GetParameter(i);
			m_runtimeParams.AddParameter(pi.szName, pi.typeGUID, pi.bAddToDebugRenderWorld);
		}
	}

	// generator (optional and typically never used by composite queries)
	{
		if (const UQS::Core::ITextualGeneratorBlueprint* pSourceGeneratorBlueprint = source.GetGenerator())
		{
			m_generator.SetGeneratorGUID(pSourceGeneratorBlueprint->GetGeneratorGUID());

			HelpBuildCInputBlueprintHierarchyFromITextualInputBlueprint(m_generator.GetInputRoot(), pSourceGeneratorBlueprint->GetInputRoot());
		}
	}

	// instant-evaluators
	{
		for (size_t i = 0, n = source.GetInstantEvaluatorCount(); i < n; ++i)
		{
			const UQS::Core::ITextualEvaluatorBlueprint& sourceInstantEvaluatorBlueprint = source.GetInstantEvaluator(i);
			CInstantEvaluatorBlueprint& targetInstantEvaluatorBlueprint = m_evaluators.AddInstantEvaluator();

			targetInstantEvaluatorBlueprint.SetEvaluatorGUID(sourceInstantEvaluatorBlueprint.GetEvaluatorGUID());
			targetInstantEvaluatorBlueprint.SetWeight(sourceInstantEvaluatorBlueprint.GetWeight());
			targetInstantEvaluatorBlueprint.SetScoreTransformGUID(sourceInstantEvaluatorBlueprint.GetScoreTransformGUID());
			targetInstantEvaluatorBlueprint.SetNegateDiscard(sourceInstantEvaluatorBlueprint.GetNegateDiscard());

			HelpBuildCInputBlueprintHierarchyFromITextualInputBlueprint(targetInstantEvaluatorBlueprint.GetInputRoot(), sourceInstantEvaluatorBlueprint.GetInputRoot());
		}
	}

	// deferred-evaluators
	{
		for (size_t i = 0, n = source.GetDeferredEvaluatorCount(); i < n; ++i)
		{
			const UQS::Core::ITextualEvaluatorBlueprint& sourceDeferredEvaluatorBlueprint = source.GetDeferredEvaluator(i);
			CDeferredEvaluatorBlueprint& targetDeferredEvaluatorBlueprint = m_evaluators.AddDeferredEvaluator();

			targetDeferredEvaluatorBlueprint.SetEvaluatorGUID(sourceDeferredEvaluatorBlueprint.GetEvaluatorGUID());
			targetDeferredEvaluatorBlueprint.SetWeight(sourceDeferredEvaluatorBlueprint.GetWeight());
			targetDeferredEvaluatorBlueprint.SetScoreTransformGUID(sourceDeferredEvaluatorBlueprint.GetScoreTransformGUID());
			targetDeferredEvaluatorBlueprint.SetNegateDiscard(sourceDeferredEvaluatorBlueprint.GetNegateDiscard());

			HelpBuildCInputBlueprintHierarchyFromITextualInputBlueprint(targetDeferredEvaluatorBlueprint.GetInputRoot(), sourceDeferredEvaluatorBlueprint.GetInputRoot());
		}
	}
}

void CQueryBlueprint::BuildITextualQueryBlueprintFromSelf(UQS::Core::ITextualQueryBlueprint& target) const
{
	target.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(m_pErrorCollector));

	// name
	{
		target.SetName(m_name.c_str());
	}

	// query factory name
	{
		target.SetQueryFactoryName(m_queryFactory.GetFactoryName());
	}

	// query factory GUID
	{
		target.SetQueryFactoryGUID(m_queryFactory.GetFactoryGUID());
	}

	// query children
	{
		for (const CQueryBlueprint& childQuery : m_queryChildren)
		{
			UQS::Core::ITextualQueryBlueprint& targetChild = target.AddChild();
			childQuery.BuildITextualQueryBlueprintFromSelf(targetChild);
		}
	}

	// max. items to keep in result set
	{
		target.SetMaxItemsToKeepInResultSet(m_maxItemsToKeepInResultSet);
	}

	// global constant-params
	{
		UQS::Core::ITextualGlobalConstantParamsBlueprint& targetConstParams = target.GetGlobalConstantParams();

		string value;
		for (size_t i = 0, n = m_constParams.GetParameterCount(); i < n; ++i)
		{
			const char* szName;
			const char* szType;
			CryGUID typeGUID;
			bool bAddToDebugRenderWorld;
			value.clear();
			std::shared_ptr<CErrorCollector> pErrorCollector;
			m_constParams.GetParameterInfo(i, szName, szType, typeGUID, *&value, bAddToDebugRenderWorld, pErrorCollector);
			targetConstParams.AddParameter(szName, szType, typeGUID, value.c_str(), bAddToDebugRenderWorld, MakeNewSyntaxErrorCollectorUniquePtr(pErrorCollector));
		}
	}

	// global runtime-params
	{
		UQS::Core::ITextualGlobalRuntimeParamsBlueprint& targetRuntimeParams = target.GetGlobalRuntimeParams();

		for (size_t i = 0, n = m_runtimeParams.GetParameterCount(); i < n; ++i)
		{
			const char* szName;
			const char* szType;
			CryGUID typeGUID;
			bool bAddToDebugRenderWorld;
			std::shared_ptr<CErrorCollector> pErrorCollector;
			m_runtimeParams.GetParameterInfo(i, szName, szType, typeGUID, bAddToDebugRenderWorld, pErrorCollector);
			targetRuntimeParams.AddParameter(szName, szType, typeGUID, bAddToDebugRenderWorld, MakeNewSyntaxErrorCollectorUniquePtr(pErrorCollector));
		}
	}

	// generator
	if (m_generator.IsSet())
	{
		UQS::Core::ITextualGeneratorBlueprint& targetGeneratorBlueprint = target.SetGenerator();

		targetGeneratorBlueprint.SetGeneratorName(m_generator.GetGeneratorName());
		targetGeneratorBlueprint.SetGeneratorGUID(m_generator.GetGeneratorGUID());
		targetGeneratorBlueprint.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(m_generator.GetErrorCollectorSharedPtr()));

		HelpBuildITextualInputBlueprintHierarchyFromCInputBlueprint(targetGeneratorBlueprint.GetInputRoot(), m_generator.GetInputRoot());
	}

	// instant-evaluators
	{
		for (size_t i = 0, n = m_evaluators.GetInstantEvaluatorCount(); i < n; ++i)
		{
			const CInstantEvaluatorBlueprint& sourceInstantEvaluatorBlueprint = m_evaluators.GetInstantEvaluator(i);
			UQS::Core::ITextualEvaluatorBlueprint& targetInstantEvaluatorBlueprint = target.AddInstantEvaluator();

			targetInstantEvaluatorBlueprint.SetEvaluatorName(sourceInstantEvaluatorBlueprint.GetEvaluatorName());
			targetInstantEvaluatorBlueprint.SetEvaluatorGUID(sourceInstantEvaluatorBlueprint.GetEvaluatorGUID());
			targetInstantEvaluatorBlueprint.SetWeight(sourceInstantEvaluatorBlueprint.GetWeight());
			targetInstantEvaluatorBlueprint.SetScoreTransformName(sourceInstantEvaluatorBlueprint.GetScoreTransformName());
			targetInstantEvaluatorBlueprint.SetScoreTransformGUID(sourceInstantEvaluatorBlueprint.GetScoreTransformGUID());
			targetInstantEvaluatorBlueprint.SetNegateDiscard(sourceInstantEvaluatorBlueprint.GetNegateDiscard());
			targetInstantEvaluatorBlueprint.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(sourceInstantEvaluatorBlueprint.Owner().GetErrorCollectorSharedPtr()));

			HelpBuildITextualInputBlueprintHierarchyFromCInputBlueprint(targetInstantEvaluatorBlueprint.GetInputRoot(), sourceInstantEvaluatorBlueprint.GetInputRoot());
		}
	}

	// deferred-evaluators
	{
		for (size_t i = 0, n = m_evaluators.GetDeferredEvaluatorCount(); i < n; ++i)
		{
			const CDeferredEvaluatorBlueprint& sourceDeferredEvaluatorBlueprint = m_evaluators.GetDeferredEvaluator(i);
			UQS::Core::ITextualEvaluatorBlueprint& targetDeferredEvaluatorBlueprint = target.AddDeferredEvaluator();

			targetDeferredEvaluatorBlueprint.SetEvaluatorName(sourceDeferredEvaluatorBlueprint.GetEvaluatorName());
			targetDeferredEvaluatorBlueprint.SetEvaluatorGUID(sourceDeferredEvaluatorBlueprint.GetEvaluatorGUID());
			targetDeferredEvaluatorBlueprint.SetWeight(sourceDeferredEvaluatorBlueprint.GetWeight());
			targetDeferredEvaluatorBlueprint.SetScoreTransformName(sourceDeferredEvaluatorBlueprint.GetScoreTransformName());
			targetDeferredEvaluatorBlueprint.SetScoreTransformGUID(sourceDeferredEvaluatorBlueprint.GetScoreTransformGUID());
			targetDeferredEvaluatorBlueprint.SetNegateDiscard(sourceDeferredEvaluatorBlueprint.GetNegateDiscard());
			targetDeferredEvaluatorBlueprint.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(sourceDeferredEvaluatorBlueprint.Owner().GetErrorCollectorSharedPtr()));

			HelpBuildITextualInputBlueprintHierarchyFromCInputBlueprint(targetDeferredEvaluatorBlueprint.GetInputRoot(), sourceDeferredEvaluatorBlueprint.GetInputRoot());
		}
	}
}

void CQueryBlueprint::HelpBuildCInputBlueprintHierarchyFromITextualInputBlueprint(CInputBlueprint& targetRoot, const UQS::Core::ITextualInputBlueprint& sourceRoot)
{
	for (size_t i = 0, n = sourceRoot.GetChildCount(); i < n; ++i)
	{
		const UQS::Core::ITextualInputBlueprint& sourceChild = sourceRoot.GetChild(i);
		const char* szParamName = sourceChild.GetParamName();
		const UQS::Client::CInputParameterID& paramID = sourceChild.GetParamID();
		const CryGUID& funcGUID = sourceChild.GetFuncGUID();
		// notice: we don't care about the Function's GUID as the Function's name is already unique
		const char* szFuncReturnValueLiteral = sourceChild.GetFuncReturnValueLiteral();
		bool bAddReturnValueToDebugRenderWorldUponExecution = sourceChild.GetAddReturnValueToDebugRenderWorldUponExecution();

		CInputBlueprint& newChild = targetRoot.AddChild(szParamName, paramID, funcGUID, szFuncReturnValueLiteral, bAddReturnValueToDebugRenderWorldUponExecution);

		// recurse down
		HelpBuildCInputBlueprintHierarchyFromITextualInputBlueprint(newChild, sourceChild);
	}
}

void CQueryBlueprint::HelpBuildITextualInputBlueprintHierarchyFromCInputBlueprint(UQS::Core::ITextualInputBlueprint& targetRoot, const CInputBlueprint& sourceRoot)
{
	for (size_t i = 0, n = sourceRoot.GetChildCount(); i < n; ++i)
	{
		const CInputBlueprint& sourceChild = sourceRoot.GetChild(i);
		const char* szParamName = sourceChild.GetParamName();
		const UQS::Client::CInputParameterID& paramID = sourceChild.GetParamID();
		const char* szFuncName = sourceChild.GetFuncName();
		const CryGUID& funcGUID = sourceChild.GetFuncGUID();
		const char* szFuncReturnValueLiteral = sourceChild.GetFuncReturnValueLiteral();
		bool bAddReturnValueToDebugRenderWorldUponExecution = sourceChild.GetAddReturnValueToDebugRenderWorldUponExecution();
		const std::shared_ptr<CErrorCollector>& pErrorCollector = sourceChild.GetErrorCollectorSharedPtr();

		UQS::Core::ITextualInputBlueprint& newChild = targetRoot.AddChild(szParamName, paramID, szFuncName, funcGUID, szFuncReturnValueLiteral, bAddReturnValueToDebugRenderWorldUponExecution);
		newChild.SetSyntaxErrorCollector(MakeNewSyntaxErrorCollectorUniquePtr(pErrorCollector));

		// recurse down
		HelpBuildITextualInputBlueprintHierarchyFromCInputBlueprint(newChild, sourceChild);
	}
}

void CQueryBlueprint::CheckQueryTraitsChange(const SQueryFactoryType::CTraits& queryTraits, const SQueryFactoryType::CTraits& oldTraits, CParametersListContext& paramListContext)
{
	if (oldTraits != queryTraits && !oldTraits.IsUndefined() && !queryTraits.IsUndefined())
	{
		if (oldTraits.SupportsParameters() != queryTraits.SupportsParameters())
		{
			paramListContext.SetParamsChanged();
			if (!queryTraits.SupportsParameters())
			{
				m_constParams = CConstParamBlueprint();
				m_runtimeParams = CRuntimeParamBlueprint();
			}
		}
		if (oldTraits.RequiresGenerator() != queryTraits.RequiresGenerator())
		{
			if (!queryTraits.RequiresGenerator())
			{
				m_generator = CGeneratorBlueprint();
			}
		}
		if (oldTraits.SupportsEvaluators() != queryTraits.SupportsEvaluators())
		{
			if (!queryTraits.SupportsEvaluators())
			{
				m_evaluators = CEvaluators();
			}
		}
		if (oldTraits.CanHaveChildren() != queryTraits.CanHaveChildren())
		{
			if (!queryTraits.CanHaveChildren())
			{
				m_queryChildren = QueryBlueprintChildren();
			}
		}
	}
}

void CQueryBlueprint::Serialize(Serialization::IArchive& archive)
{
	assert(m_pErrorCollector);

	m_pErrorCollector->Serialize(archive, *this);

	CUqsDocSerializationContext* pContext = archive.context<CUqsDocSerializationContext>();

	if (!m_name.empty())
	{
		archive(m_name, "name", "!Name");
	}

	const SQueryFactoryType::CTraits oldTraits = m_queryFactory.GetTraits();
	m_queryFactory.Serialize(archive, "queryFactory", "^Query Factory", pContext);
	const SQueryFactoryType::CTraits& queryTraits = m_queryFactory.GetTraits();

	CParametersListContext paramListContext(this, pContext);

	CheckQueryTraitsChange(queryTraits, oldTraits, paramListContext);

	archive(m_maxItemsToKeepInResultSet, "maxItemsToKeep", "Max items to keep in result set");

	if (queryTraits.SupportsParameters())
	{
		archive(m_constParams, "constParams", "Constant parameters");
		archive(m_runtimeParams, "runtimeParams", "Runtime parameters");
	}

	if (queryTraits.CanHaveChildren())
	{
		archive(m_queryChildren, "queryChildren", "Query Children");
	}

	if (queryTraits.RequiresGenerator() || queryTraits.SupportsEvaluators())
	{
		CSelectedGeneratorContext selectedGeneratorContext(this, pContext);

		if (paramListContext.GetParamsChanged())
		{
			assert(archive.isInput());

			if (pContext)
			{
				m_generator.PrepareHelpers(*pContext);
				selectedGeneratorContext.SetGeneratorProcessed(true);
				m_evaluators.PrepareHelpers(*pContext);
			}
		}

		if (queryTraits.RequiresGenerator())
		{
			archive(m_generator, "generator", "Generator");
			selectedGeneratorContext.SetGeneratorProcessed(true);
		}

		if (selectedGeneratorContext.GetGeneratorChanged())
		{
			assert(archive.isInput());

			if (pContext)
			{
				m_evaluators.PrepareHelpers(*pContext);
			}
		}

		if (queryTraits.SupportsEvaluators())
		{
			archive(m_evaluators, "evaluators", "Evaluators");
		}
	}
}

void CQueryBlueprint::PrepareHelpers(CUqsDocSerializationContext& context)
{
	m_queryFactory.UpdateTraits(&context);

	CParametersListContext paramListContext(this, &context);

	m_constParams.PrepareHelpers(context);
	m_runtimeParams.PrepareHelpers(context);

	for (CQueryBlueprint& child : m_queryChildren)
	{
		child.PrepareHelpers(context);
	}

	CSelectedGeneratorContext selectedGeneratorContext(this, &context);
	m_generator.PrepareHelpers(context);

	selectedGeneratorContext.SetGeneratorProcessed(true);

	m_evaluators.PrepareHelpers(context);
}

void CQueryBlueprint::ClearErrors()
{
	assert(m_pErrorCollector);
	m_pErrorCollector->Clear();
	m_constParams.ClearErrors();
	m_runtimeParams.ClearErrors();
	m_generator.ClearErrors();
	m_evaluators.ClearErrors();

	for (CQueryBlueprint& child : m_queryChildren)
	{
		child.ClearErrors();
	}
}

const CGeneratorBlueprint& CQueryBlueprint::GetGenerator() const
{
	return m_generator;
}

//////////////////////////////////////////////////////////////////////////
// CParametersListContext
//////////////////////////////////////////////////////////////////////////

CParametersListContext::CParametersListContext(CQueryBlueprint* pOwner, CUqsDocSerializationContext* pSerializationContext)
	: m_pSerializationContext(pSerializationContext)
	, m_pOwner(pOwner)
	, m_bParamsChanged(false)
{
	if (pSerializationContext)
	{
		pSerializationContext->PushParametersListContext(this);
	}
}

CParametersListContext::~CParametersListContext()
{
	if (m_pSerializationContext)
	{
		m_pSerializationContext->PopParametersListContext(this);
	}
}

void CParametersListContext::SetParamsChanged()
{
	m_bParamsChanged = true;
	CCentralEventManager::QueryBlueprintRuntimeParamsChanged(CCentralEventManager::SQueryBlueprintRuntimeParamsChangedEventArgs(*m_pOwner));
}

void CParametersListContext::BuildFunctionListForAvailableParameters(
  const SItemTypeName& typeNameToFilter,
  const CUqsDocSerializationContext& context,
  const std::vector<CFunctionSerializationHelper::SFunction>& allGlobalParamFunctions,
  std::vector<CFunctionSerializationHelper::SFunction>& outParamFunctions)
{
	outParamFunctions.clear();
	assert(m_pOwner);
	if (!m_pOwner)
	{
		return;
	}

	const size_t constParamsCount = m_pOwner->m_constParams.m_params.size();
	const size_t runtimeParamsCount = m_pOwner->m_runtimeParams.m_params.size();

	const size_t count = constParamsCount + runtimeParamsCount;
	outParamFunctions.reserve(count);

	const bool bApplyFilter = !typeNameToFilter.Empty() && context.GetSettings().bFilterAvailableInputsByType;

	for (const CConstParamBlueprint::SConstParam& param : m_pOwner->m_constParams.m_params)
	{
		if (param.IsValid())
		{
			if (!bApplyFilter || (param.type == typeNameToFilter))
			{
				for (const auto& func : allGlobalParamFunctions)
				{
					assert(func.pFactory);
					if (func.returnType == param.type)
					{
						outParamFunctions.emplace_back(func);
						auto& outFunc = outParamFunctions.back();
						outFunc.param = param.name;
					}
				}
			}
		}
	}

	for (const CRuntimeParamBlueprint::SRuntimeParam& param : m_pOwner->m_runtimeParams.m_params)
	{
		if (param.IsValid())
		{
			if (!bApplyFilter || (param.type == typeNameToFilter))
			{
				for (const auto& func : allGlobalParamFunctions)
				{
					assert(func.pFactory);
					if (func.returnType == param.type)
					{
						outParamFunctions.emplace_back(func);
						auto& outFunc = outParamFunctions.back();
						outFunc.param = param.name;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CSelectedGeneratorContext
//////////////////////////////////////////////////////////////////////////

CSelectedGeneratorContext::CSelectedGeneratorContext(CQueryBlueprint* pOwner, CUqsDocSerializationContext* pSerializationContext)
	: m_pSerializationContext(pSerializationContext)
	, m_pOwner(pOwner)
	, m_bGeneratorChanged(false)
	, m_bGeneratorProcessed(false)
{
	if (pSerializationContext)
	{
		pSerializationContext->PushSelectedGeneratorContext(this);
	}
}

CSelectedGeneratorContext::~CSelectedGeneratorContext()
{
	if (m_pSerializationContext)
	{
		m_pSerializationContext->PopSelectedGeneratorContext(this);
	}
}

void CSelectedGeneratorContext::BuildFunctionListForAvailableGenerators(
  const SItemTypeName& typeNameToFilter,
  const CUqsDocSerializationContext& context,
  const std::vector<CFunctionSerializationHelper::SFunction>& allIteraedItemFunctions,
  std::vector<CFunctionSerializationHelper::SFunction>& outParamFunctions)
{
	outParamFunctions.clear();
	assert(m_pOwner);
	if (!m_pOwner)
	{
		return;
	}

	const bool bApplyFilter = !typeNameToFilter.Empty() && context.GetSettings().bFilterAvailableInputsByType;

	if (bApplyFilter)
	{
		SItemTypeName generatorType;
		const CryGUID& generatorGUID = m_pOwner->GetGenerator().GetGeneratorGUID();
		if (UQS::Client::IGeneratorFactory* pFactory = UQS::Core::IHubPlugin::GetHub().GetGeneratorFactoryDatabase().FindFactoryByGUID(generatorGUID))
		{
			generatorType = context.GetItemTypeNameFromType(pFactory->GetTypeOfItemsToGenerate());
		}

		if (generatorType != typeNameToFilter)
		{
			return;
		}
	}

	for (const auto& func : allIteraedItemFunctions)
	{
		if (!bApplyFilter || (func.returnType == typeNameToFilter))
		{
			outParamFunctions.emplace_back(func);
		}
	}
}

} // namespace UQSEditor
