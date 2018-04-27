// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: ProceduralContext for the ProceduralClipRagdoll from CryMannequin

-------------------------------------------------------------------------
History:
- 27.09.2012: Created by Stephen M. North

*************************************************************************/
#include <StdAfx.h>

#include "ProceduralContextRagdoll.h"

#include "Player.h"
#include "HitDeathReactions.h"

CRYREGISTER_CLASS( CProceduralContextRagdoll );

CProceduralContextRagdoll::CProceduralContextRagdoll()
	: m_bForceRagdollFinish(false)
{
	Reset();
}

void CProceduralContextRagdoll::EnableRagdoll( const EntityId entityID, const bool bAlive, const float stiffness, const bool bFromProcClip )
{
	CRY_ASSERT( !m_targetEntityId || m_targetEntityId==entityID );

	m_targetEntityId = entityID;
	if( !m_bInRagdoll || m_bInRagdoll && m_bEntityAlive && !bAlive )
	{
		m_stiffness = stiffness;
		m_bEntityAlive = bAlive;
		m_bFromProcClip = bFromProcClip;

		QueueRagdoll(bAlive);
	}
}

void CProceduralContextRagdoll::DisableRagdoll( float blendOutTime )
{
	if( blendOutTime )
	{
		m_blendOutTime = m_blendOutTimeCurrent = blendOutTime;
		m_bInBlendOut = true;
	}
	else
	{
		Reset();

		m_bInRagdoll = true;
	}
}

void CProceduralContextRagdoll::QueueRagdoll( bool bAlive )
{
	if( m_targetEntityId == 0 )
	{
		m_targetEntityId = m_entity->GetId();
	}

	IActor* piActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_targetEntityId );

	// NOTE: The case where piActor is NULL is when you're - in the CryMann preview!
	if(piActor)
	{
		if( gEnv->bServer && !m_bDispatchedAspectProfile && (piActor->GetGameObject()->GetAspectProfile(eEA_Physics) != eAP_Ragdoll) )
		{
			m_bEntityAlive = bAlive;
			piActor->GetGameObject()->SetAspectProfile( eEA_Physics, bAlive ? eAP_Sleep : eAP_Ragdoll );
		}
		else if( !m_bInRagdoll || (m_bInRagdoll && !bAlive && m_bEntityAlive) )
		{
			SRagdollizeParams params;
			params.mass = static_cast<CActor*>(piActor)->GetActorPhysics().mass;
			params.sleep = m_bEntityAlive = bAlive;
			params.stiffness = m_stiffness;

			SGameObjectEvent event( eGFE_QueueRagdollCreation, eGOEF_ToExtensions );
			event.ptr = &params;

			piActor->GetGameObject()->SendEvent( event );

			m_bInRagdoll = true;
		}
		else
		{
			m_bInBlendOut = true;
		}
	}
#ifndef _RELEASE
	else
	{
		if( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_targetEntityId ) )
		{
			ICharacterInstance *pCharacter = pEntity->GetCharacter(0);
			if (pCharacter)
			{
				// dead guys shouldn't blink
				pCharacter->EnableProceduralFacialAnimation(false);
				//Anton :: SetDefaultPose on serialization
				if(gEnv->pSystem->IsSerializingFile() && pCharacter->GetISkeletonPose())
					pCharacter->GetISkeletonPose()->SetDefaultPose();
			}

			SEntityPhysicalizeParams pp;

			pp.fStiffnessScale = m_stiffness;
			pp.type = PE_ARTICULATED;
			pp.nSlot = 0;
			pp.bCopyJointVelocities = true;

			//never ragdollize without mass [Anton]
			pp.mass = (float)__fsel(-pp.mass, 80.0f, pp.mass);

			pe_player_dimensions playerDim;
			pe_player_dynamics playerDyn;

			playerDyn.gravity = Vec3( 0.f, 0.f, -15.0f );
			playerDyn.kInertia = 5.5f;

			pp.pPlayerDynamics = &playerDyn;

			// Joints velocities are copied by default for now
			pp.bCopyJointVelocities = !gEnv->pSystem->IsSerializingFile();
			pp.nFlagsOR = pef_monitor_poststep;
			pEntity->Physicalize(pp);

			m_bInRagdoll = true;
		}
	}
#endif
}

void CProceduralContextRagdoll::Update( float timePassedSeconds )
{
	if( m_bInBlendOut )
	{
		m_blendOutTimeCurrent -= timePassedSeconds;
		const float blendOutFactor = max( m_blendOutTime > 0.0f ? m_blendOutTimeCurrent/m_blendOutTime : 0.0f, 0.0f );

		if( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_targetEntityId ) )
		{
			if(IPhysicalEntity* pent = pEntity->GetPhysics())
			{
				pe_status_nparts parts;
				int partCount = pent->GetStatus(&parts);
				for (int i=0; i<partCount; ++i)
				{
					pe_params_part pp;
					pe_params_joint pj;
					pp.ipart = i;
					pent->GetParams(&pp);
					pj.op[1] = pp.partid;
					pj.ks = Vec3(max(m_stiffness * blendOutFactor, 1.0f));
					pent->SetParams(&pj);
				}
			}
		}

		if( blendOutFactor == 0.0f )
		{
			IActor* piActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_targetEntityId );
			if( piActor )
			{
				ForceRagdollFinish( piActor, false );
			}
			Reset();

			m_bInRagdoll = piActor ? piActor->IsDead() : true;
		}
	}
	else if( m_bInRagdoll && !m_bEntityAlive && !m_bFromProcClip )
	{
		IActor* piActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor( m_targetEntityId );
		if( piActor )
		{
			ForceRagdollFinish( piActor, true );
		}
	}
}

void CProceduralContextRagdoll::Reset()
{
	m_stiffness = 0.f;
	m_blendOutTime = 0.0f;
	m_blendOutTimeCurrent = 0.0f;
	m_bInBlendOut = m_bInRagdoll = m_bDispatchedAspectProfile = m_bFromProcClip = false;
	m_bEntityAlive = true;
	m_targetEntityId = 0;
}

void CProceduralContextRagdoll::ForceRagdollFinish(IActor* piActor, bool bForceDead)
{
	if( !m_bForceRagdollFinish && (!m_bEntityAlive || bForceDead ) )
	{
		m_bEntityAlive = false;
		static_cast<CPlayer*> (piActor)->GetHitDeathReactions()->OnRagdollize( true );
		m_bForceRagdollFinish = true;
	
		if( IEntity* pEntity = gEnv->pEntitySystem->GetEntity( m_targetEntityId ) )
		{
			if(IPhysicalEntity* pent = pEntity->GetPhysics())
			{
				pe_params_skeleton status;
				status.stiffness = 0.0f;
				pent->SetParams( &status );
			}
		}
	}
}
