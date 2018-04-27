// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "IVehicleSystem.h"

#include "VehiclePartWaterRipplesGenerator.h"
#include "Vehicle.h"

//#pragma optimize("", off)
//#pragma inline_depth(0)

CVehiclePartWaterRipplesGenerator::CVehiclePartWaterRipplesGenerator()
	: m_localOffset(ZERO)
	, m_waterRipplesScale(1.0f)
	, m_waterRipplesStrength(1.0f)
	, m_minMovementSpeed(1.0f)
	, m_onlyMovingForward(false)
{

}

CVehiclePartWaterRipplesGenerator::~CVehiclePartWaterRipplesGenerator()
{
}

bool CVehiclePartWaterRipplesGenerator::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Massbox))
		return false;

	const CVehicleParams waterRipplesTable = table.findChild("WaterRipplesGen");
	if (waterRipplesTable)
	{
		waterRipplesTable.getAttr("scale", m_waterRipplesScale);
		waterRipplesTable.getAttr("strength", m_waterRipplesStrength);
		waterRipplesTable.getAttr("minMovementSpeed", m_minMovementSpeed);
		waterRipplesTable.getAttr("moveForwardOnly", m_onlyMovingForward);

		m_minMovementSpeed = max(m_minMovementSpeed, 0.01f);
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	return true;
}

void CVehiclePartWaterRipplesGenerator::PostInit()
{
	IVehicleHelper* pHelper = m_pVehicle->GetHelper(m_pSharedParameters->m_helperPosName.c_str());
	if (pHelper)
	{
		m_localOffset = pHelper->GetLocalTM().GetTranslation();
	}
}

void CVehiclePartWaterRipplesGenerator::Update(const float frameTime)
{
	//IVehicleMovement* pMovement = m_pVehicle->GetMovement();
	const SVehicleStatus& status = m_pVehicle->GetStatus();

	const bool movingFastEnough = (status.speed > m_minMovementSpeed);
	if (movingFastEnough)
	{
		const Matrix34 vehicleWorldTM = m_pVehicle->GetEntity()->GetWorldTM();

		// Check if moving backwards...
		if (m_onlyMovingForward)
		{
			const float dotProduct = vehicleWorldTM.GetColumn1().Dot((status.vel / status.speed));
			if (dotProduct < 0.0f)
				return;
		}

		gEnv->p3DEngine->AddWaterRipple(vehicleWorldTM.TransformPoint(m_localOffset), m_waterRipplesScale, m_waterRipplesStrength);
	}
}

DEFINE_VEHICLEOBJECT(CVehiclePartWaterRipplesGenerator);
