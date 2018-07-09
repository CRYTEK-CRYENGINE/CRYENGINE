// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
-------------------------------------------------------------------------
History:
- 14:08:2012   17:12 : Created by Peter Bottomley
			Debug class to record all the shot bullets and provide info for them.

*************************************************************************/
#include "StdAfx.h"
#include "ShotDebug.h"

#ifdef SHOT_DEBUG

#include "IGameRulesSystem.h"
#include "IItem.h"
#include "GameCVars.h"
#include "Projectile.h"
#include "Utility/CryWatch.h"
#include <CryMath/Cry_Geo.h>

//------------------------------------------------------------------------

static const ColorB colDefault	(160, 160, 160, 255);		//GREY
static const ColorB colVelocity	( 53, 130, 255, 255);		//BLUE
static const ColorB colTarget		(170, 255, 114, 255);		//GREEN
static const ColorB colAimed		(255,  99, 161, 255);		//PINK
static const ColorB colInspect	(255,  52,  45, 255);		//RED

static const float hitRad =	0.02f;
static const float inspectRadSqr = sqr(hitRad*4.f);

//------------------------------------------------------------------------

CShotDebug::CShotDebug( IActor& owner, IItem& weapon )
	: m_ownerId(owner.GetEntityId())
	, m_weaponId(weapon.GetEntityId())
{
	if(g_pGameCVars->pl_shotDebug)
	{
		g_pGame->GetWeaponSystem()->AddListener(this);
		if(CGameRules* pGameRules = g_pGame->GetGameRules())
		{
			pGameRules->AddHitListener(this);
		}
	}
}

CShotDebug::~CShotDebug()
{
	g_pGame->GetWeaponSystem()->RemoveListener(this);
	if(CGameRules* pGameRules = g_pGame->GetGameRules())
	{
		pGameRules->RemoveHitListener(this);
	}
}

void CShotDebug::Update( const float dt )
{
	if(!g_pGameCVars->pl_shotDebug)
		return;

	const int projCount = m_projectiles.size();

	IEntity* pOwner = gEnv->pEntitySystem->GetEntity(m_ownerId);
	IEntity* pWeapon = gEnv->pEntitySystem->GetEntity(m_weaponId);

	CryWatch("%s:%s [%d projectiles]", pOwner?pOwner->GetName():"Owner", pWeapon?pWeapon->GetName():"Weapon", projCount);

	if(!projCount)
		return;

	const Matrix34& camMatrix = gEnv->p3DEngine->GetRenderingCamera().GetMatrix();
	const Lineseg eyeLine(camMatrix.GetTranslation(),camMatrix.GetTranslation()+(camMatrix.GetColumn1()*500.f));
	
	IRenderAuxGeom* pRender = gEnv->pRenderer->GetIRenderAuxGeom();
	CGameRules* pGameRules = g_pGame->GetGameRules();

	for(ProjectileList::iterator it = m_projectiles.begin(); it!=m_projectiles.end();)
	{
		SProjInfo& info = *it;
		const SProjInfo::Launch& launch = info.launch;
		const SProjInfo::Impact& impact = info.impact;
		const SProjInfo::Hit& hit = info.hit;

		// Remove timed out projectiles.
		if((info.flags&SProjInfo::eS_StopCountdownMask)==0)
			info.lifetime-=dt;
		if(info.lifetime<0.f)
		{
			ProjectileMap::iterator found = m_projectileMap.find(info.projectileId);
			if(found!=m_projectileMap.end())
				m_projectileMap.erase(found);
			it = m_projectiles.erase(it);
			continue;
		}

		// Check for Inspection.
		const bool isComplete = (info.flags&SProjInfo::eS_Complete)==SProjInfo::eS_Complete;
		if(isComplete)
		{
			float t,s;
			const float distSq = Distance::Lineseg_LinesegSq(eyeLine,Lineseg(launch.pos,impact.pos),&t,&s);
			if(distSq<inspectRadSqr)
			{
				info.flags |= SProjInfo::eS_Inspecting;
				info.lifetime = info.originalLifetime;
			}
			else
			{
				info.flags &= ~SProjInfo::eS_Inspecting;
			}
		}

		const bool inspecting = (info.flags&SProjInfo::eS_Inspecting)!=0;

		// Draw.
		if(pRender && isComplete)
		{
			int alpha = inspecting ? 255 : max(0,min(255,int_round(255*info.lifetime)));

			// Projectile Path.
			ColorB colPath= inspecting?colInspect:impact.target?colTarget:colDefault;
			colPath.a = alpha;
			pRender->DrawLine(launch.pos, colPath, impact.pos, colPath);

			// Velocities.
			ColorB colVel = inspecting?colInspect:colVelocity;
			colVel.a = alpha;
			pRender->DrawLine(launch.pos, colVel, launch.pos+launch.vel, colVel);
			pRender->DrawLine(impact.pos, colVel, impact.pos-impact.vel, colVel);

			// Hit point.
			ColorB colHit = inspecting?colInspect:impact.target?colTarget:colDefault;
			pRender->DrawSphere(impact.pos, hitRad, colHit);

			// Sticky hit point.
			if(!impact.relpos.IsZero())
			{
				if(IEntity* pTarget = gEnv->pEntitySystem->GetEntity(impact.target))
				{
					if(ICharacterInstance* pChar = pTarget->GetCharacter(0))
					{
						if(ISkeletonPose* pSkel = pChar->GetISkeletonPose())
						{
							const QuatT& joint = pSkel->GetAbsJointByID(impact.partid);
							QuatT root(pTarget->GetWorldTM());
							pRender->DrawSphere(((root*joint)*impact.relpos), hitRad, colHit);
						}
					}
				}
			}
		}

		// Description of inspected item.
		if(isComplete && inspecting)
		{
			const bool hasHit = ((info.flags&SProjInfo::eS_HitSet)!=0) && pGameRules;
			if(pGameRules)
			{
				IEntity* pTarget = gEnv->pEntitySystem->GetEntity(impact.target);
				CryWatch("Projectile:%s%s%s Dmg[%f]%s%s%s Pos:[%.2f %.2f %.2f]%s",
					pTarget?" Hit[":"", pTarget?pTarget->GetName():"", pTarget?"]":"",
					hasHit?hit.damage:0.f,
					hasHit?" Type[":"", hasHit?pGameRules->GetHitType(hit.hitType):"", hasHit?"]":"",
					impact.pos.x, impact.pos.y, impact.pos.z,
					impact.aimed?" Aimed":"");
			}
		}

		++it;
	}
}

//////////////////////////////////////////////////////////////////////////
//IProjectileListener
void CShotDebug::OnLaunch( CProjectile* pProjectile, const Vec3& pos, const Vec3& velocity )
{
	if(!g_pGameCVars->pl_shotDebug)
		return;

	if(pProjectile->GetWeaponId()!=m_weaponId)
		return;

	const EntityId projectileId = pProjectile->GetEntityId();
	SProjInfo info(projectileId,10.f);
	info.flags |= SProjInfo::eS_LaunchSet;
	SProjInfo::Launch& launch = info.launch;

	launch.pos = pos;
	launch.vel = (velocity.GetLengthSquared()<=1e-6f) ? Vec3(ZERO) : velocity.GetNormalized();

	m_projectiles.push_back(info);
	m_projectileMap[pProjectile->GetEntityId()] = &(m_projectiles.back());
}

void CShotDebug::OnProjectileHit( const CProjectile& projectile, const EventPhysCollision& collision )
{
	if(!g_pGameCVars->pl_shotDebug)
		return;

	if(projectile.GetWeaponId()!=m_weaponId)
		return;

	ProjectileMap::iterator found = m_projectileMap.find(projectile.GetEntityId());
	if(found==m_projectileMap.end())
		return;
	SProjInfo& info = *(found->second);
	info.flags |= SProjInfo::eS_ImpactSet;
	info.lifetime = info.originalLifetime;

	if(info.flags&SProjInfo::eS_HitSet)
		m_projectileMap.erase(found);

	SProjInfo::Impact& hit = info.impact;
	hit.pos = collision.pt;
	hit.vel = (collision.vloc[0].GetLengthSquared()<=1e-6f) ? Vec3(ZERO) : collision.vloc[0].GetNormalized();
	hit.nrm = collision.n;
	hit.relpos.zero();
	IEntity* pTarget = collision.iForeignData[1]==PHYS_FOREIGN_ID_ENTITY ? (IEntity*)collision.pForeignData[1] : NULL;
	hit.target = pTarget?pTarget->GetId():0;
	hit.partid = collision.partid[1];
	hit.matid = collision.idmat[1];
	hit.aimed = projectile.CheckAnyProjectileFlags(CProjectile::ePFlag_aimedShot);

	if(pTarget)
	{
		if(ICharacterInstance* pChar = pTarget->GetCharacter(0))
		{
			if(ISkeletonPose* pSkel = pChar->GetISkeletonPose())
			{
				QuatT joint = pSkel->GetAbsJointByID(hit.partid);
				Matrix34 invTM = pTarget->GetWorldTM()*Matrix34(joint);
				invTM.Invert();
				hit.relpos = (invTM*hit.pos);
			}
		}
	}
}

//IHitListener
void CShotDebug::OnHit( const HitInfo& hitInfo )
{
	if(!g_pGameCVars->pl_shotDebug)
		return;

	if(hitInfo.weaponId!=m_weaponId)
		return;

	CProjectile* pProjectile = g_pGame->GetWeaponSystem()->GetProjectile(hitInfo.projectileId);
	if(!pProjectile)
		return;

	ProjectileMap::iterator found = m_projectileMap.find(pProjectile->GetEntityId());
	if(found==m_projectileMap.end())
		return;
	SProjInfo& info = *(found->second);
	info.flags |= SProjInfo::eS_HitSet;
	info.lifetime = info.originalLifetime;

	if(info.flags&SProjInfo::eS_ImpactSet)
		m_projectileMap.erase(found);

	info.hit.damage = hitInfo.damage;
	info.hit.hitType = hitInfo.type;
}

#endif //SHOT_DEBUG
