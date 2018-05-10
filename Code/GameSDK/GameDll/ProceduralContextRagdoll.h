// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: ProceduralContext for the ProceduralClipRagdoll from CryMannequin

-------------------------------------------------------------------------
History:
- 27.09.2012: Created by Stephen M. North

*************************************************************************/
#pragma once

#ifndef __ProceduralContextRagdoll_h__
#define __ProceduralContextRagdoll_h__

#include <ICryMannequin.h>

#include <CryAnimation/ICryAnimation.h>
#include <CryAnimation/IAnimationPoseModifier.h>

#include <CryExtension/CryCreateClassInstance.h>
#include <CryExtension/ClassWeaver.h>
#include <CryExtension/ICryFactoryRegistryImpl.h>
#include <CryExtension/RegFactoryNode.h>

class CProceduralContextRagdoll
	: public IProceduralContext
{
public:
	PROCEDURAL_CONTEXT(CProceduralContextRagdoll, "ProceduralContextRagdoll", "37856d62-bd5f-42f0-ad8a-4314a0de6dd2"_cry_guid);

	CProceduralContextRagdoll();
	virtual ~CProceduralContextRagdoll() {}

	ILINE EntityId GetEntityTarget() const { return m_targetEntityId; }
	ILINE void SetEntityTarget( const EntityId entityID ) { m_targetEntityId = entityID; }
	ILINE void SetAspectProfileScope( bool bScope ) { m_bDispatchedAspectProfile = bScope; }
	ILINE bool GetAspectProfileScope() const { return m_bDispatchedAspectProfile; }
	ILINE bool IsInRagdoll() const{ return m_bInRagdoll; }
	ILINE void SetBlendOut( float blendOutTime )
	{
		m_blendOutTime = blendOutTime;
		m_bInBlendOut = true;
	}
	void EnableRagdoll( const EntityId entityID, const bool bAlive, const float stiffness, const bool bFromProcClip = false );
	void DisableRagdoll( float blendOutTime );
	void QueueRagdoll( bool bAlive );
	void ForceRagdollFinish(IActor* piActor, bool bForceDead);

protected:
	// IProceduralContext
	virtual void Update( float timePassedSeconds ) override;
	// ~IProceduralContext

	void Reset();

private:
	EntityId m_targetEntityId;
	float m_stiffness;
	float m_blendOutTime;
	float m_blendOutTimeCurrent;
	bool m_bInRagdoll;
	bool m_bInBlendOut;
	bool m_bEntityAlive;
	bool m_bDispatchedAspectProfile;
	bool m_bForceRagdollFinish;
	bool m_bFromProcClip;
};

#endif //__ProceduralContextRagdoll_h__