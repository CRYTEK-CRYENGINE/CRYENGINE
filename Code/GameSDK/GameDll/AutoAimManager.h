// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

20:11:2009 - Benito G.R.
*************************************************************************/

#pragma once

#ifndef __AUTOAIM_MANAGER_H__
#define __AUTOAIM_MANAGER_H__

#ifndef _RELEASE
	#define DEBUG_AUTOAIM_MANAGER 1
#else
	#define DEBUG_AUTOAIM_MANAGER 0
#endif

class CActor;
DECLARE_SHARED_POINTERS(CActor);

struct SAutoaimTargetRegisterParams
{
	SAutoaimTargetRegisterParams()
		: fallbackOffset(1.2f)
		, primaryBoneId(-1)
		, physicsBoneId(-1)
		, secondaryBoneId(-1)
		, innerRadius(0.4f)
		, outerRadius(0.8f)
		, snapRadius(2.0f)
		, snapRadiusTagged(2.0f)
		, hasSkeleton(false)
	{

	}

	float			fallbackOffset;
	float			innerRadius;
	float			outerRadius;
	float			snapRadius;
	float			snapRadiusTagged;
	int16 primaryBoneId;
	int16	physicsBoneId;
	int16 secondaryBoneId;
	bool			hasSkeleton;

};

enum EAutoaimTargetFlags
{
	eAATF_None = 0,
	eAATF_AIHostile = BIT(1),
	eAATF_StealthKillable = BIT(2),
	eAATF_CanBeGrabbed = BIT(3),
	eAATF_AIRadarTagged	= BIT(4),
};

typedef struct SAutoaimTarget
{
	SAutoaimTarget()
		: entityId(0)
		, fallbackOffset(0.0f)
		, innerRadius(0.4f)
		, outerRadius(0.8f)
		, snapRadius(2.0f)
		, snapRadiusTagged(2.0f)
		, primaryBoneId(-1)
		, physicsBoneId(-1)
		, secondaryBoneId(-1)
		, flags(eAATF_None)
		, primaryAimPosition(0.0f, 0.0f, 0.0f)
		, secondaryAimPosition(0.0f, 0.0f, 0.0f)
		, hasSkeleton(false)
	{

	}

	ILINE bool HasFlagSet(EAutoaimTargetFlags _flag) const { return ((flags & _flag) != 0); }
	ILINE bool HasFlagsSet(int8 _flags) const { return ((flags & _flags) == _flags); } 
	ILINE void SetFlag(EAutoaimTargetFlags _flag) { flags |= _flag; }
	ILINE void RemoveFlag(EAutoaimTargetFlags _flag) { flags &= ~_flag; }

	EntityId		entityId;
	CActorWeakPtr	pActorWeak;
	Vec3			primaryAimPosition;
	Vec3			secondaryAimPosition;
	float			fallbackOffset;
	float			innerRadius;
	float			outerRadius;
	float			snapRadius;
	float			snapRadiusTagged;
	int16			primaryBoneId;
	int16			physicsBoneId;
	int16			secondaryBoneId;
	int8			flags;
	bool			hasSkeleton;

} SAutoaimTarget;

typedef std::vector<SAutoaimTarget> TAutoaimTargets;

class CAutoAimManager
{
public:

	CAutoAimManager();
	~CAutoAimManager();

	bool RegisterAutoaimTargetActor(const CActor& targetActor, const SAutoaimTargetRegisterParams& registerParams);
	bool RegisterAutoaimTargetObject(const EntityId targetObjectId, const SAutoaimTargetRegisterParams& registerParams);

	void UnregisterAutoaimTarget(const EntityId entityId);

	void Update(float dt);

	ILINE void SetCloseCombatSnapTarget(EntityId snapTargetId, float snapRange, float snapSpeed) 
	{ 
		m_closeCombatSnapTargetId = snapTargetId; 
		m_closeCombatSnapTargetRange = snapRange; 
		m_closeCombatSnapTargetMoveSpeed = snapSpeed; 
	}

	ILINE EntityId GetCloseCombatSnapTarget() const { return m_closeCombatSnapTargetId; }
	ILINE float GetCloseCombatSnapTargetRange() const { return m_closeCombatSnapTargetRange; }
	ILINE float GetCloseCombatSnapTargetMoveSpeed() const { return m_closeCombatSnapTargetMoveSpeed; }

	ILINE const TAutoaimTargets& GetAutoAimTargets() const { return m_autoaimTargets; };

	const SAutoaimTarget* GetTargetInfo(EntityId targetId) const;
	bool SetTargetTagged(EntityId targetId);
	void SetTargetAsCanBeGrabbed(EntityId targetId, const bool canBeGrabbed);
	
	void OnEditorReset();

	uint8	GetLocalPlayerFaction() const;
	uint8	GetTargetFaction(IEntity& targetEntity) const;

private:

	static const int32 kMaxAutoaimTargets = 64;

	SAutoaimTarget* GetTargetInfoInternal(EntityId targetId);

	void RegisterCharacterTargetInfo(const CActor& targetActor, const SAutoaimTargetRegisterParams& registerParams);
	void UpdateTargetInfo(SAutoaimTarget& aaTarget, float fFrameTime);

	void RegisterObjectTargetInfo(const EntityId targetObjectId, const SAutoaimTargetRegisterParams& registerParams);

	bool	IsEntityRegistered(EntityId entityId) const;
	bool	IsSpaceAvailable() const;
	
#if DEBUG_AUTOAIM_MANAGER
	void  DebugDraw();
	void  DrawDisc(const Vec3& center, Vec3 axis, float innerRadius, float outerRadius, const ColorB& innerColor, const ColorB& outerColor);
	
	int				g_autoAimManagerDebug;
#endif

	TAutoaimTargets	m_autoaimTargets;

	EntityId		m_closeCombatSnapTargetId;
	float			m_closeCombatSnapTargetRange;
	float			m_closeCombatSnapTargetMoveSpeed;

	mutable uint8		m_localPlayerFaction;
};

#endif //__AUTOAIM_MANAGER_H__