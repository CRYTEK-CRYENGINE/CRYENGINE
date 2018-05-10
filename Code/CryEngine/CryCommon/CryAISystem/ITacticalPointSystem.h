// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

#include <CryAISystem/IAISystem.h>    // <> required for Interfuscator
#include <CryAISystem/IAIObject.h>    // <> required for Interfuscator
#include <CryAISystem/ICoverSystem.h> // <> required for Interfuscator

// These should live somewhere like GoalOp.h, but it's still needed by Game02/Trooper.h :(
enum EAnimationMode
{
	AIANIM_INVALID,
	AIANIM_SIGNAL = 1,
	AIANIM_ACTION,
};

enum EJumpAnimType
{
	JUMP_ANIM_FLY,
	JUMP_ANIM_LAND
};

struct IPipeUser;
struct SHideSpot;
struct SHideSpotInfo;
struct ITacticalPointResultsReceiver;

enum type_invalid_ticket { INVALID_TICKET = 0 };

//! Provide some minimal type-safety between different int ticket systems.
template<int i> struct STicket
{
	int n;
	STicket() : n(0) {};
	STicket(int _n) : n(_n) {};
	STicket(type_invalid_ticket) : n(INVALID_TICKET) {}
	operator int() const { return n; }
	void Advance() { n++; }
};

typedef STicket<1> TPSQueryID;
typedef STicket<2> TPSQueryTicket;

struct QueryContext
{
	Vec3                   actorPos;
	Vec3                   actorDir;
	IAISystem::tNavCapMask actorNavCaps;
	EntityId               actorEntityId;

	float                  actorRadius;
	float                  distanceToCover;
	float                  inCoverRadius;
	float                  effectiveCoverHeight;

	Vec3                   attentionTarget;
	Vec3                   attentionTargetDir;

	Vec3                   realTarget;
	Vec3                   realTargetDir;

	Vec3                   referencePoint;
	Vec3                   referencePointDir;

	//! \note To use all the features of TPS, define the calling actor here.
	//! \note A pointer is not safe for async queries! Of course, this does not affect games that don't use actors.
	//! Quite easy to convert to a tAIObjectID, but ensure to do the lookup once when query starts and cache the pointer.
	IAIActor* pAIActor;

	QueryContext()
		: actorPos(ZERO)
		, actorDir(ZERO)
		, actorNavCaps(0)
		, actorEntityId(0)
		, actorRadius(0.0f)
		, distanceToCover(0.0f)
		, inCoverRadius(0.0f)
		, effectiveCoverHeight(0.0f)
		, attentionTarget(ZERO)
		, attentionTargetDir(ZERO)
		, realTarget(ZERO)
		, realTargetDir(ZERO)
		, referencePoint(ZERO)
		, referencePointDir(ZERO)
		, pAIActor(0)
	{
	}
};

/* Extending a Query to your Game Project - quickstart - KAK 15/06/2009
   1. Create an instance of ITacticalPointLanguageExtender
   2. Define the query with its cost value in the appropriate category (ITacticalPointSystem::ExtendQueryLanguage())
   3. Overload the appropriate member function from ITacticalPointLanguageExtender to handle the new vocabulary
 */

//! Query type flags.
//! \note If modified, special handling needs to be done in ITacticalPointSystem::ExtendQueryLanguage.
enum ETacticalPointQueryType
{
	eTPQT_PROP_BOOL = 0,              //!< Flags queries that depend on no Object and are Boolean-valued.
	eTPQT_PROP_REAL,                  //!< Flags queries that depend on no Object and are Real-valued.
	eTPQT_TEST,                       //!< Flags queries that use an Object and are Boolean-valued.
	eTPQT_MEASURE,                    //!< Flags queries that use an Object and are Real-valued.
	eTPQT_GENERATOR,                  //!< Flags queries that generate points.
	eTPQT_GENERATOR_O,                //!< Flags queries that generate points referring to an object.
	eTPQT_PRIMARY_OBJECT,             //!< Starts range describing the primary Object.

	eTPQT_COUNT,
};

//! Query cost helpers.
enum ETacticalPointQueryCost
{
	eTPQC_IGNORE = -1,

	eTPQC_CHEAP  = 0,                //!< Cheap tests which use no heavy processing time (some math and no iteration).
	eTPQC_MEDIUM,                    //!< Medium tests (one ray trace or test, iteration of simple operation).
	eTPQC_EXPENSIVE,                 //!< Heavy processing time (multiple ray traces, multiple point tests, etc).
	eTPQC_DEFERRED,

	eTPQC_COUNT,
};

//! States of deferred tests.
enum ETacticalPointDeferredState
{
	eTPDS_Failed = 0,                                       //!< Operation execution failed.
	eTPDS_InProgress,                                       //!< Operation is still processing a request.
	eTPDS_Complete,                                         //!< Opertion completed, result is ready.
	eTPDS_UnknownRequest                                    //!< Request is unknown or not implemented.
};

//! Defines a tactical point with optional info.
struct ITacticalPoint
{
	enum ETacticalPointType
	{
		eTPT_None = 0,
		eTPT_HideSpot,
		eTPT_Point,
		eTPT_EntityPos,
		eTPT_AIObject,
		eTPT_CoverID,
	};

	// <interfuscator:shuffle>
	virtual ~ITacticalPoint() {}
	virtual Vec3                               GetPos() const = 0;
	virtual void                               SetPos(Vec3 pos) = 0;
	virtual ITacticalPoint::ETacticalPointType GetType() const = 0;
	virtual const SHideSpot*                   GetHidespot() const = 0;
	virtual tAIObjectID                        GetAIObjectId() const = 0;
	virtual bool                               IsValid() const = 0;
	// </interfuscator:shuffle>
};

//! \see STacticalPointResult for definitions.
enum ETacticalPointDataFlags
{
	eTPDF_None      = 0,      //!< No data - invalid hidespot.
	eTPDF_Pos       = 1 << 0, //!< All valid points should have this set.
	eTPDF_EntityId  = 1 << 1,
	eTPDF_ObjectPos = 1 << 2,
	eTPDF_ObjectDir = 1 << 3,
	eTPDF_Hidespot  = 1 << 4,
	eTPDF_AIObject  = 1 << 5,
	eTPDF_CoverID   = 1 << 6,
};

enum ETacticalPointQueryFlags
{
	eTPQF_None        = 0,
	eTPQF_LockResults = 1 << 0,                         //!< This can be expensive, so use with care.
};

//! Convenient masks defining commonly-used classes of point.
enum
{
	eTPDF_Mask_AbstractPoint    = eTPDF_Pos,
	eTPDF_Mask_AbstractHidespot = eTPDF_Pos | eTPDF_ObjectDir,
	eTPDF_Mask_LegacyHidespot   = eTPDF_Pos | eTPDF_ObjectDir | eTPDF_Hidespot,
};

//! Left just as a struct.
//! Could be dressed up with accessors, but checking the flags yourself is probably the most natural way to use it.
struct STacticalPointResult
{
	//! ETacticalPointDataFlags - flags the data valid in this result.
	uint32 flags;

	//! Position used by the query - present for every valid result.
	Vec3 vPos;

	//! If eTPDF_EntityId set: Entity from which point is derived.
	//! E.g.: Entity generating the point or entity of dynamic hidespot object.
	EntityId entityId;

	//! If eTPDF_ObjectPos set: Position of object from which this point is derived.
	//! E.g.: Position of the entity, cover object.
	Vec3 vObjPos;

	//! If eTPDF_ObjectDir set: Direction implied by the object.
	//! E.g.: Anchor direction, direction from hide point towards cover.
	Vec3 vObjDir;

	// This can't be exposed here {2009/07/31}.
	// If eTPDF_Hidespot set:    Legacy hidespot structure.
	// SHideSpot hidespot;

	//! If eTPDF_AIObject set: ID of AI object from which the point is derived.
	//! E.g.: Anchor direction, direction from hide point towards cover.
	tAIObjectID aiObjectId;

	CoverID     coverID;

	STacticalPointResult() { Invalidate(); }
	void Invalidate()    { flags = eTPDF_None; vPos = vObjPos = vObjDir = ZERO; entityId = aiObjectId = 0; }
	bool IsValid() const { return flags != eTPDF_None; }
};

//! Defines a point definer for generation query usage.
struct ITacticalPointGenerateResult
{
	// <interfuscator:shuffle>
	virtual ~ITacticalPointGenerateResult() {}
	virtual bool AddHideSpot(const SHideSpot& hidespot) = 0;
	virtual bool AddPoint(const Vec3& point) = 0;
	virtual bool AddEntity(IEntity* pEntity) = 0;
	virtual bool AddEntityPoint(IEntity* pEntity, const Vec3& point) = 0;
	// </interfuscator:shuffle>
};

//! Defines a language extender for game projects.
struct ITacticalPointLanguageExtender
{
	typedef const char*           TQueryType;
	typedef const QueryContext&   TOwnerType;
	typedef IAIObject* const      TObjectType;
	typedef const ITacticalPoint& TPointType;

	//! Parameters struct for generic information.
	template<typename T>
	struct SExtenderParameters
	{
		TQueryType query;
		TOwnerType pOwner;
		T&         result;

		SExtenderParameters(TQueryType _query, TOwnerType _pOwner, T& _result) : query(_query), pOwner(_pOwner), result(_result) {}
	};
	template<typename T>
	struct SRangeExtenderParameters
	{
		TQueryType query;
		T&         minParam;
		T&         maxParam;

		SRangeExtenderParameters(TQueryType _query, T& _min, T& _max) : query(_query), minParam(_min), maxParam(_max) {}
	};

	typedef SExtenderParameters<ITacticalPointGenerateResult*> TGenerateParameters;
	typedef SExtenderParameters<IAIObject*>                    TObjectParameters;
	typedef SExtenderParameters<bool>                          TBoolParameters;
	typedef SExtenderParameters<float>                         TRealParameters;
	typedef SRangeExtenderParameters<float>                    TRangeParameters;

	typedef Functor0                                           TDeferredCancelFunc;

	//! Generate struct for generate-specific information.
	struct SGenerateDetails
	{
		float  fSearchDist;
		float  fDensity;
		float  fHeight;
		string tagPointPostfix;
		string extenderStringParameter;

		SGenerateDetails(float _fSearchDist, float _fDensity, float _fHeight, string _tagPointPostfix)
			: fSearchDist(_fSearchDist)
			, fDensity(_fDensity)
			, fHeight(_fHeight)
			, tagPointPostfix(_tagPointPostfix)
		{}
	};

	// <interfuscator:shuffle>
	virtual ~ITacticalPointLanguageExtender() {}

	//! Generate points.
	virtual bool GeneratePoints(TGenerateParameters& parameters, SGenerateDetails& details, TObjectType pObject, const Vec3& vObjectPos, TObjectType pObjectAux, const Vec3& vObjectAuxPos) const { return false; }

	//! Get primary object.
	virtual bool GetObject(TObjectParameters& parameters) const { return false; }

	//! Property tests.
	virtual bool BoolProperty(TBoolParameters& parameters, TPointType point) const                                             { return false; }
	virtual bool BoolTest(TBoolParameters& parameters, TObjectType pObject, const Vec3& vObjectPos, TPointType point) const    { return false; }
	virtual bool RealProperty(TRealParameters& parameters, TPointType point) const                                             { return false; }
	virtual bool RealMeasure(TRealParameters& parameters, TObjectType pObject, const Vec3& vObjectPos, TPointType point) const { return false; }

	//! This function will be called several times for the same point as long as it returns eTPDS_InProgress.
	//! Function can set onDeferredCancel callback functor every time it returns eTPDS_InProgress - same function will be passed next time.
	//! onDeferredCancel is set internally to "nullptr" when the test reports, that it's not in progress (completed, failed, ...).
	//! onDeferredCancel callback will be called only if the query instance is cancelled to allow extender clear its state.
	//! If the test is finished (completed, failed, ...), it's an extenders responsibility to clear its state beforehand - onDeferredCancel callback
	//! will not be called in this case.
	//! If the extender doesn't have an implementation for test, return eTPDS_UnknownRequest.
	virtual ETacticalPointDeferredState DeferredBoolTest(TBoolParameters& parameters, TObjectType pObject, const Vec3& vObjectPos, TPointType point, TDeferredCancelFunc& onDeferredCancel) { return eTPDS_UnknownRequest; }

	virtual bool                        RealRange(TRangeParameters& parameters) const                                                                                                       { return false; }
	// </interfuscator:shuffle>
};

//! Simplified interface for querying from outside the AI system.
//! Style encourages "precompiling" queries for repeated use.
struct ITacticalPointSystem
{
	// <interfuscator:shuffle>
	virtual ~ITacticalPointSystem() {}

	virtual void Update(const float fBudgetSeconds) = 0;

	//! Extend the language by adding new keywords.
	//! For Generators and Primary Objects, the cost is not relevant (use eTPQC_IGNORE).
	virtual bool ExtendQueryLanguage(const char* szName, ETacticalPointQueryType eType, ETacticalPointQueryCost eCost) = 0;

	//! Language extenders.
	virtual bool       AddLanguageExtender(ITacticalPointLanguageExtender* pExtender) = 0;
	virtual bool       RemoveLanguageExtender(ITacticalPointLanguageExtender* pExtender) = 0;
	virtual IAIObject* CreateExtenderDummyObject(const char* szDummyName) = 0;
	virtual void       ReleaseExtenderDummyObject(tAIObjectID id) = 0;

	//! Get a new query ID, to allow us to build a new query.
	virtual TPSQueryID CreateQueryID(const char* psName) = 0;

	//! Destroy a query ID and release all resources associated with it.
	virtual bool DestroyQueryID(TPSQueryID queryID) = 0;

	//! Get the Name of a query by ID.
	virtual const char* GetQueryName(TPSQueryID queryID) = 0;

	//! Get the ID number of a query by name.
	virtual TPSQueryID GetQueryID(const char* psName) = 0;

	//! Get query option.
	virtual const char* GetOptionLabel(TPSQueryID queryID, int option) = 0;

	//! Build up a query.
	//! The "option" parameter allows you to build up fallback options.
	virtual bool AddToParameters(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) = 0;
	virtual bool AddToParameters(TPSQueryID queryID, const char* sSpec, bool bValue, int option = 0) = 0;
	virtual bool AddToParameters(TPSQueryID queryID, const char* sSpec, const char* sValue, int option = 0) = 0;
	virtual bool AddToGeneration(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) = 0;
	virtual bool AddToGeneration(TPSQueryID queryID, const char* sSpec, const char* sValue, int option = 0) = 0;
	virtual bool AddToConditions(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) = 0;
	virtual bool AddToConditions(TPSQueryID queryID, const char* sSpec, bool bValue, int option = 0) = 0;
	virtual bool AddToWeights(TPSQueryID queryID, const char* sSpec, float fValue, int option = 0) = 0;

	//! Start a new asynchronous query. Returns the id "ticket" for this query instance.
	//! Are types needed to avoid confusion?
	virtual TPSQueryTicket AsyncQuery(TPSQueryID queryID, const QueryContext& m_context, int flags, int nPoints, ITacticalPointResultsReceiver* pReciever) = 0;

	virtual void           UnlockResults(TPSQueryTicket queryTicket) = 0;
	virtual bool           HasLockedResults(TPSQueryTicket queryTicket) const = 0;

	//! Cancel an asynchronous query.
	virtual bool CancelAsyncQuery(TPSQueryTicket ticket) = 0;
	// </interfuscator:shuffle>
};

struct ITacticalPointResultsReceiver
{
	// <interfuscator:shuffle>
	//! Ticket is set even if bError is true to identify the query request, but no results will be returned.
	virtual void AcceptResults(bool bError, TPSQueryTicket nQueryTicket, STacticalPointResult* vResults, int nResults, int nOptionUsed) = 0;
	virtual ~ITacticalPointResultsReceiver() {}
	// </interfuscator:shuffle>
};

//! Maybe eTPSQS_None would be better than Fail, also as init value, and as first member of the enum.
enum ETacticalPointQueryState
{
	eTPSQS_InProgress,                                       //!< Currently waiting for query results.
	eTPSQS_Success,                                          //!< Query completed and found at least one point.
	eTPSQS_Fail,                                             //!< Query completed but found no acceptable points.
	eTPSQS_Error,                                            //!< Query had errors - used in wrong context?.
};

//! Deferred TPS query helper.
//! Makes it easy to prepare a query and then request it repeatedly.
class CTacticalPointQueryInstance : private ITacticalPointResultsReceiver
{
public:
	CTacticalPointQueryInstance() : m_eStatus(eTPSQS_Fail) {}
	CTacticalPointQueryInstance(TPSQueryID queryID, const QueryContext& context) : m_eStatus(eTPSQS_Fail) {}
	~CTacticalPointQueryInstance() { Cancel(); }

	void SetQueryID(TPSQueryID queryID)
	{
		m_queryID = queryID;
	}
	void SetContext(const QueryContext& context)
	{
		m_context = context;
	}
	TPSQueryID GetQueryID() const
	{
		return m_queryID;
	}
	const QueryContext& GetContext() const
	{
		return m_context;
	}

	//! Execute the given query with given context, with a single-point result.
	//! Returns status which should be checked - especially for error.
	ETacticalPointQueryState Execute(int flags)
	{
		m_Results.Reset();
		return SetupAsyncQuery(1, flags);
	}

	//! Execute the given query with given context, with up to n points result.
	//! If pResults is given, they will be written to that array, which should of course be of size >= n.
	//! If it is not given, an internal buffer will be used.
	//! Returns status which should be checked - especially for error.
	ETacticalPointQueryState Execute(int nPoints, int flags, STacticalPointResult* pResults)
	{
		m_Results.Reset();
		m_Results.m_pResultsArray = pResults;
		return SetupAsyncQuery(nPoints, flags);
	}

	// Using internal buffers doesn't make sense, but we can allow the user to provide one with minimal effort.
	// Actually, use of std::vector across boundaries and possible allocation in different threads might make this a tricky feature.
	// ETacticalPointQueryState Execute( int nPoints, std::vector<Vec3> & pResults  );

	//! Only a placeholder! Asynchronous equivalent to TestConditions method in ITacticalPointSystem interface.
	//! Test a given point if it fulfills conditions of a given query. No weights will be evaluated.
	ETacticalPointQueryState Execute(STacticalPointResult& result)
	{
		// Needs new input/interface to feed in a specific point
		assert(false);
		return eTPSQS_Error;
	}

	//! Cancel any query currently in progress.
	//! You should explicitly cancel queries.
	void Cancel()
	{
		if (m_nQueryInstanceTicket)
		{
			ITacticalPointSystem* pTPS = gEnv->pAISystem->GetTacticalPointSystem();
			pTPS->CancelAsyncQuery(m_nQueryInstanceTicket);
			pTPS->UnlockResults(m_nQueryInstanceTicket);
		}

		m_Results.Reset();
		m_eStatus = eTPSQS_Fail;
	}

	void UnlockResults()
	{
		if (m_nQueryInstanceTicket)
		{
			ITacticalPointSystem* pTPS = gEnv->pAISystem->GetTacticalPointSystem();
			pTPS->UnlockResults(m_nQueryInstanceTicket);
		}
	}

	ETacticalPointQueryState GetStatus() const               //!< Get current status.
	{
		return m_eStatus;
	}
	STacticalPointResult GetBestResult() const               //!< Best-scoring or only returned point, or zero vector if none.
	{
		return m_Results.m_vSingleResult;
	}
	int GetOptionUsed() const                                //!< Query option chosen, or -1 if none.
	{
		return m_Results.m_nOptionUsed;
	}
	int GetResultCount() const
	{
		return m_Results.m_nValidPoints;
	}

private:
	// ITacticalPointResultsReceiver.
	void AcceptResults(bool bError, TPSQueryTicket nQueryTicket, STacticalPointResult* vResults, int nResults, int nOptionUsed)
	{
		// Check that the answer matches the original question
		assert(nQueryTicket == m_nQueryInstanceTicket);

		// Check for error
		if (bError)
		{
			// Check consistency
			assert(nOptionUsed == -1 && nResults == 0 && vResults == NULL);

			// Ignore any results, set error status
			m_Results.Reset();
			m_eStatus = eTPSQS_Error;
			return;
		}

		if (nResults > 0)
		{
			// Check consistency
			assert(nOptionUsed > -1 && vResults != NULL);

			// Success
			if (m_Results.m_pResultsArray)
			{
				for (int i = 0; i < nResults; i++)
					m_Results.m_pResultsArray[i] = vResults[i];
			}
			else
			{
				assert(nResults == 1); // Actually, we should check if the request itself was for just 1 points
				m_Results.m_vSingleResult = vResults[0];
			}
			m_Results.Set(nOptionUsed, nResults, NULL, vResults[0]);
			m_eStatus = eTPSQS_Success;
		}
		else
		{
			// Check consistency
			assert(nOptionUsed == -1 && vResults == NULL);

			// Failed to find points, but without error
			// It might be polite to erase the results array (but a little wasteful)
			m_Results.Reset();
			m_eStatus = eTPSQS_Fail;
		}
	}
	// ~ITacticalPointResultsReceiver

	//! Set up for and start an async query.
	ETacticalPointQueryState SetupAsyncQuery(int nPoints, int flags)
	{
		if (m_eStatus == eTPSQS_InProgress)
		{
			// Disallow implicit cancelling - just return an error immediately
			m_eStatus = eTPSQS_Error;
			return m_eStatus;
		}

		// Mark as in progress, send this query to the TPS and get a ticket back
		m_eStatus = eTPSQS_InProgress;
		ITacticalPointSystem* pTPS = gEnv->pAISystem->GetTacticalPointSystem();
		if (m_nQueryInstanceTicket)
			pTPS->UnlockResults(m_nQueryInstanceTicket);
		m_nQueryInstanceTicket = pTPS->AsyncQuery(m_queryID, m_context, flags, nPoints, this);
		if (!m_nQueryInstanceTicket) // query didn't exist
			m_eStatus = eTPSQS_Error;

		// It is possible that the system made an answering callback immediately - usually on failure (-;
		// So, m_eStatus may already have been changed at this point - which is fine.
		if (m_eStatus != eTPSQS_InProgress)
		{
			// Might be useful to put a breakpoint here if debugging
		}

		return m_eStatus;
	}

	ETacticalPointQueryState m_eStatus;
	TPSQueryTicket           m_nQueryInstanceTicket;
	TPSQueryID               m_queryID;
	QueryContext             m_context;

	struct Results
	{
		//! Helper: make sure you set all values into a consistent state.
		void Set(int nOptionUsed, int nValidPoints, STacticalPointResult* pResultsArray, const STacticalPointResult& vSingleResult)
		{
			m_nOptionUsed = nOptionUsed;
			m_nValidPoints = nValidPoints;
			m_pResultsArray = pResultsArray;
			m_vSingleResult = vSingleResult;
		}

		void Reset()
		{
			m_nOptionUsed = -1;
			m_nValidPoints = 0;
			m_pResultsArray = NULL;
			m_vSingleResult.Invalidate();
		}

		int                   m_nOptionUsed;
		int                   m_nValidPoints;
		STacticalPointResult  m_vSingleResult;
		STacticalPointResult* m_pResultsArray;                                 //!< May be set to a receiving array, always reset to NULL after each query.
		//std::vector<Vec3> *m_pResultsVector;
	} m_Results;
};

//! \cond INTERNAL