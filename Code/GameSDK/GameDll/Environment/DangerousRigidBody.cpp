// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DangerousRigidBody.h"
#include "GameRules.h"

int CDangerousRigidBody::sDangerousRigidBodyHitTypeId	= -1;

CDangerousRigidBody::CDangerousRigidBody() : m_dangerous(false)
																						,m_friendlyFireEnabled(false)
																						,m_damageDealt(10000.f)
																						,m_lastHitTime(0.f)
																						,m_timeBetweenHits(0.5f)
																						,m_activatorTeam(0)
{
	if( CGameRules* pGameRules = g_pGame->GetGameRules() )
	{
		sDangerousRigidBodyHitTypeId = pGameRules->GetHitTypeId("punish");
	}
}

bool CDangerousRigidBody::Init( IGameObject *pGameObject )
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->BindToNetwork())
	{
		return false;
	}

	Reset();

	return true;
}

void CDangerousRigidBody::InitClient( int channelId )
{

}

void CDangerousRigidBody::Release()
{
	delete this;
}

bool CDangerousRigidBody::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	if(aspect == ASPECT_DAMAGE_STATUS)
	{
		ser.Value("m_Dangerous", m_dangerous, 'bool');
		ser.Value("m_activatorTeam", m_activatorTeam, 'ui2');
	}

	return true;
}

void CDangerousRigidBody::ProcessEvent( const SEntityEvent& event )
{
	switch(event.event)
	{
	case ENTITY_EVENT_COLLISION:
		{
			if(m_dangerous)
			{
				EventPhysCollision *pCollision = (EventPhysCollision *)(event.nParam[0]);
				if(pCollision->pEntity[0] && pCollision->pEntity[1])
				{
					IPhysicalEntity* pOtherEntityPhysics = GetEntity()->GetPhysics() != pCollision->pEntity[0] ? pCollision->pEntity[0] : pCollision->pEntity[1];
					if( IEntity* pOtherEntity = gEnv->pEntitySystem->GetEntityFromPhysics(pOtherEntityPhysics) )
					{
						EntityId localClientId = gEnv->pGameFramework->GetClientActorId();
						if(pOtherEntity->GetId() == localClientId) //Handle collision locally 
						{
							float currentTime = gEnv->pTimer->GetFrameStartTime().GetSeconds();
							if(currentTime - m_lastHitTime >  m_timeBetweenHits)
							{
								CGameRules* pGameRules = g_pGame->GetGameRules();

								bool sameTeam = m_activatorTeam && (pGameRules->GetTeam(localClientId) == m_activatorTeam);
								float damageDealt = !sameTeam ? m_damageDealt : g_pGameCVars->g_friendlyfireratio * m_damageDealt;

								Vec3 rigidBodyPos = GetEntity()->GetWorldPos();

								HitInfo hitInfo(0, localClientId, 0,
									damageDealt, 0.0f, 0, 0,
									sDangerousRigidBodyHitTypeId, rigidBodyPos, (pOtherEntity->GetWorldPos()-rigidBodyPos).GetNormalized(), Vec3(ZERO));
								pGameRules->ClientHit(hitInfo);
								m_lastHitTime = currentTime;
							}
						}
					}
				}
			}
		}
		break;
	case ENTITY_EVENT_RESET:
		{
			Reset();
		}
		break;
	}
}

uint64 CDangerousRigidBody::GetEventMask() const
{
	return ENTITY_EVENT_BIT(ENTITY_EVENT_COLLISION) | ENTITY_EVENT_BIT(ENTITY_EVENT_RESET);
}

void CDangerousRigidBody::GetMemoryUsage( ICrySizer *pSizer ) const
{
	pSizer->Add(*this);
}

bool CDangerousRigidBody::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CDangerousRigidBody::ReloadExtension not implemented");

	return true;
}

bool CDangerousRigidBody::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CDangerousRigidBody::GetEntityPoolSignature not implemented");

	return true;
}

void CDangerousRigidBody::SetIsDangerous( bool isDangerous, EntityId triggerPlayerId )
{
	if(gEnv->bServer && isDangerous != m_dangerous)
	{
		m_dangerous = isDangerous;
		m_activatorTeam = g_pGame->GetGameRules()->GetTeam(triggerPlayerId);

		CHANGED_NETWORK_STATE(this, ASPECT_DAMAGE_STATUS);
	}
}

void CDangerousRigidBody::Reset()
{
	IScriptTable*  pTable = GetEntity()->GetScriptTable();
	if (pTable != NULL)
	{
		SmartScriptTable propertiesTable;
		if (pTable->GetValue("Properties", propertiesTable))
		{
			propertiesTable->GetValue("bCurrentlyDealingDamage", m_dangerous);
			propertiesTable->GetValue("bDoesFriendlyFireDamage", m_friendlyFireEnabled);
			propertiesTable->GetValue("fDamageToDeal", m_damageDealt);
			propertiesTable->GetValue("fTimeBetweenHits", m_timeBetweenHits);
		}
	}
	m_activatorTeam = 0;
	m_lastHitTime = 0;
}

