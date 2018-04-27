// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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

		class CEvaluationResultTransform
		{
		public:

			explicit             CEvaluationResultTransform();

			// called by the resolving stage of instant- and deferred-evaluator blueprints
			// FIXME: it would be more elegant if we had decoupled this into something like an ITextualEvaluationResultTransformBlueprint from which
			//        we could resolve and which would be owned by a ITextualInstantEvaluatorBlueprint and ITextualDeferredEvaluatorBlueprint
			void                 SetScoreTransformType(EScoreTransformType scoreTransformType);
			void                 SetNegateDiscard(bool bNegateDiscard);

			// for debug purpose only when listing all instant- and deferred-evaluators
			EScoreTransformType  GetScoreTransformType() const;
			bool                 GetNegateDiscard() const;

			// called at runtime just after an evaluator has finished evaluating the item and before weighting the item's score
			void                 TransformItemEvaluationResult(SItemEvaluationResult& evaluationResultToTransform) const;

		private:

			EScoreTransformType  m_scoreTransformType;
			bool                 m_bNegateDiscard;
		};

	}
}
