// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GlobalRuntimeParamsBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualGlobalRuntimeParamsBlueprint
		//
		//===================================================================================

		CTextualGlobalRuntimeParamsBlueprint::CTextualGlobalRuntimeParamsBlueprint()
		{
			// nothing
		}

		void CTextualGlobalRuntimeParamsBlueprint::AddParameter(const char* szName, const char* szTypeName, const CryGUID& typeGUID, bool bAddToDebugRenderWorld, DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector)
		{
			m_parameters.emplace_back(szName, szTypeName, typeGUID, bAddToDebugRenderWorld, std::move(pSyntaxErrorCollector));
		}

		size_t CTextualGlobalRuntimeParamsBlueprint::GetParameterCount() const
		{
			return m_parameters.size();
		}

		ITextualGlobalRuntimeParamsBlueprint::SParameterInfo CTextualGlobalRuntimeParamsBlueprint::GetParameter(size_t index) const
		{
			assert(index < m_parameters.size());
			const SStoredParameterInfo& pi = m_parameters[index];
			return SParameterInfo(pi.name.c_str(), pi.typeName.c_str(), pi.typeGUID, pi.bAddToDebugRenderWorld, pi.pSyntaxErrorCollector.get());
		}

		//===================================================================================
		//
		// CGlobalRuntimeParamsBlueprint
		//
		//===================================================================================

		CGlobalRuntimeParamsBlueprint::CGlobalRuntimeParamsBlueprint()
		{
			// nothing
		}

		bool CGlobalRuntimeParamsBlueprint::Resolve(const ITextualGlobalRuntimeParamsBlueprint& source, const CQueryBlueprint* pParentQueryBlueprint)
		{
			//
			// - parse the runtime-params from given source
			// - we do this before inheriting from given parent because we wanna detect duplicates in the source, and not mistake an inherited from the parent for being a duplicate
			//

			const size_t numParams = source.GetParameterCount();

			for (size_t i = 0; i < numParams; ++i)
			{
				const ITextualGlobalRuntimeParamsBlueprint::SParameterInfo p = source.GetParameter(i);

				// ensure each parameter exists only once
				if (m_runtimeParameters.find(p.szName) != m_runtimeParameters.cend())
				{
					if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
					{
						pSE->AddErrorMessage("Duplicate parameter: '%s'", p.szName);
					}
					return false;
				}

				// find the item factory: first by GUID, then by name
				Client::IItemFactory* pItemFactory;
				if (!(pItemFactory = g_pHub->GetItemFactoryDatabase().FindFactoryByGUID(p.typeGUID)))
				{
					if (!(pItemFactory = g_pHub->GetItemFactoryDatabase().FindFactoryByName(p.szTypeName)))
					{
						if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
						{
							Shared::CUqsString typeGuidAsString;
							Shared::Internal::CGUIDHelper::ToString(p.typeGUID, typeGuidAsString);
							pSE->AddErrorMessage("Unknown item type: GUID = %s, name = '%s'", typeGuidAsString.c_str(), p.szTypeName);
						}
						return false;
					}
				}

				// - if the same parameter exists already in a parent query, ensure that both have the same data type (name clashes are fine, but type clashes are not!)
				// - we do these checks not only for the user's convenience to notify him of mistakes while he's editing (at runtime, the issue would get detected anyway, as one
				//   particular runtime-parameter can only have one particular type), but also for CQueryBlueprint::VisitRuntimeParams() to guarantee that "duplicate" parameters
				//   are still of the same item type
				if (pParentQueryBlueprint)
				{
					/* an example:

						Parent query:
							runtime param "A": int
							runtime param "B": float

						Child query:
							runtime param "A": int
							runtime param "B": Vec3

						=> A is fine, but B is not as the types differ in the queries
					*/

					if (const Client::IItemFactory* pParentItemFactory = FindItemFactoryByParamNameInParentRecursively(p.szName, *pParentQueryBlueprint))
					{
						// name clash detected (this is totally fine, though)
						// -> now check for type clash
						if (pParentItemFactory != pItemFactory)
						{
							// type clash detected
							if (DataSource::ISyntaxErrorCollector* pSE = p.pSyntaxErrorCollector)
							{
								pSE->AddErrorMessage("Type mismatch: expected '%s' (since the parent's parameter is of that type), but got a '%s'", pParentItemFactory->GetItemType().name(), pItemFactory->GetItemType().name());
							}
							return false;
						}
					}
				}

				m_runtimeParameters.insert(std::map<string, SParamInfo>::value_type(p.szName, SParamInfo(pItemFactory, p.bAddToDebugRenderWorld)));
			}

			return true;
		}

		const std::map<string, CGlobalRuntimeParamsBlueprint::SParamInfo>& CGlobalRuntimeParamsBlueprint::GetParams() const
		{
			return m_runtimeParameters;
		}

		void CGlobalRuntimeParamsBlueprint::PrintToConsole(CLogger& logger) const
		{
			if (m_runtimeParameters.empty())
			{
				logger.Printf("Global runtime params: (none)");
			}
			else
			{
				logger.Printf("Global runtime params:");
				CLoggerIndentation _indent;
				for (const auto& entry : m_runtimeParameters)
				{
					const char* szParamName = entry.first.c_str();
					const Client::IItemFactory* pItemFactory = entry.second.pItemFactory;
					logger.Printf("\"%s\" [%s]", szParamName, pItemFactory->GetName());
				}
			}
		}

		const Client::IItemFactory* CGlobalRuntimeParamsBlueprint::FindItemFactoryByParamNameInParentRecursively(const char* szParamNameToSearchFor, const CQueryBlueprint& parentalQueryBlueprint) const
		{
			const CGlobalRuntimeParamsBlueprint& parentGlobalRuntimeParamsBP = parentalQueryBlueprint.GetGlobalRuntimeParamsBlueprint();
			const std::map<string, SParamInfo>& params = parentGlobalRuntimeParamsBP.GetParams();
			auto it = params.find(szParamNameToSearchFor);

			if (it != params.end())
			{
				return it->second.pItemFactory;
			}

			if (const CQueryBlueprint* pGrandParent = parentalQueryBlueprint.GetParent())
			{
				// recurse up
				return pGrandParent->GetGlobalRuntimeParamsBlueprint().FindItemFactoryByParamNameInParentRecursively(szParamNameToSearchFor, *pGrandParent);
			}

			return nullptr;
		}

	}
}
