// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:  network breakability: recording of generic breaking events
   -------------------------------------------------------------------------
   History:
   - 22/01/2007   10:34 : Created by Craig Tiller
*************************************************************************/
#ifndef __GENERICRECORDINGLISTENER_H__
#define __GENERICRECORDINGLISTENER_H__

#pragma once

#include "IBreakReplicatorListener.h"

class CGenericRecordingListener : public IBreakReplicatorListener
{
public:
	CGenericRecordingListener();

	bool AcceptUpdateMesh(const EventPhysUpdateMesh* pEvent);
	bool AcceptCreateEntityPart(const EventPhysCreateEntityPart* pEvent);
	bool AcceptRemoveEntityParts(const EventPhysRemoveEntityParts* pEvent);
	bool AcceptJointBroken(const EventPhysJointBroken*);

	void OnSpawn(IEntity* pEntity, SEntitySpawnParams& params);
	void OnRemove(IEntity* pEntity);
	void EndEvent(INetContext* pCtx);

	void OnPostStep() {}

	bool AttemptAbsorb(const IProceduralBreakTypePtr& pBT)
	{
		return false;
	}

	virtual void OnStartFrame()
	{
	}

	virtual bool OnEndFrame()
	{
		CRY_ASSERT(false);
		return false;
	}

	virtual void OnTimeout()
	{
	}

	const char* GetName()
	{
		return "GenericRecordingListener";
	}

private:
	const SNetMessageDef*    m_pDef;
	IBreakDescriptionInfoPtr m_pInfo;
	DynArray<EntityId>       m_spawned;
};

#endif
