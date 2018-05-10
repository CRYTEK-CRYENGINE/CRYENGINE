// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// CEvaluatorBlueprintBase - abstract base class for CInstantEvaluatorBlueprint and CDeferredEvaluatorBlueprint
		//
		//===================================================================================

		class CEvaluatorBlueprintBase : public CBlueprintWithInputs
		{
		public:

			bool                                            Resolve(const ITextualEvaluatorBlueprint& source, const CQueryBlueprint& queryBlueprintForGlobalParamChecking);
			float                                           GetWeight() const;
			const CEvaluationResultTransform&               GetEvaluationResultTransform() const;

		protected:

			explicit                                        CEvaluatorBlueprintBase();

		private:

			                                                UQS_NON_COPYABLE(CEvaluatorBlueprintBase);

			// these are helpers that will be called by Resolve() and can only be implemented by derived classes (they deal with the underlying evaluator factory which this base class doesn't know about)
			virtual bool                                    ResolveFactory(const ITextualEvaluatorBlueprint& source) = 0;
			virtual const Client::IInputParameterRegistry&  GetInputParameterRegistry() const = 0;

		private:

			float                                           m_weight;
			CEvaluationResultTransform                      m_evaluationResultTransform;
		};

	}
}
