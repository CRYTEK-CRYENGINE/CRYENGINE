// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which is the an attachment
   of a parent Animated part

   -------------------------------------------------------------------------
   History:
   - 25:08:2005: Created by Mathieu Pinard

*************************************************************************/
#include "StdAfx.h"

#include <CryAnimation/ICryAnimation.h>
#include "IVehicleSystem.h"
#include <CryGame/IGameTokens.h>

#include "CryAction.h"
#include "Vehicle.h"
#include "VehiclePartBase.h"
#include "VehiclePartAnimated.h"
#include "VehiclePartAnimatedJoint.h"
#include "VehicleUtils.h"
#include <CryExtension/CryCreateClassInstance.h>

//#pragma optimize("", off)
//#pragma inline_depth(0)
//------------------------------------------------------------------------
CVehiclePartAnimatedJoint::CVehiclePartAnimatedJoint()
{
	m_pCharInstance = NULL;
	m_pAnimatedPart = NULL;
	m_jointId = -1;
	m_localTMInvalid = false;
	m_isMoveable = false;
	m_isTransMoveable = false;
	m_bUsePaintMaterial = false;

#ifdef VEH_USE_RPM_JOINT
	m_dialsRotMax = 0.f;
	m_initialRotOfs = 0.f;
#endif

	m_pGeometry = 0;
	m_pDestroyedGeometry = 0;
	m_pMaterial = 0;

	m_baseTM.SetIdentity();
	m_initialTM.SetIdentity();
	m_localTM.SetIdentity();
	m_nextFrameLocalTM.SetIdentity();
	m_worldTM.SetIdentity();
}

//------------------------------------------------------------------------
CVehiclePartAnimatedJoint::~CVehiclePartAnimatedJoint()
{
}

//------------------------------------------------------------------------
bool CVehiclePartAnimatedJoint::Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType)
{
	if (!CVehiclePartBase::Init(pVehicle, table, parent, initInfo, eVPT_AnimatedJoint))
		return false;

#ifdef VEH_USE_RPM_JOINT
	CVehicleParams animatedJointTable = table.findChild("AnimatedJoint");

	if (animatedJointTable)
	{
		CVehicleParams dialsTable = animatedJointTable.findChild("Dials");
		if (dialsTable)
		{
			if (dialsTable.getAttr("rotationMax", m_dialsRotMax))
				m_dialsRotMax = DEG2RAD(m_dialsRotMax);
			if (dialsTable.getAttr("ofs", m_initialRotOfs))
				m_initialRotOfs = DEG2RAD(m_initialRotOfs);
		}
	}
#endif

	InitGeometry(table);

	m_state = eVGS_Default;

	return true;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::PostInit()
{
	CVehiclePartBase::PostInit();

	if (m_pGeometry)
		SetMaterial(m_pMaterial);
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Reset()
{
	CVehiclePartBase::Reset();

	if (m_pGeometry)
		SetMaterial(m_pMaterial);

	if (m_pCharInstance)
		m_pCharInstance->SetFlags(m_pCharInstance->GetFlags() | CS_FLAG_UPDATE);
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::ResetLocalTM(bool recursive)
{
	CVehiclePartBase::ResetLocalTM(recursive);

	if (m_jointId > -1)
	{
		SetLocalBaseTM(m_initialTM);
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::InitGeometry(const CVehicleParams& table)
{
	IVehiclePart* pParent = GetParent(true);

	if (pParent)
	{
		m_pAnimatedPart = CAST_VEHICLEOBJECT(CVehiclePartAnimated, pParent);
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CVehiclePartAnimatedJoint '%s' : Parent part required.", GetName());

		m_pAnimatedPart = NULL;
	}

	if (m_pAnimatedPart)
	{
		if (m_pCharInstance = m_pAnimatedPart->GetEntity()->GetCharacter(m_pAnimatedPart->GetSlot()))
		{
			IDefaultSkeleton& rIDefaultSkeleton = m_pCharInstance->GetIDefaultSkeleton();

			m_slot = m_pAnimatedPart->GetSlot();
			m_jointId = rIDefaultSkeleton.GetJointIDByName(GetName());

			if (m_pSharedParameters->m_useOption >= 0 && m_pSharedParameters->m_useOption <= MAX_OPTIONAL_PARTS)
			{
				char jointName[128];
				if (m_pSharedParameters->m_useOption > 0)
				{
					// store actual used name
					cry_sprintf(jointName, "%s_option_%i", GetName(), m_pSharedParameters->m_useOption);

					int jointId = rIDefaultSkeleton.GetJointIDByName(jointName);
					if (jointId != -1)
						m_jointId = jointId;
				}

				// remove unused statobjs
				for (int i = 0; i <= MAX_OPTIONAL_PARTS; ++i)
				{
					if (i == m_pSharedParameters->m_useOption)
						continue;

					if (i == 0)
						cry_sprintf(jointName, "%s", GetName());
					else
						cry_sprintf(jointName, "%s_option_%i", GetName(), i);

					jointName[sizeof(jointName) - 1] = '\0';

					int jointId = rIDefaultSkeleton.GetJointIDByName(jointName);
					if (jointId > -1)
					{
						// remove all children this joint might have
						for (int n = 0, count = rIDefaultSkeleton.GetJointCount(); n < count; ++n)
						{
							if (n != jointId && rIDefaultSkeleton.GetJointParentIDByID(n) == jointId)
								SetCGASlot(n, NULL);
						}

						SetCGASlot(jointId, NULL);
					}
				}
			}

			if (m_jointId != -1)
			{
				CVehicleParams animatedJointTable = table.findChild("AnimatedJoint");
				if (animatedJointTable)
				{
					string externalFile = animatedJointTable.getAttr("filename");
					if (!externalFile.empty())
					{
						if (m_pGeometry = gEnv->p3DEngine->LoadStatObj(externalFile))
						{
							SetStatObj(m_pGeometry);

							animatedJointTable.getAttr("usePaintMaterial", m_bUsePaintMaterial);

							externalFile = animatedJointTable.getAttr("filenameDestroyed");
							if (!externalFile.empty())
								m_pDestroyedGeometry = gEnv->p3DEngine->LoadStatObj(externalFile);
						}
					}
				}
			}

			if (m_jointId > -1)
			{
				CVehicleParams animatedJointTable = table.findChild("AnimatedJoint");

				QuatT offsetQuat(ZERO, IDENTITY);
				if (animatedJointTable)
				{
					Vec3 offsetRotation;
					if (animatedJointTable.getAttr("offsetRotation", offsetRotation))
					{
						offsetQuat.q = Quat::CreateRotationVDir(offsetRotation.GetNormalizedSafe(), 0.0f);
					}
				}

				ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
				Matrix34 tm = Matrix34((pSkeletonPose->GetRelJointByID(m_jointId) * offsetQuat));

				SetLocalBaseTM(tm);
				m_initialTM = GetLocalTM(true);

				// NB SetMoveable may also be called by CSeatActionRotateTurret
#ifdef VEH_USE_RPM_JOINT
				CryFixedStringT<32> name = GetName();
				if (name.substr(0, 6) == "dials_")
				{
					SetMoveable();
					m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);
				}
#endif
			}
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Physicalize()
{
	if (m_pCharInstance && m_jointId != -1)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();

		if (pSkeletonPose)
		{
			m_physId = pSkeletonPose->GetPhysIdOnJoint(m_jointId);
		}

		m_pVehicle->RequestPhysicalization(this, false);
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetMoveable(bool allowTranslationMovement)
{
	if (!m_pCharInstance)
	{
		GameWarning("AnimatedJoint %s is missing CharInstance", GetName());
		return;
	}

	m_isMoveable = true;
	m_isTransMoveable = allowTranslationMovement;

	CryCreateClassInstanceForInterface(cryiidof<IAnimationOperatorQueue>(), m_operatorQueue);
	m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_PassengerUpdate);
}

//------------------------------------------------------------------------
bool CVehiclePartAnimatedJoint::ChangeState(EVehiclePartState state, int flags)
{
	if (!CVehiclePartBase::ChangeState(state, flags))
		return false;

	if (m_jointId != -1)
	{
		bool damaging = (state > m_state && state > eVGS_Default && state < eVGS_Destroyed);
		bool repairing = (state<m_state&& m_state> eVGS_Default && m_state < eVGS_Destroyed);
		if (damaging || repairing)
		{
			CVehiclePartAnimated* pParent = CAST_VEHICLEOBJECT(CVehiclePartAnimated, GetParent(true));
			if (pParent)
			{
				pParent->ChangeChildState(this, state, flags);
			}
		}
		else if (m_pDestroyedGeometry)
		{
			if (state == eVGS_Destroyed)
				SetStatObj(m_pDestroyedGeometry);
			else if (state == eVGS_Default)
				SetStatObj(m_pGeometry);
		}
	}

	m_state = state;

	return true;
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartAnimatedJoint::GetLocalTM(bool relativeToParentPart, bool forced)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_pCharInstance && m_jointId > -1)
	{
		VALIDATE_MAT(m_localTM);

		if (!m_isMoveable)
		{
			ISkeletonAnim* pSkeletonAnim = m_pCharInstance->GetISkeletonAnim();
			ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
			if (pSkeletonAnim && (forced || pSkeletonAnim->GetNumAnimsInFIFO(0)))
			{
				const QuatT& relJointQuat = pSkeletonPose->GetRelJointByID(m_jointId);
				CRY_ASSERT(relJointQuat.IsValid());

				SetLocalBaseTM(Matrix34(relJointQuat));
			}
		}

		if (relativeToParentPart)
		{
			return m_localTM;
		}
		else
		{
			const Matrix34& vehicleTM = LocalToVehicleTM(m_localTM);
			CRY_ASSERT(vehicleTM.IsValid());

			return VALIDATE_MAT(vehicleTM);
		}
	}

	m_localTM.SetIdentity();
	return m_localTM;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetLocalTM(const Matrix34& localTM)
{
	if (!m_pCharInstance || m_jointId < 0)
		return;

	CRY_ASSERT(localTM.IsValid());

	m_localTM = m_nextFrameLocalTM;
	m_nextFrameLocalTM = localTM;

	if (m_pAnimatedPart)
		m_pAnimatedPart->RotationChanged(this);

	InvalidateTM(true);
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetLocalBaseTM(const Matrix34& baseTM)
{
	m_baseTM = VALIDATE_MAT(baseTM);

	SetLocalTM(m_baseTM);
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SerMatrix(TSerialize ser, Matrix34& mat)
{
	Vec3 col0 = mat.GetColumn(0);
	Vec3 col1 = mat.GetColumn(1);
	Vec3 col2 = mat.GetColumn(2);
	Vec3 col3 = mat.GetColumn(3);
	ser.ValueWithDefault("matrixCol0", col0, Vec3Constants<float>::fVec3_OneX);
	ser.ValueWithDefault("matrixCol1", col1, Vec3Constants<float>::fVec3_OneY);
	ser.ValueWithDefault("matrixCol2", col2, Vec3Constants<float>::fVec3_OneZ);
	ser.Value("matrixCol3", col3);
	if (ser.IsReading())
	{
		mat.SetColumn(0, col0);
		mat.SetColumn(1, col1);
		mat.SetColumn(2, col2);
		mat.SetColumn(3, col3);
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Serialize(TSerialize ser, EEntityAspects aspects)
{
	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Vehicle part animated joint serialization");

	CVehiclePartBase::Serialize(ser, aspects);

	if (ser.GetSerializationTarget() != eST_Network)
	{
		ser.BeginGroup("VehiclePartJointMatrices");
		ser.Value("MatrixInvalid", m_localTMInvalid);
		Matrix34 baseTM(m_baseTM);
		SerMatrix(ser, baseTM);
		ser.EndGroup();

		if (ser.IsReading())
		{
			SetLocalBaseTM(baseTM);
		}
	}
}

//------------------------------------------------------------------------
Matrix34 CVehiclePartAnimatedJoint::GetWorldTM()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_pCharInstance && m_jointId > -1)
	{
		Matrix34 localTM = GetLocalTM(false);
		m_worldTM = m_pVehicle->GetEntity()->GetWorldTM() * localTM;
	}
	else
		m_worldTM = GetEntity()->GetSlotWorldTM(m_slot);

	CRY_ASSERT(m_worldTM.IsValid());

	return VALIDATE_MAT(m_worldTM);
}

//------------------------------------------------------------------------
const AABB& CVehiclePartAnimatedJoint::GetLocalBounds()
{
	if (m_pCharInstance && m_jointId > -1)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
		if (IStatObj* pStatObj = pSkeletonPose->GetStatObjOnJoint(m_jointId))
			m_localBounds = pStatObj->GetAABB();
		else
			m_localBounds.Reset();

		m_localBounds.SetTransformedAABB(GetLocalTM(false), m_localBounds);

		return m_localBounds;
	}
	else
	{
		GetEntity()->GetLocalBounds(m_localBounds);
		return m_localBounds;
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::Update(float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	CVehiclePartBase::Update(frameTime);

	if (!m_pCharInstance || m_jointId < 0)
		return;

	if (ISkeletonAnim* pSkeletonAnim = m_pCharInstance->GetISkeletonAnim())
	{
		if (m_operatorQueue)
		{
			m_operatorQueue->PushOrientation(m_jointId, IAnimationOperatorQueue::eOp_OverrideRelative, Quat(m_nextFrameLocalTM));
			if (m_isTransMoveable)
			{
				m_operatorQueue->PushPosition(m_jointId, IAnimationOperatorQueue::eOp_OverrideRelative, m_nextFrameLocalTM.GetTranslation());
			}
		}
		pSkeletonAnim->PushPoseModifier(VEH_ANIM_POSE_MODIFIER_LAYER, m_operatorQueue, "VehiclePartAnimatedJoint");
	}

#ifdef VEH_USE_RPM_JOINT
	if (m_dialsRotMax > 0.f)
	{
		if (m_pVehicle->IsPlayerDriving(true) || m_pVehicle->IsPlayerPassenger())
		{
			if (strcmp(GetName(), "dials_speedometer") == 0)
			{
				float value = 0.0f;
				IGameTokenSystem* pGTS = CCryAction::GetCryAction()->GetIGameTokenSystem();
				pGTS->GetTokenValueAs("vehicle.speedNorm", value);
				if (fabsf(m_initialRotOfs) > 0.0f)
				{
					float halfmeter = fabsf(m_initialRotOfs) * 0.25f;
					if (value < halfmeter)
						value = halfmeter;
				}
				SetLocalBaseTM(m_initialTM * Matrix33::CreateRotationZ(-value * m_dialsRotMax - m_initialRotOfs));
			}
			else if (strcmp(GetName(), "dials_revometer") == 0)
			{
				float value = 0.0f;
				IGameTokenSystem* pGTS = CCryAction::GetCryAction()->GetIGameTokenSystem();
				pGTS->GetTokenValueAs("vehicle.rpmNorm", value);
				SetLocalBaseTM(m_initialTM * Matrix33::CreateRotationZ(-value * m_dialsRotMax - m_initialRotOfs));
			}
		}
	}
#endif
}

//------------------------------------------------------------------------
IStatObj* CVehiclePartAnimatedJoint::GetStatObj()
{
	if (m_pCharInstance && m_jointId > -1)
	{
		ISkeletonPose* pSkeletonPose = m_pCharInstance->GetISkeletonPose();
		CRY_ASSERT(pSkeletonPose);
		return pSkeletonPose->GetStatObjOnJoint(m_jointId);
	}

	return NULL;
}

//------------------------------------------------------------------------
bool CVehiclePartAnimatedJoint::SetStatObj(IStatObj* pStatObj)
{
	if (!m_pCharInstance || m_jointId == -1)
		return false;

	if (pStatObj)
	{
		IStatObj* pOld = m_pCharInstance->GetISkeletonPose()->GetStatObjOnJoint(m_jointId);
		if (!pOld)
		{
			if (pStatObj->IsPhysicsExist())
			{
				// if setting a StatObj (with physics) on an empty joint, need to rephysicalize parent
				m_pVehicle->RequestPhysicalization(GetParent(true), true);
				m_pVehicle->RequestPhysicalization(this, true);
			}
		}
	}

	SetCGASlot(m_jointId, pStatObj);

	if (pStatObj && m_pGeometry)
		m_pMaterial = pStatObj->GetMaterial();

	return true;
}

//------------------------------------------------------------------------
IMaterial* CVehiclePartAnimatedJoint::GetMaterial()
{
	if (m_pCharInstance && m_jointId > -1)
	{
		if (IStatObj* pStatObj = m_pCharInstance->GetISkeletonPose()->GetStatObjOnJoint(m_jointId))
		{
			return pStatObj->GetMaterial();
		}
	}

	return NULL;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::SetMaterial(IMaterial* pMaterial)
{
	if (m_pCharInstance && m_jointId > -1)
	{
		if (IStatObj* pStatObj = m_pCharInstance->GetISkeletonPose()->GetStatObjOnJoint(m_jointId))
		{
			if (m_pGeometry && m_pMaterial && !m_bUsePaintMaterial)
			{
				pStatObj->SetMaterial(m_pMaterial);
				m_pCharInstance->GetISkeletonPose()->SetMaterialOnJoint(m_jointId, m_pMaterial);
			}
			else
				pStatObj->SetMaterial(pMaterial);
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::InvalidateTM(bool invalidate)
{
	if (invalidate && !m_localTMInvalid)
	{
		// invalidate all children
		for (TVehicleChildParts::iterator it = m_children.begin(); it != m_children.end(); ++it)
			(*it)->InvalidateTM(true);
	}

	m_localTMInvalid = invalidate;
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params)
{
	CVehiclePartBase::OnVehicleEvent(event, params);

	switch (event)
	{
	case eVE_PreVehicleDeletion:
		{
			m_pAnimatedPart = 0;
			break;
		}
	}
}

//------------------------------------------------------------------------
void CVehiclePartAnimatedJoint::OnEvent(const SVehiclePartEvent& event)
{
	CVehiclePartBase::OnEvent(event);

	if (!m_pCharInstance || m_jointId < 0)
		return;

	switch (event.type)
	{
	case eVPE_StopUsing:
		{
			m_pCharInstance->SetFlags(m_pCharInstance->GetFlags() & (~CS_FLAG_UPDATE));
			break;
		}

	case eVPE_StartUsing:
		{
			m_pCharInstance->SetFlags(m_pCharInstance->GetFlags() | CS_FLAG_UPDATE);
			break;
		}
	}
}

void CVehiclePartAnimatedJoint::GetMemoryUsage(ICrySizer* s) const
{
	s->Add(*this);
	s->AddObject(m_pAnimatedPart);
	CVehiclePartBase::GetMemoryUsageInternal(s);
}

DEFINE_VEHICLEOBJECT(CVehiclePartAnimatedJoint);
