// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/* Notes
   MTJ 22/07/07

   A fair bit of the code is a simple parser for the query language
   It doesn't yet have a well-rounded error reporting system
   It reports warnings immediately on failure conditions and then OK booleans trickle
   down to generate an error in the original query request

   MTJ 11/09/07 - TODO: Refactoring the queries themselves into a separate class to the system would be sensible

   MTJ 30/06/09 - QueryContext's IAIActor should be a CAIActor internally
 */

#include "StdAfx.h"
#include "TacticalPointSystem.h"
#include "TacticalPointQuery.h"
#include "TacticalPointQueryEnum.h"

#include <algorithm>

// For persistent debugging
#include <CryGame/IGameFramework.h>

#include "../Cover/CoverSystem.h"
#include "../Cover/CoverSurface.h"

#include "DebugDrawContext.h"
#include "StatsManager.h"
#include "Puppet.h"

#include "Navigation/NavigationSystem/NavigationSystem.h"

// Maximum time that an async query can execute before it is aborted as an error
const float MAX_SYNC_TIME_MS = 20;

CTacticalPointSystem::CVarsDef CTacticalPointSystem::CVars;

//----------------------------------------------------------------------------------------------//

CTacticalPointSystem::CTacticalPointSystem()
{
	// Initialise the word/token list
	// Please keep exactly the same order as the enum, wherein order is essential

	m_CostMap[eTPQC_CHEAP] = 1;
	m_CostMap[eTPQC_MEDIUM] = 256;
	m_CostMap[eTPQC_EXPENSIVE] = 1024;
	m_CostMap[eTPQC_DEFERRED] = -1024;

	m_GameQueryIdMap[eTPQT_PROP_BOOL] = eTPQ_GAMESTART_PROP_BOOL;
	m_GameQueryIdMap[eTPQT_PROP_REAL] = eTPQ_GAMESTART_PROP_REAL;
	m_GameQueryIdMap[eTPQT_TEST] = eTPQ_GAMESTART_TEST;
	m_GameQueryIdMap[eTPQT_MEASURE] = eTPQ_GAMESTART_MEASURE;
	m_GameQueryIdMap[eTPQT_GENERATOR] = eTPQ_GAMESTART_GENERATOR;
	m_GameQueryIdMap[eTPQT_GENERATOR_O] = eTPQ_GAMESTART_GENERATOR_O;
	m_GameQueryIdMap[eTPQT_PRIMARY_OBJECT] = eTPQ_GAMESTART_PRIMARY_OBJECT;

	// Misc
	m_mTokenToString[eTPQ_Glue] = "glue";     // Any glue word translates back to this - see cheat in inverse mapping
	m_mTokenToString[eTPQ_Around] = "around";

	// Bool Properties
	m_mTokenToString[eTPQ_PB_CoverSoft] = "coverSoft";
	m_mTokenToString[eTPQ_PB_CoverSuperior] = "coverSuperior";
	m_mTokenToString[eTPQ_PB_CoverInferior] = "coverInferior";
	m_mTokenToString[eTPQ_PB_CurrentlyUsedObject] = "currentlyUsedObject";
	m_mTokenToString[eTPQ_PB_Reachable] = "reachable";
	m_mTokenToString[eTPQ_PB_IsInNavigationMesh] = "isInNavigationMesh";

	// Real Properties
	m_mTokenToString[eTPQ_PR_CoverRadius] = "coverRadius";
	m_mTokenToString[eTPQ_PR_CoverDensity] = "coverDensity";
	m_mTokenToString[eTPQ_PR_BulletImpacts] = "bulletImpacts";
	m_mTokenToString[eTPQ_PR_CameraCenter] = "cameraCenter";
	m_mTokenToString[eTPQ_PR_HostilesDistance] = "hostilesDistance";
	m_mTokenToString[eTPQ_PR_FriendlyDistance] = "friendlyDistance";
	m_mTokenToString[eTPQ_PR_Random] = "random";
	m_mTokenToString[eTPQ_PR_Type] = "type";

	// Tests (Boolean Queries)
	m_mTokenToString[eTPQ_T_Visible] = "visible";
	m_mTokenToString[eTPQ_T_CanShoot] = "canShoot";
	m_mTokenToString[eTPQ_T_CanShootTwoRayTest] = "canShootTwoRayTest";
	m_mTokenToString[eTPQ_T_Towards] = "towards";
	m_mTokenToString[eTPQ_T_CanReachBefore] = "canReachBefore";
	m_mTokenToString[eTPQ_T_CrossesLineOfFire] = "crossesLineOfFire";
	m_mTokenToString[eTPQ_T_HasShootingPosture] = "hasShootingPosture";
	m_mTokenToString[eTPQ_T_OtherSide] = "otherSide";

	// Measures (Real Queries)
	m_mTokenToString[eTPQ_M_Distance] = "distance";
	m_mTokenToString[eTPQ_M_Distance2d] = "distance2d";
	m_mTokenToString[eTPQ_M_PathDistance] = "pathDistance";
	m_mTokenToString[eTPQ_M_ChangeInDistance] = "changeInDistance";
	m_mTokenToString[eTPQ_M_DistanceInDirection] = "distanceInDirection";
	m_mTokenToString[eTPQ_M_DistanceLeft] = "distanceLeft";
	m_mTokenToString[eTPQ_M_RatioOfDistanceFromActorAndDistance] = "ratioOfDistanceFromActorAndDistance";
	m_mTokenToString[eTPQ_M_Directness] = "directness";
	m_mTokenToString[eTPQ_M_Dot] = "dot";
	m_mTokenToString[eTPQ_M_ObjectsDot] = "objectsDot";
	m_mTokenToString[eTPQ_M_ObjectsMoveDirDot] = "objectsMoveDirDot";
	m_mTokenToString[eTPQ_M_HeightRelative] = "heightRelative";
	m_mTokenToString[eTPQ_M_AngleOfElevation] = "angleOfElevation";
	m_mTokenToString[eTPQ_M_PointDirDot] = "pointDirDot";
	m_mTokenToString[eTPQ_M_CoverHeight] = "coverHeight";
	m_mTokenToString[eTPQ_M_EffectiveCoverHeight] = "effectiveCoverHeight";
	m_mTokenToString[eTPQ_M_DistanceToRay] = "distanceToRay";
	m_mTokenToString[eTPQ_M_AbsDistanceToPlaneAtClosestRayPos] = "absDistanceToPlaneAtClosestRayPos";

	// Generators (_no_ auxiliary Object)
	m_mTokenToString[eTPQ_G_Grid] = "grid";
	m_mTokenToString[eTPQ_G_Entities] = "entities";
	m_mTokenToString[eTPQ_G_Indoor] = "indoor";
	m_mTokenToString[eTPQ_G_CurrentPos] = "currentPos";
	m_mTokenToString[eTPQ_G_CurrentCover] = "currentCover";
	m_mTokenToString[eTPQ_G_CurrentFormationPos] = "currentFormationPos";
	m_mTokenToString[eTPQ_G_Objects] = "objects";
	m_mTokenToString[eTPQ_G_PointsInNavigationMesh] = "pointsInNavigationMesh";

	// Generators independent from AI Systems
	m_mTokenToString[eTPQ_G_PureGrid] = "pureGrid";

	// GeneratorsO (_with_ auxiliary Object)
	// Note that these get combined with the aux Object later so
	// might be confusing when debugging. See enum.
	m_mTokenToString[eTPQ_GO_Hidespots] = "hidespots";
	m_mTokenToString[eTPQ_GO_Cover] = "cover";

	// Objects
	m_mTokenToString[eTPQ_O_None] = "none";
	m_mTokenToString[eTPQ_O_Actor] = "puppet";
	m_mTokenToString[eTPQ_O_AttentionTarget] = "attentionTarget";
	m_mTokenToString[eTPQ_O_RealTarget] = "realTarget";
	m_mTokenToString[eTPQ_O_ReferencePoint] = "referencePoint";
	m_mTokenToString[eTPQ_O_ReferencePointOffsettedByItsForwardDirection] = "referencePointOffsettedByItsForwardDirection";

	// Objects independent from AI Systems
	m_mTokenToString[eTPQ_O_Entity] = "entity";

	m_mTokenToString[eTPQ_O_CurrentFormationRef] = "currentFormationRef";
	m_mTokenToString[eTPQ_O_Player] = "player";
	m_mTokenToString[eTPQ_O_Leader] = "leader";
	m_mTokenToString[eTPQ_O_LastOp] = "lastOp";

	// Limits
	m_mTokenToString[eTPQ_L_Min] = "min";
	m_mTokenToString[eTPQ_L_Max] = "max";
	m_mTokenToString[eTPQ_L_Equal] = "equal";

	// Populate the inverse mapping
	std::map<TTacticalPointQuery, string>::const_iterator itM;
	for (itM = m_mTokenToString.begin(); itM != m_mTokenToString.end(); ++itM)
		m_mStringToToken[itM->second] = itM->first;
	assert(m_mNameToID.size() == m_mIDToQuery.size());

	// Cheat with gluewords, where the mapping is asymmetric
	m_mStringToToken["from"] = eTPQ_Glue;
	m_mStringToToken["to"] = eTPQ_Glue;
	m_mStringToToken["at"] = eTPQ_Glue;
	m_mStringToToken["the"] = eTPQ_Glue;
	m_mStringToToken["of"] = eTPQ_Glue;
	m_mStringToToken["for"] = eTPQ_Glue;

	// From the above, "glue" also maps to eTPQ_Glue, which does no harm
	// and might actually be handy but its use is not encouraged

	// Parameter names
	m_mParamStringToToken["density"] = eTPQP_Density;
	m_mParamStringToToken["objectsType"] = eTPQP_ObjectsType;
	m_mParamStringToToken["height"] = eTPQP_Height;
	m_mParamStringToToken["horizontalSpacing"] = eTPQP_HorizontalSpacing;
	m_mParamStringToToken["optionLabel"] = eTPQP_OptionLabel;
	m_mParamStringToToken["tagPointPostfix"] = eTPQP_TagPointPostfix;
	m_mParamStringToToken["extenderStringParameter"] = eTPQP_ExtenderStringParameter;
	m_mParamStringToToken["navigationAgentType"] = eTPQP_NavigationAgentType;

	m_mRelativeValueSourceStringToToken["objectRadius"] = eTPSRVS_objectRadius;

	// Define cost of queries
	ApplyCost(eTPQ_PB_CoverSoft, eTPQC_CHEAP);
	ApplyCost(eTPQ_PB_CoverSuperior, eTPQC_CHEAP);
	ApplyCost(eTPQ_PB_CoverInferior, eTPQC_CHEAP);
	ApplyCost(eTPQ_PR_Type, eTPQC_CHEAP);
	ApplyCost(eTPQ_PB_CurrentlyUsedObject, eTPQC_CHEAP);
	ApplyCost(eTPQ_PR_CoverRadius, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_Distance, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_Distance2d, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_ChangeInDistance, eTPQC_CHEAP);
	ApplyCost(eTPQ_PR_Random, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_DistanceInDirection, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_DistanceLeft, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_Directness, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_Dot, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_ObjectsDot, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_ObjectsMoveDirDot, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_PointDirDot, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_CoverHeight, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_EffectiveCoverHeight, eTPQC_EXPENSIVE);
	ApplyCost(eTPQ_M_DistanceToRay, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_AbsDistanceToPlaneAtClosestRayPos, eTPQC_CHEAP);
	ApplyCost(eTPQ_T_CanReachBefore, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_RatioOfDistanceFromActorAndDistance, eTPQC_CHEAP);
	ApplyCost(eTPQ_T_Towards, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_HeightRelative, eTPQC_CHEAP);
	ApplyCost(eTPQ_M_AngleOfElevation, eTPQC_CHEAP);
	ApplyCost(eTPQ_T_CrossesLineOfFire, eTPQC_CHEAP);
	ApplyCost(eTPQ_T_OtherSide, eTPQC_CHEAP);

	// Medium costs (e.g. iteration of simple operation)
	ApplyCost(eTPQ_PR_HostilesDistance, eTPQC_MEDIUM);
	ApplyCost(eTPQ_PR_FriendlyDistance, eTPQC_MEDIUM);
	ApplyCost(eTPQ_PR_CameraCenter, eTPQC_MEDIUM);
	ApplyCost(eTPQ_PB_IsInNavigationMesh, eTPQC_MEDIUM);

	// Very expensive
	ApplyCost(eTPQ_PR_BulletImpacts, eTPQC_EXPENSIVE);    // Spatial lookup
	ApplyCost(eTPQ_PR_CoverDensity, eTPQC_EXPENSIVE);     // Uses some cheap graph queries
	ApplyCost(eTPQ_T_Visible, eTPQC_DEFERRED);            // Uses raytest
	ApplyCost(eTPQ_T_CanShoot, eTPQC_DEFERRED);           // Uses raytests
	ApplyCost(eTPQ_T_CanShootTwoRayTest, eTPQC_DEFERRED); // Uses raytests
	ApplyCost(eTPQ_PB_Reachable, eTPQC_EXPENSIVE);        // Uses point-in-polygon test
	ApplyCost(eTPQ_M_PathDistance, eTPQC_DEFERRED);       // Uses pathfinding
	ApplyCost(eTPQ_T_HasShootingPosture, eTPQC_DEFERRED); // Uses raytests

	// All queries should have a cost, so check and issue warnings here
	for (itM = m_mTokenToString.begin(); itM != m_mTokenToString.end(); ++itM)
	{
		TTacticalPointQuery query = itM->first;
		if (query & (eTPQ_FLAG_PROP_BOOL | eTPQ_FLAG_PROP_REAL | eTPQ_FLAG_TEST | eTPQ_FLAG_MEASURE))
		{
			if (m_mIDToCost.find(query) == m_mIDToCost.end())
			{
				AIWarningID("<TacticalPointSystem> ", "No cost assigned to query: %s", itM->second.c_str());
				m_mIDToCost[query] = eTPQC_EXPENSIVE;
			}
		}
	}
}

//----------------------------------------------------------------------------------------------//

CTacticalPointSystem::~CTacticalPointSystem()
{
}

CTacticalPointSystem::SQueryEvaluation::SQueryEvaluation()
{
	Reset();
}

CTacticalPointSystem::SQueryEvaluation::~SQueryEvaluation()
{
	if (postureQueryID)
	{
		if (queryInstance.queryContext.pAIActor)
		{
			if (CPuppet* puppet = static_cast<CAIActor*>(queryInstance.queryContext.pAIActor)->CastToCPuppet())
				puppet->GetPostureManager().CancelPostureQuery(postureQueryID);
		}
	}

	if (visibleRayID)
		gAIEnv.pRayCaster->Cancel(visibleRayID);

	if (canShootRayID)
		gAIEnv.pRayCaster->Cancel(canShootRayID);

	if (canShootSecondRayID)
		gAIEnv.pRayCaster->Cancel(canShootSecondRayID);

	if (deferredExtenderCancelFunc)
		deferredExtenderCancelFunc();
}

//----------------------------------------------------------------------------------------------//

void CTacticalPointSystem::Reset()
{
	CRY_ASSERT_MESSAGE(m_LanguageExtenderDummyObjects.empty(), "A language extender hasn't unregistered its dummy objects");

	for (std::map<TPSQueryTicket, const SQueryInstance>::iterator iter = m_mQueryInstanceQueue.begin();
	     iter != m_mQueryInstanceQueue.end();
	     ++iter)
	{
		const SQueryInstance& instance = iter->second;
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
	}
	m_mQueryInstanceQueue.clear();

	for (std::map<TPSQueryTicket, SQueryEvaluation>::iterator iter = m_mQueryEvaluationsInProgress.begin();
	     iter != m_mQueryEvaluationsInProgress.end();
	     ++iter)
	{
		SQueryInstance& instance = iter->second.queryInstance;
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
	}
	m_mQueryEvaluationsInProgress.clear();

	m_locked.clear();

	stl::free_container(m_avoidCircles);
	stl::free_container(m_cover);
	stl::free_container(m_points);
	stl::free_container(m_LanguageExtenderDummyObjects);
}

//----------------------------------------------------------------------------------------------//

void CTacticalPointSystem::RegisterCVars()
{
	REGISTER_CVAR2("ai_DebugTacticalPoints", &CVars.DebugTacticalPoints, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Display debugging information on tactical point selection system");
	REGISTER_CVAR2("ai_DebugTacticalPointsBlocked", &CVars.DebugTacticalPointsBlocked, 0, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Highlight with red spheres any points blocked by generation phase, e.g. occupied points");
	REGISTER_CVAR2("ai_TacticalPointsDebugDrawMode", &CVars.TacticalPointsDebugDrawMode, 1, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Debugging draw mode: 1=sphere transparency, 2=sphere size");
	REGISTER_CVAR2("ai_TacticalPointsDebugFadeMode", &CVars.TacticalPointsDebugFadeMode, 2, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Debugging fade mode: 1=vanish, 2=alpha fade, 3=blink");
	REGISTER_CVAR2("ai_TacticalPointsDebugScaling", &CVars.TacticalPointsDebugScaling, 1, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Scale the size of debugging spheres for visibility");
	REGISTER_CVAR2("ai_TacticalPointsDebugTime", &CVars.TacticalPointsDebugTime, 5, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Time to display debugging spheres for (if not 'persistent'");
	REGISTER_CVAR2("ai_TacticalPointsWarnings", &CVars.TacticalPointsWarnings, 1, VF_CHEAT | VF_CHEAT_NOCHECK | VF_DUMPTODISK,
	               "Toggles TPS Warnings on and off");
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::ExtendQueryLanguage(const char* szName, ETacticalPointQueryType eType, ETacticalPointQueryCost eCost)
{
	bool bValid = (szName && szName[0]);
	assert(bValid);
	if (!bValid)
		return false;

	// Check for overflow
	const int iNewQueryId = m_GameQueryIdMap[eType] + 1;
	const uint32 uTestField = (eType == eTPQT_GENERATOR_O ? eTPQ_MASK_QUERYSPACE_GEN_O : eTPQ_MASK_QUERYSPACE); // Test the correct set of low-order nibbles for overflow
	if (!(iNewQueryId & uTestField))
		return false;

	// Must not already exist in the vocab
	if (m_mStringToToken.end() != m_mStringToToken.find(szName))
		return false;

	// Generate unique game token Id for it
	TTacticalPointQuery queryId = m_GameQueryIdMap[eType]++;
	m_mStringToToken[szName] = queryId;
	m_mTokenToString[queryId] = szName;

	// Apply the cost
	bool bResult = (eCost != eTPQC_IGNORE ? ApplyCost(queryId, eCost) : true);
	if (bResult)
	{
		if (queryId & (eTPQ_FLAG_PROP_BOOL | eTPQ_FLAG_PROP_REAL | eTPQ_FLAG_TEST | eTPQ_FLAG_MEASURE))
		{
			if (m_mIDToCost.find(queryId) == m_mIDToCost.end())
			{
				AIWarningID("<TacticalPointSystem> ", "No cost assigned to query: %s", szName);
				m_mIDToCost[queryId] = eTPQC_EXPENSIVE;
			}
		}
	}
	return bResult;
}

//----------------------------------------------------------------------------------------------//
bool CTacticalPointSystem::ApplyCost(uint32 uQueryId, ETacticalPointQueryCost eCost)
{
	const bool bValid = (eCost > eTPQC_IGNORE && eCost < eTPQC_COUNT);
	assert(bValid);
	if (!bValid)
		return false;

	m_mIDToCost[uQueryId] = m_CostMap[eCost]++;
	return true;
}

//----------------------------------------------------------------------------------------------//
int CTacticalPointSystem::GetCost(uint32 uQueryId) const
{
	std::map<TTacticalPointQuery, int>::const_iterator it = m_mIDToCost.find(uQueryId);
	if (it == m_mIDToCost.end())
	{
		assert(false);
		return eTPQC_EXPENSIVE;
	}
	return it->second;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddLanguageExtender(ITacticalPointLanguageExtender* pExtender)
{
	return stl::push_back_unique(m_LanguageExtenders, pExtender);
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RemoveLanguageExtender(ITacticalPointLanguageExtender* pExtender)
{
	return stl::find_and_erase(m_LanguageExtenders, pExtender);
}

//----------------------------------------------------------------------------------------------//
IAIObject* CTacticalPointSystem::CreateExtenderDummyObject(const char* szDummyName)
{
	m_LanguageExtenderDummyObjects.push_back(TDummyRef());
	TDummyRef& newDummyRef = m_LanguageExtenderDummyObjects.back();

	gAIEnv.pAIObjectManager->CreateDummyObject(newDummyRef, szDummyName);
	return newDummyRef.GetAIObject();
}

//----------------------------------------------------------------------------------------------//
void CTacticalPointSystem::ReleaseExtenderDummyObject(tAIObjectID id)
{
	for (TDummyObjects::iterator it = m_LanguageExtenderDummyObjects.begin(), itEnd = m_LanguageExtenderDummyObjects.end(); it != itEnd; ++it)
	{
		if (it->GetObjectID() == id)
		{
			m_LanguageExtenderDummyObjects.erase(it);
			return;
		}
	}

	CRY_ASSERT_MESSAGE(false, "Trying to release a dummy object that isn't registered for this language extender");
}

//----------------------------------------------------------------------------------------------//

int CTacticalPointSystem::SyncQuery(TPSQueryID queryID, const QueryContext& context, CTacticalPoint& point)
{
	// (MATT) If this was a single element long and maintained in a buffer, we might avoid allocations {2009/11/13}
	TTacticalPoints results;

	int nOptionUsed = SyncQueryShortlist(queryID, context, results, 1);

	if ((nOptionUsed >= 0) && !results.empty())
		point = results[0];
	else
		point = CTacticalPoint();

	return nOptionUsed;
}

//----------------------------------------------------------------------------------------------//

int CTacticalPointSystem::SyncQueryShortlist
  (TPSQueryID queryID, const QueryContext& context, TTacticalPoints& vPoints, int n)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	CAISystem* pAISystem = GetAISystem();
	vPoints.clear();

	const CTacticalPointQuery* pQuery = GetQuery(queryID);
	if (!pQuery || !pQuery->IsValid() || n < 1)
	{
		gEnv->pLog->LogError("AI: <TacticalPointSystem> Invalid TPS query");
		return -1; // Should be OPTION_ERROR or somesuch
	}

	const bool bDebug = CVars.DebugTacticalPoints != 0;
	if (bDebug)
	{
		gEnv->pLog->Log("TPS Query: %s", pQuery->GetName());
		// (MATT) Handy for tracking exactly how many TPS queries are happening {2008/10/03}
		if (context.pAIActor)
		{
			CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
			gEnv->pLog->Log("Frame: %d TPS Query: %s Actor: %s\n", gEnv->pRenderer->GetFrameID(), pQuery->GetName(), pAIActor->GetName());
		}
	}

	// Advance ticket - even queries with errors get a ticket, which should aid debugging
	m_nQueryInstanceTicket.Advance();

	// Populate an instance
	SQueryInstance instance;
	instance.nQueryInstanceID = m_nQueryInstanceTicket;
	instance.nPoints = n;
	instance.nQueryID = queryID;
	instance.pReceiver = NULL;
	instance.queryContext = context;

	// Verify it
	if (!VerifyQueryInstance(instance))
	{
		gEnv->pLog->LogError("AI: <TacticalPointSystem> TPS query failed verification");
		return -1;
	}

	// Create a query evaluation
	SQueryEvaluation evaluation;
	bool bOk = SetupQueryEvaluation(instance, evaluation);
	assert(evaluation.eEvalState == SQueryEvaluation::eHeapEvaluation);
	if (!bOk)
	{
		gEnv->pLog->LogError("AI: <TacticalPointSystem> TPS query setup failed");
		return -1;
	}

	// Skipping the queue, we immediately evaluate the query
	// It is very important that synchronous queries are completed in a timely manner
	// However, for development purposes, set a very high time limit, then check the actual time taken to issue a warning
	CTimeValue startTime = gEnv->pTimer->GetAsyncTime();
	CTimeValue timeLimit = startTime + 1.0f;   // Up to 1 second!
	bOk = ContinueQueryEvaluation(evaluation, timeLimit);

	// Check the actual time taken and issue a warning below
	CTimeValue elapsed = gEnv->pTimer->GetAsyncTime() - startTime;

	CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
	if (!bOk)
	{
		gEnv->pLog->LogError("AI: <TacticalPointSystem> Query '%s' failed during evaluation on actor '%s'",
		                     pQuery->GetName(), pAIActor ? pAIActor->GetName() : "<null>");
		return -1;
	}
#ifdef CRYAISYSTEM_DEBUG
	if (elapsed.GetMilliSecondsAsInt64() >= MAX_SYNC_TIME_MS)
	{
		AIWarningID("<TacticalPointSystem> ", "Query '%s' performed by '%s' took too long! (%dms >= %dms)",
		            pQuery->GetName(), pAIActor ? pAIActor->GetName() : "<null>", (uint32)elapsed.GetMilliSeconds(), (uint32)(MAX_SYNC_TIME_MS));
	}
#endif

	switch (evaluation.eEvalState)
	{
	case SQueryEvaluation::eCompleted:
		// Returned valid result within the time limit
		if (evaluation.iCurrentQueryOption >= 0)
		{
			// Locate the range of the best N points
			std::vector<SPointEvaluation>::iterator it = evaluation.GetIterRejectedEndAcceptedBegin();
			const std::vector<SPointEvaluation>::iterator end = evaluation.vPoints.end();
			int nPoints = int(std::distance(it, end));
			assert(nPoints > 0 && nPoints >= instance.nPoints);

#ifdef CRYAISYSTEM_DEBUG
			if (bDebug)
				AddQueryForDebug(evaluation);
#endif

			vPoints.resize(nPoints);
			for (int i = 0; i < nPoints; ++i, ++it)
				vPoints[i] = it->m_Point;
		}
		break;

	case SQueryEvaluation::eError:
		// Genuine error in query
		bOk = false;
		break;

	case SQueryEvaluation::eHeapEvaluation:
	case SQueryEvaluation::eInitialized:
	case SQueryEvaluation::eCompletedOption:
		// If we were left in this state it indicates incomplete evaluation, which should be due to running out of time
		// Might be possible to return useful debugging points, with some work
		bOk = false;
		break;

	case SQueryEvaluation::eDebugging:
	case SQueryEvaluation::eEmpty:
	default:
		// System error
		assert(false);
		bOk = false;
		break;
	}

	// Since async query is currently for internal use, we return CTacticalPoints without conversion

	return (bOk ? evaluation.iCurrentQueryOption : -1);
}

#ifdef CRYAISYSTEM_DEBUG
//-----------------------------------------------------------------------------------------------------------
void CTacticalPointSystem::AddQueryForDebug(SQueryEvaluation& evaluation)
{
	// (MATT) Not very efficient when there's a lot of queries going on. But it does need to be pretty versatile... {2009/07/16}

	EntityId targetAI = evaluation.queryInstance.queryContext.actorEntityId;
	evaluation.owner = targetAI;
	evaluation.timePlaced = gEnv->pTimer->GetAsyncTime(); // (MATT) Would use frame time, but that appears junk! {2009/11/22}
	evaluation.timeErase = evaluation.timePlaced;
	evaluation.timeErase += CTimeValue(CVars.TacticalPointsDebugTime);

	// Find any existing entry for this entity
	std::list<SQueryEvaluation>::iterator it = m_lstQueryEvaluationsForDebug.begin();

	for (; it != m_lstQueryEvaluationsForDebug.end() && it->owner != targetAI; ++it)
		;

	// Replace the existing entry or add a new one
	if (it != m_lstQueryEvaluationsForDebug.end())
		*it = evaluation;
	else
		m_lstQueryEvaluationsForDebug.push_back(evaluation);
}
#endif

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::GeneratePoints(const std::vector<CCriterion>& vGeneration, const QueryContext& context, const COptionCriteria* pOption, TTacticalPoints& vPoints) const
{
	vPoints.clear();
	std::vector<CCriterion>::const_iterator itC;
	for (itC = vGeneration.begin(); itC != vGeneration.end(); ++itC)
	{
		if (!Generate(*itC, context, pOption, vPoints))
		{
			string sBuffer;
			Unparse(*itC, sBuffer);
			if (CVars.TacticalPointsWarnings > 0)
			{
				AIWarningID("<TacticalPointSystem> ", "Generation criterion failed: %s", sBuffer.c_str());
			}
			return false;
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

const CTacticalPointQuery* CTacticalPointSystem::GetQuery(TPSQueryID nQueryID) const
{
	std::map<TPSQueryID, CTacticalPointQuery>::const_iterator itQ = m_mIDToQuery.find(nQueryID);
	return (itQ == m_mIDToQuery.end() ? NULL : &(itQ->second));
}

//----------------------------------------------------------------------------------------------//
// Take a query instance (request) and convert it into an evaluation structure

bool CTacticalPointSystem::SetupQueryEvaluation(const SQueryInstance& instance, SQueryEvaluation& evaluation) const
{
	// We may no assumptions about previous state
	CAISystem* pAISystem = GetAISystem();

	// Check this query still exists (system error)
	const CTacticalPointQuery* pQuery = GetQuery(instance.nQueryID);
	if (!pQuery)
	{
		evaluation.eEvalState = SQueryEvaluation::eError;
		assert(false);
		return false;
	}

	evaluation.queryInstance = instance;

	int nOption = evaluation.iCurrentQueryOption;
	const COptionCriteria* pOption = pQuery->GetOption(nOption);

	bool bDebugging = (CVars.DebugTacticalPoints != 0);
	if (bDebugging && nOption == 0) // Just give this info the first time
	{
		CAIActor* pAIActor = static_cast<CAIActor*>(instance.queryContext.pAIActor);
		if (pAIActor)
			gEnv->pLog->Log("Frame: %d TPS Query: %s Actor: %s\n", gEnv->pRenderer->GetFrameID(), pQuery->GetName(), pAIActor->GetName());
		else
			gEnv->pLog->Log("TPS Query: %s", pQuery->GetName());
	}

	if (bDebugging)
		gEnv->pLog->Log("[%d]: %s", nOption, pOption->GetDescription().c_str());

	// Hmm. Will this actually be correctly set?

	TTacticalPoints points;
	if (!GeneratePoints(pOption->GetAllGeneration(), instance.queryContext, pOption, points))
	{
		if (CVars.TacticalPointsWarnings > 0)
		{
			pAISystem->Warning("<TacticalPointSystem> ",
			                   "Generation failed, option %i skipped. "
			                   "Behaviours should only use queries that are valid in their context. \n", nOption);
			TPSDescriptionWarning(*pQuery, instance.queryContext, pOption);
		}
		return false;
	}

	if (!SetupHeapEvaluation(pOption->GetAllConditions(), pOption->GetAllWeights(), instance.queryContext, points, instance.nPoints, evaluation))
	{
		if (CVars.TacticalPointsWarnings > 0)
		{
			pAISystem->Warning("<TacticalPointSystem> ",
			                   "Evaluation failed, option %i skipped. "
			                   "Behaviours should only use queries that are valid in their context. \n", nOption);
			TPSDescriptionWarning(*pQuery, instance.queryContext, pOption);
		}
		// Possibly we should return false here, but people are keen on not treating this as a hard error to allow wider use of fallbacks
	}

	evaluation.eEvalState = SQueryEvaluation::eHeapEvaluation;

	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::ContinueQueryEvaluation(SQueryEvaluation& eval, CTimeValue timeLimit) const
{
	bool bOk = true;
	CTimeValue currTime;

	do
	{
		switch (eval.eEvalState)
		{
		default:
		case SQueryEvaluation::eEmpty:                                             // We should have no empty queries
		case SQueryEvaluation::eDebugging:                                         // Queries preserved for debugging need no further processing
			assert(false);
			// Jump to error handling, which should switch to state and return nothing
			bOk = false;
			break;

		case SQueryEvaluation::eCompleted:                                         // Valid result ready to return, of 0 or more points
			// (MATT) Shouldn't actually be needed, covered by the while condition {2009/11/25}
			break;

		case SQueryEvaluation::eCompletedOption:
			// We completed an option, but found no points - try the next one
			eval.iCurrentQueryOption++;
			if (!GetQuery(eval.queryInstance.nQueryID)->GetOption(eval.iCurrentQueryOption))
			{
				// We've exhausted all the options
				// Mark complete and return
				eval.eEvalState = SQueryEvaluation::eCompleted;
				eval.iCurrentQueryOption = -1;
				break;
			}
		// Fall through
		case SQueryEvaluation::eReady:
			// (Re)start generation, setup points according to query instance
			bOk = SetupQueryEvaluation(eval.queryInstance, eval);
			break;

		case SQueryEvaluation::eInitialized:
		// (MATT) Currently unused {2009/11/25}
		case SQueryEvaluation::eWaitingForDeferred:
		case SQueryEvaluation::eHeapEvaluation:
			// Continue evaluation of heap
			bOk = ContinueHeapEvaluation(eval, timeLimit);
			break;

		case SQueryEvaluation::eError:
			eval.iCurrentQueryOption = -1;
			bOk = false;
			break;
		}
	}
	while (bOk && (eval.eEvalState != SQueryEvaluation::eCompleted) &&
	       (eval.eEvalState != SQueryEvaluation::eWaitingForDeferred) && (currTime = gEnv->pTimer->GetAsyncTime()) < timeLimit);

	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::SetupHeapEvaluation(const std::vector<CCriterion>& vConditions, const std::vector<CCriterion>& vWeights, const QueryContext& context, const std::vector<CTacticalPoint>& vPoints, int n, SQueryEvaluation& eval) const
{
	// Do we have proper error handling, when the cheap tests fail? Probably need goto.
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// Note: Points are always currently in consideration, or chosen as results, or rejected, which are all mutually exclusive.

	// ------ in consideration ----- / ----- rejected (for debugging) ----- / ----- valid ----- /

	// Sort criteria into cheap and expensive
	AILogComment("<TacticalPointSystem> Evaluating %" PRISIZE_T " points with %" PRISIZE_T " conditions and %" PRISIZE_T " weights, for a result of the %d best points",
	             vPoints.size(), vConditions.size(), vWeights.size(), n);

	// Sort criteria into cheap and expensive
	// Which, again, could be done in the original query...

	eval.vCheapConds.clear();
	eval.vExpConds.clear();
	eval.vDefConds.clear();
	eval.vCheapWeights.clear();
	eval.vExpWeights.clear();

	std::vector<CCriterion>::const_iterator itC;
	for (itC = vConditions.begin(); itC != vConditions.end(); ++itC)
	{
		int cost = GetCost(itC->GetQuery());
		if (cost < 0)
			eval.vDefConds.push_back(*itC);
		else if (cost < 256)
			eval.vCheapConds.push_back(*itC);
		else
			eval.vExpConds.push_back(*itC);
	}

	{
		std::vector<CCriterion>::const_iterator itD;
		for (itD = vWeights.begin(); itD != vWeights.end(); ++itD)
		{
			if (GetCost(itD->GetQuery()) < 256) // Erk!
				eval.vCheapWeights.push_back(*itD);
			else if (itD->GetValueAsFloat() != 0.0f)
				eval.vExpWeights.push_back(*itD);
			else
				AIWarningID("<TacticalPointSystem> ", "Zero weight criterion (ignored) in query");
		}
	}

	AILogComment("<TacticalPointSystem> 3 Sorted into %" PRISIZE_T " / %" PRISIZE_T " / %" PRISIZE_T " / %" PRISIZE_T " cheap conditions/weights, expensive conditions/weights",
	             eval.vCheapConds.size(), eval.vCheapWeights.size(), eval.vExpConds.size(), eval.vExpWeights.size());

	// Sort those vectors into order!
	// Actually, the option itself should store them sorted, before they even arrive.

	// Establish total limits of the expensive weights
	// TODO: Could renormalise here to save float ops later. Think about it.
	// This could probably happen per-query also, rather than per-instance.
	// Note that all query results are normalised to the 0.0-1.0 range, before weighting, so this is easy
	float fMinExpWeight = 0.0f, fMaxExpWeight = 0.0f;
	for (itC = eval.vExpWeights.begin(); itC != eval.vExpWeights.end(); ++itC)
	{
		float w = itC->GetValueAsFloat();
		if (w > 0.0f)
			fMaxExpWeight += w;
		else
			fMinExpWeight += w;
	}

	// Same for cheap weights, but we'll only use this for debugging purposes?
	float fMinCheapWeight = 0.0f, fMaxCheapWeight = 0.0f;
	for (itC = eval.vCheapWeights.begin(); itC != eval.vCheapWeights.end(); ++itC)
	{
		float w = itC->GetValueAsFloat();
		if (w > 0.0f)
			fMaxCheapWeight += w;
		else
			fMinCheapWeight += w;
	}

	// Make sure the processing block is empty, then adjust it's size
	// This should never again grow and we will fill it from both ends
	eval.vPoints.clear();
	eval.vPoints.resize(vPoints.size());                     // Confusing - shouldn't both be vPoints

	// Transform all points into SEvaluationPoints
	// Apply cheap tests and weights to points before putting them into the block
	// Which is more important here - code locality or data locality?

	// Iterate through input points
	std::vector<CTacticalPoint>::const_iterator itInputPoints = vPoints.begin();
	std::vector<CTacticalPoint>::const_iterator itInputPointsEnd = vPoints.end();

	// Iterators defining valid ranges in the heap - both ranges are initially zero, and we fill in from both ends of the vector
	std::vector<SPointEvaluation>::iterator itHeapBegin = eval.vPoints.begin();       // Start of valid heap area
	std::vector<SPointEvaluation>::iterator itHeapEnd = eval.vPoints.begin();         // Initially heap is 0-size
	std::vector<SPointEvaluation>::iterator itRejectedBegin = eval.vPoints.end();     // Initially there are 0 rejected points
	std::vector<SPointEvaluation>::iterator itRejectedEnd = eval.vPoints.end();       // Rejected points end the vector
	// Note that accepted points will later fill in from the end of the vector, but only in the evaluation phase

	// Are there any expensive weights or conditions?
	// If only cheap, we won't have any partial evaluation
	SPointEvaluation::EPointEvaluationState initialEvalState =
	  (eval.vExpConds.empty() && eval.vExpWeights.empty() ? SPointEvaluation::eValid : SPointEvaluation::ePartial);

	// I.e. whole range is invalid
	for (; itInputPoints != itInputPointsEnd; ++itInputPoints)
	{
		const CTacticalPoint& inputPoint = *itInputPoints;

		// Test each cheap condition on this point
		bool bResult = true;
		for (itC = eval.vCheapConds.begin(); itC != eval.vCheapConds.end(); ++itC)
		{
			if (!Test(*itC, inputPoint, context, bResult)) // Actually perform a test
				return false;                                // On error condition
			if (!bResult)
				break;
		}

		// If point failed any test, reject it now
		if (!bResult)
		{
			// Create rejected point on the end of the block
			// Perhaps assertion to check for the overlap that shouldn't be possible
			SPointEvaluation evalPt(inputPoint, -100.0f, -100.0f, SPointEvaluation::eRejected);   // Is that everything?
			*(--itRejectedBegin) = evalPt;
			continue; // On point failed test
		}

		// Evaluate every cheap weight on this point
		float fWeight = 0.0f;
		float fResult;
		for (itC = eval.vCheapWeights.begin(); itC != eval.vCheapWeights.end(); ++itC)
		{
			if (!Weight(*itC, inputPoint, context, fResult))
				return false;
			fWeight += fResult * itC->GetValueAsFloat();
		}

		// (MATT) What happens if expensive condition but no weights at all? {2009/11/20}

		// (MATT) fMinExpWeight is -ve, fMaxExpWeight is +ve, they represent the extremes we might reach from this initial value {2009/11/20}
		SPointEvaluation evalPt(*itInputPoints, fWeight + fMinExpWeight, fWeight + fMaxExpWeight, initialEvalState);
		(*itHeapEnd++) = evalPt;
		std::push_heap(itHeapBegin, itHeapEnd);
		// assert here too
	}

	// Ensure we met in the middle
	assert(itHeapEnd == itRejectedBegin);

	// Finish initialising our structure
	eval.SetIterHeapEndRejectedBegin(itHeapEnd);                // We've now filled in from both ends - heap and rejected areas are valid
	eval.SetIterRejectedEndAcceptedBegin(eval.vPoints.end());   // Accepted points are 0-size

	// Actually, latter points aren't necessarily rejected - if only cheap tests...?
	assert(itHeapEnd == itRejectedBegin);
	// At end the two iterators should have met and we have a range of heapified points and some rejected at the end of the block
	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::ContinueHeapEvaluation(SQueryEvaluation& eval, CTimeValue timeLimit) const
{
	// No concept of sorting interleaved expensive weights and conditions :/
	// OTOH, we still get something out of it. Only run conditions on the most promising point.

	// ------ in heap (for consideration) ----- / ----- rejected (for debugging) ----- / ----- accepted (results) ----- /

	bool bOk = true; // True if we return without errors

	// For readability, provide these aliases for the beginning and end of the entire points vector
	const std::vector<SPointEvaluation>::iterator itHeapBegin = eval.vPoints.begin();       // Start of valid heap area
	const std::vector<SPointEvaluation>::iterator itAcceptedEnd = eval.vPoints.end();       // End of the accepted points area

	// Note these iterators have need to be written back to the structure at the end of this call
	std::vector<SPointEvaluation>::iterator itHeapEndRejectedBegin = eval.GetIterHeapEndRejectedBegin();
	std::vector<SPointEvaluation>::iterator itRejectedEndAcceptedBegin = eval.GetIterRejectedEndAcceptedBegin();

	int iImplicitExpCondsSize = 1;  // We're adding an extra implicit expensive condition
	int iExpCondsSize = (int)eval.vExpConds.size();
	int iDefCondsSize = (int)eval.vDefConds.size();
	int iExpWeightsSize = (int)eval.vExpWeights.size();

	CTimeValue currTime = gEnv->pTimer->GetAsyncTime();

	// Check that there are actually some unrejected points to consider
	// If they all failed on cheap tests or none were generated, go on to next option
	if (itHeapEndRejectedBegin == itHeapBegin)
	{
		eval.eEvalState = SQueryEvaluation::eCompletedOption;
		goto HeapReturn;
	}

	do
	{
		SPointEvaluation& topPoint = eval.vPoints[0];  // Careful, this becomes invalid later

		int iQueryIndex = topPoint.m_iQueryIndex++;
		if (iQueryIndex < iExpCondsSize + iImplicitExpCondsSize + iDefCondsSize)   // less than, surely...?
		{
			bool bResult = true;

			if (iQueryIndex < iExpCondsSize)
			{
				// Next query to be done is a condition

				CCriterion& condition = eval.vExpConds[iQueryIndex];
				if (!Test(condition, topPoint.m_Point, eval.queryInstance.queryContext, bResult))
				{
					bOk = false;
					goto HeapReturn;  // On error condition
				}
			}
			else if (iQueryIndex < iExpCondsSize + iImplicitExpCondsSize) // Implicit Expensive Conditions go here!
			{
				// We're checking for locked results at the latest possible stage!
				// Since it's potentially the most expensive test
				{
					TLockedPoints::const_iterator itLocked = m_locked.begin();
					TLockedPoints::const_iterator itLockedEnd = m_locked.end();

					for (; bResult && (itLocked != itLockedEnd); ++itLocked)
					{
						const TTacticalPoints& lockedPoints = itLocked->second;

						TTacticalPoints::const_iterator it = lockedPoints.begin();
						TTacticalPoints::const_iterator end = lockedPoints.end();

						for (; it != end; ++it)
						{
							if (topPoint.m_Point == *it)
							{
								bResult = false;
								break;
							}
						}
					}
				}
			}
			else // Deferred Expensive Conditions
			{
				CCriterion& condition = eval.vDefConds[iQueryIndex - iExpCondsSize - iImplicitExpCondsSize];
				AsyncState state = DeferredTest(condition, topPoint.m_Point, eval, bResult);

				if (state == AsyncFailed)
				{
					bOk = false;
					goto HeapReturn;  // On error condition
				}

				if (state != AsyncComplete)
				{
					eval.eEvalState = SQueryEvaluation::eWaitingForDeferred;
					--topPoint.m_iQueryIndex; // keep on the same criterion
					break;
				}

				eval.eEvalState = SQueryEvaluation::eHeapEvaluation;
			}

			if (!bResult)
			{
				// Test failed - reject the point:
				// Move out of heap, into the area for rejected points.
				std::pop_heap(eval.vPoints.begin(), itHeapEndRejectedBegin);
				--itHeapEndRejectedBegin;

				// Mark point as rejected
				itHeapEndRejectedBegin->m_eEvalState = SPointEvaluation::eRejected;
				itHeapEndRejectedBegin->m_fMin = -100.0f;
				itHeapEndRejectedBegin->m_fMax = -100.0f;

				//Have we used up all the valid points?
				if (eval.vPoints.begin() == itHeapEndRejectedBegin)
					break;
			}
			// Otherwise we will go on to the next query on this point, which will still be top
		}
		else if (iQueryIndex < iExpCondsSize + iImplicitExpCondsSize + iDefCondsSize + iExpWeightsSize)
		{
			// Next query to be done is a weight
			float fResult;
			CCriterion& weight = eval.vExpWeights[iQueryIndex - iExpCondsSize - iImplicitExpCondsSize - iDefCondsSize];
			if (!Weight(weight, topPoint.m_Point, eval.queryInstance.queryContext, fResult))
			{
				bOk = false;
				goto HeapReturn;  // On error condition
			}
			// We might consider being less strict on weights, for instance if the object doesn't exist.

			float fWeight = weight.GetValueAsFloat();
			// Narrow the potential score range
			// There's some double negation here that might be optimised out
			if (fWeight > 0.0f)
			{
				// +ve weight
				topPoint.m_fMin += fWeight * fResult;              // The min possible score is higher now
				topPoint.m_fMax -= fWeight * (1.0f - fResult);     // The max possible score is lower now
				// E.g. for a good result on a _positive_ weight (i.e. close to 1),
				// our min will go up a lot and our max down just a little
			}
			else
			{
				// -ve weight
				topPoint.m_fMin -= fWeight * (1.0f - fResult);     // The min possible score is higher now
				topPoint.m_fMax += fWeight * fResult;              // The max possible score is lower now
				// E.g. for a good result on a _negative_ weight (i.e. close to 0)
				// (just as above) our min will go up a lot and our max down just a little
			}

			// (MATT) Hang on! We're not using m_fMin! Basically, if m_fMin of this point is higher than m_fMax of the next, we can accept it. {2009/07/15}

			// This is likely to change the relative placing of the point in our heap, so re-heap it
			// Possible optimisation: check positions 1 and 2 in the heap to avoid a pop/push
			// Oddly, STL library doesn't seem to include any alternative

			// Invalidates topPoint
			std::pop_heap(itHeapBegin, itHeapEndRejectedBegin);
			std::push_heap(itHeapBegin, itHeapEndRejectedBegin);
		}
		else
		{
			// Have finished all queries on the point of highest potential
			// Beats all other points, so must be one of the best n points
			topPoint.m_eEvalState = SPointEvaluation::eAccepted;

			// Move points into accepted range, which will be formed in descending order. Invalidates topPoint
			// Note we have to first move it into the rejected range, then swap first and last rejected point
			// End result is to correctly adjust the ranges and add another point to the front of the accepted range
			std::pop_heap(itHeapBegin, itHeapEndRejectedBegin);
			--itHeapEndRejectedBegin;         // Now inserted into rejected range
			--itRejectedEndAcceptedBegin;
			std::swap(*itHeapEndRejectedBegin, *itRejectedEndAcceptedBegin); // Now swapped with a rejected element into accepted range
			// Note that if there were no rejected points, we will swap it with itself, which is fine

			// Do we now have enough points to return?
			if (++eval.nFoundBestN == eval.queryInstance.nPoints)
				eval.eEvalState = SQueryEvaluation::eCompleted;

			// Have we used up all the valid points?
			if (itHeapBegin == itHeapEndRejectedBegin)
				eval.eEvalState = SQueryEvaluation::eCompleted;
		}
	}
	while (eval.eEvalState == SQueryEvaluation::eHeapEvaluation && (currTime = gEnv->pTimer->GetAsyncTime()) < timeLimit);

	// The heap is left as any points we didn't have to fully evaluate and invalid range, of the ones we rejected

	// If we actually found some points...
	if (eval.eEvalState == SQueryEvaluation::eCompleted)
	{
		// This is slightly inefficient but will help avoid confusion: reverse the Accepted points to put them in descending score order
		std::reverse(itRejectedEndAcceptedBegin, eval.vPoints.end());
	}

HeapReturn:
	// Write-back iterators
	eval.SetIterHeapEndRejectedBegin(itHeapEndRejectedBegin);
	eval.SetIterRejectedEndAcceptedBegin(itRejectedEndAcceptedBegin);
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::Test(const CCriterion& criterion, const CTacticalPoint& point, const QueryContext& context, bool& result) const
{
	CAISystem* pAISystem = GetAISystem();

	// Query we will use for the test and type of query
	TTacticalPointQuery query = criterion.GetQuery();
	TTacticalPointQuery queryType = (TTacticalPointQuery) (query & eTPQ_MASK_QUERY_TYPE);

	// Variables we might need
	bool boolQueryResult;
	float floatQueryResult;

	// A sufficiently clever compiler would reduce all this syntax down to about 8 lines of assembler
	// At some point should check the results, in case writing a s.c.c. needs to be added to backlog

	switch (queryType)
	{
	case eTPQ_FLAG_PROP_BOOL:
		if (!BoolProperty(query, point, context, boolQueryResult))
			goto TestFail;
		result = (criterion.GetValueAsBool() == boolQueryResult);
		break;

	case eTPQ_FLAG_PROP_REAL:
		if (!RealProperty(query, point, context, floatQueryResult))
			goto TestFail;
		result = Limit(criterion.GetLimits(), floatQueryResult, criterion.GetValueAsFloat());
		break;

	case eTPQ_FLAG_TEST:
		if (!BoolTest(query, criterion.GetObject(), point, context, boolQueryResult))
			goto TestFail;
		result = (criterion.GetValueAsBool() == boolQueryResult);
		break;

	case eTPQ_FLAG_MEASURE:
		if (!RealMeasure(query, criterion.GetObject(), point, context, floatQueryResult))
			goto TestFail;
		result = Limit(criterion.GetLimits(), floatQueryResult, criterion.GetValueAsFloat());
		break;

	default:
		assert(false);
		return false;
	}

	return true;

TestFail:
	string sBuffer;
	Unparse(criterion, sBuffer);
	if (CVars.TacticalPointsWarnings > 0)
	{
		AIWarningID("<TacticalPointSystem> ", "Test criterion failed: %s", sBuffer.c_str());
	}
	return false;
}

AsyncState CTacticalPointSystem::DeferredTest(const CCriterion& criterion, const CTacticalPoint& point, SQueryEvaluation& eval,
                                              bool& result) const
{
	CAISystem* pAISystem = GetAISystem();

	// Query we will use for the test and type of query
	TTacticalPointQuery query = criterion.GetQuery();
	TTacticalPointQuery queryType = (TTacticalPointQuery) (query & eTPQ_MASK_QUERY_TYPE);

	// Variables we might need
	bool boolQueryResult;

	// A sufficiently clever compiler would reduce all this syntax down to about 8 lines of assembler
	// At some point should check the results, in case writing a s.c.c. needs to be added to backlog

	AsyncState state;

	switch (queryType)
	{
	case eTPQ_FLAG_TEST:
		{
			state = DeferredBoolTest(query, criterion.GetObject(), point, eval, boolQueryResult);
			if (state == AsyncFailed)
				goto TestFail;

			if (state == AsyncComplete)
				result = (criterion.GetValueAsBool() == boolQueryResult);
		}
		break;

	default:
		assert(false);
		return AsyncFailed;
	}

	return state;

TestFail:
	string sBuffer;
	Unparse(criterion, sBuffer);
	if (CVars.TacticalPointsWarnings > 0)
	{
		AIWarningID("<TacticalPointSystem> ", "Test criterion failed: %s", sBuffer.c_str());
	}
	return AsyncFailed;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::Weight(const CCriterion& criterion, const CTacticalPoint& point, const QueryContext& context, float& result) const
{

	// if a boolean query, call Test and return 0 or 1
	// if real, will need to call absolute and normalise to 0-1

	CAISystem* pAISystem = GetAISystem();

	// Query we will use for the test and type of query
	TTacticalPointQuery query = criterion.GetQuery();
	TTacticalPointQuery queryType = (TTacticalPointQuery) (query & eTPQ_MASK_QUERY_TYPE);

	// Variables we might need
	bool boolQueryResult = false;
	float floatQueryResult = 0.0f;

	switch (queryType)
	{
	case eTPQ_FLAG_PROP_BOOL:
		if (!BoolProperty(query, point, context, boolQueryResult))
			goto WeightFail;
		break;

	case eTPQ_FLAG_PROP_REAL:
		if (!RealProperty(query, point, context, floatQueryResult))
			goto WeightFail;
		break;

	case eTPQ_FLAG_TEST:
		if (!BoolTest(query, criterion.GetObject(), point, context, boolQueryResult))
			goto WeightFail;
		break;

	case eTPQ_FLAG_MEASURE:
		if (!RealMeasure(query, criterion.GetObject(), point, context, floatQueryResult))
			goto WeightFail;
		break;

	default:
		assert(false);
	}

	if (queryType & (eTPQ_FLAG_PROP_BOOL | eTPQ_FLAG_TEST))
	{
		result = (boolQueryResult ? 1.0f : 0.0f);
	}
	else
	{
		if (criterion.GetLimits())
		{
			result = (Limit(criterion.GetLimits(), floatQueryResult, criterion.GetValueAsFloat()) ? 1.0f : 0.0f);
		}
		else
		{
			// Keep it within the prescribed range
			float fMin, fMax;
			if (!RealRange(criterion.GetQuery(), fMin, fMax))
				goto WeightFail;
			if (floatQueryResult < fMin) floatQueryResult = 0.0f;       // Can skip normalisation
			else if (floatQueryResult > fMax)
				floatQueryResult = 1.0f;                                  // Can skip normalisation
			else
			{
				// Normalise it within that range
				floatQueryResult = (floatQueryResult - fMin) / (fMax - fMin);
			}
			assert(floatQueryResult >= 0.0f && floatQueryResult <= 1.0f);

			result = floatQueryResult;
		}
	}

	return true;

WeightFail:
	string sBuffer;
	Unparse(criterion, sBuffer);
	if (CVars.TacticalPointsWarnings > 0)
	{
		AIWarningID("<TacticalPointSystem> ", "Test criterion failed: %s", sBuffer.c_str());
	}
	return false;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::Generate(const CCriterion& criterion, const QueryContext& context, const COptionCriteria* pOption, TTacticalPoints& accumulator) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	// (MATT) Should pOption be a reference, or optional? {2008/04/23}
	assert(pOption);

	// Fetch data regarding object
	CAIObject* pObject = NULL;
	Vec3 vObjectPos(ZERO);
	if (!GetObject(criterion.GetObject(), context, pObject, vObjectPos)) return false;

	// Fetch data regarding aux object (where applicable)
	CAIObject* pObjectAux = NULL;
	Vec3 vObjectAuxPos(ZERO);
	GetObject(criterion.GetObjectAux(), context, pObjectAux, vObjectAuxPos);
	// Can be NULL if query does not use auxiliary object

	TTacticalPointQuery query = criterion.GetQuery();
	float fSearchDist = criterion.GetValueAsFloat();
	ETPSRelativeValueSource nRVS = criterion.GetValueAsRelativeValueSource();
	if (nRVS != eTPSRVS_Invalid)
	{
		switch (nRVS)
		{
		case eTPSRVS_objectRadius:
			fSearchDist = context.actorRadius;
			break;
		default:
			AIWarningID("<TacticalPointSystem> ", "Unhandled RelativeValueSource");
		}
	}

	const float fHeight = pOption->GetParams()->fHeight;

	// Attempt default implementations
	if (!GenerateInternal(query, context, fSearchDist, pOption, pObject, vObjectPos, pObjectAux, vObjectAuxPos, accumulator))
	{
		const float fDensity = pOption->GetParams()->fDensity;

		CTacticalPointGenerateResult generateResults;
		ITacticalPointGenerateResult* pResults = &generateResults;

		ITacticalPointLanguageExtender::TGenerateParameters parameters(Translate(query), context, pResults);
		ITacticalPointLanguageExtender::SGenerateDetails details(fSearchDist, fDensity, fHeight, pOption->GetParams()->tagPointPostfix);
		details.extenderStringParameter = pOption->GetParams()->extenderStringParamenter;

		// Allow the language extenders to attempt
		TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
		TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
		for (; itExtender != itExtenderEnd; ++itExtender)
		{
			ITacticalPointLanguageExtender* pExtender = (*itExtender);
			assert(pExtender);

			if (pExtender->GeneratePoints(parameters, details, (IAIObject*)pObject, vObjectPos, (IAIObject*)pObjectAux, vObjectAuxPos) &&
			    generateResults.HasPoints())
			{
				// Got results, stop trying.
				generateResults.GetPoints(accumulator);
				break;
			}
		}
	}

	// apply the height parameter
	if (!accumulator.empty() && fabs_tpl(fHeight) > 0.001f)
	{
		TTacticalPoints::iterator it = accumulator.begin();
		TTacticalPoints::iterator itEnd = accumulator.end();

		for (; it != itEnd; ++it)
		{
			Vec3 vPos(it->GetPos());
			vPos.z = vPos.z + fHeight;
			it->SetPos(vPos);
		}
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::GenerateInternal(TTacticalPointQuery query, const QueryContext& context, float fSearchDist, const COptionCriteria* pOption,
                                            CAIObject* pObject, const Vec3& vObjectPos, CAIObject* pObjectAux, const Vec3& vObjectAuxPos, TTacticalPoints& accumulator) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	CAISystem* pAISystem = GetAISystem();

	// ACTOR HACK
	Vec3 objPos = vObjectPos;
	if (pObject && pObject->CastToCAIActor() != NULL)
		objPos.z -= 1.8f;

	// Fetch actor parameters
	float fAgentRadius = context.actorRadius;

	switch (query)
	{
	case eTPQ_GO_Cover:
	case eTPQ_GO_Hidespots:
		if (gAIEnv.CVars.CoverSystem)
		{
			CRY_PROFILE_REGION(PROFILE_AI, "TPS Generate Cover Locations");

			m_cover.resize(0);
			gAIEnv.pCoverSystem->GetCover(objPos, fSearchDist, m_cover);
			uint32 coverCount = m_cover.size();

			CPipeUser* pipeUser = static_cast<CAIActor*>(context.pAIActor)->CastToCPipeUser();
			if (!pipeUser)
				return false;

			ICoverUser* pCoverUser = pipeUser->GetCoverUser();
			if (!pCoverUser)
				return false;

			// Manually update cover eyes with our object
			pipeUser->UpdateCoverEyesWithTarget(pObjectAux, vObjectAuxPos);
			const DynArray<Vec3>& eyes = pCoverUser->GetCoverEyes();

			if (eyes.size())
			{
				CRY_PROFILE_REGION(PROFILE_AI, "TPS Generate Cover Locations [GetOcclusion]");

				m_avoidCircles.resize(0);
				GatherAvoidCircles(objPos, fSearchDist, pipeUser, m_avoidCircles);

				IPersistantDebug* pPD = 0;
				if (CVars.DebugTacticalPointsBlocked)
				{
					pPD = gEnv->pGameFramework->GetIPersistantDebug();
					pPD->Begin("BadHideSpots", false);
				}

				if (CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor))
				{
					float distanceToCover = std::max<float>(context.distanceToCover, fAgentRadius);
					float inCoverRadius = std::min<float>(context.inCoverRadius, context.actorRadius) + 0.05f;
					float occupyRadius = fAgentRadius + gAIEnv.CVars.CoverSpacing;

					for (uint32 i = 0; i < coverCount; ++i)
					{
						const CoverID& coverID = m_cover[i];

						if (pCoverUser->IsCoverBlackListed(coverID))
							continue;

						Vec3 normal;
						Vec3 location = gAIEnv.pCoverSystem->GetCoverLocation(coverID, distanceToCover, 0, &normal);

						if (normal.dot(vObjectAuxPos - location) >= 0.0001f)
							continue;

						AvoidCircles::const_iterator acit = m_avoidCircles.begin();
						AvoidCircles::const_iterator acend = m_avoidCircles.end();

						bool occupied = false;

						for (; acit != acend; ++acit)
						{
							const AvoidCircle& circle = *acit;

							if ((location - circle.pos).GetLengthSquared2D() <= sqr(circle.radius + occupyRadius))
							{
								if (fabs_tpl(location.z - circle.pos.z) < 3.0f)
								{
									occupied = true;
									break;
								}
							}
						}

						if (occupied)
							continue;

						const float effectiveCoverHeight = pCoverUser->CalculateEffectiveHeightAt(location, coverID);
						if (effectiveCoverHeight != -1.0f)
						{
							if ((effectiveCoverHeight >= context.effectiveCoverHeight) || (context.effectiveCoverHeight <= 0.0001f))
								accumulator.push_back(CTacticalPoint(coverID, location));
							else
							{
								location.z += effectiveCoverHeight;

								if (pPD)
									pPD->AddCone(location, Vec3(0.0f, 0.0f, -1.0f), 0.25f, effectiveCoverHeight, Col_Red, 3.5f);
							}
						}
						else if (pPD)
							pPD->AddSphere(location, fAgentRadius, Col_Red, 3.5f);
					}
				}
				else
				{
					for (uint32 i = 0; i < coverCount; ++i)
					{
						const CoverID& coverID = m_cover[i];
						const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(coverID);

						Vec3 location = gAIEnv.pCoverSystem->GetCoverLocation(coverID, context.distanceToCover);

						if (surface.IsCircleInCover(vObjectAuxPos, location, fAgentRadius))
							accumulator.push_back(CTacticalPoint(coverID, location));
						else if (pPD)
							pPD->AddSphere(location, 0.75f, Col_Red, 3.0f);
					}
				}
			}
			else
			{
				for (uint32 i = 0; i < coverCount; ++i)
				{
					const CoverID& coverID = m_cover[i];

					Vec3 location = gAIEnv.pCoverSystem->GetCoverLocation(coverID, fAgentRadius);

					accumulator.push_back(CTacticalPoint(coverID, location));
				}
			}
		}
		break;

	case eTPQ_G_Grid:
		{
			float fDensity = pOption->GetParams()->fDensity;
			Vec3 vCenter = objPos;//pPuppet->GetPhysicsPos();

			// Copy-past from above [5/7/2010 evgeny]
			// We could go deeper than this and reap greater benefits
			CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);

			int nSteps = (int)(fSearchDist * 2.f / fDensity);
			float fGridHalfWidth = fDensity * nSteps / 2;
			I3DEngine* pEngine = gEnv->p3DEngine;

			float fi = 0.0f;
			for (int i = 0; i <= nSteps; ++i, fi += 1.0f)
			{
				float fk = 0.0f;
				for (int k = 0; k <= nSteps; ++k, fk += 1.0f)
				{
					// generate point on a plane
					Vec3 vPoint = vCenter + Vec3(-fGridHalfWidth + fDensity * fi, -fGridHalfWidth + fDensity * fk, 0.f);
					float z = vPoint.z;

					// if point generated is beneath ground
					// ...but not too low [5/7/2010 evgeny]
					float fTerrainHeightPlus1m = pEngine->GetTerrainElevation(vPoint.x, vPoint.y) + .2f;
					AABB aabb;
					pAIActor->GetLocalBounds(aabb);
					float fActorHeight = aabb.max.z - aabb.min.z;
					if (z > fTerrainHeightPlus1m - fActorHeight)
					{
						vPoint.z = max(z, fTerrainHeightPlus1m);
					}

					accumulator.push_back(CTacticalPoint(vPoint));
				}
			}
		}
		break;

	// a version of the grid generator with no external dependencies and actually generates a grid
	case eTPQ_G_PureGrid:
		{
			float density = pOption->GetParams()->fDensity;

			int gridPointsCount = (int)(fSearchDist * 2.f / density);
			float gridHalfWidth = density * gridPointsCount / 2;

			float x = 0.0f;
			for (int i = 0; i <= gridPointsCount; ++i, x += 1.0f)
			{
				float y = 0.0f;
				for (int j = 0; j <= gridPointsCount; ++j, y += 1.0f)
				{
					Vec3 point = objPos + Vec3(-gridHalfWidth + density * x, -gridHalfWidth + density * y, 0.f);
					accumulator.push_back(CTacticalPoint(point));
				}
			}
		}
		break;

	case eTPQ_G_Entities:
		{
			if (pObject)
			{
				IEntity* pEntity = pObject->GetEntity();
				EntityId nObjectID = pObject->GetEntityID();

				float fSearchDistSqr = fSearchDist * fSearchDist;

				ActorLookUp& lookUp = *gAIEnv.pActorLookUp;

				size_t activeCount = lookUp.GetActiveCount();

				for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
				{
					CAIActor* pAIActor = lookUp.GetActor<CAIActor>(actorIndex);

					// excluding self and 'nonActor' actors
					if (nObjectID != lookUp.GetEntityID(actorIndex))
					{
						IEntity* pOtherEntity = pAIActor->GetEntity();

						if ((pEntity != pOtherEntity) && objPos.GetSquaredDistance(pOtherEntity->GetPos()) < fSearchDistSqr)
							accumulator.push_back(CTacticalPoint(pOtherEntity->GetId(), pOtherEntity->GetPos()));
					}
				}
			}
		}
		break;

	case eTPQ_G_Indoor:
		return false;
		break;

	case eTPQ_G_CurrentPos:
		accumulator.push_back(CTacticalPoint(objPos));
		break;

	case eTPQ_G_CurrentCover:
		{
			// This isn't quite right I think, because last hidespot doesn't imply you got there
			CPipeUser* pObjectPipeUser = pObject ? pObject->CastToCPipeUser() : 0;
			if (pObjectPipeUser)
				accumulator.push_back(CTacticalPoint(pObjectPipeUser->m_CurrentHideObject.GetObjectPos()));
		}
		break;
	case eTPQ_G_Objects:
		{
			short objectType = pOption->GetParams()->iObjectsType;

			AIObjectOwners::const_iterator itObject;
			AIObjectOwners::const_iterator itEnd = gAIEnv.pAIObjectManager->m_Objects.end();

			if (objectType > 0)
				itObject = gAIEnv.pAIObjectManager->m_Objects.lower_bound(objectType);
			else
				itObject = gAIEnv.pAIObjectManager->m_Objects.begin();

			float fSearchDistSq = fSearchDist * fSearchDist;

			for (; itObject != itEnd; ++itObject)
			{
				if (objectType > 0 && (objectType != itObject->first))
					break;

				CWeakRef<CAIObject> refObject = itObject->second.GetWeakRef();
				CAIObject* pResultObject = refObject.GetAIObject();

				if (!pResultObject || !pResultObject->IsEnabled())
					continue;

				float distSq = objPos.GetSquaredDistance(pResultObject->GetPos());
				if (distSq < fSearchDistSq)
					accumulator.push_back(CTacticalPoint(refObject));
			}
		}
		break;
	case eTPQ_G_PointsInNavigationMesh:
		{
			const NavigationAgentTypeID undefinedNavAgentTypeId;

			NavigationAgentTypeID navAgentTypeId(undefinedNavAgentTypeId);
			Vec3 searchPivotPos(ZERO);

			const string& navAgentTypeName = pOption->GetParams()->sNavigationAgentType;
			if (!navAgentTypeName.empty())
			{
				navAgentTypeId = gAIEnv.pNavigationSystem->GetAgentTypeID(navAgentTypeName.c_str());
			}

			if (IAIActor* pAIActor = context.pAIActor)
			{
				searchPivotPos = context.actorPos;
				if (navAgentTypeId == undefinedNavAgentTypeId)
				{
					navAgentTypeId = pAIActor->GetNavigationTypeID();
				}
			}
			else
			{
				searchPivotPos = objPos;
			}

			if (navAgentTypeId != undefinedNavAgentTypeId)
			{
				AABB searchAABB;
				searchAABB.max = objPos + Vec3(+fSearchDist, +fSearchDist, 0.2f);
				searchAABB.min = objPos + Vec3(-fSearchDist, -fSearchDist, -20.0f);

				if (const NavigationMeshID meshID = gAIEnv.pNavigationSystem->GetEnclosingMeshID(navAgentTypeId, searchPivotPos))
				{
					const size_t maxCenterLocationCount = 512;
					Vec3 centerLocations[maxCenterLocationCount];
					if (size_t locationCount = gAIEnv.pNavigationSystem->GetTriangleCenterLocationsInMesh(meshID, objPos, searchAABB, centerLocations, maxCenterLocationCount, nullptr))
					{
						for (size_t i = 0; i < locationCount; ++i)
						{
							accumulator.push_back(CTacticalPoint(centerLocations[i]));
						}
					}
				}
			}
		}
		break;

	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::BoolProperty(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, bool& result) const
{
	assert(query & eTPQ_FLAG_PROP_BOOL);

	// Set result to false by default to avoid lots of else clauses
	result = false;

	// Attempt default implementations
	bool bHandled = BoolPropertyInternal(query, point, context, result);
	if (!bHandled)
	{
		ITacticalPointLanguageExtender::TBoolParameters parameters(Translate(query), context, result);

		// Allow the language extenders to attempt
		TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
		TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
		for (; !bHandled && itExtender != itExtenderEnd; ++itExtender)
		{
			ITacticalPointLanguageExtender* pExtender = (*itExtender);
			assert(pExtender);

			bHandled = pExtender->BoolProperty(parameters, point);
		}
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::BoolPropertyInternal(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, bool& result) const
{
	assert(query & eTPQ_FLAG_PROP_BOOL);

	// Variables we may need
	const SHideSpot* pHS;

	switch (query)
	{
	case eTPQ_PB_CoverSoft:
		{
			pHS = point.GetHidespot();
			result = pHS ? pHS->IsSecondary() : false;
		}
		break;

	case eTPQ_PB_CoverSuperior:
	case eTPQ_PB_CoverInferior:
		{
			pHS = point.GetHidespot();
			if (pHS->info.type == SHideSpotInfo::eHST_TRIANGULAR && pHS->pObstacle && pHS->pObstacle->IsCollidable())
			{
				result = pHS->pObstacle->fApproxRadius > 0.25f;   // Qualifies as superior
				if (query == eTPQ_PB_CoverInferior) result = !result;
			}
			else if (pHS->info.type == SHideSpotInfo::eHST_ANCHOR)
			{
				result = (query != eTPQ_PB_CoverInferior); // I.e. anchors are always superior
			}
		}
		break;

	case eTPQ_PB_CurrentlyUsedObject:
		{
			result = false;

			if (context.pAIActor)
			{
				CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
				CPipeUser* pPipeUser = pAIActor->CastToCPipeUser();
				if ((pPipeUser != NULL) && pPipeUser->m_CurrentHideObject.IsValid())
				{
					// This isn't quite right I think, because last hidespot doesn't imply you got there
					pHS = point.GetHidespot();
					if (pHS)
					{
						const ObstacleData* pObst = pHS->pObstacle;
						if (pObst && IsEquivalent(pPipeUser->m_CurrentHideObject.GetObjectPos(), pObst->vPos))
						{
							result = true;
						}
					}
				}
			}
		}
		break;

	case eTPQ_PB_Reachable:
		{
			// In principle this could be from an object rather than necessarily the actor

			const Vec3 testPosition(point.GetPos());

			// Succeed the test if either no territory is assigned or if the point is within current territory (which is assumed to be 2D)
			if (context.pAIActor)
			{
				CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);

				SShape* pTerritory = pAIActor->GetTerritoryShape();
				result = (pTerritory == NULL) || pTerritory->IsPointInsideShape(testPosition, false);
			}
		}
		break;

	case eTPQ_PB_IsInNavigationMesh:
		{
			result = false;

			const NavigationSystem* pNavigationSystem = gAIEnv.pNavigationSystem;

			if (pNavigationSystem)
			{
				CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
				if (pAIActor)
				{
					const Vec3 testPosition(point.GetPos());
					result = pNavigationSystem->IsLocationValidInNavigationMesh(pAIActor->GetNavigationTypeID(), testPosition, nullptr);
				}
			}
		}
		break;

	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::BoolTest(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, bool& result) const
{
	assert(query & eTPQ_FLAG_TEST);

	// Set result to false by default to avoid lots of else clauses
	result = false;

	// Attempt default implementations
	bool bHandled = BoolTestInternal(query, object, point, context, result);
	if (!bHandled)
	{
		CAIObject* pObject = NULL;
		Vec3 vObjectPos(ZERO);
		if (GetObject(object, context, pObject, vObjectPos))
		{
			ITacticalPointLanguageExtender::TBoolParameters parameters(Translate(query), context, result);

			// Allow the language extenders to attempt
			TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
			TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
			for (; !bHandled && itExtender != itExtenderEnd; ++itExtender)
			{
				ITacticalPointLanguageExtender* pExtender = (*itExtender);
				assert(pExtender);

				bHandled = pExtender->BoolTest(parameters, pObject, vObjectPos, point);
			}
		}
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::BoolTestInternal(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, bool& result) const
{
	assert(query & eTPQ_FLAG_TEST);

	CAIObject* pObject = NULL;
	Vec3 vObjectPos(ZERO);
	if (!GetObject(object, context, pObject, vObjectPos)) return false;

	Vec3 vTmpA; // Handy temp vector 1
	Vec3 vTmpB; // Handy temp vector 2
	float fTmp;

	// Variables we may need
	//SHideSpot * pHS;

	// Set result to false by default to avoid lots of else clauses
	result = false;

	switch (query)
	{
	case eTPQ_T_Towards:
		// Will the point move us some amount towards the object (not away)
		vTmpA = point.GetPos() - context.actorPos;
		vTmpB = vObjectPos - context.actorPos;
		fTmp = vTmpA.Dot(vTmpB);
		if (fTmp < 0.0f) break;
		// Will it also not pass the object, and not move some silly amount sideways
		if (vTmpA.GetLengthSquared() > vTmpB.GetLengthSquared()) break;
		result = true;
		break;

	case eTPQ_T_CanReachBefore:
		// For now, re-implements part of the old Compromising test, but can be expanded later
		// to use some central prediction code
		vTmpA = point.GetPos() - context.actorPos;
		vTmpB = point.GetPos() - vObjectPos;
		result = (vTmpA.GetLengthSquared() < vTmpB.GetLengthSquared());
		break;

	case eTPQ_T_CrossesLineOfFire:
		{
			Vec3 vDir(ZERO);
			if (pObject)
			{
				vDir = pObject->GetFireDir();
			}
			else
			{
				vDir = GetObjectInternalDir(object, context);
			}

			Vec3 vFireEndPoint = vObjectPos + vDir.normalized() * 30.0f;

			Lineseg lineOfFire(vObjectPos, vFireEndPoint);
			Lineseg directPath(context.actorPos, point.GetPos());

			float tA, tB;
			result = Intersect::Lineseg_Lineseg2D(lineOfFire, directPath, tA, tB);
		}
		break;

	case eTPQ_T_OtherSide:
		{
			const Vec3 objectToActor = (context.actorPos - vObjectPos).GetNormalized();
			const Vec3 objectToPoint = (point.GetPos() - vObjectPos).GetNormalized();

			result = objectToActor.Dot(objectToPoint) < 0.0f;

			break;
		}

	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

AsyncState CTacticalPointSystem::DeferredBoolTest(TTacticalPointQuery query, TTacticalPointQuery object,
                                                  const CTacticalPoint& point, SQueryEvaluation& eval, bool& result) const
{
	assert(query & eTPQ_FLAG_TEST);

	// Set result to false by default to avoid lots of else clauses
	result = false;

	// Attempt default implementations
	ETacticalPointDeferredState state = DeferredBoolTestInternal(query, object, point, eval, result);
	if (state == eTPDS_UnknownRequest)
	{
		CAIObject* pObject = NULL;
		Vec3 vObjectPos(ZERO);

		const QueryContext& context = eval.queryInstance.queryContext;

		if (GetObject(object, context, pObject, vObjectPos))
		{
			ITacticalPointLanguageExtender::TBoolParameters parameters(Translate(query), context, result);

			// Allow the language extenders to attempt
			TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
			TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
			for (; state == eTPDS_UnknownRequest && itExtender != itExtenderEnd; ++itExtender)
			{
				ITacticalPointLanguageExtender* pExtender = (*itExtender);
				assert(pExtender);

				state = pExtender->DeferredBoolTest(parameters, pObject, vObjectPos, point, eval.deferredExtenderCancelFunc);
			}
			if (state != eTPDS_InProgress)
			{
				eval.deferredExtenderCancelFunc = ITacticalPointLanguageExtender::TDeferredCancelFunc();
			}
		}
	}
	switch (state)
	{
	case eTPDS_Failed:
		return AsyncFailed;
	case eTPDS_InProgress:
		return AsyncInProgress;
	case eTPDS_Complete:
		return AsyncComplete;
	case eTPDS_UnknownRequest:
		return AsyncFailed;
	default:
		return AsyncFailed;
	}
}

//----------------------------------------------------------------------------------------------//

ETacticalPointDeferredState CTacticalPointSystem::DeferredBoolTestInternal(TTacticalPointQuery query, TTacticalPointQuery object,
                                                                           const CTacticalPoint& point, SQueryEvaluation& eval, bool& result) const
{
	assert(query & eTPQ_FLAG_TEST);

	const QueryContext& context = eval.queryInstance.queryContext;

	CAIObject* pObject = NULL;
	Vec3 vObjectPos(ZERO);
	if (!GetObject(object, eval.queryInstance.queryContext, pObject, vObjectPos))
		return eTPDS_Failed;

	Vec3 vTmpA; // Handy temp vector 1
	Vec3 vTmpB; // Handy temp vector 2

	switch (query)
	{
	case eTPQ_T_Visible:
		{
			if (eval.visibleResult != -1)
			{
				result = eval.visibleResult != 0;
				eval.visibleResult = -1;
				eval.visibleRayID = 0;

				return eTPDS_Complete;
			}
			else if (!eval.visibleRayID)
			{
				Vec3 vWaistPos = point.GetPos() + Vec3(0, 0, 1.0f);
				Vec3 vDelta = vObjectPos - vWaistPos;

				eval.visibleRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighestPriority,
				                                             RayCastRequest(
				                                               vWaistPos, vDelta, COVER_OBJECT_TYPES,
				                                               AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER),
				                                             functor(*const_cast<CTacticalPointSystem*>(this), &CTacticalPointSystem::VisibleRayComplete));

				if (CVars.DebugTacticalPoints != 0 && gAIEnv.CVars.DebugDraw != 0)
				{
					IPersistantDebug* debug = gEnv->pGameFramework->GetIPersistantDebug();
					debug->Begin("eTPQ_T_Visible", false);
					debug->AddLine(vWaistPos, vWaistPos + vDelta, (result ? Col_Green : Col_Red), 10.0f);
				}
			}

			return eTPDS_InProgress;
		}
		break;

	case eTPQ_T_CanShoot:
		{
			// Does this really work for a range of characters?

			if (context.pAIActor)
			{
				if (eval.canShootResult != -1)
				{
					result = eval.canShootResult != 0;
					eval.canShootResult = -1;
					eval.canShootRayID = 0;

					return eTPDS_Complete;
				}
				else if (!eval.canShootRayID)
				{
					CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);

					const SAIBodyInfo& bodyInfo = pAIActor->GetBodyInfo();
					const Vec3 vWeaponPos = bodyInfo.vFirePos;
					const Vec3 vWeaponDelta = vWeaponPos - pAIActor->GetPhysicsPos();

					// Project weapon delta onto test point
					const Vec3 vPoint = point.GetPos() + vWeaponDelta;
					Vec3 vDelta = vObjectPos - vPoint;

					eval.canShootRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighestPriority,
					                                              RayCastRequest(
					                                                vPoint, vDelta, COVER_OBJECT_TYPES,
					                                                AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER),
					                                              functor(*const_cast<CTacticalPointSystem*>(this), &CTacticalPointSystem::CanShootRayComplete));

					if (CVars.DebugTacticalPoints != 0 && gAIEnv.CVars.DebugDraw != 0)
					{
						IPersistantDebug* debug = gEnv->pGameFramework->GetIPersistantDebug();
						debug->Begin("eTPQ_T_CanShoot", false);
						debug->AddLine(vPoint, vPoint + vDelta, Col_Blue, 10.0f);
					}

				}

				return eTPDS_InProgress;
			}
		}
		break;

	case eTPQ_T_CanShootTwoRayTest:
		{
			if (eval.canShootResult != -1 && eval.canShootSecondRayResult != -1)
			{
				result = (eval.canShootResult != 0 && eval.canShootSecondRayResult != 0);
				eval.canShootResult = -1;
				eval.canShootRayID = 0;
				eval.canShootSecondRayResult = -1;
				eval.canShootSecondRayID = 0;

				return eTPDS_Complete;
			}
			else if (!eval.canShootRayID && !eval.canShootSecondRayID)
			{
				const float horizontalDestinationOffset = 0.15f;
				float horizontalSourceOffset = 4.0f;

				const CTacticalPointQuery* pQuery = GetQuery(eval.queryInstance.nQueryID);
				CRY_ASSERT(pQuery);

				const int nOption = eval.iCurrentQueryOption;
				const COptionCriteria* pOption = pQuery->GetOption(nOption);
				if (pOption)
				{
					horizontalSourceOffset = pOption->GetParams()->fHorizontalSpacing;
				}

				Vec3 vRight;
				Vec3 vWeaponPosition;

				const Vec3 pointPos = point.GetPos();

				if (context.pAIActor)
				{
					const CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
					const SAIBodyInfo& bodyInfo = pAIActor->GetBodyInfo();
					vRight = pAIActor->GetEntity()->GetWorldTM().GetColumn0();
					const Vec3 vWeaponPos = bodyInfo.vFirePos;
					vWeaponPosition = pAIActor->GetPos() - vWeaponPos;
				}
				else
				{
					// cross forwardVec with upVec to get rightVec
					const Vec3 pointToObjectDelta = vObjectPos - pointPos;
					const Vec3 zAxis = Vec3Constants<float>::fVec3_OneZ;
					vRight = (pointToObjectDelta % zAxis).GetNormalized();

					vWeaponPosition = Vec3(0, 0, 1.0f);
				}

				const Vec3 vWeaponSideOffset = vRight * horizontalSourceOffset;
				const Vec3 vWeaponDeltaRight = vWeaponPosition + vWeaponSideOffset;
				const Vec3 vWeaponDeltaLeft = vWeaponPosition - vWeaponSideOffset;

				const Vec3 vPointRight = pointPos + vWeaponDeltaRight;
				const Vec3 vPointLeft = pointPos + vWeaponDeltaLeft;
				const Vec3 vDeltaOffset = vRight * horizontalDestinationOffset;
				const Vec3 vDeltaRight = vObjectPos + vDeltaOffset - vPointRight;
				const Vec3 vDeltaLeft = vObjectPos - vDeltaOffset - vPointLeft;

				eval.canShootRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighestPriority,
				                                              RayCastRequest(
				                                                vPointLeft, vDeltaLeft, COVER_OBJECT_TYPES,
				                                                AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER),
				                                              functor(*const_cast<CTacticalPointSystem*>(this), &CTacticalPointSystem::CanShootRayComplete));

				eval.canShootSecondRayID = gAIEnv.pRayCaster->Queue(RayCastRequest::HighestPriority,
				                                                    RayCastRequest(
				                                                      vPointRight, vDeltaRight, COVER_OBJECT_TYPES,
				                                                      AI_VISION_RAY_CAST_FLAG_BLOCKED_BY_SOLID_COVER),
				                                                    functor(*const_cast<CTacticalPointSystem*>(this), &CTacticalPointSystem::CanShootSecondRayComplete));

				if (CVars.DebugTacticalPoints != 0 && gAIEnv.CVars.DebugDraw != 0)
				{
					IPersistantDebug* debug = gEnv->pGameFramework->GetIPersistantDebug();
					debug->Begin("eTPQ_T_CanShootTwoRayTest", false);
					debug->AddLine(vPointRight, vPointRight + vDeltaRight, Col_Blue, 10.0f);
					debug->AddLine(vPointLeft, vPointLeft + vDeltaLeft, Col_Blue, 10.0f);
				}
			}

			return eTPDS_InProgress;
		}
		break;

	case eTPQ_T_HasShootingPosture:
		{
			if (context.pAIActor)
			{
				CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
				if (CPuppet* pPuppet = pAIActor->CastToCPuppet())
				{
					if (!eval.postureQueryID)
					{
						PostureManager::PostureQuery postureQuery;
						postureQuery.actor = pAIActor;
						postureQuery.position = point.GetPos();
						postureQuery.target = vObjectPos;
						postureQuery.checks = PostureManager::CheckVisibility | PostureManager::CheckAimability;
						postureQuery.allowLean = true;
						postureQuery.allowProne = false;
						postureQuery.distancePercent = 1.0f;
						postureQuery.type = PostureManager::AimPosture;

						if (point.GetType() == ITacticalPoint::eTPT_CoverID)
							postureQuery.coverID = point.GetCoverID();

						eval.postureQueryID = pPuppet->GetPostureManager().QueryPosture(postureQuery);
					}
					else
					{
						PostureManager::PostureID postureID;
						PostureManager::PostureInfo* postureInfo;

						AsyncState state = pPuppet->GetPostureManager().GetPostureQueryResult(eval.postureQueryID, &postureID,
						                                                                      &postureInfo);

						if (state != AsyncInProgress)
							eval.postureQueryID = 0;

						if (state == AsyncComplete)
						{
							result = true;
							return eTPDS_Complete;
						}
						else if (state == AsyncFailed)
						{
							result = false;
							return eTPDS_Complete;
						}
					}

					if (!eval.postureQueryID)
					{
						result = false;
						return eTPDS_Complete;
					}
				}
			}
		}
		break;

	default:
		return eTPDS_UnknownRequest;
	}

	return eTPDS_InProgress;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RealProperty(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, float& result) const
{
	assert(query & eTPQ_FLAG_PROP_REAL);

	// Set result to 0 by default to avoid lots of else clauses
	result = 0.0f;

	// Attempt default implementations
	bool bHandled = RealPropertyInternal(query, point, context, result);
	if (!bHandled)
	{
		ITacticalPointLanguageExtender::TRealParameters parameters(Translate(query), context, result);

		// Allow the language extenders to attempt
		TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
		TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
		for (; !bHandled && itExtender != itExtenderEnd; ++itExtender)
		{
			ITacticalPointLanguageExtender* pExtender = (*itExtender);
			assert(pExtender);

			bHandled = pExtender->RealProperty(parameters, point);
		}
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RealPropertyInternal(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, float& result) const
{
	assert(query & eTPQ_FLAG_PROP_REAL);

	result = 0.0f;

	switch (query)
	{
	case eTPQ_PR_CoverRadius:
		break;

	case eTPQ_PR_CoverDensity:
		{
			assert(false);
			// INTEGRATION : (MATT) Translating this is postsponed until the basics are running {2007/08/23:16:17:01}
			/*

			   // This should move out to a method library
			   // Also, it should be more general in types of navigation and hidespot
			   // and it could potentially use some precomputation/caching

			   // Check if this point is valid for our query
			   const SHideSpot *pHideSpot = point.GetHidespot();
			   if (!pHideSpot)  break;
			   if (!pHideSpot->pNavNodes) break;

			   // Form a set of unique object indices relevant to this location
			   // Effectively this goes 2 levels through the navgraph
			   CGraphLinkManager& linkManager = gAIEnv.pGraph->GetLinkManager();
			   set<int> obstacleIdxSet;
			   vector<const GraphNode *>::const_iterator itN = pHideSpot->pNavNodes->begin();
			   for (; itN != pHideSpot->pNavNodes->end(); itN++)
			   {
			   // Fetch this nearby node and skip if not valid for this query
			   const GraphNode * pNode = (*itN);
			   if (! (pNode->navType & IAISystem::NAV_TRIANGULAR)) continue;

			   const STriangularNavData *pData = pNavNode->GetTriangularNavData();

			   const ObstacleIndexVector::const_iterator vertEnd = pData->vertices.end();
			   for (ObstacleIndexVector::const_iterator vertIt = pData->vertices.begin() ; vertIt != vertEnd ; ++vertIt)
			   {



			   }





			   const GraphNode* pPathfinderCurrent = m_pathfinderCurrent->graphNode;//m_request.m_pGraph->GetNodeManager().GetNode(m_pathfinderCurrent);
			   for (unsigned linkId = pPathfinderCurrent->firstLinkIndex ; linkId ; linkId = m_request.m_pGraph->GetLinkManager().GetNextLink(linkId))
			   {
			   unsigned nextNodeIndex = m_request.m_pGraph->GetLinkManager().GetNextNode(linkId);
			   const GraphNode* nextNode = m_request.m_pGraph->GetNodeManager().GetNode(nextNodeIndex);


			   // Run through links to all the neighboring nodes of this node
			   unsigned nLinks = pNode->GetLinkCount(linkManager);
			   for (unsigned iLink = 0 ; iLink < nLinks ; ++iLink)
			   {
			    // Fetch and check this node
			    const GraphLink& link = pNode->GetLinkIndex()
			    const GraphNode* pNext = link.pNextNode;
			    if (! (pNext->navType & IAISystem::NAV_TRIANGULAR))
			      continue;

			    // Insert all its obstacles into the set
			    const ObstacleIndexVector &vObstacles = pNext->GetTriangularNavData()->vertices;
			    obstacleIdxSet.insert(vObstacles.begin(), vObstacles.end());
			   }
			   }

			   // Start at 0, but a conventional hidepoint will always have at least one obstacle very close by.
			   // We could consider radius here, and many other things. Could become a very complex query.
			   float fDensity = 0.0f;
			   CAISystem *pAISystem = GetAISystem();
			   for (set<int>::const_iterator itO = obstacleIdxSet.begin(); itO != obstacleIdxSet.end(); itO++)
			   {
			   const ObstacleData &od = pAISystem->GetObstacle(*itO);
			   float fDist = od.vPos.GetDistance(point.GetPos());
			   if (fDist > 8.0f) continue;						// Ignore obstacles that are miles away
			   if (fDist < 2.0f) fDist = 2.0f;				// Don't allow one very close obstacle to skew results
			   fDensity += 1.0f / (fDist * fDist);		// Squaring might be a little excessive
			   }
			   // Finally, write the result
			   result = fDensity;
			 */
		}
		break;

	case eTPQ_PR_BulletImpacts:
		break;

	case eTPQ_PR_CameraCenter:
		{
			CDebugDrawContext dc;

			// Halved screen dimensions, in floats
			float fWidth = static_cast<float>(dc->GetWidth());
			float fHeight = static_cast<float>(dc->GetHeight());

			Vec3 vPos = point.GetPos();
			float fX, fY, fZ;
			// fX, fY are in range [0, 100]
			if (!dc->ProjectToScreen(vPos.x, vPos.y, vPos.z, &fX, &fY, &fZ))
			{
				result = -1;
				break;
			}

			fX *= fWidth * 0.01f;
			fY *= fHeight * 0.01f;

			// [2/5/2009 evgeny] Reject points that are behind the frustrum near plane
			const CCamera& camera = GetISystem()->GetViewCamera();
			Vec3 vCameraNormalizedDirection = camera.GetViewdir().normalize();
			Vec3 vPointRelativeToCameraNearPlane =
			  point.GetPos() - camera.GetPosition() - camera.GetNearPlane() * vCameraNormalizedDirection;
			if (vCameraNormalizedDirection.dot(vPointRelativeToCameraNearPlane) <= 0)
			{
				result = -1;
			}
			else
			{
				// [2/5/2009 evgeny] Make fX, fY be from range [0, 1]
				fX = 0.02f * fabs(fX - 50.0f);
				fY = 0.02f * fabs(fY - 50.0f);

				// The result is positive for the points on the screen
				result = 1.0f - max(fX, fY);

				if (result < -1)
					result = -1;
			}
		}
		break;

	case eTPQ_PR_Random:
		result = cry_random(0.0f, 1.0f);
		break;

	case eTPQ_PR_Type:
		{
			const CAIObject* pObj = point.GetAI().GetAIObject();
			result = (pObj ? static_cast<float>(pObj->GetType()) : 0.0f);
		}
		break;

	case eTPQ_PR_HostilesDistance:
	case eTPQ_PR_FriendlyDistance:
		{
			// Find some AIs, using a generous range
			// Choose the nearest hostile one and return a distance metric
			// If none, return the generous range
			const float fRange = 100.0f;
			Vec3 vPointPos = point.GetPos();
			AutoAIObjectIter iter(
			  gAIEnv.pAIObjectManager->GetFirstAIObjectInRange(OBJFILTER_TYPE, 0, context.actorPos, fRange, true));
			IAIObject* pOtherObj = iter->GetObject();
			float fMinDistSq = fRange * fRange;
			// This isn't a nice loop - clean it up
			while (pOtherObj)
			{
				EntityId otherObjectId = pOtherObj->GetEntityID();
				iter->Next();
				pOtherObj = iter->GetObject();
				if (!pOtherObj || !otherObjectId || otherObjectId == context.actorEntityId) continue;

				// Check whether this actor is relevant, based on the specific flavour of query
				bool bHostile = IsHostile(context.actorEntityId, otherObjectId);
				if (((eTPQ_PR_HostilesDistance == query) && bHostile) || ((eTPQ_PR_FriendlyDistance == query) && !bHostile))
				{
					float fDistSq = pOtherObj->GetPos().GetSquaredDistance(vPointPos);
					if (fDistSq < fMinDistSq)
					{
						fMinDistSq = fDistSq;
					}
				}
			}
			result = sqrtf(fMinDistSq);
		}
		break;

	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//
bool CTacticalPointSystem::IsHostile(EntityId entityId1, EntityId entityId2) const
{
	assert(entityId1 > 0);
	assert(entityId2 > 0);

	bool bRet = false;

	if (entityId1 != entityId2)
	{
		IEntitySystem* pEntitySystem = gEnv->pEntitySystem;
		assert(pEntitySystem);
		IEntity* pEntity1 = pEntitySystem->GetEntity(entityId1);
		IEntity* pEntity2 = pEntitySystem->GetEntity(entityId2);

		// Check their actors if available
		const CAIActor* pActor1 = CastToCAIActorSafe(pEntity1 ? pEntity1->GetAI() : 0);
		const CAIActor* pActor2 = CastToCAIActorSafe(pEntity2 ? pEntity2->GetAI() : 0);
		if (pActor1 != NULL && pActor2 != NULL)
			return pActor1->IsHostile(pActor2);
	}

	return(bRet);
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RealMeasure(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, float& result) const
{
	assert(query & eTPQ_FLAG_MEASURE);

	// Set result to 0 by default to avoid lots of else clauses
	result = 0.0f;

	// Attempt default implementations
	bool bHandled = RealMeasureInternal(query, object, point, context, result);
	if (!bHandled)
	{
		CAIObject* pObject = NULL;
		Vec3 vObjectPos(ZERO);
		if (GetObject(object, context, pObject, vObjectPos))
		{
			ITacticalPointLanguageExtender::TRealParameters parameters(Translate(query), context, result);

			// Allow the language extenders to attempt
			TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
			TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
			for (; !bHandled && itExtender != itExtenderEnd; ++itExtender)
			{
				ITacticalPointLanguageExtender* pExtender = (*itExtender);
				assert(pExtender);

				bHandled = pExtender->RealMeasure(parameters, pObject, vObjectPos, point);
			}
		}
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RealMeasureInternal(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, float& result) const
{
	assert(query & eTPQ_FLAG_MEASURE);

	CAIObject* pObject = 0;
	Vec3 vObjectPos(ZERO);
	if (!GetObject(object, context, pObject, vObjectPos)) return false;

	Vec3 vTmpA; // Handy temp vector 1
	Vec3 vTmpB; // Handy temp vector 2

	switch (query)
	{
	case eTPQ_M_Distance:
		result = point.GetPos().GetDistance(vObjectPos);
		break;

	case eTPQ_M_Distance2d:
		result = sqrt_tpl(point.GetPos().GetSquaredDistance2D(vObjectPos));
		break;

	case eTPQ_M_ChangeInDistance: // _from_target...
		result = point.GetPos().GetDistance(vObjectPos) - context.actorPos.GetDistance(vObjectPos);
		break;

	case eTPQ_M_DistanceInDirection:  // _of_target...
	case eTPQ_M_DistanceLeft:         // _of_target...
		assert(object != eTPQ_O_Actor); // Doesn't make sense as an object here
		vTmpA = (vObjectPos - context.actorPos).GetNormalized();
		if (eTPQ_M_DistanceLeft == query)
			vTmpA = vTmpA.Cross(Vec3(0.0f, 0.0f, 1.0f)); // Rotate through 90 degrees to form a left-vector
		vTmpB = (point.GetPos() - context.actorPos);
		result = vTmpB.Dot(vTmpA);
		break;

	case eTPQ_M_RatioOfDistanceFromActorAndDistance:
		result = (point.GetPos() - context.actorPos).GetLength2D() / (vObjectPos - context.actorPos).GetLength2D();
		break;

	case eTPQ_M_PathDistance:
		assert(false);
		break;

	case eTPQ_M_Directness:
		// What does the result mean? For example:
		// 1 means totally direct. 0.5 means that for every 2 metres travelled, we are 1 metre closer to the object.
		// 0 means we will end up no closer, or further away, from the object. -ve values [0 to -1] mean moving away.
		assert(object != eTPQ_O_Actor);    // Doesn't make sense as an object here
		// We assume direct paths here for now
		vTmpA = vObjectPos - context.actorPos;      // Current vector to object
		vTmpB = vObjectPos - point.GetPos();        // Vector to object from the considered point
		// Divide the progress we would make towards the object by the distance we would go
		{
			float distance2D = (point.GetPos() - context.actorPos).GetLength2D();
			if (distance2D > 0.0f)
				result = (vTmpA.GetLength2D() - vTmpB.GetLength2D()) / distance2D;
			else
				result = 0.0f;
		}
		break;

	case eTPQ_M_Dot:
		vTmpA = vObjectPos - context.actorPos;        // Current vector to object
		vTmpB = point.GetPos() - context.actorPos;    // Vector to the point from actor

		vTmpA.Normalize();
		vTmpB.Normalize();

		result = vTmpA.Dot(vTmpB);
		break;

	case eTPQ_M_ObjectsDot:
		{
			vTmpA = point.GetPos() - vObjectPos;    // Vector to the point from reference object
			vTmpA.Normalize();

			Vec3 vDir(ZERO);
			if (pObject)
				vDir = pObject->GetEntityDir();
			else
				vDir = GetObjectInternalDir(object, context);
			result = vDir.Dot(vTmpA);   // dot of computed vector and reference object's direction
		}
		break;

	case eTPQ_M_ObjectsMoveDirDot:
		{
			vTmpA = point.GetPos() - vObjectPos;    // Vector to the point from reference object
			vTmpA.Normalize();

			Vec3 vDir(ZERO);
			if (pObject)
				vDir = pObject->GetMoveDir();
			else
				vDir = GetObjectInternalDir(object, context);
			result = vDir.Dot(vTmpA);   // dot of computed vector and reference object's direction
		}
		break;

	case eTPQ_M_HeightRelative:
		{
			float fObjectZ = vObjectPos.z;
			float fPointZ = point.GetPos().z;
			result = fPointZ - fObjectZ;
		}
		break;

	case eTPQ_M_AngleOfElevation:
		{
			vTmpA = point.GetPos() - vObjectPos;
			float fPlanarDist = sqrt(vTmpA.x * vTmpA.x + vTmpA.y * vTmpA.y);
			float fRad = atan2_tpl(vTmpA.z, fPlanarDist);
			result = RAD2DEG(fRad);
		}
		break;

	case eTPQ_M_PointDirDot:
		{
			const CAIObject* pPointObject = point.GetAI().GetAIObject();
			result = 0.0f;
			if (!pPointObject)
				break;

			vTmpA = pPointObject->GetEntityDir();  // Should be normalised
			vTmpB = (vObjectPos - point.GetPos()).GetNormalizedSafe();
			result = vTmpA.Dot(vTmpB);
		}
		break;

	case eTPQ_M_CoverHeight:
		{
			gAIEnv.pCoverSystem->GetCoverLocation(point.GetCoverID(), 0.0f, &result);
		}
		break;

	case eTPQ_M_EffectiveCoverHeight:
		{
			Vec3 location = gAIEnv.pCoverSystem->GetCoverLocation(point.GetCoverID(), context.actorRadius);
			const CoverSurface& surface = gAIEnv.pCoverSystem->GetCoverSurface(point.GetCoverID());
			if (surface.GetCoverOcclusionAt(vObjectPos, location, 0, &result))
			{
				result = sqrt_tpl(result);
				break;
			}
			result = 0.0f;
		}
		break;

	case eTPQ_M_DistanceToRay:
		{
			// Obtain the ray normal direction.
			if (pObject != NULL)
			{
				vTmpA = pObject->GetEntityDir();
			}
			else
			{
				vTmpA = GetObjectInternalDir(object, context);
			}

			// Compute the smallest distance to the ray.
			vTmpB = point.GetPos() - vObjectPos;    // Vector to the point from reference object
			const float distanceFromRayPos = vTmpA.Dot(vTmpB);
			if (distanceFromRayPos <= 0.0f)
			{
				// Closest to the ray start position.
				result = vTmpB.GetLength();
			}
			else
			{
				// Somewhere on the ray.
				const float distanceToRaySq = vTmpB.GetLengthSquared() - (distanceFromRayPos * distanceFromRayPos);
				if (distanceToRaySq > 0.0f)
				{
					result = sqrt_tpl(distanceToRaySq);
				}
				else
				{
					// Suppress numerical noise.
					result = 0.0f;
				}
			}
		}
		break;

	case eTPQ_M_AbsDistanceToPlaneAtClosestRayPos:
		{
			// Obtain the ray normal direction.
			if (pObject != NULL)
			{
				vTmpA = pObject->GetEntityDir();
			}
			else
			{
				vTmpA = GetObjectInternalDir(object, context);
			}

			// Compute the distance of the actor's closest point on the ray.
			const float actorClosestDistanceFromRayPos = vTmpA.Dot(context.actorPos - vObjectPos);

			// Compute the distance of the tactical point's closest point on the ray.
			const float tacticalClosestDistanceFromRayPos = vTmpA.Dot(point.GetPos() - vObjectPos);

			// And then just measure the difference.
			result = fabs_tpl(actorClosestDistanceFromRayPos - tacticalClosestDistanceFromRayPos);
		}
		break;

	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RealRange(TTacticalPointQuery query, float& min, float& max) const
{
	// Only makes sense for Real queries
	assert(query & (eTPQ_FLAG_MEASURE | eTPQ_FLAG_PROP_REAL));

	// Attempt default implementations
	bool bHandled = RealRangeInternal(query, min, max);
	if (!bHandled)
	{
		ITacticalPointLanguageExtender::TRangeParameters parameters(Translate(query), min, max);

		// Allow the language extenders to attempt
		TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
		TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
		for (; !bHandled && itExtender != itExtenderEnd; ++itExtender)
		{
			ITacticalPointLanguageExtender* pExtender = (*itExtender);
			assert(pExtender);

			bHandled = pExtender->RealRange(parameters);
		}
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::RealRangeInternal(TTacticalPointQuery query, float& min, float& max) const
{
	// Only makes sense for Real queries
	assert(query & (eTPQ_FLAG_MEASURE | eTPQ_FLAG_PROP_REAL));

	// Should always set both min and max unless fail
	// It's ok if exceptional cases fall outside this range,
	// but they will be capped to the limits, so will be less useful

	// For queries that return a distance-related measure, use a common number for the max range
	const float fDistanceMax = 50.0f;

	switch (query)
	{
	case eTPQ_PR_CoverRadius:
		min = 0.1f;
		max = 5.0f;
		break;

	case eTPQ_PR_CameraCenter:
		min = -1.0f;
		max = 1.0f;
		break;

	case eTPQ_PR_Random:
		min = 0.0f;
		max = 1.0f;
		break;

	case eTPQ_PR_Type:
		min = 0.0f;
		max = 1.0f;
		break;

	case eTPQ_PR_HostilesDistance:
	case eTPQ_PR_FriendlyDistance:
		min = 0.0f;
		max = fDistanceMax;
		break;

	case eTPQ_PR_CoverDensity:
		min = 0.1f;
		max = 5.0f;
		break;

	case eTPQ_PR_BulletImpacts:
		// This is a pretty arbitrary value anyway
		min = 0.0f;
		max = 1.0f;
		break;

	case eTPQ_M_Distance:
		// Sensible for the gameplay, but might be altered to be dependant on query range
		min = 0.0f;
		max = fDistanceMax;
		break;

	case eTPQ_M_Distance2d:
		min = 0.0f;
		max = 1000.0f;
		break;

	case eTPQ_M_PathDistance:
	case eTPQ_M_ChangeInDistance:
	case eTPQ_M_DistanceInDirection:
	case eTPQ_M_DistanceLeft:
	case eTPQ_M_DistanceToRay:
	case eTPQ_M_AbsDistanceToPlaneAtClosestRayPos:
		// Sensible for the gameplay, but might be altered to be dependant on query range
		min = 0.0f;
		max = fDistanceMax;
		break;

	case eTPQ_M_Dot:
	case eTPQ_M_ObjectsDot:
	case eTPQ_M_ObjectsMoveDirDot:
	case eTPQ_M_PointDirDot:
	case eTPQ_M_Directness:
		// Most distance you can gain is 100% of the distance travelled
		// Can go in opposite direction as far as you like, but not usually helpful
		min = -1.0f;
		max = 1.0f;
		break;

	case eTPQ_M_RatioOfDistanceFromActorAndDistance:
		// (MATT) Check if we can remove this query, or add limits {2009/06/30}
		break;

	case eTPQ_M_HeightRelative:
		min = -20.0f;
		max = 20.0f;                                           // That should be ample for vertical gameplay
		break;

	case eTPQ_M_AngleOfElevation:
		min = -90.0f;
		max = 90.0f;                                           // Use degrees, don't consider direction (so no obtuse angles going backwards)
		break;

	case eTPQ_M_CoverHeight:
	case eTPQ_M_EffectiveCoverHeight:
		min = 0.0f;
		max = 20.0f;
		break;
	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::Limit(TTacticalPointQuery limit, float fAbsoluteQueryResult, float fComparisonValue) const
{
	assert(limit & eTPQ_MASK_LIMIT);
	switch (limit)
	{
	case eTPQ_L_Max:
		return (fAbsoluteQueryResult < fComparisonValue);

	case eTPQ_L_Min:
		return (fAbsoluteQueryResult > fComparisonValue);

	case eTPQ_L_Equal:
		return (fabs(fAbsoluteQueryResult - fComparisonValue) < 0.00001f);
	}

	assert(false);
	return false;
}

//----------------------------------------------------------------------------------------------//

inline bool CTacticalPointSystem::GetObject(TTacticalPointQuery object, const QueryContext& context, CAIObject*& pObject, Vec3& vObjPos) const
{
	pObject = 0;
	vObjPos.zero();

	// Attempt default implementations
	bool bHandled = GetObjectInternal(object, context, pObject, vObjPos);
	if (!bHandled)
	{
		IAIObject* pIObject = pObject;

		// (MATT) Ideally, the extenders would be fetched just once a frame as AIRefs and cached {2009/12/01}
		ITacticalPointLanguageExtender::TObjectParameters parameters(Translate(object), context, pIObject);

		// Allow the language extenders to attempt
		TLanguageExtenders::const_iterator itExtender = m_LanguageExtenders.begin();
		TLanguageExtenders::const_iterator itExtenderEnd = m_LanguageExtenders.end();
		for (; itExtender != itExtenderEnd; ++itExtender)
		{
			ITacticalPointLanguageExtender* pExtender = (*itExtender);
			assert(pExtender);

			bHandled = pExtender->GetObject(parameters);
			if (bHandled)
			{
				pObject = static_cast<CAIObject*>(pIObject);
				if (pObject)
					vObjPos = pObject->GetPos();
				break;
			}
		}
	}

	return bHandled;
}

//----------------------------------------------------------------------------------------------//

inline bool CTacticalPointSystem::GetObjectInternal(TTacticalPointQuery object, const QueryContext& context, CAIObject*& pObject, Vec3& vObjPos) const
{
	// Early out if nothing is requested
	if (eTPQ_None == object || eTPQ_O_None == object)
		return true;

	switch (object)
	{
	case eTPQ_O_Entity:
		{
			pObject = 0;
			IEntity* entity = gEnv->pEntitySystem->GetEntity(context.actorEntityId);
			if (!entity)
				return false;

			vObjPos = entity->GetPos();
		}
		break;

	case eTPQ_O_Actor:
		if (context.pAIActor)
		{
			pObject = static_cast<CAIActor*>(context.pAIActor);
			vObjPos = (pObject ? pObject->GetPos() : Vec3Constants<float>::fVec3_Zero);
		}
		else
		{
			pObject = 0;
			vObjPos = context.actorPos;
		}
		break;

	case eTPQ_O_AttentionTarget:
		// If we don't have a actor, use what was passed in
		if (!context.pAIActor)
		{
			pObject = 0;
			vObjPos = context.attentionTarget;
			break;
		}
	case eTPQ_O_RealTarget:
		// Otherwise if we do have a actor or need the real target, we need the actor
		if (context.pAIActor)
		{
			CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
			IAIObject* pAttentionTarget = pAIActor->GetAttentionTarget();
			if (pAttentionTarget && eTPQ_O_RealTarget == object)
			{
				// Use association of target if available
				CWeakRef<IAIObject> pAssociation = ((CAIObject*)pAttentionTarget)->GetAssociation();
				pAttentionTarget = (pAssociation.IsValid() ? pAssociation.GetAIObject() : pAttentionTarget);
			}
			if (pAttentionTarget)
			{
				pObject = static_cast<CAIObject*>(pAttentionTarget);
				vObjPos = pAttentionTarget->GetPos();
			}
			break;
		}
		return false;

	case eTPQ_O_ReferencePoint:
		if (context.pAIActor)
		{
			CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
			CPipeUser* pPipeUser = pAIActor->CastToCPipeUser();
			if (pPipeUser != NULL)
			{
				CAIObject* pRefPoint = pPipeUser->GetRefPoint();
				if (pRefPoint)
				{
					pObject = static_cast<CAIObject*>(pRefPoint);
					vObjPos = pRefPoint->GetPos();
				}
			}
		}
		if (!pObject)
			vObjPos = context.referencePoint;
		break;

	case eTPQ_O_ReferencePointOffsettedByItsForwardDirection:
		{
			assert(!context.referencePoint.IsZero());
			assert(!context.referencePointDir.IsZero());

			Vec3 position = context.referencePoint;
			Vec3 direction = context.referencePointDir;
			direction.Normalize();
			vObjPos = position + (direction * 10.0f);
		}
		break;

	case eTPQ_O_LastOp:
		if (context.pAIActor)
		{
			CAIActor* pAIActor = static_cast<CAIActor*>(context.pAIActor);
			CPipeUser* pPipeUser = pAIActor->CastToCPipeUser();
			if (pPipeUser && (pObject = pPipeUser->GetLastOpResult()))
				vObjPos = pObject->GetPos();
			else
				vObjPos.zero();
			break;
		}
		return false;

	case eTPQ_O_Player:
		pObject = GetAISystem()->GetPlayer();
		vObjPos = (pObject ? pObject->GetPos() : Vec3Constants<float>::fVec3_Zero);
		break;

	default:
		return false;
	}

	return true;
}

//----------------------------------------------------------------------------------------------//

inline Vec3 CTacticalPointSystem::GetObjectInternalDir(TTacticalPointQuery object, const QueryContext& context) const
{
	// Need object position

	switch (object)
	{
	case eTPQ_O_Actor:
		return context.actorDir;
		break;

	case eTPQ_O_AttentionTarget:
		return context.attentionTargetDir;
		break;

	case eTPQ_O_ReferencePoint:
		return context.referencePointDir;
		break;

	case eTPQ_O_RealTarget:
		return context.realTargetDir;

	case eTPQ_None:
	case eTPQ_O_None:
		// Requests no object
		break;
	default:
		assert(false);
	}

	// Obj can be NULL if the actor doesn't have an object of that type right now
	// It is cast to a CAIObject for more flexibility, should always be safe.
	return Vec3(0.0f, 0.0f, 1.0f);
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::Parse(const char* sSpec, TTacticalPointQuery& _query, TTacticalPointQuery& _limits,
                                 TTacticalPointQuery& _object, TTacticalPointQuery& _objectAux) const
{
	string sInput(sSpec);
	const int MAXWORDS = 8;
	string sWords[MAXWORDS];

	int iC = 0, iWord = 0;

	for (; iWord < MAXWORDS; iWord++)
	{
		sWords[iWord] = sInput.Tokenize("_", iC);
		if (sWords[iWord].empty())
		{
			break;
		}
	}

	TTacticalPointQuery token;
	iWord = 0;
	token = Translate(sWords[iWord++]);   // Need to tighten this up
	if (token == eTPQ_None) return false;

	// Assemble in a buffer first
	TTacticalPointQuery query = eTPQ_None, limits = eTPQ_None, object = eTPQ_None, objectAux = eTPQ_None;

	// Assign limits if any
	if (token & eTPQ_MASK_LIMIT)
	{
		limits = token;                     // Store
		token = Translate(sWords[iWord++]); // Advance
	}
	else limits = eTPQ_None;

	// Word should be a valid query of some type
	if (!(token & eTPQ_MASK_QUERY_TYPE)) return false;
	query = token;            // Store

	// What we do after this depends on the type of query

	// These all expect a "from" Object
	if (query & (eTPQ_FLAG_TEST | eTPQ_FLAG_MEASURE | eTPQ_FLAG_GENERATOR_O))
	{
		token = Translate(sWords[iWord++]);   // Advance
		if (token != eTPQ_Glue) return false; // Expect from/to/at etc
		token = Translate(sWords[iWord++]);   // Advance
		if (!(token & eTPQ_MASK_OBJECT)) return false;

		// The Object is either primary or auxiliary
		if (query & eTPQ_FLAG_GENERATOR_O)
			objectAux = token;
		else
			object = token;
	}

	// These expect an "around" Object at this stage
	if (query & (eTPQ_FLAG_GENERATOR | eTPQ_FLAG_GENERATOR_O))
	{
		token = Translate(sWords[iWord++]);     // Advance
		if (token != eTPQ_Around) return false; // The special Generator
		token = Translate(sWords[iWord++]);     // Advance
		if (!(token & eTPQ_MASK_OBJECT)) return false;

		// This is the primary Object
		object = token;
	}

	// Final shared check that there are no more words
	if (!sWords[iWord].empty()) return false;

	// Write only on successful parsing
	// Does not imply that the results are necessary a valid combination
	_query = query;
	_limits = limits;
	_object = object;
	_objectAux = objectAux;
	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::Unparse(const CCriterion& criterion, string& description) const
{
	description.clear();
	TTacticalPointQuery limits = criterion.GetLimits();
	TTacticalPointQuery query = criterion.GetQuery();
	TTacticalPointQuery object = criterion.GetObject();
	TTacticalPointQuery objectAux = criterion.GetObjectAux();

	bool bOk = true;
	const char* sWord;
	if (limits)
	{
		sWord = Translate(limits);
		if (!sWord) { sWord = "ERROR"; bOk = false; }
		description.append(sWord);
		description.append("_");
	}

	sWord = Translate(query);
	if (!query) { sWord = "ERROR"; bOk = false; }
	description.append(sWord);

	if (objectAux)
	{
		sWord = Translate(objectAux);
		if (!sWord) { sWord = "ERROR"; bOk = false; }
		description.append("_glue_");
		description.append(sWord);
	}

	if (object)
	{
		sWord = Translate(object);
		if (!query) { sWord = "ERROR"; bOk = false; }
		description.append("_glue_");
		description.append(sWord);
	}

	// Can't interpret the value here.
	// Have to leave that for caller

	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::CheckToken(TTacticalPointQuery token) const
{
	return (m_mTokenToString.find(token) != m_mTokenToString.end());
}

//----------------------------------------------------------------------------------------------//

TTacticalPointQuery CTacticalPointSystem::Translate(const char* sWord) const
{
	sBuffer.assign(sWord);
	std::map<string, TTacticalPointQuery>::const_iterator itM;
	itM = m_mStringToToken.find(sBuffer);
	if (itM == m_mStringToToken.end())
	{
		const char* sReportWord = (sWord ? sWord : "NULL");
		if (sReportWord[0] == '\0')
			AIWarningID("<TacticalPointSystem> ", " Empty query word - have you forgotten the last part of a query string?");
		else
			AIWarningID("<TacticalPointSystem> ", " Unrecognised query word \"%s\"", sReportWord);
		return eTPQ_None;
	}
	return itM->second;
}

//----------------------------------------------------------------------------------------------//

ETacticalPointQueryParameter CTacticalPointSystem::TranslateParam(const char* sWord) const
{
	sBuffer.assign(sWord);
	std::map<string, ETacticalPointQueryParameter>::const_iterator itM;
	itM = m_mParamStringToToken.find(sBuffer);
	if (itM == m_mParamStringToToken.end())
	{
		const char* sReportWord = (sWord ? sWord : "NULL");
		if (sReportWord[0] == '\0')
			AIWarningID("<TacticalPointSystem> ", " Empty query word - have you forgotten the last part of a query string?");
		else
			AIWarningID("<TacticalPointSystem> ", " Unrecognised query parameter word \"%s\"", sReportWord);
		return eTPQP_Invalid;
	}
	return itM->second;
}

//----------------------------------------------------------------------------------------------//

ETPSRelativeValueSource CTacticalPointSystem::TranslateRelativeValueSource(const char* sWord) const
{
	sBuffer.assign(sWord);
	std::map<string, ETPSRelativeValueSource>::const_iterator itM;
	itM = m_mRelativeValueSourceStringToToken.find(sBuffer);
	if (itM == m_mRelativeValueSourceStringToToken.end())
	{
		const char* sReportWord = (sWord ? sWord : "NULL");
		if (sReportWord[0] == '\0')
			AIWarningID("<TacticalPointSystem> ", " Empty query word - have you forgotten the last part of a query string?");
		else
			AIWarningID("<TacticalPointSystem> ", " Unrecognised relative value source in query \"%s\"", sReportWord);
		return eTPSRVS_Invalid;
	}
	return itM->second;
}

//----------------------------------------------------------------------------------------------//

const char* CTacticalPointSystem::Translate(TTacticalPointQuery etpToken) const
{
	std::map<TTacticalPointQuery, string>::const_iterator itM;
	itM = m_mTokenToString.find(etpToken);
	if (itM == m_mTokenToString.end())
	{
		AIWarningID("<TacticalPointSystem> ", " Unrecognised token %xh", etpToken);
		return NULL;
	}
	return itM->second;
}

//----------------------------------------------------------------------------------------------//

TPSQueryID CTacticalPointSystem::CreateQueryID(const char* psName)
{
	// Check if a query with that name already exists
	if (m_mNameToID.find(psName) != m_mNameToID.end())
	{
		AIWarningID("<TacticalPointSystem> ", "Query already exists with name \"%s\"", psName);
		return INVALID_TICKET;
	}

	// Advance the query ID ticket and add this as new entry
	m_LastQueryID.Advance();
	m_mIDToQuery.insert(std::make_pair(m_LastQueryID, CTacticalPointQuery(psName)));
	m_mNameToID[psName] = m_LastQueryID;

	return m_LastQueryID;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::DestroyQueryID(TPSQueryID nID)
{
	assert(m_mNameToID.size() == m_mIDToQuery.size());

	std::map<TPSQueryID, CTacticalPointQuery>::iterator it = m_mIDToQuery.find(nID);
	if (it == m_mIDToQuery.end()) return false;

	const char* psName = it->second.GetName();
	if (m_mNameToID.find(psName) == m_mNameToID.end())
	{
		AIWarningID("<TacticalPointSystem> ", "Query with name \"%s\" erased but couldn't be found in map", psName);
		m_mIDToQuery.erase(it);
		return false;
	}

	// Storing names in multiple places - this could be tidier

	m_mNameToID.erase(psName);
	m_mIDToQuery.erase(it);

	return true;
}

//----------------------------------------------------------------------------------------------//

// Get the Name of a query by ID
const char* CTacticalPointSystem::GetQueryName(TPSQueryID nID)
{
	std::map<TPSQueryID, CTacticalPointQuery>::const_iterator itM = m_mIDToQuery.find(nID);
	if (itM == m_mIDToQuery.end()) return NULL;

	return itM->second.GetName();
}

//----------------------------------------------------------------------------------------------//

// Get the ID number of a query by name
TPSQueryID CTacticalPointSystem::GetQueryID(const char* psName)
{
	std::map<string, TPSQueryID>::const_iterator itM;
	itM = m_mNameToID.find(psName);
	if (itM == m_mNameToID.end()) return INVALID_TICKET;

	return itM->second;
}

//----------------------------------------------------------------------------------------------//

const char* CTacticalPointSystem::GetOptionLabel(TPSQueryID queryID, int option)
{
	// Find the query, if it exists
	std::map<TPSQueryID, CTacticalPointQuery>::const_iterator it = m_mIDToQuery.find(queryID);

	const char* pOptionLabel = NULL;

	if (it != m_mIDToQuery.end())
	{
		const COptionCriteria* pOption = it->second.GetOption(option);
		if (pOption != NULL)
		{
			pOptionLabel = pOption->GetParams()->sSignalToSend.c_str();
		}
	}

	return pOptionLabel;
}

//----------------------------------------------------------------------------------------------//

void CTacticalPointSystem::DestroyAllQueries()
{
	m_mIDToQuery.clear();
	m_mNameToID.clear();

	for (std::map<TPSQueryTicket, const SQueryInstance>::iterator iter = m_mQueryInstanceQueue.begin();
	     iter != m_mQueryInstanceQueue.end();
	     ++iter)
	{
		const SQueryInstance& instance = iter->second;
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
	}
	m_mQueryInstanceQueue.clear();

	for (std::map<TPSQueryTicket, SQueryEvaluation>::iterator iter = m_mQueryEvaluationsInProgress.begin();
	     iter != m_mQueryEvaluationsInProgress.end();
	     ++iter)
	{
		SQueryInstance& instance = iter->second.queryInstance;
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
	}
	m_mQueryEvaluationsInProgress.clear();
}

void CTacticalPointSystem::Update(const float fBudgetSeconds)
{
	// Convert to absolute integer values time limit for precision and efficiency
	// Convert to floats only for debugging
	CTimeValue timeStart = gEnv->pTimer->GetAsyncTime();
	CTimeValue timeLimit = timeStart + CTimeValue(fBudgetSeconds);
	CTimeValue lastTime = timeStart;

	int nQueriesProcessed = 0;
	bool bDebugging = (CVars.DebugTacticalPoints != 0);

	do
	{
		// If we are not currently processing any query, bring out a new instance
		if (m_mQueryEvaluationsInProgress.empty())
		{
			// If there are no more queries to start, then there's no work to do
			if (m_mQueryInstanceQueue.empty())
				break;

			// Find the next request in the queue
			const SQueryInstance& instance = m_mQueryInstanceQueue.begin()->second;
			SQueryEvaluation& evaluation = m_mQueryEvaluationsInProgress.insert(std::make_pair(instance.nQueryInstanceID, SQueryEvaluation())).first->second;
			SetupQueryEvaluation(instance, evaluation);
			if (evaluation.eEvalState != SQueryEvaluation::eHeapEvaluation)   // Ideally, something like eReady first, with no work done
			{
				// An error
				assert(false);
				//let the listener know that there was a failure and it should so something besides wait for a result which is not coming.
				instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
				m_mQueryEvaluationsInProgress.erase(instance.nQueryInstanceID);
				continue;
			}

			// Pop the request from the queue
			m_mQueryInstanceQueue.erase(m_mQueryInstanceQueue.begin());
		}

		// We should now have a non-empty WIP queue
		SQueryEvaluation& evaluation = m_mQueryEvaluationsInProgress.begin()->second;
		ContinueQueryEvaluation(evaluation, timeLimit);

		CTimeValue currTime = gEnv->pTimer->GetAsyncTime();
		evaluation.queryInstance.nFramesProcessed++;
		lastTime = currTime;

		// Check if we completed that evaluation
		if (evaluation.eEvalState == SQueryEvaluation::eCompleted)
		{
			// Prepare results and make the callback
			CallbackQuery(evaluation);

#ifdef CRYAISYSTEM_DEBUG
			if (bDebugging)
			{
				// Move into debugging queue
				// Currently we do this for _everybody_ which might be useful or might be excessive
				// Choosing what to display is done separately
				evaluation.eEvalState = SQueryEvaluation::eDebugging;
				AddQueryForDebug(evaluation);

				// Performance stats
				const CTacticalPointQuery* pQuery = GetQuery(evaluation.queryInstance.nQueryID);
				CAIActor* pAIActor = static_cast<CAIActor*>(evaluation.queryInstance.queryContext.pAIActor);
				const char* sName = pAIActor ? pAIActor->GetName() : "NoActor";
				gEnv->pLog->Log("TPS Query: %s Actor: %s Frame completed: %d Frames processed: %d\n",
				                pQuery->GetName(), sName, gEnv->pRenderer->GetFrameID(), evaluation.queryInstance.nFramesProcessed);
			}
#endif

			m_mQueryEvaluationsInProgress.erase(m_mQueryEvaluationsInProgress.begin());
			nQueriesProcessed++;
			break;  // only one result per update so other subsystems can have a chance of updating too
			        // since the callback is intra-tps
		}
	}
	while (lastTime < timeLimit);

#ifdef CRYAISYSTEM_DEBUG
	// Update performance stats
	int nPreviousAsyncInFrame = (int) gAIEnv.pStatsManager->GetStat(eStat_AsyncTPSQueries);   // If this is our only call to Update, this will always be 0
	gAIEnv.pStatsManager->SetStat(eStat_AsyncTPSQueries, (float) (nPreviousAsyncInFrame + nQueriesProcessed));

	// Maintain any debugging data
	// Iterate through entry list
	std::list<SQueryEvaluation>::iterator entryIter = m_lstQueryEvaluationsForDebug.begin();
	while (entryIter != m_lstQueryEvaluationsForDebug.end())
	{
		SQueryEvaluation& sEntry = *entryIter;

		if (!sEntry.bPersistent && sEntry.timeErase < timeStart)
			entryIter = m_lstQueryEvaluationsForDebug.erase(entryIter);
		else
			++entryIter;
	}
#endif
}

void CTacticalPointSystem::Serialize(TSerialize ser)
{
	ser.Value("m_LanguageExtenderDummyObjects", m_LanguageExtenderDummyObjects);
}

void CTacticalPointSystem::CallbackQuery(SQueryEvaluation& evaluation)
{
	assert(evaluation.eEvalState == SQueryEvaluation::eCompleted);
	int nOptionUsed = evaluation.iCurrentQueryOption;
	SQueryInstance& instance = evaluation.queryInstance;

	if (nOptionUsed >= 0)
	{
		m_points.resize(0);

		// Locate the range of the best N points
		std::vector<SPointEvaluation>::iterator it = evaluation.GetIterRejectedEndAcceptedBegin();
		const std::vector<SPointEvaluation>::iterator end = evaluation.vPoints.end();
		int nPoints = int(std::distance(it, end));
		m_points.resize(nPoints);
		assert(nPoints > 0 && nPoints >= instance.nPoints);

		TTacticalPoints* locked = 0;
		if ((nPoints > 0) && (instance.flags & eTPQF_LockResults))
		{
			assert(m_locked.size() <= gAIEnv.pActorLookUp->GetActiveCount());

			std::pair<TLockedPoints::iterator, bool> insertedIt = m_locked.insert(TLockedPoints::value_type(instance.nQueryInstanceID, TTacticalPoints()));
			locked = &insertedIt.first->second;
			locked->reserve(nPoints);
		}

		for (int i = 0; i < nPoints; i++, ++it)
		{
			// confusing a...b
			STacticalPointResult& a = m_points[i];
			CTacticalPoint& b = it->m_Point;
			a.Invalidate();

			a.vPos = b.GetPos();
			a.flags |= eTPDF_Pos;

			if (const SHideSpotInfo* pInfo = b.GetHidespotInfo())
			{
				a.vObjDir = pInfo->dir;
				a.vObjPos = pInfo->pos;
				a.flags |= eTPDF_ObjectDir | eTPDF_ObjectPos | eTPDF_Hidespot;
				a.aiObjectId = b.GetHidespot()->pAnchorObject ? b.GetHidespot()->pAnchorObject->GetAIObjectID() : 0;
			}

			if (b.GetType() == ITacticalPoint::eTPT_CoverID)
			{
				a.coverID = b.GetCoverID();
				a.flags |= eTPDF_CoverID;
			}

			tAIObjectID id = b.GetAI().GetObjectID();
			if (id)
			{
				a.aiObjectId = id;
				a.flags |= eTPDF_AIObject;
			}

			if (b.GetType() == ITacticalPoint::eTPT_EntityPos)
			{
				a.entityId = b.GetEntityId();
				a.flags |= eTPDF_EntityId;
			}

			// And the hidespot itself?

			// Lock this point if needed
			if (locked)
				locked->push_back(b);
		}

		// We assume this is virtually free
		// Return normal results

		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, &m_points[0], nPoints, nOptionUsed);
	}
	else
	{
		// Return fail - no results but no errors
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
	}
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToParameters(TPSQueryID queryID, const char* sSpec, float fValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToParameters(sSpec, fValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToParameters(float) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToParameters(TPSQueryID queryID, const char* sSpec, bool bValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToParameters(sSpec, bValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToParameters(bool) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToParameters(TPSQueryID queryID, const char* sSpec, const char* sValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToParameters(sSpec, sValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToParameters(string) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToGeneration(TPSQueryID queryID, const char* sSpec, float fValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToGeneration(sSpec, fValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToGeneration(float) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToGeneration(TPSQueryID queryID, const char* sSpec, const char* sValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToGeneration(sSpec, TranslateRelativeValueSource(sValue));
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToGeneration(string) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToConditions(TPSQueryID queryID, const char* sSpec, float fValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToConditions(sSpec, fValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToConditions(float) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToConditions(TPSQueryID queryID, const char* sSpec, bool bValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToConditions(sSpec, bValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToConditions(bool) failed");
	return bOk;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointSystem::AddToWeights(TPSQueryID queryID, const char* sSpec, float fValue, int option)
{
	COptionCriteria* pOption = GetQueryOptionPointer(queryID, option);
	if (!pOption) return false;

	bool bOk = pOption->AddToWeights(sSpec, fValue);
	if (!bOk) AIWarningID("<TacticalPointSystem> ", "AddToWeights failed");
	return bOk;

}

//----------------------------------------------------------------------------------------------//
int CTacticalPointSystem::TestConditions(TPSQueryID queryID, const QueryContext& context, Vec3& point, bool& bValid) const
{
	// NOTE: often used to find if a point 'held in hand' fulfills the conditions expressed by a given TPS query.

	// Find the query, if it exists
	std::map<TPSQueryID, CTacticalPointQuery>::const_iterator it = m_mIDToQuery.find(queryID);
	if (it == m_mIDToQuery.end())
	{
		bValid = false;
		return -1;
	}

	const CTacticalPointQuery& query = it->second;
	TTacticalPoints vPoints;

	//MATT why here?
	if (CVars.DebugTacticalPoints != 0)
		gEnv->pLog->Log("TPS Query: %s", query.GetName());

	// (MATT) TODO Hard to implement elegantly just now {2009/11/22}
	assert(false);

	/*

	   // Work through just the available options in order, until we find some valid points or we run out of options
	   int i=0;
	   const COptionCriteria *pOption = NULL;
	   while (	vPoints.size() < 1 && (pOption = query.GetOption(i++)) )
	   {
	    const std::vector<CCriterion> &vConditions = pOption->GetAllConditions();
	    const std::vector<CCriterion> &vWeights = pOption->GetAllWeights();

	    // Ensure we have no points left overy from a previous option
	    // That should only happen if there is an error in the query - usually if an option finds any points, we return them
	    vPoints.clear();

	    // Add the point we want to test. This effectively stands in for the generation phase of the query
	    vPoints.push_back( CTacticalPoint( point ));

	    if (!EvaluatePoints( vConditions, vWeights, context, vPoints, 1))
	      return -1;
	   }

	   bValid = (vPoints.size() > 0);

	   return i;
	 */
	return -1;
}

//----------------------------------------------------------------------------------------------//

// Start a new asynchronous query. Returns the id "ticket" for this query instance.
// Types needed to avoid confusion?
TPSQueryTicket CTacticalPointSystem::AsyncQuery(TPSQueryID queryID, const QueryContext& m_context, int flags, int nPoints, ITacticalPointResultsReceiver* pReciever)
{
	// Advance ticket - even queries with errors get a ticket, which should aid debugging
	m_nQueryInstanceTicket.Advance();

	// Populate an instance
	SQueryInstance instance;
	instance.nQueryInstanceID = m_nQueryInstanceTicket;
	instance.nPoints = nPoints;
	instance.nQueryID = queryID;
	instance.pReceiver = pReciever;
	instance.queryContext = m_context;
	instance.timeRequested = gEnv->pTimer->GetAsyncTime(); // (MATT) Would use frame time, but that appears junk! {2009/11/22}
	instance.flags = flags;

	// Check the instance for validity
	if (!VerifyQueryInstance(instance))
		return INVALID_TICKET;

	bool bInserted = m_mQueryInstanceQueue.insert(std::make_pair(m_nQueryInstanceTicket, instance)).second;
	assert(bInserted);

	return m_nQueryInstanceTicket;
}

//----------------------------------------------------------------------------------------------//
bool CTacticalPointSystem::VerifyQueryInstance(const SQueryInstance& instance) const
{
	// Check the queryID
	if (m_mIDToQuery.find(instance.nQueryID) == m_mIDToQuery.end())
		return false;

	// Check a sensible number of return values
	if (instance.nPoints < 1)
		return false;

	// Check the query... which should already be validated actually

	// Check the context
	// (Assume its ok for now)

	return true;
}

//----------------------------------------------------------------------------------------------//

// Cancel an asynchronous query.
bool CTacticalPointSystem::CancelAsyncQuery(TPSQueryTicket ticket)
{
	// Find in queue...
	std::map<TPSQueryTicket, const SQueryInstance>::iterator itA = m_mQueryInstanceQueue.find(ticket);
	if (itA != m_mQueryInstanceQueue.end())
	{
		const SQueryInstance& instance = itA->second;
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
		m_mQueryInstanceQueue.erase(itA);
		return true;
	}

	// Is it currently begin evaluated already?
	std::map<TPSQueryTicket, SQueryEvaluation>::iterator itB = m_mQueryEvaluationsInProgress.find(ticket);
	if (itB != m_mQueryEvaluationsInProgress.end())
	{
		SQueryInstance& instance = itB->second.queryInstance;
		instance.pReceiver->AcceptResults(false, instance.nQueryInstanceID, NULL, 0, -1);
		m_mQueryEvaluationsInProgress.erase(itB);
		return true;
	}

	return false;
}

void CTacticalPointSystem::UnlockResults(TPSQueryTicket queryTicket)
{
	TLockedPoints::iterator it = m_locked.find(queryTicket);
	if (it != m_locked.end())
		m_locked.erase(it);
}

bool CTacticalPointSystem::HasLockedResults(TPSQueryTicket queryTicket) const
{
	TLockedPoints::const_iterator it = m_locked.find(queryTicket);
	return it != m_locked.end();
}

//----------------------------------------------------------------------------------------------//

COptionCriteria* CTacticalPointSystem::GetQueryOptionPointer(TPSQueryID nID, int option)
{
	if (option < 0 || option > 32) return NULL; // Reject silly numbers of fallback options

	// Find the query, if it exists
	std::map<TPSQueryID, CTacticalPointQuery>::iterator it = m_mIDToQuery.find(nID);
	if (it == m_mIDToQuery.end()) return NULL;

	CTacticalPointQuery& query = it->second;

	// Add options if need be (we haven't specified what order these things must be filled out, after all)
	while (!query.GetOption(option))
		query.AddOption(COptionCriteria());

	return query.GetOption(option);
}

//----------------------------------------------------------------------------------------------//

void CTacticalPointSystem::TPSDescriptionWarning(const CTacticalPointQuery& query, const QueryContext& context, const COptionCriteria* pOption, const char* sMessage) const
{
	AIWarningID("<TacticalPointSystem> ",
	            "%s%s"
	            "Query \'%s\' description: \n%s",
	            (sMessage ? sMessage : ""), (sMessage ? "\n" : ""),      // Message if any, add newline if message
	            query.GetName(),                                         // Name
	            (pOption ? pOption->GetDescription().c_str() : "None")); // Optional query description
}

//-----------------------------------------------------------------------------------------------------------
void CTacticalPointSystem::DebugDraw() const
{
#ifdef CRYAISYSTEM_DEBUG
	// Note that time-related updating of the debug lists is done in Update

	CAISystem* pAISystem = GetAISystem();

	if (CVars.DebugTacticalPoints == 0)
		return;

	// Make sure the target AI is the current AI Stats Target, unless this is a 'special case'
	EntityId id = 0;
	if (gAIEnv.CVars.StatsTarget)
	{
		const char* sTarget = gAIEnv.CVars.StatsTarget;
		if (CAIObject* pTargetObject = gAIEnv.pAIObjectManager->GetAIObjectByName(sTarget))
			id = pTargetObject->GetEntityID();
		else if (!strcmp(gAIEnv.CVars.StatsTarget, "all"))
			id = -1;
	}

	CDebugDrawContext dc;
	dc->Init3DMode();
	dc->SetAlphaBlended(true);
	dc->SetDrawInFront(true);

	CTimeValue timeNow = gEnv->pTimer->GetAsyncTime(); // (MATT) Would use frame time, but that appears junk! {2009/11/22}

	// Iterate through entry list
	std::list<SQueryEvaluation>::const_iterator entryIter = m_lstQueryEvaluationsForDebug.begin();
	while (entryIter != m_lstQueryEvaluationsForDebug.end())
	{
		const SQueryEvaluation& sEntry = *entryIter++;

		if (id != -1 && sEntry.queryInstance.queryContext.actorEntityId != id && sEntry.queryInstance.queryContext.actorEntityId != GetAISystem()->GetAgentDebugTarget())
			continue;

		int iDrawMode = CVars.TacticalPointsDebugDrawMode;
		int iFadeMode = CVars.TacticalPointsDebugFadeMode;
		float fSizeScaling = CVars.TacticalPointsDebugScaling;
		float fAlphaModifier = 1.f;

		// If not drawing anything, skip on
		if (!iDrawMode)
			continue;

		float fPercentageLifetime = (timeNow - sEntry.timePlaced).GetSeconds() / (sEntry.timeErase - sEntry.timePlaced).GetSeconds();
		fPercentageLifetime = clamp_tpl(fPercentageLifetime, 0.0f, 1.0f);

		//	Disapparation effect when time nearly up
		float fPercentageDisapparation = 0.1f;
		if (!sEntry.bPersistent && (1.0f - fPercentageLifetime) < fPercentageDisapparation)
		{
			switch (iFadeMode)
			{
			case 1: // Vanish, so do nothing
				break;

			case 2: // Alpha fade
				fAlphaModifier = (1.0f - fPercentageLifetime) / fPercentageDisapparation;
				break;

			case 3: // Blink
				if ((timeNow - sEntry.timePlaced).GetPeriodicFraction(CTimeValue(0.2f)) > 0.5f)
					continue; // Don't display points in 'off' part of blink cycle
				break;
			}
		}

		// Iterate through point vector
		int iFirstN = 0;
		std::vector<SPointEvaluation>::const_iterator pointIter;
		for (pointIter = sEntry.vPoints.begin(); pointIter != sEntry.vPoints.end(); ++pointIter)
		{
			const SPointEvaluation& sPoint = (*pointIter);
			Vec3 vPos = sPoint.m_Point.GetPos();

			ColorF color(0, 0, 0, 0);
			switch (sPoint.m_eEvalState)
			{
			case SPointEvaluation::eRejected:
				color = ColorF(1.0f, 0.0f, 0.0f);
				break;                                                                    // Red        - failed a condition
			case SPointEvaluation::ePartial:
				color = ColorF(0.3f, 0.3f, 0.6f);
				break;                                                                    // Blue/mauve - of those tested, no conditions failed
			case SPointEvaluation::eValid:
				color = ColorF(0.3f, 0.6f, 0.3f);
				break;                                                                    // Green      - all conditions passed, but did not score in best n
			case SPointEvaluation::eAccepted:
				color = ColorF(1.0f, 1.0f, 1.0f);
				break;                                                                    // White      - one of the n best results
			case SPointEvaluation::eBest:
				color = ColorF(1.0f, 1.0f, 0.0f);
				break;                                                                    // Yellow     - the best result
			default:
				assert(false);
			}

			switch (iDrawMode)
			{
			case 1:
				{
					// Use a fading transparency
					// Used to modulate that with fitness. Less obvious how to do that now.
					float fRadius = 0.5f * sPoint.m_fSizeModifier * fSizeScaling;
					color.a = fAlphaModifier;
					dc->DrawSphere(vPos, fRadius, color);
				}
				break;
			case 2:
				{
					// Modulate inner radius of sphere with fitness
					// Coudl actually work quite well for min/max?
					float fRadiusOuter = 0.5f * sPoint.m_fSizeModifier * fSizeScaling;
					float fRadiusInner = fRadiusOuter * sPoint.m_fMax;

					ColorF colorOuter = color;
					ColorF colorInner = color;
					colorOuter.a = fAlphaModifier * 0.4f;
					colorInner.b = fAlphaModifier;

					dc->DrawSphere(vPos, fRadiusOuter, colorOuter);
					dc->DrawSphere(vPos, fRadiusInner, colorInner);
				}
				break;
			}

			// Always draw the debug text
			if (fabs(sPoint.m_fMax - sPoint.m_fMin) < 0.001f)    // Within f.p. error, we've found an exact value for this point
				dc->Draw3dLabel(vPos, 1.5f, "%.2f", sPoint.m_fMax);
			else
				dc->Draw3dLabel(vPos, 1.5f, "[%.2f - %0.2f]", sPoint.m_fMin, sPoint.m_fMax);

		} // Next point
	}   // Next entry
#endif
}

void CTacticalPointSystem::GatherAvoidCircles(const Vec3& center, float range, CAIObject* requester,
                                              AvoidCircles& avoidCircles) const
{
	ActorLookUp& lookUp = *gAIEnv.pActorLookUp;

	size_t activeActorCount = lookUp.GetActiveCount();

	for (size_t actorIndex = 0; actorIndex < activeActorCount; ++actorIndex)
	{
		CAIObject* aiObject = lookUp.GetActor<CAIObject>(actorIndex);

		if (aiObject != requester)
		{
			if (CPipeUser* pipeUser = aiObject->CastToCPipeUser())
			{
				float radius = pipeUser->GetParameters().m_fPassRadius;
				float distanceToCover = pipeUser->GetParameters().distanceToCover;

				CoverID coverID = pipeUser->GetCoverID();
				if (coverID)
				{
					AvoidCircle circle;
					circle.pos = gAIEnv.pCoverSystem->GetCoverLocation(coverID, distanceToCover);
					circle.radius = radius;

					avoidCircles.push_back(circle);
				}

				CoverID regCoverID = pipeUser->GetCoverRegister();

				if (regCoverID && (coverID != regCoverID))
				{
					AvoidCircle circle;
					circle.pos = gAIEnv.pCoverSystem->GetCoverLocation(regCoverID, distanceToCover);
					circle.radius = radius;

					avoidCircles.push_back(circle);
				}
			}
		}
	}
}

void CTacticalPointSystem::VisibleRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	SQueryEvaluation& eval = m_mQueryEvaluationsInProgress.begin()->second;
	assert(eval.visibleRayID);
	eval.visibleResult = result ? 0 : 1;
	eval.visibleRayID = 0;
}

void CTacticalPointSystem::CanShootRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	SQueryEvaluation& eval = m_mQueryEvaluationsInProgress.begin()->second;
	assert(eval.canShootRayID);
	eval.canShootResult = result ? 0 : 1;
	eval.canShootRayID = 0;
}

void CTacticalPointSystem::CanShootSecondRayComplete(const QueuedRayID& rayID, const RayCastResult& result)
{
	SQueryEvaluation& eval = m_mQueryEvaluationsInProgress.begin()->second;
	assert(eval.canShootSecondRayID);
	eval.canShootSecondRayResult = result ? 0 : 1;
	eval.canShootSecondRayID = 0;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointGenerateResult::AddHideSpot(const SHideSpot& hidespot)
{
	m_Points.push_back(CTacticalPoint(hidespot));
	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointGenerateResult::AddPoint(const Vec3& point)
{
	m_Points.push_back(CTacticalPoint(point));
	return true;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointGenerateResult::AddEntity(IEntity* pEntity)
{
	bool bResult = false;
	assert(pEntity);
	if (pEntity)
	{
		m_Points.push_back(CTacticalPoint(pEntity->GetId(), pEntity->GetWorldPos()));
		bResult = true;
	}
	return bResult;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointGenerateResult::AddEntityPoint(IEntity* pEntity, const Vec3& point)
{
	bool bResult = false;
	assert(pEntity);
	if (pEntity)
	{
		m_Points.push_back(CTacticalPoint(pEntity->GetId(), point));
		bResult = true;
	}
	return bResult;
}

//----------------------------------------------------------------------------------------------//

bool CTacticalPointGenerateResult::HasPoints() const
{
	return (!m_Points.empty());
}

//----------------------------------------------------------------------------------------------//

void CTacticalPointGenerateResult::GetPoints(TTacticalPoints& points) const
{
	points = m_Points;
}
