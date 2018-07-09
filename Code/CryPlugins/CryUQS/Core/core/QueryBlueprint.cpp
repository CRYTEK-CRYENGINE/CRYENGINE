// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "QueryBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CTextualQueryBlueprint
		//
		//===================================================================================

		CTextualQueryBlueprint::CTextualQueryBlueprint()
			: m_queryFactoryGUID(CryGUID::Null())
			, m_maxItemsToKeepInResultSet(0)
		{
			// nothing
		}

		CTextualQueryBlueprint::~CTextualQueryBlueprint()
		{
			for (CTextualEvaluatorBlueprint* pIE : m_instantEvaluators)
			{
				delete pIE;
			}

			for (CTextualEvaluatorBlueprint* pDE : m_deferredEvaluators)
			{
				delete pDE;
			}

			for (CTextualQueryBlueprint* pChild : m_children)
			{
				delete pChild;
			}
		}

		void CTextualQueryBlueprint::SetName(const char* szName)
		{
			m_name = szName;
		}

		void CTextualQueryBlueprint::SetQueryFactoryName(const char* szFactoryName)
		{
			m_queryFactoryName = szFactoryName;
		}

		void CTextualQueryBlueprint::SetQueryFactoryGUID(const CryGUID& factoryGUID)
		{
			m_queryFactoryGUID = factoryGUID;
		}

		void CTextualQueryBlueprint::SetMaxItemsToKeepInResultSet(size_t maxItems)
		{
			m_maxItemsToKeepInResultSet = maxItems;
		}

		ITextualGlobalConstantParamsBlueprint& CTextualQueryBlueprint::GetGlobalConstantParams()
		{
			return m_globalConstantParams;
		}

		ITextualGlobalRuntimeParamsBlueprint& CTextualQueryBlueprint::GetGlobalRuntimeParams()
		{
			return m_globalRuntimeParams;
		}

		ITextualGeneratorBlueprint& CTextualQueryBlueprint::SetGenerator()
		{
			m_pGenerator.reset(new CTextualGeneratorBlueprint);
			return *m_pGenerator;
		}

		ITextualEvaluatorBlueprint& CTextualQueryBlueprint::AddInstantEvaluator()
		{
			CTextualEvaluatorBlueprint* pNewInstantEvaluatorBP = new CTextualEvaluatorBlueprint;
			m_instantEvaluators.push_back(pNewInstantEvaluatorBP);
			return *pNewInstantEvaluatorBP;
		}

		size_t CTextualQueryBlueprint::GetInstantEvaluatorCount() const
		{
			return m_instantEvaluators.size();
		}

		ITextualEvaluatorBlueprint& CTextualQueryBlueprint::AddDeferredEvaluator()
		{
			CTextualEvaluatorBlueprint* pNewDeferredEvaluatorBlueprint = new CTextualEvaluatorBlueprint;
			m_deferredEvaluators.push_back(pNewDeferredEvaluatorBlueprint);
			return *pNewDeferredEvaluatorBlueprint;
		}

		ITextualQueryBlueprint& CTextualQueryBlueprint::AddChild()
		{
			CTextualQueryBlueprint* pNewChild = new CTextualQueryBlueprint;
			m_children.push_back(pNewChild);
			return *pNewChild;
		}

		size_t CTextualQueryBlueprint::GetDeferredEvaluatorCount() const
		{
			return m_deferredEvaluators.size();
		}

		const char* CTextualQueryBlueprint::GetName() const
		{
			return m_name.c_str();
		}

		const char* CTextualQueryBlueprint::GetQueryFactoryName() const
		{
			return m_queryFactoryName.c_str();
		}

		const CryGUID& CTextualQueryBlueprint::GetQueryFactoryGUID() const
		{
			return m_queryFactoryGUID;
		}

		size_t CTextualQueryBlueprint::GetMaxItemsToKeepInResultSet() const
		{
			return m_maxItemsToKeepInResultSet;
		}

		const ITextualGlobalConstantParamsBlueprint& CTextualQueryBlueprint::GetGlobalConstantParams() const
		{
			return m_globalConstantParams;
		}

		const ITextualGlobalRuntimeParamsBlueprint& CTextualQueryBlueprint::GetGlobalRuntimeParams() const
		{
			return m_globalRuntimeParams;
		}

		const ITextualGeneratorBlueprint* CTextualQueryBlueprint::GetGenerator() const
		{
			return m_pGenerator.get();
		}

		const ITextualEvaluatorBlueprint& CTextualQueryBlueprint::GetInstantEvaluator(size_t index) const
		{
			assert(index < m_instantEvaluators.size());
			return *m_instantEvaluators[index];
		}

		const ITextualEvaluatorBlueprint& CTextualQueryBlueprint::GetDeferredEvaluator(size_t index) const
		{
			assert(index < m_deferredEvaluators.size());
			return *m_deferredEvaluators[index];
		}

		size_t CTextualQueryBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		const ITextualQueryBlueprint& CTextualQueryBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return *m_children[index];
		}

		void CTextualQueryBlueprint::SetSyntaxErrorCollector(DataSource::SyntaxErrorCollectorUniquePtr pSyntaxErrorCollector)
		{
			m_pSyntaxErrorCollector = std::move(pSyntaxErrorCollector);
		}

		DataSource::ISyntaxErrorCollector* CTextualQueryBlueprint::GetSyntaxErrorCollector() const
		{
			return m_pSyntaxErrorCollector.get();
		}

		//===================================================================================
		//
		// CQueryBlueprint
		//
		//===================================================================================

		CQueryBlueprint::CQueryBlueprint()
			: m_pQueryFactory(nullptr)
			, m_maxItemsToKeepInResultSet(0)
			, m_pParent(nullptr)
		{
			// nothing
		}

		CQueryBlueprint::~CQueryBlueprint()
		{
			for (CInstantEvaluatorBlueprint* pIE : m_instantEvaluators)
			{
				delete pIE;
			}

			for (CDeferredEvaluatorBlueprint* pDE : m_deferredEvaluators)
			{
				delete pDE;
			}
		}

		const char* CQueryBlueprint::GetName() const
		{
			return m_name.c_str();
		}

		void CQueryBlueprint::VisitRuntimeParams(Client::IQueryBlueprintRuntimeParamVisitor& visitor) const
		{
			std::map<string, Client::IItemFactory*> allRuntimeParamsInTheHierarchy;

			GrabRuntimeParamsRecursively(allRuntimeParamsInTheHierarchy);

			for (const auto& pair : allRuntimeParamsInTheHierarchy)
			{
				const char* szParamName = pair.first.c_str();
				Client::IItemFactory* pItemFactory = pair.second;
				assert(pItemFactory);
				visitor.OnRuntimeParamVisited(szParamName, *pItemFactory);
			}
		}

		const Shared::CTypeInfo& CQueryBlueprint::GetOutputType() const
		{
			assert(m_pQueryFactory);
			return m_pQueryFactory->GetQueryBlueprintType(*this);
		}

		bool CQueryBlueprint::Resolve(const ITextualQueryBlueprint& source)
		{
			// name
			m_name = source.GetName();

			// query factory: first search by its GUID, then by its name
			{
				const CryGUID& queryFactoryGUID = source.GetQueryFactoryGUID();
				const char* szQueryFactoryName = source.GetQueryFactoryName();

				if(!(m_pQueryFactory = static_cast<CQueryFactoryBase*>(g_pHub->GetQueryFactoryDatabase().FindFactoryByGUID(queryFactoryGUID))))         // the static_cast<> is kinda ok'ish here, since IQueryFactory and its derived class CQueryFactoryBase are _both_ defined in the core, so we definitely know about the inheritance hierarchy
				{
					if (!(m_pQueryFactory = static_cast<CQueryFactoryBase*>(g_pHub->GetQueryFactoryDatabase().FindFactoryByName(szQueryFactoryName))))  // ditto
					{
						if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							Shared::CUqsString guidAsString;
							Shared::Internal::CGUIDHelper::ToString(queryFactoryGUID, guidAsString);
							pSE->AddErrorMessage("Unknown QueryFactory: GUID = %s, name = '%s'", guidAsString.c_str(), szQueryFactoryName);
						}
						return false;
					}
				}
			}

			// max. items to keep in result set
			m_maxItemsToKeepInResultSet = source.GetMaxItemsToKeepInResultSet();

			// ensure no duplicates between constant-params and runtime-params
			for (size_t i1 = 0; i1 < source.GetGlobalConstantParams().GetParameterCount(); ++i1)
			{
				for (size_t i2 = 0; i2 < source.GetGlobalRuntimeParams().GetParameterCount(); ++i2)
				{
					const CTextualGlobalConstantParamsBlueprint::SParameterInfo p1 = source.GetGlobalConstantParams().GetParameter(i1);
					const CTextualGlobalRuntimeParamsBlueprint::SParameterInfo p2 = source.GetGlobalRuntimeParams().GetParameter(i2);

					if (strcmp(p1.szName, p2.szName) == 0)
					{
						// output error to constant-params
						if (DataSource::ISyntaxErrorCollector* pSE = p1.pSyntaxErrorCollector)
						{
							pSE->AddErrorMessage("Global constant-parameter clashes with runtime-parameter of the same name: '%s'", p1.szName);
						}

						// output error to runtime-params
						if (DataSource::ISyntaxErrorCollector* pSE = p2.pSyntaxErrorCollector)
						{
							pSE->AddErrorMessage("Global runtime-parameter clashes with constant-parameter of the same name: '%s'", p2.szName);
						}

						return false;
					}
				}
			}

			// global constant-params
			if (!m_globalConstantParams.Resolve(source.GetGlobalConstantParams()))
			{
				return false;
			}

			// global runtime-params
			if (!m_globalRuntimeParams.Resolve(source.GetGlobalRuntimeParams(), m_pParent))
			{
				return false;
			}

			// generator (a generator is optional and typically never exists in composite queries)
			if (const ITextualGeneratorBlueprint* pGeneratorBlueprint = source.GetGenerator())
			{
				m_pGenerator.reset(new CGeneratorBlueprint);
				if (!m_pGenerator->Resolve(*pGeneratorBlueprint, *this))
				{
					m_pGenerator.reset();    // nullify the generator so that CInputBlueprint::Resolve() won't be tempted to use this half-baked object for further checks (and crash becuase it might be lacking a generator-factory)
					return false;
				}
			}

			// instant-evaluators
			for (size_t i = 0; i < source.GetInstantEvaluatorCount(); ++i)
			{
				CInstantEvaluatorBlueprint* pNewInstantEvaluatorBP = new CInstantEvaluatorBlueprint;
				m_instantEvaluators.push_back(pNewInstantEvaluatorBP);
				if (!pNewInstantEvaluatorBP->Resolve(source.GetInstantEvaluator(i), *this))
				{
					return false;
				}
			}

			// deferred-evaluators
			for (size_t i = 0; i < source.GetDeferredEvaluatorCount(); ++i)
			{
				CDeferredEvaluatorBlueprint* pNewDeferredEvaluatorBP = new CDeferredEvaluatorBlueprint;
				m_deferredEvaluators.push_back(pNewDeferredEvaluatorBP);
				if (!pNewDeferredEvaluatorBP->Resolve(source.GetDeferredEvaluator(i), *this))
				{
					return false;
				}
			}

			//
			// - ensure that the max. number of instant- and deferred-evaluators is not exceeded
			// - otherwise, CQuery cannot keep track of all evaluator's states as a bitfield during query execution
			//

			{
				const size_t numInstantEvaluators = m_instantEvaluators.size();
				if (numInstantEvaluators > UQS_MAX_EVALUATORS)
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Exceeded the maximum number of instant-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numInstantEvaluators);
					}
					return false;
				}
			}

			{
				const size_t numDeferredEvaluators = m_deferredEvaluators.size();
				if (numDeferredEvaluators > UQS_MAX_EVALUATORS)
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Exceeded the maximum number of deferred-evaluators in the query blueprint (max %i supported, %i present in the blueprint)", UQS_MAX_EVALUATORS, (int)numDeferredEvaluators);
					}
					return false;
				}
			}

			// children (recurse down)
			for (size_t i = 0, n = source.GetChildCount(); i < n; ++i)
			{
				const ITextualQueryBlueprint& childSource = source.GetChild(i);
				std::shared_ptr<CQueryBlueprint> pChildTarget(new CQueryBlueprint);
				pChildTarget->m_pParent = this;
				m_children.push_back(pChildTarget);
				if (!pChildTarget->Resolve(childSource))
					return false;
			}

			// if the query-factory expects to have a generator in the blueprint, then ensure that one was provided (and the other way around)
			if (m_pQueryFactory)  // might be a nullptr in case of unknown query factory (c. f. code further above)
			{
				if (m_pQueryFactory->RequiresGenerator() && !m_pGenerator)
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Query factories of type '%s' require a generator, but none is provided", m_pQueryFactory->GetName());
					}
					return false;
				}
				else if (!m_pQueryFactory->RequiresGenerator() && m_pGenerator)
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("Query factories of type '%s' don't require a generator, but one was provided", m_pQueryFactory->GetName());
					}
					return false;
				}
			}

			// validate the min/max number of child queries
			if (m_pQueryFactory)  // might be a nullptr in case of unknown query factory (c. f. code further above)
			{
				const size_t actualNumChildQueries = m_children.size();

				// min required child queries
				{
					const size_t minRequiredChildQueries = m_pQueryFactory->GetMinRequiredChildren();
					if (actualNumChildQueries < minRequiredChildQueries)
					{
						if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Too few child queries: %i (expected at least %i)", (int)actualNumChildQueries, (int)minRequiredChildQueries);
						}
						return false;
					}
				}

				// max allowed child queries
				{
					const size_t maxAllowedChildQueries = m_pQueryFactory->GetMaxAllowedChildren();
					if (actualNumChildQueries > maxAllowedChildQueries)
					{
						if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							pSE->AddErrorMessage("Too many child queries: %i (expected no more than %i)", (int)actualNumChildQueries, (int)maxAllowedChildQueries);
						}
						return false;
					}
				}
			}

			// have the query-factory check that if it deals with child-queries that their output types are compatible among each other
			{
				string error;
				size_t indexOfChildCausingTheError = 0;

				if (!m_pQueryFactory->CheckOutputTypeCompatibilityAmongChildQueryBlueprints(*this, error, indexOfChildCausingTheError))
				{
					// write the error message into the child that caused the error
					const ITextualQueryBlueprint& childSource = source.GetChild(indexOfChildCausingTheError);
					if (DataSource::ISyntaxErrorCollector* pSE = childSource.GetSyntaxErrorCollector())
					{
						pSE->AddErrorMessage("%s", error.c_str());
					}
					return false;
				}
			}

			// sort the instant-evaluator blueprints by cost and evaluation modality such that their order at execution time won't change
			// (this also helps the user to read the query history as it will show all evaluators in order of how they were executed)
			SortInstantEvaluatorBlueprintsByCostAndEvaluationModality();

			return true;
		}

		const CQueryBlueprint* CQueryBlueprint::GetParent() const
		{
			return m_pParent;
		}

		size_t CQueryBlueprint::GetChildCount() const
		{
			return m_children.size();
		}

		std::shared_ptr<const CQueryBlueprint> CQueryBlueprint::GetChild(size_t index) const
		{
			assert(index < m_children.size());
			return m_children[index];
		}

		int CQueryBlueprint::GetChildIndex(const CQueryBlueprint* pChildToSearchFor) const
		{
			for (size_t i = 0; i < m_children.size(); ++i)
			{
				if (m_children[i].get() == pChildToSearchFor)
					return (int)i;
			}
			return -1;
		}

		QueryBaseUniquePtr CQueryBlueprint::CreateQuery(const CQueryBase::SCtorContext& ctorContext) const
		{
			assert(m_pQueryFactory);
			return m_pQueryFactory->CreateQuery(ctorContext);
		}

		int CQueryBlueprint::GetMaxItemsToKeepInResultSet() const
		{
			return m_maxItemsToKeepInResultSet;
		}

		const CGlobalConstantParamsBlueprint& CQueryBlueprint::GetGlobalConstantParamsBlueprint() const
		{
			return m_globalConstantParams;
		}

		const CGlobalRuntimeParamsBlueprint& CQueryBlueprint::GetGlobalRuntimeParamsBlueprint() const
		{
			return m_globalRuntimeParams;
		}

		const CGeneratorBlueprint* CQueryBlueprint::GetGeneratorBlueprint() const
		{
			return m_pGenerator.get();
		}

		const std::vector<CInstantEvaluatorBlueprint*>& CQueryBlueprint::GetInstantEvaluatorBlueprints() const
		{
			return m_instantEvaluators;
		}

		const std::vector<CDeferredEvaluatorBlueprint*>& CQueryBlueprint::GetDeferredEvaluatorBlueprints() const
		{
			return m_deferredEvaluators;
		}

		bool CQueryBlueprint::CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(const Shared::IVariantDict& runtimeParamsToValidate, Shared::CUqsString& error) const
		{
			//
			// ensure that all required runtime-params have been passed in and that their data types match those in this query-blueprint
			//

			const std::map<string, CGlobalRuntimeParamsBlueprint::SParamInfo>& expectedRuntimeParams = m_globalRuntimeParams.GetParams();

			for (const auto& pair : expectedRuntimeParams)
			{
				const char* szKey = pair.first.c_str();
				const Client::IItemFactory* pFoundItemFactory = runtimeParamsToValidate.FindItemFactory(szKey);

				if (pFoundItemFactory == nullptr)
				{
					error.Format("Missing runtime-param: '%s'", szKey);
					return false;
				}

				const Client::IItemFactory* pExpectedItemFactory = pair.second.pItemFactory;
				assert(pExpectedItemFactory);

				if (pFoundItemFactory != pExpectedItemFactory)
				{
					error.Format("Runtime-param '%s' mismatches the item-factory: expected '%s', but received '%s'", szKey, pExpectedItemFactory->GetName(), pFoundItemFactory->GetName());
					return false;
				}
			}

			// recurse down
			for (const std::shared_ptr<CQueryBlueprint>& pChild : m_children)
			{
				if (!pChild->CheckPresenceAndTypeOfGlobalRuntimeParamsRecursively(runtimeParamsToValidate, error))
				{
					return false;
				}
			}

			return true;
		}

		const Shared::CTypeInfo* CQueryBlueprint::GetTypeOfShuttledItemsToExpect() const
		{
			assert(m_pQueryFactory);
			return m_pQueryFactory->GetTypeOfShuttledItemsToExpect(*this);
		}

		const CQueryFactoryBase& CQueryBlueprint::GetQueryFactory() const
		{
			assert(m_pQueryFactory);
			return *m_pQueryFactory;
		}

		void CQueryBlueprint::PrintToConsole(CLogger& logger) const
		{
			//
			// name + output type + max items to generate
			//

			const Shared::CTypeInfo& outputType = GetOutputType();
			logger.Printf("\"%s\" [%s] (max items in result set = %i)", m_name.c_str(), outputType.name(), m_maxItemsToKeepInResultSet);
			CLoggerIndentation _indent;

			//
			// factory
			//

			assert(m_pQueryFactory);
			logger.Printf("Query factory = %s", m_pQueryFactory->GetName());

			//
			// global constant params
			//

			m_globalConstantParams.PrintToConsole(logger);

			//
			// global runtime params
			//

			m_globalRuntimeParams.PrintToConsole(logger);

			//
			// generator (optional - typically never used by composite queries)
			//

			if (m_pGenerator)
			{
				m_pGenerator->PrintToConsole(logger);
			}
			else
			{
				logger.Printf("Generator: (none)");
			}

			//
			// instant-evaluators
			//

			if (m_instantEvaluators.empty())
			{
				logger.Printf("Instant Evaluators: (none)");
			}
			else
			{
				logger.Printf("Instant Evaluators:");
				CLoggerIndentation _indent;
				for (size_t i = 0, n = m_instantEvaluators.size(); i < n; ++i)
				{
					stack_string prefix;
					prefix.Format("#%i: ", (int)i);
					m_instantEvaluators[i]->PrintToConsole(logger, prefix.c_str());
				}
			}

			//
			// deferred-evaluators
			//

			if (m_deferredEvaluators.empty())
			{
				logger.Printf("Deferred Evaluators: (none)");
			}
			else
			{
				logger.Printf("Deferred Evaluators:");
				CLoggerIndentation _indent;
				for (size_t i = 0, n = m_deferredEvaluators.size(); i < n; ++i)
				{
					stack_string prefix;
					prefix.Format("#%i: ", (int)i);
					m_deferredEvaluators[i]->PrintToConsole(logger, prefix.c_str());
				}
			}

			//
			// children
			//

			if (m_children.empty())
			{
				logger.Printf("Children: (none)");
			}
			else
			{
				logger.Printf("Children: %i", (int)m_children.size());
				for (size_t i = 0, n = m_children.size(); i < n; ++i)
				{
					logger.Printf("Child %i:", (int)i);
					CLoggerIndentation _indent;
					m_children[i]->PrintToConsole(logger);
				}
			}
		}

		void CQueryBlueprint::SortInstantEvaluatorBlueprintsByCostAndEvaluationModality()
		{
			//
			// sort all instant-evaluator blueprints such that we end up with the following order:
			//
			//   cheap tester1
			//   cheap tester2
			//   cheap tester3
			//   ...
			//   cheap scorer1
			//   cheap scorer2
			//   cheap scorer3
			//   ...
			//   expensive tester1
			//   expensive tester2
			//   expensive tester3
			//   ...
			//   expensive scorer1
			//   expensive scorer2
			//   expensive scorer3
			//   ...
			//

			std::vector<CInstantEvaluatorBlueprint *> cheapTesters;
			std::vector<CInstantEvaluatorBlueprint *> cheapScorers;
			std::vector<CInstantEvaluatorBlueprint *> expensiveTesters;
			std::vector<CInstantEvaluatorBlueprint *> expensiveScorers;

			for (CInstantEvaluatorBlueprint* pIEBlueprint : m_instantEvaluators)
			{
				const Client::IInstantEvaluatorFactory& factory = pIEBlueprint->GetFactory();
				const Client::IInstantEvaluatorFactory::ECostCategory costCategory = factory.GetCostCategory();
				const Client::IInstantEvaluatorFactory::EEvaluationModality evaluationModality = factory.GetEvaluationModality();

				switch (costCategory)
				{
				case Client::IInstantEvaluatorFactory::ECostCategory::Cheap:
					switch (evaluationModality)
					{
					case Client::IInstantEvaluatorFactory::EEvaluationModality::Testing:
						cheapTesters.push_back(pIEBlueprint);
						break;

					case Client::IInstantEvaluatorFactory::EEvaluationModality::Scoring:
						cheapScorers.push_back(pIEBlueprint);
						break;

					default:
						assert(0);
					}
					break;

				case Client::IInstantEvaluatorFactory::ECostCategory::Expensive:
					switch (evaluationModality)
					{
					case Client::IInstantEvaluatorFactory::EEvaluationModality::Testing:
						expensiveTesters.push_back(pIEBlueprint);
						break;

					case Client::IInstantEvaluatorFactory::EEvaluationModality::Scoring:
						expensiveScorers.push_back(pIEBlueprint);
						break;

					default:
						assert(0);
					}
					break;

				default:
					assert(0);
				}
			}

			m_instantEvaluators.clear();

			m_instantEvaluators.insert(m_instantEvaluators.end(), cheapTesters.begin(), cheapTesters.end());
			m_instantEvaluators.insert(m_instantEvaluators.end(), cheapScorers.begin(), cheapScorers.end());
			m_instantEvaluators.insert(m_instantEvaluators.end(), expensiveTesters.begin(), expensiveTesters.end());
			m_instantEvaluators.insert(m_instantEvaluators.end(), expensiveScorers.begin(), expensiveScorers.end());
		}

		void CQueryBlueprint::GrabRuntimeParamsRecursively(std::map<string, Client::IItemFactory*>& out) const
		{
			const std::map<string, CGlobalRuntimeParamsBlueprint::SParamInfo>& params = m_globalRuntimeParams.GetParams();

			for (const auto& pair : params)
			{
				const char* szParamName = pair.first.c_str();
				Client::IItemFactory* pItemFactory = pair.second.pItemFactory;
				assert(pItemFactory);
				out[szParamName] = pItemFactory;	// potentially overwrite the param from a parent; we presume here that both have the same item type (this is ensured by CGlobalRuntimeParamsBlueprint::Resolve())
			}

			// recurse down all children
			for (const std::shared_ptr<CQueryBlueprint>& pChild : m_children)
			{
				pChild->GrabRuntimeParamsRecursively(out);
			}
		}

	}
}
