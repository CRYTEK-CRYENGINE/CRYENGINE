// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which uses animated characters

   -------------------------------------------------------------------------
   History:
   - 24:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehicleComponent.h"
#include "VehiclePartSubPartWheel.h"
#include "VehiclePartTread.h"
#include "VehicleUtils.h"
#include <CryRenderer/IShader.h>
#include <CryExtension/CryCreateClassInstance.h>

//------------------------------------------------------------------------
CVehiclePartTread::CVehiclePartTread()
{
	m_pCharInstance = NULL;
	m_slot = -1;
	m_lastWheelIndex = 0;
	m_pMaterial = 0;
	m_damageRatio = 0.0f;

	m_uvSpeedMultiplier = 0.0f;
	m_bForceSetU = true;
	m_wantedU = 0.0f;
	m_currentU = m_wantedU + 1.0f;

	m_pShaderResources = NULL;
	m_pMaterial = NULL;
	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_operatorQueue);
}

//------------------------------------------------------------------------
bool CVehiclePartTread::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Tread))
		return false;

	CVehicleParams subTable = table.findChild("Tread"); // Tread subtable
	if (!subTable)
		return false;

	string filename = subTable.getAttr("filename");
	if (filename.empty())
		return false;

	subTable.getAttr("uvSpeedMultiplier", m_uvSpeedMultiplier);

	if (table.haveAttr("component"))
	{
		if (CVehicleComponent* pComponent = static_cast<CVehicleComponent*>(m_pVehicle->GetComponent(table.getAttr("component"))))
			pComponent->AddPart(this);
	}

	m_slot = GetEntity()->LoadCharacter(m_slot, filename);

	m_pCharInstance = GetEntity()->GetCharacter(m_slot);
	if (!m_pCharInstance)
		return false;

	if (subTable.haveAttr("materialName"))
	{
		string materialName = subTable.getAttr("materialName");
		materialName.MakeLower();

		IMaterial* pMaterial = 0;
		const char* subMtlName = 0;
		int subMtlSlot = -1;

		// find tread material
		IEntityRender* pIEntityRender = GetEntity()->GetRenderInterface();

		{
			pMaterial = pIEntityRender->GetRenderMaterial(m_slot);
			if (pMaterial)
			{
				// if matname doesn't fit, look in submaterials
				if (string(pMaterial->GetName()).MakeLower().find(materialName) == string::npos)
				{
					for (int i = 0; i < pMaterial->GetSubMtlCount(); ++i)
					{
						if (string(pMaterial->GetSubMtl(i)->GetName()).MakeLower().find(materialName) != string::npos)
						{
							subMtlName = pMaterial->GetSubMtl(i)->GetName();
							subMtlSlot = i;
							break;
						}
					}
				}
			}
		}

		if (pMaterial)
		{
			// clone
			IMaterial* pCloned = pMaterial->GetMaterialManager()->CloneMultiMaterial(pMaterial, subMtlName);
			if (pCloned)
			{
				pIEntityRender->SetSlotMaterial(m_slot, pCloned);
				pMaterial = pCloned;
				m_pMaterial = pMaterial;
			}

			if (subMtlSlot > -1)
				m_pShaderResources = pMaterial->GetShaderItem(subMtlSlot).m_pShaderResources;
			else
				m_pShaderResources = pMaterial->GetShaderItem().m_pShaderResources;
		}

		if (m_pShaderResources)
		{
			for (int i = 0; i < EFTT_MAX; ++i)
			{
				if (!m_pShaderResources->GetTexture(i))
					continue;

				SEfTexModificator& modif = *m_pShaderResources->GetTexture(i)->AddModificator();

				modif.SetMember("m_eMoveType[0]", 1.0f);  // ETMM_Fixed: u = m_OscRate[0] + sourceU

				modif.SetMember("m_OscRate[0]", 0.0f);
			}
		}
	}

	char wheelName[256];

	IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
	for (int i = 0; i < rIDefaultSkeleton.GetJointCount(); ++i)
	{
		const char* boneName = rIDefaultSkeleton.GetJointNameByID(i);
		if (0 != boneName)
		{
			// extract wheel name
			const size_t len = strcspn(boneName, "_");
			if (len < strlen(boneName) && len < sizeof(wheelName))
			{
				cry_strcpy(wheelName, boneName, len);

				CVehiclePartSubPartWheel* pWheel = (CVehiclePartSubPartWheel*)m_pVehicle->GetPart(wheelName);
				if (pWheel)
				{
					SWheelInfo wheelInfo;
					wheelInfo.slot = pWheel->m_slot;
					wheelInfo.jointId = i;
					wheelInfo.pWheel = pWheel;
					m_wheels.push_back(wheelInfo);

					m_lastWheelIndex = pWheel->GetWheelIndex();
				}
			}
		}
	}

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	m_state = eVGS_Default;
	return true;
}

//------------------------------------------------------------------------
void CVehiclePartTread::Reset()
{
	m_bForceSetU = false;
	m_wantedU = 0.0f;
	m_currentU = m_wantedU + 1.0f;
	UpdateU();

	if (m_pCharInstance)
		m_pCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(0);

	CVehiclePartBase::Reset();
}

//------------------------------------------------------------------------
bool CVehiclePartTread::ChangeState(EVehiclePartState state, int flags /* =0 */)
{
	const EVehiclePartState previousState = m_state;
	const bool stateChanged = CVehiclePartBase::ChangeState(state, flags);
	m_state = state;

	if (stateChanged)
	{
		if (m_state == IVehiclePart::eVGS_Destroyed)
		{
			Hide(true);
		}
		else if (previousState == IVehiclePart::eVGS_Destroyed)
		{
			Hide(false);
		}

		if (const auto pMovement = m_pVehicle->GetMovement())
		{
			if (m_state == IVehiclePart::eVGS_Damaged1)
			{
				SVehicleMovementEventParams params;
				pMovement->OnEvent(IVehicleMovement::eVME_TireBlown, params);
			}
			else if (m_state == eVGS_Default)
			{
				SVehicleMovementEventParams params;
				pMovement->OnEvent(IVehicleMovement::eVME_TireRestored, params);
			}
		}
	}

	return stateChanged;
}

//------------------------------------------------------------------------
void CVehiclePartTread::InitGeometry()
{
}

//------------------------------------------------------------------------
void CVehiclePartTread::Physicalize()
{
	Update(1.0f);
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartTread::GetLocalTM(bool relativeToParentPart, bool forced)
{
	return GetEntity()->GetSlotLocalTM(m_slot, relativeToParentPart);
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartTread::GetWorldTM()
{
	return GetEntity()->GetSlotWorldTM(m_slot);
}

//------------------------------------------------------------------------
const AABB& CVehiclePartTread::GetLocalBounds()
{
	if (m_pCharInstance)
	{
		m_bounds = m_pCharInstance->GetAABB();
		m_bounds.SetTransformedAABB(GetEntity()->GetSlotLocalTM(m_slot, false), m_bounds);
	}
	else
		GetEntity()->GetLocalBounds(m_bounds);

	return m_bounds;
}

//------------------------------------------------------------------------
void CVehiclePartTread::Update(const float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!m_pCharInstance)
		return;

	if (m_bForceSetU)
	{
		m_bForceSetU = false;
		m_currentU = m_wantedU + 1.0f;
		UpdateU();
	}

	ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();

	pSkeletonPose->SetForceSkeletonUpdate(0);

	if (VehicleCVars().v_staticTreadDeform == 0 && m_pVehicle->GetStatus().speed < 0.001f)
	{
		return;
	}

	if (frameTime > 0.f &&
	    (m_damageRatio >= 1.f || !m_pVehicle->GetGameObject()->IsProbablyVisible() || m_pVehicle->IsProbablyDistant()))
	{
		return;
	}

	// we need a tread update in next frame
	pSkeletonPose->SetForceSkeletonUpdate(1);
	m_pVehicle->NeedsUpdate();

	// animate the UV texture according to the wheels speed
	if (m_uvSpeedMultiplier != 0.0f && frameTime > 0.0f)
	{
		IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
		pe_status_wheel wheelStatus;
		wheelStatus.iWheel = m_lastWheelIndex;

		if (pPhysics && pPhysics->GetStatus(&wheelStatus) != 0)
		{
			m_wantedU += m_uvSpeedMultiplier * (wheelStatus.w * wheelStatus.r * frameTime);
			m_wantedU -= std::floor(m_wantedU);
			UpdateU();
		}
	}

	// deform the tread to follow the wheels
	QuatT absRoot = pSkeletonPose->GetAbsJointByID(0);

	for (TWheelInfoVector::const_iterator ite = m_wheels.begin(), end = m_wheels.end(); ite != end; ++ite)
	{
		const SWheelInfo& wheelInfo = *ite;
		const Matrix34& slotTM = GetEntity()->GetSlotLocalTM(wheelInfo.slot, true);
		VALIDATE_MAT(slotTM);

		if (m_operatorQueue)
		{
			m_operatorQueue->PushPosition(wheelInfo.jointId,
			                              IAnimationOperatorQueue::eOp_Override, slotTM.GetTranslation());
		}

#if ENABLE_VEHICLE_DEBUG
		if (VehicleCVars().v_debugdraw == 4)
		{
			Vec3 local = GetEntity()->GetWorldTM().GetInverted() * slotTM.GetTranslation();
			gEnv->pAuxGeomRenderer->DrawSphere(GetEntity()->GetWorldTM() * (local + Vec3((float)sgn(local.x) * 0.5f, 0.f, 0.f)), 0.1f, ColorB(0, 0, 255, 255));
		}
#endif
	}
	ISkeletonAnim* pSkeletonAnim = m_pCharInstance->GetISkeletonAnim();
	pSkeletonAnim->PushPoseModifier(VEH_ANIM_POSE_MODIFIER_LAYER, m_operatorQueue, "VehiclePartAnimatedJoint");
}

//------------------------------------------------------------------------
void CVehiclePartTread::UpdateU()
{
	if (!m_pShaderResources || m_currentU == m_wantedU)
	{
		return;
	}

	m_currentU = m_wantedU;

	for (int i = 0; i < EFTT_MAX; ++i)
	{
		if (!m_pShaderResources->GetTexture(i))
		{
			continue;
		}

		SEfTexModificator& modif = *m_pShaderResources->GetTexture(i)->AddModificator();

		modif.SetMember("m_OscRate[0]", m_wantedU);
	}
}

DEFINE_VEHICLEOBJECT(CVehiclePartTread);
