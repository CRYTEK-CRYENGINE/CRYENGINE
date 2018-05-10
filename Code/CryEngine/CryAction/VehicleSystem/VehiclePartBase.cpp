// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a base class for vehicle parts

   -------------------------------------------------------------------------
   History:
   - 23:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehicleComponent.h"
#include "VehicleSeat.h"
#include "VehicleSeatSerializer.h"
#include "VehicleUtils.h"
#include "PersistantDebug.h"

DEFINE_SHARED_PARAMS_TYPE_INFO(CVehiclePartBase::SSharedParams_Parts)

const char* CVehiclePartBase::ms_geometryDestroyedSuffixes[eVGS_Last] =
{
	"",
	"_damaged_1",
	"_damaged"
};

const char* CVehiclePartBase::ms_nullGeometryDestroyedSuffix = "";

// Suppressed uninitMemberVar warnings due to use of BEGIN_SHARED_PARAMS macro
// cppcheck-suppress uninitMemberVar
CVehiclePartBase::CVehiclePartBase()
	: m_pVehicle(nullptr)
	, m_pParentPart(nullptr)
	, m_pClonedMaterial(nullptr)
	, m_physId(-1)
	, m_state(eVGS_Last)
	, m_hideMode(eVPH_NoFade)
	, m_hideCount(0)
	, m_slot(-1)
	, m_users(0)
	, m_isRotationBlocked(false)
	, m_damageRatio(0.0f)
	, m_hideTimeMax(0.0f)
	, m_hideTimeCount(0.0f)
	, m_index(-1)
{}

//------------------------------------------------------------------------
CVehiclePartBase::~CVehiclePartBase()
{
}

//------------------------------------------------------------------------
bool CVehiclePartBase::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* pParent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	m_pVehicle = (CVehicle*) pVehicle;

	if (!RegisterSharedParameters(pVehicle, table, partType))
		return false;

	bool hidden = false;
	table.getAttr("isHidden", hidden);
	m_hideCount = (hidden == true) ? 1 : 0;

	reinterpret_cast<CVehicle*>(pVehicle)->m_mass += m_pSharedParameters->m_mass;

	m_pParentPart = pParent;

	if (pParent)
		pParent->AddChildPart(this);

	string component = table.getAttr("component");
	if (!component.empty())
		initInfo.partComponentMap.push_back(CVehicle::SPartComponentPair(this, component));

	// read optional multiple components (part->component relationship is now 1:n)
	if (CVehicleParams components = table.findChild("Components"))
	{
		int i = 0;
		int c = components.getChildCount();

		for (; i < c; i++)
		{
			CVehicleParams componentRef = components.getChild(i);
			initInfo.partComponentMap.push_back(CVehicle::SPartComponentPair(this, componentRef.getAttr("value")));
		}
	}

	string filename = table.getAttr("filename");
	if (!filename.empty())
	{
		if (filename.find("."))
			m_slot = GetEntity()->LoadGeometry(m_slot, filename);
		else
			m_slot = GetEntity()->LoadCharacter(m_slot, filename);
	}

	m_isRotationBlocked = false;

	m_index = initInfo.index;

	return true;
}

//------------------------------------------------------------------------

namespace
{
void CheckLowSpecFile(string& filename)
{
	// try to load _low asset on lowspec

	size_t dot = filename.rfind('.');
	if (dot != ~0)
	{
		string stripped = filename.substr(0, dot);
		string suffix = filename.substr(dot);

		string lowspecFile = stripped + "_low" + suffix;

		if (FILE* pFile = gEnv->pCryPak->FOpen(lowspecFile.c_str(), "rbx"))
		{
			filename = lowspecFile;
			gEnv->pCryPak->FClose(pFile);

			CryLog("using lowspec file %s", lowspecFile.c_str());
		}
	}
}
}

//------------------------------------------------------------------------
bool CVehiclePartBase::RegisterSharedParameters(IVehicle* pVehicle, const CVehicleParams& table, int partType)
{
	string name = table.getAttr("name");
	if (name.empty())
		return false;

	string sharedParamsName = pVehicle->GetEntity()->GetClass()->GetName();
	const char* pModification = pVehicle->GetModification();
	if (pModification != 0 && strlen(pModification))
	{
		sharedParamsName.append("::");

		sharedParamsName.append(pModification);
	}
	sharedParamsName.append("::Part::");
	sharedParamsName.append(name);

	ISharedParamsManager* pSharedParamsManager = CCryAction::GetCryAction()->GetISharedParamsManager();

	CRY_ASSERT(pSharedParamsManager);

	m_pSharedParameters = CastSharedParamsPtr<SSharedParams_Parts>(pSharedParamsManager->Get(sharedParamsName));

	if (!m_pSharedParameters)
	{
		SSharedParams_Parts sharedParams;

		// read shared parameters from xml
		sharedParams.m_typeId = partType;

		// base
		sharedParams.m_name = table.getAttr("name");
		table.getAttr("disableCollision", sharedParams.m_disableCollision);
		sharedParams.m_helperPosName = table.getAttr("helper");
		table.getAttr("position", sharedParams.m_position);
		table.getAttr("useOption", sharedParams.m_useOption);
		table.getAttr("mass", sharedParams.m_mass);
		table.getAttr("density", sharedParams.m_density);

		if (table.getAttr("disablePhysics", sharedParams.m_isPhysicalized))
			sharedParams.m_isPhysicalized = !sharedParams.m_isPhysicalized;

		// animated
		CVehicleParams animatedTable = table.findChild("Animated");
		if (animatedTable)
		{
			sharedParams.m_filename = animatedTable.getAttr("filename");
			if (!sharedParams.m_filename.empty())
			{
				sharedParams.m_filenameDestroyed = animatedTable.getAttr("filenameDestroyed");
				if (sharedParams.m_filenameDestroyed.empty())
					sharedParams.m_filenameDestroyed = sharedParams.m_filename;

				if (VehicleCVars().v_vehicle_quality == 1)
				{
					CheckLowSpecFile(sharedParams.m_filename);
					CheckLowSpecFile(sharedParams.m_filenameDestroyed);
				}
			}
			else
			{
				return false;
			}

			if (animatedTable.haveAttr("destroyedSuffix"))
				sharedParams.m_destroyedSuffix = animatedTable.getAttr("destroyedSuffix");
			else
				sharedParams.m_destroyedSuffix.clear();
		}

		// animated joint
		CVehicleParams animatedJointTable = table.findChild("AnimatedJoint");
		if (animatedJointTable)
		{
			animatedJointTable.getAttr("detachBaseForce", sharedParams.m_detachBaseForce);
			animatedJointTable.getAttr("detachProbability", sharedParams.m_detachProbability);
		}

		// wheel
		CVehicleParams subTable = table.findChild("SubPartWheel");
		if (subTable)
		{
			subTable.getAttr("suspLength", sharedParams.m_suspLength);
			subTable.getAttr("slipFrictionMod", sharedParams.m_slipFrictionMod);
			subTable.getAttr("slipSlope", sharedParams.m_slipSlope);
			subTable.getAttr("rimRadius", sharedParams.m_rimRadius);
			subTable.getAttr("torqueScale", sharedParams.m_torqueScale);

			sharedParams.m_wheelIndex = pVehicle->GetWheelCount();
		}

		// register the read params
		m_pSharedParameters = CastSharedParamsPtr<SSharedParams_Parts>(pSharedParamsManager->Register(sharedParamsName, sharedParams));

		CRY_ASSERT(m_pSharedParameters != 0);
		if (!m_pSharedParameters)
			return false;
	}
	else
	{
		// additional check on requested part type
		CRY_ASSERT(m_pSharedParameters->m_typeId == partType);
	}

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartBase::PostInit()
{
}

//------------------------------------------------------------------------
EntityId CVehiclePartBase::SpawnEntity()
{
	char pPartName[128];
	cry_sprintf(pPartName, "%s_part_%s", m_pVehicle->GetEntity()->GetName(), GetName());

	SEntitySpawnParams params;
	params.sName = pPartName;
	params.nFlags = ENTITY_FLAG_CLIENT_ONLY;
	params.pClass = gEnv->pEntitySystem->GetClassRegistry()->FindClass("VehiclePart");

	IEntity* pPartEntity = gEnv->pEntitySystem->SpawnEntity(params, true);
	if (pPartEntity)
	{
		if (m_pParentPart)
			m_pParentPart->GetEntity()->AttachChild(pPartEntity);
		else
			m_pVehicle->GetEntity()->AttachChild(pPartEntity);

		pPartEntity->SetFlags(pPartEntity->GetFlags() | ENTITY_FLAG_CASTSHADOW);

		return pPartEntity->GetId();
	}

	CRY_ASSERT(0);
	return 0;
}

//------------------------------------------------------------------------
IEntity* CVehiclePartBase::GetEntity()
{
	if (m_pParentPart)
		return m_pParentPart->GetEntity();
	else
		return m_pVehicle->GetEntity();
}

//------------------------------------------------------------------------
void CVehiclePartBase::Release()
{
	delete this;
}

//------------------------------------------------------------------------
void CVehiclePartBase::Reset()
{
	m_damageRatio = 0.0f;

	ResetLocalTM(true);

	ChangeState(eVGS_Default);
}

//------------------------------------------------------------------------
void CVehiclePartBase::OnEvent(const SVehiclePartEvent& event)
{
	switch (event.type)
	{
	case eVPE_Damaged:
		{
			m_damageRatio = event.fparam;

			const EVehiclePartState state = GetStateForDamageRatio(m_damageRatio);
			ChangeState(state, eVPSF_Physicalize);
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
		break;
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
		m_isRotationBlocked = event.bparam;
		break;
	}
}

//------------------------------------------------------------------------
void CVehiclePartBase::Hide(bool hide)
{
	if (m_slot == -1)
		return;

	IEntity* pEntity = GetEntity();
	if (!pEntity)
		return;

	if (hide)
	{
		++m_hideCount;

		if (m_hideCount > 0)
			pEntity->SetSlotFlags(m_slot, pEntity->GetSlotFlags(m_slot) & ~ENTITY_SLOT_RENDER);
	}
	else
	{
		--m_hideCount;

		if (m_hideCount <= 0)
			pEntity->SetSlotFlags(m_slot, pEntity->GetSlotFlags(m_slot) | ENTITY_SLOT_RENDER);
	}
}

//------------------------------------------------------------------------
IVehiclePart::EVehiclePartState CVehiclePartBase::GetMaxState()
{
	EVehiclePartState state = m_state;

	for (TVehicleChildParts::iterator it = m_children.begin(), end = m_children.end(); it != end; ++it)
	{
		EVehiclePartState maxState = (*it)->GetMaxState();

		if (maxState > state && maxState != eVGS_Last)
			state = maxState;
	}

	return state;
}

//------------------------------------------------------------------------
IVehiclePart::EVehiclePartState CVehiclePartBase::GetStateForDamageRatio(float ratio)
{
	if (ratio >= 1.f && m_pVehicle->IsDestroyable())
		return m_pVehicle->IsDestroyed() ? eVGS_Destroyed : eVGS_Damaged1;

	return eVGS_Default;
}

//------------------------------------------------------------------------
bool CVehiclePartBase::ChangeState(EVehiclePartState state, int flags)
{
	if (state == m_state && !(flags & eVPSF_Force))
		return false;

	// once destroyed, we only allow to return to default state
	if (m_state == eVGS_Destroyed && state != eVGS_Default && !(flags & eVPSF_Force))
		return false;

	if (eVPT_Base == GetType() && m_hideCount == 0)
	{
		if (state == eVGS_Destroyed)
			Hide(true);
		else if (state == eVGS_Default)
			Hide(false);
	}

	return true;
}

//------------------------------------------------------------------------
IStatObj* CVehiclePartBase::GetSubGeometry(CVehiclePartBase* pPart, EVehiclePartState state, Matrix34& position, bool removeFromParent)
{
	if (CVehiclePartBase* pParentPartBase = (CVehiclePartBase*) m_pParentPart)
		return pParentPartBase->GetSubGeometry(pPart, state, position, removeFromParent);
	else
		return NULL;
}

//------------------------------------------------------------------------
void CVehiclePartBase::GetGeometryName(EVehiclePartState state, string& name)
{
	name = GetName();

	name += GetDestroyedGeometrySuffix(state);
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartBase::GetLocalTM(bool relativeToParentPart, bool forced)
{
	const Matrix34 tm = GetEntity()->GetSlotLocalTM(m_slot, relativeToParentPart);

	return VALIDATE_MAT(tm);
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetLocalTM(const Matrix34& localTM)
{
	GetEntity()->SetSlotLocalTM(m_slot, localTM);
}

//------------------------------------------------------------------------
void CVehiclePartBase::ResetLocalTM(bool recursive)
{
	if (recursive)
	{
		for (TVehicleChildParts::iterator it = m_children.begin(), end = m_children.end(); it != end; ++it)
			(*it)->ResetLocalTM(true);
	}
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartBase::GetWorldTM()
{
	return VALIDATE_MAT(GetEntity()->GetSlotWorldTM(m_slot));
}

//------------------------------------------------------------------------
const Matrix34& CVehiclePartBase::LocalToVehicleTM(const Matrix34& localTM)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	static Matrix34 tm;
	tm = VALIDATE_MAT(localTM);

	IVehiclePart* pParent = GetParent();
	while (pParent)
	{
		tm = pParent->GetLocalTM(true) * tm;
		pParent = pParent->GetParent();
	}

	return VALIDATE_MAT(tm);
}

//------------------------------------------------------------------------
const AABB& CVehiclePartBase::GetLocalBounds()
{
	if (m_slot == -1)
	{
		m_bounds.Reset();
	}
	else
	{

		if (IStatObj* pStatObj = GetEntity()->GetStatObj(m_slot))
		{
			m_bounds = pStatObj->GetAABB();
			m_bounds.SetTransformedAABB(GetEntity()->GetSlotLocalTM(m_slot, false), m_bounds);
		}
		else if (ICharacterInstance* pCharInstance = GetEntity()->GetCharacter(m_slot))
		{
			m_bounds = pCharInstance->GetAABB();
			m_bounds.SetTransformedAABB(GetEntity()->GetSlotLocalTM(m_slot, false), m_bounds);
		}
		else
		{
			GetEntity()->GetLocalBounds(m_bounds);
		}
	}

	return VALIDATE_AABB(m_bounds);
}

//------------------------------------------------------------------------
void CVehiclePartBase::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_hideMode != eVPH_NoFade)
	{
		if (m_hideMode == eVPH_FadeIn)
		{
			m_hideTimeCount += frameTime;

			if (m_hideTimeCount >= m_hideTimeMax)
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		}
		else if (m_hideMode == eVPH_FadeOut)
		{
			m_hideTimeCount -= frameTime;

			if (m_hideTimeCount <= 0.0f)
				m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);
		}
		else
			CRY_ASSERT(0);

		if (IMaterial* pMaterialMain = GetMaterial())
		{
			const float opacity = min(1.0f, max(0.0f, (m_hideTimeCount / m_hideTimeMax)));

			if (IRenderShaderResources* pShaderRes = pMaterialMain->GetShaderItem().m_pShaderResources)
			{
				pShaderRes->SetStrengthValue(EFTT_OPACITY, opacity);
			}

			for (int i = 0; i < pMaterialMain->GetSubMtlCount(); i++)
			{
				if (IMaterial* pMaterial = pMaterialMain->GetSubMtl(i))
				{
					if (IRenderShaderResources* pShaderRes = pMaterial->GetShaderItem().m_pShaderResources)
					{
						pShaderRes->SetStrengthValue(EFTT_OPACITY, opacity);
					}
				}
			}
		}
	}
}

//------------------------------------------------------------------------
IMaterial* CVehiclePartBase::GetMaterial()
{
	if (m_pClonedMaterial)
		return m_pClonedMaterial;
	else
		return GetEntity()->GetMaterial();
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetMaterial(IMaterial* pMaterial)
{
	// moved to CVehiclePartAnimated
}

//------------------------------------------------------------------------
void CVehiclePartBase::ParsePhysicsParams(SEntityPhysicalizeParams& physicalizeParams, const CVehicleParams& table)
{
	table.getAttr("mass", physicalizeParams.mass);
	table.getAttr("density", physicalizeParams.density);
}

//------------------------------------------------------------------------
void CVehiclePartBase::CheckColltypeHeavy(int partid)
{
	if (partid < 0 || !GetEntity()->GetPhysics())
		return;

	if (!m_pSharedParameters->m_disableCollision)
	{
		pe_params_part paramsPart;
		paramsPart.partid = partid;

		if (m_pVehicle->GetMass() >= VEHICLE_MASS_COLLTYPE_HEAVY)
		{
			paramsPart.flagsAND = ~geom_colltype_debris; // light debris from breakable objs
			paramsPart.flagsColliderAND = geom_colltype3;
			paramsPart.flagsColliderOR = geom_colltype3; // for heavy vehicles
		}

		paramsPart.flagsColliderOR |= geom_colltype6; // vehicle-only colltype

		GetEntity()->GetPhysics()->SetParams(&paramsPart);
	}
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetFlags(int partid, int flags, bool add)
{
	if (partid < 0 || !GetEntity()->GetPhysics())
		return;

	pe_params_part paramsPart;
	paramsPart.partid = partid;

	if (add)
	{
		paramsPart.flagsOR = flags;
	}
	else
	{
		paramsPart.flagsAND = ~flags;
	}

	GetEntity()->GetPhysics()->SetParams(&paramsPart);
}

//------------------------------------------------------------------------
void CVehiclePartBase::SetFlagsCollider(int partid, int flagsCollider)
{
	if (partid < 0 || !GetEntity()->GetPhysics())
		return;

	pe_params_part paramsPart;
	paramsPart.partid = partid;

	paramsPart.flagsColliderAND = 0;
	paramsPart.flagsColliderOR = flagsCollider;

	GetEntity()->GetPhysics()->SetParams(&paramsPart);
}

//------------------------------------------------------------------------
IVehiclePart* CVehiclePartBase::GetParent(bool root /*=false*/)
{
	IVehiclePart* pParent = m_pParentPart;

	if (root && pParent)
	{
		while (IVehiclePart* pRoot = pParent->GetParent(false))
			pParent = pRoot;
	}

	return pParent;
}

//------------------------------------------------------------------------
void CVehiclePartBase::Serialize(TSerialize ser, EEntityAspects aspects)
{
	bool bSaveGame = ser.GetSerializationTarget() != eST_Network;

	if (bSaveGame)
	{
		ser.BeginGroup("VehiclePart");

		ser.Value("m_damageRatio", m_damageRatio);
		ser.EnumValue("m_hideMode", m_hideMode, eVPH_NoFade, eVPH_FadeOut);
		ser.Value("m_hideTimeCount", m_hideTimeCount);
	}

	if (bSaveGame || aspects & CVehicle::ASPECT_COMPONENT_DAMAGE)
	{
		NET_PROFILE_SCOPE("ComponentDamage", ser.IsReading());

		EVehiclePartState state = m_state;
		ser.EnumValue("m_state", state, eVGS_Default, eVGS_Last);

		if (ser.IsReading())
		{
			ChangeState(state, eVPSF_Physicalize);
		}
	}

	if (!bSaveGame)
	{
		if (aspects & CVehicle::ASPECT_PART_MATRIX)
		{
			NET_PROFILE_SCOPE("AspectPartMatrix", ser.IsReading());

			Quat q;
			Matrix34 currentTM = GetLocalBaseTM();
			if (ser.IsWriting())
				q = Quat(currentTM);

			ser.Value("rotation", q, 'ori1');

			if (ser.IsReading())
			{
				Matrix34 newTM(q);
				newTM.SetTranslation(currentTM.GetTranslation());
				SetLocalBaseTM(newTM);
			}
		}
	}

	if (bSaveGame)
		ser.EndGroup();
}

//------------------------------------------------------------------------
void CVehiclePartBase::CloneMaterial()
{
	if (m_pClonedMaterial)
		return;

	if (IMaterial* pMaterialOriginal = GetMaterial())
	{
		m_pClonedMaterial = pMaterialOriginal->GetMaterialManager()->CloneMultiMaterial(pMaterialOriginal);
		if (m_pClonedMaterial)
			SetMaterial(m_pClonedMaterial);
	}
}

//------------------------------------------------------------------------
bool CVehiclePartBase::SetCGASlot(int jointId, IStatObj* pStatObj, bool bUpdatePhys)
{
	CRY_ASSERT(m_slot >= 0);

	if (m_slot < 0)
		return false;

	if (ICharacterInstance* pChar = GetEntity()->GetCharacter(m_slot))
		pChar->GetISkeletonPose()->SetStatObjOnJoint(jointId, pStatObj);
	else
		return false;
	if (bUpdatePhys && GetEntity()->GetPhysics())
	{
		if (pStatObj)
		{
			pe_params_part pp;
			pp.partid = EntityPhysicsUtils::AllocPartIdRange(GetEntity()->GetPhysicalEntityPartId0(m_slot), EntityPhysicsUtils::PARTID_MAX_SLOTS) | jointId;
			pp.pPhysGeom = pStatObj->GetPhysGeom();
			GetEntity()->GetPhysics()->SetParams(&pp);
		}
		else
			GetEntity()->GetPhysics()->RemoveGeometry(EntityPhysicsUtils::AllocPartIdRange(GetEntity()->GetPhysicalEntityPartId0(m_slot), EntityPhysicsUtils::PARTID_MAX_SLOTS) | jointId);
	}
	return true;
}

void CVehiclePartBase::GetMemoryUsageInternal(ICrySizer* s) const
{

}

DEFINE_VEHICLEOBJECT(CVehiclePartBase);
