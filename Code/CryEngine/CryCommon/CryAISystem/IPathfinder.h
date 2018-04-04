// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Pathfinder Interface.

   -------------------------------------------------------------------------
   History:
   - 15:1:2009   18:01 : Created by Márcio Martins
   - 4 May 2009        : Evgeny Adamenkov: Removed IRenderer

*************************************************************************/

#ifndef __IPATHFINDER_H__
#define __IPATHFINDER_H__

#if _MSC_VER > 1000
	#pragma once
#endif

class CAIActor;

struct IAIPathFinderListerner;
struct IAIPathAgent;

#include <CryMemory/IMemory.h> // <> required for Interfuscator
#include <CryAISystem/INavigationSystem.h>
#include <CryCore/functor.h>
#include <CryAISystem/IMNM.h>
#include <CryAISystem/IAgent.h>

/* WARNING: These interfaces and structures are soon to be deprecated.
            Use at your own risk of having to change your code later!
 */

//! Passing through navigational SO methods.
enum ENavSOMethod
{
	nSOmNone,               //!< Not passing or not passable.
	nSOmAction,             //!< Execute an AI action.
	nSOmPriorityAction,     //!< Execute a higher priority AI action.
	nSOmStraight,           //!< Just pass straight.
	nSOmSignalAnimation,    //!< Play signal animation.
	nSOmActionAnimation,    //!< Play action animation.
	nSOmLast
};

//! Represents an object that might be blocking a link.
//! Each blocker is assumed to be spherical, with the position centred around the floor so that links can intersect it.
struct NavigationBlocker
{
	//! \param pos Centre of the sphere.
	//! \param radius Radius of the sphere.
	//! \param costAddMod Fixed cost (in m) associated with the blocker obscuring a link - a value of 0 has no effect - a value of 10 would make the link effectively 10m longer than it is.
	//! \param costMultMod Cost modification factor - a value of 0 has no effect - a value of 10 would make the link. 10x more costly. -ve disables the link.
	//! \param radialDecay Indicates if the cost modifiers should decay linearly to 0 over the radius of the sphere.
	//! \param directional Indicates if the cost should be unaffected for motion in a radial direction.
	NavigationBlocker(const Vec3& pos, float radius, float costAddMod, float costMultMod, bool radialDecay, bool directional)
		: sphere(pos, radius), costAddMod(costAddMod), costMultMod(costMultMod), restrictedLocation(false),
		radialDecay(radialDecay), directional(directional) {}

	//! Just to allow std::vector::resize(0).
	NavigationBlocker() : sphere(Vec3(0, 0, 0), 0.0f), costAddMod(0), costMultMod(0), radialDecay(false) { assert("Should never get called"); }

	Sphere sphere;
	bool   radialDecay;
	bool   directional;

	//! Absolute cost added to any link going through this blocker (useful for small blockers).
	float costAddMod;

	//! Multiplier for link costs going through this blocker (0 means no extra cost, 1 means to double etc).
	float costMultMod;

	//! Info to speed up the intersection checks.
	//! If this is true then the blocker is small enough that it only affects the nav type it resides in. If false then it affects everything.
	bool                       restrictedLocation;
	IAISystem::ENavigationType navType;
	union Location
	{

		//! Similar for other nav types when there's info to go in them.
		//! \note No node because the node "areas" can overlap so it's not useful.
		struct
		{
			int nBuildingID;
		} waypoint;
	};

	//! Only gets used if restrictedLocation = true.
	Location location;
};

typedef DynArray<NavigationBlocker> NavigationBlockers;

struct PathPointDescriptor
{
	struct SmartObjectNavData : public _i_reference_target_t
	{
		unsigned fromIndex;
		unsigned toIndex;

		//! Callable only inside the AISystem module. It's implemented there.
		void Serialize(TSerialize ser);
	};
	typedef _smart_ptr<SmartObjectNavData> SmartObjectNavDataPtr;

	struct OffMeshLinkData
	{
		OffMeshLinkData()
			: offMeshLinkID(0)
		{}

		MNM::OffMeshLinkID offMeshLinkID;
	};

	PathPointDescriptor(IAISystem::ENavigationType _navType = IAISystem::NAV_UNSET, const Vec3& _vPos = ZERO)
		: vPos(_vPos)
		, navType(_navType)
		, navTypeCustomId(0)
		, iTriId(0)
		, pSONavData(0)
		, navSOMethod(nSOmNone)
	{}

	PathPointDescriptor(const Vec3& _vPos)
		: vPos(_vPos)
		, navType(IAISystem::NAV_UNSET)
		, navTypeCustomId(0)
		, iTriId(0)
		, pSONavData(0)
		, navSOMethod(nSOmNone)
	{}

	//! Callable only inside the AISystem module. It's implemented there.
	void Serialize(TSerialize ser);

	bool IsEquivalent(const PathPointDescriptor& other) const
	{
		return (navType == other.navType) && vPos.IsEquivalent(other.vPos, 0.01f);
	}

	static bool ArePointsEquivalent(const PathPointDescriptor& point1, const PathPointDescriptor& point2)
	{
		return point1.IsEquivalent(point2);
	}

	Vec3                       vPos;
	IAISystem::ENavigationType navType;
	uint16                     navTypeCustomId;

	uint32                     iTriId;
	OffMeshLinkData            offMeshLinkData;

	SmartObjectNavDataPtr      pSONavData;

	ENavSOMethod               navSOMethod;

};

struct PathfindingExtraConstraint
{
	enum EExtraConstraintType
	{
		ECT_MAXCOST,
		ECT_MINDISTFROMPOINT,
		ECT_AVOIDSPHERE,
		ECT_AVOIDCAPSULE
	};

	EExtraConstraintType type;

	union UConstraint
	{
		struct SConstraintMaxCost
		{
			float maxCost;
		};
		struct SConstraintMinDistFromPoint
		{
			float px, py, pz; //!< Can't use Vec3 as it has a constructor.
			float minDistSq;
		};
		struct SConstraintAvoidSphere
		{
			float px, py, pz; //!< Can't use Vec3 as it has a constructor.
			float minDistSq;
		};
		struct SConstraintAvoidCapsule
		{
			float px, py, pz;
			float qx, qy, qz;
			float minDistSq;
		};
		SConstraintMaxCost          maxCost;
		SConstraintMinDistFromPoint minDistFromPoint;
		SConstraintAvoidSphere      avoidSphere;
		SConstraintAvoidCapsule     avoidCapsule;
	};
	UConstraint constraint;
};

typedef DynArray<PathfindingExtraConstraint> PathfindingExtraConstraints;

struct PathfindRequest
{
	enum ERequestType
	{
		TYPE_ACTOR,
		TYPE_RAW,
	};
	ERequestType                type;

	unsigned                    startIndex;
	unsigned                    endIndex;
	Vec3                        startPos;
	Vec3                        startDir;
	Vec3                        endPos;

	Vec3                        endDir; //!< Mangitude of endDir (between 0 and 1) indicates the tendency to line up at the end of the path.
	bool                        bSuccess;
	IAIPathAgent*               pRequester;
	int                         nForceTargetBuildingId;
	bool                        allowDangerousDestination;
	float                       endTol;
	float                       endDistance;

	bool                        isDirectional; //!< As a result of RequestPathInDirection or RequestPathTo.

	bool                        bPathEndIsAsRequested; //! This gets set to false if the path end position doesn't match the requested end position (e.g. in the event of a partial path, or if the destination is in forbidden).

	int                         id;
	IAISystem::tNavCapMask      navCapMask;
	float                       passRadius;

	PathfindingExtraConstraints extraConstraints;

	PathfindRequest(ERequestType type)
		: type(type),
		startIndex(0),
		endIndex(0),
		pRequester(0),
		bPathEndIsAsRequested(false),
		allowDangerousDestination(false),
		endTol(std::numeric_limits<float>::max()),
		endDistance(0),
		nForceTargetBuildingId(-1),
		isDirectional(false),
		id(-1),
		navCapMask(IAISystem::NAV_UNSET),
		passRadius(0.0f)
	{
	}

	//! Callable only inside the AISystem module. It's implemented there.
	void Serialize(TSerialize ser);

	void GetMemoryUsage(ICrySizer* pSizer) const { /*LATER*/ }
};

struct PathfindingHeuristicProperties
{
	PathfindingHeuristicProperties(const AgentPathfindingProperties& properties, const IAIPathAgent* pAgent = 0)
		: agentproperties(properties), pAgent(pAgent) {}

	PathfindingHeuristicProperties()
		: pAgent(0) {}

	AgentPathfindingProperties agentproperties;
	const IAIPathAgent*        pAgent;
};

struct PathFollowerParams
{
	PathFollowerParams()
		: normalSpeed(0.0f),
		pathRadius(0.0f),
		pathLookAheadDist(1.0f),
		maxAccel(0.0f),
		maxDecel(1.0f),
		minSpeed(0.0f),
		maxSpeed(10.0f),
		endAccuracy(0.2f),
		endDistance(0.0f),
		stopAtEnd(true),
		use2D(true),
		isVehicle(false),
		isAllowedToShortcut(true),
		snapEndPointToGround(true),
		navCapMask(IAISystem::NAV_UNSET),
		passRadius(0.5f)
	{}

	// OLD: Remove this when possible, Animation to take over majority of logic.
	float normalSpeed;       //!< Normal entity speed.
	float pathRadius;        //!< Max deviation allowed from the path.
	float pathLookAheadDist; //!< How far we look ahead along the path - normally the same as pathRadius.
	float maxAccel;          //!< Maximum acceleration of the entity.
	float maxDecel;          //!< Maximum deceleration of the entity.
	float minSpeed;          //!< Minimum output speed (unless it's zero on path end etc).
	float maxSpeed;          //!< Maximum output speed.

	//! KEEP: Additions and useful state for new impl.
	float endAccuracy;   //!< How close to the end point the agent must be to finish pathing.
	float endDistance;   //!< Stop this much before the end.
	bool  stopAtEnd;     //!< Aim to finish the path by reaching the end position (stationary), or simply overshoot.
	bool  use2D;         //!< Follow in 2 or 3D.
	bool  isVehicle;
	bool  isAllowedToShortcut;
	bool  snapEndPointToGround; //!< try to make sure, that the path ends on the ground

	// TODO: Add to serialize...
	//! The navigation capabilities of the agent.
	IAISystem::tNavCapMask navCapMask;

	//! The minimum radius of the agent for navigating.
	float passRadius;

	//! Callable only inside the AISystem module. It's implemented there.
	void Serialize(TSerialize ser);
};

struct PathFollowResult
{
	struct SPredictedState
	{
		SPredictedState() {}
		SPredictedState(const Vec3& p, const Vec3& v) : pos(p), vel(v) {}
		Vec3 pos;
		Vec3 vel;
	};
	typedef DynArray<SPredictedState> TPredictedStates;

	// OLD: Obsolete & to be replaced by new impl.

	float             desiredPredictionTime; //!< Maximum time to predict out to - actual prediction may not go this far.

	float             predictionDeltaTime; //!< First element in predictedStates will be now + predictionDeltaTime, etc.

	TPredictedStates* predictedStates;  //!< If this is non-zero then on output the prediction will be placed into it.

	PathFollowResult()
		: predictionDeltaTime(0.1f), predictedStates(0), desiredPredictionTime(0), followTargetPos(0), inflectionPoint(0) {}

	bool reachedEnd;
	Vec3 velocityOut;

	// NEW: Replaces data above.

	Vec3 followTargetPos;  //!< The furthest point on the path we can move directly towards without colliding with anything. If the turningPoint and inflectionPoint are equal, they represent the end of the path.

	Vec3 inflectionPoint;  //!< The next point on the path beyond the follow target that deviates substantially from a straight-line path.

	/// The maximum distance the agent can safely move in a straight line beyond the turning point
	//	float maxOverrunDistance;
};

//! Intermediary and minimal interface to use the pathfinder without requiring an AI object.
// TODO: Fix the long function names. At the moment they collide with IAIObject base. The alternative would be to make IAIObject derive from IAIPathAgent.
struct IAIPathAgent
{
	// <interfuscator:shuffle>
	virtual ~IAIPathAgent(){}
	virtual IEntity*                    GetPathAgentEntity() const = 0;
	virtual const char*                 GetPathAgentName() const = 0;
	virtual unsigned short              GetPathAgentType() const = 0;

	virtual float                       GetPathAgentPassRadius() const = 0;
	virtual Vec3                        GetPathAgentPos() const = 0;
	virtual Vec3                        GetPathAgentVelocity() const = 0;

	virtual const AgentMovementAbility& GetPathAgentMovementAbility() const = 0;

	//! This cannot easily be const, but has no side-effects.
	virtual void GetPathAgentNavigationBlockers(NavigationBlockers& blockers, const PathfindRequest* pRequest) = 0;

	// TODO: Remove this from the interface.
	// Most of it could be stored in the path request, except that it gets set at the start
	// of the path request and it's used everywhere in the AISystem.
	virtual unsigned int GetPathAgentLastNavNode() const = 0;
	virtual void         SetPathAgentLastNavNode(unsigned int lastNavNode) = 0;

	virtual void         SetPathToFollow(const char* pathName) = 0;
	virtual void         SetPathAttributeToFollow(bool bSpline) = 0;

	//! Path finding avoids blocker type by radius.
	virtual void SetPFBlockerRadius(int blockerType, float radius) = 0;

	//! Can path be modified to use request.targetPoint?  Results are cacheded in request.
	virtual ETriState CanTargetPointBeReached(CTargetPointRequest& request) = 0;

	//! Is request still valid/usable?
	virtual bool                 UseTargetPointRequest(const CTargetPointRequest& request) = 0;

	virtual bool                 GetValidPositionNearby(const Vec3& proposedPosition, Vec3& adjustedPosition) const = 0;
	virtual bool                 GetTeleportPosition(Vec3& teleportPos) const = 0;

	virtual class IPathFollower* GetPathFollower() const = 0;

	virtual bool                 IsPointValidForAgent(const Vec3& pos, uint32 flags) const = 0;
	// </interfuscator:shuffle>
};

typedef std::list<PathPointDescriptor> TPathPoints;

struct SNavPathParams
{
	SNavPathParams(const Vec3& start = Vec3Constants<float>::fVec3_Zero, const Vec3& end = Vec3Constants<float>::fVec3_Zero,
	               const Vec3& startDir = Vec3Constants<float>::fVec3_Zero, const Vec3& endDir = Vec3Constants<float>::fVec3_Zero,
	               int nForceBuildingID = -1, bool allowDangerousDestination = false, float endDistance = 0.0f,
	               bool continueMovingAtEnd = false, bool isDirectional = false)
		: start(start)
		, end(end)
		, startDir(startDir)
		, endDir(endDir)
		, nForceBuildingID(nForceBuildingID)
		, allowDangerousDestination(allowDangerousDestination)
		, precalculatedPath(false)
		, inhibitPathRegeneration(false)
		, continueMovingAtEnd(continueMovingAtEnd)
		, endDistance(endDistance)
		, isDirectional(isDirectional)
	{}

	Vec3 start;
	Vec3 end;
	Vec3 startDir;
	Vec3 endDir;
	int  nForceBuildingID;
	bool allowDangerousDestination;

	//! If path is precalculated it should not be regenerated, and also some things like steering. Will be disabled.
	bool precalculatedPath;

	//! Sometimes it is necessary to disable a normal path from getting regenerated.
	bool inhibitPathRegeneration;
	bool continueMovingAtEnd;
	bool isDirectional;

	//! The requested cut distance of the path, positive value means distance from path end, negative value means distance from path start.
	float            endDistance;

	NavigationMeshID meshID;

	void             Clear()
	{
		start = Vec3Constants<float>::fVec3_Zero;
		end = Vec3Constants<float>::fVec3_Zero;
		startDir = Vec3Constants<float>::fVec3_Zero;
		endDir = Vec3Constants<float>::fVec3_Zero;
		allowDangerousDestination = false;
		precalculatedPath = false;
		inhibitPathRegeneration = false;
		continueMovingAtEnd = false;
		isDirectional = false;
		endDistance = 0.0f;
		meshID = NavigationMeshID(0);
	}

	void Serialize(TSerialize ser)
	{
		ser.Value("start", start);
		ser.Value("end", end);
		ser.Value("startDir", startDir);
		ser.Value("endDir", endDir);
		ser.Value("nForceBuildingID", nForceBuildingID);
		ser.Value("allowDangerousDestination", allowDangerousDestination);
		ser.Value("precalculatedPath", precalculatedPath);
		ser.Value("inhibitPathRegeneration", inhibitPathRegeneration);
		ser.Value("continueMovingAtEnd", continueMovingAtEnd);
		ser.Value("isDirectional", isDirectional);
		ser.Value("endDistance", endDistance);

		if (ser.IsReading())
		{
			uint32 meshIdAsUint32;
			ser.Value("meshID", meshIdAsUint32);
			meshID = NavigationMeshID(meshIdAsUint32);
		}
		else
		{
			uint32 meshIdAsUint32 = (uint32) meshID;
			ser.Value("meshID", meshIdAsUint32);
		}
	}
};

class INavPath
{
public:
	// <interfuscator:shuffle>
	virtual ~INavPath(){}
	virtual void                                       Release() = 0;
	virtual void                                       CopyTo(INavPath* pRecipient) const = 0;
	virtual INavPath*                                  Clone() const = 0;

	virtual NavigationMeshID                           GetMeshID() const = 0;

	virtual int                                        GetVersion() const = 0;
	virtual void                                       SetVersion(int version) = 0;

	virtual void                                       SetParams(const SNavPathParams& params) = 0;
	virtual const SNavPathParams&                      GetParams() const = 0;
	virtual SNavPathParams&                            GetParams() = 0;
	virtual const TPathPoints&                         GetPath() const = 0;
	virtual void                                       SetPathPoints(const TPathPoints& points) = 0;

	virtual float                                      GetPathLength(bool twoD) const = 0;
	virtual void                                       PushFront(const PathPointDescriptor& newPathPoint, bool force = false) = 0;
	virtual void                                       PushBack(const PathPointDescriptor& newPathPoint, bool force = false) = 0;
	virtual void                                       Clear(const char* contextName) = 0;
	virtual bool                                       Advance(PathPointDescriptor& nextPathPoint) = 0;

	virtual bool                                       GetPathEndIsAsRequested() const = 0;
	virtual void                                       SetPathEndIsAsRequested(bool value) = 0;

	virtual bool                                       Empty() const = 0;

	virtual const PathPointDescriptor*                 GetLastPathPoint() const = 0;
	virtual const PathPointDescriptor*                 GetPrevPathPoint() const = 0;
	virtual const PathPointDescriptor*                 GetNextPathPoint() const = 0;
	virtual const PathPointDescriptor*                 GetNextNextPathPoint() const = 0;

	virtual const Vec3&                                GetNextPathPos(const Vec3& defaultPos = Vec3Constants<float>::fVec3_Zero) const = 0;
	virtual const Vec3&                                GetLastPathPos(const Vec3& defaultPos = Vec3Constants<float>::fVec3_Zero) const = 0;

	virtual bool                                       GetPosAlongPath(Vec3& posOut, float dist, bool twoD, bool extrapolateBeyondEnd, IAISystem::ENavigationType* nextPointType = NULL) const = 0;
	virtual float                                      GetDistToPath(Vec3& pathPosOut, float& distAlongPathOut, const Vec3& pos, float dist, bool twoD) const = 0;
	virtual float                                      GetDistToSmartObject(bool twoD) const = 0;
	virtual PathPointDescriptor::SmartObjectNavDataPtr GetLastPathPointAnimNavSOData() const = 0;
	virtual void                                       SetPreviousPoint(const PathPointDescriptor& previousPoint) = 0;

	virtual AABB                                       GetAABB(float dist) const = 0;

	virtual bool                                       GetPathPropertiesAhead(float distAhead, bool twoD, Vec3& posOut, Vec3& dirOut, float* invROut, float& lowestPathDotOut, bool scaleOutputWithDist) const = 0;

	virtual void                                       SetEndDir(const Vec3& endDir) = 0;
	virtual const Vec3& GetEndDir() const = 0;

	virtual bool        UpdateAndSteerAlongPath(Vec3& dirOut, float& distToEndOut, float& distToPathOut, bool& isResolvingSticking, Vec3& pathDirOut, Vec3& pathAheadDirOut, Vec3& pathAheadPosOut, Vec3 currentPos, const Vec3& currentVel, float lookAhead, float pathRadius, float dt, bool resolveSticking, bool twoD) = 0;

	virtual bool        AdjustPathAroundObstacles(const Vec3& currentpos, const AgentMovementAbility& movementAbility) = 0;

	virtual void        TrimPath(float length, bool twoD) = 0;
	;
	virtual float       GetDiscardedPathLength() const = 0;
	//virtual bool AdjustPathAroundObstacles(const CPathObstacles &obstacles, IAISystem::tNavCapMask navCapMask) = 0;
	//virtual void AddObjectAdjustedFor(const class CAIObject *pObject) = 0;
	//virtual const std::vector<const class CAIObject*> &GetObjectsAdjustedFor() const = 0;
	//virtual void ClearObjectsAdjustedFor() = 0;
	virtual float     UpdatePathPosition(Vec3 agentPos, float pathLookahead, bool twoD, bool allowPathToFinish) = 0;
	virtual Vec3      CalculateTargetPos(Vec3 agentPos, float lookAhead, float minLookAheadAlongPath, float pathRadius, bool twoD) const = 0;
	virtual ETriState CanTargetPointBeReached(CTargetPointRequest& request, const CAIActor* pAIActor, bool twoD) const = 0;
	virtual bool      UseTargetPointRequest(const CTargetPointRequest& request, CAIActor* pAIActor, bool twoD) = 0;

	virtual void      Draw(const Vec3& drawOffset = ZERO) const = 0;
	virtual void      Dump(const char* name) const = 0;

	bool              ArePathsEqual(const INavPath& otherNavPath)
	{
		const TPathPoints& path1 = this->GetPath();
		const TPathPoints& path2 = otherNavPath.GetPath();
		const TPathPoints::size_type path1Size = path1.size();
		const TPathPoints::size_type path2Size = path2.size();
		if (path1Size != path2Size)
			return false;

		TPathPoints::const_iterator path1It = path1.begin();
		TPathPoints::const_iterator path1End = path1.end();
		TPathPoints::const_iterator path2It = path2.begin();

		typedef std::pair<TPathPoints::const_iterator, TPathPoints::const_iterator> TMismatchResult;

		TMismatchResult result = std::mismatch(path1It, path1End, path2It, PathPointDescriptor::ArePointsEquivalent);
		return result.first == path1.end();
	}
	// </interfuscator:shuffle>
};

DECLARE_SHARED_POINTERS(INavPath);

enum EMNMPathResult
{
	eMNMPR_NoPathFound = 0,
	eMNMPR_Success,
};

struct MNMPathRequestResult
{
	MNMPathRequestResult()
		: cost(0.f)
		, id(0)
		, result(eMNMPR_NoPathFound)
	{}

	ILINE bool HasPathBeenFound() const
	{
		return result == eMNMPR_Success;
	}

	INavPathPtr    pPath;
	float          cost;
	uint32         id;
	EMNMPathResult result;
};

struct IPathObstacles
{
	virtual ~IPathObstacles() {}

	virtual bool IsPathIntersectingObstacles(const NavigationMeshID meshID, const Vec3& start, const Vec3& end, float radius) const = 0;
	virtual bool IsPointInsideObstacles(const Vec3& position) const = 0;
	virtual bool IsLineSegmentIntersectingObstaclesOrCloseToThem(const Lineseg& linesegToTest, float maxDistanceToConsiderClose) const = 0;
};

class IPathFollower
{
public:
	// <interfuscator:shuffle>
	virtual ~IPathFollower(){}

	virtual void Release() = 0;

	virtual void Reset() = 0;

	//! This attaches us to a particular path (pass 0 to detach).
	virtual void AttachToPath(INavPath* navPath) = 0;

	virtual void SetParams(const PathFollowerParams& params) = 0;

	//! Just view the params.
	virtual const PathFollowerParams& GetParams() const = 0;
	virtual PathFollowerParams&       GetParams() = 0;

	//! Advances the follow target along the path as far as possible while ensuring the follow target remains reachable.
	//! \return true if the follow target is reachable, false otherwise.
	virtual bool Update(PathFollowResult& result, const Vec3& curPos, const Vec3& curVel, float dt) = 0;

	//! Advances the current state in terms of position - effectively pretending that the follower has gone further than it has.
	virtual void Advance(float distance) = 0;

	//! Returns the distance from the lookahead to the end, plus the distance from the position passed in to the LA if pCurPos != 0.
	virtual float GetDistToEnd(const Vec3* pCurPos) const = 0;

	//! Returns the distance along the path from the current look-ahead position to the first smart object path segment.
	//! If there's no path, or no smart objects on the path, then std::numeric_limits<float>::max() will be returned.
	virtual float GetDistToSmartObject() const = 0;
	virtual float GetDistToNavType(IAISystem::ENavigationType navType) const = 0;

	//! Returns a point on the path some distance ahead. actualDist is set according to how far we looked.
	//! This value may be less than dist if we'd reach the end.
	virtual Vec3 GetPathPointAhead(float dist, float& actualDist) const = 0;

	virtual void Draw(const Vec3& drawOffset = ZERO) const = 0;

	virtual void Serialize(TSerialize ser) = 0;

	//! Checks ability to walk along a piecewise linear path starting from the current position.
	//! This is useful for example when animation would like to deviate from the path.
	virtual bool CheckWalkability(const Vec2* path, const size_t length) const = 0;

	//! Can the pathfollower cut corners if there is space to do so? (default: true).
	virtual bool GetAllowCuttingCorners() const = 0;

	//! Sets whether or not the pathfollower is allowed to cut corners if there is space to do so. (default: true).
	virtual void SetAllowCuttingCorners(const bool allowCuttingCorners) = 0;

	//! Checks for whether the attached path is affected by a NavMesh change or whether it would be still be fully traversable from its current position.
	virtual bool IsRemainingPathAffectedByNavMeshChange(const NavigationMeshID affectedMeshID, const MNM::TileID affectedTileID) const = 0;
	// </interfuscator:shuffle>
};

DECLARE_SHARED_POINTERS(IPathFollower);

namespace MNM
{
typedef unsigned int QueuedPathID;

namespace Constants
{
enum EQueuedPathID
{
	eQueuedPathID_InvalidID = 0,
};
}
}

enum EMNMDangers
{
	eMNMDangers_None            = 0,
	eMNMDangers_AttentionTarget = BIT(0),
	eMNMDangers_Explosive       = BIT(1),
	eMNMDangers_GroupMates      = BIT(2),
};

typedef uint32 MNMDangersFlags;

struct MNMPathRequest
{
	typedef Functor2<const MNM::QueuedPathID&, MNMPathRequestResult&> Callback;

	MNMPathRequest()
		: resultCallback(0)
		, startLocation(ZERO)
		, endLocation(ZERO)
		, endDirection(FORWARD_DIRECTION)
		, agentTypeID(NavigationAgentTypeID())
		, forceTargetBuildingId(0)
		, endTolerance(0.0f)
		, endDistance(0.0f)
		, allowDangerousDestination(false)
		, dangersToAvoidFlags(eMNMDangers_None)
		, beautify(true)
	{

	}

	MNMPathRequest(const Vec3& start, const Vec3& end, const Vec3& _endDirection, int _forceTargetBuildingId,
	               float _endTolerance, float _endDistance, bool _allowDangerousDestination, const Callback& callback,
	               const NavigationAgentTypeID& _agentTypeID, const MNMDangersFlags dangersFlags = eMNMDangers_None)
		: resultCallback(callback)
		, startLocation(start)
		, endLocation(end)
		, endDirection(_endDirection)
		, agentTypeID(_agentTypeID)
		, forceTargetBuildingId(_forceTargetBuildingId)
		, endTolerance(_endTolerance)
		, endDistance(_endDistance)
		, allowDangerousDestination(false)
		, dangersToAvoidFlags(dangersFlags)
		, beautify(true)
	{

	}

	Callback              resultCallback;

	Vec3                  startLocation;
	Vec3                  endLocation;
	Vec3                  endDirection;

	NavigationAgentTypeID agentTypeID;

	bool                  beautify; //!< Set beautify to false if you don't want to "beautify" the path (make it little less jagged, and more curvy).

	int                   forceTargetBuildingId;
	float                 endTolerance;
	float                 endDistance;
	bool                  allowDangerousDestination;
	MNMDangersFlags       dangersToAvoidFlags;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct IMNMPathfinder
{
	// <interfuscator:shuffle>
	virtual ~IMNMPathfinder() {};

	//! Request a path (look at MNMPathRequest for relevant request info).
	//! This request is queued and processed in a seperate thread.
	//! The path result is sent to the callback function specified in the request.
	//! Returns an ID so that you can cancel the request.
	virtual MNM::QueuedPathID RequestPathTo(const IAIPathAgent* pRequester, const MNMPathRequest& request) = 0;

	//! Cancel a requested path by ID.
	virtual void CancelPathRequest(MNM::QueuedPathID requestId) = 0;

	virtual bool CheckIfPointsAreOnStraightWalkableLine(const NavigationMeshID& meshID, const Vec3& source, const Vec3& destination, float heightOffset = 0.2f) const = 0;

	// </interfuscator:shuffle>
};

struct IAIPathFinder
{
public:
	// <interfuscator:shuffle>
	virtual ~IAIPathFinder(){}
	virtual void                   RequestPathTo(const Vec3& start, const Vec3& end, const Vec3& endDir, IAIPathAgent* pRequester, bool allowDangerousDestination, int forceTargetBuildingId, float endTol, float endDistance) = 0;
	virtual void                   RequestPathTo(uint32 startIndex, uint32 endIndex, const Vec3& endDir, IAIPathAgent* pRequester, bool allowDangerousDestination, int forceTargetBuildingId, float endTol, float endDistance) = 0;
	virtual int                    RequestRawPathTo(const Vec3& start, const Vec3& end, float passRadius, IAISystem::tNavCapMask navCapMask, unsigned& lastNavNode, bool allowDangerousDestination, float endTol, const PathfindingExtraConstraints& constraints, IAIPathAgent* pReference = 0) = 0;
	virtual void                   RequestPathInDirection(const Vec3& start, const Vec3& pos, float maxDist, IAIPathAgent* pRequester, float endDistance) = 0;

	virtual void                   CancelAnyPathsFor(IAIPathAgent* pRequester, bool actorRemoved = false) = 0;
	virtual void                   CancelCurrentRequest() = 0;

	virtual void                   RescheduleCurrentPathfindRequest() = 0;
	virtual bool                   IsFindingPathFor(const IAIPathAgent* pRequester) const = 0;

	virtual Vec3                   GetBestPosition(const PathfindingHeuristicProperties& heuristic, float maxCost, const Vec3& startPos, const Vec3& endPos, unsigned startHintIndex, IAISystem::tNavCapMask navCapMask) = 0;

	virtual INavPath*              CreateEmptyPath() const = 0;
	virtual IPathFollower*         CreatePathFollower(const PathFollowerParams& params) const = 0;

	virtual const INavPath*        GetCurrentPath() const = 0;
	virtual const PathfindRequest* GetPathfindCurrentRequest() const = 0;

	virtual void                   FlushPathQueue() = 0;
	virtual bool                   IsPathQueueEmpty() const = 0;

	virtual void                   Reset(IAISystem::EResetReason reason) = 0;

	virtual void                   RegisterPathFinderListener(IAIPathFinderListerner* pListener) = 0;
	virtual void                   UnregisterPathFinderListener(IAIPathFinderListerner* pListener) = 0;
	// </interfuscator:shuffle>
};

#endif //__IPATHFINDER_H__
