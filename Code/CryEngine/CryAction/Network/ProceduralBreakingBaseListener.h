// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: common listener code for procedural breaks
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __PROCEDURALBREAKBASELISTENER_H__
#define __PROCEDURALBREAKBASELISTENER_H__

#pragma once

#if !NET_USE_SIMPLE_BREAKAGE

	#include "IBreakReplicatorListener.h"
	#include "IProceduralBreakType.h"
	#include <CryCore/Containers/VectorMap.h>

void DumpSpawnRecs(const DynArray<SProceduralSpawnRec>& recs, const DynArray<SJointBreakRec>& joints, const char* title);

class CProceduralBreakingBaseListener : public IBreakReplicatorListener
{
public:
	CProceduralBreakingBaseListener(IProceduralBreakTypePtr pBreakType);
	~CProceduralBreakingBaseListener();
	bool            AcceptUpdateMesh(const EventPhysUpdateMesh* pEvent);
	bool            AcceptCreateEntityPart(const EventPhysCreateEntityPart* pEvent);
	bool            AcceptRemoveEntityParts(const EventPhysRemoveEntityParts* pEvent);
	bool            AcceptJointBroken(const EventPhysJointBroken* pEvent);

	void            OnSpawn(IEntity* pEntity, SEntitySpawnParams& params);
	void            OnRemove(IEntity* pEntity);

	void            EndEvent(INetContext* pCtx);

	virtual void    OnPostStep();
	virtual void    OnStartFrame();
	virtual bool    OnEndFrame();

	virtual void    OnTimeout();

	CObjectSelector GetObjectSelector(int idx);

	const char*     GetName()
	{
		if (m_name.empty())
			m_name.Format("ProceduralBreaking.%s.%s", GetListenerName(), m_pBreakType->GetName());
		return m_name.c_str();
	}

protected:
	IProceduralBreakType* GetBreakType()           { return m_pBreakType; }
	void                  DisallowAdditions()      { m_allowAdditions = false; }
	int                   GetNumEmptySteps() const { return m_numEmptySteps; }

private:
	VectorMap<EntityId, int> m_localEntityIds;
	IProceduralBreakTypePtr  m_pBreakType;
	#if BREAK_HIERARCHICAL_TRACKING
	int                      m_physTime;
	int                      m_activityThisStep;
	int                      m_numEmptySteps;
	#endif
	ENetBreakOperation       m_curBreakOp;
	int                      m_curBreakId;
	int                      m_curBreakPart;
	bool                     m_allowAdditions;
	string                   m_name; // usually empty

	struct SJustSpawnedObject
	{
		ENetBreakOperation op;
		int                idxRef;
		EntityId           to;
		int                virtId;
	};
	std::vector<SJustSpawnedObject> m_justSpawnedObjects;

	virtual int         GotBreakOp(ENetBreakOperation op, int idxRef, int partid, EntityId to) = 0;
	virtual void        FinishBreakOp(ENetBreakOperation op, int idxRef, EntityId to, int virtId) = 0;
	virtual void        GotJointBreak(int idxRef, int id, int epicenter) = 0;
	virtual void        GotJointBreakParticle(const EventPhysCreateEntityPart* pEvent) = 0;
	virtual void        Complete() = 0;
	virtual bool        AllowComplete() = 0;
	virtual const char* GetListenerName() = 0;

	bool                AcceptIfRelated(IPhysicalEntity* pEnt, ENetBreakOperation breakOp, int partid);
};

#endif // !NET_USE_SIMPLE_BREAKAGE

#endif
