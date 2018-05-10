// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Laser accessory (re-factored from Crysis L.A.M.)

-------------------------------------------------------------------------
History:
- 11-6-2008   Created by Benito Gangoso Rodriguez

*************************************************************************/

#include "StdAfx.h"
#include "Laser.h"
#include "Actor.h"
#include "Weapon.h"
#include "Game.h"
#include "ItemSharedParams.h"
#include "UI/UIManager.h"
#include "UI/UICVars.h"
#include "ItemResourceCache.h"
#include "GameCVars.h"
#include "GameTypeInfo.h"
#include "Single.h"

#define LASER_UPDATE_TIME	0.2f
#define LASER_ATTACH_NAME "LASERBEAM"

CLaserBeam::SLaserUpdateDesc::SLaserUpdateDesc(const Vec3& laserPos, const Vec3& laserDir, float frameTime, bool bOwnerHidden)
:	m_laserPos(laserPos)
,	m_laserDir(laserDir)
,	m_frameTime(frameTime)
,	m_ownerCloaked(false)
,	m_weaponZoomed(false)
,	m_bOwnerHidden(bOwnerHidden)
{
}

CLaserBeam::CLaserBeam()
	: m_pLaserParams(nullptr)
	, m_lastHit(ZERO)
	, m_lastLaserUpdatePosition(ZERO)
	, m_lastLaserUpdateDirection(FORWARD_DIRECTION)
	, m_laserUpdateTimer(0.0f)
	, m_ownerEntityId(0)
	, m_laserEntityId(0)
	, m_queuedRayId(0)
	, m_geometrySlot(eIGS_FirstPerson)
	, m_laserDotSlot(-1)
	, m_laserGeometrySlot(-1)
	, m_laserOn(false)
	, m_hasHitData(false)
	, m_hitSolid(false)
	, m_usingEntityAttachment(false)
{
}

CLaserBeam::~CLaserBeam()
{
	DestroyLaserEntity();
	if (m_queuedRayId != 0)
	{
		g_pGame->GetRayCaster().Cancel(m_queuedRayId);
		m_queuedRayId = 0;
	}
}

void CLaserBeam::Initialize(EntityId owneEntityId, const SLaserParams* pParams, eGeometrySlot geometrySlot)
{
	m_geometrySlot = geometrySlot;
	m_ownerEntityId = owneEntityId;
	m_pLaserParams = pParams;

	if (m_laserEntityId)
	{
		//Check if entity is valid
		IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_laserEntityId);
		if(!pEntity)
			m_laserEntityId = 0;
	}
}

IEntity* CLaserBeam::CreateLaserEntity()
{
	IEntity* pLaserEntity = gEnv->pEntitySystem->GetEntity(m_laserEntityId);
	if (pLaserEntity)
		return pLaserEntity;

	IEntity* pOwnerEntity = gEnv->pEntitySystem->GetEntity(m_ownerEntityId);
	if (!pOwnerEntity)
		return 0;

	SEntitySpawnParams spawnParams;
	spawnParams.pClass = gEnv->pEntitySystem->GetClassRegistry()->GetDefaultClass();
	spawnParams.sName = "LaserBeam";
	spawnParams.nFlags = (pOwnerEntity->GetFlags() | ENTITY_FLAG_NO_SAVE) & ~ENTITY_FLAG_CASTSHADOW;

	IEntity* pNewEntity = gEnv->pEntitySystem->SpawnEntity(spawnParams);

	if(pNewEntity)
	{
		m_laserEntityId = pNewEntity->GetId();


		IEntityRender *pIEntityRender = pNewEntity->GetRenderInterface();
		IRenderNode * pRenderNode = pIEntityRender?pIEntityRender->GetRenderNode():NULL;

		if(pRenderNode)
			pRenderNode->SetRndFlags(ERF_RENDER_ALWAYS,true);

		SetLaserEntitySlots(false);
	}

	return pNewEntity;
}


IEntity* CLaserBeam::GetLaserEntity()
{
	IEntity* pLaserEntity = gEnv->pEntitySystem->GetEntity(m_laserEntityId);
	return pLaserEntity;
}


void CLaserBeam::TurnOnLaser()
{
	if(m_pLaserParams)
	{
		if (m_laserOn == false)
		{
			SetLaserEntitySlots(false);

			m_laserUpdateTimer = LASER_UPDATE_TIME;
			m_laserOn = true;
		}
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "LASER PARAMS: Item of type CLaser is missing it's laser params!");
	}
}


void CLaserBeam::TurnOffLaser()
{
	m_laserOn = false;
	SetLaserEntitySlots(true);
}

IAttachmentManager* CLaserBeam::GetLaserCharacterAttachmentManager()
{
	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	CItem* pOwnerItem = static_cast<CItem*>(pItemSystem->GetItem(m_ownerEntityId));

	if (pOwnerItem)
	{
		IEntity* pAttachedEntity = pOwnerItem->GetEntity();

		if(pOwnerItem->IsAccessory())
		{
			EntityId parentId = pOwnerItem->GetParentId();

			if(parentId)
			{
				if(IEntity* pParentEnt = gEnv->pEntitySystem->GetEntity(parentId))
				{
					pAttachedEntity = pParentEnt;
				}
			}
		}				

		if (ICharacterInstance *pCharacter = pAttachedEntity->GetCharacter(eIGS_FirstPerson))
		{
			return pCharacter->GetIAttachmentManager();				
		}
	}

	return NULL;
}

void CLaserBeam::FixAttachment(IEntity* pLaserEntity)
{
	m_usingEntityAttachment = false;

	IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
	CItem* pOwnerItem = static_cast<CItem*>(pItemSystem->GetItem(m_ownerEntityId));

	if (pOwnerItem)
	{
		IEntity* pOwnerEntity = pOwnerItem->GetEntity();
		IEntity* pAttachedEntity = pOwnerEntity;
		const char* attach_helper = "laser_term";

		Vec3 offset = pOwnerItem->GetSlotHelperPos(m_geometrySlot, attach_helper, false);

		if(m_geometrySlot == eIGS_FirstPerson)
		{
			if(pOwnerItem->IsAccessory())
			{
				EntityId parentId = pOwnerItem->GetParentId();

				if(parentId)
				{
					if(CItem* pParentItem = static_cast<CItem*>(pItemSystem->GetItem(parentId)))
					{
						const SAccessoryParams* pParams = pParentItem->GetAccessoryParams(pAttachedEntity->GetClass());

						attach_helper = pParams->attach_helper.c_str();
						pAttachedEntity = pParentItem->GetEntity();
					}
				}
			}

			if(pAttachedEntity)
			{
				ICharacterInstance *pCharacter = pAttachedEntity->GetCharacter(eIGS_FirstPerson);
				if (pCharacter)
				{
					IAttachmentManager *pAttachmentManager = pCharacter->GetIAttachmentManager();

					IAttachment *pLaserAttachment = pAttachmentManager->GetInterfaceByName(LASER_ATTACH_NAME);

					if(!pLaserAttachment)
					{
						IAttachment *pAttachment = pAttachmentManager->GetInterfaceByName(attach_helper);

						if(pAttachment)
						{
							const char* pBone = pCharacter->GetIDefaultSkeleton().GetJointNameByID(pAttachment->GetJointID());

							pLaserAttachment = pAttachmentManager->CreateAttachment(LASER_ATTACH_NAME, CA_BONE, pBone);

							if(pLaserAttachment)
							{
								QuatT relative = pAttachment->GetAttRelativeDefault();

								if(pOwnerItem->GetEntity() != pAttachedEntity)
								 {
									Matrix34 mtx(relative);
									relative.t = relative * offset;
								}

								pLaserAttachment->SetAttRelativeDefault(relative);
							}
						}
					}

					if(pLaserAttachment)
					{
						CEntityAttachment* pEntAttach = new CEntityAttachment;

						pEntAttach->SetEntityId(m_laserEntityId);
						pLaserAttachment->AddBinding(pEntAttach);
						pLaserAttachment->HideAttachment(0);

						m_usingEntityAttachment = true;
					}		
				}
			}
		}	

		if(!m_usingEntityAttachment && pOwnerEntity)
		{
			pOwnerEntity->AttachChild(pLaserEntity);
			pLaserEntity->SetLocalTM(Matrix34::CreateTranslationMat(offset));
		}
	}
}


void CLaserBeam::SetLaserEntitySlots(bool freeSlots)
{
	if(m_pLaserParams)
	{
		IEntity* pLaserEntity = GetLaserEntity();
		if(pLaserEntity)
		{
			if(freeSlots)
			{
				if(m_laserDotSlot != -1)
					pLaserEntity->FreeSlot(m_laserDotSlot);
				if(m_laserGeometrySlot != -1)
					pLaserEntity->FreeSlot(m_laserGeometrySlot);

				m_laserDotSlot = m_laserGeometrySlot = -1;

				pLaserEntity->DetachThis();

				if(m_usingEntityAttachment)
				{
					if(IAttachmentManager* pAttachmentManager = GetLaserCharacterAttachmentManager())
					{
						if(IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(LASER_ATTACH_NAME))
						{
							pAttachment->ClearBinding();
						}
					}
				}
			}
			else
			{
				m_laserGeometrySlot = pLaserEntity->LoadGeometry(-1, m_pLaserParams->laser_geometry_tp.c_str());
				pLaserEntity->SetSlotFlags(m_laserGeometrySlot, pLaserEntity->GetSlotFlags(m_laserGeometrySlot)|ENTITY_SLOT_RENDER);
				if (m_pLaserParams->show_dot)
				{
					IParticleEffect* pEffect = gEnv->pParticleManager->FindEffect(m_pLaserParams->laser_dot[GetIndexFromGeometrySlot()].c_str());
					if(pEffect)
						m_laserDotSlot = pLaserEntity->LoadParticleEmitter(-1,pEffect);
				}

				FixAttachment(pLaserEntity);
			}
		}
	}
	else
	{
		GameWarning("LASER PARAMS: Item of type CLaser is missing it's laser params!");
	}
}


void CLaserBeam::DestroyLaserEntity()
{
	if(IAttachmentManager* pAttachmentManager = GetLaserCharacterAttachmentManager())
	{
		if(IAttachment* pAttachment = pAttachmentManager->GetInterfaceByName(LASER_ATTACH_NAME))
		{
			pAttachment->ClearBinding();
			pAttachmentManager->RemoveAttachmentByInterface(pAttachment);
		}	
	}
	
	if (m_laserEntityId)
		gEnv->pEntitySystem->RemoveEntity(m_laserEntityId);
	m_laserEntityId = 0;
}


void CLaserBeam::UpdateLaser(const CLaserBeam::SLaserUpdateDesc& laserUpdateDesc)
{
	if(m_pLaserParams && m_laserOn)
	{
		IEntity* pLaserEntity = CreateLaserEntity();
		if (pLaserEntity)
		{
			m_lastLaserUpdatePosition = laserUpdateDesc.m_laserPos;
			m_lastLaserUpdateDirection = laserUpdateDesc.m_laserDir;

			m_laserUpdateTimer += laserUpdateDesc.m_frameTime;

			UpdateLaserGeometry(*pLaserEntity);

			if(m_laserUpdateTimer < LASER_UPDATE_TIME)
				return;

			m_laserUpdateTimer = cry_random(0.0f, LASER_UPDATE_TIME * 0.4f);

			if ((laserUpdateDesc.m_ownerCloaked && !laserUpdateDesc.m_weaponZoomed) || laserUpdateDesc.m_bOwnerHidden)
			{
				pLaserEntity->Hide(true);
				return;
			}
			pLaserEntity->Hide(false);

			const float range = m_pLaserParams->laser_range[GetIndexFromGeometrySlot()];

			// Use the same flags as the AI system uses for visbility.
			const int objects = ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid|ent_independent; //ent_living;
			const int flags = (geom_colltype_ray << rwi_colltype_bit) | rwi_colltype_any | (10 & rwi_pierceability_mask) | (geom_colltype14 << rwi_colltype_bit);

			//If we did not get a result, just cancel it, we will queue a new one again
			RayCastRequest::Priority requestPriority = RayCastRequest::MediumPriority;
			if (m_queuedRayId != 0)
			{
				g_pGame->GetRayCaster().Cancel(m_queuedRayId);
				m_queuedRayId = 0;
				requestPriority = RayCastRequest::HighPriority;
			}

			IItemSystem* pItemSystem = g_pGame->GetIGameFramework()->GetIItemSystem();
			IPhysicalEntity* pSkipEntity = NULL;
			uint8 numSkips = 0;

			CItem* pItem = static_cast<CItem*>(pItemSystem->GetItem(m_ownerEntityId));

			if(pItem)
			{
				if(pItem->IsAccessory())
				{
					CItem* pParentItem = static_cast<CItem*>(pItemSystem->GetItem(pItem->GetParentId()));
				
					if(pParentItem)
					{
						pItem = pParentItem;
					}					
				}

				IEntity* pOwnerEnt = 0;
				CWeapon* pWeapon = static_cast<CWeapon*>(pItem->GetIWeapon());
				if (pWeapon && pWeapon->GetHostId() != 0)
				{
					pOwnerEnt = gEnv->pEntitySystem->GetEntity(pWeapon->GetHostId());
				}
				else
				{
					pOwnerEnt = pItem->GetOwner();
				}

				if(pOwnerEnt)
				{
					IPhysicalEntity* pOwnerPhysics = pOwnerEnt->GetPhysics();

					if(pOwnerPhysics)
					{
						pSkipEntity = pOwnerPhysics;
						numSkips++;
					}
				}
			}

			m_queuedRayId = g_pGame->GetRayCaster().Queue(
				requestPriority,
				RayCastRequest(laserUpdateDesc.m_laserPos, laserUpdateDesc.m_laserDir*range,
				objects,
				flags,
				&pSkipEntity,
				numSkips),
				functor(*this, &CLaserBeam::OnRayCastDataReceived));
		}

	}
	else if (!m_pLaserParams)
	{
		GameWarning("LASER PARAMS: Item of type CLaser is missing it's laser params!");
	}
}

void CLaserBeam::OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(m_pLaserParams);
	CRY_ASSERT(rayID == m_queuedRayId);

	m_queuedRayId = 0;

	const float range = m_pLaserParams->laser_range[eIGS_ThirdPerson];
	float laserLength = range;

	Vec3 hitPos(0,0,0);
	bool hitSolid = false;

	if (result.hitCount > 0)
	{
		laserLength = result.hits[0].dist;
		hitPos = result.hits[0].pt;
		hitSolid = true;
	}
	else
	{
		hitPos = m_lastLaserUpdatePosition + (m_lastLaserUpdateDirection * range);
		laserLength = range + 0.1f;
	}

	const CCamera& camera = GetISystem()->GetViewCamera();

	// Hit near plane
	if (m_lastLaserUpdateDirection.Dot(camera.GetViewdir()) < 0.0f)
	{
		Plane nearPlane;
		nearPlane.SetPlane(camera.GetViewdir(), camera.GetPosition());
		nearPlane.d -= camera.GetNearPlane()+0.15f;
		Ray ray(m_lastLaserUpdatePosition, m_lastLaserUpdateDirection);
		Vec3 out;
		if (Intersect::Ray_Plane(ray, nearPlane, out))
		{
			float dist = Distance::Point_Point(m_lastLaserUpdatePosition, out);
			if (dist < laserLength)
			{
				laserLength = dist;
				hitPos = out;
				hitSolid = true;
			}
		}
	}

	m_hasHitData = true;
	m_lastHit = hitPos;
	m_hitSolid = hitSolid;


}

void CLaserBeam::UpdateLaserGeometry( IEntity& laserEntity )
{
	// Scale the laser based on the distance.
	const float assetLengthInv = 0.5f;
	const float bias = g_pGameCVars->i_laser_hitPosOffset;

	const float finalLaserLen = m_hasHitData ? max(0.f, m_lastLaserUpdatePosition.GetDistance(GetLastHit()) - bias) : 0.001f;

	const float scale = finalLaserLen * assetLengthInv;
	const float thickness = m_pLaserParams->laser_thickness[GetIndexFromGeometrySlot()];

	const Quat inverseWorldQuat(laserEntity.GetWorldRotation().GetInverted());
	Vec3 localSpaceDirection = inverseWorldQuat * m_lastLaserUpdateDirection;

	const Vec3 finalHitPos = localSpaceDirection * finalLaserLen;

	const Matrix34 localLaserMatrix = Matrix33::CreateOrientation(localSpaceDirection, Vec3(0.f, 0.f, 1.f), 0.f) *
		Matrix34::CreateScale(Vec3(thickness, scale, thickness));

	laserEntity.SetSlotLocalTM(m_laserGeometrySlot, localLaserMatrix);

	// Set Dot matrix
	if (m_hitSolid)
	{
		const Matrix34 mt = Matrix34::CreateTranslationMat(finalHitPos);
		laserEntity.SetSlotLocalTM(m_laserDotSlot, mt);
	}
	else
	{
		Matrix34 scaleMatrix;
		scaleMatrix.SetIdentity();
		scaleMatrix.SetScale(Vec3(0.001f,0.001f,0.001f));
		laserEntity.SetSlotLocalTM(m_laserDotSlot, scaleMatrix);
	}
}



int CLaserBeam::GetIndexFromGeometrySlot()
{
	return m_geometrySlot == eIGS_FirstPerson ? 0 : 1;
}



void CLaserBeam::SetGeometrySlot(eGeometrySlot geometrySlot)
{
	if (geometrySlot == m_geometrySlot)
		return;
	if (m_laserOn)
	{
		SetLaserEntitySlots(true);
		m_geometrySlot = geometrySlot;
		SetLaserEntitySlots(false);
	}
	else
	{
		m_geometrySlot = geometrySlot;
	}
}


//-----------------------------------------------------


CLaser::CLaser()
{
}


CLaser::~CLaser()
{
	CWeapon* pWeapon = GetWeapon();
	if (pWeapon)
		pWeapon->SetFiringLocator(0);
}


void CLaser::Reset()
{
	CAccessory::Reset();
	
	DrawSlot(eIGS_ThirdPersonAux, false);
	DrawSlot(eIGS_Aux1,false);
	m_laserBeam.TurnOffLaser();
}

bool CLaser::ResetParams()
{
	bool success = BaseClass::ResetParams();

	if(success)
	{
		m_laserBeam.SetParams(m_sharedparams->pLaserParams);
	}
	
	return success;
}


bool CLaser::Init(IGameObject * pGameObject )
{
	if(!CAccessory::Init(pGameObject))
		return false;

	eGeometrySlot geometrySlot = eIGS_FirstPerson;
	m_laserBeam.Initialize(GetEntityId(), m_sharedparams->pLaserParams, geometrySlot);

	return true;
}

//-------------------------------------------------
void CLaser::OnAttach(bool attach)
{	
	if(attach)
	{
		CWeapon* pParent = GetWeapon();

		if((pParent == NULL) || pParent->GetStats().selected)
		{
			TurnOnLaser();
		}
	}
	else
	{
		TurnOffLaser();
	}
}

//-----------------------------------------------
void CLaser::OnParentSelect(bool select)
{
	if(select)
	{
		TurnOnLaser();
	}
	else
	{
		TurnOffLaser();
	}
}

//---------------------------------------------
void CLaser::ActivateLaser(bool activate)
{
	if(activate)
		TurnOnLaser(true);
	else
		TurnOffLaser(true);
}

//---------------------------------------------
CWeapon* CLaser::GetWeapon() const
{
	IItem* pParentItem = m_pItemSystem->GetItem(GetParentId());
	if(pParentItem == NULL)
		return 0;
	CWeapon* pParentWeapon = static_cast<CWeapon*>(pParentItem->GetIWeapon());
	return pParentWeapon;
}

//--------------------------------------------
void CLaser::TurnOnLaser(bool manual /*= false*/)
{
	CWeapon* pParentWeapon = GetWeapon();
	if(pParentWeapon == NULL)
		return;

	m_laserHelperFP.clear();
	const SAccessoryParams *params = GetWeapon()->GetAccessoryParams(GetEntity()->GetClass());
	if (params)
		m_laserHelperFP = params->attach_helper;

	int slot = pParentWeapon->IsOwnerFP() ? eIGS_FirstPerson: eIGS_ThirdPerson;

	CActor* pOwner = pParentWeapon->GetOwnerActor();
	GetGameObject()->EnableUpdateSlot(this, eIUS_General);
	if (pOwner && pOwner->IsPlayer())
		pParentWeapon->SetFiringLocator(this);
	m_laserBeam.TurnOnLaser();

	//Turn off crosshair
	if (pParentWeapon->IsOwnerClient())
	{
		pParentWeapon->SetCrosshairMode(CWeapon::eWeaponCrossHair_ForceOff);
		pParentWeapon->FadeCrosshair(0.0f, g_pGame->GetUI()->GetCVars()->hud_Crosshair_laser_fadeOutTime);
	}
}


//--------------------------------------------
void CLaser::TurnOffLaser(bool manual /*= false*/)
{
	if(!m_laserBeam.IsLaserActivated())
		return;

	GetGameObject()->DisableUpdateSlot(this, eIUS_General);

	CWeapon* pParentWeapon = GetWeapon();
	if(pParentWeapon == NULL)
		return;
	bool ownerIsFP = pParentWeapon? pParentWeapon->IsOwnerFP(): false;

	m_laserBeam.TurnOffLaser();

	//Turn on crosshair
	if (pParentWeapon->IsOwnerClient())
	{
		pParentWeapon->SetCrosshairMode(CWeapon::eWeaponCrossHair_Default);
		pParentWeapon->FadeCrosshair(1.0f, g_pGame->GetUI()->GetCVars()->hud_Crosshair_laser_fadeInTime);
	}

	pParentWeapon->SetFiringLocator(0);
}

//-------------------------------------------
void CLaser::Update(SEntityUpdateContext& ctx, int slot)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	if((slot == eIUS_General) && m_laserBeam.IsLaserActivated())
	{
		RequireUpdate(eIUS_General);

		CWeapon* pParentWeapon = GetWeapon();
		if (pParentWeapon)
		{
			CActor* pOwnerActor = pParentWeapon->GetOwnerActor();
			Vec3 laserPos, laserDir;
			GetLaserPositionAndDirection(pParentWeapon, laserPos, laserDir);

			CLaserBeam::SLaserUpdateDesc laserUpdateDesc(laserPos, laserDir, ctx.fFrameTime, pParentWeapon->GetEntity()->IsHidden());
			laserUpdateDesc.m_weaponZoomed = pParentWeapon->IsZoomed() || pParentWeapon->IsZoomingInOrOut();

			m_laserBeam.UpdateLaser(laserUpdateDesc);
		}
	}
}

//--------------------------------------------
void CLaser::GetLaserPositionAndDirection(CWeapon* pParentWeapon, Vec3& pos, Vec3& dir)
{
	const char* entityLocationHelper = m_laserHelperFP.c_str();
	const char* laserTermHelper = "laser_term";
	const bool relative = false;
	const bool absolute = true;

	const int slot = pParentWeapon->IsOwnerFP() ? eIGS_FirstPerson : eIGS_ThirdPerson;

	Matrix34 entityLocation =
		Matrix34::CreateTranslationMat(pParentWeapon->GetSlotHelperPos(slot, entityLocationHelper, absolute)) *
		pParentWeapon->GetSlotHelperRotation(slot, entityLocationHelper, absolute);

	Matrix34 helperLocation =
		Matrix34::CreateTranslationMat(GetSlotHelperPos(slot, laserTermHelper, relative)) *
		GetSlotHelperRotation(slot, laserTermHelper, relative);

	Matrix34 finalLocation = entityLocation * helperLocation;

	pos = finalLocation.GetTranslation();
	dir = finalLocation.GetColumn1();
}

//--------------------------------------------
void CLaser::OnEnterFirstPerson()
{
	CAccessory::OnEnterFirstPerson();

	m_laserBeam.SetGeometrySlot(eIGS_FirstPerson);

	if (m_laserBeam.IsLaserActivated())
	{	
		TurnOffLaser();
		TurnOnLaser();
	}
}

//---------------------------------------------
void CLaser::OnEnterThirdPerson()
{
	CAccessory::OnEnterThirdPerson();

	m_laserBeam.SetGeometrySlot(eIGS_ThirdPerson);

	if (m_laserBeam.IsLaserActivated())
	{	
		TurnOffLaser();
		TurnOnLaser();
	}
}


//--------------------------------------------

bool CLaser::GetProbableHit(EntityId weaponId, const IFireMode* pFireMode, Vec3& hit)
{
	if (!m_laserBeam.IsLaserActivated())
		return false;

	if(gEnv->bMultiplayer)
	{
		CWeapon* pParentWeapon = GetWeapon();
		if (pParentWeapon && pParentWeapon->IsZoomed())
		{
			return false;
		}
	}

	CWeapon* pWeapon = GetWeapon();
	int slot = pWeapon->IsOwnerFP() ? eIGS_FirstPerson : eIGS_ThirdPerson;

	Vec3 lastBeamHit = m_laserBeam.GetLastHit();
	Vec3 currentBeamPosition = pWeapon->GetSlotHelperPos(slot, "weapon_term", true);
	Matrix33 rotation = pWeapon->GetSlotHelperRotation(slot, "weapon_term", true);
	Vec3 currentBeamDirection = rotation.GetColumn1();

	const CFireMode* pCFireMode = static_cast<const CFireMode*>(pFireMode);
	const CSingle* pSingle = crygti_cast<const CSingle*>(pCFireMode);
	if (pSingle && pSingle->GetShared()->fireparams.laser_beam_uses_spread)
	{
		currentBeamDirection = pSingle->ApplySpread(currentBeamDirection, pSingle->GetSpread());
	}

	float distanceToLastHit = lastBeamHit.GetDistance(currentBeamPosition);
	hit = currentBeamPosition + currentBeamDirection * distanceToLastHit;

	return true;
}


bool CLaser::GetFiringPos(EntityId weaponId, const IFireMode* pFireMode, Vec3& pos)
{
	CWeapon* pParentWeapon = GetWeapon();

	if (!m_laserBeam.IsLaserActivated())
		return false;
	
	if (pParentWeapon && (!gEnv->bMultiplayer ||!pParentWeapon->IsZoomed()))
	{
		int slot = pParentWeapon->IsOwnerFP() ? eIGS_FirstPerson : eIGS_ThirdPerson;

		pos = pParentWeapon->GetSlotHelperPos(slot, "weapon_term", true);
		return true;
	}

	return false;
}


bool CLaser::GetFiringDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	CWeapon* pParentWeapon = GetWeapon();

	if (!m_laserBeam.IsLaserActivated())
		return false;

	if(gEnv->bMultiplayer && pParentWeapon && pParentWeapon->IsZoomed())
	{
		return false;
	}

	if(!probableHit.IsZero() && !firingPos.IsZero())
	{
		dir = (probableHit - firingPos).GetNormalized();

		return true;
	}


	if (pParentWeapon)
	{
		int slot = pParentWeapon->IsOwnerFP() ? eIGS_FirstPerson : eIGS_ThirdPerson;

		Matrix33 rotation = pParentWeapon->GetSlotHelperRotation(slot, "weapon_term", true);

		dir = rotation.GetColumn1();
		return true;
	}

	return false;
}


bool CLaser::GetActualWeaponDir(EntityId weaponId, const IFireMode* pFireMode, Vec3& dir, const Vec3& probableHit, const Vec3& firingPos)
{
	return false;
}


bool CLaser::GetFiringVelocity(EntityId weaponId, const IFireMode* pFireMode, Vec3& vel, const Vec3& firingDir)
{
	return false;
}


void CLaser::WeaponReleased()
{
}
