// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef AIOBJECT_H
#define AIOBJECT_H

#if _MSC_VER > 1000
	#pragma once
#endif // _MSC_VER > 1000

#include <CryPhysics/IPhysics.h>
#include <CryAISystem/IAIObject.h>
#include "Reference.h"

#ifdef CRYAISYSTEM_DEBUG
	#include "AIRecorder.h"
#endif //CRYAISYSTEM_DEBUG

#include <VisionMap.h>

struct GraphNode;
class CFormation;
class CRecorderUnit;

/*! Basic ai object class. Defines a framework that all puppets and points of interest
   later follow.
 */
class CAIObject :
	public IAIObject
#ifdef CRYAISYSTEM_DEBUG
	, public CRecordable
#endif //CRYAISYSTEM_DEBUG
{
	// only allow these classes to create AI objects.
	//	TODO: ideally only one place where objects are created
	friend class CAIObjectManager;
	friend struct SAIObjectCreationHelper;
protected:
	CAIObject();
	virtual ~CAIObject();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//IAIObject interfaces//////////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//Startup/shutdown//////////////////////////////////////////////////////////////////////
	virtual void Reset(EObjectResetType type) override;
	virtual void Release() override;

	// "true" if method Update(EUpdateType type) has been invoked AT LEAST once
	virtual bool IsUpdatedOnce() const override;

	virtual bool IsEnabled() const override;
	virtual void Event(unsigned short, SAIEVENT* pEvent) override;
	virtual void EntityEvent(const SEntityEvent& event) override;
	//Startup/shutdown//////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//Basic properties//////////////////////////////////////////////////////////////////////
	virtual tAIObjectID     GetAIObjectID() const override;

	virtual const VisionID& GetVisionID() const override;
	virtual void            SetObservable(bool observable) override;
	virtual bool            IsObservable() const override;

	virtual void            GetObservablePositions(ObservableParams& observableParams) const override;
	virtual uint32          GetObservableTypeMask() const;
	virtual void            GetPhysicalSkipEntities(PhysSkipList& skipList) const;
	void                    UpdateObservableSkipList() const;

	virtual void            SetName(const char* pName) override;
	virtual const char*     GetName() const override;

	virtual unsigned short  GetAIType() const override;
	virtual ESubType        GetSubType() const override;
	virtual void            SetType(unsigned short type) override;

	virtual void            SetPos(const Vec3& pos, const Vec3& dirForw = Vec3Constants<float>::fVec3_OneX) override;
	virtual const Vec3& GetPos() const override;
	virtual const Vec3  GetPosInNavigationMesh(const uint32 agentTypeID) const override;

	virtual void        SetRadius(float fRadius) override;
	virtual float       GetRadius() const override;

	//Body direction here should be animated character body direction, with a fall back to entity direction.
	virtual const Vec3&        GetBodyDir() const override;
	virtual void               SetBodyDir(const Vec3& dir) override;

	virtual const Vec3&        GetViewDir() const override;
	virtual void               SetViewDir(const Vec3& dir) override;

	virtual const Vec3& GetEntityDir() const override;
	virtual void        SetEntityDir(const Vec3& dir) override;

	virtual const Vec3& GetMoveDir() const override;
	virtual void        SetMoveDir(const Vec3& dir) override;
	virtual Vec3        GetVelocity() const override;

	//Basic properties//////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	virtual EFieldOfViewResult IsObjectInFOV(const IAIObject* pTarget, float distanceStale = 1.f) const override;
	virtual EFieldOfViewResult IsPointInFOV(const Vec3& pos, float distanceScale = 1.0f) const override;

	////////////////////////////////////////////////////////////////////////////////////////
	//Serialize/////////////////////////////////////////////////////////////////////////////
	//Tell AI what entity owns this so can sort out linking during save/load
	virtual void     SetEntityID(unsigned ID) override;
	virtual unsigned GetEntityID() const override;
	virtual IEntity* GetEntity() const override;

	virtual void     Serialize(TSerialize ser) override;
	virtual void     PostSerialize() override {};

	bool             ShouldSerialize() const;
	void             SetShouldSerialize(bool ser) { m_serialize = ser; }

	//Serialize/////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//Starting to assume WAY to many conflicting things about possible derived classes//////

	//Returns position weapon fires from
	void                 SetFirePos(const Vec3& pos);
	virtual const Vec3&  GetFirePos() const override;
	virtual IBlackBoard* GetBlackBoard() override { return NULL; }

	virtual uint8        GetFactionID() const override;
	virtual void         SetFactionID(uint8 factionID) override;

	virtual void         SetGroupId(int id) override;
	virtual int          GetGroupId() const override;
	virtual bool         IsHostile(const IAIObject* pOther, bool bUsingAIIgnorePlayer = true) const override;
	virtual void         SetThreateningForHostileFactions(const bool threatening) override;
	virtual bool         IsThreateningForHostileFactions() const override;

	virtual bool         IsTargetable() const;

	// Returns the EntityId to be used by the perception manager when this AIObject is perceived by another.
	virtual EntityId       GetPerceivedEntityID() const override;

	virtual void           SetProxy(IAIActorProxy* proxy);
	virtual IAIActorProxy* GetProxy() const override;

	//Starting to assume WAY to many conflicting things about possible derived classes//////
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//Formations////////////////////////////////////////////////////////////////////////////
	virtual bool        CreateFormation(const unsigned int nCrc32ForFormationName, Vec3 vTargetPos = ZERO) override;
	virtual bool        CreateFormation(const char* szName, Vec3 vTargetPos = ZERO) override;
	virtual bool        HasFormation() override;
	virtual bool        ReleaseFormation() override;

	virtual void        CreateGroupFormation(IAIObject* pLeader) override            {}
	virtual void        SetFormationPos(const Vec2& v2RelPos) override               {}
	virtual void        SetFormationLookingPoint(const Vec3& v3RelPos) override      {}
	virtual void        SetFormationAngleThreshold(float fThresholdDegrees) override {}
	virtual const Vec3& GetFormationPos() override                                   { return Vec3Constants<float>::fVec3_Zero; }
	virtual const Vec3  GetFormationVelocity() override;
	virtual const Vec3& GetFormationLookingPoint() override                          { return Vec3Constants<float>::fVec3_Zero; }
	//Sets a randomly rotating range for the AIObject's formation sight directions.
	virtual void        SetFormationUpdateSight(float range, float minTime, float maxTime) override;
	//Formations////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	//IAIObject interfaces//////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//IAIRecordable interfaces////////////////////////////////////////////////////////////////////////////////////////
	virtual void            RecordEvent(IAIRecordable::e_AIDbgEvent event, const IAIRecordable::RecorderEventData* pEventData = NULL) override {}
	virtual void            RecordSnapshot() override                                                                                          {}
	virtual IAIDebugRecord* GetAIDebugRecord() override;

#ifdef CRYAISYSTEM_DEBUG
	CRecorderUnit* CreateAIDebugRecord();
#endif //CRYAISYSTEM_DEBUG
	//IAIRecordable interfaces////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//CAIObject interfaces////////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//Original Virtuals/////////////////////////////////////////////////////////////////////
	virtual IPhysicalEntity*    GetPhysics(bool wantCharacterPhysics = false) const;

	virtual void                SetFireDir(const Vec3& dir);
	virtual const Vec3&         GetFireDir() const;
	virtual const Vec3&         GetShootAtPos() const  { return m_vPosition; } ///< Returns the position to shoot (bbox middle for tank, etc)

	virtual CWeakRef<CAIObject> GetAssociation() const { return m_refAssociation; }
	virtual void                SetAssociation(CWeakRef<CAIObject> refAssociation);

	virtual void                OnObjectRemoved(CAIObject* pObject) {}
	//Original Virtuals/////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//local level///////////////////////////////////////////////////////////////////////////
	const Vec3&           GetLastPosition() { return m_vLastPosition; }
	Vec3                  GetPhysicsPos() const;
	void                  SetExpectedPhysicsPos(const Vec3& pos) override;

	const char*           GetEventName(unsigned short eType) const;

	void                  SetSubType(ESubType type);
	inline unsigned short GetType() const { return m_nObjectType; } // used internally to inline the function calls
	//local level///////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	////////////////////////////////////////////////////////////////////////////////////////
	//ObjectContainer/WeakRef stuff/////////////////////////////////////////////////////////
	void SetSelfReference(CWeakRef<CAIObject> ref)
	{
		// Should only ever have to call once
		// Should never be invalid
		// Is serialized like any other reference
		assert(m_refThis.IsNil());
		m_refThis = ref;
	}

	CWeakRef<CAIObject> GetSelfReference() const
	{
		assert(!m_refThis.IsNil());
		return m_refThis;
	}

	bool HasSelfReference() const
	{
		return m_refThis.IsSet();
	}
	//ObjectContainer/WeakRef stuff/////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	//CAIObject interfaces////////////////////////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

protected:
	void SetVisionID(const VisionID& visionID);

	bool     m_bEnabled;
	uint16   m_nObjectType;   //	type would be Dummy
	ESubType m_objectSubType; //	subType would be soundDummy, formationPoint, etc
	int      m_groupId;
	float    m_fRadius;

	EntityId m_entityID;

private:
	CWeakRef<CAIObject> m_refThis;

	Vec3                m_vPosition;
	Vec3                m_vEntityDir;
	Vec3                m_vBodyDir; // direction of AI body, animated body direction if available
	Vec3                m_vMoveDir; // last move direction of the entity
	Vec3                m_vView;    // view direction (where my head is looking at, tank turret turned, etc)

public:
	bool         m_bUpdatedOnce;
	mutable bool m_bTouched;      // this gets frequent write access.

	// please add a new variable below, the section above is optimized for memory caches.

	Vec3        m_vLastPosition;
	CFormation* m_pFormation;

private:

	Vec3     m_vFirePosition;
	Vec3     m_vFireDir;

	Vec3     m_vExpectedPhysicsPos;       // Where the AIObject is expected to be next frame (only valid when m_expectedPhysicsPosFrameId == current frameID)
	int      m_expectedPhysicsPosFrameId; // Timestamp of m_vExpectedPhysicsPos

	VisionID m_visionID;
	uint8    m_factionID;
	bool     m_isThreateningForHostileFactions;
	bool     m_observable;

	bool                m_serialize;

	string              m_name;
protected:
	CWeakRef<CAIObject> m_refAssociation;
};

#endif
