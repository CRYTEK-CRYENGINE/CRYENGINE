// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CryAISystem/INavigationSystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// CInstantEvaluator_TestMinDistance
		//
		// - discards the item currently being evaluated if the distance between 2 points is smaller than a certain amount
		//
		//===================================================================================

		class CInstantEvaluator_TestMinDistance : public Client::CInstantEvaluatorBase<
			CInstantEvaluator_TestMinDistance,
			Client::IInstantEvaluatorFactory::ECostCategory::Cheap,
			Client::IInstantEvaluatorFactory::EEvaluationModality::Testing
		>
		{
		public:
			struct SParams
			{
				Pos3      pos1;
				Pos3      pos2;
				float     minRequiredDistance;

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("pos1", pos1, "POS1", "");
					UQS_EXPOSE_PARAM("pos2", pos2, "POS2", "");
					UQS_EXPOSE_PARAM("minRequiredDistance", minRequiredDistance, "MIND", "Minimally required distance between Pos1 and Pos2. If smaller then the item will get discarded.");
				UQS_EXPOSE_PARAMS_END
			};

			ERunStatus    DoRun(const SRunContext& runContext, const SParams& params) const;
		};

		//===================================================================================
		//
		// CInstantEvaluator_TestMaxDistance
		//
		// - discards the item currently being evaluated if the distance between 2 points is larger than a certain amount
		//
		//===================================================================================

		class CInstantEvaluator_TestMaxDistance : public Client::CInstantEvaluatorBase<
			CInstantEvaluator_TestMaxDistance,
			Client::IInstantEvaluatorFactory::ECostCategory::Cheap,
			Client::IInstantEvaluatorFactory::EEvaluationModality::Testing
		>
		{
		public:
			struct SParams
			{
				Pos3      pos1;
				Pos3      pos2;
				float     maxAllowedDistance;

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("pos1", pos1, "POS1", "");
					UQS_EXPOSE_PARAM("pos2", pos2, "POS2", "");
					UQS_EXPOSE_PARAM("maxAllowedDistance", maxAllowedDistance, "MAXD", "Maximally allowed distance between Pos1 and Pos3. If larger then the item will get discarded.");
				UQS_EXPOSE_PARAMS_END
			};

			ERunStatus    DoRun(const SRunContext& runContext, const SParams& params) const;
		};

		//===================================================================================
		//
		// CInstantEvaluator_TestLocationInNavMesh
		//
		// - checks for whether a given Pos3 resides in the NavMesh (roughly on a walkable surface)
		// - discards the item if it's not on the NavMesh
		//
		//===================================================================================

		class CInstantEvaluator_TestLocationInNavMesh : public Client::CInstantEvaluatorBase<
			CInstantEvaluator_TestLocationInNavMesh,
			Client::IInstantEvaluatorFactory::ECostCategory::Expensive,
			Client::IInstantEvaluatorFactory::EEvaluationModality::Testing
		>
		{
		public:
			struct SParams
			{
				Pos3                    locationToTest;
				NavigationAgentTypeID   navigationAgentTypeID;

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("locationToTest", locationToTest, "LOCA", "Rough location in the Navigatin Mesh to test.");
					UQS_EXPOSE_PARAM("navigationAgentTypeID", navigationAgentTypeID, "AGEN", "Agent type used to pick the according Navigation Mesh layer for testing.");
				UQS_EXPOSE_PARAMS_END
			};

		public:
			explicit                    CInstantEvaluator_TestLocationInNavMesh();
			ERunStatus                  DoRun(const SRunContext& runContext, const SParams& params) const;

		private:
			INavigationSystem*          m_pNavSys;
		};

		//===================================================================================
		//
		// CInstantEvaluator_ScoreDistance
		//
		// - scores the distance between 2 points
		// - the higher the distance the better the score
		// - a threshold is used as a "reference distance" for increasing the score linearly; larger distances clamp the score to 1.0
		//
		//===================================================================================

		class CInstantEvaluator_ScoreDistance : public Client::CInstantEvaluatorBase<
			CInstantEvaluator_ScoreDistance,
			Client::IInstantEvaluatorFactory::ECostCategory::Cheap,
			Client::IInstantEvaluatorFactory::EEvaluationModality::Scoring
		>
		{
		public:
			struct SParams
			{
				Pos3      pos1;
				Pos3      pos2;
				float     distanceThreshold;

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("pos1", pos1, "POS1", "");
					UQS_EXPOSE_PARAM("pos2", pos2, "POS2", "");
					UQS_EXPOSE_PARAM("distanceThreshold", distanceThreshold, "MAXD", "Clamping distance to map between 0.0 and 1.0");
				UQS_EXPOSE_PARAMS_END
			};

			ERunStatus    DoRun(const SRunContext& runContext, const SParams& params) const;
		};

		//===================================================================================
		//
		// CInstantEvaluator_ScoreDistanceInverse
		//
		// - scores the distance between 2 points
		// - the shorter the distance the better the score
		// - a threshold is used as a "reference distance" for decreasing the score linearly; larger distances clamp the score to 0.0
		//
		//===================================================================================

		class CInstantEvaluator_ScoreDistanceInverse : public Client::CInstantEvaluatorBase<
			CInstantEvaluator_ScoreDistanceInverse,
			Client::IInstantEvaluatorFactory::ECostCategory::Cheap,
			Client::IInstantEvaluatorFactory::EEvaluationModality::Scoring
		>
		{
		public:
			struct SParams
			{
				Pos3      pos1;
				Pos3      pos2;
				float     distanceThreshold;

				UQS_EXPOSE_PARAMS_BEGIN
					UQS_EXPOSE_PARAM("pos1", pos1, "POS1", "");
					UQS_EXPOSE_PARAM("pos2", pos2, "POS2", "");
					UQS_EXPOSE_PARAM("distanceThreshold", distanceThreshold, "MAXD", "Clamping distance to map between 0.0 and 1.0");
				UQS_EXPOSE_PARAMS_END
			};

			ERunStatus    DoRun(const SRunContext& runContext, const SParams& params) const;
		};

		//===================================================================================
		//
		// CInstantEvaluator_ScoreRandom
		//
		// - provides a random score between [0.0 .. 1.0]
		//
		//===================================================================================

		class CInstantEvaluator_ScoreRandom : public Client::CInstantEvaluatorBase<
			CInstantEvaluator_ScoreRandom,
			Client::IInstantEvaluatorFactory::ECostCategory::Cheap,
			Client::IInstantEvaluatorFactory::EEvaluationModality::Scoring
		>
		{
		public:
			struct SParams
			{
				// nothing
				UQS_EXPOSE_PARAMS_BEGIN
				UQS_EXPOSE_PARAMS_END
			};

			ERunStatus    DoRun(const SRunContext& runContext, const SParams& params) const;
		};

	}
}
