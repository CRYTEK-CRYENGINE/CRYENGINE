// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Entity that can change it's physicalisation state during a
		game.
  
 -------------------------------------------------------------------------
  History:
  - 19:09:2010: Created by Colin Gulliver

*************************************************************************/

#include "StdAfx.h"
#include "NetworkedPhysicsEntity.h"

//-----------------------------------------------------------------------------
CNetworkedPhysicsEntity::CNetworkedPhysicsEntity()
{
	m_physicsType = ePhys_NotPhysicalized;
	m_requestedPhysicsType = ePhys_NotPhysicalized;
}

//-----------------------------------------------------------------------------
CNetworkedPhysicsEntity::~CNetworkedPhysicsEntity()
{
	SAFE_DELETE(m_physicsParams.pBuoyancy);
}

//-----------------------------------------------------------------------------
bool CNetworkedPhysicsEntity::Init(IGameObject *pGameObject)
{
	SetGameObject(pGameObject);

	if (!GetGameObject()->CaptureProfileManager(this))
		return false;

	if (!pGameObject->BindToNetwork())
	{
		return false;
	}

	SEntityPhysicalizeParams params;
	params.type = PE_NONE;
	params.density = 0.f;
	params.mass = 20.f;
	GetEntity()->Physicalize(params);

	ReadPhysicsParams();

	return true;
}

//-----------------------------------------------------------------------------
bool CNetworkedPhysicsEntity::ReloadExtension( IGameObject * pGameObject, const SEntitySpawnParams &params )
{
	ResetGameObject();

	CRY_ASSERT_MESSAGE(false, "CCTFFlag::ReloadExtension not implemented");
	
	return false;
}

//-----------------------------------------------------------------------------
bool CNetworkedPhysicsEntity::GetEntityPoolSignature( TSerialize signature )
{
	CRY_ASSERT_MESSAGE(false, "CCTFFlag::GetEntityPoolSignature not implemented");
	
	return true;
}

//-----------------------------------------------------------------------------
void CNetworkedPhysicsEntity::Release()
{
	delete this;
}

//-----------------------------------------------------------------------------
void CNetworkedPhysicsEntity::GetMemoryUsage(ICrySizer *pSizer) const
{
	pSizer->Add(*this);
}

//-----------------------------------------------------------------------------
bool CNetworkedPhysicsEntity::NetSerialize( TSerialize ser, EEntityAspects aspect, uint8 profile, int flags )
{
	NET_PROFILE_SCOPE("NetworkedPhysicsEntity", ser.IsReading());

	if (aspect == eEA_Physics)
	{
		pe_type type = PE_NONE;
		
		switch (profile)
		{
			case ePhys_PhysicalizedRigid:
			{
				type = PE_RIGID;
				break;
			}
			case ePhys_PhysicalizedStatic:
			{
				type = PE_STATIC;

				// Serialise the position ourselves - physics system won't do it for static entities
				const Matrix34 &worldTM = GetEntity()->GetWorldTM();
				Vec3 worldPos = worldTM.GetTranslation();
				ser.Value("worldPos", worldPos, 'wrld');
				if (ser.IsReading())
				{
					Matrix34 newTM = worldTM;
					newTM.SetTranslation(worldPos);
					GetEntity()->SetWorldTM(newTM);
				}

				break;
			}
		}

		if (type == PE_NONE)
			return true;

		if (ser.IsWriting())
		{
			if (!GetEntity()->GetPhysicalEntity() || GetEntity()->GetPhysicalEntity()->GetType() != type)
			{
				gEnv->pPhysicalWorld->SerializeGarbageTypedSnapshot( ser, type, 0 );
				return true;
			}
		}

		GetEntity()->PhysicsNetSerializeTyped( ser, type, flags );
	}
	return true;
}

//------------------------------------------------------------------------
bool CNetworkedPhysicsEntity::SetAspectProfile( EEntityAspects aspect, uint8 profile )
{
	bool bRetVal = false;
	if (aspect == eEA_Physics)
	{
		switch (profile)
		{
			case ePhys_PhysicalizedRigid:
			{
				if (m_physicsType!=ePhys_PhysicalizedRigid)
				{
					m_physicsParams.type = PE_RIGID;
					GetEntity()->Physicalize(m_physicsParams);
					m_physicsType = ePhys_PhysicalizedRigid;
					GetEntity()->GetPhysics()->SetParams(m_physicsParams.pBuoyancy);
				}
				bRetVal = true;
				break;
			}
			case ePhys_PhysicalizedStatic:
			{
				if (m_physicsType != ePhys_PhysicalizedStatic)
				{
					m_physicsParams.type = PE_STATIC;
					GetEntity()->Physicalize(m_physicsParams);
					m_physicsType = ePhys_PhysicalizedStatic;
				}
				bRetVal = true;
				break;
			}
			case ePhys_NotPhysicalized:
			{
				if (m_physicsType!=ePhys_NotPhysicalized)
				{
					SEntityPhysicalizeParams params;
					params.type = PE_NONE;
					GetEntity()->Physicalize(params);
					m_physicsType = ePhys_NotPhysicalized;
				}
				bRetVal = true;
				break;
			}
			default:
				{
					CryLog("Unsupported physicalization type in CNetworkedPhysicsEntity!");
				}
		}
	}

	IPhysicalEntity * pPhysEnt = GetEntity()->GetPhysics();
	if(pPhysEnt)
	{
			// Turn off some collisions
			// NB: by leaving pe_params_part.ipart undefined, all the geom flags will changed
			pe_params_part pp;
			pp.flagsAND = ~(geom_colltype_ray|geom_colltype_player|geom_colltype_explosion|geom_colltype_debris|geom_colltype_foliage_proxy);
			pPhysEnt->SetParams(&pp);

			pe_simulation_params psp;
			psp.damping = 0.5f;
			pPhysEnt->SetParams(&psp);

			//Add a bit of angular velocity so it doesn't look bizarre.
			pe_action_set_velocity physicsAction;
			physicsAction.w = cry_random_componentwise(Vec3(-7.5f), Vec3(7.5f));
			pPhysEnt->Action(&physicsAction);
	}

	return bRetVal;
}

//------------------------------------------------------------------------
uint8 CNetworkedPhysicsEntity::GetDefaultProfile( EEntityAspects aspect )
{
	if (aspect == eEA_Physics)
	{
		return ePhys_NotPhysicalized;
	}
	else
	{
		return 0;
	}
}

//------------------------------------------------------------------------
void CNetworkedPhysicsEntity::Physicalize( ePhysicalization physicsType )
{
	m_requestedPhysicsType = physicsType;
	if (gEnv->bServer)
	{
		GetGameObject()->SetAspectProfile(eEA_Physics, physicsType);
	}
}

//------------------------------------------------------------------------
void CNetworkedPhysicsEntity::ReadPhysicsParams()
{
	SmartScriptTable pScriptTable = GetEntity()->GetScriptTable();
	if (pScriptTable)
	{
		SmartScriptTable pProperties;
		if (pScriptTable->GetValue("Properties", pProperties))
		{
			SmartScriptTable pPhysicsParams;
			if (pProperties->GetValue("Physics", pPhysicsParams))
			{
				CScriptSetGetChain chain(pPhysicsParams);
				chain.GetValue( "mass", m_physicsParams.mass );
				chain.GetValue( "density", m_physicsParams.density );
				chain.GetValue( "flags", m_physicsParams.nFlagsOR );
				chain.GetValue( "partid", m_physicsParams.nAttachToPart );
				chain.GetValue( "stiffness_scale", m_physicsParams.fStiffnessScale );
				chain.GetValue( "lod", m_physicsParams.nLod );

				if(!m_physicsParams.pBuoyancy)
				{
					m_physicsParams.pBuoyancy = new pe_params_buoyancy();
				}
				chain.GetValue( "water_damping", m_physicsParams.pBuoyancy->waterDamping );
				chain.GetValue( "water_resistance", m_physicsParams.pBuoyancy->waterResistance );
				chain.GetValue( "water_density", m_physicsParams.pBuoyancy->waterDensity );

				m_physicsParams.type = PE_RIGID;

				return;
			}
		}
	}
	CRY_ASSERT(!"Failed to read physics params from script for NetworkedPhysicsEntity");
}

//------------------------------------------------------------------------
void CNetworkedPhysicsEntity::SetAuthority( bool auth )
{
	// This won't be called as SetAuthority is no more a part of the IGameObjectExtension.
	// Use ENTITY_EVENT_SET_AUTHORITY instead.
	if (auth && gEnv->bServer)
	{
		if (m_requestedPhysicsType != m_physicsType)
		{
			GetGameObject()->SetAspectProfile(eEA_Physics, m_requestedPhysicsType);
		}
	}
}
