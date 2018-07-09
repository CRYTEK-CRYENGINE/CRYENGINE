// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "InstantEvaluatorBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CInstantEvaluatorBlueprint
		//
		//===================================================================================

		CInstantEvaluatorBlueprint::CInstantEvaluatorBlueprint()
			: m_pInstantEvaluatorFactory(nullptr)
		{
			// nothing
		}

		Client::IInstantEvaluatorFactory& CInstantEvaluatorBlueprint::GetFactory() const
		{
			assert(m_pInstantEvaluatorFactory);
			return *m_pInstantEvaluatorFactory;
		}

		void CInstantEvaluatorBlueprint::PrintToConsole(CLogger& logger, const char* szMessagePrefix) const
		{
			const char* szCost;
			const char* szModality;

			switch (m_pInstantEvaluatorFactory->GetCostCategory())
			{
			case Client::IInstantEvaluatorFactory::ECostCategory::Cheap:
				szCost = "cheap";
				break;

			case Client::IInstantEvaluatorFactory::ECostCategory::Expensive:
				szCost = "expensive";
				break;

			default:
				szCost = "? (programming error)";
				break;
			}

			switch (m_pInstantEvaluatorFactory->GetEvaluationModality())
			{
			case Client::IInstantEvaluatorFactory::EEvaluationModality::Testing:
				szModality = "tester";
				break;

			case Client::IInstantEvaluatorFactory::EEvaluationModality::Scoring:
				szModality = "scorer";
				break;

			default:
				szModality = "? (programming error)";
				break;
			}

			const CEvaluationResultTransform& evaluationResultTransform = GetEvaluationResultTransform();
			const EScoreTransformType scoreTransformType = evaluationResultTransform.GetScoreTransformType();
			const IScoreTransformFactory* pScoreTransformFactory = g_pHub->GetScoreTransformFactoryDatabase().FindFactoryByCallback([scoreTransformType](const IScoreTransformFactory& scoreTransformFactory) { return scoreTransformFactory.GetScoreTransformType() == scoreTransformType; });

			logger.Printf("%s%s [%s %s] (weight = %f, scoreTransform = %s, negateDiscard = %s)",
				szMessagePrefix,
				m_pInstantEvaluatorFactory->GetName(),
				szCost,
				szModality,
				GetWeight(),
				pScoreTransformFactory ? pScoreTransformFactory->GetName() : "(unknown scoreTransform - cannot happen)",
				evaluationResultTransform.GetNegateDiscard() ? "true" : "false"
			);
		}

		bool CInstantEvaluatorBlueprint::ResolveFactory(const ITextualEvaluatorBlueprint& source)
		{
			const CryGUID& evaluatorGUID = source.GetEvaluatorGUID();
			const char* szEvaluatorName = source.GetEvaluatorName();

			//
			// instant-evaluator factory: first search by its GUID, then by its name
			//

			if (!(m_pInstantEvaluatorFactory = g_pHub->GetInstantEvaluatorFactoryDatabase().FindFactoryByGUID(evaluatorGUID)))
			{
				if (!(m_pInstantEvaluatorFactory = g_pHub->GetInstantEvaluatorFactoryDatabase().FindFactoryByName(szEvaluatorName)))
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						Shared::CUqsString guidAsString;
						Shared::Internal::CGUIDHelper::ToString(evaluatorGUID, guidAsString);
						pSE->AddErrorMessage("Unknown InstantEvaluatorFactory: GUID = %s, name = '%s'", guidAsString.c_str(), szEvaluatorName);
					}
					return false;
				}
			}

			return true;
		}

		const Client::IInputParameterRegistry& CInstantEvaluatorBlueprint::GetInputParameterRegistry() const
		{
			assert(m_pInstantEvaluatorFactory);
			return m_pInstantEvaluatorFactory->GetInputParameterRegistry();
		}

	}
}
