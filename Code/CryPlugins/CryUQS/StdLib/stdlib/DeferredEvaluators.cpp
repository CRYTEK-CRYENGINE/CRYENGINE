// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryRenderer/IRenderer.h>
#include <CryPhysics/physinterface.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void CStdLibRegistration::InstantiateDeferredEvaluatorFactoriesForRegistration()
		{
			{
				Client::CDeferredEvaluatorFactory<CDeferredEvaluator_TestRaycast>::SCtorParams ctorParams;

				ctorParams.szName = "std::TestRaycast";
				ctorParams.guid = "e8d294e5-ab1f-40ce-abc1-9b6fc687c990"_cry_guid;
				ctorParams.szDescription =
					"Tests a raycast between 2 given positions.\n"
					"Whether success or failure of the raycast counts as overall success or failure of the evaluator can be specified by a parameter.\n"
					"NOTICE: The underlying raycaster uses the *renderer* to limit the number of raycasts per frame!\n"
					"As such, it will not work on a dedicated server as there is no renderer (!) (Read: a null-pointer crash would occur!)\n"
					"You should rather consider this evaluator as a reference for your own implementation.";

				static const Client::CDeferredEvaluatorFactory<CDeferredEvaluator_TestRaycast> deferredEvaluatorFactory_TestRaycast(ctorParams);
			}
		}

		//===================================================================================
		//
		// CDeferredEvaluator_TestRaycast::CRaycastRegulator
		//
		//===================================================================================

		CDeferredEvaluator_TestRaycast::CRaycastRegulator::CRaycastRegulator(int maxRequestsPerSecond)
			: m_maxRequestsPerSecond(maxRequestsPerSecond)
			, m_timeLastFiredRaycast(0)
		{}

		bool CDeferredEvaluator_TestRaycast::CRaycastRegulator::RequestRaycast()
		{
			const float currentTime = gEnv->pTimer->GetAsyncCurTime();
			bool bRaycastAllowed = false;

			if (!m_timeLastFiredRaycast || (currentTime - m_timeLastFiredRaycast) >= 1.0f / (float) m_maxRequestsPerSecond)
			{
				bRaycastAllowed = true;
				m_timeLastFiredRaycast = currentTime;
			}

			return bRaycastAllowed;
		}

		//===================================================================================
		//
		// CDeferredEvaluator_TestRaycast
		//
		//===================================================================================

		CDeferredEvaluator_TestRaycast::CRaycastRegulator CDeferredEvaluator_TestRaycast::s_regulator(360);  // allow up to 360 raycasts per second, which amounts to 12 raycasts per frame at 30 FPS

		CDeferredEvaluator_TestRaycast::CDeferredEvaluator_TestRaycast(const SParams& params)
			: m_params(params)
		{
			// nothing
		}

		Client::IDeferredEvaluator::EUpdateStatus CDeferredEvaluator_TestRaycast::Update(const SUpdateContext& updateContext)
		{
			if (s_regulator.RequestRaycast())
			{
				//
				// perform the raycast
				//

				// these constants are exactly the same as the ones in <IVisionMap.h> [2016-08-16]
				const int raycastPierceability = 10;
				const int objectTypesToTestAgainst = ent_terrain | ent_static | ent_rigid | ent_sleeping_rigid;
				const int blockingHardColliders = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (raycastPierceability & rwi_pierceability_mask);
				const int blockingSoftColliders = (geom_colltype14 << rwi_colltype_bit);

				ray_hit singleHit;

				IPhysicalWorld::SRWIParams params;
				params.org = m_params.from.value;
				params.dir = m_params.to.value - m_params.from.value;
				params.objtypes = objectTypesToTestAgainst;
				params.flags = blockingHardColliders | blockingSoftColliders;
				params.hits = &singleHit;
				params.nMaxHits = 1;
				params.pSkipEnts = nullptr;
				params.nSkipEnts = 0;

				const bool bHitSomething = (gEnv->pPhysicalWorld->RayWorldIntersection(params) > 0);

				updateContext.evaluationResult.bDiscardItem = (bHitSomething == m_params.raycastShallSucceed);

				// persistent debug drawing of the ray
				IF_UNLIKELY(updateContext.blackboard.pDebugRenderWorldPersistent)
				{
					if (bHitSomething)
					{
						// yellow line: start pos -> impact
						// red line:    impact -> end pos
						updateContext.blackboard.pDebugRenderWorldPersistent->AddLine(m_params.from.value, singleHit.pt, Col_Yellow);
						updateContext.blackboard.pDebugRenderWorldPersistent->AddLine(singleHit.pt, m_params.to.value, Col_Red);
					}
					else
					{
						// green line: start pos -> end pos
						updateContext.blackboard.pDebugRenderWorldPersistent->AddLine(m_params.from.value, m_params.to.value, Col_Green);
					}
				}

				return EUpdateStatus::Finished;
			}
			else
			{
				//
				// we're still short on resources
				//

				// draw a white line to indicate that we're still waiting to start the raycast
				IF_UNLIKELY(updateContext.blackboard.pDebugRenderWorldImmediate)
				{
					updateContext.blackboard.pDebugRenderWorldImmediate->DrawLine(m_params.from.value, m_params.to.value, Col_White);
				}

				return EUpdateStatus::BusyButBlockedDueToResourceShortage;
			}
		}

	}
}
