// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleActionAutoTarget.h"

const char* CVehicleActionAutoTarget::m_name = "VehicleActionAutoTarget";

CVehicleActionAutoTarget::CVehicleActionAutoTarget() : m_pVehicle(NULL), m_RegisteredWithAutoAimManager(false)
{
}

CVehicleActionAutoTarget::~CVehicleActionAutoTarget()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
}

bool CVehicleActionAutoTarget::Init( IVehicle* pVehicle, const CVehicleParams& table )
{
	m_pVehicle = pVehicle;
	pVehicle->RegisterVehicleEventListener(this, "AutoTarget");

	IEntity* pEntity = pVehicle->GetEntity();
	if( ICharacterInstance* pCharacter = pEntity->GetCharacter(0) )
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacter->GetIDefaultSkeleton();
		{
			const char* primaryBoneName;
			table.getAttr("PrimaryBoneName", &primaryBoneName);
			m_autoAimParams.primaryBoneId = rIDefaultSkeleton.GetJointIDByName(primaryBoneName);

			const char* secondaryBoneName;
			table.getAttr("SecondaryBoneName", &secondaryBoneName);
			m_autoAimParams.secondaryBoneId = rIDefaultSkeleton.GetJointIDByName(secondaryBoneName);

			const char* physicsBoneName;
			table.getAttr("PhysicsBoneName", &physicsBoneName);
			m_autoAimParams.physicsBoneId = rIDefaultSkeleton.GetJointIDByName(physicsBoneName);

			m_autoAimParams.hasSkeleton = true;
		}
	}

	table.getAttr("FallbackOffset", m_autoAimParams.fallbackOffset);
	table.getAttr("InnerRadius", m_autoAimParams.innerRadius);
	table.getAttr("OuterRadius", m_autoAimParams.outerRadius);
	table.getAttr("SnapRadius", m_autoAimParams.snapRadius);
	table.getAttr("SnapRadiusTagged", m_autoAimParams.snapRadiusTagged);

	return true;
}

void CVehicleActionAutoTarget::Reset()
{
	if(m_RegisteredWithAutoAimManager)
	{
		g_pGame->GetAutoAimManager().UnregisterAutoaimTarget(m_pVehicle->GetEntityId());
		m_RegisteredWithAutoAimManager = false;
	}
}

void CVehicleActionAutoTarget::Release()
{
	Reset();

	delete this;
}

int CVehicleActionAutoTarget::OnEvent(int eventType, SVehicleEventParams& eventParams)
{
	return 1;
}

void CVehicleActionAutoTarget::OnVehicleEvent( EVehicleEvent event, const SVehicleEventParams& params )
{
	if( (event == eVE_Destroyed || event == eVE_PassengerExit) && m_RegisteredWithAutoAimManager)
	{
		//No longer a target
		g_pGame->GetAutoAimManager().UnregisterAutoaimTarget(m_pVehicle->GetEntityId());
		m_RegisteredWithAutoAimManager = false;
	}
	else if(event == eVE_PassengerEnter && !m_RegisteredWithAutoAimManager)
	{
		IActor* pDriver = m_pVehicle->GetDriver();
		if(pDriver && !pDriver->IsClient()) //No need to aim at yourself
		{
			g_pGame->GetAutoAimManager().RegisterAutoaimTargetObject(m_pVehicle->GetEntityId(), m_autoAimParams);
			m_RegisteredWithAutoAimManager = true;
		}
	}
}

int IVehicleAction::OnEvent( int eventType, SVehicleEventParams& eventParams )
{
	return 0;
}

DEFINE_VEHICLEOBJECT(CVehicleActionAutoTarget);
