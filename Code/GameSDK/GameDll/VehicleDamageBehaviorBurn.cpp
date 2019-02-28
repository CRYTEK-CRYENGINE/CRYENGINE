// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "IVehicleSystem.h"
#include "VehicleDamageBehaviorBurn.h"
#include "Game.h"
#include "GameRules.h"

//------------------------------------------------------------------------
CVehicleDamageBehaviorBurn::CVehicleDamageBehaviorBurn()
	: m_pVehicle(nullptr)
	, m_pHelper(nullptr)
	, m_damageRatioMin(1.0f)
	, m_damage(0.0f)
	, m_selfDamage(0.0f)
	, m_interval(0.0f)
	, m_radius(0.0f)
	, m_isActive(false)
	, m_timeCounter(0.0f)
	, m_shooterId(0)
	, m_timerId(-1)
{
}

//------------------------------------------------------------------------
CVehicleDamageBehaviorBurn::~CVehicleDamageBehaviorBurn()
{
	if (gEnv->pAISystem)
	  gEnv->pAISystem->RegisterDamageRegion(this, Sphere(ZERO, -1.0f)); // disable
}


//------------------------------------------------------------------------
bool CVehicleDamageBehaviorBurn::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_isActive = false;
	m_damageRatioMin = 1.f;
	m_timerId = -1;

	m_shooterId = 0;

	table.getAttr("damageRatioMin", m_damageRatioMin);

	if (CVehicleParams burnTable = table.findChild("Burn"))
	{
		burnTable.getAttr("damage", m_damage);
    burnTable.getAttr("selfDamage", m_selfDamage);
		burnTable.getAttr("interval", m_interval);
		burnTable.getAttr("radius", m_radius);

		if (burnTable.haveAttr("helper"))
			m_pHelper = m_pVehicle->GetHelper(burnTable.getAttr("helper"));

		return true;
	}

	return false;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Reset()
{
	if (m_isActive)
	{
		Activate(false);
	}

	m_shooterId = 0;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Activate(bool activate)
{
  if (activate && !m_isActive)
  {
    m_timeCounter = m_interval;
    m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
    m_timerId = m_pVehicle->SetTimer(-1, 20000, this); // total burn of 60 secs

		IAISystem* pAISystem = gEnv->pAISystem;
		if( !m_pVehicle->IsDestroyed() && !m_pVehicle->IsFlipped() && pAISystem && pAISystem->IsEnabled() && pAISystem->GetSmartObjectManager() )
		{
			gEnv->pAISystem->GetSmartObjectManager()->SetSmartObjectState(m_pVehicle->GetEntity(), "Exploding");        
		}

	m_pVehicle->NeedsUpdate(IVehicle::eVUF_AwakePhysics);
  }
  else if (!activate && m_isActive)
  {
    m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
    m_pVehicle->KillTimer(m_timerId);
    m_timerId = -1;

		if (gEnv->pAISystem)
	    gEnv->pAISystem->RegisterDamageRegion(this, Sphere(ZERO, -1.0f)); // disable
  }

  m_isActive = activate;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
  if (event == eVDBE_Repair)
  {
    if (behaviorParams.componentDamageRatio < m_damageRatioMin)
      Activate(false);

		m_shooterId = 0;
  }
	else 
	{
    if (behaviorParams.componentDamageRatio >= m_damageRatioMin)
	    Activate(true);

		m_shooterId = behaviorParams.shooterId;
	}
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
  switch (event)
  {
  case eVE_Timer:
    if (params.iParam == m_timerId)
      Activate(false);
    break;
  }
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Update(const float deltaTime)
{
	m_timeCounter -= deltaTime;

	if (m_timeCounter <= 0.0f)
	{
		CGameRules *pGameRules = g_pGame->GetGameRules();
		if (pGameRules && gEnv->bServer)
		{	
			Vec3 worldPos;
			if (m_pHelper)
				worldPos = m_pHelper->GetWorldSpaceTranslation();
			else
				worldPos = m_pVehicle->GetEntity()->GetWorldTM().GetTranslation();

      SEntityProximityQuery query;
      query.box = AABB(worldPos-Vec3(m_radius), worldPos+Vec3(m_radius));
      gEnv->pEntitySystem->QueryProximity(query);

			// first keep a temp copy of the query results (ServerHit can cause another query
			//	from another system, which invalidates our results)
			std::vector<IEntity*> tempQueryResults;
			tempQueryResults.reserve(query.nCount);
			for(int i=0; i<query.nCount; ++i)
			{
				tempQueryResults.push_back(query.pEntities[i]);
			}

			IEntity* pEntity = 0;
			for (size_t i = 0; i < tempQueryResults.size(); ++i)
			{			
				if ((pEntity = tempQueryResults[i]) && pEntity->GetPhysics())
				{
          float damage = (pEntity->GetId() == m_pVehicle->GetEntityId()) ? m_selfDamage : m_damage;

					// SNH: need to check vertical distance here as the QueryProximity() call seems to work in 2d only
					Vec3 pos = pEntity->GetWorldPos();
					if(abs(pos.z - worldPos.z) < m_radius)
					{
						if (damage > 0.f)
						{
							HitInfo hitInfo;
							hitInfo.damage = damage;
							hitInfo.pos = worldPos;
							hitInfo.radius = m_radius;
							hitInfo.targetId = pEntity->GetId();
							hitInfo.shooterId = m_shooterId;
							hitInfo.weaponId = m_pVehicle->GetEntityId();
							hitInfo.type = CGameRules::EHitType::Fire;
							hitInfo.dir.y = 0.f;
							assert(hitInfo.dir.IsZero());
							pGameRules->ServerHit(hitInfo);
						}   
					}
				}
			}
			
			if (gEnv->pAISystem)
	      gEnv->pAISystem->RegisterDamageRegion(this, Sphere(worldPos, m_radius));
		}

		m_timeCounter = m_interval;
	}

  m_pVehicle->NeedsUpdate();
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorBurn::Serialize(TSerialize ser, EEntityAspects aspects)
{
	if(ser.GetSerializationTarget()!=eST_Network)
	{
		bool active = m_isActive;
		ser.Value("Active", active);
		ser.Value("time", m_timeCounter);
		ser.Value("shooterId", m_shooterId);

    if (ser.IsReading())
    {
			if(active != m_isActive)
				Activate(active);
    }
	}
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorBurn);
