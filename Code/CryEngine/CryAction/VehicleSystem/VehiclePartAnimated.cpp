// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Implements a part for vehicles which uses animated characters

#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehicleUtils.h"
#include "VehicleComponent.h"


//------------------------------------------------------------------------
CVehiclePartAnimated::CVehiclePartAnimated()
	: m_pCharInstanceDestroyed(NULL)
	, m_pCharInstance(NULL)
	, m_ignoreDestroyedState(false)
{
	m_hullMatId[0] = m_hullMatId[1] = -1;
	m_lastAnimLayerAssigned = 0;
	m_iRotChangedFrameId = 0;
	m_serializeForceRotationUpdate = false;
	m_initialiseOnChangeState = true;
}

//------------------------------------------------------------------------
CVehiclePartAnimated::~CVehiclePartAnimated()
{
	if (m_pCharInstanceDestroyed)
	{
		m_pCharInstanceDestroyed->Release();
		m_pCharInstanceDestroyed = 0;
	}
}

//------------------------------------------------------------------------
bool CVehiclePartAnimated::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_Animated))
		return false;

	if (CVehicleParams animatedTable = table.findChild("Animated"))
	{
		animatedTable.getAttr("ignoreDestroyedState", m_ignoreDestroyedState);
	}

	InitGeometry();

	m_state = eVGS_Default;

	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::Release()
{
	if (m_pCharInstance)
	{
		if (ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose())
			pSkeletonPose->SetPostProcessCallback(NULL, NULL);

		m_pCharInstance = 0;
	}

	if (m_pCharInstanceDestroyed)
	{
		m_pCharInstanceDestroyed->Release();
		m_pCharInstanceDestroyed = 0;
	}

	if (m_slot != -1)
		m_pVehicle->GetEntity()->FreeSlot(m_slot);

	CVehiclePartBase::Release();
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::Reset()
{
	CVehiclePartBase::Reset();

	SetDrivingProxy(false);

	if (m_slot > -1 && m_pCharInstance)
	{
		ISkeletonAnim* pSkeletonAnim = m_pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
		CRY_ASSERT(pSkeletonPose);
		IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
		pSkeletonAnim->StopAnimationsAllLayers();
		pSkeletonPose->SetDefaultPose();
		pSkeletonPose->SetForceSkeletonUpdate(0);

		for (int i = 0; i < rIDefaultSkeleton.GetJointCount(); ++i)
			pSkeletonPose->SetMaterialOnJoint(i, NULL);
	}

	m_iRotChangedFrameId = 0;
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::OnEvent(const SVehiclePartEvent& event)
{
	switch (event.type)
	{
	case eVPE_Damaged:
		{
			float old = m_damageRatio;
			m_damageRatio = event.fparam;

			if (m_damageRatio >= 1.0f && old < 1.f)
			{
				EVehiclePartState state = GetStateForDamageRatio(m_damageRatio);
				ChangeState(state, eVPSF_Physicalize);
			}

		}
		break;
	case eVPE_Repair:
		{
			float old = m_damageRatio;
			m_damageRatio = max(0.f, event.fparam);

			if (eVPT_Base == GetType() && m_damageRatio <= COMPONENT_DAMAGE_LEVEL_RATIO && m_hideCount == 0)
				Hide(false);

			if (m_damageRatio <= COMPONENT_DAMAGE_LEVEL_RATIO && old > COMPONENT_DAMAGE_LEVEL_RATIO)
				ChangeState(eVGS_Default, eVPSF_Physicalize);
		}
		break;
	case eVPE_StartUsing:
		{
			m_users++;

			if (m_users == 1)
			{
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
			}
		}
		break;
	case eVPE_StopUsing:
		{
			if (m_users > 0)
				--m_users;

			if (m_users <= 0)
			{
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
			}
		}
		break;
	case eVPE_GotDirty:
		{
			break;
		}
	case eVPE_Fade:
		{
			if (event.bparam)
				m_hideMode = eVPH_FadeOut;
			else
				m_hideMode = eVPH_FadeIn;

			m_hideTimeCount = event.fparam;
			m_hideTimeMax = event.fparam;

			m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
		}
	break;
	case eVPE_Hide:
		{
			Hide(event.bparam);
		}
		break;
	case eVPE_BlockRotation:
		{
			m_isRotationBlocked = event.bparam;
		}
		break;
	case eVPE_DriverEntered:
		{
			if (!m_pVehicle->IsFlipped())
				SetDrivingProxy(true);
		}
		break;
	case eVPE_DriverLeft:
		{
			SetDrivingProxy(false);
		}
		break;
	case eVPE_FlippedOver:
		{
			if (event.bparam)
				SetDrivingProxy(false);
			else if (m_pVehicle->GetDriver())
				SetDrivingProxy(true);
		}
		break;
	}
}

//------------------------------------------------------------------------
IVehiclePart::EVehiclePartState CVehiclePartAnimated::GetStateForDamageRatio(float ratio)
{
	if (ratio >= 1.f && m_pVehicle->IsDestroyable())
		return eVGS_Destroyed;

	return eVGS_Default;
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::SetMaterial(IMaterial* pMaterial)
{
	if (m_pClonedMaterial)
		m_pClonedMaterial = NULL;

	GetEntity()->SetMaterial(pMaterial);
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::InitGeometry()
{
	if (!m_pSharedParameters->m_filename.empty())
	{
		m_slot = GetEntity()->LoadCharacter(m_slot, m_pSharedParameters->m_filename.c_str());
		m_pCharInstance = GetEntity()->GetCharacter(m_slot);

		if (m_pCharInstance)
		{
			if (m_pCharInstance->GetIMaterial() && !GetEntity()->GetMaterial())
			{
				SetMaterial(m_pCharInstance->GetIMaterial());
			}

			ISkeletonAnim* pSkeletonAnim = m_pCharInstance->GetISkeletonAnim();
			ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();

			if (pSkeletonAnim)
				pSkeletonAnim->StopAnimationsAllLayers();

			if (m_hideCount == 0)
				GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_RENDER);
			else
				GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) & ~(ENTITY_SLOT_RENDER | ENTITY_SLOT_RENDER_NEAREST));

#if ENABLE_VEHICLE_DEBUG
			if (IsDebugParts())
			{
				CryLog("joint transformations for %s", m_pCharInstance->GetFilePath());

				for (int i = 0; i < rIDefaultSkeleton.GetJointCount(); ++i)
					VehicleUtils::LogMatrix(rIDefaultSkeleton.GetJointNameByID(i), Matrix34(pSkeletonPose->GetRelJointByID(i)));
			}
#endif
		}

		if (!m_pCharInstanceDestroyed && !m_pSharedParameters->m_filenameDestroyed.empty())
			m_pCharInstanceDestroyed = gEnv->pCharacterManager->CreateInstance(m_pSharedParameters->m_filenameDestroyed.c_str());

		if (m_pCharInstanceDestroyed)
		{
			m_pCharInstanceDestroyed->AddRef();

#if ENABLE_VEHICLE_DEBUG
			if (IsDebugParts())
			{
				CryLog("joint transformations for %s", m_pCharInstanceDestroyed->GetFilePath());

				ISkeletonPose* pSkeletonDestroyed = m_pCharInstanceDestroyed->GetISkeletonPose();
				IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstanceDestroyed->GetIDefaultSkeleton();
				for (int i = 0; i < rIDefaultSkeleton.GetJointCount(); ++i)
					VehicleUtils::LogMatrix(rIDefaultSkeleton.GetJointNameByID(i), Matrix34(pSkeletonDestroyed->GetRelJointByID(i)));
			}
#endif
		}
	}

	if (m_pSharedParameters->m_isPhysicalized && m_slot > -1)
		GetEntity()->UnphysicalizeSlot(m_slot);

	if (m_pCharInstance)
	{
		m_pCharInstance->GetISkeletonAnim()->StopAnimationsAllLayers();

		if (m_hideCount == 0)
			GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_RENDER);
		else
			GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) & ~(ENTITY_SLOT_RENDER | ENTITY_SLOT_RENDER_NEAREST));
	}

	// Disable hand-placed (static) decals on vehicles
	GetEntity()->SetFlags(GetEntity()->GetFlags() | ENTITY_FLAG_NO_DECALNODE_DECALS);
}

//------------------------------------------------------------------------
IStatObj* CVehiclePartAnimated::GetGeometryForState(CVehiclePartAnimatedJoint* pPart, EVehiclePartState state)
{
	string name;
	pPart->GetGeometryName(state, name);

	IStatObj* pStatObj = 0;

	if (state > eVGS_Default)
	{
		if (pPart->m_pDestroyedGeometry)
		{
			pStatObj = pPart->m_pDestroyedGeometry;
		}
		else
		{
			IDefaultSkeleton* pIDefaultSkeleton = m_pCharInstanceDestroyed ? &m_pCharInstanceDestroyed->GetIDefaultSkeleton() : 0;
			if (pIDefaultSkeleton)
			{
				int jointId = pIDefaultSkeleton->GetJointIDByName(name.c_str());

				ISkeletonPose* pSkeletonPose = m_pCharInstanceDestroyed->GetISkeletonPose();
				if (jointId != -1)
					pStatObj = pSkeletonPose->GetStatObjOnJoint(jointId);
			}
		}
	}
	else
	{
		TStringStatObjMap::const_iterator it = m_intactStatObjs.find(name.c_str());
		if (it != m_intactStatObjs.end())
			pStatObj = it->second;
	}

	return pStatObj;
}

//------------------------------------------------------------------------
IStatObj* CVehiclePartAnimated::GetDestroyedGeometry(const char* pJointName, unsigned int index)
{
	if (pJointName[0] && m_pCharInstanceDestroyed)
	{
		IDefaultSkeleton& rICharacterModelSkeletonDestroyed = m_pCharInstanceDestroyed->GetIDefaultSkeleton();
		ISkeletonPose* pSkeletonDestroyed = m_pCharInstanceDestroyed->GetISkeletonPose();
		CRY_ASSERT(pSkeletonDestroyed);

		char buffer[256];

		const char* pSuffix = !m_pSharedParameters->m_destroyedSuffix.empty() ? m_pSharedParameters->m_destroyedSuffix.c_str() : GetDestroyedGeometrySuffix(eVGS_Destroyed);

		if (index == 0)
		{
			cry_sprintf(buffer, "%s%s", pJointName, pSuffix);
		}
		else
		{
			cry_sprintf(buffer, "%s_debris_%u", pJointName, index);
		}

		buffer[sizeof(buffer) - 1] = '\0';

		int16 jointIdForDestroyed = rICharacterModelSkeletonDestroyed.GetJointIDByName(buffer);
		if (jointIdForDestroyed > -1)
		{
			return pSkeletonDestroyed->GetStatObjOnJoint(jointIdForDestroyed);
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartAnimated::GetDestroyedGeometryTM(const char* pJointName, unsigned int index)
{
	if (pJointName[0] && m_pCharInstanceDestroyed)
	{
		IDefaultSkeleton& rICharacterModelSkeletonDestroyed = m_pCharInstanceDestroyed->GetIDefaultSkeleton();
		ISkeletonPose* pSkeletonDestroyed = m_pCharInstanceDestroyed->GetISkeletonPose();
		CRY_ASSERT(pSkeletonDestroyed);

		char buffer[256];

		const char* pSuffix = !m_pSharedParameters->m_destroyedSuffix.empty() ? m_pSharedParameters->m_destroyedSuffix.c_str() : GetDestroyedGeometrySuffix(eVGS_Destroyed);

		if (index == 0)
		{
			cry_sprintf(buffer, "%s%s", pJointName, pSuffix);
		}
		else
		{
			cry_sprintf(buffer, "%s_debris_%u", pJointName, index);
		}

		buffer[sizeof(buffer) - 1] = '\0';

		int16 jointIdForDestroyed = rICharacterModelSkeletonDestroyed.GetJointIDByName(buffer);
		if (jointIdForDestroyed > -1)
			return Matrix34(pSkeletonDestroyed->GetAbsJointByID(jointIdForDestroyed));
	}

	Matrix34 identTM;
	identTM.SetIdentity();
	return identTM;
}

//------------------------------------------------------------------------
bool CVehiclePartAnimated::ChangeState(EVehiclePartState state, int flags)
{
	if ((state == eVGS_Default) && m_initialiseOnChangeState)
	{
		// Initialise!
		// Having to do this because of the way the glass code
		// swaps a cstatobj. The way the vehicle code stores its
		// statobj in m_intactStatObjs is going to need reviewing
		if (m_pCharInstance)
		{
			ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
			ISkeletonPose* pSkeletonPoseDestroyed = m_pCharInstanceDestroyed ? m_pCharInstanceDestroyed->GetISkeletonPose() : NULL;
			IDefaultSkeleton* pICharacterModelSkeletonDestroyed = m_pCharInstanceDestroyed ? &m_pCharInstanceDestroyed->GetIDefaultSkeleton() : NULL;
			if (pSkeletonPose)
			{
				const bool bDestroyedSkelExists = pSkeletonPoseDestroyed && pICharacterModelSkeletonDestroyed;
				for (uint32 i = 0; i < rIDefaultSkeleton.GetJointCount(); i++)
				{
					if (IStatObj* pStatObjIntact = pSkeletonPose->GetStatObjOnJoint(i))
					{
						const char* jointName = rIDefaultSkeleton.GetJointNameByID(i);

						if (m_intactStatObjs.find(CONST_TEMP_STRING(jointName)) == m_intactStatObjs.end())
						{
							m_intactStatObjs.insert(TStringStatObjMap::value_type(jointName, pStatObjIntact));
						}

						// tell the streaming engine to stream destroyed version together with non destroyed
						if (bDestroyedSkelExists && i < pICharacterModelSkeletonDestroyed->GetJointCount())
						{
							if (IStatObj* pStatObjIntactDestroyed = pSkeletonPoseDestroyed->GetStatObjOnJoint(i))
							{
								pStatObjIntact->SetStreamingDependencyFilePath(pStatObjIntactDestroyed->GetFilePath());
							}
						}
					}
				}
			}
		}
		m_initialiseOnChangeState = false;
	}

	bool change = CVehiclePartBase::ChangeState(state, flags);

	if (state == eVGS_Default && !change)
	{
		// need to restore state if one of the children is in higher state
		EVehiclePartState maxState = GetMaxState();

		if (maxState > m_state)
			change = true;
	}

	if (!change)
	{
		return false;
	}

	if (state == eVGS_Destroyed)
	{
		if (m_ignoreDestroyedState)
			return false;

		if (m_pCharInstance && m_pCharInstanceDestroyed)
		{
			ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
			IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
			if (pSkeletonPose)
			{
				IMaterial* pDestroyedMaterial = m_pVehicle->GetDestroyedMaterial();

				for (uint32 i = 0; i < rIDefaultSkeleton.GetJointCount(); i++)
				{
					if (IStatObj* pStatObjIntact = pSkeletonPose->GetStatObjOnJoint(i))
					{
						const char* jointName = rIDefaultSkeleton.GetJointNameByID(i);
						IStatObj* pStatObj = GetDestroyedGeometry(jointName);

						// sets new StatObj to joint, if null, removes it.
						// object whose name includes "proxy" are not removed.
						if (pStatObj || !strstr(jointName, "proxy"))
						{
							SetCGASlot(i, pStatObj);

							if (pStatObj && !pDestroyedMaterial)
							{
								if (IMaterial* pMaterial = pStatObj->GetMaterial())
									SetMaterial(pMaterial);
							}

#if ENABLE_VEHICLE_DEBUG
							if (IsDebugParts())
							{
								CryLog("swapping StatObj on joint %u (%s) -> %s", i, jointName, pStatObj ? pStatObj->GetGeoName() : "<NULL>");
							}
#endif
						}
					}
				}

				FlagSkeleton(pSkeletonPose, rIDefaultSkeleton);

				for (TStringVehiclePartMap::iterator ite = m_jointParts.begin(); ite != m_jointParts.end(); ++ite)
				{
					IVehiclePart* pPart = ite->second;
					pPart->ChangeState(state, flags | eVPSF_Physicalize);
				}

				CryCharAnimationParams animParams;
				animParams.m_nFlags |= CA_LOOP_ANIMATION;
				// pSkeleton->SetRedirectToLayer0(1);
				// pSkeleton->StartAnimation("Default",0,  0,0, animParams);  // [MR: commented out on Ivos request]

				if (pDestroyedMaterial)
				{
					SetMaterial(pDestroyedMaterial);
				}
			}
		}
	}
	else if (state == eVGS_Default)
	{
		if (m_pCharInstance && m_pCharInstanceDestroyed)
		{
			// reset material (in case we replaced it with the destroyed material)
			IMaterial* pMaterial = m_pVehicle->GetPaintMaterial();
			if (!pMaterial)
			{
				// no paint, so revert to the material already set on the character
				pMaterial = m_pCharInstance->GetIMaterial();
			}
			if (pMaterial)
			{
				SetMaterial(pMaterial);
			}

			IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
			{
				for (TStringStatObjMap::iterator ite = m_intactStatObjs.begin(); ite != m_intactStatObjs.end(); ++ite)
				{
					const string& jointName = ite->first;
					IStatObj* pStatObj = ite->second;

					int16 jointId = rIDefaultSkeleton.GetJointIDByName(jointName.c_str());
					if (jointId > -1)
					{
						// if compound StatObj (from deformation), use first SubObj for restoring
						if (pStatObj != NULL)
						{
							if (!pStatObj->GetRenderMesh() && pStatObj->GetSubObjectCount() > 0)
							{
								pStatObj = pStatObj->GetSubObject(0)->pStatObj;
							}

							SetCGASlot(jointId, pStatObj);

#if ENABLE_VEHICLE_DEBUG
							if (IsDebugParts())
								CryLog("restoring StatObj on joint %i (%s) -> %s", jointId, jointName.c_str(), pStatObj ? pStatObj->GetGeoName() : "<NULL>");
#endif
						}

						TStringVehiclePartMap::iterator it = m_jointParts.find(jointName);
						if (it != m_jointParts.end())
						{
							it->second->ChangeState(state, flags & ~eVPSF_Physicalize | eVPSF_Force);
						}
					}
				}
				flags |= eVPSF_Physicalize;
			}
		}
	}

	m_state = state;

	// physicalize after all parts have been restored
	if (flags & eVPSF_Physicalize && GetEntity()->GetPhysics())
	{
		Physicalize();
		for (TStringVehiclePartMap::iterator it = m_jointParts.begin(); it != m_jointParts.end(); ++it)
		{
			it->second->Physicalize();
		}
	}

	return true;
}

//------------------------------------------------------------------------
bool CVehiclePartAnimated::ChangeChildState(CVehiclePartAnimatedJoint* pPart, EVehiclePartState state, int flags)
{
	// only handle range between intact and destroyed
	if (state > pPart->GetState() && (state < eVGS_Damaged1 || state >= eVGS_Destroyed))
		return false;

	if (state < pPart->GetState() && pPart->GetState() >= eVGS_Destroyed)
		return false;

	int jointId = pPart->GetJointId();

	if (pPart->GetState() == eVGS_Default)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance ? m_pCharInstance->GetISkeletonPose() : NULL;

		if (IStatObj* pStatObjIntact = pSkeletonPose ? pSkeletonPose->GetStatObjOnJoint(jointId) : NULL)
		{
			IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
			const char* jointName = rIDefaultSkeleton.GetJointNameByID(jointId);

			if (m_intactStatObjs.find(CONST_TEMP_STRING(jointName)) == m_intactStatObjs.end())
				m_intactStatObjs.insert(TStringStatObjMap::value_type(jointName, pStatObjIntact));
		}
	}

	if (m_jointParts.find(pPart->GetName()) == m_jointParts.end())
		m_jointParts.insert(TStringVehiclePartMap::value_type(pPart->GetName(), pPart));

	IStatObj* pStatObj = GetGeometryForState(pPart, state);

	if (pStatObj)
		SetCGASlot(jointId, pStatObj, (flags & eVPSF_Physicalize) != 0);

	return true;
}

//------------------------------------------------------------------------
IStatObj* CVehiclePartAnimated::GetSubGeometry(CVehiclePartBase* pPart, EVehiclePartState state, Matrix34& localTM, bool removeFromParent)
{
	ICharacterInstance* pCharInstance = 0;
	string jointName;
	pPart->GetGeometryName(state, jointName);
	int jointId = -1;

	if (state == eVGS_Destroyed)
	{
		pCharInstance = m_pCharInstanceDestroyed;

		if (pCharInstance)
			jointId = pCharInstance->GetIDefaultSkeleton().GetJointIDByName(jointName.c_str());
	}
	else
	{
		// lookup first on intact, then on destroyed model
		pCharInstance = m_pCharInstance;

		if (pCharInstance)
			jointId = pCharInstance->GetIDefaultSkeleton().GetJointIDByName(jointName.c_str());

		if (jointId == -1)
		{
			pCharInstance = m_pCharInstanceDestroyed;

			if (pCharInstance)
				jointId = pCharInstance->GetIDefaultSkeleton().GetJointIDByName(jointName.c_str());
		}
	}

	if (jointId != -1)
	{
		if (ISkeletonPose* pSkeletonPose = pCharInstance->GetISkeletonPose())
		{
			localTM = Matrix34(pCharInstance->GetISkeletonPose()->GetAbsJointByID(jointId));

			if (IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(jointId))
			{
				if (removeFromParent && (pCharInstance != m_pCharInstanceDestroyed))
				{
					if (m_intactStatObjs.find(pPart->GetName()) == m_intactStatObjs.end())
					{
						m_intactStatObjs.insert(TStringStatObjMap::value_type(pPart->GetName(), pStatObj));
					}

					SetCGASlot(jointId, NULL);
				}

				if (pPart)
				{
					m_jointParts.insert(TStringVehiclePartMap::value_type(pPart->GetName(), pPart));
				}

				return pStatObj;
			}
			else if ((state == eVGS_Default) && (pPart->GetState() != eVGS_Default))
			{
				TStringStatObjMap::const_iterator it = m_intactStatObjs.find(pPart->GetName());

				if (it != m_intactStatObjs.end())
				{
					return it->second;
				}
			}
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::Physicalize()
{
	if (m_pSharedParameters->m_isPhysicalized && GetEntity()->GetPhysics())
	{
		if (m_slot != -1)
			GetEntity()->UnphysicalizeSlot(m_slot);

		SEntityPhysicalizeParams params;
		params.mass = m_pSharedParameters->m_mass;
		params.density = m_pSharedParameters->m_density;
		params.nSlot = m_slot;
		GetEntity()->PhysicalizeSlot(m_slot, params); // always returns -1 for chars

		if (m_pCharInstance)
		{
			FlagSkeleton(m_pCharInstance->GetISkeletonPose(), m_pCharInstance->GetIDefaultSkeleton());
		}

		m_pVehicle->RequestPhysicalization(this, false);
	}

	GetEntity()->SetSlotFlags(m_slot, GetEntity()->GetSlotFlags(m_slot) | ENTITY_SLOT_IGNORE_PHYSICS);
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::FlagSkeleton(ISkeletonPose* pSkeletonPose, IDefaultSkeleton& rIDefaultSkeleton)
{
	if (!pSkeletonPose)
		return;

	IPhysicalEntity* pPhysics = GetEntity()->GetPhysics();
	if (!pPhysics)
		return;

	string name;
	int idWater = rIDefaultSkeleton.GetJointIDByName("proxy_water");
	uint32 buoyancyParts = (idWater != -1) ? 1 : 0;

	uint32 jointCount = rIDefaultSkeleton.GetJointCount();
	for (uint32 i = 0; i < jointCount; ++i)
	{
		int physId = pSkeletonPose->GetPhysIdOnJoint(i);

		if (physId >= 0)
		{
			CheckColltypeHeavy(physId);
			name = rIDefaultSkeleton.GetJointNameByID(i);

			// when water proxy available, remove float from all others
			// if no water proxy, we leave only "proxy" parts floating
			if (idWater != -1)
			{
				if (i == idWater)
				{
					SetFlags(physId, geom_collides, false);
					SetFlagsCollider(physId, 0);
				}
				else
					SetFlags(physId, geom_floats, false);
			}
			else
			{
				if (name.find("proxy") != string::npos)
					++buoyancyParts;
				else
					SetFlags(physId, geom_floats, false);
			}

			// all objects which have a corresponding *_proxy on the skeleton
			// are set to ray collision only
			if (name.find("_proxy") == string::npos)
			{
				name.append("_proxy");
				int proxyId = rIDefaultSkeleton.GetJointIDByName(name.c_str());

				if (proxyId != -1)
				{
					// remove ray collision from hull proxy(s)
					SetFlags(pSkeletonPose->GetPhysIdOnJoint(proxyId), geom_colltype_ray | geom_colltype13, false);

					// get StatObj from main part, to connect proxies foreignData with it
					IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(i);

					if (pStatObj)
					{
						pe_params_part params;
						params.partid = proxyId;
						if (pPhysics->GetParams(&params))
						{
							if (params.pPhysGeom && params.pPhysGeom->pGeom)
								params.pPhysGeom->pGeom->SetForeignData(pStatObj, 0);
						}
					}

					for (int p = 2; p < 6; ++p)
					{
						// check additional proxies, by naming convention _02, .. _05
						char buf[64];
						cry_sprintf(buf, "%s_%02i", name.c_str(), p);

						proxyId = rIDefaultSkeleton.GetJointIDByName(buf);
						if (proxyId == -1)
							break;

						int proxyPhysId = pSkeletonPose->GetPhysIdOnJoint(proxyId);
						if (proxyPhysId == -1)
							continue;

						SetFlags(proxyPhysId, geom_colltype_ray | geom_colltype13, false);

						// connect proxies to main StatObj (needed for bullet tests, decals)
						if (pStatObj)
						{
							pe_params_part params;
							params.partid = proxyPhysId;
							if (pPhysics->GetParams(&params))
							{
								if (params.pPhysGeom && params.pPhysGeom->pGeom)
									params.pPhysGeom->pGeom->SetForeignData(pStatObj, 0);
							}
						}
					}

					// set ray-collision only on the part
					SetFlags(physId, geom_collides | geom_floats, false);
					SetFlags(physId, geom_colltype_ray | geom_colltype13, true);
					SetFlagsCollider(physId, 0);
				}
			}
		}
	}

	if (buoyancyParts == 0)
	{
		// as fallback, use part with largest volume for buoyancy
		int partId = -1;
		float maxV = 0.f;

		pe_status_nparts nparts;
		int numParts = pPhysics->GetStatus(&nparts);

		for (int i = 0; i < numParts; ++i)
		{
			pe_params_part params;
			params.ipart = i;
			if (pPhysics->GetParams(&params))
			{
				float v = (params.pPhysGeomProxy) ? params.pPhysGeomProxy->V : params.pPhysGeom->V;
				if (v > maxV)
				{
					partId = params.partid;
					maxV = v;
				}
			}
		}

		if (partId != -1)
			SetFlags(partId, geom_floats, true);
		else
			GameWarning("[CVehiclePartAnimated]: <%s> has no buoyancy parts! (Check material for correct physicalization setup properties)", GetEntity()->GetName());
	}

	int jointId, physId;
	if ((jointId = rIDefaultSkeleton.GetJointIDByName("proxy_skirt")) != -1)
	{
		if ((physId = pSkeletonPose->GetPhysIdOnJoint(jointId)) != -1)
		{
			SetFlags(physId, geom_collides | geom_floats, false);
			SetFlags(physId, geom_colltype_ray | geom_colltype13 | geom_colltype_player | geom_colltype_foliage, true);
			SetFlagsCollider(physId, 0);
		}
	}

	// remove collision flags from all _proxy geoms by debug cvar
	// useful for seeing through, testing ray proxies etc
	if (VehicleCVars().v_disable_hull > 0)
	{
		for (uint32 i = 0; i < rIDefaultSkeleton.GetJointCount(); ++i)
		{
			if (strstr(rIDefaultSkeleton.GetJointNameByID(i), "_proxy"))
			{
				SetFlags(pSkeletonPose->GetPhysIdOnJoint(i), geom_collides | geom_floats, false);
				SetFlagsCollider(pSkeletonPose->GetPhysIdOnJoint(i), 0);
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::RotationChanged(CVehiclePartAnimatedJoint* pJoint)
{
	// craig: cannot drop these changes if the vehicle is on a server
	bool cull = false;
	if (gEnv->IsClient())
		cull = m_pVehicle->IsProbablyDistant();

	if (m_iRotChangedFrameId != gEnv->nMainFrameID && !cull)
	{
		// force skeleton update for this frame (this is intended, contact Ivo)
		if (m_pCharInstance)
			m_pCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(1);

		m_iRotChangedFrameId = gEnv->nMainFrameID;

		// require update for the next frame to reset skeleton update
		m_pVehicle->NeedsUpdate();
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::Update(const float frameTime)
{
	CVehiclePartBase::Update(frameTime);

	if (m_iRotChangedFrameId)
	{
		if (m_iRotChangedFrameId < gEnv->nMainFrameID)
		{
			if (m_pCharInstance)
				m_pCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(0);

			m_iRotChangedFrameId = 0;
		}
		else
			m_pVehicle->NeedsUpdate();
	}
	else if (m_serializeForceRotationUpdate)
	{
		m_serializeForceRotationUpdate = false;
		if (m_pCharInstance)
			m_pCharInstance->GetISkeletonPose()->SetForceSkeletonUpdate(1);
		m_iRotChangedFrameId = gEnv->nMainFrameID;
		m_pVehicle->NeedsUpdate();
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::SetDrivingProxy(bool bDrive)
{
	IVehicleMovement* pMovement = m_pVehicle->GetMovement();
	if (!(pMovement && pMovement->UseDrivingProxy()))
		return;

	if (0 == m_hullMatId[bDrive]) // 0 means, nothin to do
		return;

	if (!m_pCharInstance)
		return;

	ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
	if (!pSkeletonPose)
		return;
	IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();
	IPhysicalEntity* pPhysics = m_pVehicle->GetEntity()->GetPhysics();
	if (!pPhysics)
		return;

	int id = rIDefaultSkeleton.GetJointIDByName("hull_proxy");

	if (id < 0)
	{
		m_hullMatId[0] = m_hullMatId[1] = 0;
		return;
	}

	int partid = pSkeletonPose->GetPhysIdOnJoint(id);

	if (partid == -1)
		return;

	pe_params_part params;
	params.partid = partid;
	params.ipart = -1;
	if (!pPhysics->GetParams(&params) || !params.nMats)
		return;

	phys_geometry* pGeom = params.pPhysGeom;
	if (pGeom && pGeom->surface_idx < pGeom->nMats)
	{
		ISurfaceTypeManager* pSurfaceMan = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceTypeManager();

		// initialize once
		if (m_hullMatId[0] < 0)
		{
			int idDriving = 0;
			int idOrig = pGeom->pMatMapping[pGeom->surface_idx];
			const char* matOrig = pSurfaceMan->GetSurfaceType(idOrig)->GetName();

			if (strstr(matOrig, "mat_metal"))
				idDriving = pSurfaceMan->GetSurfaceTypeByName("mat_metal_nofric")->GetId();
			else
			{
				string mat(matOrig);
				mat.append("_nofric");

				idDriving = pSurfaceMan->GetSurfaceTypeByName(mat.c_str(), NULL, false)->GetId();
			}

			//if (pDebug->GetIVal())
			//CryLog("%s looking up driving surface replacement for %s (id %i) -> got id %i", m_pVehicle->GetEntity()->GetName(), matOrig, idOrig, idDriving);

			if (idDriving > 0)
			{
				// store old and new id
				m_hullMatId[0] = idOrig;
				m_hullMatId[1] = idDriving;

				/*if (pDebug->GetIVal())
				   {
				   const char* matDriving = pSurfaceMan->GetSurfaceType(idDriving)->GetName();
				   CryLog("%s storing hull matId for swapping: %i (%s) -> %i (%s)", m_pVehicle->GetEntity()->GetName(), m_hullMatId[0], matOrig, m_hullMatId[1], matDriving);
				   }*/
			}
			else
				m_hullMatId[0] = m_hullMatId[1] = 0;
		}

		// only swap if materials available
		if (m_hullMatId[bDrive] > 0)
		{
#if ENABLE_VEHICLE_DEBUG
			if (VehicleCVars().v_debugdraw == eVDB_Parts)
				CryLog("%s swapping hull proxy from %i (%s) to matId %i (%s)", m_pVehicle->GetEntity()->GetName(), m_hullMatId[bDrive ^ 1], pSurfaceMan->GetSurfaceType(m_hullMatId[bDrive ^ 1])->GetName(), m_hullMatId[bDrive], pSurfaceMan->GetSurfaceType(m_hullMatId[bDrive])->GetName());
#endif

			for (int n = 0; n < pGeom->nMats; ++n)
				pGeom->pMatMapping[n] = m_hullMatId[bDrive];
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::Serialize(TSerialize ser, EEntityAspects aspects)
{
	CVehiclePartBase::Serialize(ser, aspects);
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::PostSerialize()
{
	if (m_iRotChangedFrameId)
	{
		// if necessary, make sure skeleton gets updated in 1st frame
		m_iRotChangedFrameId = 0;
		RotationChanged(0);
	}
	else if (gEnv->pSystem->IsSerializingFile() == 2 && m_pVehicle->IsDestroyed())
		m_serializeForceRotationUpdate = true;
}

#if ENABLE_VEHICLE_DEBUG
void DumpSkeleton(IDefaultSkeleton& rIDefaultSkeleton)
{
	for (int i = 0; i < rIDefaultSkeleton.GetJointCount(); ++i)
	{
		const char* name = rIDefaultSkeleton.GetJointNameByID(i);
		int parentid = rIDefaultSkeleton.GetJointParentIDByID(i);

		if (parentid != -1)
			CryLog("joint %i: %s (parent %i)", i, name, parentid);
		else
			CryLog("joint %i: %s", i, name);
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimated::Dump()
{
	if (m_pCharInstance)
	{
		CryLog("<%s> pCharInstance:", GetName());
		DumpSkeleton(m_pCharInstance->GetIDefaultSkeleton());
	}

	if (m_pCharInstanceDestroyed)
	{
		CryLog("<%s> pCharInstanceDestroyed:", GetName());
		DumpSkeleton(m_pCharInstanceDestroyed->GetIDefaultSkeleton());
	}
}
#endif

//------------------------------------------------------------------------
int CVehiclePartAnimated::GetNextFreeLayer()
{
	if (ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(m_slot))
	{
		ISkeletonAnim* pSkeletonAnim = pCharInstance->GetISkeletonAnim();
		CRY_ASSERT(pSkeletonAnim);

		for (int i = 1; i < ISkeletonAnim::LayerCount; i++)
		{
			if (pSkeletonAnim->GetNumAnimsInFIFO(i) == 0)
				return i;
		}
	}

	return -1;
}

void CVehiclePartAnimated::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_intactStatObjs);
	s->AddObject(m_jointParts);
	CVehiclePartBase::GetMemoryUsage(s);
}

DEFINE_VEHICLEOBJECT(CVehiclePartAnimated);
