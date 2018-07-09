// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StartupConsistencyChecker.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		void CStartupConsistencyChecker::CheckForConsistencyErrors()
		{
			//
			// clear previous errors so that the user can correct some of the problems while editing a query and then re-check for consistency
			//

			m_errors.clear();

			//
			// ensure no duplicate item factories (same item type)
			//

			{
				const ItemFactoryDatabase& itemFactoryDB = g_pHub->GetItemFactoryDatabase();

				for (size_t i1 = 0, n = itemFactoryDB.GetFactoryCount(); i1 < n; ++i1)
				{
					const Client::IItemFactory& itemFactory1 = itemFactoryDB.GetFactory(i1);
					const Shared::CTypeInfo& itemType1 = itemFactory1.GetItemType();
					int duplicateCount = 0;
					stack_string typeNamesForErrorMessage;

					for (size_t i2 = i1 + 1; i2 < n; ++i2)
					{
						const Client::IItemFactory& itemFactory2 = itemFactoryDB.GetFactory(i2);
						const Shared::CTypeInfo& itemType2 = itemFactory2.GetItemType();

						if (itemType1 == itemType2)
						{
							++duplicateCount;

							if (duplicateCount == 1)
							{
								typeNamesForErrorMessage = typeNamesForErrorMessage + "'" + itemFactory1.GetName() + "'";
							}

							typeNamesForErrorMessage = typeNamesForErrorMessage + ", '" + itemFactory2.GetName() + "'";
						}
					}

					if (duplicateCount > 0)
					{
						string error;
						error.Format("Duplicate ItemFactory that creates items of type %s (you registered %i such item-factories under these names: %s)", itemType1.name(), duplicateCount + 1, typeNamesForErrorMessage.c_str());
						m_errors.push_back(error);
					}
				}
			}

#if UQS_SCHEMATYC_SUPPORT
			//
			// ensure no duplicate GUIDs in item converters
			//

			{
				std::set<CryGUID> guidsInUse;

				const ItemFactoryDatabase& itemFactoryDB = g_pHub->GetItemFactoryDatabase();

				for (size_t iItemFactory = 0; iItemFactory < itemFactoryDB.GetFactoryCount(); ++iItemFactory)
				{
					const Client::IItemFactory& itemFactory = itemFactoryDB.GetFactory(iItemFactory);
					const char* szItemFactoryNameForErrorMessages = itemFactory.GetName();

					CheckItemConvertersConsistency(itemFactory.GetFromForeignTypeConverters(), szItemFactoryNameForErrorMessages, guidsInUse);
					CheckItemConvertersConsistency(itemFactory.GetToForeignTypeConverters(), szItemFactoryNameForErrorMessages, guidsInUse);
				}
			}
#endif // UQS_SCHEMATYC_SUPPORT

			//
			// ensure consistency of item factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetItemFactoryDatabase(), "ItemFactory");

			//
			// ensure ensure consistency of function factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetFunctionFactoryDatabase(), "FunctionFactory");

			//
			// ensure ensure consistency of generator factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetGeneratorFactoryDatabase(), "GeneratorFactory");

			//
			// ensure ensure consistency of instant-evaluator factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetInstantEvaluatorFactoryDatabase(), "InstantEvaluatorFactory");

			//
			// ensure ensure consistency of deferred-evaluator factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetDeferredEvaluatorFactoryDatabase(), "DeferredEvaluatorFactory");

			//
			// ensure ensure consistency of query factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetQueryFactoryDatabase(), "QueryFactory");

			//
			// ensure ensure consistency of score-transform factories
			//

			CheckFactoryDatabaseConsistency(g_pHub->GetScoreTransformFactoryDatabase(), "ScoreTransformFactory");

			//
			// ensure that all generators generate items for which item-factories are registered
			//

			{
				const GeneratorFactoryDatabase& generatorFactoryDB = g_pHub->GetGeneratorFactoryDatabase();

				for (size_t i = 0, n = generatorFactoryDB.GetFactoryCount(); i < n; ++i)
				{
					const Client::IGeneratorFactory& generatorFactory = generatorFactoryDB.GetFactory(i);
					const Shared::CTypeInfo& itemTypeToGenerate = generatorFactory.GetTypeOfItemsToGenerate();

					if (!g_pHub->GetUtils().FindItemFactoryByType(itemTypeToGenerate))
					{
						string error;
						error.Format("Generator '%s' wants to generate items of type '%s', but there is no corresponding item-factory for this type registered", generatorFactory.GetName(), itemTypeToGenerate.name());
						m_errors.push_back(error);
					}
				}
			}

			//
			// ensure that the type of shuttled items that certain generators expect have a corresponding item-factory registered
			//

			{
				const GeneratorFactoryDatabase& generatorFactoryDB = g_pHub->GetGeneratorFactoryDatabase();

				for (size_t i = 0, n = generatorFactoryDB.GetFactoryCount(); i < n; ++i)
				{
					const Client::IGeneratorFactory& generatorFactory = generatorFactoryDB.GetFactory(i);

					if (const Shared::CTypeInfo* pShuttledItemTypeToExpect = generatorFactory.GetTypeOfShuttledItemsToExpect())
					{
						if (!g_pHub->GetUtils().FindItemFactoryByType(*pShuttledItemTypeToExpect))
						{
							string error;
							error.Format("Generator '%s' expects shuttled items of type '%s', but there is no corresponding item-factory for this type registered", generatorFactory.GetName(), pShuttledItemTypeToExpect->name());
							m_errors.push_back(error);
						}
					}
				}
			}

			//
			// ensure that all kinds of return types of functions have a corresponding item-factory registered
			//

			{
				const FunctionFactoryDatabase& functionFactoryDB = g_pHub->GetFunctionFactoryDatabase();

				for (size_t i = 0, n = functionFactoryDB.GetFactoryCount(); i < n; ++i)
				{
					const Client::IFunctionFactory& functionFactory = functionFactoryDB.GetFactory(i);
					const Shared::CTypeInfo& returnTypeOfFunction = functionFactory.GetReturnType();

					if (!g_pHub->GetUtils().FindItemFactoryByType(returnTypeOfFunction))
					{
						string error;
						error.Format("Function '%s' returns a '%s', but there is no corresponding item-factory for this type registered", functionFactory.GetName(), returnTypeOfFunction.name());
						m_errors.push_back(error);
					}
				}
			}

			//
			// ensure that all input parameters have known item types, unique names and unique memory offsets
			//
			// -> 1. function params
			// -> 2. generator params
			// -> 3. instant-evaluator params
			// -> 4. deferred-evaluator params
			//

			{
				// 1. function params
				{
					const FunctionFactoryDatabase& functionFactoryDB = g_pHub->GetFunctionFactoryDatabase();

					for (size_t i = 0, n = functionFactoryDB.GetFactoryCount(); i < n; ++i)
					{
						const Client::IFunctionFactory& functionFactory = functionFactoryDB.GetFactory(i);
						stack_string messagePrefixForPossibleError;
						messagePrefixForPossibleError.Format("Function '%s'", functionFactory.GetName());
						CheckInputParametersConsistency(functionFactory.GetInputParameterRegistry(), messagePrefixForPossibleError.c_str());
					}
				}

				// 2. generator params
				{
					const GeneratorFactoryDatabase& generatorFactoryDB = g_pHub->GetGeneratorFactoryDatabase();

					for (size_t i = 0, n = generatorFactoryDB.GetFactoryCount(); i < n; ++i)
					{
						const Client::IGeneratorFactory& generatorFactory = generatorFactoryDB.GetFactory(i);
						stack_string messagePrefixForPossibleError;
						messagePrefixForPossibleError.Format("Generator '%s'", generatorFactory.GetName());
						CheckInputParametersConsistency(generatorFactory.GetInputParameterRegistry(), messagePrefixForPossibleError.c_str());
					}
				}

				// 3. instant-evaluator params
				{
					const IInstantEvaluatorFactoryDatabase& ieFactoryDB = g_pHub->GetInstantEvaluatorFactoryDatabase();

					for (size_t i = 0, n = ieFactoryDB.GetFactoryCount(); i < n; ++i)
					{
						const Client::IInstantEvaluatorFactory& ieFactory = ieFactoryDB.GetFactory(i);
						stack_string messagePrefixForPossibleError;
						messagePrefixForPossibleError.Format("Instant-Evaluator '%s'", ieFactory.GetName());
						CheckInputParametersConsistency(ieFactory.GetInputParameterRegistry(), messagePrefixForPossibleError.c_str());
					}
				}

				// 4. deferred-evaluator params
				{
					const IDeferredEvaluatorFactoryDatabase& deFactoryDB = g_pHub->GetDeferredEvaluatorFactoryDatabase();

					for (size_t i = 0, n = deFactoryDB.GetFactoryCount(); i < n; ++i)
					{
						const Client::IDeferredEvaluatorFactory& deFactory = deFactoryDB.GetFactory(i);
						stack_string messagePrefixForPossibleError;
						messagePrefixForPossibleError.Format("Deferred-Evaluator '%s'", deFactory.GetName());
						CheckInputParametersConsistency(deFactory.GetInputParameterRegistry(), messagePrefixForPossibleError.c_str());
					}
				}
			}

			// TODO: more consistency checks
		}

		template <class TFactoryDB>
		void CStartupConsistencyChecker::CheckFactoryDatabaseConsistency(const TFactoryDB& factoryDB, const char* szFactoryDatabaseNameForErrorMessages)
		{
			// ensure that all factories have a non-empty GUID
			{
				const std::set<string> factoryNamesWithEmptyGUIDs = factoryDB.GetFactoryNamesWithEmptyGUIDs();

				if (!factoryNamesWithEmptyGUIDs.empty())
				{
					for (const string& factoryName : factoryNamesWithEmptyGUIDs)
					{
						string error;
						error.Format("%s with name = '%s' has an empty GUID", szFactoryDatabaseNameForErrorMessages, factoryName.c_str());
						m_errors.push_back(error);
					}
				}
			}

			// check for duplicates by name
			{
				const std::map<string, int> duplicateFactoryNames = factoryDB.GetDuplicateFactoryNames();

				if (!duplicateFactoryNames.empty())
				{
					for (const auto& pair : duplicateFactoryNames)
					{
						string error;
						error.Format("Duplicate %s with name = '%s' (you tried to register a %s under this name %i times)", szFactoryDatabaseNameForErrorMessages, pair.first.c_str(), szFactoryDatabaseNameForErrorMessages, pair.second);
						m_errors.push_back(error);
					}
				}
			}

			// check for duplicates by GUID
			{
				const std::map<CryGUID, int> duplicateFactoryGUIDs = factoryDB.GetDuplicateFactoryGUIDs();

				if (!duplicateFactoryGUIDs.empty())
				{
					for (const auto& pair : duplicateFactoryGUIDs)
					{
						Shared::CUqsString guidAsString;
						Shared::Internal::CGUIDHelper::ToString(pair.first, guidAsString);
						string error;
						error.Format("Duplicate %s with GUID = %s (you tried to register a %s under this GUID %i times)", szFactoryDatabaseNameForErrorMessages, guidAsString.c_str(), szFactoryDatabaseNameForErrorMessages, pair.second);
						m_errors.push_back(error);
					}
				}
			}
		}

		void CStartupConsistencyChecker::CheckInputParametersConsistency(const Client::IInputParameterRegistry& registry, const char* szErrorMessagePrefix)
		{
			for (size_t i = 0, n = registry.GetParameterCount(); i < n; ++i)
			{
				const Client::IInputParameterRegistry::SParameterInfo& pi = registry.GetParameter(i);

				// check for known item type
				if (g_pHub->GetUtils().FindItemFactoryByType(pi.type) == nullptr)
				{
					string error;
					error.Format("%s: parameter '%s' is of type '%s' but there is no such item-factory registered", szErrorMessagePrefix, pi.szName, pi.type.name());
					m_errors.push_back(error);
				}

				// check for unique name (prevent copy & paste error)
				for (size_t k = i + 1; k < n; ++k)
				{
					const Client::IInputParameterRegistry::SParameterInfo& pi2 = registry.GetParameter(k);
					if (strcmp(pi2.szName, pi.szName) == 0)
					{
						string error;
						error.Format("%s: duplicate parameter name: '%s' (could be a copy & paste error)", szErrorMessagePrefix, pi2.szName);
						m_errors.push_back(error);
					}
				}

				// ensure that the parameter ID is non-empty
				if (pi.id.IsEmpty())
				{
					string error;
					error.Format("%s: parameter with name '%s' has an empty ID", szErrorMessagePrefix, pi.szName);
					m_errors.push_back(error);
				}

				// check for unique parameter ID (prevent copy & paste error)
				for (size_t k = i + 1; k < n; ++k)
				{
					const Client::IInputParameterRegistry::SParameterInfo& pi2 = registry.GetParameter(k);
					if (pi2.id == pi.id)
					{
						string error;
						char parameterIdAsString[5];
						pi2.id.ToString(parameterIdAsString);
						error.Format("%s: parameter with name '%s' has duplicated ID: '%s' (could be a copy & paste error)", szErrorMessagePrefix, pi2.szName, parameterIdAsString);
						m_errors.push_back(error);
					}
				}

				// check for unique memory offset (prevent copy & paste error)
				for (size_t k = i + 1; k < n; ++k)
				{
					const Client::IInputParameterRegistry::SParameterInfo& pi2 = registry.GetParameter(k);
					if (pi2.offset == pi.offset)
					{
						string error;
						error.Format("%s: duplicate memory offset for parameter '%s': the parameter '%s' is already using this offset (could be a copy & paste error)", szErrorMessagePrefix, pi2.szName, pi.szName);
						m_errors.push_back(error);
					}
				}
			}
		}

#if UQS_SCHEMATYC_SUPPORT
		void CStartupConsistencyChecker::CheckItemConvertersConsistency(const Client::IItemConverterCollection& itemConverters, const char* szItemFactoryNameForErrorMessages, std::set<CryGUID>& guidsInUse)
		{
			for (size_t i = 0, n = itemConverters.GetItemConverterCount(); i < n; ++i)
			{
				const Client::IItemConverter& converter = itemConverters.GetItemConverter(i);

				const CryGUID& guid = converter.GetGUID();
				auto res = guidsInUse.insert(guid);
				const bool bInsertSucceeded = res.second;

				if (!bInsertSucceeded)
				{
					// duplicate detected
					Shared::CUqsString guidAsString;
					Shared::Internal::CGUIDHelper::ToString(guid, guidAsString);
					string error;
					error.Format("ItemFactory '%s': clashing GUID in ItemConverter '%s -> %s': %s (used by a different ItemConverter already)", szItemFactoryNameForErrorMessages, converter.GetFromName(), converter.GetToName(), guidAsString.c_str());
					m_errors.push_back(error);
				}
			}
		}
#endif // UQS_SCHEMATYC_SUPPORT

		size_t CStartupConsistencyChecker::GetErrorCount() const
		{
			return m_errors.size();
		}

		const char* CStartupConsistencyChecker::GetError(size_t index) const
		{
			assert(index < m_errors.size());
			return m_errors[index].c_str();
		}

	}
}
