// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleSeatActionWeaponsBone.h"
#include "VehicleUtils.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include <CryExtension/CryCreateClassInstance.h>

CVehicleSeatActionWeaponsBone::CVehicleSeatActionWeaponsBone()
	: m_currentMovementRotation(IDENTITY)
	, m_pSkeletonPose(NULL)
	, m_positionBoneId(-1)
	, m_forwardOffset(0.f)
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_poseModifier);
}

CVehicleSeatActionWeaponsBone::~CVehicleSeatActionWeaponsBone()
{
	m_pVehicle->UnregisterVehicleEventListener(this);
}

bool CVehicleSeatActionWeaponsBone::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	if (!CVehicleSeatActionWeapons::Init(pVehicle, pSeat, table))
		return false;

	if (ICharacterInstance* pCharacterInstance = m_pVehicle->GetEntity()->GetCharacter(0))
	{
		IDefaultSkeleton& rIDefaultSkeleton = pCharacterInstance->GetIDefaultSkeleton();
		{
			if (table.haveAttr("PositionBone"))
			{
				const char* boneName = table.getAttr("PositionBone");
				m_positionBoneId = rIDefaultSkeleton.GetJointIDByName(boneName);

				if (m_positionBoneId >= 0)
				{
					m_pSkeletonPose = pCharacterInstance->GetISkeletonPose();
				}
			}

			if (table.haveAttr("ForwardOffset"))
			{
				table.getAttr("ForwardOffset", m_forwardOffset);
			}
		}
	}

	//SVehicleWeapon& weapon = GetVehicleWeapon();
	//IEntity* pWeaponEntity = gEnv->pEntitySystem->GetEntity(weapon.weaponEntityId);
	//if(pWeaponEntity)
	//{
	//	pVehicle->GetEntity()->AttachChild(pWeaponEntity);
	//}

	pVehicle->RegisterVehicleEventListener(this, "CVehicleSeatActionWeaponsBone");

	return true;
}

bool CVehicleSeatActionWeaponsBone::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit)
{
	Vec3 pos, dir;
	if (CalcFiringPosDir(pos, &dir, pFireMode))
	{
		m_currentMovementRotation.SetIdentity();
		hit = pos + (dir * 20.f);
		return true;
	}
	return false;
}

bool CVehicleSeatActionWeaponsBone::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);
	return CalcFiringPosDir(pos);
}

bool CVehicleSeatActionWeaponsBone::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	IActor* pDriver = m_pVehicle->GetDriver();
	bool isLocal = pDriver && pDriver->IsClient();

	if (isLocal)
	{
		if (!probableHit.IsZero())
		{
			dir = (probableHit - firingPos).GetNormalized();
			return true;
		}
		else
		{
			Vec3 tempPos;
			if (CalcFiringPosDir(tempPos, &dir, pFireMode))
			{
				m_currentMovementRotation.SetIdentity();
				return true;
			}
		}

		return CVehicleSeatActionWeapons::GetFiringDir(weaponId, pFireMode, dir, probableHit, firingPos);
	}
	else
	{
		dir = (m_fireTarget - firingPos).normalize();
		return true;
	}

}

void CVehicleSeatActionWeaponsBone::UpdateWeaponTM(SVehicleWeapon& weapon)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_pSkeletonPose)
	{
		if (IWeapon* pWeapon = GetIWeapon(weapon))
		{
			IEntity* pEntityWeapon = GetEntity(weapon);

			IActor* pDriver = m_pVehicle->GetDriver();
			bool isLocal = pDriver && pDriver->IsClient();

			Vec3 pos, dir;
			const Vec3 prevFireTarget(m_fireTarget);
			if (CalcFiringPosDir(pos, &dir, pWeapon->GetFireMode(pWeapon->GetCurrentFireMode())))
			{
			}
			else if (isLocal)
			{
				const CCamera& cam = GetISystem()->GetViewCamera();
				pos = cam.GetPosition();
				dir = cam.GetViewdir();
			}
			else
			{
				const QuatT entLoc(m_pVehicle->GetEntity()->GetWorldTM());
				const QuatT boneWorld(entLoc * m_pSkeletonPose->GetAbsJointByID(m_positionBoneId));
				pos = boneWorld.t;
				dir = boneWorld.GetColumn1();
			}

			m_fireTarget = pos + (dir * 50.f);
			if ((prevFireTarget - m_fireTarget).GetLengthSquared() > 0.01f)
			{
				m_pSeat->ChangedNetworkState(CVehicle::ASPECT_SEAT_ACTION);
			}

			const Quat rotation = Quat::CreateRotationVDir(dir.GetNormalized(), 0.f);
			Matrix34 worldTM = Matrix34::Create(Vec3(1.f, 1.f, 1.f), rotation, pos);
			pEntityWeapon->SetWorldTM(worldTM);

#if ENABLE_VEHICLE_DEBUG
			if (VehicleCVars().v_draw_tm)
				VehicleUtils::DrawTM(worldTM);
#endif
		}
	}
}

void CVehicleSeatActionWeaponsBone::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehicleSeatActionWeapons::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() == eST_Network && aspects & CVehicle::ASPECT_SEAT_ACTION)
	{
		ser.Value("m_fireTarget", m_fireTarget, 'wrl3');
	}
}

void CVehicleSeatActionWeaponsBone::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CVehicleSeatActionWeapons::OnVehicleEvent(event, params);

	if (event == eVE_VehicleMovementRotation)
	{
		const Quat currentRotation(params.fParam, params.vParam);
		m_currentMovementRotation = currentRotation;
	}
}

bool CVehicleSeatActionWeaponsBone::CalcFiringPosDir(Vec3& rPos, Vec3* pDir, const IFireMode* pFireMode) const
{
	if (m_pSkeletonPose)
	{
		const QuatT entLoc(m_pVehicle->GetEntity()->GetWorldTM());
		const QuatT& boneLocal = m_pSkeletonPose->GetAbsJointByID(m_positionBoneId);
		const QuatT boneWorld(entLoc * m_currentMovementRotation.GetInverted() * boneLocal);

		if (pDir)
		{
			Vec3 dir(boneWorld.GetColumn1().GetNormalized());
			if (pFireMode)
			{
				pFireMode->ApplyAutoAim(dir, boneWorld.t);
				dir = pFireMode->ApplySpread(dir, pFireMode->GetSpread()).GetNormalized();
			}
			*pDir = dir;
		}

		rPos = boneWorld.t + (boneWorld.GetColumn1() * m_forwardOffset);
		return true;
	}
	return false;
}
