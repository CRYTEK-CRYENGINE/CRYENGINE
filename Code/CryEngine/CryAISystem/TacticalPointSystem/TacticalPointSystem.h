// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   TacticalPointSystem.h
   $Id$
   $DateTime$
   Description: System for Tactical Point queries
   (Major extension of the old hidepoint search concept)
   ---------------------------------------------------------------------
   History:
   - 02:07:2007 : Created by Matthew Jack

 *********************************************************************/

#ifndef __TacticalPointSystem_H__
#define __TacticalPointSystem_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/ITacticalPointSystem.h>

#include "TacticalPointQuery.h"

#include <CryMath/Cry_Math.h>
#include "TacticalPointQueryEnum.h"
#include "PipeUser.h"
#include "HideSpot.h"

#include "../Cover/CoverSystem.h"
#include "PostureManager.h"

// Forward declarations

class CPipeUser;
class CTacticalPointQuery;
class COptionCriteria;

enum ETacticalPointQuery;
enum ETacticalPointQueryParameter;

//----------------------------------------------------------------------------------------------//

inline void InitQueryContextFromActor(CAIActor* pAIActor, QueryContext& context)
{
	if (!pAIActor)
	{
		AIAssert(0);
		return;
	}

	context.pAIActor = pAIActor;
	context.actorPos = pAIActor->GetPos();
	context.actorDir = pAIActor->GetViewDir();
	context.actorNavCaps = pAIActor->m_movementAbility.pathfindingProperties.navCapMask;
	context.actorEntityId = pAIActor->GetEntityID();

	context.actorRadius = pAIActor->GetParameters().m_fPassRadius;
	context.distanceToCover = pAIActor->GetParameters().distanceToCover;
	context.inCoverRadius = pAIActor->GetParameters().inCoverRadius;
	context.effectiveCoverHeight = pAIActor->GetParameters().effectiveCoverHeight;

	context.realTarget.zero();
	context.realTargetDir.zero();

	IAIObject* attentionTarget = pAIActor->GetAttentionTarget();
	if (attentionTarget)
	{
		context.attentionTarget = attentionTarget->GetPos();
		context.attentionTargetDir = attentionTarget->GetViewDir();

		if (const IAIObject* liveTarget = CAIActor::GetLiveTarget(static_cast<const CAIObject*>(attentionTarget)))
		{
			context.realTarget = liveTarget->GetPos();
			context.realTargetDir = liveTarget->GetViewDir();
		}
	}
	else
	{
		context.attentionTarget.zero();
		context.attentionTargetDir.zero();
	}

	CPipeUser* pPipeUser = pAIActor->CastToCPipeUser();
	IAIObject* refPoint = pPipeUser ? pPipeUser->GetRefPoint() : 0;

	if (refPoint)
	{
		context.referencePoint = refPoint->GetPos();
		context.referencePointDir = refPoint->GetViewDir();
	}
	else
	{
		context.referencePoint.zero();
		context.referencePointDir.zero();
	}
}

//----------------------------------------------------------------------------------------------//

class CTacticalPoint : public ITacticalPoint
{
public:
	// cppcheck-suppress uninitMemberVar
	CTacticalPoint() : eTPType(eTPT_None) {}

	// Need this for the STL containers that we shouldn't be using
	CTacticalPoint(const CTacticalPoint& that)
	{
		*this = that;
	}

	const CTacticalPoint& operator=(const CTacticalPoint& that)
	{
		eTPType = that.eTPType;
		memcpy(&vPos, &that.vPos, sizeof(vPos));

		switch (eTPType)
		{
		case eTPT_None:
		case eTPT_Point:
			break;

		case eTPT_EntityPos:
			m_entityId = that.m_entityId;
			break;

		case eTPT_HideSpot:
			m_Hidespot = that.m_Hidespot;
			break;

		case eTPT_AIObject:
			m_refObject = that.m_refObject;
			break;

		case eTPT_CoverID:
			coverID = that.coverID;
			break;

		default:
			assert(false);
		}
		return *this;
	}

	bool operator==(const CTacticalPoint& that) const
	{
		if (eTPType != that.eTPType)
			return false;

		switch (eTPType)
		{
		case eTPT_None:
			return false;
		case eTPT_HideSpot:
			{
				if (m_Hidespot.pAnchorObject && (m_Hidespot.pAnchorObject != that.m_Hidespot.pAnchorObject))
					return false;
				else if (m_Hidespot.info.type != that.m_Hidespot.info.type)
					return false;
				else
					return ((m_Hidespot.info.pos - that.m_Hidespot.info.pos).len2() < 0.5f * 0.5f);
			}
		case eTPT_Point:
			return ((vPos - that.vPos).len2() < 0.5f * 0.5f);
		case eTPT_EntityPos:
			return m_entityId == that.m_entityId;
		case eTPT_AIObject:
			return m_refObject == that.m_refObject;
		case eTPT_CoverID:
			return coverID == that.coverID;
		default:
			break;
		}
		return false;
	}

	bool operator!=(const CTacticalPoint& that) const
	{
		return !operator==(that);
	}

	// cppcheck-suppress uninitMemberVar
	explicit CTacticalPoint(const SHideSpot& hidespot)
		: eTPType(eTPT_HideSpot)
		, m_Hidespot(hidespot)
		, m_entityId(0)
		, vPos(hidespot.info.pos)
	{
	}

	// cppcheck-suppress uninitMemberVar
	explicit CTacticalPoint(const Vec3& vPoint) : eTPType(eTPT_Point)
	{
		vPos = vPoint;
	}

	// cppcheck-suppress uninitMemberVar
	explicit CTacticalPoint(CWeakRef<CAIObject> refObj) : eTPType(eTPT_AIObject)
	{
		// Ideally, we might not store position in this case, as the reference may become invalid
		CAIObject* pObj = refObj.GetAIObject();
		vPos = (pObj ? pObj->GetPos() : ZERO);
		m_refObject = refObj;
	}

	CTacticalPoint(EntityId entityId, const Vec3& vPoint) : eTPType(eTPT_EntityPos)
	{
		vPos = vPoint;
		m_entityId = entityId;
	}

	// cppcheck-suppress uninitMemberVar
	CTacticalPoint(const CoverID& coverID, const Vec3& pos) : eTPType(eTPT_CoverID)
	{
		this->vPos = pos;
		this->coverID = coverID;
	}

	~CTacticalPoint()
	{
		switch (eTPType)
		{
		case eTPT_None:
		case eTPT_Point:
		case eTPT_EntityPos:
		case eTPT_HideSpot:
		case eTPT_AIObject:
		case eTPT_CoverID:
			break;

		default:
			assert(false);
		}
		;
	}

	virtual Vec3                               GetPos() const          { return vPos; }
	virtual void                               SetPos(Vec3 pos)        { vPos = pos; }
	virtual ITacticalPoint::ETacticalPointType GetType() const         { return eTPType; }
	virtual const SHideSpot*                   GetHidespot() const     { return (eTPType == eTPT_HideSpot ? &m_Hidespot : NULL); }
	virtual tAIObjectID                        GetAIObjectId() const   { return (eTPType == eTPT_AIObject ? m_refObject.GetObjectID() : INVALID_AIOBJECTID); }
	virtual bool                               IsValid() const         { return eTPType != eTPT_None; }

	const SHideSpotInfo*                       GetHidespotInfo() const { return (eTPType == eTPT_HideSpot ? &(m_Hidespot.info) : NULL); }

	inline CoverID                             GetCoverID() const
	{
		return coverID;
	}

	// These methods only available within the AI system
	CWeakRef<CAIObject>       GetAI()             { return (eTPType == eTPT_AIObject ? m_refObject : NILREF); }
	CWeakRef<const CAIObject> GetAI() const       { return (eTPType == eTPT_AIObject ? m_refObject : NILREF); }

	EntityId                  GetEntityId() const { return (eTPType == eTPT_EntityPos ? m_entityId : INVALID_ENTITYID); }

private:
	// Type specifying the relevant union member if any
	ETacticalPointType  eTPType;

	SHideSpot           m_Hidespot;
	EntityId            m_entityId;
	CWeakRef<CAIObject> m_refObject;

	CoverID             coverID;
	Vec3                vPos;
};
typedef std::vector<CTacticalPoint> TTacticalPoints;

//----------------------------------------------------------------------------------------------//

class CTacticalPointGenerateResult : public ITacticalPointGenerateResult
{
public:
	virtual bool AddHideSpot(const SHideSpot& hidespot);
	virtual bool AddPoint(const Vec3& point);
	virtual bool AddEntity(IEntity* pEntity);
	virtual bool AddEntityPoint(IEntity* pEntity, const Vec3& point);

	bool         HasPoints() const;
	void         GetPoints(TTacticalPoints& points) const;

private:
	TTacticalPoints m_Points;
};

// There need only ever be one of these in game, hence a singleton
// Creates on first request, which is lightweight

class CTacticalPointSystem : public ITacticalPointSystem
{
	friend class COptionCriteria;
	friend class CCriterion;

	friend struct SAIEnvironment;
	friend class CAISystem; //TODO see why these are needed and why this has a private constructor

public:
	static void RegisterCVars();

public:

	void Reset();

	// ---------- ITacticalPointSystem methods ----------
	// Separate this out completely into an adaptor?

	// Timesliced update within the main AI thread
	// Ideally performs just housekeeping and manages the asynchronous subtasks
	virtual void Update(const float fBudgetSeconds) override;

	// Get a new query ID, to allow us to build a new query
	virtual TPSQueryID  CreateQueryID(const char* psName) override;
	// Destroy a query ID and release all resources associated with it
	virtual bool        DestroyQueryID(TPSQueryID queryID) override;
	// Get the Name of a query by ID
	virtual const char* GetQueryName(TPSQueryID queryID) override;
	// Get the query ID of a query by name
	virtual TPSQueryID  GetQueryID(const char* psName) override;
	// Returns a pointer to indexed option of a given query
	virtual const char* GetOptionLabel(TPSQueryID queryID, int option) override;

	// Build up a query
	// The "option" parameter allows you to build up fallback options
	virtual bool AddToParameters(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) override;
	virtual bool AddToParameters(TPSQueryID queryID, const char* sSpec, bool bValue, int option = 0) override;
	virtual bool AddToParameters(TPSQueryID queryID, const char* sSpec, const char* sValue, int option = 0) override;
	virtual bool AddToGeneration(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) override;
	virtual bool AddToGeneration(TPSQueryID queryID, const char* sSpec, const char* sValue, int option = 0) override;
	virtual bool AddToConditions(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) override;
	virtual bool AddToConditions(TPSQueryID queryID, const char* sSpec, bool bValue, int option = 0) override;
	virtual bool AddToWeights(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) override;

	// Start a new asynchronous query. Returns the id "ticket" for this query instance.
	// Types needed to avoid confusion?
	virtual TPSQueryTicket AsyncQuery(TPSQueryID queryID, const QueryContext& m_context, int flags, int nPoints, ITacticalPointResultsReceiver* pReciever) override;

	virtual void           UnlockResults(TPSQueryTicket queryTicket) override;
	virtual bool           HasLockedResults(TPSQueryTicket queryTicket) const override;

	// Cancel an asynchronous query.
	virtual bool CancelAsyncQuery(TPSQueryTicket ticket) override;

	// ---------- ~ End of ITacticalPointSystem methods ~ ----------

	// Test a given point if it fulfills conditions of a given query.
	int TestConditions(TPSQueryID queryID, const QueryContext& context, Vec3& point, bool& bValid) const;

	// Make a synchronous query for one point
	// Returns: query option used, or -1 if there was an error or no points found
	// Has no side-effects
	int SyncQuery(TPSQueryID queryID, const QueryContext& context, CTacticalPoint& point);

	// Synchronous query for up to n best points in order of decreasing fitness
	// Returns: query option used, or -1 if there was an error or no points found
	// No side-effects
	int SyncQueryShortlist(TPSQueryID queryID, const QueryContext& context, TTacticalPoints& points, int n);

	// Destroy all stored queries, usually on AI reload
	void DestroyAllQueries();

	void Serialize(TSerialize ser);

	void DebugDraw() const;

	// Extend the language by adding new keywords
	virtual bool ExtendQueryLanguage(const char* szName, ETacticalPointQueryType eType, ETacticalPointQueryCost eCost) override;

	// Language extenders
	virtual bool       AddLanguageExtender(ITacticalPointLanguageExtender* pExtender) override;
	virtual bool       RemoveLanguageExtender(ITacticalPointLanguageExtender* pExtender) override;
	virtual IAIObject* CreateExtenderDummyObject(const char* szDummyName) override;
	virtual void       ReleaseExtenderDummyObject(tAIObjectID id) override;

private:
	struct SQueryInstance
	{
		TPSQueryTicket                 nQueryInstanceID;
		TPSQueryID                     nQueryID;
		QueryContext                   queryContext;
		int                            flags;
		int                            nPoints;
		ITacticalPointResultsReceiver* pReceiver;

		// Performance monitoring, for latency and throughput
		CTimeValue timeRequested;
		int        nFramesProcessed;
		SQueryInstance()
		{
			flags = 0;
			nPoints = 0;
			pReceiver = NULL;
			nFramesProcessed = 0;
		}
	};

	// Struct used to represent points in the process of being evaluated
	// Essentially storing the low level progress of a particular query option
	// The extra state may be discarded after evaluation or preserved for debugging
	// This structure could be shrunk - but shrink the CTacticalPoint structure first
	struct SPointEvaluation
	{
		// State of a TPS point in the evaluation pipeline
		// Note that a point is considered partial initially
		enum EPointEvaluationState
		{
			eEmpty,                                              // An invalid instance. Just present for convenience
			eRejected,                                           // Point failed one of the conditions
			ePartial,                                            // Point conditions are not fully evaluated
			eValid,                                              // Point is valid but not one of the n best
			eAccepted,                                           // Point was accepted as one of the n best
			eBest,                                               // Point was the best found
		} m_eEvalState;

		// [0, 1] Track the min and max scores this point can potentially attain. These converge to precise result.
		float          m_fMin, m_fMax;

		int            m_iQueryIndex;                          // The index of the next criterion to be evaluated on this point
		float          m_fSizeModifier;                        // Size of debug sphere. E.g. modify if known to be inside obstacle, for visibility
		CTacticalPoint m_Point;                                // The point being considered

		SPointEvaluation(const CTacticalPoint& point, float fMin, float fMax, EPointEvaluationState eEvalState)
			: m_eEvalState(eEvalState)
			, m_fMin(fMin)
			, m_fMax(fMax)
			, m_iQueryIndex(0)
			, m_fSizeModifier(1.0f)
			, m_Point(point)
		{}

		SPointEvaluation()
			: m_eEvalState(eEmpty)
			, m_fMin(-100.0f)
			, m_fMax(-100.0f)
			, m_iQueryIndex(-1)
			, m_fSizeModifier(1.0f)
		{}

		// Used for heapsort. We always sort so that the highest _potential_ score comes to the top of the heap
		// Note that in the STL heap structure it is the largest element that resides in position 0
		bool operator<(const SPointEvaluation& that) const { return (m_fMax < that.m_fMax); }
	};

	// The progress of a particular query instance at a high level
	struct SQueryEvaluation
	{
		// Describing our state machine. Note that we loop through a series of states for each option.
		// This could even have a couple more states, if generation/setup prove costly
		enum QueryEvaluationState
		{
			eEmpty,                                              // An invalid instance. Just present for convenience
			eReady,                                              // Moved out of waiting queue, ready to begin processing
			eInitialized,                                        // Generation complete, criterion have been sorted, points transformed into SPointEvaluations
			eHeapEvaluation,                                     // We've heapified and are processing the heap
			eWaitingForDeferred,                                 // We're waiting for deferred conditions to complete
			eCompletedOption,                                    // An option completed but returned no results - must initialise the next
			eCompleted,                                          // We are ready to return results from an option, or that all options failed
			eDebugging,                                          // Have returned a result, currently preserved for debugging
			eError,                                              // Something went wrong - aborting with no results
		}   eEvalState;
		int iCurrentQueryOption;                               // Whether we are currently evaluating the first option, the first fallback, the next, etc.

		typedef std::vector<CCriterion> Criteria;
		Criteria                                            vCheapConds;
		Criteria                                            vExpConds;
		Criteria                                            vDefConds;
		Criteria                                            vCheapWeights;
		Criteria                                            vExpWeights;

		PostureManager::PostureQueryID                      postureQueryID;

		QueuedRayID                                         visibleRayID;
		int                                                 visibleResult;

		QueuedRayID                                         canShootRayID;
		int                                                 canShootResult;

		QueuedRayID                                         canShootSecondRayID;
		int                                                 canShootSecondRayResult;

		ITacticalPointLanguageExtender::TDeferredCancelFunc deferredExtenderCancelFunc;

		std::vector<SPointEvaluation>                       vPoints;

		SQueryInstance                                      queryInstance; // Defines details of the query request
		int                                                 nFoundBestN;   // Number of best points found so far

		EntityId                                            owner;

		// Used for debugging
		bool       bPersistent;
		CTimeValue timePlaced, timeErase;

		SQueryEvaluation();

		~SQueryEvaluation();

		// Avoid using this on non-empty instances due to std::vector copy
		SQueryEvaluation(const SQueryEvaluation& that)
		{
			*this = that;
		}

		// Avoid using this on non-empty instances due to std::vector copy
		void operator=(const SQueryEvaluation& that)
		{
			eEvalState = that.eEvalState;
			iCurrentQueryOption = that.iCurrentQueryOption;
			vCheapConds = that.vCheapConds;
			vExpConds = that.vExpConds;
			vCheapWeights = that.vCheapWeights;
			vExpWeights = that.vExpWeights;
			vPoints = that.vPoints;
			indexHeapEndRejectedBegin = that.indexHeapEndRejectedBegin;
			indexRejectedEndAcceptedBegin = that.indexRejectedEndAcceptedBegin;
			queryInstance = that.queryInstance;
			nFoundBestN = that.nFoundBestN;
			owner = that.owner;
			bPersistent = that.bPersistent;
			timePlaced = that.timePlaced;
			timeErase = that.timeErase;
			postureQueryID = that.postureQueryID;
			visibleRayID = that.visibleRayID;
			visibleResult = that.visibleResult;
			canShootRayID = that.canShootRayID;
			canShootResult = that.canShootResult;
			canShootSecondRayID = that.canShootSecondRayID;
			canShootSecondRayResult = that.canShootSecondRayResult;
			deferredExtenderCancelFunc = that.deferredExtenderCancelFunc;
			if (that.deferredExtenderCancelFunc)
			{
				CryFatalError("Can't make a copy of a deferredExtenderCancelFunc - it will lead to a double call - this message should never happen.");
			}
		}

		void Reset()
		{
			eEvalState = eEmpty;
			iCurrentQueryOption = 0;
			vCheapConds.clear();
			vCheapWeights.clear();
			vExpConds.clear();
			vExpWeights.clear();
			vPoints.clear();
			indexHeapEndRejectedBegin = 0;
			indexRejectedEndAcceptedBegin = 0;
			queryInstance = SQueryInstance();
			nFoundBestN = 0;
			owner = 0;
			bPersistent = false;
			timePlaced.SetValue(0);
			timeErase.SetValue(0);
			postureQueryID = 0;

			visibleRayID = 0;
			visibleResult = -1;
			canShootRayID = 0;
			canShootResult = -1;
			canShootSecondRayID = 0;
			canShootSecondRayResult = -1;
			deferredExtenderCancelFunc = ITacticalPointLanguageExtender::TDeferredCancelFunc();
		}

		const std::vector<SPointEvaluation>::iterator GetIterHeapEndRejectedBegin(void)
		{
			return vPoints.begin() + indexHeapEndRejectedBegin;
		}

		const std::vector<SPointEvaluation>::iterator GetIterRejectedEndAcceptedBegin(void)
		{
			return vPoints.begin() + indexRejectedEndAcceptedBegin;
		}

		void SetIterRejectedEndAcceptedBegin(const std::vector<SPointEvaluation>::iterator& it)
		{
			indexRejectedEndAcceptedBegin = std::distance(vPoints.begin(), it);
			assert(indexRejectedEndAcceptedBegin <= vPoints.size());
		}

		void SetIterHeapEndRejectedBegin(const std::vector<SPointEvaluation>::iterator& it)
		{
			indexHeapEndRejectedBegin = std::distance(vPoints.begin(), it);
			assert(indexHeapEndRejectedBegin <= vPoints.size());
		}

	private:
		// Indices delineating ranges of the heap
		// Stored in this form to be safe for copying, but converted ot iterators for use.
		size_t indexHeapEndRejectedBegin;     // Defining the end of the heap range within the vector
		size_t indexRejectedEndAcceptedBegin; // Defining the beginning of the rejected points range within the vector
	};

	CTacticalPointSystem();
	~CTacticalPointSystem();

	bool ApplyCost(uint32 uQueryId, ETacticalPointQueryCost eCost);
	int  GetCost(uint32 uQueryId) const;

	bool VerifyQueryInstance(const SQueryInstance& instance) const;

	// Generates a vector of points based on a vector of generation criteria and any option criteria, based on the given actor
	bool GeneratePoints(const std::vector<CCriterion>& vGeneration, const QueryContext& context, const COptionCriteria* pOption, std::vector<CTacticalPoint>& vPoints) const;

	// Take a query instance (request) and convert it into an evaluation structure
	// Moves structure into m_eEvalState == eHeapEvaluation (we return true), or eError (we return false)
	// Note we skip states currently
	// Usually starts from eReady...
	bool SetupQueryEvaluation(const SQueryInstance& instance, SQueryEvaluation& evaluation) const;

	// Continue evaluation of a query, whatever stage it might be at
	// Structure should be m_eEvalState == {eInitialized, eHeapEvaluation, eCompletedOption }
	// Moves structure to {eHeapEvaluation, eCompletedOption, eCompleted}
	bool ContinueQueryEvaluation(SQueryEvaluation& evaluation, CTimeValue timeLimit) const;

	// For a query option, set up evaluation of points, including generation and heapifying
	// -----------Structure should be m_eEvalState == {eInitialized, eCompletedOption }
	// -----------Moves structure to {eHeapEvaluation, eCompletedOption, eCompleted}
	bool SetupHeapEvaluation(const std::vector<CCriterion>& vConditions, const std::vector<CCriterion>& vWeights, const QueryContext& context, const std::vector<CTacticalPoint>& vPoints, int n, SQueryEvaluation& evaluation) const;

	// Takes a vector of points and vectors of conditions and weights to evaluate them with, upon the given actor
	// Returns up to n valid points in the same vector
	// Structure should be m_eEvalState == {eInitialized, eHeapEvaluation}
	bool ContinueHeapEvaluation(SQueryEvaluation& eval, CTimeValue timeLimit) const;

	// Callback with results from a completed async query
	void CallbackQuery(SQueryEvaluation& evaluation);

	// Test a single point against a single criterion, given an actor
	bool       Test(const CCriterion& criterion, const CTacticalPoint& point, const QueryContext& context, bool& result) const;
	// Test a single point against a single criterion, given an actor
	AsyncState DeferredTest(const CCriterion& criterion, const CTacticalPoint& point, SQueryEvaluation& eval, bool& result) const;
	// Calculate a single weighting criterion for a single point, given an actor
	bool       Weight(const CCriterion& criterion, const CTacticalPoint& point, const QueryContext& context, float& result) const;
	// Generate a set of points given a single criterion and an actor and any option criteria
	bool       Generate(const CCriterion& criterion, const QueryContext& context, const COptionCriteria* pOption, TTacticalPoints& accumulator) const;
	bool       GenerateInternal(TTacticalPointQuery query, const QueryContext& context, float fSearchDist, const COptionCriteria* pOption,
	                            CAIObject* pObject, const Vec3& vObjectPos, CAIObject* pObjectAux, const Vec3& vObjectAuxPos, TTacticalPoints& accumulator) const;

	// If a query is based on a Boolean Property we gain the core bool result here before processing
	bool                        BoolProperty(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, bool& result) const;
	bool                        BoolPropertyInternal(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, bool& result) const;
	// If a query is based on a Boolean Test we gain the core bool result here before processing
	bool                        BoolTest(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, bool& result) const;
	bool                        BoolTestInternal(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, bool& result) const;
	AsyncState                  DeferredBoolTest(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, SQueryEvaluation& eval, bool& result) const;
	ETacticalPointDeferredState DeferredBoolTestInternal(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, SQueryEvaluation& eval, bool& result) const;

	// If a query is based on a Real Property we gain the core _absolute_ float result here before processing
	bool RealProperty(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, float& result) const;
	bool RealPropertyInternal(TTacticalPointQuery query, const CTacticalPoint& point, const QueryContext& context, float& result) const;
	// If a query is based on a Real Measure we gain the core _absolute_ float result here before processing
	bool RealMeasure(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, float& result) const;
	bool RealMeasureInternal(TTacticalPointQuery query, TTacticalPointQuery object, const CTacticalPoint& point, const QueryContext& context, float& result) const;

	// Obtain the absolute range of any given _real_ query,
	// in the context of this complete query option, but independent of any given point
	bool RealRange(TTacticalPointQuery query, float& min, float& max) const;
	bool RealRangeInternal(TTacticalPointQuery query, float& min, float& max) const;

	// Apply limits to an absolute float result. This is trivial - in break from convention we assume it cannot fail.
	inline bool Limit(TTacticalPointQuery limit, float fAbsoluteQueryResult, float fComparisonValue) const;
	// Fetch object as a CAIObject. This might or might not later be moved into the Test and Weight methods?
	inline bool GetObject(TTacticalPointQuery object, const QueryContext& context, CAIObject*& pObject, Vec3& vObjPos) const;
	inline bool GetObjectInternal(TTacticalPointQuery object, const QueryContext& context, CAIObject*& pObject, Vec3& vObjPos) const;
	inline Vec3 GetObjectInternalDir(TTacticalPointQuery object, const QueryContext& context) const;

	bool        Parse(const char* sSpec, TTacticalPointQuery& query, TTacticalPointQuery& limits,
	                  TTacticalPointQuery& object, TTacticalPointQuery& objectAux) const;
	bool        Unparse(const CCriterion& criterion, string& description) const;
	bool        CheckToken(TTacticalPointQuery token) const;

	// Our own concept of "hostile" target
	bool IsHostile(EntityId entityId1, EntityId entityId2) const;

	// Convert a single word into a single TPS token - or eTPQ_None if unrecognised
	TTacticalPointQuery Translate(const char* sWord) const;
	// Convert a single TPS token into a single word - or NULL if unrecognised
	const char*         Translate(TTacticalPointQuery etpToken) const;

	// Convert a single word into a single TPS parameter token - or eTPQP_None if unrecognised
	ETacticalPointQueryParameter TranslateParam(const char* sWord) const;
	ETPSRelativeValueSource      TranslateRelativeValueSource(const char* sWord) const;

	// bit of duplicaiton here...?
	const CTacticalPointQuery* GetQuery(TPSQueryID nQueryID) const;
	COptionCriteria*           GetQueryOptionPointer(TPSQueryID queryID, int options);

	// Generate a warning message, giving as many standard details of the context as possible
	void TPSDescriptionWarning(const CTacticalPointQuery& query, const QueryContext& context, const COptionCriteria* pOption, const char* sMessage = NULL) const;

#ifdef CRYAISYSTEM_DEBUG
	void AddQueryForDebug(SQueryEvaluation& evaluation);
#endif

	static CTacticalPointSystem*                   s_pTPS;   // The singleton

	mutable string                                 sBuffer;  // Buffer used in string lookup

	std::map<string, TTacticalPointQuery>          m_mStringToToken; // Map of strings to their tokens
	std::map<TTacticalPointQuery, string>          m_mTokenToString; // Map of tokens to their strings

	std::map<string, ETacticalPointQueryParameter> m_mParamStringToToken;               // Mapping of string parameters to enum values
	std::map<string, ETPSRelativeValueSource>      m_mRelativeValueSourceStringToToken; // Mapping of string RelativeValueSource descriptions to enum values

	std::map<TTacticalPointQuery, int>             m_mIDToCost; // Map of strings to query costs
	int m_CostMap[eTPQC_COUNT];
	int m_GameQueryIdMap[eTPQT_COUNT];

	typedef std::vector<ITacticalPointLanguageExtender*> TLanguageExtenders;
	TLanguageExtenders m_LanguageExtenders;

	typedef CCountedRef<CAIObject> TDummyRef;
	typedef std::vector<TDummyRef> TDummyObjects;
	TDummyObjects  m_LanguageExtenderDummyObjects;

	TPSQueryTicket m_nQueryInstanceTicket;

	std::map<TPSQueryTicket, const SQueryInstance> m_mQueryInstanceQueue;
	std::map<TPSQueryTicket, SQueryEvaluation>     m_mQueryEvaluationsInProgress;

#ifdef CRYAISYSTEM_DEBUG
	std::list<SQueryEvaluation> m_lstQueryEvaluationsForDebug;
#endif

	TPSQueryID                                m_LastQueryID; // ID of last query created
	std::map<TPSQueryID, CTacticalPointQuery> m_mIDToQuery;  // Map of IDs to stored queries
	std::map<string, TPSQueryID>              m_mNameToID;   // Map of strings to query IDs

	typedef std::map<TPSQueryTicket, TTacticalPoints> TLockedPoints;
	TLockedPoints                             m_locked;

	mutable std::vector<Vec3>                 m_occupiedSpots;
	mutable std::vector<STacticalPointResult> m_points;

	struct AvoidCircle
	{
		Vec3  pos;
		float radius;
	};

	struct CVarsDef
	{
		int   DebugTacticalPoints;
		int   DebugTacticalPointsBlocked;
		int   TacticalPointsDebugDrawMode;
		int   TacticalPointsDebugFadeMode;
		float TacticalPointsDebugScaling;
		float TacticalPointsDebugTime;
		int   TacticalPointsWarnings;
	};

	typedef std::vector<AvoidCircle> AvoidCircles;
	mutable AvoidCircles                  m_avoidCircles;
	void GatherAvoidCircles(const Vec3& center, float range, CAIObject* requester, AvoidCircles& avoidCircles) const;
	mutable CCoverSystem::CoverCollection m_cover;
	void GatherCoverEyes();

	void VisibleRayComplete(const QueuedRayID& rayID, const RayCastResult& result);
	void CanShootRayComplete(const QueuedRayID& rayID, const RayCastResult& result);
	void CanShootSecondRayComplete(const QueuedRayID& rayID, const RayCastResult& result);

	// CVars
	static CVarsDef CVars;
};

#endif //__TacticalPointSystem_H__
