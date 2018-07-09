// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   LeaderAction.h

   Description: Header for CUnitImg class

   -------------------------------------------------------------------------
   History:
   - 22/05/2006 : Moved from LeaderAction by Mikko

 *********************************************************************/

#ifndef __UnitImg_H__
#define __UnitImg_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryAISystem/IAgent.h>
#include "UnitAction.h"
#include <list>

class CAIActor;
//class CAIObject;
class CUnitImg;
typedef std::list<CUnitImg> TUnitList;

class CUnitImg
{
public:
	typedef enum
	{
		UF_FOLLOWING = 1 << 0,
		  UF_HIDING  = 1 << 1,
		  UF_BEHIND  = 1 << 2,
		  UF_FAR     = 1 << 3,
		  UF_MOVING  = 1 << 4,
		  UF_SPECIAL = 1 << 5
	} EUnitFlags;

	CUnitImg();
	CUnitImg(CWeakRef<CAIActor> refUnit);
	~CUnitImg();

	inline bool operator==(const CAIActor* thing) const    { return m_refUnit.GetAIObject() == thing; }
	inline bool operator==(const CUnitImg& otherImg) const { return m_refUnit == otherImg.m_refUnit; }
	inline bool operator!=(const CUnitImg& otherImg) const { return m_refUnit != otherImg.m_refUnit; }
	inline bool operator!=(const CAIActor* thing) const    { return m_refUnit.GetAIObject() != thing; }

	void        TaskExecuted();
	void        ExecuteTask();
	void        ClearPlanning(int priority = 0);
	void        ClearUnitAction(EUnitAction action);
	bool        IsPlanFinished() const;
	void        SuspendTask();
	void        ResumeTask();
	inline bool IsTaskSuspended() const { return m_bTaskSuspended; };
	bool        IsBlocked() const;
	inline bool Idle() const            { return m_pCurrentAction == NULL; };

	inline void SetFollowing()
	{
		m_flags |= UF_FOLLOWING;
	}

	inline void ClearFollowing()
	{
		m_flags &= ~UF_FOLLOWING;
	}

	inline bool IsFollowing()
	{
		return (m_flags & UF_FOLLOWING) != 0;
	}

	inline bool IsSpecial()
	{
		return (m_flags & UF_SPECIAL) != 0;
	}

	inline void SetSpecial()
	{
		m_flags |= UF_SPECIAL;
	}

	inline void ClearSpecial()
	{
		m_flags &= ~UF_SPECIAL;
	}

	inline void SetHiding()
	{
		m_flags |= UF_HIDING;
	}

	inline void ClearHiding()
	{
		m_flags &= ~UF_HIDING;
	}

	inline bool IsHiding()
	{
		return (m_flags & UF_HIDING) != 0;
	}

	inline void SetBehind()
	{
		m_flags |= UF_BEHIND;
	}

	inline void ClearBehind()
	{
		m_flags &= ~UF_BEHIND;
	}

	inline bool IsBehind()
	{
		return (m_flags & UF_BEHIND) != 0;
	}

	inline void SetMoving()
	{
		m_flags |= UF_MOVING;
	}

	inline void ClearMoving()
	{
		m_flags &= ~UF_MOVING;
	}

	inline bool IsMoving()
	{
		return (m_flags & UF_MOVING) != 0;
	}

	inline void SetFar()
	{
		m_flags |= UF_FAR;
	}

	inline void ClearFar()
	{
		m_flags &= ~UF_FAR;
	}

	inline bool IsFar()
	{
		return (m_flags & UF_FAR) != 0;
	}

	inline void ClearFlags()
	{
		m_flags = 0;
	}

	inline void         SetProperties(unsigned int prop) { m_properties = prop; };
	inline unsigned int GetProperties() const            { return m_properties; };
	const CUnitAction*  GetCurrentAction() const         { return m_pCurrentAction; };
	inline int          GetClass() const                 { return m_SoldierClass; };
	inline void         SetClass(int soldierclass)       { m_SoldierClass = soldierclass; };
	inline float        GetWidth()                       { return m_fWidth; }
	inline float        GetHeight()                      { return m_fHeight; }
	inline void         SetWidth(float w)                { m_fWidth = w; }
	inline void         SetHeight(float h)               { m_fHeight = h; }

	void                Serialize(TSerialize ser);
	void                Reset();

	CWeakRef<CAIActor> m_refUnit;
	TActionList        m_Plan;
	Vec3               m_TagPoint;
	uint16             m_flags;
	int                m_Group;   // used to split units in different groups
	int                m_FormationPointIndex;
	int                m_SoldierClass;
	float              m_fDistance;
	float              m_fDistance2;
	CTimeValue         m_lastReinforcementTime;

	uint32             m_properties; // binary mask to set properties (UPR_*)
	CTimeValue         m_lastMoveTime;

private:
	CUnitAction* m_pCurrentAction;
	bool         m_bTaskSuspended;
	float        m_fWidth;
	float        m_fHeight;
};

#endif
