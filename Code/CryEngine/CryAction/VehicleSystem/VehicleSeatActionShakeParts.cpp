// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include "VehicleSeatActionShakeParts.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehicleSeatActionShakeParts::SSharedParams);

CVehicleSeatActionShakeParts::CVehicleSeatActionShakeParts()
{
	m_pVehicle = NULL;
}

//------------------------------------------------------------------------
bool CVehicleSeatActionShakeParts::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	CryFixedStringT<256> sharedParamsName;
	IEntity* pEntity = pVehicle->GetEntity();
	ISharedParamsManager* pSharedParamsManager = gEnv->pGameFramework->GetISharedParamsManager();
	CRY_ASSERT(pSharedParamsManager);
	sharedParamsName.Format("%s::%s::CVehicleSeatActionShakeParts", pEntity->GetClass()->GetName(), pVehicle->GetModification());
	m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Get(sharedParamsName));

	if (!m_pSharedParams)
	{
		SSharedParams sharedParams;
		CVehicleParams partsTable = table.findChild("Parts");
		if (partsTable)
		{
			const int partCount = partsTable.getChildCount();
			m_controlledParts.reserve(partCount);
			sharedParams.partInfos.reserve(partCount);
			for (int partIdx = 0; partIdx < partCount; ++partIdx)
			{
				CVehicleParams partTable = partsTable.getChild(partIdx);

				if (strcmp(partTable.getTag(), "Part") == 0)
				{
					const char* controlledPartName = partTable.getAttr("name");

					IVehiclePart* pControlledPart = m_pVehicle->GetPart(controlledPartName);
					if (pControlledPart)
					{
						sharedParams.partInfos.resize(sharedParams.partInfos.size() + 1);
						SSharedParams::SPartInfo* partInfo = &sharedParams.partInfos.back();

						partInfo->partIndex = pControlledPart->GetIndex();
						partInfo->amplitudeUpDown = 0.f;
						partInfo->amplitudeRot = 0.f;
						partInfo->freq = 0.f;
						partInfo->suspensionAmp = 0.f;
						partInfo->suspensionResponse = 0.f;
						partInfo->suspensionSharpness = 0.f;

						partTable.getAttr("amp", partInfo->amplitudeUpDown);
						partTable.getAttr("ampRot", partInfo->amplitudeRot);
						partTable.getAttr("freq", partInfo->freq);
						partTable.getAttr("suspensionAmp", partInfo->suspensionAmp);
						partTable.getAttr("suspensionResponse", partInfo->suspensionResponse);
						partTable.getAttr("suspensionSharpness", partInfo->suspensionSharpness);
						partInfo->amplitudeRot = DEG2RAD(partInfo->amplitudeRot);
					}
				}
			}
		}
		else
		{
			GameWarning("Can't initialise CVehicleSeatActionShakeParts there's no Parts node, vehicle %s", pEntity->GetName());
		}
		m_pSharedParams = CastSharedParamsPtr<SSharedParams>(pSharedParamsManager->Register(sharedParamsName, sharedParams));
	}

	SSharedParams::TPartInfosConst& partInfos = m_pSharedParams->partInfos;
	SSharedParams::TPartInfosConst::const_iterator partInfo = partInfos.begin();
	m_controlledParts.resize(partInfos.size());
	TParts::iterator part = m_controlledParts.begin();
	for (; partInfo != partInfos.end(); ++partInfo, ++part)
	{
		IVehiclePart* pControlledPart = m_pVehicle->GetPart(partInfo->partIndex);
		pControlledPart->SetMoveable(true);
		part->noiseUpDown.Setup(0.f, 0.f);
		part->noiseRot.Setup(0.f, 0.f, 0x567282);
		part->zpos = 0.f;
	}

	return true;
}

void CVehicleSeatActionShakeParts::StartUsing(EntityId passengerId)
{
	if (!m_pVehicle->IsPlayerDriving(true) && !m_pVehicle->IsPlayerPassenger())
		return;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);

	for (TParts::iterator partInfo = m_controlledParts.begin(); partInfo != m_controlledParts.end(); ++partInfo)
		partInfo->zpos = 0.f;
}

void CVehicleSeatActionShakeParts::StopUsing()
{
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

void CVehicleSeatActionShakeParts::Update(const float deltaTime)
{
	IVehicleMovement* pMovement = m_pVehicle->GetMovement();
	if (pMovement == NULL)
		return;

	SVehicleMovementStatusGeneral status;
	pMovement->GetStatus(&status);

	SVehicleMovementStatusWheeled wheeledStatus;
	pMovement->GetStatus(&wheeledStatus);

	float invTopSpeed = 1.f / (status.topSpeed + 1.f);
	float speedNorm = min(1.f, status.speed * invTopSpeed);
	float accelNorm = min(1.f, status.localAccel.GetLengthSquared() * sqr(invTopSpeed));
	float freqModifier = min(1.f, (0.5f * invTopSpeed + status.speed) * invTopSpeed * 0.667f);

	TParts::iterator partInstance = m_controlledParts.begin();
	SSharedParams::TPartInfosConst& partInfos = m_pSharedParams->partInfos;
	SSharedParams::TPartInfosConst::const_iterator partInfo = partInfos.begin();

	for (; partInfo != partInfos.end(); ++partInfo, ++partInstance)
	{
		IVehiclePart* pPart = m_pVehicle->GetPart(partInfo->partIndex);
		if (pPart)
		{
			float amp = partInfo->amplitudeUpDown * max(speedNorm, accelNorm);
			float ampRot = partInfo->amplitudeRot * speedNorm;
			float freq = partInfo->freq * freqModifier;
			partInstance->noiseUpDown.SetAmpFreq(amp, freq);
			partInstance->noiseRot.SetAmpFreq(ampRot, freq);
			float randPos = partInstance->noiseUpDown.Update(deltaTime);
			float randRot = partInstance->noiseRot.Update(deltaTime);
			float targetZ = partInfo->suspensionAmp * clamp_tpl(wheeledStatus.suspensionCompressionRate * partInfo->suspensionResponse, -1.f, +1.f);
			partInstance->zpos += (targetZ - partInstance->zpos) * approxOneExp(partInfo->suspensionSharpness * deltaTime);
			Matrix34 tm;
			tm.SetRotationZ(randRot);
			tm.SetTranslation(Vec3(0.f, 0.f, randPos + partInstance->zpos));
			pPart->SetLocalBaseTM(pPart->GetLocalInitialTM() * tm);
		}
	}
}

void CVehicleSeatActionShakeParts::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddContainer(m_controlledParts);
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionShakeParts);
