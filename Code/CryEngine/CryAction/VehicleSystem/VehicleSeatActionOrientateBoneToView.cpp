// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleSeatActionOrientateBoneToView.h"
#include "IVehicleSystem.h"
#include "Vehicle.h"
#include "VehicleSeat.h"
#include <CryExtension/CryCreateClassInstance.h>

CVehicleSeatActionOrientateBoneToView::CVehicleSeatActionOrientateBoneToView() : m_pSeat(NULL)
	, m_pVehicle(NULL)
	, m_MoveBoneId(-1)
	, m_LookBoneId(-1)
	, m_Sluggishness(0.f)
	, m_BoneOrientationAngles(0.f, 0.f, 0.f)
	, m_BoneSmoothingRate(0.f, 0.f, 0.f)
	, m_pAnimatedCharacter(NULL)
{
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_poseModifier);
}

bool CVehicleSeatActionOrientateBoneToView::Init(IVehicle* pVehicle, IVehicleSeat* pSeat, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;
	m_pSeat = pSeat;

	IDefaultSkeleton& rIDefaultSkeleton = *GetCharacterModelSkeleton();
	{
		if (table.haveAttr("MoveBone"))
		{
			const char* boneName = table.getAttr("MoveBone");
			m_MoveBoneId = rIDefaultSkeleton.GetJointIDByName(boneName);
		}

		if (table.haveAttr("LookBone"))
		{
			const char* boneName = table.getAttr("LookBone");
			m_LookBoneId = rIDefaultSkeleton.GetJointIDByName(boneName);
		}
	}

	if (table.haveAttr("Sluggishness"))
	{
		table.getAttr("Sluggishness", m_Sluggishness);
	}

	if (CVehicleParams baseOrientationTable = table.findChild("MoveBoneBaseOrientation"))
	{
		float x, y, z;
		baseOrientationTable.getAttr("x", x);
		baseOrientationTable.getAttr("y", y);
		baseOrientationTable.getAttr("z", z);

		m_BoneBaseOrientation = Quat::CreateRotationXYZ(Ang3(x, y, z));
	}
	else
	{
		m_BoneBaseOrientation.SetIdentity();
	}

	return true;
}

void CVehicleSeatActionOrientateBoneToView::Reset()
{
	// Enable PrePhysUpdate.
	m_pVehicle->GetGameObject()->EnablePrePhysicsUpdate(ePPU_Always);
};

void CVehicleSeatActionOrientateBoneToView::StartUsing(EntityId passengerId)
{
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
}

void CVehicleSeatActionOrientateBoneToView::StopUsing()
{
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
}

void CVehicleSeatActionOrientateBoneToView::PrePhysUpdate(const float deltaTime)
{
	Vec3 lookPos = GetCurrentLookPosition();
	Vec3 aimPos = GetDesiredAimPosition();

	const Ang3 desiredViewAngles = GetDesiredViewAngles(lookPos, aimPos);

	//Rotating between -Pi and Pi and need to catch the cross over from one to the other to make sure smoothing is in correct direction
	float yawDif = m_BoneOrientationAngles.z - desiredViewAngles.z;
	if (fabs(yawDif) > gf_PI)
	{
		m_BoneOrientationAngles.z += (float)__fsel(m_BoneOrientationAngles.z, -2.f * gf_PI, 2.f * gf_PI);
	}

	SmoothCD<Ang3>(m_BoneOrientationAngles, m_BoneSmoothingRate, deltaTime, desiredViewAngles, m_Sluggishness);

	IAnimationPoseModifierPtr modPtr = m_poseModifier;
	m_pVehicle->GetEntity()->GetCharacter(0)->GetISkeletonAnim()->PushPoseModifier(2, modPtr, "VehicleSeat");

	Quat vehicleRotation = m_pVehicle->GetEntity()->GetWorldRotation();

	Quat headOrientation;
	headOrientation.SetRotationXYZ(m_BoneOrientationAngles);

	Quat finalheadOrientation = vehicleRotation.GetInverted() * headOrientation * m_BoneBaseOrientation;
	m_poseModifier->PushOrientation(m_MoveBoneId, IAnimationOperatorQueue::eOp_Override, finalheadOrientation);
}

Ang3 CVehicleSeatActionOrientateBoneToView::GetDesiredViewAngles(const Vec3& lookPos, const Vec3& aimPos) const
{
	Vec3 forwardDir = (aimPos - lookPos).GetNormalized();
	Vec3 upDir = Vec3(0.f, 0.f, 1.f);
	Vec3 sideDir = forwardDir.Cross(upDir);
	sideDir.Normalize();
	upDir = sideDir.Cross(forwardDir);
	upDir.Normalize();

	Matrix34 matrix;
	matrix.SetFromVectors(sideDir, forwardDir, upDir, Vec3(0.f, 0.f, 0.f));

	Ang3 lookAngles;
	lookAngles.SetAnglesXYZ(matrix);

	return lookAngles;
}

Vec3 CVehicleSeatActionOrientateBoneToView::GetDesiredAimPosition() const
{
	IActor* pActor = m_pVehicle->GetDriver();
	if (pActor && pActor->IsClient())
	{
		const CCamera& camera = gEnv->pSystem->GetViewCamera();
		return camera.GetPosition() + (camera.GetViewdir() * 100.f);
	}

	return m_pVehicle->GetEntity()->GetWorldTM().GetColumn1();
}

Vec3 CVehicleSeatActionOrientateBoneToView::GetCurrentLookPosition() const
{
	ISkeletonPose* pSkeleton = GetSkeleton();
	CRY_ASSERT_MESSAGE(pSkeleton, "CVehicleSeatActionOrientateBoneToView::GetCurrentLookPosition - Couldn't get ISkeletonPose from vehicle entity");

	QuatT lookQuat = pSkeleton->GetAbsJointByID(m_LookBoneId);
	return m_pVehicle->GetEntity()->GetWorldPos() + lookQuat.t;
}

void CVehicleSeatActionOrientateBoneToView::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
}

IDefaultSkeleton* CVehicleSeatActionOrientateBoneToView::GetCharacterModelSkeleton() const
{
	if (ICharacterInstance* pCharacterInstance = m_pVehicle->GetEntity()->GetCharacter(0))
	{
		return &pCharacterInstance->GetIDefaultSkeleton();
	}

	return NULL;
}
ISkeletonPose* CVehicleSeatActionOrientateBoneToView::GetSkeleton() const
{
	if (ICharacterInstance* pCharacterInstance = m_pVehicle->GetEntity()->GetCharacter(0))
	{
		return pCharacterInstance->GetISkeletonPose();
	}

	return NULL;
}

DEFINE_VEHICLEOBJECT(CVehicleSeatActionOrientateBoneToView);
