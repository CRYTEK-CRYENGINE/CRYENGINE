// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StickyProjectile.h"
#include "Player.h"
#include "WeaponSystem.h"
#include "GamePhysicsSettings.h"

namespace
{
	int GetJointIdFromPartId(IEntity& entity, const int partId, Vec3* pReturnPos = NULL)
	{
		ICharacterInstance* pCharInst = entity.GetCharacter(0);
		int jointId = EntityPhysicsUtils::GetSlotIdx(partId, pCharInst && pCharInst->GetObjectType()==CGA);
		CRY_ASSERT(pCharInst);
		if(pCharInst)
		{
			if(IAttachment* pAttachment = pCharInst->GetIAttachmentManager()->GetInterfaceByPhysId(jointId))
			{
				if(pReturnPos)
				{
					*pReturnPos = pAttachment->GetAttWorldAbsolute().t;
				}
				return pAttachment->GetJointID();
			}
		}
		if(pReturnPos && pCharInst)
		{
			*pReturnPos = entity.GetWorldTM().TransformPoint(pCharInst->GetISkeletonPose()->GetAbsJointByID(jointId).t);
		}
		return jointId;
	}

	struct SStuckSer
	{
		SStuckSer()
			:	pThis(0)
			,	pProjectile(0) {}

		SStuckSer(CStickyProjectile* _pThis, CProjectile* _pProjectile)
			:	pThis(_pThis)
			,	pProjectile(_pProjectile) {}

		bool IsStuck() const {return pThis->IsStuck();}
		void SetStuck(bool stuck) {return pThis->NetSetStuck(pProjectile, stuck);}

	private:
		CStickyProjectile* pThis;
		CProjectile* pProjectile;
	} s_stuckSer;

	int s_attachNameID = 0;
}

CStickyProjectile::SStickParams::SStickParams(CProjectile* pProjectile, EventPhysCollision *pCollision, EOrientation orientation )
	:	m_pProjectile(pProjectile)
	,	m_ownerId(pProjectile->GetOwnerId())
	,	m_stickPosition(pCollision->pt)
	,	m_stickNormal(orientation==eO_AlignToSurface?pCollision->n:pCollision->vloc[0].GetNormalizedSafe(pCollision->n))
	, m_bStickToFriendlies(false)
{
	int trgId = 1;
	int srcId = 0;
	m_pTarget = pCollision->pEntity[trgId];

	if (m_pTarget == m_pProjectile->GetEntity()->GetPhysics())
	{
		trgId = 0;
		srcId = 1;
		m_pTarget = pCollision->pEntity[trgId];
	}

	m_targetPartId = pCollision->partid[trgId];
	m_targetMatId = pCollision->idmat[trgId];

	if(gEnv->bMultiplayer)
	{
		m_bStickToFriendlies = true;
	}
}

CStickyProjectile::SStickParams::SStickParams(CProjectile* pProjectile, const HitInfo& rHitInfo, EOrientation orientation )
	: m_pProjectile(pProjectile)
	, m_ownerId(pProjectile->GetOwnerId())
	, m_bStickToFriendlies(false)
{
	IEntity * pTargetEntity = gEnv->pEntitySystem->GetEntity(rHitInfo.targetId);
	m_pTarget				= pTargetEntity->GetPhysics();
	m_targetPartId	= rHitInfo.partId;
	m_targetMatId		= rHitInfo.material;

	m_stickNormal		= rHitInfo.normal;

	//Hitinfo doesn't provide a precise position to stick to, so we'll have to generate
	//	 one from the part ID.
	if((rHitInfo.partId != uint16(-1)) && pTargetEntity && pTargetEntity->GetCharacter(0))
	{
		// perhaps it's an attachment?
		Vec3 position(0.f,0.f,0.f);
		GetJointIdFromPartId( *pTargetEntity, rHitInfo.partId, &position );
		m_stickPosition = position;
	}

	if(gEnv->bMultiplayer)
	{
		m_bStickToFriendlies = true;
	}
}

CStickyProjectile::CStickyProjectile(bool bOrientateToCollisionNormal, bool bHandleLocally)
	:	m_characterAttachmentCrC(0)
	,	m_stuckPos(ZERO)
	,	m_stuckRot(IDENTITY)
	,	m_parentId(0)
	, m_childId(0)
	,	m_stuckJoint(0)
	, m_flags((bOrientateToCollisionNormal?eSF_OrientateToCollNormal:0)|(bHandleLocally?eSF_HandleLocally:0))
	,	m_stuckPartId(0)
{
}

CStickyProjectile::~CStickyProjectile()
{
	m_childId = 0;
	UnStick();
	SetParentId(0);
}

bool CStickyProjectile::IsStuckToPlayer() const
{
	static const TFlags stuckToPlayer = eSF_IsStuck|eSF_StuckToPlayer;
	return (m_flags&stuckToPlayer)==stuckToPlayer;
}

bool CStickyProjectile::IsStuckToAI() const
{
	static const TFlags stuckToAI = eSF_IsStuck|eSF_StuckToAI;
	return (m_flags&stuckToAI)==stuckToAI;
}

bool CStickyProjectile::IsValid() const
{
	if (!IsStuck())
		return false;

	if(m_parentId == 0)
		return true;

	pe_params_part part;
	part.partid = m_stuckPartId;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(m_parentId);
	if(pEntity)
	{
		if (pEntity->IsHidden())
		{
			return false;
		}
		if(ICharacterInstance* pCharacter = pEntity->GetCharacter(0))
		{
			if(ISkeletonPose* pSkel = pCharacter->GetISkeletonPose())
			{
				IPhysicalEntity* pSkelPhysics = pSkel->GetCharacterPhysics();
				if (pSkelPhysics)
				{
					return pSkelPhysics->GetParams(&part) != 0;
				}
			}
		}
		else
		{
			IPhysicalEntity* pPhysics = pEntity->GetPhysics();
			if (pPhysics)
			{
				return pPhysics->GetParams(&part) != 0;
			}
		}
	}

	return false;
}

void CStickyProjectile::SwapToCorpse( const EntityId corpseId )
{
	SetParentId(corpseId);
}

void CStickyProjectile::Reset()
{
	m_stuckPos.zero();
	m_stuckRot.SetIdentity();
	m_flags &= ~eSF_Reset;
	SetParentId(0);
	m_childId = 0;
}

void CStickyProjectile::Stick(const SStickParams& stickParams)
{
	if(!gEnv->bServer && ((m_flags&eSF_HandleLocally)==0))
		return;

	// Must have physics.
	if(!stickParams.m_pTarget)
		return;

	// Don't stick to Breakables.
	if(ISurfaceType *pSurfaceType = gEnv->p3DEngine->GetMaterialManager()->GetSurfaceType(stickParams.m_targetMatId))
	{
		if (pSurfaceType->GetBreakability() == 1)
			return;
	}

	// Ignore water collision
	if ( CProjectile::GetMaterialLookUp().IsMaterial( stickParams.m_targetMatId, CProjectile::SMaterialLookUp::eType_Water ) )
		return;

	// Don't stick to particles type physics.
	const int peType = stickParams.m_pTarget->GetType();
	if( peType == PE_PARTICLE )
		return;

	const int iForiegnData = stickParams.m_pTarget->GetiForeignData();
	IEntity* pTargetEntity = iForiegnData==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)stickParams.m_pTarget->GetForeignData(PHYS_FOREIGN_ID_ENTITY) : NULL;
	if( pTargetEntity )
	{
		const EntityId targetEntityId = pTargetEntity->GetId();

		// Don't stick to owner.
		if(targetEntityId==stickParams.m_ownerId)
			return;

		//Do not attach to other projectiles
		if(g_pGame->GetWeaponSystem()->GetProjectile(targetEntityId))
			return;

		// Do Stick.
		if(!StickToEntity(stickParams, pTargetEntity))
			return;
	}
	else
	{
		// Without an entity, we expect the object to not be moving. So if it isn't the terrain or a static, then don't stick to it.
		if( iForiegnData!=PHYS_FOREIGN_ID_TERRAIN && iForiegnData!=PHYS_FOREIGN_ID_STATIC )
			return;

		// Do Stick.
		if(!StickToStatic(stickParams))
			return;
	}

	//////////////////////////////////////////////////////////////////////////

	IEntity* pProjectileEntity = stickParams.m_pProjectile->GetEntity();
	if(CWeapon* pWeapon = stickParams.m_pProjectile->GetWeapon())
	{
		pWeapon->OnProjectileCollided( pProjectileEntity->GetId(), stickParams.m_pTarget, stickParams.m_stickPosition );
	}

	if((m_flags&eSF_HandleLocally)==0)
	{
		CHANGED_NETWORK_STATE(stickParams.m_pProjectile, eEA_GameServerStatic);
	}

	SendHitInfo(stickParams.m_pProjectile, pTargetEntity);

	IPhysicalEntity* pPhysEnt = pProjectileEntity->GetPhysics();
	if(((m_flags&eSF_IsStuck)!=0) && pPhysEnt)
	{
		pe_params_part pp;
		pp.flagsAND = ~geom_colltype_explosion;
		pp.flagsColliderAND = ~geom_colltype_solid;
		pp.mass = 0.0f;
		pPhysEnt->SetParams(&pp);
	}
}

bool CStickyProjectile::StickToStatic( const SStickParams& stickParams )
{
	m_stuckNormal = stickParams.m_stickNormal;
	m_stuckPartId = stickParams.m_targetPartId;

	IEntity* pProjectileEntity = stickParams.m_pProjectile->GetEntity();
	QuatT loc;
	CalculateLocationForStick( *pProjectileEntity, stickParams.m_stickPosition, stickParams.m_stickNormal, loc );
	pProjectileEntity->SetWorldTM(Matrix34(loc));

	m_stuckPos = loc.t;
	m_stuckRot = loc.q;
	AttachTo(stickParams.m_pProjectile, NULL);

	m_childId = pProjectileEntity->GetId();
	m_flags |= eSF_IsStuck;
	return true;
}

bool CStickyProjectile::StickToEntity( const SStickParams& stickParams, IEntity* pTargetEntity )
{
	IEntity* pProjectileEntity = stickParams.m_pProjectile->GetEntity();
	ICharacterInstance* pCharInstance = pTargetEntity->GetCharacter(0);
	if( pCharInstance)	
	{
		IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTargetEntity->GetId());
		if (!pActor || (stickParams.m_bStickToFriendlies || !pActor->IsFriendlyEntity(stickParams.m_ownerId)) && (gEnv->bMultiplayer || !pActor->IsDead()))
		{
			m_stuckJoint = GetJointIdFromPartId(*pTargetEntity, stickParams.m_targetPartId);
			m_stuckNormal = stickParams.m_stickNormal;
			m_stuckPartId = stickParams.m_targetPartId;

			IDefaultSkeleton& rIDefaultSkeleton = pCharInstance->GetIDefaultSkeleton();
			ISkeletonPose* pSkeleton = pCharInstance->GetISkeletonPose();
			const char* boneName = rIDefaultSkeleton.GetJointNameByID(m_stuckJoint);
			const QuatT jointWorld = QuatT(pTargetEntity->GetWorldTM()) * pSkeleton->GetAbsJointByID(m_stuckJoint);
			QuatT loc;
			CalculateLocationForStick( *pProjectileEntity, stickParams.m_stickPosition, stickParams.m_stickNormal, loc );
			pProjectileEntity->SetWorldTM(Matrix34(loc));

			// Get the local pos and rot.
			loc = jointWorld.GetInverted() * loc;
			m_stuckPos = loc.t;
			m_stuckRot = loc.q;

			// Attach.
			if(AttachToCharacter( stickParams.m_pProjectile, *pTargetEntity, *pCharInstance, boneName))
			{
				m_flags |= eSF_IsStuck;
				m_flags |= pActor ? pActor->IsPlayer() ? eSF_StuckToPlayer : eSF_StuckToAI : 0;
				SetParentId(pTargetEntity->GetId());
				m_childId = pProjectileEntity->GetId();
				return true;
			}
		}
	}
	else
	{
		m_stuckNormal = stickParams.m_stickNormal;
		m_stuckPartId = stickParams.m_targetPartId;

		QuatT loc;
		CalculateLocationForStick( *pProjectileEntity, stickParams.m_stickPosition, stickParams.m_stickNormal, loc );

		AttachTo(stickParams.m_pProjectile, pTargetEntity);

		pProjectileEntity->SetWorldTM(Matrix34(loc));

		// Set as Stuck.
		SetParentId(pTargetEntity->GetId());
		m_childId = pProjectileEntity->GetId();
		m_flags |= eSF_IsStuck;

		//Store position and rotation relative to parent entity
		m_stuckPos = pProjectileEntity->GetPos();
		m_stuckRot = pProjectileEntity->GetRotation();
		return true;
	}

	return false;
}

void CStickyProjectile::AttachTo(CProjectile* pProjectile, IEntity* pTargetEntity)
{
	IEntity* pProjectileEntity = pProjectile->GetEntity();

	if(pTargetEntity)
	{
		pTargetEntity->AttachChild(pProjectileEntity);
		SetProjectilePhysics(pProjectile, ePT_Static);
	}
	else
	{
		SetProjectilePhysics(pProjectile, ePT_Static);
	}
}

bool CStickyProjectile::AttachToCharacter(CProjectile* pProjectile, IEntity& pEntity, ICharacterInstance& pCharacter, const char* boneName)
{
	IEntity* pProjectileEntity = pProjectile->GetEntity();

	char attachName[16];
	cry_sprintf(attachName, "StickyProj_%d", s_attachNameID++);

	IAttachment* pCharacterAttachment = pCharacter.GetIAttachmentManager()->CreateAttachment(attachName, CA_BONE, boneName, false); 

	if(!pCharacterAttachment)
	{
		CryLogAlways("Could not create attachment for StickyProjectile[%s]. AttachmentName[%s] BoneName[%s]", pProjectileEntity->GetName(), attachName, boneName );
		CRY_ASSERT_MESSAGE(pCharacterAttachment, "Could not create attachment for StickyProjectile. This must be fixed.");
		return false;
	}

	m_characterAttachmentCrC = pCharacterAttachment->GetNameCRC();

	SetProjectilePhysics(pProjectile, ePT_None);

	pCharacterAttachment->SetAttRelativeDefault(QuatT(m_stuckRot, m_stuckPos));

	CEntityAttachment *pEntityAttachment = new CEntityAttachment();
	pEntityAttachment->SetEntityId(pProjectileEntity->GetId());

	pCharacterAttachment->AddBinding(pEntityAttachment);

	return true;
}

void CStickyProjectile::SetProjectilePhysics(CProjectile* pProjectile, EPhysicalizationType physics)
{
	pProjectile->GetGameObject()->SetAspectProfile( eEA_Physics, physics);
}

void CStickyProjectile::UnStick()
{
	if((m_flags&eSF_IsStuck)==0)
		return;

	const EntityId childId = m_childId;

	if(m_characterAttachmentCrC)
	{
		if(IEntity* pParentEntity = gEnv->pEntitySystem->GetEntity(m_parentId))
		{
			if(ICharacterInstance* pCharacterInstance = pParentEntity->GetCharacter(0))
			{
				IAttachmentManager* pAttachManager = pCharacterInstance->GetIAttachmentManager();
				Reset();
				pAttachManager->RemoveAttachmentByNameCRC(m_characterAttachmentCrC);
				m_characterAttachmentCrC = 0;
			}
		}
	}
	else
	{
		if(IEntity* pChildEntity = gEnv->pEntitySystem->GetEntity(childId))
		{
			Reset();
			pChildEntity->DetachThis(IEntity::ATTACHMENT_KEEP_TRANSFORMATION);
		}
	}

	if(gEnv->bMultiplayer)
	{
		if((m_flags&eSF_IsStuck)==0)
		{
			if(CProjectile* pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(childId))
			{
				SetProjectilePhysics(pProjectile, ePT_Particle);
			}
		}
	}
}

void CStickyProjectile::Serialize(TSerialize ser)
{
}

void CStickyProjectile::NetSerialize(CProjectile* pProjectile, TSerialize ser, EEntityAspects aspect)
{
	if(aspect == ASPECT_STICK)
	{
		ser.Value("pos", m_stuckPos, 'lwld');
		ser.Value("rot", m_stuckRot, 'ori1');
		ser.Value("joint", m_stuckJoint, 'i16');
		bool isStuckToEntity = m_parentId > 0;
		EntityId parentId = m_parentId;
		ser.Value("isStuckToEntity", isStuckToEntity, 'bool');
		ser.Value("stuckEntity", parentId, 'eid');
		SetParentId(parentId);

		if(isStuckToEntity && !m_parentId && ser.IsReading()) //Still waiting to resolve entity Id
		{
			ser.FlagPartialRead();
			return;
		}

		s_stuckSer = SStuckSer(this, pProjectile);
		ser.Value(
			"stuck",
			&s_stuckSer,
			&SStuckSer::IsStuck,
			&SStuckSer::SetStuck,
			'bool');
	}
}

NetworkAspectType CStickyProjectile::GetNetSerializeAspects()
{
	return ASPECT_STICK;
}

void CStickyProjectile::NetSetStuck(CProjectile* pProjectile, bool stuck)
{
	if(stuck && ((m_flags&eSF_IsStuck)==0))
	{
		IEntity* pTargetEntity = gEnv->pEntitySystem->GetEntity(m_parentId);
		if(pTargetEntity)
		{
			if(ICharacterInstance* pTargetCharacter = pTargetEntity->GetCharacter(0))
			{
				const char* boneName = pTargetCharacter->GetIDefaultSkeleton().GetJointNameByID(m_stuckJoint);
				if(AttachToCharacter(pProjectile, *pTargetEntity, *pTargetCharacter, boneName))
				{
					IActor* pActor = g_pGame->GetIGameFramework()->GetIActorSystem()->GetActor(pTargetEntity->GetId());
					m_flags |= eSF_IsStuck;
					m_flags |= pActor ? pActor->IsPlayer() ? eSF_StuckToPlayer : eSF_StuckToAI : 0;
					m_childId = pProjectile->GetEntityId();
				}
			}
		}
		if((m_flags&eSF_IsStuck)==0)
		{
			IEntity* pProjectileEntity = pProjectile->GetEntity();
			AttachTo(pProjectile, pTargetEntity);
			m_childId = pProjectileEntity->GetId();
			if(pTargetEntity) //If we have a parent then the stuck position/rotation are local to the parent
			{
				pProjectileEntity->SetPos(m_stuckPos);
				pProjectileEntity->SetRotation(m_stuckRot);
			}
			else if(m_flags&eSF_OrientateToCollNormal)
			{
				Matrix34 mat;
				mat.SetTranslation(m_stuckPos);
				mat.SetRotation33(Matrix33(m_stuckRot));
				pProjectileEntity->SetWorldTM(mat);
			}
			else
			{
				pProjectileEntity->SetPos(m_stuckPos);
			}
			m_flags |= eSF_IsStuck;
		}
	}
}

void CStickyProjectile::CalculateLocationForStick( const IEntity& projectile, const Vec3& collPos, const Vec3& collNormal, QuatT& outLocation ) const
{
	if(m_flags&eSF_OrientateToCollNormal)
	{
		const float bigz = fabs_tpl(collNormal.z)-fabs_tpl(collNormal.y);
		const Vec3 temp(0.f,(float)__fsel(bigz,1.f,0.f),(float)__fsel(bigz,0.f,1.f));
		outLocation.q = Quat(Matrix33::CreateOrientation( temp.Cross(collNormal), -collNormal, 0));

		AABB aabb;
		projectile.GetLocalBounds(aabb);
		outLocation.t = collPos + (outLocation.q.GetColumn2() * ((aabb.max.y-aabb.min.y)*0.5f));
	}
	else
	{
		outLocation.q = projectile.GetRotation();
		outLocation.t = collPos + (outLocation.q.GetColumn1() * 0.1f);
	}
}

void CStickyProjectile::SendHitInfo(const CProjectile* pProjectile, IEntity* pTargetEntity)
{
	if( !gEnv->bMultiplayer )
	{
		EntityId targetId = pTargetEntity ? pTargetEntity->GetId() : 0;
		HitInfo hitInfo;
		hitInfo.shooterId = pProjectile->GetOwnerId();
		hitInfo.weaponId = pProjectile->GetEntityId();
		hitInfo.targetId = targetId;
		hitInfo.damage = 0.0f;
		hitInfo.dir = -m_stuckNormal;
		hitInfo.normal = m_stuckNormal; // this has to be in an opposite direction from the hitInfo.dir or the hit is ignored as a 'backface' hit
		hitInfo.type = pProjectile->GetHitType();
		g_pGame->GetGameRules()->ClientHit(hitInfo);
	}
}

void CStickyProjectile::OnEntityEvent( IEntity *pEntity, const SEntityEvent& event )
{
	switch(event.event)
	{
	case ENTITY_EVENT_DONE:
		{
			const EntityId entityId = (EntityId)event.nParam[0];
			if(entityId==m_parentId)
			{
				UnStick();
			}
		}
		break;
	}
}

void CStickyProjectile::SetParentId( const EntityId newParentId )
{
	if(m_parentId!=newParentId)
	{
		if(m_parentId)
			gEnv->pEntitySystem->RemoveEntityEventListener(m_parentId, ENTITY_EVENT_DONE, this);

		m_parentId = newParentId;

		if(newParentId)
			gEnv->pEntitySystem->AddEntityEventListener(m_parentId, ENTITY_EVENT_DONE, this);
	}
}
