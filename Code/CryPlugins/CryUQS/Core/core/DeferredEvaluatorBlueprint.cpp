// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeferredEvaluatorBlueprint.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CDeferredEvaluatorBlueprint
		//
		//===================================================================================

		CDeferredEvaluatorBlueprint::CDeferredEvaluatorBlueprint()
			: m_pDeferredEvaluatorFactory(nullptr)
		{
			// nothing
		}

		Client::IDeferredEvaluatorFactory& CDeferredEvaluatorBlueprint::GetFactory() const
		{
			assert(m_pDeferredEvaluatorFactory);
			return *m_pDeferredEvaluatorFactory;
		}

		void CDeferredEvaluatorBlueprint::PrintToConsole(CLogger& logger, const char* szMessagePrefix) const
		{
			const CEvaluationResultTransform& evaluationResultTransform = GetEvaluationResultTransform();
			const EScoreTransformType scoreTransformType = evaluationResultTransform.GetScoreTransformType();
			const IScoreTransformFactory* pScoreTransformFactory = g_pHub->GetScoreTransformFactoryDatabase().FindFactoryByCallback([scoreTransformType](const IScoreTransformFactory& scoreTransformFactory) { return scoreTransformFactory.GetScoreTransformType() == scoreTransformType; });

			logger.Printf("%s%s (weight = %f, scoreTransform = %s, negateDiscard = %s)",
				szMessagePrefix,
				m_pDeferredEvaluatorFactory->GetName(),
				GetWeight(),
				pScoreTransformFactory ? pScoreTransformFactory->GetName() : "(unknown scoreTransform - cannot happen)",
				evaluationResultTransform.GetNegateDiscard() ? "true" : "false"
			);

			//logger.Printf("%s%s (weight = %f)", szMessagePrefix, m_pDeferredEvaluatorFactory->GetName(), GetWeight());
		}

		bool CDeferredEvaluatorBlueprint::ResolveFactory(const ITextualEvaluatorBlueprint& source)
		{
			const CryGUID& evaluatorGUID = source.GetEvaluatorGUID();
			const char* szEvaluatorName = source.GetEvaluatorName();

			//
			// deferred-evaluator factory: first search by its GUID, then by its name
			//

			if (!(m_pDeferredEvaluatorFactory = g_pHub->GetDeferredEvaluatorFactoryDatabase().FindFactoryByGUID(evaluatorGUID)))
			{
				if (!(m_pDeferredEvaluatorFactory = g_pHub->GetDeferredEvaluatorFactoryDatabase().FindFactoryByName(szEvaluatorName)))
				{
					if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
					{
						Shared::CUqsString guidAsString;
						Shared::Internal::CGUIDHelper::ToString(evaluatorGUID, guidAsString);
						pSE->AddErrorMessage("Unknown DeferredEvaluatorFactory: GUID = %s, name = '%s'", guidAsString.c_str(), szEvaluatorName);
					}
					return false;
				}
			}

			return true;
		}

		const Client::IInputParameterRegistry& CDeferredEvaluatorBlueprint::GetInputParameterRegistry() const
		{
			assert(m_pDeferredEvaluatorFactory);
			return m_pDeferredEvaluatorFactory->GetInputParameterRegistry();
		}

	}
}
