// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "singleboxtree.h"
#include "cylindergeom.h"
#include "capsulegeom.h"
#include "spheregeom.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "livingentity.h"
#include "rigidentity.h"
#include "waterman.h"

#define collider_flags (m_parts[0].flagsCollider)
#define GROUND_EPS_SIZE max(0.01f*m_size.z, 0.004f)

inline Vec3_tpl<short> EncodeVec6b(const Vec3 &vec) {
	return Vec3_tpl<short>(
		float2int(min_safe(100.0f,max_safe(-100.0f,vec.x))*(32767/100.0f)),
		float2int(min_safe(100.0f,max_safe(-100.0f,vec.y))*(32767/100.0f)),
		float2int(min_safe(100.0f,max_safe(-100.0f,vec.z))*(32767/100.0f)));
}
inline Vec3 DecodeVec6b(const Vec3_tpl<short> &vec) {
	return Vec3(vec.x,vec.y,vec.z)*(100.0f/32767);
}

struct vec4b {
	short yaw;
	char pitch;
	unsigned char len;
};
inline vec4b EncodeVec4b(const Vec3 &vec) {
	vec4b res = { 0,0,0 };
	if (vec.len2()>sqr(1.0f/512)) {
		float len = vec.len();
		res.yaw = float2int(atan2_tpl(vec.y,vec.x)*(32767/g_PI));
		res.pitch = float2int(asin_tpl(vec.z/len)*(127/g_PI));
		res.len = float2int(min(20.0f,len)*(255/20.0f));
	}
	return res;
}
inline Vec3 DecodeVec4b(const vec4b &vec) {
	float yaw=vec.yaw*(g_PI/32767), pitch=vec.pitch*(g_PI/127), len=vec.len*(20.0f/255);
	float t = cos_tpl(pitch);
	return Vec3(t*cos_tpl(yaw),t*sin_tpl(yaw),sin_tpl(pitch))*len;
}

CLivingEntity::CLivingEntity(CPhysicalWorld *pWorld, IGeneralMemoryHeap* pHeap)
	: CPhysicalEntity(pWorld, pHeap)
	, m_vel(ZERO)
	, m_velRequested(ZERO)
	, m_gravity(0, 0, -9.81f)
	, m_nslope(0.0f, 0.0f, 1.0f)
	, m_dtRequested(0.0f)
	, m_kInertia(8.0f)
	, m_kInertiaAccel(0.0f)
	, m_kAirControl(0.1f)
	, m_kAirResistance(0.0f)
	, m_hCyl(1.1f)
	, m_hEye(1.7f)
	, m_hPivot(0.0f)
	, m_size(0.4f, 0.4f, 0.6f)
	, m_dh(0.0f)
	, m_dhSpeed(0.0f)
	, m_dhAcc(0.0f)
	, m_stablehTime(1.0f)
	, m_hLatest(-1e-10f)
	, m_nodSpeed(60.0f)
	, m_mass(80)
	, m_massinv(2.0f / 80)
	, m_surface_idx(0)
	, m_lastGroundSurfaceIdx(-1)
	, m_lastGroundSurfaceIdxAux(-1)
	, m_lastGroundPrim(-1)
	, m_timeFlying(0.0f)
	, m_timeForceInertia(0.0f)
	, m_slopeSlide(cos_tpl(g_PI * 0.2f))
	, m_slopeClimb(cos_tpl(g_PI * 0.3f))
	, m_slopeJump(cos_tpl(g_PI*0.3f))
	, m_slopeFall(cos_tpl(g_PI*0.39f))
	, m_maxVelGround(10.0f)
	, m_groundContactEps(GROUND_EPS_SIZE)
	, m_timeImpulseRecover(5.0f)
	, m_pCylinderGeom(nullptr)
	, m_hHead(1.9f)
	, m_pHeadGeom(nullptr)
	, m_timeUseLowCap(-1.0f)
	, m_timeSinceStanceChange(0.0f)
	, m_timeSinceImpulseContact(10.0f)
	, m_timeStepFull(0.0f)
	, m_timeStepPerformed(0.0f)
	, m_iSnapshot(0)
	, m_iTimeLastSend(-1)
	, m_collTypes(ent_terrain | ent_static | ent_sleeping_rigid | ent_rigid | ent_living)
	, m_pLivingEntToIgnore(nullptr)
	, m_bFlying(1)
	, m_bJumpRequested(0)
	, m_bSwimming(0)
	, m_bUseCapsule(0)
	, m_bIgnoreCommands(0)
	, m_bStateReading(0)
	, m_bActive(1)
	, m_bActiveEnvironment(0)
	, m_bStuck(0)
	, m_bReleaseGroundColliderWhenNotActive(1)
	, m_bHadCollisions(0)
	, m_bSquashed(0)
	, m_pLastGroundCollider(nullptr)
	, m_iLastGroundColliderPart(0)
	, m_posLastGroundColl(ZERO)
	, m_velGround(ZERO)
	, m_deltaPos(ZERO)
	, m_posLocal(ZERO)
	, m_deltaV(ZERO)
	, m_deltaQRot(IDENTITY)
	, m_timeSmooth(0.16f)
	, m_timeRotChanged(-1.0f)
	, m_pContacts(nullptr)
	, m_nContacts(0)
	, m_nContactsAlloc(0)
	, m_pBody(nullptr)
	, m_lockLiving(0)
	, m_lockStep(0)
	, m_forceFly(false)
{
	if (pWorld) 
		m_gravity = pWorld->m_vars.gravity; 
	 	
	m_iSimClass = 3;
	
	cylinder cyl;
	cyl.axis.Set(0,0,1); cyl.center.zero();
	cyl.hh = m_size.z; cyl.r = m_size.x;
	
	if (m_pWorld) {
		if (m_bUseCapsule)
			((CCapsuleGeom*)(m_pCylinderGeom = new CCapsuleGeom))->CreateCapsule((capsule*)&cyl);
		else 
			(m_pCylinderGeom = new CCylinderGeom)->CreateCylinder(&cyl);
		m_pCylinderGeom->CalcPhysicalProperties(&m_CylinderGeomPhys);
		m_CylinderGeomPhys.pMatMapping = 0;
		m_CylinderGeomPhys.nMats = 0;
		m_CylinderGeomPhys.nRefCount = -1;
		m_parts = m_pWorld->AllocEntityPart(); m_nPartsAlloc = 1;
		m_parts[0].pPhysGeom = m_parts[0].pPhysGeomProxy = &m_CylinderGeomPhys;
		m_parts[0].id = 100;
		m_parts[0].pos.Set(0,0,m_hCyl-m_hPivot);
		m_parts[0].q.SetIdentity();
		m_parts[0].scale = 1.0f;
		m_parts[0].mass = m_mass;
		m_parts[0].surface_idx = m_surface_idx;
		m_parts[0].flags = geom_collides|geom_monitor_contacts|geom_floats;
		m_parts[0].flagsCollider = geom_colltype_player;
		m_parts[0].minContactDist = m_size.z*0.1;
		m_parts[0].pNewCoords = (coord_block_BBox*)&m_parts[0].pos;
		m_parts[0].idmatBreakable = -1;
		m_parts[0].pLattice = 0;
		m_parts[0].pMatMapping = 0;
		m_parts[0].nMats = 0;
		m_parts[0].pPlaceholder = 0;
		m_nParts = 1;
	}
	
	//CPhysicalEntity
	m_flags = pef_pushable_by_players | pef_traceable | lef_push_players | lef_push_objects | pef_never_break;
	m_collisionClass.type |= collision_class_living;
}

CLivingEntity::~CLivingEntity()
{
	for(int i=0;i<m_nParts;i++) {
		if (!m_parts[i].pPhysGeom || m_parts[i].pMatMapping!=m_parts[i].pPhysGeom->pMatMapping)
			delete[] m_parts[i].pMatMapping; m_parts[i].pMatMapping=0;
	}
	m_parts[0].pPhysGeom = m_parts[0].pPhysGeomProxy = 0;
// 	if (m_history!=m_history_buf) delete[] m_history;
// 	if (m_actions!=m_actions_buf) delete[] m_actions;
	if (m_pContacts) delete[] m_pContacts;
	if (m_pCylinderGeom) delete m_pCylinderGeom;
	if (m_pBody) delete[] m_pBody;
	if (m_pHeadGeom) delete m_pHeadGeom;
}																					


void CLivingEntity::ReleaseGroundCollider()
{
	if (m_pLastGroundCollider) {
		m_pLastGroundCollider->Release();
		m_pLastGroundCollider = 0;
	}
}

void CLivingEntity::SetGroundCollider(CPhysicalEntity *pCollider, int bAcceptStatic)
{
	ReleaseGroundCollider();
	if (pCollider && pCollider->m_iSimClass+bAcceptStatic>0) {
		pCollider->AddRef();
		m_pLastGroundCollider = pCollider;
	}
}

int CLivingEntity::Awake(int bAwake,int iSource)
{ 
	if (m_pLastGroundCollider && m_pLastGroundCollider->m_iSimClass==7)
		ReleaseGroundCollider();
	if (!(m_bActiveEnvironment = bAwake)) {
		m_vel.zero(); m_velRequested.zero(); m_gravity.zero();
	}
	return 1; 
}


float CLivingEntity::UnprojectionNeeded(const Vec3 &pos,const quaternionf &qrot, float hCollider,float hPivot, 
																				const Vec3 &newdim, int bCapsule, Vec3 &dirUnproj, int iCaller) const
{
	int i,j,nents;
	float t;
	geom_world_data gwd[3];
	intersection_params ip;
	CPhysicalEntity **pentlist;
	geom_contact *pcontacts;

	cylinder newcyl;
	CCylinderGeom geomCyl;
	CCapsuleGeom geomCaps;
	CCylinderGeom *pgeom = bCapsule ? &geomCaps : &geomCyl;
	newcyl.r = newdim.x; newcyl.hh = newdim.z;
	newcyl.center.zero();
	newcyl.axis.Set(0,0,1);
	geomCyl.CreateCylinder(&newcyl);
	geomCaps.CreateCapsule((capsule*)&newcyl);
	Vec3 BBox[2];
	BBox[0].Set(-newdim.x,-newdim.x,-hPivot) += pos;
	BBox[1].Set(newdim.x,newdim.x,newdim.z+hCollider-hPivot+newdim.x*bCapsule) += pos;

	nents = m_pWorld->GetEntitiesAround(BBox[0],BBox[1], pentlist, m_collTypes|ent_no_ondemand_activation, 0,0,iCaller);
	gwd[0].R = Matrix33(qrot); 
	gwd[0].offset = pos + gwd[0].R*Vec3(0,0,hCollider-hPivot); 
	gwd[0].v = -dirUnproj;
	ip.time_interval = m_size.z*2;
	for(i=0;i<nents;i++) 
		if (pentlist[i]!=this && (!m_pForeignData || pentlist[i]->m_pForeignData!=m_pForeignData) && !pentlist[i]->IgnoreCollisionsWith(this,1)) {
			if (pentlist[i]->GetType()!=PE_LIVING) {
				ReadLock lock(pentlist[i]->m_lockUpdate);
				for(j=0;j<pentlist[i]->m_nParts;j++) if (pentlist[i]->m_parts[j].flags & collider_flags) {
					pentlist[i]->GetPartTransform(j, gwd[1].offset,gwd[1].R,gwd[1].scale, this);
					if (pgeom->Intersect(pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom, gwd,gwd+1, &ip, pcontacts)) {
						got_unproj:
						if (dirUnproj.len2()==0) dirUnproj = pcontacts[0].dir;
						t = pcontacts[0].t;	// lock should be released after reading t
						return t;
					}
					for(int ipart=1;ipart<m_nParts;ipart++) if (m_parts[ipart].flagsCollider & pentlist[i]->m_parts[j].flags) {
						gwd[2].R = Matrix33(qrot); 
						gwd[2].offset = pos + qrot*m_parts[ipart].pos; 
						gwd[2].scale = m_parts[ipart].scale;
						gwd[2].v = -dirUnproj;
						if (m_parts[ipart].pPhysGeomProxy->pGeom->Intersect(pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom, gwd+2,gwd+1, &ip, pcontacts))
							goto got_unproj;
					}
				}
			} else if (pentlist[i]->m_parts[0].flags&collider_flags && !IgnoreCollision(m_collisionClass, pentlist[i]->m_collisionClass)) {
				CLivingEntity *pent = (CLivingEntity*)pentlist[i];
				if (fabs_tpl((pos.z+hCollider-hPivot)-(pent->m_pos.z+pent->m_hCyl-pent->m_hPivot)) < newdim.z+pent->m_size.z &&
						len2(Vec2(pos)-Vec2(pent->m_pos)) < sqr(newdim.x+pent->m_size.x))
					return 1E10;
			}
	}
	return 0;
}


int CLivingEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued()) {
		if (_params->type==pe_player_dimensions::type_id && (unsigned int)m_iSimClass<7u) {
			int iter=0,iCaller=get_iCaller();
			WriteLockCond lockc(m_pWorld->m_lockCaller[iCaller], iCaller==MAX_PHYS_THREADS);
			float hCyl,hPivot;	
			Vec3 size,pos;
			quaternionf qrot;
			{ ReadLock lock(m_lockUpdate);
				hCyl=m_hCyl; size=m_size;	hPivot=m_hPivot;
				pos=m_pos; qrot=m_qrot; 
			}
			pe_player_dimensions *params = (pe_player_dimensions*)_params;
			Vec3 sizeCollider = is_unused(params->sizeCollider) ? size:params->sizeCollider;
			float heightCollider = is_unused(params->heightCollider) ? hCyl:params->heightCollider;
			float unproj=0,step; 
			if ((heightCollider!=hCyl || sizeCollider.x!=size.x || sizeCollider.z!=size.z) && m_pWorld && m_bActive) 
			{ do {
					unproj += (step = UnprojectionNeeded(pos+params->dirUnproj*unproj,qrot,heightCollider,
						hPivot,sizeCollider,m_bUseCapsule,params->dirUnproj,iCaller))*1.01f;
				} while (unproj<params->maxUnproj && ++iter<8 && step>0);
				if (unproj>params->maxUnproj || iter==8)
					return 0;
			}
		}
		return 1;
	}

	int res;
	Vec3 prevpos=m_pos;
	quaternionf prevq = m_qrot;

	if (res = CPhysicalEntity::SetParams(_params,1)) {
		if (_params->type==pe_params_pos::type_id && !(((pe_params_pos*)_params)->bRecalcBounds & 32)) {
			if ((prevpos-m_pos).len2()>sqr(m_size.z*0.01)) {
				WriteLock lock(m_lockLiving);
				ReleaseGroundCollider(); m_bFlying = 1;
				m_vel.zero(); m_dh=0;
			}
			for(int i=0;i<3;i++) if (fabs_tpl(m_qrot.v[i])<1e-10f) 
				m_qrot.v[i] = 0;
			if (fabs_tpl(m_qrot.w)<1e-10f) 
				m_qrot.w = 0;
			m_qrot.Normalize();
			if (m_pBody && !(m_pBody->Ibody_inv*Vec3(1)).len2() && m_pWorld->m_lastTimeInterval) {
				Quat dq = m_qrot*!prevq;
				m_pBody->w = dq.v.len2()<sqr(0.3f) ? dq.v*2/m_pWorld->m_lastTimeInterval : Vec3(ZERO);
				m_timeRotChanged = m_pWorld->m_timePhysics;
			}

			//if ((m_qrot.v^prevq.v).len2() > m_qrot.v.len2()*prevq.v.len2()*sqr(0.001f)) {
			if (fabs_tpl(m_qrot.v.x)+fabs_tpl(m_qrot.v.y)+fabs_tpl(prevq.v.x)+fabs_tpl(prevq.v.y)>0) {
				Vec3 dirUnproj(0,0,1);
				if (((pe_params_pos*)_params)->bRecalcBounds && m_bActive && UnprojectionNeeded(m_pos,m_qrot,m_hCyl,m_hPivot,m_size,m_bUseCapsule,dirUnproj)>0) {
					pe_params_pos pp; pp.q = prevq;
					CPhysicalEntity::SetParams(&pp,1);
					return 0;
				}
			}
			/*if (m_qrot.s!=prevq.s || m_qrot.v!=prevq.v) {
				Vec3 angles;
				m_qrot.get_Euler_angles_xyz(angles);
				m_qrot = quaternionf(angles.z,Vec3(0,0,1));
			}*/
			//m_BBox[0].z = min(m_BBox[0].z,m_pos.z-m_hPivot);
		}
		return res;
	}
	
	if (_params->type==pe_player_dimensions::type_id) {
		pe_player_dimensions *params = (pe_player_dimensions*)_params;
		ENTITY_VALIDATE("CLivingEntity:SetParams(player_dimensions)",params);
 		if (is_unused(params->sizeCollider)) params->sizeCollider = m_size;
		if (is_unused(params->heightCollider)) params->heightCollider = m_hCyl;
		if (is_unused(params->heightPivot)) params->heightPivot = m_hPivot;
		if (is_unused(params->bUseCapsule)) params->bUseCapsule = m_bUseCapsule;

		float unproj=0,step; res=0;
		if ((params->heightCollider!=m_hCyl || params->heightPivot!=m_hPivot || params->sizeCollider.x!=m_size.x || params->sizeCollider.z!=m_size.z || 
				 params->bUseCapsule!=m_bUseCapsule) && m_pWorld && m_bActive) 
		{ do {
				unproj += (step = UnprojectionNeeded(m_pos+params->dirUnproj*unproj,m_qrot,params->heightCollider,
					params->heightPivot,params->sizeCollider,params->bUseCapsule,params->dirUnproj))*1.01f;
			} while (unproj<params->maxUnproj && ++res<8 && step>0);
			if (unproj>params->maxUnproj || res==8)
				return 0;
		}

		{ WriteLock lock(m_lockUpdate);
			m_pos += params->dirUnproj*unproj;
			
			if (params->heightCollider!=m_hCyl || params->heightPivot!=m_hPivot || params->sizeCollider.x!=m_size.x || params->sizeCollider.z!=m_size.z || params->bUseCapsule!=m_bUseCapsule) {
				m_hPivot = params->heightPivot;
				cylinder newdim;
				newdim.r = params->sizeCollider.x;
				newdim.hh = params->sizeCollider.z;
				newdim.center.zero();
				newdim.axis.Set(0,0,1);
				if (m_bUseCapsule!=params->bUseCapsule) {
					if (m_pCylinderGeom) delete m_pCylinderGeom;
					m_CylinderGeomPhys.pGeom = m_pCylinderGeom = params->bUseCapsule ? (new CCapsuleGeom) : (new CCylinderGeom);
				}
				if (m_bUseCapsule = params->bUseCapsule)
					((CCapsuleGeom*)m_pCylinderGeom)->CreateCapsule((capsule*)&newdim);
				else
					m_pCylinderGeom->CreateCylinder(&newdim);
				m_hCyl = params->heightCollider;
				m_parts[0].pos.Set(0,0,m_hCyl-m_hPivot);
				m_size.Set(params->sizeCollider.x, params->sizeCollider.x, params->sizeCollider.z);
				sphere sph;
				sph.center.Set(0,0,-m_size.z);
				sph.r = m_size.x;
			}
			if (!is_unused(params->groundContactEps)) m_groundContactEps = max(params->groundContactEps, GROUND_EPS_SIZE);

			if (!is_unused(params->heightEye)) {
				/*if (m_hEye!=params->heightEye) {
					m_dh += params->heightEye-m_hEye;
					m_dhSpeed = 4*(params->heightEye-m_hEye);
				}*/
				m_hEye = params->heightEye;
				m_timeSinceStanceChange = 0;
			}

			if (!is_unused(params->heightHead)) m_hHead = params->heightHead;
			if (!is_unused(params->headRadius)) {
				sphere sph;
				sph.center.zero(); sph.r = params->headRadius;
				if (sph.r>0) {
					if (!m_pHeadGeom) m_pHeadGeom = new CSphereGeom;
					m_pHeadGeom->CreateSphere(&sph);
				}	else if (m_pHeadGeom) {
					delete m_pHeadGeom; m_pHeadGeom = 0;
				}
			}
			ComputeBBox(m_BBox);
		}
		m_pWorld->RepositionEntity(this,1|8);

		return 1;
	}

	if (_params->type==pe_player_dynamics::type_id) {
		pe_player_dynamics *params = (pe_player_dynamics*)_params;
		WriteLock lock(m_lockLiving);
		if (!is_unused(params->kInertia)) m_kInertia = params->kInertia;
		if (!is_unused(params->kInertiaAccel)) m_kInertiaAccel = params->kInertiaAccel;
		if (!is_unused(params->kAirControl)) m_kAirControl = params->kAirControl;
		if (!is_unused(params->kAirResistance)) m_kAirResistance = params->kAirResistance;
		if (!is_unused(params->gravity)) m_gravity = params->gravity;
		else if (!is_unused(params->gravity.z)) m_gravity.Set(0,0,-params->gravity.z);
		if (!is_unused(params->nodSpeed)) m_nodSpeed = params->nodSpeed;
		if (!is_unused(params->bSwimming)) m_bSwimming = params->bSwimming;
		if (!is_unused(params->mass)) { m_mass = params->mass; m_massinv = m_mass>0 ? 1.0f/m_mass:0.0f; }
		if (!is_unused(params->surface_idx))
			m_surface_idx = m_parts[0].surface_idx = params->surface_idx;
		if (!is_unused(params->minSlideAngle)) m_slopeSlide = cos_tpl(params->minSlideAngle*(g_PI/180.0f));
		if (!is_unused(params->maxClimbAngle)) m_slopeClimb = cos_tpl(params->maxClimbAngle*(g_PI/180.0f));
		if (!is_unused(params->maxJumpAngle)) m_slopeJump = cos_tpl(params->maxJumpAngle*(g_PI/180.0f));
		if (!is_unused(params->minFallAngle))	m_slopeFall = cos_tpl(params->minFallAngle*(g_PI/180.0f));
		if (!is_unused(params->maxVelGround))	m_maxVelGround = params->maxVelGround;
		if (!is_unused(params->timeImpulseRecover)) m_timeImpulseRecover = params->timeImpulseRecover;
		if (!is_unused(params->collTypes)) m_collTypes = params->collTypes;
		if (!is_unused(params->pLivingEntToIgnore)) m_pLivingEntToIgnore=(CPhysicalEntity*)params->pLivingEntToIgnore;
		if (!is_unused(params->bReleaseGroundColliderWhenNotActive)) m_bReleaseGroundColliderWhenNotActive = params->bReleaseGroundColliderWhenNotActive;
		if (!is_unused(params->bActive)) {
			if (m_bActive+params->bActive*2==2)
				m_bFlying = 1;
			m_bActive = params->bActive;
		}
		return 1;
	}

	return 0;
}


int CLivingEntity::GetParams(pe_params *_params) const
{
	int res;
	if (res = CPhysicalEntity::GetParams(_params))
		return res;
	
	if (_params->type==pe_player_dimensions::type_id) {
		pe_player_dimensions *params = (pe_player_dimensions*)_params;
		ReadLock lock(m_lockLiving);
		params->heightPivot = m_hPivot;
		params->sizeCollider = m_size;
		params->heightCollider = m_hCyl;
		params->heightEye = m_hEye; 
		params->headRadius = m_pHeadGeom ? m_pHeadGeom->m_sphere.r : 0.0f;
		params->heightHead = m_hHead;
		params->bUseCapsule = m_bUseCapsule;
		params->groundContactEps = m_groundContactEps;
		return 1;
	}

	if (_params->type==pe_player_dynamics::type_id) {
		pe_player_dynamics *params = (pe_player_dynamics*)_params;
		ReadLock lock(m_lockLiving);
		params->kInertia = m_kInertia;
		params->kInertiaAccel = m_kInertiaAccel;
		params->kAirControl = m_kAirControl;
		params->kAirResistance = m_kAirResistance;
		params->gravity = m_gravity;
		params->nodSpeed = m_nodSpeed;
		params->bSwimming = m_bSwimming;
		params->mass = m_mass;
		params->surface_idx = m_surface_idx;
		params->minSlideAngle = acos_tpl(m_slopeSlide)*(180.0f/g_PI);
		params->maxClimbAngle = acos_tpl(m_slopeClimb)*(180.0f/g_PI);
		params->maxJumpAngle = acos_tpl(m_slopeJump)*(180.0f/g_PI);
		params->minFallAngle = acos_tpl(m_slopeFall)*(180.0f/g_PI);
		params->maxVelGround = m_maxVelGround;
		params->timeImpulseRecover = m_timeImpulseRecover;
		params->collTypes = m_collTypes;
		params->pLivingEntToIgnore = m_pLivingEntToIgnore;
		params->bActive = m_bActive;
		params->bReleaseGroundColliderWhenNotActive = m_bReleaseGroundColliderWhenNotActive;
		return 1;
	}

	return 0;
}


int CLivingEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id, int bThreadSafe)
{
	m_parts[0].flags &= ~(geom_colltype_ray|geom_colltype_foliage_proxy);
	id = CPhysicalEntity::AddGeometry(pgeom,params,id);
	m_parts[m_nParts-1].flags |= geom_monitor_contacts;
	return id;
}


void CLivingEntity::RemoveGeometry(int id, int bThreadSafe)
{
	CPhysicalEntity::RemoveGeometry(id);
	if (m_nParts==1)
		m_parts[0].flags |= geom_colltype_ray;
}


int CLivingEntity::GetStatus(pe_status *_status) const
{
	if (_status->type==pe_status_pos::type_id) {
		pe_status_pos *status = (pe_status_pos*)_status;
		int res,flags = status->flags; 
		status->flags |= status_thread_safe;
		{ WriteLock lock(m_lockUpdate);
			Vec3 prevPos = m_pos;
			/*if (m_pWorld->m_vars.bMultithreaded)
				if (m_pWorld->m_iLastLogPump>m_timeLogged)
					m_posLogged = m_pos;
				else
					m_pos = m_posLogged;*/
			if (m_bActive)
				((CLivingEntity*)this)->m_pos += m_deltaPos*(m_timeSmooth*(1/0.3f))-m_qrot*Vec3(0,0,m_dh);
			res = CPhysicalEntity::GetStatus(_status);
			((CLivingEntity*)this)->m_pos = prevPos;
		}
		status->flags = flags;
		return res;
	}

	if (CPhysicalEntity::GetStatus(_status))
		return 1;

	if (_status->type==pe_status_living::type_id) {
		pe_status_living *status = (pe_status_living*)_status;
		ReadLock lock(m_lockLiving);
		status->bFlying = m_bFlying & m_bActive;
		status->timeFlying = m_timeFlying;
		status->camOffset = m_qrot*Vec3(0,0,/*-m_hPivot+m_hEye*/-m_dh);
		status->vel = m_vel;
		if (m_pLastGroundCollider)
			status->vel += m_velGround;

		status->velUnconstrained = m_vel;
		status->velRequested = m_velRequested;
		status->velGround = m_velGround;
		status->groundHeight = m_hLatest;
		status->groundSlope = m_nslope;
		status->groundSurfaceIdx = m_lastGroundSurfaceIdx;
		status->groundSurfaceIdxAux = m_lastGroundSurfaceIdxAux;
		status->pGroundCollider = m_pLastGroundCollider;
		status->iGroundColliderPart = m_iLastGroundColliderPart;
		status->timeSinceStanceChange = m_timeSinceStanceChange;
		status->bStuck = m_bStuck;
		status->pLockStep = &m_lockStep;
		status->bSquashed = m_bSquashed;
		return 1;
	} else

	if (_status->type==pe_status_dynamics::type_id) {
		pe_status_dynamics *status = (pe_status_dynamics*)_status;
		ReadLock lock(m_lockLiving);
		status->v = m_vel;
		if (m_pLastGroundCollider)
			status->v += m_velGround;
		float rdt = 1.0f/m_timeSmooth;
		quaternionf dq = m_deltaQRot;
		status->a = m_deltaV*rdt;
		if (dq.v.len2()<sqr(0.05f))
			status->w = dq.v*(2*rdt);
		else
			status->w = dq.v.normalized()*(acos_tpl(dq.w)*2*rdt);
		status->centerOfMass = (m_BBox[0]+m_BBox[1])*0.5f;
		status->mass = m_mass;
		/*for(idx=idx-1&m_szHistory-1; dt<status->time_interval; idx=idx-1&m_szHistory-1) {
			rdt = 1.0f/m_history[idx].dt; dt += m_history[idx].dt;
			status->a += (m_history[idx].v-m_history[idx-1&m_szHistory-1].v)*rdt; 
			status->w += (m_history[idx].q.v-m_history[idx-1&m_szHistory-1].q.v)*2*rdt;
			status->v += m_history[idx].v;
		}
		if (dt>0) {
			rdt = 1.0f/dt;
			status->v *= dt;
			status->a *= dt;
			status->w *= dt;
		}*/
		return 1;
	}

	if (_status->type==pe_status_timeslices::type_id) {
		pe_status_timeslices *status = (pe_status_timeslices*)_status;
		ReadLock lock(m_lockLiving);
		if (!status->pTimeSlices) return 0;
		int nSlices=0;
		return nSlices;
	}

	if (_status->type==pe_status_collisions::type_id) {
//		pe_status_collisions *status = (pe_status_collisions*)_status;
		return m_bHadCollisions--;
	}

	if (_status->type==pe_status_check_stance::type_id) {
		pe_status_check_stance *status = (pe_status_check_stance*)_status;
		Vec3 pos=m_pos, size=m_size;
		quaternionf qrot=m_qrot;
		float hCollider=m_hCyl;
		int bCapsule=m_bUseCapsule;
		if (!is_unused(status->pos)) pos = status->pos;
		if (!is_unused(status->q)) qrot = status->q;
		if (!is_unused(status->sizeCollider)) size = status->sizeCollider;
		if (!is_unused(status->heightCollider)) hCollider = status->heightCollider;
		if (!is_unused(status->bUseCapsule)) bCapsule = status->bUseCapsule;

		return ((status->unproj = UnprojectionNeeded(pos,qrot,hCollider,m_hPivot,size,bCapsule,status->dirUnproj))==0);
	}

	return 0;
}


int CLivingEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_action->type==pe_action_move::type_id) {
		if (!m_bIgnoreCommands) {
			pe_action_move *action = (pe_action_move*)_action;
			bool bForceHistory = false;
			ENTITY_VALIDATE("CLivingEntity:Action(action_move)",action);

			WriteLockCond lock(m_lockLiving,bThreadSafe^1);
			if (!is_unused(action->dir)) {
				//m_velRequested.zero();
				
				/*if (m_kAirControl==1) {
					m_velRequested = m_vel = action->dir;
					m_bJumpRequested = action->iJump;
				} else*/ if (!action->iJump || action->iJump==3) {
 					m_velRequested = action->dir;
					if (action->iJump==3)
						m_forceFly = true;
					//if (m_bFlying && m_vel.len2()>0 && !m_bSwimming && !m_pWorld->m_vars.bFlyMode) 
					//	m_velRequested -= m_vel*((action->dir*m_vel)/m_vel.len2());
				} else {
					if (action->iJump==2) {
						m_vel += action->dir; bForceHistory = true; m_forceFly = false;
						if (action->dir.z>1.0f) {
							m_timeFlying=0;
							m_bJumpRequested = 1;
						}
					}	else {
						m_vel = action->dir; bForceHistory = true; m_forceFly = false;
						m_timeFlying=0;
						m_bJumpRequested = 1;
					}
				}
				if (m_flags & lef_snap_velocities) {
					m_velRequested = DecodeVec4b(EncodeVec4b(m_velRequested));
					m_vel = DecodeVec6b(EncodeVec6b(m_vel));
				}
				m_dtRequested = action->dt;
			} 
			return 1;
		}
		return 0;
	}	
	
	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		ENTITY_VALIDATE("CLivingEntity:Action(action_impulse)",action);

		WriteLock lock(m_lockLiving);
		Vec3 impulse = action->impulse;
		if (action->iSource==2) // explosion
			impulse *= 0.3f;
		if (action->iApplyTime==0) {
			//if (!m_bFlying) m_velRequested.zero();
			m_vel += impulse*m_massinv;
			//m_velRequested = m_vel;
			if (m_flags & lef_snap_velocities)
				m_vel = DecodeVec6b(EncodeVec6b(m_vel));
			if (impulse.z*m_massinv>1.0f) {
				m_bFlying=1; m_timeFlying=0;
			}
		} else {
			pe_action_move am;
			am.dir = impulse*m_massinv;
			am.iJump = 2;
			Action(&am,1);
		}
		/*if (!m_bFlying && action->impulse.z<0 && m_dh<m_size.z*0.1f) {
			m_dhSpeed = action->impulse.z*m_massinv;
			m_dhAcc = max(sqr(m_dhSpeed)/(m_size.z*2), m_nodSpeed*0.15f);
		}*/
		if (m_kInertia==0)
			m_timeForceInertia = m_timeImpulseRecover;
		return 1;
	}
	
	if (_action->type==pe_action_reset::type_id) {
		WriteLock lock(m_lockLiving);
		m_vel.zero(); m_velRequested.zero();
		m_dh = m_dhSpeed = 0; m_stablehTime = 1;
		m_bFlying = 1;
		return 1;
	}

	if (_action->type==pe_action_set_velocity::type_id) {
		pe_action_set_velocity *action = (pe_action_set_velocity*)_action;
		WriteLock lock(m_lockLiving);
	
		if (!is_unused(action->v)) {
			m_velRequested = m_vel = action->v;
			m_bJumpRequested = 1;
		}

		return 1;
	}

	return CPhysicalEntity::Action(_action,1);
}


int CLivingEntity::GetStateSnapshot(CStream &stm,float time_back,int flags)
{
	WriteLock lock0(m_lockUpdate),lock1(m_lockLiving);
	Vec3 pos_prev=m_pos, vel_prev=m_vel, nslope_prev=m_nslope;
	int bFlying_prev=m_bFlying;
	float timeFlying_prev=m_timeFlying, timeUseLowCap_prev=m_timeUseLowCap;
	quaternionf qrot_prev=m_qrot;
	CPhysicalEntity *pLastGroundCollider=m_pLastGroundCollider;
	int iLastGroundColliderPart=m_iLastGroundColliderPart;
	Vec3 posLastGroundColl=m_posLastGroundColl;
	ReleaseGroundCollider();
	stm.WriteNumberInBits(SNAPSHOT_VERSION,4);
	if (m_pWorld->m_vars.bMultiplayer) {
		WriteCompressedPos(stm,m_pos,(m_id+m_iSnapshot&31)!=0);
		if (m_pWorld->m_iTimePhysics!=m_iTimeLastSend) {
			m_iSnapshot++; m_iTimeLastSend = m_pWorld->m_iTimePhysics;
		}
	} else
		stm.Write(m_pos);

	if (m_flags & lef_snap_velocities) {
		if (m_vel.len2()>0) {
			stm.Write(true);
			Vec3_tpl<short> v = EncodeVec6b(m_vel);
			stm.Write(v.x); stm.Write(v.y); stm.Write(v.z);
		} else stm.Write(false);
		if (m_velRequested.len2()>0) {
			stm.Write(true);
			vec4b vr = EncodeVec4b(m_velRequested);
			stm.Write(vr.yaw); stm.Write(vr.pitch);	stm.Write(vr.len);
		} else stm.Write(false);
	} else {
		if (m_vel.len2()>0) {
			stm.Write(true);
			stm.Write(m_vel);
		} else stm.Write(false);
		if (m_velRequested.len2()>0) {
			stm.Write(true);
			stm.Write(m_velRequested);
		} else stm.Write(false);
	}
	if (m_timeFlying>0) {
		stm.Write(true);
		stm.Write((unsigned short)float2int(m_timeFlying*6553.6f));
	} else stm.Write(false);
	unsigned int imft = 0;
	stm.WriteNumberInBits(imft,2);
	stm.Write((bool)(m_bFlying!=0));

	/*if (m_pLastGroundCollider) {
		stm.Write(true);
		WritePacked(stm, m_pWorld->GetPhysicalEntityId(m_pLastGroundCollider)+1);
		WritePacked(stm, m_iLastGroundColliderPart);
		stm.Write((Vec3&)m_posLastGroundColl);
	} else
		stm.Write(false);*/

	m_pos=pos_prev; m_vel=vel_prev; m_bFlying=bFlying_prev; m_qrot=qrot_prev; 
	m_timeFlying=timeFlying_prev; m_timeUseLowCap = timeUseLowCap_prev;
	m_nslope=nslope_prev;
	SetGroundCollider(pLastGroundCollider);
	m_iLastGroundColliderPart=iLastGroundColliderPart;
	m_posLastGroundColl=posLastGroundColl;
	
	return 1;
}

void SLivingEntityNetSerialize::Serialize( TSerialize ser )
{
	ser.Value( "pos", pos, 'wrld');
	ser.Value( "vel", vel, 'pLVl');
	ser.Value( "velRequested", velRequested, 'pLVl');
}

int CLivingEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
 	WriteLock lock0(m_lockUpdate),lock1(m_lockLiving);

	SLivingEntityNetSerialize helper = {
		m_pos,
		m_vel,
		m_velRequested,
	};
	helper.Serialize( ser );

	return 1;
}


int CLivingEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	bool bnz,bCompressed;
	unsigned short tmp;
	int ver=0;
	stm.ReadNumberInBits(ver,4);
	if (ver!=SNAPSHOT_VERSION)
		return 0;

	WriteLock lock0(m_lockUpdate),lock1(m_lockLiving);
	if (flags & ssf_no_update) {
		Vec3 dummy;
		if (m_pWorld->m_vars.bMultiplayer)
			ReadCompressedPos(stm,dummy,bCompressed);
		else
			stm.Seek(stm.GetReadPos()+sizeof(Vec3)*8);
		stm.Read(bnz); if (bnz) stm.Seek(stm.GetReadPos()+((m_flags & lef_snap_velocities) ? 6:sizeof(Vec3))*8);
		stm.Read(bnz); if (bnz) stm.Seek(stm.GetReadPos()+((m_flags & lef_snap_velocities) ? 4:sizeof(Vec3))*8);
		stm.Read(bnz); if (bnz) stm.Seek(stm.GetReadPos()+sizeof(tmp)*8);
		stm.Seek(stm.GetReadPos()+3);
		return 1;
	}

	m_posLocal = m_pos + m_deltaPos*(m_timeSmooth*(1/0.3f));

	Vec3 pos0=m_pos/*,vel0=m_vel,velRequested0=m_velRequested*/;
//	int bFlying0=m_bFlying;
	
	if (m_pWorld->m_vars.bMultiplayer) {
		ReadCompressedPos(stm,m_pos,bCompressed);
		// if we received compressed pos, and our pos0 compressed is equal to it, assume the server had pos equal to our uncompressed pos0
		if (bCompressed && (CompressPos(pos0)-m_pos).len2()<sqr(0.0001f))
			m_pos = pos0;
	} else
		stm.Read(m_pos);

	if (m_flags & lef_snap_velocities) {
		stm.Read(bnz); if (bnz) {
			Vec3_tpl<short> v; stm.Read(v.x); stm.Read(v.y); stm.Read(v.z);
			m_vel = DecodeVec6b(v);
		} else m_vel.zero();
		stm.Read(bnz); if (bnz) {
			vec4b vr; stm.Read(vr.yaw); stm.Read(vr.pitch);	stm.Read(vr.len);
			m_velRequested = DecodeVec4b(vr);
		} else m_velRequested.zero();
	} else {
		stm.Read(bnz); if (bnz)
			stm.Read(m_vel);
		else m_vel.zero();
		stm.Read(bnz); if (bnz)
			stm.Read(m_velRequested);
		else m_velRequested.zero();
	}
	stm.Read(bnz); if (bnz) {
		stm.Read(tmp); m_timeFlying = tmp*(10.0f/65536);
	} else m_timeFlying = 0;
	unsigned int imft; stm.ReadNumberInBits(imft,2);
	static float flytable[] = {0.0f, 0.2f, 0.5f, 1.0f};
	stm.Read(bnz);
	m_bFlying = bnz ? 1:0;
	ReleaseGroundCollider();
	m_bJumpRequested = 0;

	/*stm.Read(bnz);
	if (bnz) {
		SetGroundCollider((CPhysicalEntity*)m_pWorld->GetPhysicalEntityById(ReadPackedInt(stm)-1));
		m_iLastGroundColliderPart = ReadPackedInt(stm);
		stm.Read((Vec3&)m_posLastGroundColl);
	}*/

#ifdef _DEBUG
	float diff = (m_pos-pos0).len2();
	if (diff>sqr(0.0001f) && flags&ssf_compensate_time_diff && m_bActive)
		m_pWorld->m_pLog->Log("Local client desync: %.4f",sqrt_tpl(diff));
#endif
	if (m_pos.len2()>1E18) {
		m_pos = pos0;
		return 1;
	}

	return 1;
}


int CLivingEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	SLivingEntityNetSerialize helper;
	helper.Serialize( ser );

	if ((ser.GetSerializationTarget()==eST_Network) && (helper.pos.len2()<50.0f))
		return 1;

	if (ser.ShouldCommitValues() && m_pWorld)
	{
		pe_params_pos setpos;	setpos.bRecalcBounds |= 16|32;
		pe_action_set_velocity velocity;

//		Vec3 debugOnlyOriginalHelperPos = helper.pos;

		if (flags & ssf_compensate_time_diff)
		{
			float dtBack=(m_pWorld->m_iTimeSnapshot[1]-m_pWorld->m_iTimeSnapshot[0])*m_pWorld->m_vars.timeGranularity;
			helper.pos += helper.vel * dtBack;
		}

		const float MAX_DIFFERENCE = std::max( helper.vel.GetLength() * 0.1f, 0.1f );

/*
		{
			IPersistantDebug * pPD = gEnv->pGameFramework->GetIPersistantDebug();
			Vec3 pts[3] = {debugOnlyOriginalHelperPos, helper.pos, m_pos};
			pPD->Begin("Snap", false);
			Vec3 bump(0,0,0.2f);
			for (int i=0; i<3; i++)
			{
				Vec3 a = pts[i] + bump;
				Vec3 b = pts[(i+1)%3] + bump;
				pPD->AddLine( a, b, ColorF(1,0,0,1), 0.3f );
			}
			pPD->Begin("Snap2", true);
			pPD->AddPlanarDisc( m_pos + bump*0.5f, 0, MAX_DIFFERENCE, ColorF(0,0,1,0.5), 0.3f );
		}
*/

		float distance = m_pos.GetDistance(helper.pos);
		setpos.pos = helper.pos;

		SetParams( &setpos,0 );

		WriteLock lock1(m_lockLiving);
		m_vel = helper.vel;
		m_velRequested = helper.velRequested;
	}

	return 1;
}


float CLivingEntity::ShootRayDown(le_precomp_entity* pents, int nents, le_precomp_part *pparts, const Vec3 &pos,
	Vec3 &nslope, float time_interval, bool bUseRotation,bool bUpdateGroundCollider,bool bIgnoreSmallObjects)
{
	int i,j,jbest,ncont,idbest,idbestAux=-1,iPrim=-1,bHasMatSubst=0;
	int j1,iCaller=get_iCaller_int();
	Matrix33 R;
	Vec3 pt,axis=m_qrot*Vec3(0,0,1);
	float h=-1E10f,maxdim,maxarea,haux=-1E10f;
	box bbox;
	if (bUseRotation)
		R = Matrix33(m_qrot);
	else {
		maxdim = sqr(m_qrot.v.z)+sqr(m_qrot.w);
		if (maxdim>0.0001f) {
			maxdim = 1/sqrt_tpl(maxdim);
			R = Matrix33(m_qrot*Quat(m_qrot.w*maxdim,0,0,m_qrot.v.z*maxdim));
		} else
			R = Matrix33(Quat(0,1,0,0));
	}
	const Vec3 dirp = (R*Vec3(0,0,-1));
	CRayGeom aray; aray.CreateRay(pos+R*Vec3(0,0,m_hCyl-m_size.z-m_hPivot),R*Vec3(0,0,-1.5f*(m_hCyl-m_size.z)),&dirp);
	geom_world_data gwd;
	geom_contact *pcontacts;
	intersection_params ip;
	CPhysicalEntity *pPrevCollider=m_pLastGroundCollider, *pentbest=NULL;
	nslope = m_nslope;

	for(i=nents-1;i>=0;--i) 
		if (pents[i].pent!=this && !pents[i].ignoreCollisionsWith) { 
		PrefetchLine(&pents[i-1],0);
		CPhysicalEntity *const pent = pents[i].pent;
		for(j=0; j<pent->m_nColliders; j++)
			if (pent->m_pColliders[j]==this && pent->HasConstraintContactsWith(pent->m_pColliders[j]))
				break;
		if (j<pent->m_nColliders)
			continue;
		for(j1=pents[i].iPartsBegin;j1<pents[i].iPartsEnd;bHasMatSubst|=pparts[j1++].partflags & geom_mat_substitutor) 
			if ((pparts[j1].partflags & collider_flags) && !(pparts[j1].partflags & geom_no_coll_response)) {
			PrefetchLine(&pparts[j1+1],0);
			if (bIgnoreSmallObjects && (pentbest || pent->GetRigidBody(j)->v.len2()>1)) {
				pparts[j1].pgeom->GetBBox(&bbox);
				maxarea = max(max(bbox.size.x*bbox.size.y, bbox.size.x*bbox.size.z), bbox.size.y*bbox.size.z)*sqr(pparts[j1].partscale)*4;
				maxdim = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*pparts[j1].partscale*2;
				if (maxarea<sqr(m_size.x)*g_PI*0.25f && maxdim<m_size.z*1.4f)
					continue;
			}
			gwd.R = Matrix33(pparts[j1].partrot);
			gwd.offset = pparts[j1].partoff;
			gwd.scale = pparts[j1].partscale;
			if (ncont=pparts[j1].pgeom->Intersect(&aray, &gwd,0, &ip, pcontacts)) { 
				if (pcontacts[ncont-1].n*aray.m_ray.dir<0) {
					if (pparts[j1].partflags & geom_colltype0 && pcontacts[ncont-1].pt*axis>haux) {
						haux = (pt=pcontacts[ncont-1].pt)*axis;
						idbestAux = pent->GetMatId(pcontacts[ncont-1].id[0], pparts[j1].ipart);
					}
					if (pcontacts[ncont-1].pt*axis>h) {
						h = (pt=pcontacts[ncont-1].pt)*axis; pentbest=pent; jbest=pparts[j1].ipart; nslope = pcontacts[ncont-1].n;
						if (pents[i].iSimClass==3) {
							nslope = pcontacts[ncont-1].pt-pent->m_pos;
							nslope -= axis*(axis*nslope);
							if (nslope.len2()>1E-4f)
								nslope.normalize();
							else nslope = m_qrot*Vec3(1,0,0);
							nslope = nslope*0.965925826f + axis*0.2588190451f; // make it act like a 75 degrees slope
						}
						idbest = pent->GetMatId(pcontacts[ncont-1].id[0], pparts[j1].ipart);
						iPrim = pcontacts[ncont-1].iPrim[0];
					}
				}
			}
		}
	}

	if (bUpdateGroundCollider) {
		m_lastGroundSurfaceIdx=m_lastGroundSurfaceIdxAux = -1;
		if (pentbest) {
			SetGroundCollider(pentbest, (pentbest->m_parts[jbest].flags & geom_manually_breakable)!=0);
			m_iLastGroundColliderPart = jbest;
			Vec3 coll_origin;	quaternionf coll_q;	float coll_scale;
			pentbest->GetPartTransform(jbest, coll_origin,coll_q,coll_scale, this);
			m_posLastGroundColl = ((pt-coll_origin)*coll_q)/coll_scale;
			int mask = (2<<pentbest->m_iSimClass)-(pentbest->m_id>>31);
			if (bHasMatSubst) for(i=nents-1;i>=0;--i) for(j1=pents[i].iPartsBegin;j1<pents[i].iPartsEnd;++j1)
				if (pparts[j1].partflags & geom_mat_substitutor && pents[i].pent->m_parts[pparts[j1].ipart].flagsCollider & mask &&
						pparts[j1].pgeom->PointInsideStatus(((pt-pparts[j1].partoff)*pparts[j1].partrot)*(fabs_tpl(pparts[j1].partscale-1)>1e-4f ? 1.0f/pparts[j1].partscale:1.0f)))
					idbest=idbestAux = pents[i].pent->GetMatId(pparts[j1].surface_idx, pparts[j1].ipart);
			m_lastGroundSurfaceIdx = idbest;
			m_lastGroundSurfaceIdxAux = idbestAux;
			m_lastGroundPrim = iPrim;

			if (pPrevCollider!=pentbest)
				AddLegsImpulse(m_gravity*time_interval+m_vel,nslope,true);
			else
				AddLegsImpulse(m_gravity*time_interval,nslope,false);

			pe_status_dynamics sd;
			sd.ipart = jbest;
			pentbest->GetStatus(&sd);
			m_velGround = sd.v+(sd.w^pt-sd.centerOfMass);
		}
		else if (m_pLastGroundCollider)
			ReleaseGroundCollider();
	}
	return h;
}


void CLivingEntity::AddLegsImpulse(const Vec3 &vel, const Vec3 &nslope, bool bInstantChange)
{
	int ncoll;
	CPhysicalEntity **pColliders/*,*pPrevCollider=m_pLastGroundCollider*/;
	pe_status_sample_contact_area ssca;
	RigidBody body(false),*pbody;
	pe_action_impulse ai;
	ssca.ptTest = m_pos-m_qrot*Vec3(0,0,m_hPivot); ssca.dirTest = m_gravity;

	if (m_pLastGroundCollider && m_flags&lef_push_objects &&
			(unsigned int)m_pLastGroundCollider->m_iSimClass-1u<2u && m_pLastGroundCollider->m_flags & pef_pushable_by_players && 
			((pbody=m_pLastGroundCollider->GetRigidBodyTrans(&body,m_iLastGroundColliderPart,this))->Minv<m_massinv*10 ||
			 (ncoll=m_pLastGroundCollider->GetColliders(pColliders))==0 || (ncoll==1 && pColliders[0]==m_pLastGroundCollider)) &&
			!m_pLastGroundCollider->GetStatus(&ssca))
	{
		Vec3 vrel = vel;
		ai.point = ssca.ptTest;
		if (bInstantChange)
			vrel -= pbody->v+(pbody->w^ai.point-pbody->pos);
		Matrix33 K; K.SetZero();
		RigidBody body(false), *pbody=m_pLastGroundCollider->GetRigidBodyTrans(&body, m_iLastGroundColliderPart, this, 0,true);
		pbody->GetContactMatrix(ai.point-pbody->pos, K);
		K(0,0)+=m_massinv; K(1,1)+=m_massinv; K(2,2)+=m_massinv;
		ai.impulse = nslope*(min(0.0f,nslope*vrel)/(nslope*K*nslope));
		m_pLastGroundCollider->Action(&ai,1);
	}
}


void CLivingEntity::RegisterContact(const Vec3 &posSelf, const Vec3& pt,const Vec3& n, CPhysicalEntity *pCollider, int ipart,int idmat, 
																		float imp, int bLegsContact, int iPrim, int ipartMin)
{
	if (m_flags & (pef_monitor_collisions | pef_log_collisions) && !m_bStateReading) {
		EventPhysCollision epc;
		epc.pEntity[0] = this; 
		epc.pForeignData[0] = m_pForeignData; 
		epc.iForeignData[0] = m_iForeignData;
		epc.pEntity[1] = pCollider; 
		epc.pForeignData[1] = pCollider->m_pForeignData; 
		epc.iForeignData[1] = pCollider->m_iForeignData;
		RigidBody body(false), *pbody = pCollider->GetRigidBodyTrans(&body,ipart,this);
		Vec3 v = pbody->v+(pbody->w^pt-pbody->pos);
		epc.n = n;
		if (bLegsContact) {
			if (!(pCollider->m_parts[ipart].flags & geom_manually_breakable)) {
				if (!((v-m_vel-m_velGround).len2()>3 && pbody->V<m_size.GetVolume()*8))
					return;
				epc.n = v.normalized();
				imp = (epc.n*(v-m_vel-m_velGround))*(m_mass*(m_mass*pbody->Minv+1.0f));
			}
		}
		epc.pt = pt;
		epc.vloc[0] = m_vel+m_velGround;
		epc.vloc[1] = v;
		epc.mass[0] = m_mass;
		epc.mass[1] = pbody->M;
		epc.idCollider = m_pWorld->GetPhysicalEntityId(pCollider);
		epc.partid[0] = m_parts[ipartMin].id;
		epc.partid[1] = pCollider->m_parts[ipart].id;
		epc.idmat[0] = m_surface_idx;
		epc.idmat[1] = pCollider->GetMatId(idmat,ipart);
		epc.penetration = epc.radius = 0;
		epc.normImpulse = max(0.0f,imp);
		if (pCollider->m_parts[ipart].flags & geom_manually_breakable) {
			//if (!(pCollider->m_parts[ipart].flags & geom_can_modify))
			//	return;
			epc.pt = posSelf+m_qrot*m_parts[0].pos;
			epc.pt += n*(n*(pt-epc.pt));
			float cosa=n*(m_qrot*Vec3(0,0,1)), sina=sqrt_tpl(max(0.0f,1.0f-cosa*cosa));
			epc.radius = (m_size.x*cosa+(m_size.z+m_size.x*m_bUseCapsule)*sina)*1.05f;
		}
		epc.iPrim[1] = iPrim;
		m_pWorld->OnEvent(m_flags,&epc);
		m_bHadCollisions = 1;
	}
}

void CLivingEntity::RegisterUnprojContact(const le_contact &unproj)
{
	if (m_nContacts==m_nContactsAlloc) {
		le_contact *pContacts = m_pContacts;
		m_pContacts = new le_contact[m_nContactsAlloc+=4];
		NO_BUFFER_OVERRUN	// cppcheck-suppress memsetClass
		memcpy(m_pContacts, pContacts, m_nContacts*sizeof(le_contact));
		delete[] pContacts;
	}
	NO_BUFFER_OVERRUN
	memcpy(m_pContacts+m_nContacts, &unproj, sizeof(unproj));
	m_pContacts[m_nContacts++].pSolverContact[0] = 0;
	AddCollider(unproj.pent);
	unproj.pent->AddCollider(this);
}


float CLivingEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull-0.001f)
		return time_interval;
	return min_safe(m_timeStepFull-m_timeStepPerformed,time_interval);
}

Vec3 CLivingEntity::SyncWithGroundCollider(float time_interval)
{
	int i; Vec3 newpos=m_pos;
	if (m_pLastGroundCollider && m_pLastGroundCollider->m_iSimClass==7)
		ReleaseGroundCollider();
	WriteLockCond lock(m_lockLiving,m_bStateReading^1);
	Vec3 velGround0 = m_velGround;
	m_velGround.zero();

	if (m_pLastGroundCollider) {
		Vec3 coll_origin;	quaternionf coll_q;	float coll_scale;
		m_pLastGroundCollider->GetPartTransform(i=m_iLastGroundColliderPart, coll_origin,coll_q,coll_scale, this);
		newpos = coll_q*m_posLastGroundColl*coll_scale + coll_origin + m_qrot*Vec3(0,0,m_hPivot);
 		const Vec3 diff = (newpos-m_pos);
		const float fMaxDist = m_maxVelGround * time_interval;
 		if ( diff.len2() > sqr(fMaxDist) )
 				newpos = m_pos+(diff).normalized()*(fMaxDist);

		pe_status_dynamics sd;
		sd.ipart = i;
		m_pLastGroundCollider->GetStatus(&sd);

		Matrix33 K; K.SetZero();
		RigidBody body(false), *pbody=m_pLastGroundCollider->GetRigidBodyTrans(&body, i, this, 0,true);
		pbody->GetContactMatrix(newpos-pbody->pos,K);
		Vec3 velGround = sd.v+(sd.w^newpos-sd.centerOfMass);
		if ((velGround-velGround0).len2() < m_gravity.len2()*sqr(time_interval*2.0f))
			m_velGround = velGround;
		else {
			Vec3 dP = (K+Matrix33(Diag33(Vec3(m_massinv)))).GetInverted()*(velGround-velGround0);
			m_velGround = velGround0+dP*m_massinv;
			if (m_velGround.len2() > velGround.len2())
				m_velGround = velGround;
			else {
				pe_action_impulse ai;
				ai.point = newpos;
				ai.impulse = -dP;
				ai.iApplyTime = 0;
				ai.ipart = i;
				m_pLastGroundCollider->Action(&ai);
			}
		}

		if (m_velGround.len2()>sqr(m_maxVelGround))
			m_velGround.normalize() *= m_maxVelGround;
	}
	if (m_pWorld->m_bWorldStep==2)
		m_pos = newpos;
	return newpos;
}


void CLivingEntity::ComputeBBox(Vec3 *BBox, int flags) 
{ 
	CPhysicalEntity::ComputeBBox(BBox,flags); 
	Vec3 pt = m_pNewCoords->pos-m_pNewCoords->q*Vec3(0,0,m_hPivot);
	BBox[0] = min(BBox[0],pt); BBox[1] = max(BBox[1],pt);
	if (sqr(m_pNewCoords->q.v.x)+sqr(m_pNewCoords->q.v.y)<0.001f) {
		Vec3 size = m_size+Vec3(0,0,m_bUseCapsule*m_size.x);
		BBox[0] = min(BBox[0],pt+Vec3(0,0,m_hCyl)-size); BBox[1] = max(BBox[1],pt+Vec3(0,0,m_hCyl)+size); 
	}
	if (m_pHeadGeom) {
		pt = m_pNewCoords->pos+m_pNewCoords->q*Vec3(0,0,m_hHead+m_pHeadGeom->m_sphere.r-m_hPivot);
		BBox[0] = min(BBox[0],pt); BBox[1] = max(BBox[1],pt);
	}
}

void CLivingEntity::UpdatePosition(const Vec3 &pos, const Vec3 *BBox, int bGridLocked)
{
	WriteLockCond lock(m_lockUpdate,m_bStateReading^1); 
	m_pos = pos;

	if (m_nParts==1)	{
		m_parts[0].BBox[0]=m_parts[0].pNewCoords->BBox[0]; m_parts[0].BBox[1]=m_parts[0].pNewCoords->BBox[1];
		m_parts[0].pNewCoords = (coord_block_BBox*)&m_parts[0].pos;
		m_BBox[0] = BBox[0]; m_BBox[1] = BBox[1];
	} else {
		for(int i=0;i<m_nParts;i++) m_parts[i].pNewCoords = (coord_block_BBox*)&m_parts[i].pos;
		ComputeBBox(m_BBox);
	}
	m_pWorld->UnlockGrid(this,-bGridLocked);
}

void CLivingEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
	if (m_dtRequested>0) {
		if( time_interval > m_dtRequested )
			m_timeStepFull = m_dtRequested;
		m_dtRequested = 0;
	}
	m_nContacts = 0; m_bMoved = 0;
}


int CLivingEntity::Step(float time_interval)
{
	if (time_interval<=0)
		return 1;
	float dt = m_timeStepFull-m_timeStepPerformed;
	time_interval = m_pWorld->m_bWorldStep==2 ? min(time_interval, dt) : dt;
	time_interval = max(time_interval, 0.001f);

	const int iCaller = get_iCaller_int();
	int i,j,jmin,ipartMin,nents,ncont,bFlying,bWasFlying,bUnprojected,idmat,iPrim, bHasExtraParts=0,
		bHasFastPhys,icnt,nUnproj,bStaticUnproj,bDynUnproj,bMoving=0,nPrecompEnts=0,nPrecompParts=0, nUsedPartsCount=0,
		&nNoResponseAllocLE=m_pWorld->m_threadData[iCaller].nNoResponseAllocLE,
		&nPrecompEntsAlloc=m_pWorld->m_threadData[iCaller].nPrecompEntsAllocLE,&nPrecompPartsAlloc=m_pWorld->m_threadData[iCaller].nPrecompPartsAllocLE; 
	const Quat oldQRot = m_qrot;
	Vec3 pos,vel,pos0,vel0,newpos,move(ZERO),nslope,ncontactHist[4],ncontact,ptcontact,ncontactSum,BBoxInner[2],velGround,axis,sz,heightAdj=Vec3(0.f);
	float movelen,tmin,vrel,movesum,kInertia;
	le_precomp_entity *&pPrecompEnts=m_pWorld->m_threadData[iCaller].pTmpPrecompEntsLE;
	le_precomp_part *&pPrecompParts=m_pWorld->m_threadData[iCaller].pTmpPrecompPartsLE;
	le_tmp_contact *&pNoResponseContactLE=m_pWorld->m_threadData[iCaller].pTmpNoResponseContactLE, *pNRC;
	le_contact unproj[8];
	CCylinderGeom CylinderGeomOuter,*pCyl[2];
	geom_world_data gwd[3];
	intersection_params ip;
	CPhysicalEntity **pentlist, *pentmin=NULL;
	geom_contact *pcontacts;
	pe_action_impulse ai;
	pe_status_dynamics sd;
	WriteLockCond lockMain(m_lockStep,m_bStateReading^1);
	ip.bNoAreaContacts = true;

	IF (nNoResponseAllocLE==0, 0) pNoResponseContactLE=new le_tmp_contact[nNoResponseAllocLE=16];

	if (m_timeForceInertia>0.0001f)
		kInertia = 6.0f;
	else if (m_kInertiaAccel && m_velRequested.len2()>0.1f)
		kInertia = m_kInertiaAccel;
	else
		kInertia = m_kInertia;

	pos = m_pWorld->m_bWorldStep==2 ? m_pos : SyncWithGroundCollider(time_interval);
	vel0=vel = m_vel;
	bWasFlying = bFlying = m_bFlying;
	if (!m_bStateReading) {
		m_timeUseLowCap -= time_interval;
		m_timeSinceStanceChange += time_interval;
		m_timeSinceImpulseContact += time_interval;
		m_timeForceInertia = max(0.0f,m_timeForceInertia-time_interval);
		m_timeStepPerformed += time_interval;
	}

	if (m_bActive && 
			(!(vel.len2()==0 && m_velRequested.len2()==0 && (!bFlying || m_gravity.len2()==0) && m_dhSpeed==0 && m_dhAcc==0) || 
			m_bActiveEnvironment || m_nslope.z<m_slopeSlide || m_velGround.len2()>0))	
	{
		CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
		PHYS_ENTITY_PROFILER

		m_bActiveEnvironment = 0;
		//m_nslope.Set(0,0,1);
		if (kInertia==0 && !bFlying && !m_bJumpRequested || m_pWorld->m_vars.bFlyMode) {
			vel = m_velRequested;
			vel -= m_nslope*(m_nslope*vel);
		}
		if (bFlying && !m_pWorld->m_vars.bFlyMode && m_kAirControl>0)	{
			if (kInertia>0) {
				Vec3 velDelta = m_velRequested * (m_kInertia*time_interval*m_kAirControl);
				Vec3 velDiff = m_velRequested - vel;
					
				const float kAirControlSelect = m_kAirControl-1.0f;

				velDelta.x = (float)__fsel(-(velDiff.x * velDelta.x), 0.0f, velDelta.x);
				velDelta.y = (float)__fsel(-(velDiff.y * velDelta.y), 0.0f, velDelta.y);
				velDelta.z = (float)__fsel(-(velDiff.z * velDelta.z), 0.0f, velDelta.z);

				velDelta.x = (float)__fsel(velDiff.x, min(velDiff.x, velDelta.x), max(velDiff.x, velDelta.x));
				velDelta.y = (float)__fsel(velDiff.y, min(velDiff.y, velDelta.y), max(velDiff.y, velDelta.y));
				velDelta.z = (float)__fsel(velDiff.z, min(velDiff.z, velDelta.z), max(velDiff.z, velDelta.z));
				
				velDelta.y = (float)__fsel(kAirControlSelect, velDiff.y, velDelta.y);
				velDelta.x = (float)__fsel(kAirControlSelect, velDiff.x, velDelta.x);				

				vel += velDelta;
			} else if (m_gravity.len2()>0)
				vel = m_gravity*(vel*m_gravity-m_velRequested*m_gravity)/m_gravity.len2()+m_velRequested;
			else
				vel = m_velRequested;
		}

		//filippo:m_forceFly is to let the game set a velocity no matter what is the status of the entity.
		if (m_forceFly)
			vel = m_velRequested;
 		else if (bFlying && !m_bSwimming && !m_pWorld->m_vars.bFlyMode) 
 			move += m_gravity*sqr(time_interval)*0.5f; 

		if (vel.len2() > sqr(m_pWorld->m_vars.maxVelPlayers))
			vel.normalize() *= m_pWorld->m_vars.maxVelPlayers;
		
		move += vel*time_interval;

		m_forceFly = false;
		bUnprojected = 0;
		axis = m_qrot*Vec3(0,0,1);
		if (_isnan(move.len2())) //necessary? Is there any way that this can occur? - Rich S
			return 1;
		--m_bSquashed; m_bSquashed-=m_bSquashed>>31;

		if (m_pWorld->m_vars.iCollisionMode!=0 && m_pWorld->m_vars.bFlyMode) {	//Rich S - is bFlyMode a cheat? Can we disable in release?
			pos+=move; bFlying=1; m_hLatest=0; ReleaseGroundCollider();
		} else {

			const float fSizeX = m_size.x;
			const float fSizeZ = m_size.z;
			movelen = move.len(); 
			const float fGap = (movelen+m_pWorld->m_vars.maxContactGapPlayer)*1.5f;
			const Vec3 posDiff = pos - m_pos, maxUpStep = axis*(m_hCyl-m_size.z-m_size.x*m_bUseCapsule);
			BBoxInner[0] = m_BBox[0]+(posDiff)-Vec3(fGap,fGap,fGap)+min(maxUpStep,Vec3(0));
			BBoxInner[1] = m_BBox[1]+(posDiff)+Vec3(fGap,fGap,fGap)+max(maxUpStep,Vec3(0));
			const float fGap2 = max(10.0f*time_interval,fGap); // adds a safety margin of m_size.x width
			const Vec3 BBoxOuter0 = m_BBox[0]+(posDiff)-Vec3(fGap2,fGap2,fGap2)+min(maxUpStep,Vec3(0));
			const Vec3 BBoxOuter1 = m_BBox[1]+(posDiff)+Vec3(fGap2,fGap2,fGap2)+max(maxUpStep,Vec3(0));

			nents = m_pWorld->GetEntitiesAround(BBoxOuter0,BBoxOuter1, 
				pentlist, m_collTypes|ent_independent|ent_triggers|ent_sort_by_mass, this, 0,iCaller);

			if (m_vel.len2()>sqr(0.01f) || m_velRequested.len2()) for(i=0;i<m_nColliders;i++) if (m_pColliders[i]->HasConstraintContactsWith(this,constraint_inactive))
				m_pColliders[i]->Awake();

			const float fMassInv = m_massinv;
			for(i=j=bHasFastPhys=0,vrel=0; i<nents; i++) if (!m_pForeignData || pentlist[i]->m_pForeignData!=m_pForeignData){
				Vec3 size = pentlist[i]->m_BBox[1]-pentlist[i]->m_BBox[0];
				int bFastPhys = 0;
				if (IgnoreCollision(m_collisionClass, pentlist[i]->m_collisionClass))
					continue;
				if (pentlist[i]->m_iSimClass==2) {
					if (pentlist[i]->m_flags & ref_small_and_fast)
						continue;
					else if (pentlist[i]->GetMassInv()*0.4f<fMassInv) {
						pentlist[i]->GetStatus(&sd);
						vrel = max(vrel,sd.v.len()+sd.w.len()*max(max(size.x,size.y),size.z));;
						bHasFastPhys |= (bFastPhys = isneg(fSizeX*0.2f-vrel*time_interval));
					}
				}
				if (!bFastPhys && !AABB_overlap(pentlist[i]->m_BBox,BBoxInner) && size.len2()>0)
					continue;
				idmat = pentlist[i]->GetType();
				if (idmat==PE_SOFT || idmat==PE_ROPE) 
					pentlist[i]->Awake();
				else if (pentlist[i]->m_iSimClass<4 && 
					(idmat!=PE_LIVING && !pentlist[i]->IgnoreCollisionsWith(this,1) || 
					 idmat==PE_LIVING && pentlist[i]->m_parts[0].flags&collider_flags && 
					 pentlist[i]!=this && m_pLivingEntToIgnore!=pentlist[i] && ((CLivingEntity*)pentlist[i])->m_pLivingEntToIgnore!=this))
				{
					if (pentlist[i]->m_iSimClass==1 && m_timeSinceImpulseContact<0.2f && pentlist[i]->GetMassInv()>0)	{
						int ipart; unsigned int flags; 
						for(ipart=0,flags=0; ipart<pentlist[i]->m_nParts; ipart++) 
							flags |= pentlist[i]->m_parts[ipart].flags;
						if (flags & collider_flags)
							pentlist[i]->Awake();
					}
					pentlist[j++] = pentlist[i];
				}
			}
			nents = j;
			pos0 = pos;
			bStaticUnproj = bDynUnproj = 0;
			newpos = pos+move;

			IF (nents>nPrecompEntsAlloc, 0) delete[] pPrecompEnts, pPrecompEnts=new le_precomp_entity[nPrecompEntsAlloc=nents];
			for(i=0; i<nents; i++) {
				le_precomp_entity &ent = pPrecompEnts[nPrecompEnts++];
				ent.BBox[0]=pentlist[i]->m_BBox[0];
				ent.BBox[1]=pentlist[i]->m_BBox[1]; 
				ent.sz=(sz=pentlist[i]->m_BBox[1]-pentlist[i]->m_BBox[0]); 
				ent.bCheckBBox=sz.len2()>0;
				ent.massinv=pentlist[i]->GetMassInv(); 
				ent.entType=pentlist[i]->GetType(); 
				ent.iSimClass=pentlist[i]->m_iSimClass; 
				ent.pent=pentlist[i]; 
				ent.nParts=0;
				ent.ignoreCollisionsWith=pentlist[i]->IgnoreCollisionsWith(this,1);
				ent.iPartsBegin=nPrecompParts; 
				ent.iPartsEnd=nPrecompParts+(nUsedPartsCount=pentlist[i]->GetUsedPartsCount(iCaller));
				IF (ent.iPartsEnd>nPrecompPartsAlloc, 0) { 
					ReallocateList(pPrecompParts, nPrecompParts, ent.iPartsEnd+128); 
					nPrecompPartsAlloc=ent.iPartsEnd+128; 
				} 
				for(int j1=0; j1<nUsedPartsCount; j1++) {
					le_precomp_part &part = pPrecompParts[nPrecompParts++];
					Vec3 *pBBox = pentlist[i]->GetPartBBox(part.ipart=j=pentlist[i]->GetUsedPart(iCaller,j1), part.BBox, this);
					part.BBox[0]=pBBox[0]; part.BBox[1]=pBBox[1];
					pentlist[i]->GetPartTransform(j, part.partoff,part.partrot,part.partscale, this);
					part.partflags=pentlist[i]->m_parts[j].flags; 
					part.pgeom=pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom; 
					part.surface_idx=pentlist[i]->m_parts[j].surface_idx;
					ent.nParts+=1-iszero(-j1>>31 & part.partflags & (int)collider_flags);
				}
			}
		
			// first, check if we need unprojection in the initial position
			if (sqr(m_qrot.v.x)+sqr(m_qrot.v.y)<sqr(0.001f))
				gwd[0].R.SetIdentity();
			else
				gwd[0].R = Matrix33(m_qrot);
			gwd[0].centerOfMass = gwd[0].offset = pos + gwd[0].R*m_parts[0].pos; 
			gwd[0].v.zero(); gwd[0].w.zero(); // since we check a static character against potentially moving environment here
			ip.vrel_min = fSizeX;
			ip.time_interval = time_interval*2;
			ip.maxUnproj = fSizeX*2.5f;
			ip.ptOutsidePivot[0] = gwd[0].offset;
			pCyl[0] = m_pCylinderGeom;
			if (bHasFastPhys) {
				cylinder cylOuter;
				cylOuter.r = fSizeX+min(fSizeX*1.5f,vrel*time_interval);
				cylOuter.hh = fSizeZ+min(fSizeZ,vrel*time_interval);
				cylOuter.center.zero();
				cylOuter.axis.Set(0,0,1);
				CylinderGeomOuter.CreateCylinder(&cylOuter);
				pCyl[1] = &CylinderGeomOuter;
			}

			retry_without_ground_sync:
			if (m_parts[0].flagsCollider) {
				for(i=nUnproj=0,ncontactSum.zero();i<nents;++i) 
					if (pPrecompEnts[i].entType!=PE_LIVING) {
					const float minv = pPrecompEnts[i].massinv;
					const int bHeavy = isneg(minv*0.4f-fMassInv);
					const int iSimClass = pPrecompEnts[i].iSimClass;
					CPhysicalEntity *const pent = pPrecompEnts[i].pent;
					int bFastPhys = 0;
					if (bHasFastPhys && bHeavy && iSimClass==2 && pent!=m_pLastGroundCollider) {
						pent->GetStatus(&sd);
						const Vec3 &sz0 = pPrecompEnts[i].sz;
						vrel = max(vrel,sd.v.len()+sd.w.len()*max(max(sz0.x,sz0.y),sz0.z));
						bFastPhys = isneg(fSizeX*0.2f-vrel*time_interval);
						gwd[1].v = sd.v; gwd[1].w = sd.w;
						gwd[1].centerOfMass = sd.centerOfMass;
					}	else {
						gwd[1].v.zero(); gwd[1].w.zero();
					}

					for(int iCyl=0; iCyl<bFastPhys+m_nParts; iCyl++) {
						int igwd,flagsCollider;	IGeometry *pCurCyl; 
						if (iCyl<=bFastPhys) {
							pCurCyl=pCyl[iCyl]; igwd=0;	flagsCollider=collider_flags;
						} else {
							pCurCyl = m_parts[iCyl-bFastPhys].pPhysGeom->pGeom;
							gwd[igwd=2].R = Matrix33(m_qrot*m_parts[iCyl-bFastPhys].q);
							gwd[2].centerOfMass = gwd[2].offset = pos + m_qrot*m_parts[iCyl-bFastPhys].pos; 
							gwd[2].scale = m_parts[iCyl-bFastPhys].scale;
							if (!((flagsCollider = m_parts[iCyl-bFastPhys].flagsCollider) & geom_colltype_solid))
								continue;
							bHasExtraParts = 1;
						}
						for(int j1=pPrecompEnts[i].iPartsBegin, bCheckBBox=pPrecompEnts[i].bCheckBBox; j1<pPrecompEnts[i].iPartsEnd; ++j1)
							if (pPrecompParts[j1].partflags & flagsCollider && (!bCheckBBox || AABB_overlap(BBoxInner,pPrecompParts[j1].BBox)))	{
								gwd[1].R = Matrix33(pPrecompParts[j1].partrot);
								gwd[1].offset = pPrecompParts[j1].partoff;
								gwd[1].scale = pPrecompParts[j1].partscale;
								j = pPrecompParts[j1].ipart;
								//if (m_pWorld->m_pRenderer) m_pWorld->m_pRenderer->DrawGeometry(pent->m_parts[j].pPhysGeomProxy->pGeom, &gwd[1], 4); 

								if (icnt=pCurCyl->Intersect(pPrecompParts[j1].pgeom, gwd+igwd,gwd+1, &ip, pcontacts)) {
									const uint32 uFlags = m_flags;
									for(ncont=0; ncont<icnt-1 && pcontacts[ncont].dir*(pcontacts[ncont].pt-gwd[0].offset)>0; ncont++);
									if ((pos0-m_pos).len2()>sqr(0.001f)) {//m_pos-posncont==icnt) {
										gwd[0].offset+=m_pos-pos; move-=m_pos-pos; pos=m_pos; 
										pos0=m_pos; goto retry_without_ground_sync;	// if we made SyncWithGroundCollider and it caused intersections, roll it back
										//continue;
									}
									if (iCyl==0 && bHeavy && !(pPrecompParts[j1].partflags & geom_no_coll_response)) {
										const int bVeryHeavyFlags = (~(-isneg(minv*2-fMassInv))) | pent->m_flags;
										if (m_pWorld->m_bWorldStep==2) { // this means step induced by rigid bodies moving around
											// if the entity is rigid, store the contact
											int bPushOther;
											if (bPushOther = iSimClass>0 && minv>0) {// && pent->m_iGroup==m_pWorld->m_iCurGroup) {
												nUnproj = min(nUnproj+1,(int)(CRY_ARRAY_COUNT(unproj)));
												unproj[nUnproj-1].pent = pent;
												unproj[nUnproj-1].ipart = j;
												unproj[nUnproj-1].pt = pcontacts[ncont].pt;
												unproj[nUnproj-1].n = pcontacts[ncont].n;
												unproj[nUnproj-1].center = gwd[0].offset;
												unproj[nUnproj-1].penetration = pcontacts[ncont].t;
											}	else if (!(bVeryHeavyFlags & pef_cannot_squash_players))	{
												m_bSquashed = min(5, m_bSquashed+5*isneg(ncontactSum*pcontacts[ncont].n+0.99f));
												if (iSimClass>0)
													ncontactSum = pcontacts[ncont].n;
											}

											// check previous contacts from this frame, register in entity if there are conflicts
											for(icnt=0; icnt<nUnproj-bPushOther; icnt++) {
												const float fUnprojDotContact = unproj[icnt].n*pcontacts[ncont].n;
												if (fUnprojDotContact<0) {
													RegisterUnprojContact(unproj[icnt]);
													if (bPushOther)
														RegisterUnprojContact(unproj[nUnproj-1]);
													if (!((bVeryHeavyFlags|unproj[icnt].pent->m_flags) & pef_cannot_squash_players))
														m_bSquashed = min(5, m_bSquashed+5*isneg(max(5*minv-fMassInv, fUnprojDotContact+0.99f)));
												}	
											}
										} else if (!(bVeryHeavyFlags & pef_cannot_squash_players)) {
											m_bSquashed = min(5, m_bSquashed+5*isneg(ncontactSum*pcontacts[ncont].n+0.99f));
											if (iSimClass>0)
												ncontactSum = pcontacts[ncont].n;
										}

										if (uFlags & pef_pushable_by_players) {
											(minv==0 ? bStaticUnproj:bDynUnproj)++;
											Vec3 offs = pcontacts[ncont].dir*(pcontacts[ncont].t+m_pWorld->m_vars.maxContactGapPlayer);
											pos += offs; gwd[0].offset += offs;
											bUnprojected = 1;
											if (pcontacts[ncont].t>m_size.x)
												ip.ptOutsidePivot[0].Set(1E11f,1E11f,1E11f);
										}
									}

									if (iSimClass==2) {
										Matrix33 K(ZERO);
										if (uFlags & pef_pushable_by_players)
											K += Matrix33(Diag33(m_massinv));
										int bPushOther = (uFlags & lef_push_objects) && (pent->m_flags & pef_pushable_by_players);
										bPushOther |= iszero(iSimClass-2);
										bPushOther &= iszero((int)(INT_PTR)pent-(int)(INT_PTR)m_pLastGroundCollider)^1;
										if (bPushOther)	{
											RigidBody body(false), *pbody=pent->GetRigidBodyTrans(&body, j, this, 0,true);
											pbody->GetContactMatrix(pcontacts[ncont].center-pbody->pos, K);
										}
										else if (!(uFlags & pef_pushable_by_players))
											continue;
										pcontacts[ncont].center -= pcontacts[ncont].dir*pcontacts[ncont].t;
										ncontact = -pcontacts[ncont].n;//(gwd[0].offset-pcontacts[ncont].center).normalized();
										if (fabs_tpl(ncontact.z)<0.5f) {
											ncontact.z=0; ncontact.normalize();
										}
										RigidBody body(false), *pbody = pent->GetRigidBodyTrans(&body,j,this);
										vrel = ncontact*(pbody->v+(pbody->w^pcontacts[ncont].center-pbody->pos)-vel-m_velGround);
										if (iCyl==0 || fabs_tpl(vrel)*time_interval>m_size.x*0.2f) {
											vrel = max(0.0f,vrel-ncontact*(vel+m_velGround));
											float imp=vrel/max(1e-6f,ncontact*K*ncontact);
											ai.impulse = ncontact*imp;
											ai.point = pcontacts[ncont].center;	ai.ipart = 0;
											if (ai.impulse.len2()*sqr(fMassInv) > max(1.0f,sqr(vrel)))
												ai.impulse.normalize() *= fabs_tpl(vrel)*m_mass*0.5f;
											if ((uFlags & (pef_pushable_by_players|geom_no_coll_response)) == pef_pushable_by_players)
												vel += ai.impulse*fMassInv; 
											/*if (vel.z>-5) {
											vel.z = max(0.0f, vel.z); vel.z += ai.impulse.len()*fMassInv*0.1f; 
											}*/
											bFlying = 1; m_timeFlying = 0;
											if (m_kInertia==0)
												m_timeForceInertia = m_timeImpulseRecover;
											if (bPushOther) {
												if (fabs_tpl(pcontacts[ncont].dir*axis)<0.7f)
													ai.impulse -= axis*(axis*ai.impulse); // ignore vertical component - might be semi-random depending on cylinder intersection
												ai.impulse.Flip(); ai.ipart = j;
												//if (pent->GetType()<PE_LIVING)
												//	ai.impulse *= 0.2f;
												pent->Action(&ai,1);
												m_timeSinceImpulseContact = 0;
											}
											idmat = pPrecompParts[j1].surface_idx&pcontacts[ncont].id[1]>>31 | max(pcontacts[ncont].id[1],0);
											RegisterContact(pos,pcontacts[ncont].pt,ncontact,pent,j,idmat,imp,0,pcontacts[ncont].iPrim[1]);
										}
									}
									//break;
								}
							}
					}
				}
			}
			if (bStaticUnproj && bDynUnproj && m_bSquashed) {
				pos = pos0; // disable unprojection if we are being squashed
				bStaticUnproj = bDynUnproj = 0;
			}
			else if (bStaticUnproj+bDynUnproj>0) {
				newpos = pos+move;
			}
			
			float h = ShootRayDown(pPrecompEnts,nents,pPrecompParts,newpos,nslope);
			float hcur = newpos*axis-m_hPivot;
			const float fAxisDotSlope = axis*nslope;
			if (fAxisDotSlope>m_slopeFall && 
					(hcur<h && hcur>h-(m_hCyl-m_size.z)*1.01f || 
					 hcur>h && sqr_signed(hcur-h)<vel.len2()*sqr(time_interval) && !bFlying && !m_bJumpRequested && !m_bSwimming)) 
			{
				if (h>hcur && m_nslope*axis<m_slopeSlide && m_nslope*nslope<m_slopeSlide && 
						fAxisDotSlope<m_slopeSlide && m_velRequested.len2()==0) 
				{	
					newpos = pos; vel.zero();
				}	else
					newpos += (heightAdj = axis*(h+m_hPivot-newpos*axis));
				move = newpos-pos; movelen = move.len();
			}
			pos0 = pos;
			if (m_bJumpRequested)
				AddLegsImpulse(-vel,m_nslope,true);
			m_bJumpRequested = 0;
			m_bStuck = 0; ncontactSum.zero();

			if (movelen>m_size.x*1E-4f && m_parts[0].flagsCollider!=0) {
				ip.bSweepTest = true;
				gwd[0].v = move/movelen;
				int iter = 0; 
				float move0 = movelen; movesum = 0;
				pe_player_dimensions pd;

				do {
					float tlim = 0.0f;
					gwd[0].offset = pos + gwd[0].R*m_parts[0].pos; 
					ip.time_interval = movelen+m_pWorld->m_vars.maxContactGapPlayer; tmin = ip.time_interval*2;	iPrim = -1;
					pNRC = pNoResponseContactLE; pNRC->pent = NULL; pNRC->tmin = tmin;
					for(i=0; i<nents; ++i) {
						if (pPrecompEnts[i].entType!=PE_LIVING || 
								pPrecompEnts[i].nParts+bHasExtraParts > 0 || 
								max(sqr(m_qrot.v.x)+sqr(m_qrot.v.y),sqr(pPrecompEnts[i].pent->m_qrot.v.x)+sqr(pPrecompEnts[i].pent->m_qrot.v.y))>0.001f) 
						{
							CPhysicalEntity *const pent=pPrecompEnts[i].pent;
							int bCheckBBox=pPrecompEnts[i].bCheckBBox;
							for(int j1=pPrecompEnts[i].iPartsBegin; j1<pPrecompEnts[i].iPartsEnd; ++j1) {
								const uint32 uPartFlags = pPrecompParts[j1].partflags;
								if (uPartFlags & (collider_flags|geom_log_interactions) && 
									(!bCheckBBox || AABB_overlap(BBoxInner,pPrecompParts[j1].BBox))) 
								{
									if (uPartFlags & geom_log_interactions) {
										EventPhysBBoxOverlap event;
										event.pEntity[0]=this; event.pForeignData[0]=m_pForeignData; event.iForeignData[0]=m_iForeignData;
										event.pEntity[1]=pent; event.pForeignData[1]=pent->m_pForeignData; event.iForeignData[1]=pent->m_iForeignData;
										m_pWorld->OnEvent(m_flags, &event);
										if (!(uPartFlags & collider_flags))
											continue;
									}
									gwd[1].R = Matrix33(pPrecompParts[j1].partrot);
									gwd[1].offset = pPrecompParts[j1].partoff;
									gwd[1].scale = pPrecompParts[j1].partscale; 

									int ipart = 0;
									if((ncont = m_pCylinderGeom->Intersect(pPrecompParts[j1].pgeom, gwd,gwd+1, &ip, pcontacts)))
										got_contact:
										if (pcontacts[ncont-1].t<tmin && pcontacts[ncont-1].n*gwd[0].v>0)	{
											if (!((pPrecompParts[j1].partflags | m_parts[ipart].flags)&geom_no_coll_response)) {
												// Solid contact
												tmin = pcontacts[ncont-1].t; tlim=0.0f; ncontact = -pcontacts[ncont-1].n; ptcontact = pcontacts[ncont-1].pt; iPrim = pcontacts[ncont-1].iPrim[1];
												pentmin=pent; jmin=pPrecompParts[j1].ipart; idmat=pPrecompParts[j1].surface_idx&pcontacts[ncont-1].id[1]>>31 | max(pcontacts[ncont-1].id[1],0); ipartMin=ipart;
											} else if (pcontacts[ncont-1].t<pNRC->tmin) {
												// Non-response contact
												pNRC->pent = pent; pNRC->tmin = pcontacts[ncont-1].t;
												pNRC->ptcontact = pcontacts[ncont-1].pt; pNRC->ncontact = -pcontacts[ncont-1].n;
												pNRC->ipart = pPrecompParts[j1].ipart; pNRC->iPrim = pcontacts[ncont-1].iPrim[1];
												pNRC->idmat = pPrecompParts[j1].surface_idx&pcontacts[ncont-1].id[1]>>31 | max(pcontacts[ncont-1].id[1],0);
											}
										}
									for(ipart++; ipart<(m_nParts&-bHasExtraParts); ipart++) if (m_parts[ipart].flagsCollider & uPartFlags) {
										gwd[2].R = Matrix33(m_qrot*m_parts[ipart].q);
										gwd[2].centerOfMass = gwd[2].offset = pos + m_qrot*m_parts[ipart].pos; 
										gwd[2].scale = m_parts[ipart].scale; gwd[2].v = gwd[0].v;
										if((ncont = m_parts[ipart].pPhysGeomProxy->pGeom->Intersect(pPrecompParts[j1].pgeom, gwd+2,gwd+1, &ip, pcontacts)))
											goto got_contact;
									}
								}
							}
						}	else {
							CPhysicalEntity *const pent=pPrecompEnts[i].pent;
							pent->GetParams(&pd);
							Vec2 dorigin,ncoll,move2d=(Vec2)gwd[0].v;
							dorigin = Vec2(pos-pent->m_pos);
							float kb=dorigin*move2d, kc=len2(dorigin)-sqr(m_size.x+pd.sizeCollider.x), kd=kb*kb-kc, zup0,zdown0,zup1,zdown1;
							if (kd>=0) {
								zup0 = (zdown0 = pos.z-m_hPivot)+m_hCyl+m_size.z+m_size.x*m_bUseCapsule; 
								zdown0 = max(zdown0, zup0-(m_size.x*m_bUseCapsule+m_size.z)*2);
								zup1 = (zdown1 = pent->m_pos.z-pd.heightPivot)+pd.heightCollider+pd.sizeCollider.z+pd.sizeCollider.x*pd.bUseCapsule;
								zdown1 = max(zdown1, zup1-(pd.sizeCollider.x*pd.bUseCapsule+pd.sizeCollider.z)*2);
								kd=sqrt_tpl(kd); 
								float tfirst=-kb+kd; ncoll = Vec2(pos+gwd[0].v*tfirst-pent->m_pos);
								bool bSideContact = min(zup0+gwd[0].v.z*tfirst,zup1)>max(zdown0+gwd[0].v.z*tfirst,zdown1);
								if (tfirst>-m_size.x && tfirst<tmin && ncoll*move2d>=0 && bSideContact)
									continue;	// if entities separate during this timestep, ignore other collisions
								tfirst=-kb-kd; ncoll = Vec2(pos+gwd[0].v*tfirst-pent->m_pos);
								/*if (tfirst<-m_size.x || min(zup0+gwd[0].v.z*tfirst,zup1)<max(zdown0+gwd[0].v.z*tfirst,zdown1) || ncoll*move2d>=0) { 
									tfirst=-kb+kd; ncoll = Vec2(pos+gwd[0].v*tfirst-pent->m_pos); 
								}*/
								if (tfirst>-m_size.x && tfirst<tmin && ncoll*move2d<0 && bSideContact) { 
									tlim = (tmin = tfirst)*iszero(iter); ncontact.Set(ncoll.x,ncoll.y,0).normalize(); ptcontact = pos+gwd[0].v*tfirst-ncontact*m_size.x;
									pentmin=pent; jmin=0; idmat=m_surface_idx; iPrim=-1; ipartMin=0;
								}
								// also check for cap-cap contact
								if (fabs_tpl(gwd[0].v.z)>m_size.z*1E-5f) {
									int nSignZ = sgnnz(gwd[0].v.z);	//Could be float?
									zup0 = pos.z-m_hPivot+m_hCyl+(m_size.z+m_size.x*m_bUseCapsule)*nSignZ;
									zdown1 = pent->m_pos.z-pd.heightPivot+pd.heightCollider-(pd.sizeCollider.z+pd.sizeCollider.x*pd.bUseCapsule)*nSignZ; 
									tfirst = zdown1-zup0;
									if (inrange(tfirst,-m_size.x*gwd[0].v.z,tmin*gwd[0].v.z) && 
											len2(dorigin*gwd[0].v.z+move2d*tfirst)<sqr((m_size.x+pd.sizeCollider.x)*gwd[0].v.z)) 
									{
										tmin = tfirst/gwd[0].v.z; ncontact.Set(0,0,-nSignZ); (ptcontact = pos+gwd[0].v*tfirst).z += m_size.z*nSignZ; //? 
										pentmin=pent; jmin=-1; idmat=m_surface_idx; iPrim=-1;	ipartMin=0;
									}
								}
							}
						}
						if (pNRC->pent) {
							IF (pNRC>=(pNoResponseContactLE+nNoResponseAllocLE-1), 0) {
								delete [] pNoResponseContactLE; pNoResponseContactLE=new le_tmp_contact[nNoResponseAllocLE+16];
								pNRC=pNoResponseContactLE+nNoResponseAllocLE-1; nNoResponseAllocLE+=16;
							}
							++pNRC; pNRC->pent=NULL; pNRC->tmin=ip.time_interval*2;
						}
					}

					if (tmin<=ip.time_interval) {
						tmin = max(tlim,tmin-m_pWorld->m_vars.maxContactGapPlayer);
						pos += gwd[0].v*tmin; 
						static const float g_cosSlide=cos_tpl(0.3f), g_sinSlide=sin_tpl(0.3f);
						/*if (bFlying) {
							if ((ncontact*axis)*(1-m_bSwimming)>g_cosSlide)
								ncontact = axis*g_cosSlide + (pos-ptcontact-axis*(axis*(pos-ptcontact))).normalized()*g_sinSlide;
						} else */
						if (!bFlying && inrange(ncontact*axis, 0.85f,-0.1f) && (unsigned int)pentmin->m_iSimClass-1u<2u && 
							pentmin->GetMassInv()>m_massinv*0.25f)
							ncontact.z=0, ncontact.normalize();
						int bPush = pentmin->m_iSimClass>0 || isneg(min(m_slopeClimb-ncontact*axis, ncontact*axis+m_slopeFall));
						int bUnmovable = isneg(-pentmin->m_iSimClass>>31 & ~(-((int)m_flags & pef_pushable_by_players)>>31));
						bPush &= bUnmovable^1;
						{
							Matrix33 K(ZERO);
							int bPushOther = (m_flags & (pentmin->m_iSimClass==3 ? lef_push_players : lef_push_objects)) && 
								(pentmin->m_flags & pef_pushable_by_players) && 
								(pentmin->m_iSimClass | m_pWorld->m_vars.bPlayersCanBreak | pentmin->m_flags & pef_players_can_break);
							bPushOther &= iszero((int)(INT_PTR)pentmin-(int)(INT_PTR)m_pLastGroundCollider)^1;
							bPushOther |= bUnmovable;
							if (bPushOther)	{
								RigidBody body(false), *pbody=pentmin->GetRigidBodyTrans(&body, jmin, this, 0,true);
								pbody->GetContactMatrix(ptcontact-pbody->pos, K);
							}
							if (!bPushOther || /*pentmin->m_iSimClass-3 | */m_flags & pef_pushable_by_players)
								K += Matrix33(Diag33(m_massinv));
							else 
								bPush = 0;
							vrel = ncontact*(vel+m_velGround); //(ncontact*gwd[0].v)*vel.len();
							ai.impulse = ncontact;
							if (pentmin->m_iSimClass==3) {
								// make the player slide off when standing on other players
								vrel -= ncontact*((CLivingEntity*)pentmin)->m_vel;
								if (ncontact*axis > 0.95f) {
									ai.impulse += (pos-pentmin->m_pos-axis*(axis*(pos-pentmin->m_pos))).normalized();
									if (inrange(vrel,-1.0f,1.0f))
										vrel = -1.0f;
								}
							} else {
								RigidBody body(false), *pbody = pentmin->GetRigidBodyTrans(&body,jmin,this);
								vrel -= ncontact*(pbody->v+(pbody->w^ptcontact-pbody->pos));
							}
							vrel = min(0.0f, vrel);
							float imp = -vrel*1.01f/max(1e-6f,ncontact*K*ncontact);
							jmin -= jmin>>31;
							if (bPush || m_flags & lef_report_sliding_contacts || pentmin->m_parts[jmin].flags & geom_manually_breakable)
								RegisterContact(pos,ptcontact,ncontact,pentmin,jmin,idmat,imp*bPush,0,iPrim,ipartMin);
							ai.impulse *= imp;
							ai.point = ptcontact;	ai.ipart = 0;
							if (ai.impulse.len2()*sqr(m_massinv) > max(1.0f,sqr(vrel)))
								ai.impulse.normalize() *= fabs_tpl(vrel)*m_mass*0.5f;
							if (bPush) {
								vel += ai.impulse*m_massinv;
								if (m_kInertia==0 && (pentmin->m_iSimClass-3 | m_flags & pef_pushable_by_players))
									m_timeForceInertia = m_timeImpulseRecover;
							}
							if (bPushOther) {
								ai.impulse.Flip(); ai.ipart = jmin;
								if (fabs_tpl(ncontact*axis)<0.7f)
									ai.impulse -= axis*(axis*ai.impulse); // ignore vertical component - might be semi-random depending on cylinder intersection
								ai.iApplyTime = isneg(pentmin->m_iSimClass-3)<<1;
								if (pentmin->GetType()<PE_LIVING)
									ai.impulse *= 0.2f;
								pentmin->Action(&ai);
								if (pentmin->m_iSimClass<3)
									m_timeSinceImpulseContact = 0;
							}
						}
						movelen -= tmin; movesum += tmin;
						for(i=0;i<iter && ncontactHist[i]*ncontact<0.95f;i++);
						if (i==iter)
							ncontactSum += ncontact;
						ncontactHist[iter] = ncontact;
						if (iter==1 && movesum==0.0f) {
							ncontact = (ncontactHist[0]^ncontactHist[1]).normalized();
							gwd[0].v = ncontact*(gwd[0].v*ncontact);
						}	else {
							gwd[0].v -= ncontact*(gwd[0].v*ncontact);
							gwd[0].v = gwd[0].v*0.9998f+ncontact*(gwd[0].v.len()*0.02f);
						}
						tmin = gwd[0].v.len(); movelen*=tmin; 
						if (tmin>0) gwd[0].v/=tmin;
						movelen = (float)__fsel(gwd[0].v*move, movelen, 0.0f);
					} else {
						pos += gwd[0].v*movelen; movesum += movelen; /* exit do-loop */ iter=1000;
					}
					for (le_tmp_contact* c=pNoResponseContactLE; c<=pNRC && c->pent; c++)
						if (c->tmin<tmin)
							RegisterContact(pos,c->ptcontact,c->ncontact,c->pent,c->ipart,c->idmat,0.f,0,c->iPrim);
				} while(movelen>m_pWorld->m_vars.maxContactGapPlayer*0.1f && ++iter<3);

				const float fContactSumLenSq = ncontactSum.len2();
				if(movesum<move0*0.001f && (sqr_signed(ncontactSum.z)>sqr(0.4f)*fContactSumLenSq || fContactSumLenSq<0.6f))
					m_bStuck = 1;

				if (m_parts[0].flagsCollider!=0 && (bUnprojected || !(m_flags & lef_loosen_stuck_checks))) {
					ip.bSweepTest = false;
					gwd[0].offset = pos + gwd[0].R*m_parts[0].pos; 
					gwd[0].v = -axis; 
					ip.bStopAtFirstTri = true; ip.bNoBorder = true; ip.time_interval = m_size.z*10;
					for(i=0; i<nents; ++i) 
						if (pPrecompEnts[i].iSimClass==0) {
							CPhysicalEntity *const pent = pPrecompEnts[i].pent;
							for(int j1=pPrecompEnts[i].iPartsBegin; j1<pPrecompEnts[i].iPartsEnd; ++j1) 
								if (pPrecompParts[j1].partflags & collider_flags && !(pPrecompParts[j1].partflags & geom_no_coll_response)) {
									gwd[1].R = Matrix33(pPrecompParts[j1].partrot);
									gwd[1].offset = pPrecompParts[j1].partoff;
									gwd[1].scale = pPrecompParts[j1].partscale;
									if(m_pCylinderGeom->Intersect(pPrecompParts[j1].pgeom, gwd,gwd+1, &ip, pcontacts)) { 
									if (pcontacts->t>m_pWorld->m_vars.maxContactGapPlayer)
										vel.zero(),m_bStuck=1;	
									pos = pos0; m_timeUseLowCap=1.0f; 
									goto nomove; 
								}
							}
					} nomove:;
				}
			} else
				pos += move;

			if (!m_pLastGroundCollider)// || m_pLastGroundCollider->GetMassInv()>m_massinv)
				velGround.zero();
			else
				velGround = m_velGround;
			m_hLatest = h = ShootRayDown(pPrecompEnts, nents, pPrecompParts, pos,nslope, time_interval,false,true);
			if (nslope*axis<0.087f)
				nslope = m_nslope;
			else {
				WriteLockCond lock(m_lockLiving,m_bStateReading^1);
				m_nslope = nslope;
			}
			if (m_pLastGroundCollider) {
				RegisterContact(newpos,newpos,m_qrot*Vec3(0,0,1),m_pLastGroundCollider,m_iLastGroundColliderPart,m_lastGroundSurfaceIdx,0,1,m_lastGroundPrim);
				if (m_pLastGroundCollider->m_iSimClass==0)
					ReleaseGroundCollider();
			}

			if (bFlying)
				m_timeFlying += time_interval;
			int bGroundContact = isneg(max(pos*axis-m_hPivot-(h+m_groundContactEps), m_slopeFall-nslope*axis));
			if (!bGroundContact)
				ReleaseGroundCollider();

			bFlying = m_pWorld->m_vars.bFlyMode || m_gravity*axis>0 || m_bSwimming || ((bGroundContact|m_bStuck)^1);
			m_bActiveEnvironment = m_bStuck;

			if (bFlying)
				Step_HandleFlying(vel, velGround, bWasFlying, heightAdj, kInertia, time_interval);
			else {
				if (bWasFlying)
					Step_HandleWasFlying(vel, bFlying, axis, bGroundContact);
				
				Vec3 velReq = m_velRequested,g;
				if (!m_bSwimming) velReq -= m_nslope*(velReq*m_nslope);
				if (kInertia * time_interval > 1.0f)	kInertia = 1.0f/time_interval;
				Vec3 last_force = (velReq-vel)*kInertia;
				const float axisSlope = m_nslope*axis;
				if (axisSlope<m_slopeSlide && !m_bSwimming)	{
					g = m_gravity;
					last_force += g-m_nslope*(g*m_nslope);
				}
				
				const Vec3 velIncLastForce = vel + (last_force*time_interval);
				if (velIncLastForce*vel<0 && velIncLastForce*m_velRequested<=0)
					vel.zero();
				else
					vel = velIncLastForce;

				if (axisSlope<m_slopeClimb) {
					const float axisVel = vel*axis;
					if (axisVel>0 && last_force*axis>0)
						vel -= axis*(axisVel);
					if ((pos-pos0)*axis > m_size.z*0.001f)
						vel -= axis*(axis*vel);
				}
				if (axisSlope<m_slopeFall && !m_bStuck) {
					bFlying=1; vel += m_nslope-axis*(axisSlope);
				}
				if (m_velRequested.len2()==0 && vel.len2()<0.001f || vel.len2()<0.0001f) 
					vel.zero();
			}

			if (!bFlying)
				m_timeFlying = 0;

			if (m_flags & lef_snap_velocities)
				vel = DecodeVec6b(EncodeVec6b(vel));

			if (!m_bStateReading) {
				float dh;
				if (!bFlying && (dh=(pos-pos0)*axis)>m_size.z*0.01f) {
					m_dhSpeed = max(m_dhSpeed, dh/m_stablehTime);
					m_dh += dh;
					m_stablehTime = 0;
				}	else
					dh = 0;
				m_stablehTime = min(m_stablehTime+time_interval,0.5f);

				m_dhSpeed += m_dhAcc*time_interval;
				if (m_dhAcc==0 && m_dh*m_dhSpeed<0 || m_dh*m_dhAcc<0 || m_dh*(m_dh-m_dhSpeed*time_interval)<0)
					m_dh = m_dhSpeed = m_dhAcc = 0;
				else
					m_dh -= m_dhSpeed*time_interval;
			}

			if (m_pHeadGeom) {
				ip.bSweepTest = true;
				gwd[0].offset = pos + gwd[0].R*m_parts[0].pos; 
				gwd[0].v = axis; 
				tmin = ip.time_interval = m_hHead-m_hCyl-min(m_dh,0.0f);
				for(i=0;i<nents;i++) if (pentlist[i]->m_iSimClass==0) {//pentlist[i]->GetType()!=PE_LIVING && pentlist[i]->GetMassInv()*0.4f<m_massinv) {
					for(int j1=0;j1<pentlist[i]->GetUsedPartsCount(iCaller);j1++) 
						if (pentlist[i]->m_parts[j=pentlist[i]->GetUsedPart(iCaller,j1)].flags & collider_flags) {
							pentlist[i]->GetPartTransform(j, gwd[1].offset,gwd[1].R,gwd[1].scale, this);
							if(m_pHeadGeom->Intersect(pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom, gwd,gwd+1, &ip, pcontacts))
								tmin = min(tmin,(float)pcontacts[0].t);
						}
				}
				if (m_dh<ip.time_interval+min(m_dh,0.0f)-tmin || fabs_tpl(m_dhSpeed)+fabs_tpl(m_dhAcc)==0)
					m_dh = ip.time_interval+min(m_dh,0.0f)-tmin;
			}
		}
		
		coord_block coord;
		coord.pos = pos; coord.q = m_qrot;
		m_pNewCoords = &coord;
		coord_block_BBox partCoord;
		partCoord.pos = m_parts[0].pos; partCoord.q = m_parts[0].q;
		partCoord.scale = m_parts[0].scale; m_parts[0].pNewCoords = &partCoord;
		ComputeBBox(BBoxInner, update_part_bboxes & m_nParts-2>>31);
		int locked = m_pWorld->RepositionEntity(this,1,BBoxInner);
		pos = coord.pos;
		m_pNewCoords = (coord_block*)&m_pos;
		UpdatePosition(pos,BBoxInner, locked);
		bMoving = 1;
	} else if (!m_bActive) {
		m_pos = pos;
		if (m_velRequested.len2()>0) {
			m_pos += m_velRequested*time_interval;
			m_BBox[0] += m_velRequested*time_interval; m_BBox[1] += m_velRequested*time_interval;
			m_pWorld->RepositionEntity(this,1|8);
			bMoving = 1;
		}
		if (m_bReleaseGroundColliderWhenNotActive!=0)
			ReleaseGroundCollider();
	}
	
	{ WriteLockCond lock(m_lockLiving,m_bStateReading^1);
		//if (m_pWorld->m_vars.bMultiplayer)
		//	m_pos = CompressPos(m_pos);
		
		m_deltaV		= (m_vel - vel0);
		m_vel				= vel+m_vel-vel0;		
		m_bFlying		= bFlying;
		m_deltaQRot = m_qrot * !oldQRot;

		m_timeSmooth = (float)__fsel(-time_interval, m_timeSmooth, time_interval);
		if (m_pWorld->m_bUpdateOnlyFlagged) {
			m_deltaPos = m_posLocal-m_pos;
			if (m_deltaPos.len2()<sqr(0.01f) || m_deltaPos.len2()>sqr(2.0f))
				m_deltaPos.zero();
		}
		if (m_pBody)
			if (!m_nColliders) {
				delete m_pBody; m_pBody=0;
			} else {
				m_pBody->pos=m_pos+m_qrot*Vec3(0,0,m_hCyl); m_pBody->q=m_qrot;
				m_pBody->P=(m_pBody->v=m_vel)*(m_pBody->M=m_mass);
				m_pBody->Minv=m_massinv; 
				if (m_pWorld->m_timePhysics > m_timeRotChanged+0.05f)
					m_pBody->w.zero(), m_pBody->L.zero();
				/*quaternionf dq = m_history[m_iHist].q*!m_history[m_iHist-3&m_szHistory-1].q;
				float dt=0; for(i=0; i<4; i++)
					dt += m_history[m_iHist-i&m_szHistory-1].dt;
				if (inrange(dt, 0.0f,1.0f)) {
					if (dq.v.len2()<sqr(0.05f))
						m_pBody->w = dq.v*(2/dt);
					else
						m_pBody->w = dq.v.normalized()*(acos_tpl(dq.w)*2/dt);
				}*/
			}
	}

	if (!m_bStateReading) {
		if( bMoving )	{
			Vec3 gravity; MARK_UNUSED gravity;
			pe_params_buoyancy pb;
			m_pWorld->CheckAreas(this,gravity,&pb,0);
			if (!is_unused(gravity))
				m_gravity = gravity;

			if (m_pWorld->m_pWaterMan)
				m_pWorld->m_pWaterMan->OnWaterInteraction(this);
		}

		if (m_flags & (pef_monitor_poststep | pef_log_poststep)) {
			EventPhysPostStep epps; InitEvent(&epps,this,iCaller);
			epps.dt=time_interval; epps.pos=m_pos; epps.q=m_qrot; epps.idStep=m_pWorld->m_idStep;
			epps.pos -= m_qrot*Vec3(0,0,m_dh);
			m_pWorld->OnEvent(m_flags,&epps);
		}
		/*if (m_pWorld->m_iLastLogPump > m_timeLogged)
			m_posLogged = pos0;
		m_timeLogged = m_pWorld->m_iLastLogPump;*/
	}

	return 1;
}



float CLivingEntity::CalcEnergy(float time_interval)
{
	float E=m_mass*sqr(m_vel);
	if (m_pBody)
		E = max(E, m_mass*m_pBody->v.len2());
	for(int i=0;i<m_nContacts;i++) // account for extra energy we are going to add by enforcing penertation unprojections
		E += sqr(min(3.0f,(m_pContacts[i].penetration*10.0f)))*m_pContacts[i].pent->GetRigidBody(m_pContacts[i].ipart)->M;
	return E; 
}

int CLivingEntity::RegisterContacts(float time_interval,int nMaxPlaneContacts)
{
	int i,j;
	Vec3 pt[2],axis=m_qrot*Vec3(0,0,1);
	float h;
	entity_contact *pcontact;

	if (m_iSimClass!=7) for(i=0;i<m_nContacts;i++) {
		h = (m_pContacts[i].pt-m_pContacts[i].center)*axis;
		pt[0] = pt[1] = m_pContacts[i].pt;
		if (fabs_tpl(m_pContacts[i].n*axis)>0.7f && fabs_tpl(h)>m_size.z*0.9f) {	// contact with cap
			pt[1] += (m_pContacts[i].center-pt[0])*2;	pt[1].z = pt[0].z;
		} else { // contact with side
			pt[0] -= axis*(h-m_size.z); pt[1] -= axis*(h+m_size.z);
		} 

		for(j=0;j<2;j++) {
			if (!(pcontact=(entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact)))) 
				return 0;
			pcontact->flags = 0;
			pcontact->pent[0] = m_pContacts[i].pent;
			pcontact->ipart[0] = m_pContacts[i].ipart;
			pcontact->pbody[0] = m_pContacts[i].pent->GetRigidBody(m_pContacts[i].ipart);
			pcontact->pent[1] = this;
			pcontact->ipart[1] = 0;
			pcontact->pbody[1] = GetRigidBody();
			pcontact->friction = 0.5f;
			pcontact->vreq = m_pContacts[i].n*min(3.0f,(m_pContacts[i].penetration*10.0f));
			pcontact->pt[0] = pcontact->pt[1] = pt[j];
			pcontact->n = m_pContacts[i].n;
			//pcontact->K.SetZero();
			//m_pContacts[i].pent->GetContactMatrix(m_pContacts[i].pt, m_pContacts[i].ipart, pcontact->K);
			::RegisterContact(pcontact);
			m_pContacts[i].pSolverContact[j] = pcontact;
		}
		DisablePreCG();
	}

	if (m_pBody && (!m_bFlying || m_bSwimming) && (pcontact=(entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact)))) {
		pcontact->pent[0] = this;
		pcontact->ipart[0] = -1;
		pcontact->pbody[0] = m_pBody;
		if (m_pLastGroundCollider) {
			pcontact->pent[1] = m_pLastGroundCollider;
			pcontact->ipart[1] = m_iLastGroundColliderPart;
		} else {
			pcontact->pent[1] = &g_StaticPhysicalEntity;
			pcontact->ipart[1] = -1;
		}
		pcontact->pbody[1] = pcontact->pent[1]->GetRigidBody(pcontact->ipart[1]);
		pcontact->friction = m_velRequested.len2() || m_slopeClimb<=0 ? 0.0f : (1-sqr(m_slopeClimb))/m_slopeClimb;
		pcontact->pt[0]=pcontact->pt[1] = m_pos;
		pcontact->n = m_bFlying ? m_qrot*Vec3(0,0,1):m_nslope;
		//pcontact->K.SetZero();
		//pcontact->K(0,0)=pcontact->K(1,1)=pcontact->K(2,2) = m_massinv;
		//pcontact->pent[1]->GetContactMatrix(pcontact->pt[1], pcontact->ipart[1], pcontact->K);
		::RegisterContact(pcontact);
		DisablePreCG();
	}

	return 1;
}

int CLivingEntity::Update(float time_interval, float damping)
{
	int i,j;
	for(i=0;i<m_nContacts;i++) if (m_pContacts[i].pSolverContact[0])
		RegisterContact(m_pos,m_pContacts[i].pt,m_pContacts[i].n, m_pContacts[i].pent,m_pContacts[i].ipart, m_surface_idx,
			m_pContacts[i].pSolverContact[0]->Pspare+m_pContacts[i].pSolverContact[1]->Pspare);
	for(i=j=0;i<m_nColliders;i++) if (!m_pColliders[i]->HasConstraintContactsWith(this)) {
		m_pColliders[i]->RemoveColliderMono(this);
		m_pColliders[i]->Release();
	}	else
		m_pColliders[j++] = m_pColliders[i];
	m_nColliders = j;
	m_nContacts = 0;	
	if (m_pBody) {
		const Vec3 v = m_pBody->v * damping;
		const Vec3 w = m_pBody->w * damping;
		m_pBody->v = v;
		m_pBody->w = w;
		m_vel = v;
		m_bJumpRequested |= isneg(-v.z);

		if ((m_pBody->Ibody*w).len2()>0) {
			m_qrot *= Quat::CreateRotationAA(w.len()*time_interval, Vec3(0,0,sgnnz((w*m_qrot).z)));
			m_qrot.Normalize();
			m_pBody->w.zero(); m_pBody->L.zero();
		}
	}
	return 1;
}


RigidBody *CLivingEntity::GetRigidBody(int ipart,int bWillModify)
{
	if (!bWillModify)
		return CPhysicalEntity::GetRigidBody(ipart);
	else {
		if (!m_pBody) {
			m_pBody = new RigidBody;
			m_pBody->Create(m_pos,Vec3(1),m_qrot,m_size.GetVolume()*8+gf_PI*(4.0f/3)*cube(m_size.x)*m_bUseCapsule,m_mass,m_qrot,m_pos);
		}
		Vec3 axis = m_qrot*Vec3(0,0,1);
		m_pBody->pos=m_pos+axis*m_hCyl; m_pBody->q=m_qrot;
		m_pBody->P=(m_pBody->v=m_vel)*(m_pBody->M=m_mass);
		m_pBody->Minv=m_massinv; 
		m_pBody->Ibody_inv.zero();
		m_pBody->Iinv.SetZero(); 
		m_pBody->Ibody.zero();
		//m_pBody->Ibody_inv.z = m_massinv/(g_PI*m_size.z*sqr(sqr(m_size.x)));
		//dotproduct_matrix(axis,axis*m_pBody->Ibody_inv.z, m_pBody->Iinv);
		//m_pBody->w.zero(); m_pBody->L.zero();
		if (m_kInertia==0)
			m_timeForceInertia = m_timeImpulseRecover;
		return m_pBody;
	}
}


void CLivingEntity::OnContactResolved(entity_contact *pContact, int iop, int iGroupId)
{
	if (!(pContact->flags & contact_angular)) {
		Vec3 n;
		if (pContact->flags & contact_rope_stretchy)
			return;
		if (!(pContact->flags & contact_rope) || iop==0)
			n = pContact->n;
		else
			n = iop==1 ? pContact->vreq : ((rope_solver_vtx*)pContact->pBounceCount)[iop-2].v.normalized();
		m_velRequested -= n*min(0.0f,m_velRequested*n);
		m_vel -= n*min(0.0f,m_vel*n);
	}
}


void CLivingEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	if (m_bActive || m_nParts>0 && m_parts[0].flags & collider_flags) {
		CPhysicalEntity::DrawHelperInformation(pRenderer, flags);

		geom_world_data gwd;
		gwd.R = Matrix33(m_qrot);
		gwd.offset = m_pos + gwd.R*m_parts[0].pos;
		if (m_pHeadGeom) {
			gwd.offset += gwd.R*Vec3(0,0,m_hHead-m_hCyl-m_dh);
			m_pHeadGeom->DrawWireframe(pRenderer,&gwd,0,m_iSimClass);
		}
	}
}


void CLivingEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_LIVING)
		pSizer->AddObject(this, sizeof(CLivingEntity));
	CPhysicalEntity::GetMemoryStatistics(pSizer);
}

void CLivingEntity::Step_HandleFlying(Vec3 &vel, const Vec3& velGround, int bWasFlying, const Vec3& heightAdj, const float kInertia, const float time_interval)
{
	Vec3 last_force, gravity, gravityAdjusted;
	if (!bWasFlying)
		vel += velGround;
	if (!m_pWorld->m_vars.bFlyMode) {
		gravity = m_gravity * time_interval;
		if ((gravity*heightAdj)<0.f) { // Remove any slope adjustment from the gravity accumulation
			gravityAdjusted = gravity + heightAdj/time_interval;
			if (gravityAdjusted*gravity>0.f)
				vel += gravityAdjusted;
		} else
			vel += gravity;
		if (m_bSwimming) 
			vel += (m_velRequested-vel)*kInertia*time_interval;
		/*if ((vel+last_force*time_interval)*vel<0)
			vel.zero();
		else*/
		last_force = -vel*m_kAirResistance*time_interval;
		if (last_force.len2()<vel.len2()*4.0f)
			vel += last_force;
	}
	if (vel.len2()<1E-10f) vel.zero();
	ReleaseGroundCollider();
}

void CLivingEntity::Step_HandleWasFlying(Vec3& vel, int& bFlying, const Vec3& axis, const int bGroundContact)
{
	if (!m_bSwimming) {
		if (vel*axis<-4.0f && !m_bStateReading && (m_nodSpeed > 0.0f)) {
			m_dhSpeed = vel*axis;
			m_dhAcc = max(sqr(m_dhSpeed)/(m_size.z*2), m_nodSpeed);
		}
		vel -= m_velGround;
		if (m_nslope*axis<m_slopeFall && (vel*axis<-4) && !m_bStuck){
			vel -= m_nslope*(vel*m_nslope)*1.0f;
			if (vel*axis>0) vel -= axis*(axis*vel);
			bFlying = 1;
		} else if (m_nslope*axis<m_slopeFall || (!bGroundContact || m_velRequested.len2()>0) && (m_nslope*axis>m_slopeClimb || vel*axis<0))
			vel -= m_nslope*(vel*m_nslope);
		else
			vel.zero();
	}
}
