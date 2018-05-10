// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace Core
	{

		//===================================================================================
		//
		// SItemEvaluationResult
		//
		// - result of having evaluated a single item by a single evaluator
		//
		//===================================================================================

		struct SItemEvaluationResult
		{
			explicit       SItemEvaluationResult();
			float          score;		// [0.0 .. 1.0] (not yet multiplied by the weight!)
			bool           bDiscardItem;
		};

		inline SItemEvaluationResult::SItemEvaluationResult()
			: score(1.0f)	// Important: keep this at 1.0f (the highest possible score) to prevent filter-only evaluators from getting their future outcome overestimated (which would kill optimization efforts!)
			, bDiscardItem(false)
		{}

	}
}
