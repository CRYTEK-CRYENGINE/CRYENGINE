// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>

#include <CryMath/Random.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void CStdLibRegistration::InstantiateInstantEvaluatorFactoriesForRegistration()
		{
			{
				Client::CInstantEvaluatorFactory<CInstantEvaluator_TestMinDistance>::SCtorParams ctorParams;

				ctorParams.szName = "std::TestMinDistance";
				ctorParams.guid = "1913cd53-ba27-4285-a901-fa83997c681f"_cry_guid;
				ctorParams.szDescription = "Discards the item currently being evaluated if the distance between 2 points is smaller than a certain amount.";

				static const Client::CInstantEvaluatorFactory<CInstantEvaluator_TestMinDistance> instantEvaluatorFactory_TestMinDistance(ctorParams);
			}

			{
				Client::CInstantEvaluatorFactory<CInstantEvaluator_TestMaxDistance>::SCtorParams ctorParams;

				ctorParams.szName = "std::TestMaxDistance";
				ctorParams.guid = "46cb5dcd-aacf-4b6c-b30f-5e678dbac707"_cry_guid;
				ctorParams.szDescription = "Discards the item currently being evaluated if the distance between 2 points is larger than a certain amount.";

				static const Client::CInstantEvaluatorFactory<CInstantEvaluator_TestMaxDistance> instantEvaluatorFactory_TestMaxDistance(ctorParams);
			}

			{
				Client::CInstantEvaluatorFactory<CInstantEvaluator_TestLocationInNavMesh>::SCtorParams ctorParams;

				ctorParams.szName = "std::TestLocationInNavMesh";
				ctorParams.guid = "1c62cf8c-09f1-4ee8-a0e5-bd2a6c6c44e3"_cry_guid;
				ctorParams.szDescription = "Checks for whether a given Pos3 resides in the NavMesh(roughly on a walkable surface).\nDiscards the item if it's not on the NavMesh.";

				static const Client::CInstantEvaluatorFactory<CInstantEvaluator_TestLocationInNavMesh> instantEvaluatorFactory_TestLocationInNavMesh(ctorParams);
			}

			{
				Client::CInstantEvaluatorFactory<CInstantEvaluator_ScoreDistance>::SCtorParams ctorParams;

				ctorParams.szName = "std::ScoreDistance";
				ctorParams.guid = "fc41cc13-3967-462a-b464-b61af028f613"_cry_guid;
				ctorParams.szDescription = "Scores the distance between 2 points.\nThe higher the distance the better the score.\nA threshold is used as a 'reference distance' for increasing the score linearly; distances larger than that will clamp the score to 1.0.";

				static const Client::CInstantEvaluatorFactory<CInstantEvaluator_ScoreDistance> instantEvaluatorFactory_ScoreDistance(ctorParams);
			}

			{
				Client::CInstantEvaluatorFactory<CInstantEvaluator_ScoreDistanceInverse>::SCtorParams ctorParams;

				ctorParams.szName = "std::ScoreDistanceInverse";
				ctorParams.guid = "78a52467-32a0-4bcd-918a-df7db9d2b715"_cry_guid;
				ctorParams.szDescription = "Scores the distance between 2 points.\nThe shorter the distance the better the score.\nA threshold is used as a 'reference distance' for decreasing the score linearly; distances larger than that will clamp the score to 0.0.";

				static const Client::CInstantEvaluatorFactory<CInstantEvaluator_ScoreDistanceInverse> instantEvaluatorFactory_ScoreDistanceInverse(ctorParams);
			}

			{
				Client::CInstantEvaluatorFactory<CInstantEvaluator_ScoreRandom>::SCtorParams ctorParams;

				ctorParams.szName = "std::ScoreRandom";
				ctorParams.guid = "ff8bd552-db0d-47a2-9238-14f5e1eeedbf"_cry_guid;
				ctorParams.szDescription = "Provides a random score between [0.0 .. 1.0].";

				static const Client::CInstantEvaluatorFactory<CInstantEvaluator_ScoreRandom> instantEvaluatorFactory_ScoreRandom(ctorParams);
			}
		}

		//===================================================================================
		//
		// CInstantEvaluator_TestMinDistance
		//
		//===================================================================================

		Client::IInstantEvaluator::ERunStatus CInstantEvaluator_TestMinDistance::DoRun(const SRunContext& runContext, const SParams& params) const
		{
			const float minRequiredDistanceSqr = sqr(params.minRequiredDistance);
			const float actualDistanceSqr = (params.pos1.value - params.pos2.value).GetLengthSquared();

			if (actualDistanceSqr < minRequiredDistanceSqr)
			{
				runContext.evaluationResult.bDiscardItem = true;
			}

			return ERunStatus::Finished;
		}

		//===================================================================================
		//
		// CInstantEvaluator_TestMaxDistance
		//
		//===================================================================================

		Client::IInstantEvaluator::ERunStatus CInstantEvaluator_TestMaxDistance::DoRun(const SRunContext& runContext, const SParams& params) const
		{
			const float maxAllowedDistanceSqr = sqr(params.maxAllowedDistance);
			const float actualDistanceSqr = (params.pos1.value - params.pos2.value).GetLengthSquared();

			if (actualDistanceSqr > maxAllowedDistanceSqr)
			{
				runContext.evaluationResult.bDiscardItem = true;
			}

			return ERunStatus::Finished;
		}

		//===================================================================================
		//
		// CInstantEvaluator_TestLocationInNavMesh
		//
		//===================================================================================

		CInstantEvaluator_TestLocationInNavMesh::CInstantEvaluator_TestLocationInNavMesh()
		{
			m_pNavSys = gEnv->pAISystem->GetNavigationSystem();
			assert(m_pNavSys);
		}

		Client::IInstantEvaluator::ERunStatus CInstantEvaluator_TestLocationInNavMesh::DoRun(const SRunContext& runContext, const SParams& params) const
		{
			//TODO: Get Navmesh query filter from somewhere
			INavMeshQueryFilter* pQueryFilter = nullptr;
			const bool bIsInNavMesh = m_pNavSys->IsLocationValidInNavigationMesh(params.navigationAgentTypeID, params.locationToTest.value, pQueryFilter);
			runContext.evaluationResult.bDiscardItem = !bIsInNavMesh;
			return ERunStatus::Finished;
		}

		//===================================================================================
		//
		// CInstantEvaluator_ScoreDistance
		//
		//===================================================================================

		Client::IInstantEvaluator::ERunStatus CInstantEvaluator_ScoreDistance::DoRun(const SRunContext& runContext, const SParams& params) const
		{
			// guard against potential div-by-zero and other weird behavior
			IF_UNLIKELY(params.distanceThreshold <= 0.0f)
			{
				runContext.error.Format("param 'distanceThreshold' should be > 0.0 (but is: %f)", params.distanceThreshold);
				return ERunStatus::ExceptionOccurred;
			}

			const float distanceBetweenPoints = (params.pos1.value - params.pos2.value).GetLength();
			runContext.evaluationResult.score = std::min(distanceBetweenPoints, params.distanceThreshold) / params.distanceThreshold;
			return ERunStatus::Finished;
		}

		//===================================================================================
		//
		// CInstantEvaluator_ScoreDistanceInverse
		//
		//===================================================================================

		Client::IInstantEvaluator::ERunStatus CInstantEvaluator_ScoreDistanceInverse::DoRun(const SRunContext& runContext, const SParams& params) const
		{
			// guard against potential div-by-zero and other weird behavior
			IF_UNLIKELY(params.distanceThreshold <= 0.0f)
			{
				runContext.error.Format("param 'distanceThreshold' should be > 0.0 (but is: %f)", params.distanceThreshold);
				return ERunStatus::ExceptionOccurred;
			}

			const float distanceBetweenPoints = (params.pos1.value - params.pos2.value).GetLength();
			runContext.evaluationResult.score = 1.0f - std::min(distanceBetweenPoints, params.distanceThreshold) / params.distanceThreshold;
			return ERunStatus::Finished;
		}

		//===================================================================================
		//
		// CInstantEvaluator_ScoreRandom
		//
		//===================================================================================

		Client::IInstantEvaluator::ERunStatus CInstantEvaluator_ScoreRandom::DoRun(const SRunContext& runContext, const SParams& params) const
		{
			runContext.evaluationResult.score = cry_random(0.0f, 1.0f);
			return ERunStatus::Finished;
		}

	}
}
