// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 20:04:2006   13:02 : Created by Márcio Martins

*************************************************************************/
#include "StdAfx.h"
#include "VehicleWeaponControlled.h"
#include "Actor.h"
#include <IViewSystem.h>
#include <IVehicleSystem.h>
#include "Single.h"
#include "GameCVars.h"
#include "Player.h"
#include "Game.h"

#include "UI/HUD/HUDEventDispatcher.h"


//#pragma optimize("", off)
//#pragma inline_depth(0)


CControlledLocator::~CControlledLocator()
{}

bool CControlledLocator::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit)
{
  bool ret = false;
  IEntity *entity = gEnv->pEntitySystem->GetEntity(weaponId);

  if (entity)
  {
    const Vec3 &dir = entity->GetForwardDir();
    hit = entity->GetWorldTM().GetTranslation() + dir * WEAPON_CONTROLLED_HIT_RANGE;
    ret = true;
  }

  return ret;
}

bool CControlledLocator::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
  bool ret = false;
  IEntity *entity = gEnv->pEntitySystem->GetEntity(weaponId);

  if (entity)
  {
    const Vec3 &dir = entity->GetWorldTM().GetColumn1();
    pos = entity->GetWorldTM().GetTranslation() + dir * 1.5f;
    ret = true;
  }

  return ret;
}

bool CControlledLocator::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
  bool ret = false;
  IEntity *entity = gEnv->pEntitySystem->GetEntity(weaponId);

  if (entity)
  {
    dir = entity->GetWorldTM().GetColumn1();
    ret = true;
  }

  return ret;

}

bool CControlledLocator::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
  bool ret = false;
  IEntity *entity = gEnv->pEntitySystem->GetEntity(weaponId);

  if (entity)
  {
    dir = entity->GetWorldTM().GetColumn1();
    ret = true;
  }

  return ret;
}

bool CControlledLocator::GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir) 
{
  vel = firingDir * 100.0f;

  return true;
}

void CControlledLocator::WeaponReleased()
{

}

//------------------------------------------------------------------------

CVehicleWeaponControlled::CVehicleWeaponControlled() 
	: CVehicleMountedWeapon()
	, m_CurrentTime(0.5f)
	, m_Angles(ZERO)
	, m_Firing(false)
	, m_FirePause(false)
	, m_FireBlocked(false)
{

}

void CVehicleWeaponControlled::StartFire()
{
  if (!m_FireBlocked)
  {
    Base::StartFire();

    if (!m_vehicleId && GetEntity()->GetParent())
    {
      m_vehicleId = GetEntity()->GetParent()->GetId();
    }

    m_Firing = true;
    m_FirePause = false;

    SetFiringLocator(&m_ControlledLocator);
  }
}

void CVehicleWeaponControlled::StopFire()
{
  Base::StopFire();

  m_Firing = false;
  m_FirePause = false;
}

void CVehicleWeaponControlled::OnReset()
{
  Base::OnReset();

  m_FireBlocked = false;
}


void CVehicleWeaponControlled::StartUse(EntityId userId)
{
  Base::StartUse(userId);
  m_CurrentTime = 0.5f;

  IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
  if (pVehicle)
  {
	  SHUDEvent hudEvent(eHUDEvent_AddEntity);
	  hudEvent.AddData((int)pVehicle->GetEntityId());
	  CHUDEventDispatcher::CallEvent(hudEvent);
  }
}

void CVehicleWeaponControlled::StopUse(EntityId userId)
{
   EntityId id = GetOwnerId();

   Base::StopUse(userId);

   SetOwnerId(id);
   m_CurrentTime = 0.5f;

   IVehicle *pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId);
   if (pVehicle)
   {
	   SHUDEvent hudEvent(eHUDEvent_RemoveEntity);
	   hudEvent.AddData((int)pVehicle->GetEntityId());
	   CHUDEventDispatcher::CallEvent(hudEvent);
   }
}

const Matrix34& LocalToVehicleTM(IVehiclePart* pParent, const Matrix34& localTM)
{
  CRY_PROFILE_FUNCTION( PROFILE_ACTION );

  static Matrix34 tm;  
  tm = localTM;

  //IVehiclePart* pParent = GetParent();
  while (pParent)
  {
    tm = pParent->GetLocalTM(true) * tm;
    pParent = pParent->GetParent();
  }      

  return tm;
}


static float factor1 = 0.5f;
static float factor2 = 1.0f;
static float factor3 = 1.0f;

void CVehicleWeaponControlled::Update(SEntityUpdateContext& ctx, int update)
{
	IVehicle *pVehicle = m_vehicleId ? gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId) : NULL; 
  if (!m_vehicleId && GetEntity()->GetParent())
  {
    IEntity *entity = GetEntity();
    
    if (entity)
    {
      IEntity *parent = entity->GetParent();
      if (parent)
      {
				m_vehicleId = parent->GetId();
        pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(parent->GetId());
      }
    }
  }



  if (pVehicle)
  {
		IVehiclePart *pPart = pVehicle->GetWeaponParentPart(GetEntityId());
		if(pPart)
		{
			if(IVehiclePart *pParentPart = pPart->GetParent())
			{
				CRY_ASSERT(pVehicle->GetEntity());

				if(ICharacterInstance *characterInst = pVehicle->GetEntity()->GetCharacter(pParentPart->GetSlot()))
				{
					if(ISkeletonPose* pose = characterInst->GetISkeletonPose())
					{
						IDefaultSkeleton& rIDefaultSkeleton = characterInst->GetIDefaultSkeleton();
						int16 joint = rIDefaultSkeleton.GetJointIDByName(pPart->GetName());
						const QuatT &jQuat = pose->GetAbsJointByID(joint);

						Matrix34 localT(jQuat);
						localT.SetTranslation(jQuat.t/* - Vec3(0.0f, 0.75f, 0.0f)*/);

						Matrix34 vehicleWorldTm = pVehicle->GetEntity()->GetWorldTM();
						Matrix34 mat = vehicleWorldTm * localT;
						Vec3 vehicleSide2 = pPart->GetParent()->GetLocalTM(true, true).GetTranslation();

						CPlayer *pl = this->GetOwnerPlayer();

						Matrix33 mat2;
						if (!m_destination.IsEquivalent(ZERO))
						{
							Vec3 diff = GetDestination() - mat.GetTranslation(); //pPart->GetWorldTM().GetTranslation();
							diff.Normalize();

							Matrix33 loc(mat);
							loc.Invert();

							Vec3 diffLocal = loc.TransformVector(diff);

							Matrix33 desMat;
							desMat.SetRotationVDir(diffLocal, 0.0f);

							Vec3 test = GetEntity()->GetLocalTM().GetColumn0();

							Ang3 testTM(desMat);

							float za = testTM.x - m_Angles.x;
							za = (za < 0.0f) ? -gf_PI : gf_PI;
							za *= 0.05f * ctx.fFrameTime;

							m_Angles.x += za;
							Limit(m_Angles.x, -gf_PI * 0.33f, gf_PI * 0.33f);

							if (testTM.z > m_Angles.z + 0.05f)
							{
								m_Angles.z += gf_PI * factor1 * ctx.fFrameTime;        
							}
							else if (testTM.z < m_Angles.z - 0.05f)
							{
								m_Angles.z -= gf_PI * factor1 * ctx.fFrameTime;        
							}
							else
							{
								m_Angles.z = testTM.z;
							}

							Limit(m_Angles.z, -gf_PI * 0.33f, gf_PI * 0.33f);
							mat2.SetRotationXYZ(m_Angles);
						}
						else
						{
							if (!m_FireBlocked)
							{
								m_Angles.x = m_Angles.x - ctx.fFrameTime * factor2 * m_Angles.x;
								m_Angles.z = m_Angles.z - ctx.fFrameTime * factor2 * m_Angles.z;
							}
							mat2.SetRotationXYZ(m_Angles);
						}

						mat = mat * mat2; 


						GetEntity()->SetWorldTM(mat);


						if (pl)
						{
							Matrix34 worldGunMat = vehicleWorldTm * localT;

							if (!pl->IsDead())
							{

								Vec3 trans = worldGunMat.GetTranslation() - worldGunMat.GetColumn2() * 0.7f;
								worldGunMat.SetTranslation(trans);

								pl->GetEntity()->SetWorldTM(worldGunMat);


								float dot = mat.GetColumn1().dot(worldGunMat.GetColumn0());
								Update3PAnim(pl, 0.5f - dot * 0.5f, ctx.fFrameTime, mat);
							}
							else
							{

								ICharacterInstance* pCharacter = pl->GetEntity()->GetCharacter(0);
								int boneId = pCharacter ? pCharacter->GetIDefaultSkeleton().GetJointIDByName("Spine03") : 7;

								pl->LinkToMountedWeapon(0);
								if (IVehicleSeat* seat = pVehicle->GetSeatForPassenger(pl->GetEntityId()))
								{
									seat->Exit(false, true);
								}

								Matrix33 rot(worldGunMat);
								Vec3 offset(0.0f, 0.0f, 0.70f);
								Vec3 transformedOff = rot.TransformVector(offset);
								Vec3 trans = worldGunMat.GetTranslation();
								trans -= transformedOff;
								worldGunMat.SetTranslation(trans);
								pl->GetEntity()->SetWorldTM(worldGunMat);
								pl->GetEntity()->SetPos(worldGunMat.GetTranslation()); //worldGunMat.GetTranslation());
								pl->RagDollize(true);

								if (boneId > -1)
								{
									IPhysicalEntity *physEnt = pl->GetEntity()->GetPhysics();
									if (physEnt)
									{
										pe_simulation_params simulationParams;
										physEnt->GetParams(&simulationParams);

										pe_params_pos pos;
										pos.pos = GetEntity()->GetPos();
										physEnt->SetParams(&pos);

										pe_action_impulse impulse;
										impulse.ipart = boneId;
										impulse.angImpulse = Vec3(0.0f, 0.0f, 1.0f);
										impulse.impulse = worldGunMat.GetColumn1() * -1.5f * simulationParams.mass;
										physEnt->Action(&impulse);
									}
								}

								StopUse(GetOwnerId());

								SetOwnerId(0);
								StopFire();

								m_FireBlocked = true;
							} // IsDead
						} // pl
					} // pose
				} // characterInst
			} // pParentPart
		} // pPart
	} // pVehicle

  Base::Update(ctx, update);
  RequireUpdate(eIUS_General);
}


void CVehicleWeaponControlled::Update3PAnim(CPlayer *player, float goalTime, float frameTime, const Matrix34 &mat)
{
  if (player)
  {
    if (IEntity *entity = player->GetEntity())
    {
      const float ANIM_ANGLE_RANGE = gf_PI*0.25f;
      static float dir = 0.05f;
      static float pos = 0.5f;

      pos += dir;
      if (pos > 1.0f)
      {
        pos = 1.0f;
        dir = -dir;
      }
      else if (pos < 0.0f)
      {
        pos = 0.0f;
        dir = -dir;
      }

      m_CurrentTime = LERP(m_CurrentTime, goalTime, frameTime);
     
      if (ICharacterInstance *character = entity->GetCharacter(0))
      {
        ISkeletonAnim *pSkeletonAnim = character->GetISkeletonAnim();
        assert(pSkeletonAnim);

        //Update manually animation time, to match current weapon orientation
        uint32 numAnimsLayer = pSkeletonAnim->GetNumAnimsInFIFO(0);
        for(uint32 i=0; i<numAnimsLayer; i++)
        {
          CAnimation &animation = pSkeletonAnim->GetAnimFromFIFO(0, i);
          if (animation.HasStaticFlag( CA_MANUAL_UPDATE ))
          {
            float time = m_CurrentTime; //pos; //fmod_tpl(aimRad / ANIM_ANGLE_RANGE, 1.0f);
            time = (float)__fsel(time, time, 1.0f + time);
            pSkeletonAnim->SetAnimationNormalizedTime(&animation, time);
            animation.ClearStaticFlag( CA_DISABLE_MULTILAYER );
          }
        }
      }


      const SMountParams* pMountParams = GetMountedParams();
      SEntitySlotInfo info;
      if (GetEntity()->GetSlotInfo(eIGS_ThirdPerson, info))
      {
        if (info.pStatObj)
        {
          IStatObj *pStatsObj = info.pStatObj;

          const Vec3 &leftHandPos = pStatsObj->GetHelperPos(pMountParams->left_hand_helper.c_str()); 
          const Vec3 &rightHandPos = pStatsObj->GetHelperPos(pMountParams->right_hand_helper.c_str());
 
          const Vec3 leftIKPos = mat.TransformPoint(leftHandPos);
          const Vec3 rightIKPos = mat.TransformPoint(rightHandPos);

          player->SetIKPos("leftArm",		leftIKPos, 1);
          player->SetIKPos("rightArm",	rightIKPos, 1);
        }
      }
    }
  }
}




CVehicleWeaponPulseC::CVehicleWeaponPulseC() 
	: CVehicleWeapon()
	, m_TargetPos( ZERO )
	, m_StartDir( FORWARD_DIRECTION )
{

}

void CVehicleWeaponPulseC::StartFire()
{
  Base::StartFire();

	if(!m_vehicleId && GetEntity()->GetParent())
	{
		m_vehicleId = GetEntity()->GetParent()->GetId();
		CRY_ASSERT(gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId) && "Using VehicleWeapons on non-vehicles may lead to unexpected behavior.");
	}
}

void CVehicleWeaponPulseC::SetDestination(const Vec3& pos)
{
  m_TargetPos = pos; 
}

void CVehicleWeaponPulseC::Update(SEntityUpdateContext& ctx, int update)
{
	if(!m_vehicleId && GetEntity()->GetParent())
	{
		m_vehicleId = GetEntity()->GetParent()->GetId();
		CRY_ASSERT(gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(m_vehicleId) && "Using VehicleWeapons on non-vehicles may lead to unexpected behavior.");
	}

	IVehicle* pVehicle = GetVehicle();
	if(pVehicle)
	{
		IVehiclePart* pPart = pVehicle->GetWeaponParentPart(GetEntityId()); 
		if(pPart)
		{
			const Matrix34& partWorldTM = pPart->GetWorldTM();
			const Vec3 partDirection = partWorldTM.GetColumn1();
			const Vec3 partPosition = partWorldTM.GetTranslation();

			//ColorB col(255, 0, 0);
			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(partPosition, col, partPosition + 100.0f * partDirection, col, 2.0f);

			//ok, ok, i'll optimise this later 
			Matrix34 mat = pVehicle->GetEntity()->GetWorldTM();
			Matrix33 matO(mat);
			matO.Invert();

			const Vec3 diff = (m_TargetPos - partPosition).GetNormalized();

			m_destination.SetLerp(partDirection, diff, ctx.fFrameTime);

			Quat quat1;
			//quat1.SetRotationVDir(diff, 0.0f);
			quat1.SetRotationVDir(m_destination.GetNormalized(), 0.0f);
			Matrix33 mat2(quat1);
			mat2 = matO * mat2; 

			m_destination = m_destination * 10000.0f + partPosition;
			m_targetPosition = m_destination;
			m_aimPosition = m_destination;

			pPart->SetLocalTM(Matrix34(mat2, pPart->GetLocalTM(true, true).GetTranslation()));
		}
	}

  Base::Update(ctx, update);
}



