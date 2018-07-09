// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef IAIObject_h
#define IAIObject_h

#include <CryAISystem/IAgent.h> // <> required for Interfuscator

#ifndef INVALID_AIOBJECTID
typedef uint32 tAIObjectID;   //TODO make a handle or something instead, description needs to be accessible from many places
	#define INVALID_AIOBJECTID ((tAIObjectID)(0))
#endif

struct GraphNode;
struct IAIActor;
struct IBlackBoard;
struct VisionID;
struct ObservableParams;

class CAIActor;
class CAIPlayer;
class CAIVehicle;
class CLeader;
class CPuppet;
class CAIFlyingVehicle;

struct IAIObject :
	public IAIRecordable
{
	enum EUpdateType
	{
		Full,
		Dry,
	};
	
	enum ESubType
	{
		STP_NONE,
		STP_FORMATION,
		STP_REFPOINT,
		STP_LOOKAT,
		STP_CAR,
		STP_BOAT,
		STP_HELI,
		STP_2D_FLY,
		STP_SOUND,
		STP_GUNFIRE,
		STP_MEMORY,
		STP_BEACON,
		STP_SPECIAL,
		STP_ANIM_TARGET,
		STP_HELICRYSIS2,
		STP_MAXVALUE
	};

	enum EFieldOfViewResult
	{
		eFOV_Outside = 0,
		eFOV_Primary,
		eFOV_Secondary,
	};

protected:
	union
	{
		struct
		{
			uint32 _fastcast_CAIActor         : 1;
			uint32 _fastcast_CAIAttribute     : 1; // (MATT) Unused {2009/03/31}
			uint32 _fastcast_CAIPlayer        : 1;
			uint32 _fastcast_CLeader          : 1;
			uint32 _fastcast_CPipeUser        : 1;
			uint32 _fastcast_CPuppet          : 1;
			uint32 _fastcast_CAIVehicle       : 1;
			uint32 _fastcast_CAIFlyingVehicle : 1;
		};
		int _fastcast_any;
	};
public:

	////////////////////////////////////////////////////////////////////////////////////////
	//Startup/shutdown
	IAIObject() : _fastcast_any(0) {}
	// <interfuscator:shuffle>
	virtual ~IAIObject() {}
	virtual void Reset(EObjectResetType type) = 0;
	virtual void Release() = 0;

	//! \return true if method Update(EUpdateType type) has been invoked at least once.
	virtual bool IsUpdatedOnce() const = 0;

	virtual bool IsEnabled() const = 0;
	virtual void Event(unsigned short, SAIEVENT* pEvent) = 0;
	virtual void EntityEvent(const SEntityEvent& event) = 0;
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//Basic properties
	virtual const VisionID& GetVisionID() const = 0;
	virtual void            SetObservable(bool observable) = 0;
	virtual bool            IsObservable() const = 0;

	virtual void            GetObservablePositions(ObservableParams& observableParams) const = 0;

	virtual tAIObjectID     GetAIObjectID() const = 0;

	virtual void            SetName(const char* pName) = 0;
	virtual const char*     GetName() const = 0;

	virtual unsigned short  GetAIType() const = 0;
	virtual ESubType        GetSubType() const = 0;
	virtual void            SetType(unsigned short type) = 0;

	virtual void            SetPos(const Vec3& pos, const Vec3& dirForw = Vec3Constants<float>::fVec3_OneX) = 0;
	virtual const Vec3& GetPos() const = 0;
	virtual void        SetExpectedPhysicsPos(const Vec3& pos) = 0;
	virtual const Vec3  GetPosInNavigationMesh(const uint32 agentTypeID) const = 0;

	virtual void        SetRadius(float fRadius) = 0;
	virtual float       GetRadius() const = 0;

	//! Body direction here should be animated character body direction, with a fall back to entity direction.
	virtual const Vec3& GetBodyDir() const = 0;
	virtual void        SetBodyDir(const Vec3& dir) = 0;

	virtual const Vec3& GetViewDir() const = 0;
	virtual void        SetViewDir(const Vec3& dir) = 0;

	virtual const Vec3& GetEntityDir() const = 0;
	virtual void        SetEntityDir(const Vec3& dir) = 0;

	virtual const Vec3& GetMoveDir() const = 0;
	virtual void        SetMoveDir(const Vec3& dir) = 0;
	virtual Vec3        GetVelocity() const = 0;
	////////////////////////////////////////////////////////////////////////////////////////

	virtual EFieldOfViewResult IsObjectInFOV(const IAIObject* pTarget, float distanceStale = 1.f) const = 0;
	virtual EFieldOfViewResult IsPointInFOV(const Vec3& pos, float distanceScale = 1.0f) const = 0;

	virtual EntityId GetPerceivedEntityID() const = 0;

	////////////////////////////////////////////////////////////////////////////////////////
	//Serialize
	//! Tell AI what entity owns this so can sort out linking during save/load.
	virtual void     SetEntityID(unsigned ID) = 0;
	virtual unsigned GetEntityID() const = 0;
	virtual IEntity* GetEntity() const = 0;

	//! \note EntityId / other AI objects may not be valid at this point. Use PostSerialize if needed.
	virtual void Serialize(TSerialize ser) = 0;
	virtual void PostSerialize() = 0;
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	// Starting to assume WAY to many conflicting things about possible derived classes
	//! \return position weapon fires from.
	virtual const Vec3&  GetFirePos() const = 0;

	virtual IBlackBoard* GetBlackBoard() { return NULL; }

	virtual void         SetGroupId(int id) = 0;
	virtual int          GetGroupId() const = 0;
	virtual bool         IsHostile(const IAIObject* pOther, bool bUsingAIIgnorePlayer = true) const = 0;
	virtual void         SetThreateningForHostileFactions(const bool threatening) = 0;
	virtual bool         IsThreateningForHostileFactions() const = 0;

	virtual uint8        GetFactionID() const = 0;
	virtual void         SetFactionID(uint8 factionID) = 0;

	//! \return true if the AIObject is AIActor (but not CLeader).
	virtual bool           IsAgent() const { return false; }

	virtual IAIActorProxy* GetProxy() const = 0;

	//Starting to assume WAY to many conflicting things about possible derived classes//////
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	// Formations
	virtual bool        CreateFormation(const unsigned int nCrc32ForFormationName, Vec3 vTargetPos = ZERO) = 0;
	virtual bool        CreateFormation(const char* szName, Vec3 vTargetPos = ZERO) = 0;
	virtual bool        HasFormation() = 0;
	virtual bool        ReleaseFormation() = 0;

	virtual void        CreateGroupFormation(IAIObject* pLeader)            {; }
	virtual void        SetFormationPos(const Vec2& v2RelPos)               {; }
	virtual void        SetFormationLookingPoint(const Vec3& v3RelPos)      {; }
	virtual void        SetFormationAngleThreshold(float fThresholdDegrees) {; }
	virtual const Vec3& GetFormationPos()                                   { return Vec3Constants<float>::fVec3_Zero; }
	virtual const Vec3  GetFormationVelocity()                              { return Vec3Constants<float>::fVec3_Zero; }
	virtual const Vec3& GetFormationLookingPoint()                          { return Vec3Constants<float>::fVec3_Zero; }

	//! Sets a randomly rotating range for the AIObject's formation sight directions.
	virtual void SetFormationUpdateSight(float range, float minTime, float maxTime) = 0;
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	// Fast Casting
	virtual const IAIActor*  CastToIAIActor() const         { return NULL; }
	virtual IAIActor*        CastToIAIActor()               { return NULL; }
	const CAIActor*          CastToCAIActor() const         { return _fastcast_CAIActor ? (const CAIActor*) this : NULL; }
	CAIActor*                CastToCAIActor()               { return _fastcast_CAIActor ? (CAIActor*) this : NULL; }

	const CAIPlayer*         CastToCAIPlayer() const        { return _fastcast_CAIPlayer ? (const CAIPlayer*) this : NULL; }
	CAIPlayer*               CastToCAIPlayer()              { return _fastcast_CAIPlayer ? (CAIPlayer*) this : NULL; }

	const CLeader*           CastToCLeader() const          { return _fastcast_CLeader ? (const CLeader*) this : NULL; }
	CLeader*                 CastToCLeader()                { return _fastcast_CLeader ? (CLeader*) this : NULL; }

	virtual const IPipeUser* CastToIPipeUser() const        { return NULL; }
	virtual IPipeUser*       CastToIPipeUser()              { return NULL; }
	const CPipeUser*         CastToCPipeUser() const        { return _fastcast_CPipeUser ? (const CPipeUser*) this : NULL; }
	CPipeUser*               CastToCPipeUser()              { return _fastcast_CPipeUser ? (CPipeUser*) this : NULL; }

	virtual const IPuppet*   CastToIPuppet() const          { return NULL; }
	virtual IPuppet*         CastToIPuppet()                { return NULL; }
	const CPuppet*           CastToCPuppet() const          { return _fastcast_CPuppet ? (const CPuppet*) this : NULL; }
	CPuppet*                 CastToCPuppet()                { return _fastcast_CPuppet ? (CPuppet*) this : NULL; }

	const CAIVehicle*        CastToCAIVehicle() const       { return _fastcast_CAIVehicle ? (const CAIVehicle*) this : NULL; }
	CAIVehicle*              CastToCAIVehicle()             { return _fastcast_CAIVehicle ? (CAIVehicle*) this : NULL; }

	const CAIFlyingVehicle*  CastToCAIFlyingVehicle() const { return _fastcast_CAIFlyingVehicle ? (const CAIFlyingVehicle*) this : NULL; }
	CAIFlyingVehicle*        CastToCAIFlyingVehicle()       { return _fastcast_CAIFlyingVehicle ? (CAIFlyingVehicle*) this : NULL; }
	////////////////////////////////////////////////////////////////////////////////////////
	// </interfuscator:shuffle>
};

////////////////////////////////////////////////////////////////////////////////////////
//Needed inside and outside of class?

inline const IAIActor*  CastToIAIActorSafe(const IAIObject* pAI)  { return pAI ? pAI->CastToIAIActor() : 0; }
inline IAIActor*        CastToIAIActorSafe(IAIObject* pAI)        { return pAI ? pAI->CastToIAIActor() : 0; }

inline const IPuppet*   CastToIPuppetSafe(const IAIObject* pAI)   { return pAI ? pAI->CastToIPuppet() : 0; }
inline IPuppet*         CastToIPuppetSafe(IAIObject* pAI)         { return pAI ? pAI->CastToIPuppet() : 0; }

inline const IPipeUser* CastToIPipeUserSafe(const IAIObject* pAI) { return pAI ? pAI->CastToIPipeUser() : 0; }
inline IPipeUser*       CastToIPipeUserSafe(IAIObject* pAI)       { return pAI ? pAI->CastToIPipeUser() : 0; }
////////////////////////////////////////////////////////////////////////////////////////

#endif
