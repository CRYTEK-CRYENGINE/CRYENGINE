#include "StdAfx.h"

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CEvaluationResultTransform
		//
		//===================================================================================

		CEvaluationResultTransform::CEvaluationResultTransform()
			: m_scoreTransformType(CScoreTransformFactory::GetDefaultScoreTransformFactory().GetScoreTransformType())
			, m_bNegateDiscard(false)
		{
			// nothing
		}

		void CEvaluationResultTransform::SetScoreTransformType(EScoreTransformType scoreTransformType)
		{
			m_scoreTransformType = scoreTransformType;
		}

		void CEvaluationResultTransform::SetNegateDiscard(bool bNegateDiscard)
		{
			m_bNegateDiscard = bNegateDiscard;
		}

		EScoreTransformType CEvaluationResultTransform::GetScoreTransformType() const
		{
			return m_scoreTransformType;
		}

		bool CEvaluationResultTransform::GetNegateDiscard() const
		{
			return m_bNegateDiscard;
		}

		void CEvaluationResultTransform::TransformItemEvaluationResult(SItemEvaluationResult& evaluationResultToTransform) const
		{
			//
			// apply the score transform
			//

			switch (m_scoreTransformType)
			{
			case EScoreTransformType::Linear:
				// nothing (leave the score as it is)
				break;

			case EScoreTransformType::LinearInverse:
				evaluationResultToTransform.score = 1.0f - evaluationResultToTransform.score;
				break;

			case EScoreTransformType::Sine180:
				evaluationResultToTransform.score = std::sin(evaluationResultToTransform.score * gf_PI);
				break;

			case EScoreTransformType::Sine180Inverse:
				evaluationResultToTransform.score = 1.0f - std::sin(evaluationResultToTransform.score * gf_PI);
				break;

			case EScoreTransformType::Square:
				evaluationResultToTransform.score *= evaluationResultToTransform.score;
				break;

			case EScoreTransformType::SquareInverse:
				evaluationResultToTransform.score = 1.0f - sqr(evaluationResultToTransform.score);
				break;

			default:
				assert(0);
				break;
			}

			//
			// apply the discard-setting
			//

			if (m_bNegateDiscard)
			{
				evaluationResultToTransform.bDiscardItem = !evaluationResultToTransform.bDiscardItem;
			}
		}

	}
}
