// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InputBlueprint.h"
#include "ItemSerializationSupport.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualInputBlueprint
		//
		//===================================================================================

		CTextualInputBlueprint::CTextualInputBlueprint()
			: m_paramID(Client::CInputParameterID::CreateEmpty())
			, m_funcGUID(CryGUID::Null())
			, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
		{
		}

		CTextualInputBlueprint::CTextualInputBlueprint(const char* szParamName, const Client::CInputParameterID& paramID, const char* szFuncName, const CryGUID& funcGUID, const char* szFuncReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution)
			: m_paramName(szParamName)
			, m_paramID(paramID)
			, m_funcName(szFuncName)
			, m_funcGUID(funcGUID)
			, m_funcReturnValueLiteral(szFuncReturnValueLiteral)
			, m_bAddReturnValueToDebugRenderWorldUponExecution(bAddReturnValueToDebugRenderWorldUponExecution)
		{
		}

		CTextualInputBlueprint::~CTextualInputBlueprint()
		{
			for(CTextualInputBlueprint* pChild : m_children)
			{
				delete pChild;
			}
		}

		const char* CTextualInputBlueprint::GetParamName() const
		{
			return m_paramName.c_str();
		}

		const Client::CInputParameterID& CTextualInputBlueprint::GetParamID() const
		{
			return m_paramID;
		}

		const char* CTextualInputBlueprint::GetFuncName() const
		{
			return m_funcName.c_str();
		}

		const CryGUID& CTextualInputBlueprint::GetFuncGUID() const
		{
			return m_funcGUID;
		}

		const char* CTextualInputBlueprint::GetFuncReturnValueLiteral() const
		{
			return m_funcReturnValueLiteral.c_str();
		}

		bool CTextualInputBlueprint::GetAddReturnValueToDebugRenderWorldUponExecution() const
		{
			return m_bAddReturnValueToDebugRenderWorldUponExecution;
		}

		void CTextualInputBlueprint::SetParamName(const char* szParamName)
		{
			m_paramName = szParamName;
		}

		void CTextualInputBlueprint::SetParamID(const Client::CInputParameterID& paramID)
		{
			m_paramID = paramID;
		}

		void CTextualInputBlueprint::SetFuncName(const char* szFuncName)
		{
			m_funcName = szFuncName;
		}

		void CTextualInputBlueprint::SetFuncGUID(const CryGUID& funcGUID)
		{
			m_funcGUID = funcGUID;
		}

		void CTextualInputBlueprint::SetFuncReturnValueLiteral(const char* szValue)
		{
			m_funcReturnValueLiteral = szValue;
		}

		void CTextualInputBlueprint::SetAddReturnValueToDebugRenderWorldUponExecution(bool bAddReturnValueToDebugRenderWorldUponExecution)
		{
			m_bAddReturnValueToDebugRenderWorldUponExecution = bAddReturnValueToDebugRenderWorldUponExecution;
		}

		ITextualInputBlueprint& CTextualInputBlueprint::AddChild(const char* szParamName, const Client::CInputParameterID& paramID, const char* szFuncName, const CryGUID& funcGUID, const char* szFuncReturnValueLiteral, bool bAddReturnValueToDebugRenderWorldUponExecution)
		{
			CTextualInputBlueprint* pChild = new CTextualInputBlueprint(szParamName, paramID, szFuncName, funcGUID, szFuncReturnValueLiteral, bAddReturnValueToDebugRenderWorldUponExecution);
			m_children.push_back(pChild);
			return *pChild;
		}

		size_t CTextualInputBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		const ITextualInputBlueprint& CTextualInputBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return *m_children[index];
		}

		const ITextualInputBlueprint* CTextualInputBlueprint::FindChildByParamName(const char* szParamName) const
		{
			for(const CTextualInputBlueprint* pChild : m_children)
			{
				if(strcmp(pChild->m_paramName.c_str(), szParamName) == 0)
					return pChild;
			}
			return nullptr;
		}

		const ITextualInputBlueprint* CTextualInputBlueprint::FindChildByParamID(const Client::CInputParameterID& paramID) const
		{
			for (const CTextualInputBlueprint* pChild : m_children)
			{
				if (pChild->m_paramID == paramID)
					return pChild;
			}
			return nullptr;
		}

		void CTextualInputBlueprint::SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector)
		{
			m_pSyntaxErrorCollector = std::move(pSyntaxErrorCollector);
		}

		DataSource::ISyntaxErrorCollector* CTextualInputBlueprint::GetSyntaxErrorCollector() const
		{
			return m_pSyntaxErrorCollector.get();
		}

		//===================================================================================
		//
		// CInputBlueprint
		//
		//===================================================================================

		CInputBlueprint::CInputBlueprint()
			: m_pFunctionFactory(nullptr)
			, m_bAddReturnValueToDebugRenderWorldUponExecution(false)
		{
		}

		CInputBlueprint::~CInputBlueprint()
		{
			for(CInputBlueprint* pChild : m_children)
			{
				delete pChild;
			}
		}

		bool CInputBlueprint::Resolve(const ITextualInputBlueprint& sourceParent, const Client::IInputParameterRegistry& inputParamsReg, const CQueryBlueprint& queryBlueprintForGlobalParamChecking, bool bResolvingForAGenerator)
		{
			const size_t numParamsExpected = inputParamsReg.GetParameterCount();
			const size_t numParamsProvided = sourceParent.GetChildCount();

			//
			// ensure the correct number of parameters has been provided
			//

			if (numParamsProvided != numParamsExpected)
			{
				if (DataSource::ISyntaxErrorCollector* pSE = sourceParent.GetSyntaxErrorCollector())
				{
					pSE->AddErrorMessage("Incorrect number of parameters provided: expected %i, got %i", (int)numParamsExpected, (int)numParamsProvided);
				}
				return false;
			}

			//
			// parse one parameter after another
			//

			for (size_t i = 0; i < numParamsExpected; ++i)
			{
				const Client::IInputParameterRegistry::SParameterInfo& pi = inputParamsReg.GetParameter(i);

				//
				// look up the child it's being represented by: first search by parameter ID, then by parameter name
				//

				const ITextualInputBlueprint* pSourceChild;
				if (!(pSourceChild = sourceParent.FindChildByParamID(pi.id)))
				{
					if (!(pSourceChild = sourceParent.FindChildByParamName(pi.szName)))
					{
						if (DataSource::ISyntaxErrorCollector* pSE = sourceParent.GetSyntaxErrorCollector())
						{
							char idAsFourCharacterString[5];
							pi.id.ToString(idAsFourCharacterString);
							pSE->AddErrorMessage("Missing parameter: ID = '%s', name = '%s'", idAsFourCharacterString, pi.szName);
						}
						return false;
					}
				}

				CInputBlueprint *pNewChild = new CInputBlueprint;
				m_children.push_back(pNewChild);

				//
				// look up the function of that new child: first search by GUID, then fall back to its name
				//

				const CryGUID& funcGUID = pSourceChild->GetFuncGUID();
				const char* szFuncName = pSourceChild->GetFuncName();
				if (!(pNewChild->m_pFunctionFactory = g_pHub->GetFunctionFactoryDatabase().FindFactoryByGUID(funcGUID)))
				{
					if (!(pNewChild->m_pFunctionFactory = g_pHub->GetFunctionFactoryDatabase().FindFactoryByName(szFuncName)))
					{
						if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
						{
							Shared::CUqsString guidAsString;
							Shared::Internal::CGUIDHelper::ToString(funcGUID, guidAsString);
							pSE->AddErrorMessage("Unknown function: GUID = %s, name = '%s'", guidAsString.c_str(), szFuncName);
						}
						return false;
					}
				}

				//
				// ensure that the function's return type matches the parameter's type
				//

				const Shared::CTypeInfo& childReturnType = pNewChild->m_pFunctionFactory->GetReturnType();
				if (childReturnType != pi.type)
				{
					if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Parameter '%s' is of type '%s', but Function '%s' returns a '%s'", pi.szName, pi.type.name(), szFuncName, childReturnType.name());
					}
					return false;
				}

				//
				// see if the function is configured to output debug stuff into the debug-render-world every time it will be called at runtime
				//

				pNewChild->m_bAddReturnValueToDebugRenderWorldUponExecution = pSourceChild->GetAddReturnValueToDebugRenderWorldUponExecution();

				//
				// - if this function returns the iterated item, ensure that the function is not part of a generator
				// - reason: at runtime, there's no iteration going on during the generator phase, only during the evaluator phase (of course)
				//

				if (bResolvingForAGenerator && pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == Client::IFunctionFactory::ELeafFunctionKind::IteratedItem)
				{
					if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Generators cannot use functions that return the iterated item (this is only possible for evaluators)");
					}
					return false;
				}

				//
				// if the function returns the iterated item, ensure that this item type matches the type of items to generate
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == Client::IFunctionFactory::ELeafFunctionKind::IteratedItem)
				{
					// just in case the generator had already problems getting resolved and we ended up without one so far...
					if (const CGeneratorBlueprint* pGeneratorBP = queryBlueprintForGlobalParamChecking.GetGeneratorBlueprint())
					{
						const Shared::CTypeInfo& returnType = pNewChild->m_pFunctionFactory->GetReturnType();
						const Shared::CTypeInfo& typeOfItemsToGenerate = pGeneratorBP->GetTypeOfItemsToGenerate();

						if (returnType != typeOfItemsToGenerate)
						{
							if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("This function returns items of type '%s', which mismatches the type of items to generate: '%s'", returnType.name(), typeOfItemsToGenerate.name());
							}
							return false;
						}
						else
						{
							pNewChild->m_leafFunctionReturnValue.SetItemIteration();
						}
					}
				}

				//
				// if the function returns a global param (ELeafFunctionKind::GlobalParam), ensure that this global param exists and that its type matches
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == Client::IFunctionFactory::ELeafFunctionKind::GlobalParam)
				{
					const char* szNameOfGlobalParam = pSourceChild->GetFuncReturnValueLiteral();
					const Client::IItemFactory* pItemFactoryOfThatGlobalParam = nullptr;

					// search among the global constant-params
					{
						const CGlobalConstantParamsBlueprint& constantParamsBP = queryBlueprintForGlobalParamChecking.GetGlobalConstantParamsBlueprint();
						const std::map<string, CGlobalConstantParamsBlueprint::SParamInfo>& params = constantParamsBP.GetParams();
						auto it = params.find(szNameOfGlobalParam);
						if (it != params.cend())
						{
							pItemFactoryOfThatGlobalParam = it->second.pItemFactory;
						}
					}

					// search among the global runtime-params
					if(!pItemFactoryOfThatGlobalParam)
					{
						const CGlobalRuntimeParamsBlueprint& runtimeParamsBP = queryBlueprintForGlobalParamChecking.GetGlobalRuntimeParamsBlueprint();
						const std::map<string, CGlobalRuntimeParamsBlueprint::SParamInfo>& params = runtimeParamsBP.GetParams();
						auto it = params.find(szNameOfGlobalParam);
						if (it != params.cend())
						{
							pItemFactoryOfThatGlobalParam = it->second.pItemFactory;
						}
					}

					// if the global param exists, then check for type mismatches
					if (pItemFactoryOfThatGlobalParam)
					{
						const Shared::CTypeInfo& typeOfGlobalParam = pItemFactoryOfThatGlobalParam->GetItemType();
						const Shared::CTypeInfo& returnTypeOfFunction = pNewChild->m_pFunctionFactory->GetReturnType();

						// types mismatch?
						if (typeOfGlobalParam != returnTypeOfFunction)
						{
							if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("Return type of function '%s' (%s) mismatches the type of the global param '%s' (%s)", pNewChild->m_pFunctionFactory->GetName(), returnTypeOfFunction.name(), szNameOfGlobalParam, typeOfGlobalParam.name());
							}
							return false;
						}
						else
						{
							pNewChild->m_leafFunctionReturnValue.SetGlobalParam(szNameOfGlobalParam);
						}
					}
					else
					{
						// the referenced global param doesn't exist -> syntax error
						if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Function '%s' returns an unknown global param: '%s'", pNewChild->m_pFunctionFactory->GetName(), szNameOfGlobalParam);
						}
						return false;
					}
				}

				//
				// if the function returns a literal (ELeafFunctionKind::Literal), then make sure the textual representation of that literal can be parsed
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == Client::IFunctionFactory::ELeafFunctionKind::Literal)
				{
					const Shared::CTypeInfo& returnType = pNewChild->m_pFunctionFactory->GetReturnType();
					Client::IItemFactory* pItemFactoryOfReturnType = g_pHub->GetUtils().FindItemFactoryByType(returnType);

					// notice: if no item-factory for the function's return type was found, then the function's return type hasn't been registered in the item-factory-database
					//         (in fact, the StartupConsistencyChecker should have detected this already, but still, we handle it gracefully here)
					if (pItemFactoryOfReturnType)
					{
						if (pItemFactoryOfReturnType->CanBePersistantlySerialized())
						{
							// try to deserialize the literal from its textual representation

							const char* szTextualRepresentationOfThatLiteral = pSourceChild->GetFuncReturnValueLiteral();
							IItemSerializationSupport& itemSerializationSupport = UQS::Core::IHubPlugin::GetHub().GetItemSerializationSupport();
							Shared::CUqsString deserializationErrorMessage;

							void* pTmpItem = pItemFactoryOfReturnType->CreateItems(1, Client::IItemFactory::EItemInitMode::UseDefaultConstructor);
							const bool bDeserializationSucceeded = itemSerializationSupport.DeserializeItemFromCStringLiteral(pTmpItem, *pItemFactoryOfReturnType, szTextualRepresentationOfThatLiteral, &deserializationErrorMessage);

							if (bDeserializationSucceeded)
							{
								pNewChild->m_leafFunctionReturnValue.SetLiteral(*pItemFactoryOfReturnType, pTmpItem);
							}
							else
							{
								if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
								{
									pSE->AddErrorMessage("Function '%s' returns a literal of type %s, but the literal could not be parsed from its archive representation: '%s'. Reason:\n%s", pNewChild->m_pFunctionFactory->GetName(), returnType.name(), szTextualRepresentationOfThatLiteral, deserializationErrorMessage.c_str());
								}
								return false;
							}

							pItemFactoryOfReturnType->DestroyItems(pTmpItem);
						}
						else
						{
							// - the type of the literal has no textual representation (e. g. it could be a pointer or some complex struct)
							// - in fact, we should never get here, unless a client has forcefully hacked around or accidentally
							//   registered a wrong function as ELeafFunctionKind::Literal
							if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("Function '%s' is of kind ELeafFunctionKind::Literal but its return type (%s) cannot be represented in textual form", pNewChild->m_pFunctionFactory->GetName(), returnType.name());
							}
							return false;
						}
					}
				}

				//
				// if the function returns the shuttled items (ELeafFunctionKind::ShuttledItems), then make sure that the query will be provided (at runtime) with shuttled items that match the type of the function's return value
				//

				if (pNewChild->m_pFunctionFactory->GetLeafFunctionKind() == Client::IFunctionFactory::ELeafFunctionKind::ShuttledItems)
				{
					if (const Shared::CTypeInfo* pTypeOfPossiblyShuttledItems = queryBlueprintForGlobalParamChecking.GetTypeOfShuttledItemsToExpect())
					{
						// notice: the type of the shuttle actually specifies the *container* type of items, hence we need access to the *contained* type
						const Shared::CTypeInfo* pContainedType = pNewChild->m_pFunctionFactory->GetContainedType();

						// if this assert fails, then something must have become inconsistent between Client::Internal::CFunc_ShuttledItems<> and Client::Internal::SContainedTypeRetriever<>
						assert(pContainedType);

						if (*pContainedType != *pTypeOfPossiblyShuttledItems)
						{
							if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
							{
								pSE->AddErrorMessage("Function '%s' is of kind ELeafFunctionKind::ShuttledItems and expects the shuttled items to be of type '%s', but they are actually of type '%s'", pNewChild->m_pFunctionFactory->GetName(), pContainedType->name(), pTypeOfPossiblyShuttledItems->name());
							}
							return false;
						}
						else
						{
							pNewChild->m_leafFunctionReturnValue.SetShuttledItems();
						}
					}
					else
					{
						if (DataSource::ISyntaxErrorCollector* pSE = pSourceChild->GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Function '%s' is of kind ELeafFunctionKind::ShuttledItems, but the query does not support shuttled items in this context", pNewChild->m_pFunctionFactory->GetName());
						}
						return false;
					}
				}

				//
				// recurse down the function call hierarchy
				//

				if (!pNewChild->Resolve(*pSourceChild, pNewChild->m_pFunctionFactory->GetInputParameterRegistry(), queryBlueprintForGlobalParamChecking, bResolvingForAGenerator))
				{
					return false;
				}
			}

			return true;
		}

		size_t CInputBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		const CInputBlueprint& CInputBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return *m_children[index];
		}

		Client::IFunctionFactory* CInputBlueprint::GetFunctionFactory() const
		{
			return m_pFunctionFactory;
		}

		const CLeafFunctionReturnValue& CInputBlueprint::GetLeafFunctionReturnValue() const
		{
			return m_leafFunctionReturnValue;
		}

		bool CInputBlueprint::GetAddReturnValueToDebugRenderWorldUponExecution() const
		{
			return m_bAddReturnValueToDebugRenderWorldUponExecution;
		}

	}
}
