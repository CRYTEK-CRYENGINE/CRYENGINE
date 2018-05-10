// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		CEvaluatorBlueprintBase::CEvaluatorBlueprintBase()
			: m_weight(1.0f)
			, m_evaluationResultTransform()
		{
			// nothing
		}

		bool CEvaluatorBlueprintBase::Resolve(const ITextualEvaluatorBlueprint& source, const CQueryBlueprint& queryBlueprintForGlobalParamChecking)
		{
			//
			// evaluator factory
			//

			if (!ResolveFactory(source))
			{
				return false;
			}

			//
			// weight
			//

			m_weight = source.GetWeight();

			//
			// score transform: first search by its GUID, then by its name
			//

			const CryGUID& scoreTransformGUID = source.GetScoreTransformGUID();
			const char* szScoreTransformName = source.GetScoreTransformName();
			const IScoreTransformFactory* pScoreTransformFactory;

			if (!(pScoreTransformFactory = g_pHub->GetScoreTransformFactoryDatabase().FindFactoryByGUID(scoreTransformGUID)))
			{
				if (szScoreTransformName[0] != '\0')  // the concept of a score-transforms has been added after some query blueprints were already around, so these may still contain an empty name by default now
				{
					if (!(pScoreTransformFactory = g_pHub->GetScoreTransformFactoryDatabase().FindFactoryByName(szScoreTransformName)))
					{
						if (DataSource::ISyntaxErrorCollector* pSE = source.GetSyntaxErrorCollector())
						{
							Shared::CUqsString guidAsString;
							Shared::Internal::CGUIDHelper::ToString(scoreTransformGUID, guidAsString);
							pSE->AddErrorMessage("Unknown ScoreTransformFactory: GUID = %s, name = '%s'", guidAsString.c_str(), szScoreTransformName);
						}
						return false;
					}
				}
			}

			if (pScoreTransformFactory) // otherwise just keep the default score-transform that has been set in m_evaluationResultTransform's ctor
			{
				m_evaluationResultTransform.SetScoreTransformType(pScoreTransformFactory->GetScoreTransformType());
			}

			//
			// negate-discard setting
			//

			m_evaluationResultTransform.SetNegateDiscard(source.GetNegateDiscard());

			//
			// input parameters
			//

			CInputBlueprint inputRoot;
			const ITextualInputBlueprint& textualInputRoot = source.GetInputRoot();
			const Client::IInputParameterRegistry& inputParamsReg = GetInputParameterRegistry();

			if (!inputRoot.Resolve(textualInputRoot, inputParamsReg, queryBlueprintForGlobalParamChecking, false))
			{
				return false;
			}

			ResolveInputs(inputRoot);

			return true;
		}

		float CEvaluatorBlueprintBase::GetWeight() const
		{
			return m_weight;
		}

		const CEvaluationResultTransform& CEvaluatorBlueprintBase::GetEvaluationResultTransform() const
		{
			return m_evaluationResultTransform;
		}

	}
}
