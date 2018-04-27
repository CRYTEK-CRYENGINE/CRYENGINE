// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

//#pragma  optimize("", off)

#if UQS_SCHEMATYC_SUPPORT

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CSchematycEnvDataTypeFinder
		//
		//===================================================================================

		CSchematycEnvDataTypeFinder::CSchematycEnvDataTypeFinder(const Schematyc::CTypeName& typeNameToSearchFor)
			: m_typeNameToSearchFor(typeNameToSearchFor)
			, m_pFoundDataType(nullptr)
		{}

		Schematyc::EVisitStatus CSchematycEnvDataTypeFinder::OnVisitEnvDataType(const Schematyc::IEnvDataType& dataType)
		{
			const Schematyc::CTypeName& typeName = dataType.GetDesc().GetName();

			if (typeName == m_typeNameToSearchFor)
			{
				m_pFoundDataType = &dataType;
				return Schematyc::EVisitStatus::Stop;
			}
			else
			{
				return Schematyc::EVisitStatus::Continue;
			}
		}

		const Schematyc::IEnvDataType* CSchematycEnvDataTypeFinder::FindEnvDataTypeByTypeName(const Schematyc::CTypeName& typeNameToSearchFor)
		{
			CSchematycEnvDataTypeFinder finder(typeNameToSearchFor);
			auto visitor = SCHEMATYC_MEMBER_DELEGATE(
				&CSchematycEnvDataTypeFinder::OnVisitEnvDataType, finder);
			gEnv->pSchematyc->GetEnvRegistry().VisitDataTypes(visitor);

			return finder.m_pFoundDataType;
		}

		//===================================================================================
		//
		// CSchematycEnvDataTypeCollector
		//
		//===================================================================================

		std::vector<const Schematyc::IEnvDataType*> CSchematycEnvDataTypeCollector::CollectAllEnvDataTypes()
		{
			CSchematycEnvDataTypeCollector collector;
			Schematyc::EnvDataTypeConstVisitor visitor = SCHEMATYC_MEMBER_DELEGATE(&CSchematycEnvDataTypeCollector::OnVisitEnvDataType,collector);
			gEnv->pSchematyc->GetEnvRegistry().VisitDataTypes(visitor);
			return std::move(collector.m_allEnvDataTypes);
		}

		Schematyc::EVisitStatus CSchematycEnvDataTypeCollector::OnVisitEnvDataType(const Schematyc::IEnvDataType& dataType)
		{
			m_allEnvDataTypes.push_back(&dataType);
			return Schematyc::EVisitStatus::Continue;
		}

		//===================================================================================
		//
		// CQueryBlueprintRuntimeParamGrabber
		//
		//===================================================================================

		void CQueryBlueprintRuntimeParamGrabber::GrabRequiredRuntimeParamsOfQueryBlueprint(const Core::IQueryBlueprint& queryBlueprint, std::map<string, Client::IItemFactory *>& out)
		{
			CQueryBlueprintRuntimeParamGrabber grabber;
			queryBlueprint.VisitRuntimeParams(grabber);
			out = std::move(grabber.m_requiredRuntimeParams);
		}

		void CQueryBlueprintRuntimeParamGrabber::OnRuntimeParamVisited(const char* szParamName, Client::IItemFactory& itemFactory)
		{
			m_requiredRuntimeParams[szParamName] = &itemFactory;
		}

		//===================================================================================
		//
		// CItemConverterLookup
		//
		//===================================================================================

		CItemConverterLookup::CItemConverterLookup(const std::vector<const Client::IItemConverter*>& itemConverters, ELookupMethod lookupMethod)
		{
			for (const Client::IItemConverter* pItemConverter : itemConverters)
			{
				const Shared::CTypeInfo& typeInfo = (lookupMethod == ELookupMethod::UseFromTypeAsKey) ? pItemConverter->GetFromItemType() : pItemConverter->GetToItemType();
				const Schematyc::CTypeName& key = typeInfo.GetSchematycTypeName();
				m_itemConverters.insert(std::make_pair(key, pItemConverter));
			}
		}

		const Client::IItemConverter* CItemConverterLookup::FindItemConverterByTypeInfo(const Shared::CTypeInfo& typeInfoToSearchFor) const
		{
			const Schematyc::CTypeName& typeNameToSearchFor = typeInfoToSearchFor.GetSchematycTypeName();
			VectorMap<Schematyc::CTypeName, const Client::IItemConverter*>::const_iterator it = m_itemConverters.find(typeNameToSearchFor);
			return (it == m_itemConverters.end()) ? nullptr : it->second;
		}

		//===================================================================================
		//
		// CSchematycUqsComponentEnvFunctionBase
		//
		//===================================================================================

		CSchematycUqsComponentEnvFunctionBase::CSchematycUqsComponentEnvFunctionBase(const Schematyc::SSourceFileInfo& sourceFileInfo)
			: CEnvElementBase(sourceFileInfo)
		{
		}

		CSchematycUqsComponentEnvFunctionBase::~CSchematycUqsComponentEnvFunctionBase()
		{
		}

		bool CSchematycUqsComponentEnvFunctionBase::IsValidScope(Schematyc::IEnvElement& scope) const
		{
			switch (scope.GetType())
			{
			case Schematyc::EEnvElementType::Module:
			case Schematyc::EEnvElementType::DataType:
			case Schematyc::EEnvElementType::Class:
			case Schematyc::EEnvElementType::Component:
			case Schematyc::EEnvElementType::Action:
				return true;

			default:
				return false;
			}
		}

		bool CSchematycUqsComponentEnvFunctionBase::Validate() const
		{
			return true;
		}

		Schematyc::EnvFunctionFlags CSchematycUqsComponentEnvFunctionBase::GetFunctionFlags() const
		{
			return Schematyc::EEnvFunctionFlags::Member;
		}

		const Schematyc::CCommonTypeDesc* CSchematycUqsComponentEnvFunctionBase::GetObjectTypeDesc() const
		{
			return &Schematyc::GetTypeDesc<CSchematycUqsComponent>();
		}

		uint32 CSchematycUqsComponentEnvFunctionBase::GetInputCount() const
		{
			return m_functionBinding.inputCount;
		}

		uint32 CSchematycUqsComponentEnvFunctionBase::GetInputId(uint32 inputIdx) const
		{
			return inputIdx < m_functionBinding.inputCount ? m_functionBinding.params[m_functionBinding.inputs[inputIdx]].id : Schematyc::InvalidId;
		}

		const char* CSchematycUqsComponentEnvFunctionBase::GetInputName(uint32 inputIdx) const
		{
			return inputIdx < m_functionBinding.inputCount ? m_functionBinding.params[m_functionBinding.inputs[inputIdx]].name.c_str() : "";
		}

		const char* CSchematycUqsComponentEnvFunctionBase::GetInputDescription(uint32 inputIdx) const
		{
			return "";
		}

		Schematyc::CAnyConstPtr CSchematycUqsComponentEnvFunctionBase::GetInputData(uint32 inputIdx) const
		{
			return inputIdx < m_functionBinding.inputCount ? m_functionBinding.params[m_functionBinding.inputs[inputIdx]].pData.get() : nullptr;
		}

		uint32 CSchematycUqsComponentEnvFunctionBase::GetOutputCount() const
		{
			return m_functionBinding.outputCount;
		}

		uint32 CSchematycUqsComponentEnvFunctionBase::GetOutputId(uint32 outputIdx) const
		{
			return outputIdx < m_functionBinding.outputCount ? m_functionBinding.params[m_functionBinding.outputs[outputIdx]].id : Schematyc::InvalidId;
		}

		const char* CSchematycUqsComponentEnvFunctionBase::GetOutputName(uint32 outputIdx) const
		{
			return outputIdx < m_functionBinding.outputCount ? m_functionBinding.params[m_functionBinding.outputs[outputIdx]].name.c_str() : "";
		}

		const char* CSchematycUqsComponentEnvFunctionBase::GetOutputDescription(uint32 outputIdx) const
		{
			return "";
		}

		Schematyc::CAnyConstPtr CSchematycUqsComponentEnvFunctionBase::GetOutputData(uint32 outputIdx) const
		{
			return outputIdx < m_functionBinding.outputCount ? m_functionBinding.params[m_functionBinding.outputs[outputIdx]].pData.get() : nullptr;
		}

		void CSchematycUqsComponentEnvFunctionBase::Execute(Schematyc::CRuntimeParamMap& params, void* pObject) const
		{
			assert(pObject != nullptr);
			ExecuteOnSchematycUqsComponent(params, *static_cast<CSchematycUqsComponent*>(pObject));
		}

		bool CSchematycUqsComponentEnvFunctionBase::AreAllParamtersRecognized() const
		{
			// check if all parameters have a default value by now (if they don't, then this particular port won't show up in the graph)
			for (uint32 i = 0; i < m_functionBinding.totalParamsCount; ++i)
			{
				if (!m_functionBinding.params[i].pData)
					return false;
			}
			return true;
		}

		void CSchematycUqsComponentEnvFunctionBase::AddInputParamByTypeName(const char* szParamName, uint32 id, const Schematyc::CTypeName& typeName)
		{
			AddInputParam(szParamName, id, &typeName, nullptr);
		}

		void CSchematycUqsComponentEnvFunctionBase::AddInputParamByTypeDesc(const char* szParamName, uint32 id, const Schematyc::CCommonTypeDesc& typeDesc)
		{
			AddInputParam(szParamName, id, nullptr, &typeDesc);
		}

		void CSchematycUqsComponentEnvFunctionBase::AddInputParam(const char* szParamName, uint32 id, const Schematyc::CTypeName* pTypeName, const Schematyc::CCommonTypeDesc* pTypeDesc)
		{
			assert(m_functionBinding.totalParamsCount < Schematyc::EnvFunctionUtils::MaxParams);

			m_functionBinding.params[m_functionBinding.totalParamsCount] = CreateParam(szParamName, id, pTypeName, pTypeDesc, Schematyc::EnvFunctionUtils::EParamFlags::BoundToInput);
			m_functionBinding.inputs[m_functionBinding.inputCount++] = m_functionBinding.totalParamsCount++;
		}

		void CSchematycUqsComponentEnvFunctionBase::AddOutputParamByTypeName(const char* szParamName, uint32 id, const Schematyc::CTypeName& typeName)
		{
			AddOutputParam(szParamName, id, &typeName, nullptr);
		}

		void CSchematycUqsComponentEnvFunctionBase::AddOutputParamByTypeDesc(const char* szParamName, uint32 id, const Schematyc::CCommonTypeDesc& typeDesc)
		{
			AddOutputParam(szParamName, id, nullptr, &typeDesc);
		}

		void CSchematycUqsComponentEnvFunctionBase::AddOutputParam(const char* szParamName, uint32 id, const Schematyc::CTypeName* pTypeName, const Schematyc::CCommonTypeDesc* pTypeDesc)
		{
			assert(m_functionBinding.totalParamsCount < Schematyc::EnvFunctionUtils::MaxParams);

			m_functionBinding.params[m_functionBinding.totalParamsCount] = CreateParam(szParamName, id, pTypeName, pTypeDesc, Schematyc::EnvFunctionUtils::EParamFlags::BoundToOutput);
			m_functionBinding.outputs[m_functionBinding.outputCount++] = m_functionBinding.totalParamsCount++;
		}

		Schematyc::CAnyConstRef CSchematycUqsComponentEnvFunctionBase::GetUntypedInputParam(const Schematyc::CRuntimeParamMap& params, uint32 inputIdx) const
		{
			assert(inputIdx < m_functionBinding.inputCount);

			const uint32 paramIdx = m_functionBinding.inputs[inputIdx];
			const SMyParamBinding& paramBinding = m_functionBinding.params[paramIdx];
			const Schematyc::CUniqueId id = Schematyc::CUniqueId::FromUInt32(paramBinding.id);
			Schematyc::CAnyConstPtr ptr = params.GetInput(id);

			// if the user didn't connect the input port then use the default value provided by paramBinding
			if (!ptr)
			{
				ptr = paramBinding.pData;
				assert(ptr);
			}

			return *ptr;
		}

		Schematyc::CAnyRef CSchematycUqsComponentEnvFunctionBase::GetUntypedOutputParam(Schematyc::CRuntimeParamMap& params, uint32 outputIdx) const
		{
			assert(outputIdx < m_functionBinding.outputCount);

			const uint32 paramIdx = m_functionBinding.outputs[outputIdx];
			const SMyParamBinding& paramBinding = m_functionBinding.params[paramIdx];
			const Schematyc::CUniqueId id = Schematyc::CUniqueId::FromUInt32(paramBinding.id);
			Schematyc::CAnyPtr ptr = params.GetOutput(id);

			// if the user didn't connect the input port then use the default value provided by paramBinding
			if (!ptr)
			{
				ptr = paramBinding.pData;
				assert(ptr);
			}

			return *ptr;
		}

		stack_string CSchematycUqsComponentEnvFunctionBase::PatchName(const char* szName)
		{
			stack_string patchedName = szName;
			for (char* c = patchedName.begin(); *c != '\0'; ++c)
			{
				if (c[0] == ':' && c[1] == ':')
				{
					*(++c) = ' ';
				}
			}
			return patchedName;
		}

		CSchematycUqsComponentEnvFunctionBase::SMyParamBinding CSchematycUqsComponentEnvFunctionBase::CreateParam(const char* szParamName, uint32 id, const Schematyc::CTypeName* pTypeName, const Schematyc::CCommonTypeDesc* pTypeDesc, Schematyc::EnvFunctionUtils::EParamFlags paramFlags) const
		{
			assert(pTypeName || pTypeDesc);

			// TODO: ensure each param ends up with a unique id

			SMyParamBinding p;

			p.flags = paramFlags;
			p.idx = m_functionBinding.totalParamsCount;
			p.id = id;
			p.name = szParamName;

			// figure out the type descriptor by looking it up in the IEnvRegistry
			if (!pTypeDesc)
			{
				const Schematyc::IEnvDataType* pEnvDataType = CSchematycEnvDataTypeFinder::FindEnvDataTypeByTypeName(*pTypeName);

				// Note: it could be that we didn't find the corresponding IEnvDataType.
				//       This could be the case if the item type has no corresponding type on the schematyc side (e. g. CAIActor might only be known to UQS),
				//       or (and let's hope this is *not* case) that some modules haven't had yet a chance to register their data types to schematyc.
				//       So, if it's missing, we simply miss out on the default value - let's see how that behaves at edit- and runtime (UPDATE: the behavior is that this particular input port will be missing in the UI graph)
				if (pEnvDataType)
				{
					// fine
					pTypeDesc = &pEnvDataType->GetDesc();
				}
			}

			// provide a default value for the parameter
			if (pTypeDesc)
			{
				p.pData = Schematyc::CAnyValue::MakeSharedDefault(*pTypeDesc);
			}

			return p;
		}

		//===================================================================================
		//
		// CSchematycUqsComponentEnvFunction_AddParam
		//
		//===================================================================================

		CSchematycUqsComponentEnvFunction_AddParam::CSchematycUqsComponentEnvFunction_AddParam(const Schematyc::SSourceFileInfo& sourceFileInfo, const Schematyc::IEnvDataType& envDataType, const std::vector<const Client::IItemConverter*>& itemConverters)
			: CSchematycUqsComponentEnvFunctionBase(sourceFileInfo)
			, m_itemConvertersFromSchematycToUqs(itemConverters, CItemConverterLookup::ELookupMethod::UseToTypeAsKey)
		{
			static const uint64 partialGUID = (uint64)0xcccd009ce746a061;

			SetName(stack_string().Format("AddParam [%s]", envDataType.GetName()));
			SetGUID(CryGUID::Construct(envDataType.GetGUID().hipart, partialGUID));

			// inputs
			AddInputParamByTypeDesc("Name", 'name', Schematyc::GetTypeDesc<Schematyc::CSharedString>());
			AddInputParamByTypeName("Value", 'valu', envDataType.GetDesc().GetName());
		}

		void CSchematycUqsComponentEnvFunction_AddParam::ExecuteOnSchematycUqsComponent(Schematyc::CRuntimeParamMap& params, CSchematycUqsComponent& schematycEntityUqsComponent) const
		{
			// inputs
			const Schematyc::CSharedString& name = GetTypedInputParam<Schematyc::CSharedString>(params, 0);
			Schematyc::CAnyConstRef value = GetUntypedInputParam(params, 1);

			Client::IItemFactory* pItemFactoryOfRuntimeParam = schematycEntityUqsComponent.GetItemFactoryOfRuntimeParam(name.c_str());
			if (!pItemFactoryOfRuntimeParam)
			{
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponentEnvFunction_AddParam::ExecuteOnSchematycUqsComponent: unrecognized runtime-parameter: '%s'", name.c_str());
				return;
			}

			Shared::IVariantDict& runtimeParams = schematycEntityUqsComponent.GetRuntimeParamsStorageOfUpcomingQuery();

			const Schematyc::CTypeName& sourceTypeName = value.GetTypeDesc().GetName();
			const Shared::CTypeInfo& targetTypeInfo = pItemFactoryOfRuntimeParam->GetItemType();
			const Schematyc::CTypeName& targetTypeName = targetTypeInfo.GetSchematycTypeName();

			if (targetTypeName == sourceTypeName)
			{
				runtimeParams.AddOrReplace(name.c_str(), *pItemFactoryOfRuntimeParam, value.GetValue());
			}
			else
			{
				if (const Client::IItemConverter* pItemConverter = m_itemConvertersFromSchematycToUqs.FindItemConverterByTypeInfo(targetTypeInfo))
				{
					assert(pItemConverter->GetToItemType() == targetTypeInfo);
					assert(pItemConverter->GetFromItemType().GetSchematycTypeName() == sourceTypeName);

					void* pConvertedItem = pItemFactoryOfRuntimeParam->CreateItems(1, Client::IItemFactory::EItemInitMode::UseDefaultConstructor);
					pItemConverter->ConvertItem(value.GetValue(), pConvertedItem);
					runtimeParams.AddOrReplace(name.c_str(), *pItemFactoryOfRuntimeParam, pConvertedItem);
					pItemFactoryOfRuntimeParam->DestroyItems(pConvertedItem);
				}
				else
				{
					SCHEMATYC_ENV_WARNING("CSchematycUqsComponentEnvFunction_AddParam::ExecuteOnSchematycUqsComponent: no item-converter found for converting from '%s' to '%s'", sourceTypeName.c_str(), pItemFactoryOfRuntimeParam->GetItemType().name());
				}
			}
		}

		//===================================================================================
		//
		// CSchematycUqsComponentEnvFunction_GetResult
		//
		//===================================================================================

		CSchematycUqsComponentEnvFunction_GetResult::CSchematycUqsComponentEnvFunction_GetResult(const Schematyc::SSourceFileInfo& sourceFileInfo, const Schematyc::IEnvDataType& envDataType, const std::vector<const Client::IItemConverter*>& itemConverters)
			: CSchematycUqsComponentEnvFunctionBase(sourceFileInfo)
			, m_itemConvertersFromUqsToSchematyc(itemConverters, CItemConverterLookup::ELookupMethod::UseFromTypeAsKey)
		{
			static const uint64 partialGUID = (uint64)0x5783cfdf2a2b9394;

			SetName(stack_string().Format("GetResult [%s]", envDataType.GetName()));
			SetGUID(CryGUID::Construct(envDataType.GetGUID().hipart, partialGUID));

			// inputs
			AddInputParamByTypeDesc("QueryID", 'quid', Schematyc::GetTypeDesc<CSchematycUqsComponent::SQueryIdWrapper>());
			AddInputParamByTypeDesc("ResultIdx", 'ridx', Schematyc::GetTypeDesc<int32>());

			// outputs
			AddOutputParamByTypeName("Item", 'item', envDataType.GetDesc().GetName());
			AddOutputParamByTypeDesc("Score", 'scor', Schematyc::GetTypeDesc<float>());
		}

		void CSchematycUqsComponentEnvFunction_GetResult::ExecuteOnSchematycUqsComponent(Schematyc::CRuntimeParamMap& params, CSchematycUqsComponent& schematycEntityUqsComponent) const
		{
			// inputs
			const CSchematycUqsComponent::SQueryIdWrapper& queryId = GetTypedInputParam<CSchematycUqsComponent::SQueryIdWrapper>(params, 0);
			const int32 resultIdx = GetTypedInputParam<int32>(params, 1);

			// outputs
			Schematyc::CAnyRef item = GetUntypedOutputParam(params, 0);
			float& score = GetTypedOutputParam<float>(params, 1);

			const Core::IQueryResultSet* pQueryResultSet = schematycEntityUqsComponent.GetQueryResultSet(queryId.queryId);

			if (!pQueryResultSet)
			{
				Shared::CUqsString queryIdAsString;
				queryId.queryId.ToString(queryIdAsString);
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponentEnvFunction_GetResult::ExecuteOnSchematycUqsComponent: unknown queryID: %s", queryIdAsString.c_str());
				return;
			}

			if (resultIdx < 0 || resultIdx >= (int32)pQueryResultSet->GetResultCount())
			{
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponentEnvFunction_GetResult::ExecuteOnSchematycUqsComponent: resultIdx out of range (should be [0..%i])", (int)pQueryResultSet->GetResultCount() - 1);
				return;
			}

			Client::IItemFactory& itemFactoryOfResultSet = pQueryResultSet->GetItemFactory();
			const Shared::CTypeInfo& sourceTypeInfo = itemFactoryOfResultSet.GetItemType();
			const Schematyc::CTypeName& sourceTypeName = sourceTypeInfo.GetSchematycTypeName();
			const Schematyc::CTypeName& targetTypeName = item.GetTypeDesc().GetName();

			const Core::IQueryResultSet::SResultSetEntry entry = pQueryResultSet->GetResult((size_t)resultIdx);

			// write outputs
			if (targetTypeName == sourceTypeName)
			{
				itemFactoryOfResultSet.CopyItem(item.GetValue(), entry.pItem);
				score = entry.score;
			}
			else
			{
				if (const Client::IItemConverter* pItemConverter = m_itemConvertersFromUqsToSchematyc.FindItemConverterByTypeInfo(sourceTypeInfo))
				{
					assert(pItemConverter->GetFromItemType() == sourceTypeInfo);
					assert(pItemConverter->GetToItemType().GetSchematycTypeName() == targetTypeName);

					pItemConverter->ConvertItem(entry.pItem, item.GetValue());
					score = entry.score;
				}
				else
				{
					SCHEMATYC_ENV_WARNING("CSchematycUqsComponentEnvFunction_GetResult::ExecuteOnSchematycUqsComponent: no item-converter found for converting from '%s' to '%s'", sourceTypeName.c_str(), targetTypeName.c_str());
				}
			}
		}

		//===================================================================================
		//
		// CSchematycUqsComponentEnvFunctionGeneratorBase
		//
		//===================================================================================

		void CSchematycUqsComponentEnvFunctionGeneratorBase::GenerateFunctions(Schematyc::CEnvRegistrationScope& scope) const
		{
			const ItemFactoryDatabase& itemFactoryDB = g_pHub->GetItemFactoryDatabase();

			//
			// collect all IEnvDataTypes that exist in Schematyc
			//

			const std::vector<const Schematyc::IEnvDataType*> allEnvDataTypes = CSchematycEnvDataTypeCollector::CollectAllEnvDataTypes();

			//
			// build a mapping from IEnvDataType to IItemFactory
			//

			std::map<const Schematyc::IEnvDataType*, Client::IItemFactory*> envDataType2itemFactory;

			for (size_t i = 0, n = itemFactoryDB.GetFactoryCount(); i < n; ++i)
			{
				Client::IItemFactory& itemFactory = itemFactoryDB.GetFactory(i);
				const Schematyc::CTypeName& schematycTypeName = itemFactory.GetItemType().GetSchematycTypeName();
				auto it = std::find_if(
					allEnvDataTypes.cbegin(),
					allEnvDataTypes.cend(),
					[schematycTypeName](const Schematyc::IEnvDataType* pEnvDataType) { return pEnvDataType->GetDesc().GetName() == schematycTypeName; });

				if (it != allEnvDataTypes.cend())
				{
					envDataType2itemFactory[*it] = &itemFactory;
				}
			}

			//
			// collect all IEnvDataTypes that somehow relate to UQS item types (either directly through an IItemFactory or through at least 1 IItemConverter)
			//

			struct SEnvDataTypeMapEntry
			{
				std::vector<const Client::IItemConverter*> itemConverters;
			};

			std::map<const Schematyc::IEnvDataType*, SEnvDataTypeMapEntry> envDataTypeMap;

			for (const Schematyc::IEnvDataType* pEnvDataType : allEnvDataTypes)
			{
				//
				// direct match? (i. e. there's an IEnvDataType on the Schematyc side that corresponds to an IItemFactory on the UQS side)
				//

				if (envDataType2itemFactory.find(pEnvDataType) != envDataType2itemFactory.cend())
				{
					// yep (create a new entry that doesn't contain any optional item-converters yet)
					envDataTypeMap[pEnvDataType];
				}

				//
				// check for item converters: collect all that convert from/to the IEnvDataType to/from some item type on the UQS side
				//

				for (size_t i = 0; i < itemFactoryDB.GetFactoryCount(); ++i)
				{
					const Client::IItemFactory& itemFactory = itemFactoryDB.GetFactory(i);
					const Client::IItemConverterCollection& itemConverters = GetItemConverterCollectionFromItemFactory(itemFactory);

					for (size_t k = 0; k < itemConverters.GetItemConverterCount(); ++k)
					{
						const Client::IItemConverter& itemConverter = itemConverters.GetItemConverter(k);

						if (MatchItemConverterAgainstEnvDataType(itemConverter, *pEnvDataType))
						{
							envDataTypeMap[pEnvDataType].itemConverters.push_back(&itemConverter);
						}
					}
				}
			}

			//
			// now generate all Schematyc functions
			//

			for (const auto& pair : envDataTypeMap)
			{
				const Schematyc::IEnvDataType* pEnvDataType = pair.first;
				const SEnvDataTypeMapEntry& info = pair.second;
				GenerateSchematycFunction(scope, *pEnvDataType, info.itemConverters);
			}
		}

		//===================================================================================
		//
		// CSchematycUqsComponentEnvFunctionGenerator_AddParam
		//
		//===================================================================================

		const Client::IItemConverterCollection& CSchematycUqsComponentEnvFunctionGenerator_AddParam::GetItemConverterCollectionFromItemFactory(const Client::IItemFactory& itemFactory) const
		{
			return itemFactory.GetFromForeignTypeConverters();
		}

		bool CSchematycUqsComponentEnvFunctionGenerator_AddParam::MatchItemConverterAgainstEnvDataType(const Client::IItemConverter& itemConverter, const Schematyc::IEnvDataType& envDataType) const
		{
			return itemConverter.GetFromItemType().GetSchematycTypeName() == envDataType.GetDesc().GetName();
		}

		void CSchematycUqsComponentEnvFunctionGenerator_AddParam::GenerateSchematycFunction(Schematyc::CEnvRegistrationScope& scope, const Schematyc::IEnvDataType& envDataType, const std::vector<const Client::IItemConverter*>& itemConverters) const
		{
			std::shared_ptr<CSchematycUqsComponentEnvFunction_AddParam> pFunc(new CSchematycUqsComponentEnvFunction_AddParam(SCHEMATYC_SOURCE_FILE_INFO, envDataType, itemConverters));

			if (pFunc->AreAllParamtersRecognized())
			{
				scope.Register(pFunc);
			}
		}

		//===================================================================================
		//
		// CSchematycUqsComponentEnvFunctionGenerator_GetResult
		//
		//===================================================================================

		const Client::IItemConverterCollection& CSchematycUqsComponentEnvFunctionGenerator_GetResult::GetItemConverterCollectionFromItemFactory(const Client::IItemFactory& itemFactory) const
		{
			return itemFactory.GetToForeignTypeConverters();
		}

		bool CSchematycUqsComponentEnvFunctionGenerator_GetResult::MatchItemConverterAgainstEnvDataType(const Client::IItemConverter& itemConverter, const Schematyc::IEnvDataType& envDataType) const
		{
			return itemConverter.GetToItemType().GetSchematycTypeName() == envDataType.GetDesc().GetName();
		}

		void CSchematycUqsComponentEnvFunctionGenerator_GetResult::GenerateSchematycFunction(Schematyc::CEnvRegistrationScope& scope, const Schematyc::IEnvDataType& envDataType, const std::vector<const Client::IItemConverter*>& itemConverters) const
		{
			std::shared_ptr<CSchematycUqsComponentEnvFunction_GetResult> pFunc(new CSchematycUqsComponentEnvFunction_GetResult(SCHEMATYC_SOURCE_FILE_INFO, envDataType, itemConverters));

			if (pFunc->AreAllParamtersRecognized())
			{
				scope.Register(pFunc);
			}
		}

		//===================================================================================
		//
		// CSchematycUqsComponent::SQueryIdWrapper
		//
		//===================================================================================

		CSchematycUqsComponent::SQueryIdWrapper::SQueryIdWrapper()
			: queryId(CQueryID::CreateInvalid())
		{
		}

		CSchematycUqsComponent::SQueryIdWrapper::SQueryIdWrapper(const CQueryID& id)
			: queryId(id)
		{
		}

		bool CSchematycUqsComponent::SQueryIdWrapper::operator==(const SQueryIdWrapper& other) const
		{
			return this->queryId == other.queryId;
		}

		bool CSchematycUqsComponent::SQueryIdWrapper::Serialize(Serialization::IArchive& archive, const char* szName, const char* szLabel)
		{
			return archive(this->queryId, szName, szLabel);
		}

		void CSchematycUqsComponent::SQueryIdWrapper::ReflectType(Schematyc::CTypeDesc<SQueryIdWrapper>& desc)
		{
			desc.SetGUID("b0c4a5a2-8a6e-4a4f-b7c1-90bfb9d1898b"_cry_guid);
			desc.SetLabel("QueryID");
			desc.SetDescription("Handle of a running query.");
			desc.SetToStringOperator<&ToString>();
		}

		void CSchematycUqsComponent::SQueryIdWrapper::ToString(Schematyc::IString& output, const SQueryIdWrapper& input)
		{
			Shared::CUqsString tmp;
			input.queryId.ToString(tmp);
			output.assign(tmp.c_str());
		}

		void CSchematycUqsComponent::SQueryIdWrapper::Register(Schematyc::CEnvRegistrationScope& scope)
		{
			scope.Register(SCHEMATYC_MAKE_ENV_DATA_TYPE(SQueryIdWrapper));

			{
				auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&SchematycFunction_Equal, "ae7a5b0a-e3f4-4cd7-b01a-bbad7c7fe11e"_cry_guid, "Equal");
				pFunction->BindOutput(0, 'resu', "Result");
				pFunction->BindInput(1, 'qid1', "QueryID 1");
				pFunction->BindInput(2, 'qid2', "QueryID 2");
				scope.Register(pFunction);
			}
		}

		bool CSchematycUqsComponent::SQueryIdWrapper::SchematycFunction_Equal(const SQueryIdWrapper& queryId1, const SQueryIdWrapper& queryId2)
		{
			return queryId1.queryId == queryId2.queryId;
		}

		//===================================================================================
		//
		// SQueryFinishedSignal
		//
		//===================================================================================

		CSchematycUqsComponent::SQueryFinishedSignal::SQueryFinishedSignal()
			: queryId(CQueryID::CreateInvalid())
			, resultCount(0)
		{
			// nothing
		}

		CSchematycUqsComponent::SQueryFinishedSignal::SQueryFinishedSignal(const SQueryIdWrapper& _queryId, int32 _resultCount)
			: queryId(_queryId)
			, resultCount(_resultCount)
		{
			// nothing
		}

		void CSchematycUqsComponent::SQueryFinishedSignal::ReflectType(Schematyc::CTypeDesc<SQueryFinishedSignal>& desc)
		{
			desc.SetGUID("babd0a8b-2f84-4019-b6f5-0758ac62ec56"_cry_guid);
			desc.SetLabel("QueryFinished");
			desc.SetDescription("Sent when a query finishes without having run into an exception.");
			desc.AddMember(&SQueryFinishedSignal::queryId, 'quid', "queryId", "QueryID", nullptr, SQueryIdWrapper());
			desc.AddMember(&SQueryFinishedSignal::resultCount, 'resc', "resultCount", "ResultCount", nullptr, 0);
		}

		//===================================================================================
		//
		// SQueryExceptionSignal
		//
		//===================================================================================

		CSchematycUqsComponent::SQueryExceptionSignal::SQueryExceptionSignal()
			: queryId(CQueryID::CreateInvalid())
			, exceptionMessage()
		{
			// nothing
		}

		CSchematycUqsComponent::SQueryExceptionSignal::SQueryExceptionSignal(const SQueryIdWrapper& _queryId, const Schematyc::CSharedString& _exceptionMessage)
			: queryId(_queryId)
			, exceptionMessage(_exceptionMessage)
		{
			// nothing
		}

		void CSchematycUqsComponent::SQueryExceptionSignal::ReflectType(Schematyc::CTypeDesc<SQueryExceptionSignal>& desc)
		{
			desc.SetGUID("5d1ea803-63bb-436d-8044-252a150b916f"_cry_guid);
			desc.SetLabel("QueryException");
			desc.SetDescription("Sent when a query runs into an excpetion before finishing.");
			desc.AddMember(&SQueryExceptionSignal::queryId, 'quid', "queryId", "QueryID", nullptr, SQueryIdWrapper());
			desc.AddMember(&SQueryExceptionSignal::exceptionMessage, 'exms', "exceptionMessage", "ExceptionMessage", nullptr, "");
		}

		//===================================================================================
		//
		// CSchematycUqsComponent::SUpcomingQueryInfo
		//
		//===================================================================================

		void CSchematycUqsComponent::SUpcomingQueryInfo::Clear()
		{
			this->queryBlueprintID = CQueryBlueprintID();
			this->querierName.clear();
			this->requiredRuntimeParams.clear();
			this->runtimeParams.Clear();
		}

		//===================================================================================
		//
		// CSchematycUqsComponent
		//
		//===================================================================================

		CSchematycUqsComponent::CSchematycUqsComponent()
		{
		}

		CSchematycUqsComponent::~CSchematycUqsComponent()
		{
			CancelRunningQueriesAndClearFinishedOnes();
		}

		void CSchematycUqsComponent::Initialize()
		{
			CancelRunningQueriesAndClearFinishedOnes();

			Schematyc::SUpdateParams updateParams(SCHEMATYC_MEMBER_DELEGATE(&CSchematycUqsComponent::Update, *this), m_connectionScope);
			gEnv->pSchematyc->GetUpdateScheduler().Connect(updateParams);
		}

		void CSchematycUqsComponent::OnShutDown()
		{
			CancelRunningQueriesAndClearFinishedOnes();
		}

		Client::IItemFactory* CSchematycUqsComponent::GetItemFactoryOfRuntimeParam(const char* szRuntimeParamName) const
		{
			auto it = m_upcomingQueryInfo.requiredRuntimeParams.find(szRuntimeParamName);
			return (it == m_upcomingQueryInfo.requiredRuntimeParams.cend()) ? nullptr : it->second;
		}

		Shared::CVariantDict& CSchematycUqsComponent::GetRuntimeParamsStorageOfUpcomingQuery()
		{
			return m_upcomingQueryInfo.runtimeParams;
		}

		const IQueryResultSet* CSchematycUqsComponent::GetQueryResultSet(const CQueryID& queryID) const
		{
			auto it = m_succeededQueries.find(queryID);
			return (it == m_succeededQueries.cend()) ? nullptr : it->second.pQueryResultSet.get();
		}

		void CSchematycUqsComponent::ReflectType(Schematyc::CTypeDesc<CSchematycUqsComponent>& desc)
		{
			desc.SetGUID("1d00010d-5b91-4b56-8cdd-52c8d966acb8"_cry_guid);
			desc.SetLabel("UQS");
			desc.SetDescription("Universal Query System component");
			//desc.SetIcon("icons:schematyc/entity_audio_component.ico");
			desc.SetComponentFlags({ EEntityComponentFlags::Singleton });
		}

		void CSchematycUqsComponent::Register(Schematyc::IEnvRegistrar& registrar)
		{
			Schematyc::CEnvRegistrationScope entityScope = registrar.Scope(IEntity::GetEntityScopeGUID());
			{
				Schematyc::CEnvRegistrationScope componentScope = entityScope.Register(SCHEMATYC_MAKE_ENV_COMPONENT(CSchematycUqsComponent));
				{
					//
					// data types
					//

					SQueryIdWrapper::Register(componentScope);

					//
					// functions
					//

					{

						//
						// generate "AddParam" and "GetResult" functions
						//

						CSchematycUqsComponentEnvFunctionGenerator_AddParam().GenerateFunctions(componentScope);
						CSchematycUqsComponentEnvFunctionGenerator_GetResult().GenerateFunctions(componentScope);

						//
						// BeginQuery()
						//

						{
							std::shared_ptr<Schematyc::CEnvFunction> pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycUqsComponent::SchematycFunction_BeginQuery, "5ebcdda0-31f5-4e2d-afef-63fc0368cf9c"_cry_guid, "BeginQuery");
							pFunction->SetDescription("Prepares for starting a query");
							pFunction->BindInput(1, 'qbpn', "QueryBlueprint", "Name of the query blueprint that shall be started.");
							pFunction->BindInput(2, 'qunm', "QuerierName", "For debugging purpose: name of the querier to identify more easily in the query history.");
							componentScope.Register(pFunction);
						}

						//
						// StartQuery()
						//

						{
							std::shared_ptr<Schematyc::CEnvFunction> pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CSchematycUqsComponent::SchematycFunction_StartQuery, "f7205181-41cb-478d-a773-0d33a3805a99"_cry_guid, "StartQuery");
							pFunction->SetDescription("Starts the query that was previously prepared");
							pFunction->BindOutput(1, 'quid', "QueryID");
							componentScope.Register(pFunction);
						}
					}

					//
					// signals
					//

					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SQueryFinishedSignal));
					componentScope.Register(SCHEMATYC_MAKE_ENV_SIGNAL(SQueryExceptionSignal));

				}
			}
		}

		void CSchematycUqsComponent::SchematycFunction_BeginQuery(const Schematyc::CSharedString& queryBlueprintName, const Schematyc::CSharedString& querierName)
		{
			m_upcomingQueryInfo.Clear();

			IHub* pHub = IHubPlugin::GetHubPtr();

			if (!pHub)
			{
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponent::SchematycFunction_BeginQuery: UQS system is not available");
				return;
			}

			const CQueryBlueprintID queryBlueprintID = pHub->GetQueryBlueprintLibrary().FindQueryBlueprintIDByName(queryBlueprintName.c_str());

			if (!queryBlueprintID.IsOrHasBeenValid())
			{
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponent::SchematycFunction_BeginQuery: unknown or broken query blueprint: '%s'", queryBlueprintName.c_str());
				return;
			}

			// grab all required runtime-parameters
			if (const IQueryBlueprint* pQueryBlueprint = pHub->GetQueryBlueprintLibrary().GetQueryBlueprintByID(queryBlueprintID))
			{
				CQueryBlueprintRuntimeParamGrabber::GrabRequiredRuntimeParamsOfQueryBlueprint(*pQueryBlueprint, m_upcomingQueryInfo.requiredRuntimeParams);
			}

			m_upcomingQueryInfo.queryBlueprintID = queryBlueprintID;
			m_upcomingQueryInfo.querierName = querierName;
		}

		void CSchematycUqsComponent::SchematycFunction_StartQuery(SQueryIdWrapper& outQueryId)
		{
			IHub* pHub = IHubPlugin::GetHubPtr();

			if (!pHub)
			{
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponent::SchematycFunction_StartQuery: UQS system is not available");
				return;
			}

			const Client::SQueryRequest request(m_upcomingQueryInfo.queryBlueprintID, m_upcomingQueryInfo.runtimeParams, m_upcomingQueryInfo.querierName.c_str(), functor(*this, &CSchematycUqsComponent::OnQueryResult));
			Shared::CUqsString errorMessage;

			const CQueryID queryID = pHub->GetQueryManager().StartQuery(request, errorMessage);

			if (queryID.IsValid())
			{
				m_runningQueries.insert(queryID);
				outQueryId.queryId = queryID;
			}
			else
			{
				// FIXME: should we maybe propagate this through the SQueryExceptionSignal ? (but then again, that signal needs a valid QueryID)
				SCHEMATYC_ENV_WARNING("CSchematycUqsComponent::SchematycFunction_StartQuery: unable to start query '%s', error: %s", m_upcomingQueryInfo.queryBlueprintID.GetQueryBlueprintName(), errorMessage.c_str());
			}

			m_upcomingQueryInfo.Clear();
		}

		void CSchematycUqsComponent::Update(const Schematyc::SUpdateContext& updateContext)
		{
			FlushFinishedQueries();
		}

		void CSchematycUqsComponent::CancelRunningQueriesAndClearFinishedOnes()
		{
			if (IHub* pHub = IHubPlugin::GetHubPtr())
			{
				IQueryManager& queryManager = pHub->GetQueryManager();

				for (const CQueryID& queryID : m_runningQueries)
				{
					queryManager.CancelQuery(queryID);
				}
			}

			m_runningQueries.clear();
			m_succeededQueries.clear();
			m_failedQueries.clear();
		}

		void CSchematycUqsComponent::FlushFinishedQueries()
		{
			// fire signals for all queries that finished without exception
			for (const auto& pair : m_succeededQueries)
			{
				const SQueryIdWrapper queryID(pair.first);
				const int32 resultCount = (int32)pair.second.pQueryResultSet->GetResultCount();
				const SQueryFinishedSignal signal(queryID, resultCount);
				if (GetEntity()->GetSchematycObject())
					GetEntity()->GetSchematycObject()->ProcessSignal(signal, GetGUID());
			}

			// fire signals for all queries that finished with an exception
			for (const auto& pair : m_failedQueries)
			{
				const SQueryIdWrapper queryID(pair.first);
				const Schematyc::CSharedString exceptionMessage(pair.second.c_str());
				const SQueryExceptionSignal signal(queryID, exceptionMessage);
				if (GetEntity()->GetSchematycObject())
					GetEntity()->GetSchematycObject()->ProcessSignal(signal, GetGUID());
			}

			m_succeededQueries.clear();
			m_failedQueries.clear();
		}

		void CSchematycUqsComponent::OnQueryResult(const SQueryResult& queryResult)
		{
			m_runningQueries.erase(queryResult.queryID);

			switch (queryResult.status)
			{
			case SQueryResult::Success:
				m_succeededQueries[queryResult.queryID].pQueryResultSet = std::move(queryResult.pResultSet);
				break;

			case SQueryResult::ExceptionOccurred:
				m_failedQueries[queryResult.queryID] = queryResult.szError;
				break;

			case SQueryResult::CanceledByHubTearDown:
				// nothing (?)
				break;

			default:
				assert(0);
			}
		}

	}
}

#endif // UQS_SCHEMATYC_SUPPORT
