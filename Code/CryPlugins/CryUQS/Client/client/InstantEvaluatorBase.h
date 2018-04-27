// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Client
	{

		//===================================================================================
		//
		// CInstantEvaluatorBase<>
		//
		//===================================================================================

		template <class TInstantEvaluator, IInstantEvaluatorFactory::ECostCategory tCostCategory, IInstantEvaluatorFactory::EEvaluationModality tEvaluationModality>
		class CInstantEvaluatorBase : public IInstantEvaluator
		{
		public:
			static const IInstantEvaluatorFactory::ECostCategory         kCostCategory = tCostCategory;              // for CInstantEvaluatorFactory<>::GetCostCategory()
			static const IInstantEvaluatorFactory::EEvaluationModality   kEvaluationModality = tEvaluationModality;  // for CInstantEvaluatorFactory<>::GetEvaluationModality()

		public:
			virtual ERunStatus                                           Run(const SRunContext& runContext, const void* pParams) const override final;
		};

		template <class TInstantEvaluator, IInstantEvaluatorFactory::ECostCategory tCostCategory, IInstantEvaluatorFactory::EEvaluationModality tEvaluationModality>
		IInstantEvaluator::ERunStatus CInstantEvaluatorBase<TInstantEvaluator, tCostCategory, tEvaluationModality>::Run(const SRunContext& runContext, const void* pParams) const
		{
			const TInstantEvaluator* pActualEvaluator = static_cast<const TInstantEvaluator*>(this);
			const typename TInstantEvaluator::SParams* pActualParams = static_cast<const typename TInstantEvaluator::SParams*>(pParams);
			return pActualEvaluator->DoRun(runContext, *pActualParams);
		}

	}
}
