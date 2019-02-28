// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a suspension part

   -------------------------------------------------------------------------
   History:
   - 16:11:2011: Created by Stan Fichele

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartSubPart.h"
#include "VehiclePartSuspensionPart.h"
#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

CVehiclePartSuspensionPart::CVehiclePartSuspensionPart()
	: m_animatedRoot(nullptr)
	, m_targetPart(nullptr)
	, m_initialRot(ZERO)
	, m_targetOffset(ZERO)
	, m_pos0(ZERO)
	, m_direction0(ZERO)
	, m_invLength0(0.0f)
	, m_jointId(0)
	, m_mode(0)
	, m_ikFlags(0)
{
}

CVehiclePartSuspensionPart::~CVehiclePartSuspensionPart()
{
}

bool CVehiclePartSuspensionPart::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* pParent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!inherited::Init(pVehicle, table, pParent, initInfo, partType))
		return false;

	m_animatedRoot = CAST_VEHICLEOBJECT(CVehiclePartAnimated, GetParent(true));
	m_jointId = -1;
	m_ikFlags = 0;

	if (m_animatedRoot)
	{
		if (CVehicleParams subPartTable = table.findChild("SubPart"))
		{
			// We need to remove this part from the animated root, otherwise we have two parts
			// NB: for now we are not doing anything with the physics - infact its preferable
			// if we dont have physics on suspension arms!
			const char* geoName = subPartTable.getAttr("geometryname");
			ICharacterInstance* pCharInstance = m_animatedRoot->GetEntity()->GetCharacter(m_animatedRoot->GetSlot());
			if (pCharInstance)
			{
				IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
				ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
				m_jointId = rIDefaultSkeleton.GetJointIDByName(geoName);
				pSkeletonPose->SetStatObjOnJoint(m_jointId, NULL);
			}
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CVehiclePartSuspensionPart: needs to have an AnimatedPart as a parent!");
		return false;
	}

	CVehicleParams ikTable = table.findChild("IK");
	if (!ikTable)
		return false;

	const char* targetPartName = ikTable.getAttr("target");
	m_targetPart = static_cast<CVehiclePartBase*>(pVehicle->GetPart(targetPartName));
	if (m_targetPart == NULL)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CVehiclePartSuspensionPart: couldn't find target part: '%s'", targetPartName);
		return false;
	}

	// Set up the target
	m_targetOffset.zero();
	const char* targetHelper = ikTable.getAttr("targetHelper");
	if (targetHelper && targetHelper[0])
	{
		if (IVehicleHelper* pHelper = m_pVehicle->GetHelper(targetHelper))
		{
			// NB: this is in vehicle space, and needs translating in PostInit()
			m_targetOffset = pHelper->GetLocalTM().GetTranslation();
			m_ikFlags |= k_flagTargetHelper;
		}
	}
	Vec3 offset(0);
	ikTable.getAttr("offset", offset);
	m_targetOffset += offset;

	m_mode = k_modeStretch;
	const char* mode = ikTable.getAttr("mode");
	if (strcmp(mode, "rotate") == 0)
		m_mode = k_modeRotate;
	if (strcmp(mode, "stretch") == 0)
		m_mode = k_modeStretch;
	if (strcmp(mode, "snap") == 0)
		m_mode = k_modeSnapToEF;

	bool bIgnoreTargetRotation = 0;
	ikTable.getAttr("ignoreTargetRotation", bIgnoreTargetRotation);
	if (bIgnoreTargetRotation)
		m_ikFlags |= k_flagIgnoreTargetRotation;

	return true;
}

void CVehiclePartSuspensionPart::PostInit()
{
	const Matrix34& tm = GetLocalTM(false);
	const Matrix34& targetTm = m_targetPart->GetLocalTM(false);
	if (m_ikFlags & k_flagTargetHelper)
		m_targetOffset = targetTm.GetInverted() * m_targetOffset;
	m_pos0 = tm.GetTranslation();
	Vec3 targetPos = targetTm * m_targetOffset;
	m_direction0 = targetPos - m_pos0;
	float length = m_direction0.GetLength();
	if (length > 1e-2f)
	{
		m_invLength0 = 1.f / length;
		m_direction0 = m_direction0 / length;
	}
	else
	{
		m_invLength0 = 0.f;
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CVehiclePartSuspensionPart: inavlid suspension IK setup target is TOO close");
	}
	m_initialRot = Quat(tm);
}

void CVehiclePartSuspensionPart::OnEvent(const SVehiclePartEvent& event)
{
	inherited::OnEvent(event);
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
}

void CVehiclePartSuspensionPart::Reset()
{
	inherited::Reset();
}

void CVehiclePartSuspensionPart::Release()
{
	inherited::Release();
}

bool CVehiclePartSuspensionPart::ChangeState(EVehiclePartState state, int flags)
{
	if (!inherited::ChangeState(state, flags))
		return false;

	// Remove the statobj from the cga joint (again)
	if (m_animatedRoot && m_jointId >= 0)
	{
		ICharacterInstance* pCharInstance = m_animatedRoot->GetEntity()->GetCharacter(m_animatedRoot->GetSlot());
		if (pCharInstance)
		{
			ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose();
			pSkeletonPose->SetStatObjOnJoint(m_jointId, NULL);
		}
	}
	return true;
}

void CVehiclePartSuspensionPart::Physicalize()
{
	inherited::Physicalize();
}

void CVehiclePartSuspensionPart::Update(const float frameTime)
{
	inherited::Update(frameTime);

	const Matrix34& parentTm = m_pParentPart->GetLocalTM(false);
	const Matrix34& targetTm = m_targetPart->GetLocalTM(false);

	Vec3 pos = parentTm * m_pos0;
	Vec3 targetPos = (m_ikFlags & k_flagIgnoreTargetRotation) ? (targetTm.GetColumn3() + m_targetOffset) : (targetTm * m_targetOffset);
	Vec3 dir = targetPos - pos;
	float length = dir.GetLength();
	if (length > 1e-2f)
	{
		Matrix33 rot = Matrix33::CreateRotationV0V1(m_direction0, dir * (1.f / length));
		Matrix33 partRot = rot * Matrix33(m_initialRot);

		if (m_mode == k_modeRotate || m_mode == k_modeSnapToEF)
		{
			if (m_mode == k_modeSnapToEF)
			{
				pos = targetPos - rot * m_direction0;
			}
			Matrix34 tm(partRot, pos);
			SetLocalTM(tm);
		}
		else if (m_mode == k_modeStretch)
		{
			const float scale = length * m_invLength0;
			const Vec3 z = m_direction0;
			const Vec3 sz = m_direction0 * (scale - 1.f);
			Matrix33 scaleM;
			scaleM.m00 = 1.f + sz.x * z.x;
			scaleM.m01 = sz.y * z.x;
			scaleM.m02 = sz.z * z.x;
			scaleM.m10 = sz.x * z.y;
			scaleM.m11 = 1.f + sz.y * z.y;
			scaleM.m12 = sz.z * z.y;
			scaleM.m20 = sz.x * z.z;
			scaleM.m21 = sz.y * z.z;
			scaleM.m22 = 1.f + sz.z * z.z;
			Matrix34 tm(partRot * scaleM, pos);
			SetLocalTM(tm);
		}
	}

#if !defined(_RELEASE)
	if (VehicleCVars().v_debugSuspensionIK)
	{
		IRenderAuxGeom* pAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		SAuxGeomRenderFlags flags = pAuxGeom->GetRenderFlags();
		SAuxGeomRenderFlags oldFlags = pAuxGeom->GetRenderFlags();
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pAuxGeom->SetRenderFlags(flags);

		ColorB colRed(255, 0, 0, 255);
		ColorB colBlue(0, 0, 255, 255);
		ColorB colWhite(255, 255, 255, 255);
		ColorB colGreen(0, 255, 0, 255);

		pos = m_pVehicle->GetEntity()->GetWorldTM() * pos;
		targetPos = m_pVehicle->GetEntity()->GetWorldTM() * targetPos;
		pAuxGeom->DrawSphere(pos, 0.02f, colGreen);
		pAuxGeom->DrawSphere(targetPos, 0.02f, colBlue);
		pAuxGeom->DrawLine(pos, colWhite, targetPos, colWhite);
	}
#endif
}

DEFINE_VEHICLEOBJECT(CVehiclePartSuspensionPart);
