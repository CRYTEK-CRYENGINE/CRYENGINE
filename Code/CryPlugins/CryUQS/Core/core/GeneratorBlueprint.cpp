// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GeneratorBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualGeneratorBlueprint
		//
		//===================================================================================

		CTextualGeneratorBlueprint::CTextualGeneratorBlueprint()
			: m_generatorGUID(CryGUID::Null())
		{
		}

		void CTextualGeneratorBlueprint::SetGeneratorName(const char* szGeneratorName)
		{
			m_generatorName = szGeneratorName;
		}

		void CTextualGeneratorBlueprint::SetGeneratorGUID(const CryGUID& generatorGUID)
		{
			m_generatorGUID = generatorGUID;
		}

		ITextualInputBlueprint& CTextualGeneratorBlueprint::GetInputRoot()
		{
			return m_rootInput;
		}

		const char* CTextualGeneratorBlueprint::GetGeneratorName() const
		{
			return m_generatorName.c_str();
		}

		const CryGUID& CTextualGeneratorBlueprint::GetGeneratorGUID() const
		{
			return m_generatorGUID;
		}

		const ITextualInputBlueprint& CTextualGeneratorBlueprint::GetInputRoot() const
		{
			return m_rootInput;
		}

		void CTextualGeneratorBlueprint::SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector)
		{
			m_pSyntaxErrorCollector = std::move(pSyntaxErrorCollector);
		}

		DataSource::ISyntaxErrorCollector* CTextualGeneratorBlueprint::GetSyntaxErrorCollector() const
		{
			return m_pSyntaxErrorCollector.get();
		}

		//===================================================================================
		//
		// CGeneratorBlueprint
		//
		//===================================================================================

		CGeneratorBlueprint::CGeneratorBlueprint()
			: m_pGeneratorFactory(nullptr)
		{}

		bool CGeneratorBlueprint::Resolve(const ITextualGeneratorBlueprint& source, const CQueryBlueprint& queryBlueprintForGlobalParamChecking)
		{
			const CryGUID& generatorGUID = source.GetGeneratorGUID();
			const char* szGeneratorName = source.GetGeneratorName();

			//
			// look up the generator factory: first search by its GUID, then by its name
			//

			if (!(m_pGeneratorFactory = g_pHub->GetGeneratorFactoryDatabase().FindFactoryByGUID(generatorGUID)))
			{
				if (!(m_pGeneratorFactory = g_pHub->GetGeneratorFactoryDatabase().FindFactoryByName(szGeneratorName)))
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						Shared::CUqsString guidAsString;
						Shared::Internal::CGUIDHelper::ToString(generatorGUID, guidAsString);
						pSE->AddErrorMessage("Unknown GeneratorFactory: GUID = %s, name = '%s'", guidAsString.c_str(), szGeneratorName);
					}
					return false;
				}
			}

			//
			// if the generator expects shuttled items of a certain type, then make sure the query will store such items at runtime on the blackboard
			//

			if (const Shared::CTypeInfo* pExpectedShuttleType = m_pGeneratorFactory->GetTypeOfShuttledItemsToExpect())
			{
				if (const Shared::CTypeInfo* pTypeOfPossiblyShuttledItems = queryBlueprintForGlobalParamChecking.GetTypeOfShuttledItemsToExpect())
				{
					if (*pExpectedShuttleType != *pTypeOfPossiblyShuttledItems)
					{
						if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Generator '%s' expects the shuttled items to be of type '%s', but they are actually of type '%s'", szGeneratorName, pExpectedShuttleType->name(), pTypeOfPossiblyShuttledItems->name());
						}
						return false;
					}
				}
				else
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Generator '%s' expects shuttled items, but the query does not support shuttled items in this context", szGeneratorName);
					}
					return false;
				}
			}

			//
			// resolve input parameters
			//

			CInputBlueprint inputRoot;
			const ITextualInputBlueprint& textualInputRoot = source.GetInputRoot();
			const Client::IInputParameterRegistry& inputParamsReg = m_pGeneratorFactory->GetInputParameterRegistry();

			if (!inputRoot.Resolve(textualInputRoot, inputParamsReg, queryBlueprintForGlobalParamChecking, true))
			{
				return false;
			}

			ResolveInputs(inputRoot);

			return true;
		}

		const Shared::CTypeInfo& CGeneratorBlueprint::GetTypeOfItemsToGenerate() const
		{
			assert(m_pGeneratorFactory);
			return m_pGeneratorFactory->GetTypeOfItemsToGenerate();
		}

		Client::GeneratorUniquePtr CGeneratorBlueprint::InstantiateGenerator(const SQueryBlackboard& blackboard, Shared::CUqsString& error) const
		{
			assert(m_pGeneratorFactory);

			//
			// create the input parameters (they will get filled by the function calls below)
			//

			Client::ParamsHolderUniquePtr pParamsHolder = m_pGeneratorFactory->GetParamsHolderFactory().CreateParamsHolder();
			void* pParams = pParamsHolder->GetParams();

			//
			// fill all input parameters via function calls
			//

			CFunctionCallHierarchy functionCalls;

			if (!InstantiateFunctionCallHierarchy(functionCalls, blackboard, error))    // notice: the blackboard.pItemIterationContext is still a nullptr (we're not iterating on the items yet)
			{
				return nullptr;
			}

			bool bExceptionOccurredDuringFunctionCalls = false;
			const Client::IFunction::SExecuteContext execContext(0, blackboard, error, bExceptionOccurredDuringFunctionCalls);  // currentItemIndex == 0: this is just a dummy, as we're not iterating on items

			functionCalls.ExecuteAll(execContext, pParams, m_pGeneratorFactory->GetInputParameterRegistry());

			if (bExceptionOccurredDuringFunctionCalls)
			{
				return nullptr;
			}

			//
			// instantiate the generator with the input params
			//

			Client::GeneratorUniquePtr pGenerator = m_pGeneratorFactory->CreateGenerator(pParams);   // never returns NULL
			assert(pGenerator);
			return pGenerator;
		}

		void CGeneratorBlueprint::PrintToConsole(CLogger& logger) const
		{
			logger.Printf("Generator: %s [%s]", m_pGeneratorFactory->GetName(), m_pGeneratorFactory->GetTypeOfItemsToGenerate().name());
		}

	}
}
