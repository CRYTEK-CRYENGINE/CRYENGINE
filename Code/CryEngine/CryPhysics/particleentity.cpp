// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "singleboxtree.h"
#include "raybv.h"
#include "raygeom.h"
#include "intersectionchecks.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "waterman.h"
#include "particleentity.h"

CParticleEntity *CParticleEntity::g_pCurParticle[MAX_PHYS_THREADS+1] = { 0 };


CParticleEntity::CParticleEntity(CPhysicalWorld *pWorld, IGeneralMemoryHeap* pHeap)
	: CPhysicalEntity(pWorld, pHeap)
	, m_mass(0.2f)
	, m_dim(0.05f)
	, m_rdim(20.0f)
	, m_dimLying(0.05f)
	, m_kAirResistance(0.0f)
	, m_kWaterResistance(0.5f)
	, m_accThrust(0.0f)
	, m_kAccLift(0.0f)
	, m_gravity(0.0f, 0.0f, -9.81f)
	, m_gravity0(0.0f, 0.0f, -9.81f)
	, m_waterGravity(Vec3(0.0f, 0.0f, -9.81f) * 0.8f)
	, m_rollax(0.0f, 0.0f, 1.0f)
	, m_normal(0.0f, 0.0f, 1.0f)
	, m_heading(1.0f, 0.0f, 0.0f)
	, m_vel(ZERO)
	, m_wspin(ZERO)
	, m_qspin(IDENTITY)
	, m_minBounceVel(1.5f)
	, m_minVel(0.02f)
	, m_surface_idx(0)
	, m_pColliderToIgnore(nullptr)
	, m_collTypes(ent_all)
	, m_slide_normal(0.0f, 0.0f, 1.0f)
	, m_timeSurplus(0.0f)
	, m_depth(0.0f)
	, m_velMedium(ZERO)
	, m_timeStepPerformed(0.0f)
	, m_timeStepFull(0.0f)
	, m_timeForceAwake(0.0f)
	, m_sleepTime(0.0f)
	, m_areaCheckPeriod(6)
	, m_nStepCount(0)
	, m_bHadCollisions(0)
	, m_bRecentCollisions(0)
	, m_bForceAwake(2)
	, m_iPierceability(sf_max_pierceable)
	, m_bSliding(0)
	, m_bDontPlayHitEffect(0)
	, m_lockParticle(0)
{
	if (pWorld)
	{
		m_gravity0 = m_gravity = pWorld->m_vars.gravity;
		m_waterGravity = m_gravity*0.8f;
	}	
	
	//CPhysicalEntity
	m_flags = 0;

	m_defpart.flags = 0;
	m_defpart.id = 0;
	m_defpart.pos.zero();
	m_defpart.q.SetIdentity();
	m_defpart.scale = 1.0f;
	m_defpart.mass = 0;
	m_defpart.minContactDist = 0;

	m_collisionClass.type |= collision_class_particle;

	//CPhysicalPlaceholder
	m_iSimClass = 4;
	m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = NO_GRID_REG;
}

CParticleEntity::~CParticleEntity()
{
	if (m_nParts>0 && m_pWorld)
		m_pWorld->GetGeomManager()->UnregisterGeometry(m_parts[0].pPhysGeom);
	if (m_pColliderToIgnore && m_pWorld && !m_pWorld->m_bMassDestruction)
		m_pColliderToIgnore->Release();
	m_nParts = 0;
}


int CParticleEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1;

	int res;
	if (res = CPhysicalEntity::SetParams(_params,1)) {
		if (_params->type==pe_params_flags::type_id) {
			pe_params_particle pp;
			SetParams(&pp,1);
		}
		return res;
	}

	if (_params->type==pe_params_particle::type_id) {
		pe_params_particle *params = (pe_params_particle*)_params;
		ENTITY_VALIDATE("CParticleEntity:SetParams(pe_params_particle)",params);
		WriteLock lock(m_lockParticle);
		if (!is_unused(params->mass)) m_mass = params->mass; 
		if (!is_unused(params->size)) {
			m_rdim = 1.0f/(m_dim = params->size*0.5f);
			m_dimLying = params->size*0.5f;
		}
		if (!is_unused(params->thickness)) m_dimLying = params->thickness*0.5f;
		if (!is_unused(params->kAirResistance)) m_kAirResistance = params->kAirResistance;
		if (!is_unused(params->kWaterResistance)) m_kWaterResistance = params->kWaterResistance;
		if (!is_unused(params->accThrust)) m_accThrust = params->accThrust;
		if (!is_unused(params->accLift) && !is_unused(params->velocity))
			m_kAccLift = params->velocity!=0 ? params->accLift/fabs_tpl(params->velocity):0;
		if (!is_unused(params->heading)) m_heading = params->heading;
		if (!is_unused(params->velocity)) { m_vel = m_heading*params->velocity; if (m_vel.len2()>0) m_bForceAwake=1; }
		if (!is_unused(params->wspin)) m_wspin = params->wspin;
		if (!is_unused(params->gravity)) m_gravity0 = m_gravity = params->gravity;
		if (!is_unused(params->waterGravity)) m_waterGravity = params->waterGravity;
		if (!is_unused(params->normal)) m_normal = params->normal;
		if (!is_unused(params->rollAxis)) m_rollax = params->rollAxis;
		if (!is_unused(params->q0)) m_qspin = params->q0;
		if (!is_unused(params->minBounceVel)) m_minBounceVel = params->minBounceVel;
		if (!is_unused(params->minVel)) m_minVel = params->minVel;
		if (!is_unused(params->surface_idx)) m_surface_idx = params->surface_idx;
		if (!is_unused(params->flags)) m_flags = params->flags;
		if (!is_unused(params->dontPlayHitEffect)) m_bDontPlayHitEffect = params->dontPlayHitEffect;

		m_bSliding = 0;

		if (m_flags & particle_traceable) {
			if (m_ig[0].x==NO_GRID_REG)
				m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = GRID_REG_PENDING;
			if (m_pos.len2()>0)
				m_pWorld->RepositionEntity(this,1|8);
		}	else {
			if (m_ig[0].x!=NO_GRID_REG) {
        WriteLock __lock(m_pWorld->m_lockGrid);
				m_pWorld->DetachEntityGridThunks(this);
				m_ig[0].x=m_ig[1].x=m_ig[0].y=m_ig[1].y = NO_GRID_REG;
			}
		}
		
		if (!is_unused(params->pColliderToIgnore)) {
			if (m_pColliderToIgnore)
				m_pColliderToIgnore->Release();
			if (m_pColliderToIgnore = (CPhysicalEntity*)params->pColliderToIgnore)
				m_pColliderToIgnore->AddRef();
		}
		if (!is_unused(params->iPierceability)) m_iPierceability = params->iPierceability;
		if (!is_unused(params->collTypes)) m_collTypes = params->collTypes;
		if (!is_unused(params->areaCheckPeriod)) {
			m_areaCheckPeriod = params->areaCheckPeriod; m_nStepCount = 0;
		}

		if (!(m_flags & particle_constant_orientation)) {
			if (!(m_flags & particle_no_path_alignment)) {
				Vec3 dirbuf[3]; 
				dirbuf[0] = m_gravity.normalized()^m_heading; dirbuf[1] = m_heading;
			//	if (dirbuf[0].len2()<0.01f) dirbuf[0] = m_heading.orthogonal();
				if (dirbuf[0].len2()<0.01f) dirbuf[0].SetOrthogonal(m_heading);
				dirbuf[0].normalize(); dirbuf[2] = dirbuf[0]^dirbuf[1];
				m_qrot = quaternionf(GetMtxFromBasisT(dirbuf))*m_qspin;
			} else
				m_qrot = m_qspin;
		}

		{ WriteLock lock1(m_lockUpdate);
			m_BBox[0] = m_pos-Vec3(m_dim,m_dim,m_dim);
			m_BBox[1] = m_pos+Vec3(m_dim,m_dim,m_dim);
		}
		return 1;
	}

	return 0;
}


int CParticleEntity::GetParams(pe_params *_params) const
{
	if (CPhysicalEntity::GetParams(_params))
		return 1;

	if (_params->type==pe_params_particle::type_id) {
		pe_params_particle *params = (pe_params_particle*)_params;
		ReadLock lock(m_lockParticle);
		params->mass = m_mass; 
		params->size = m_dim*2.0f;
		params->thickness = m_dimLying*2.0f;
		params->heading = m_heading;
		params->velocity = m_vel.len();
		params->wspin = m_wspin;
		params->gravity = m_gravity;
		params->normal = m_normal;
		params->rollAxis = m_rollax;
		params->kAirResistance = m_kAirResistance;
		params->accThrust = m_accThrust;
		params->accLift = params->velocity*m_kAccLift;
		params->q0 = m_qspin;
		params->surface_idx = m_surface_idx;
		params->flags = m_flags;
		params->pColliderToIgnore = m_pColliderToIgnore;
		params->iPierceability = m_iPierceability;
		params->collTypes = m_collTypes;
		params->minBounceVel = m_minBounceVel;
		params->minVel = m_minVel;
		params->dontPlayHitEffect = m_bDontPlayHitEffect;
		return 1;
	}

	return 0;
}


int CParticleEntity::GetStateSnapshot(CStream &stm, float time_back, int flags)
{
	ReadLock lock0(m_lockUpdate),lock1(m_lockParticle);
	stm.WriteNumberInBits(SNAPSHOT_VERSION, 4);
	if (m_pWorld->m_vars.bMultiplayer) {
		if (!IsAwake()) {
			if (m_sleepTime>5.0f)
				stm.Write(false);
			else {
				stm.Write(true);
				stm.Write(m_pos);
				stm.Write(false);
			}
		} else {
			stm.Write(true); stm.Write(m_pos);
			stm.Write(true);
			stm.Write(m_vel);
			if (!m_bSliding) stm.Write(false);
			else {
				stm.Write(true);
				stm.Write(asin_tpl(m_slide_normal.z));
				stm.Write(atan2_tpl(m_slide_normal.y,m_slide_normal.x));
			}
		}
	} else {
		stm.Write(m_pos);
		stm.Write(m_vel);
		if (!m_bSliding) stm.Write(false);
		else {
			stm.Write(true);
			stm.Write(asin_tpl(m_slide_normal.z));
			stm.Write(atan2_tpl(m_slide_normal.y,m_slide_normal.x));
		}
	}
	/*if (m_qspin.v.len2()<0.01*0.01) stm.Write(false);
	else {
		stm.Write(true); 
		//CHANGED_BY_IVO (NOTE: order of angles is flipped!!!!)
		//float angles[3]; m_qspin.get_Euler_angles_xyz(angles[0],angles[1],angles[2]);
		//EULER_IVO
		//Vec3 TempAng; m_qspin.GetEulerAngles_XYZ( TempAng );	
		Vec3 angles = Ang3::GetAnglesXYZ(Matrix33(m_qspin));	
		for(int i=0;i<3;i++) stm.Write((unsigned short)float2int((angles[i]+pi)*(65535.0/2/pi)));
	}
	if (m_wspin.len2()==0) stm.Write(false);
	else {
		stm.Write(true); stm.Write(m_wspin);
	}*/

	return 1;
}

int CParticleEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	bool bnz;	int ver=0;
	stm.ReadNumberInBits(ver,4);
	if (ver!=SNAPSHOT_VERSION)
		return 0;

	WriteLock lock0(m_lockUpdate),lock1(m_lockParticle);
	if (!(flags & ssf_no_update)) {
		if (m_pWorld->m_vars.bMultiplayer) {
			stm.Read(bnz); if (bnz) {
				stm.Read(m_pos);
				stm.Read(bnz); if (bnz) {
					stm.Read(m_vel);
					stm.Read(bnz); if (bnz) {
						m_bSliding = 1;
						float yaw,pitch;
						stm.Read(pitch); stm.Read(yaw);
						m_slide_normal(cos_tpl(yaw)*cos_tpl(pitch),sin_tpl(yaw)*cos_tpl(pitch),sin_tpl(pitch));
					}	else m_bSliding = 0;
				}
			}
		}	else {
			stm.Read(m_pos);
			stm.Read(m_vel);
			stm.Read(bnz); if (bnz) {
				m_bSliding = 1;
				float yaw,pitch;
				stm.Read(pitch); stm.Read(yaw);
				m_slide_normal(cos_tpl(yaw)*cos_tpl(pitch),sin_tpl(yaw)*cos_tpl(pitch),sin_tpl(pitch));
			}	else m_bSliding = 0;
		}
		if (m_bForceAwake!=0)
			m_bForceAwake = 2;
	}	else {
		if (m_pWorld->m_vars.bMultiplayer) {
			stm.Read(bnz); if (bnz) {
				stm.Seek(stm.GetReadPos()+sizeof(Vec3)*8);
				stm.Read(bnz); if (bnz) {
					stm.Seek(stm.GetReadPos()+sizeof(Vec3)*8);
					stm.Read(bnz); if (bnz)
						stm.Seek(stm.GetReadPos()+2*sizeof(float)*8);
				}
			}
		} else {
			stm.Seek(stm.GetReadPos()+2*sizeof(Vec3)*8);
			stm.Read(bnz); if (bnz)
				stm.Seek(stm.GetReadPos()+2*sizeof(float)*8);
		}
	}
	/*stm.Read(bnz); if (bnz) {
		unsigned short tmp; int i; Vec3 axis(zero);
		for(i=0,m_qspin.SetIdentity(); i<3; i++) {
			axis[i] = 1;
			//stm.Read(tmp); m_qspin*=quaternionf(tmp*(2*pi/65535.0)-pi, axis);
			stm.Read(tmp); m_qspin = GetRotationAA((float)(tmp*(2*pi/65535.0)-pi), axis)*m_qspin;
			axis[i] = 0;
		}
	}
	stm.Read(bnz); if (bnz)
		stm.Read(m_wspin);
	else m_wspin.zero();*/

	return 1;
}


int CParticleEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	if (ser.GetSerializationTarget()==eST_Network)
	{
		if (ser.BeginOptionalGroup("ifawake", IsAwake()!=0))
		{
			ser.Value("vel", m_vel, 'pPVl');
			ser.EndGroup();
		}
		ser.Value("pos", m_pos, 'wrl3');
	}
	else
	{
		ser.BeginGroup("ParticleEntity");
		CPhysicalEntity::GetStateSnapshot(ser, time_back, flags);
		ser.Value("vel", m_vel);
		ser.EndGroup();
	}

	return 1;
}

int CParticleEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	if (ser.GetSerializationTarget()==eST_Network)
	{
		if (ser.BeginOptionalGroup("ifawake", IsAwake()!=0))
		{
			ser.Value("vel", m_vel, 'pPVl');
			ser.EndGroup();
			m_heading=m_vel.normalized();
		}

		ser.Value("pos", m_pos, 'wrl3');
	}
	else
	{
		ser.BeginGroup("ParticleEntity");
		CPhysicalEntity::GetStateSnapshot(ser, flags);
		ser.Value("vel", m_vel);
		ser.EndGroup();
	}

	return 1;
}


int CParticleEntity::GetStatus(pe_status* _status) const
{
	if (CPhysicalEntity::GetStatus(_status))
		return 1;

	if (_status->type==pe_status_collisions::type_id)
		return m_bHadCollisions ? m_bHadCollisions-- : 0;

	if (_status->type==pe_status_dynamics::type_id) {
		pe_status_dynamics *status = (pe_status_dynamics*)_status;
		ReadLock lock(m_lockParticle);
		Vec3 gravity; float kAirResistance;
		if (m_depth<0) {
			gravity = m_waterGravity; kAirResistance = m_kWaterResistance;
		} else {
			gravity = m_gravity; kAirResistance = m_kAirResistance;
		}
		status->v = m_vel;
		status->a = gravity+m_heading*m_accThrust-m_vel*kAirResistance+(m_heading^(m_heading^m_gravity)).normalize()*m_kAccLift*m_vel.len();
		status->w = m_wspin;
		status->centerOfMass = m_pos;
		status->mass = m_mass;
		return 1;
	}

	return 0;
}


int CParticleEntity::Action(pe_action* _action, int bThreadSafe)
{
	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		Vec3 P=action->impulse, L(ZERO);
		if (!is_unused(action->angImpulse))
			L = action->angImpulse;
		else if (!is_unused(action->point))
			L = action->point-m_pos^action->impulse;
		if (m_mass > 0)
			m_vel += P/m_mass;
		if (m_mass*m_dim>0 && !(m_flags & particle_constant_orientation))
			m_wspin += L/(0.4f*m_mass*m_dim);
		return 1;
	}

	if (_action->type==pe_action_set_velocity::type_id) {
		pe_action_set_velocity *action = (pe_action_set_velocity *)_action;
		if (!is_unused(action->v)) m_heading = (m_vel = action->v).normalized();
		if (!is_unused(action->w)) m_wspin = action->w;
		return 1;
	}

	if (_action->type==pe_action_reset::type_id) {
		m_vel.zero(); m_wspin.zero();
		return 1;
	}

	return CPhysicalEntity::Action(_action,1);
}

int CParticleEntity::Awake(int bAwake,int iSource) 
{ 
	if (bAwake)
		m_bForceAwake = 1; 
	else {
		m_bForceAwake = 0;
		m_vel.zero();
	}
	return m_iSimClass;
}

int CParticleEntity::IsAwake(int ipart) const
{
	ReadLock lock(m_lockParticle);
	if (!m_pWorld)
		return false;
	Vec3 gravity = m_depth<0 ? m_waterGravity : m_gravity;
	return m_pos.z>-1000.0f && m_bForceAwake!=0 && 
				 (m_bForceAwake==1 || (m_vel.len2()>sqr(m_minVel) || 
				  !m_bRecentCollisions && !m_bSliding && gravity.len2()>0));
}


void CParticleEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
}
float CParticleEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull-0.001f)
		return time_interval;
	return min_safe(m_timeStepFull-m_timeStepPerformed,time_interval);
}

/*int CheckPointInside(const Vec3& pt)
{
	int i,j,nents;
	CPhysicalEntity **pents;
	nents = g_pPhysWorlds[0]->GetEntitiesAround(pt,pt,pents,ent_static|ent_rigid|ent_sleeping_rigid|ent_rigid);
	for(i=0;i<nents;i++) for(j=0;j<pents[i]->m_nParts;j++) 
		if (pents[i]->m_parts[j].flags & geom_colltype0 && 
				pents[i]->m_parts[j].pPhysGeomProxy->pGeom->PointInsideStatus(
				 (((pt-pents[i]->m_pos)*pents[i]->m_qrot-pents[i]->m_parts[j].pos)*pents[i]->m_parts[j].q)/pents[i]->m_parts[j].scale))
		return 1;
	return 0;
}

int g_retest=0;*/

int CParticleEntity::DoStep(float time_interval, int iCaller)
{
	static ray_hit _hits[MAX_PHYS_THREADS+1][8];
  ray_hit (&hits)[8] = _hits[get_iCaller()];
	pe_action_impulse ai;
	//pe_action_register_coll_event arce;
	pe_status_dynamics sd;
	Vec3 pos,vel,heading,slide_normal,pos0,vel0,heading0,vtang,vel_next,gravity;
	float vn,vtang_len,rvtang_len,e,k,friction,kAirResistance;
	IPhysicalEntity *pIgnoredColliders[2] = { m_pColliderToIgnore,this };
	quaternionf qrot = m_qrot;
	int i,j,nhits,bHit,flags,bHasIgnore=(m_pColliderToIgnore!=0);
	IPhysicalWorld::SRWIParams rp;
	IPhysicalEntity *portals[16];
	EventPhysCollision event;
	event.penetration=event.radius = 0;
	portals[0] = nullptr;

	if (m_depth<0) {
		gravity = m_waterGravity*min(1.0f,-m_depth*m_rdim); kAirResistance = m_kWaterResistance;
	} else {
		gravity = m_gravity; kAirResistance = m_kAirResistance;
	}
	if (min(m_timeStepPerformed-m_timeStepFull+0.0001f,m_timeStepFull)>0)
		time_interval = 0.0001f;
	m_timeStepPerformed += time_interval;

	if (m_pColliderToIgnore && m_pColliderToIgnore->m_iSimClass==7)
		m_pColliderToIgnore = 0;

  if (m_timeSurplus==0)
    m_timeSurplus=1;

	if (IsAwake()) {
		CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
		PHYS_ENTITY_PROFILER
		g_pCurParticle[iCaller] = this;

		pos=pos0 = m_pos; vel=vel0 = m_vel;
		heading = m_heading; slide_normal = m_slide_normal;
		if (!m_bSliding)
			vel0 += gravity*time_interval*0.5f;
		flags = m_flags;
		pos += vel0*time_interval;
		m_bRecentCollisions = max(0,(int)m_bRecentCollisions-1);

		if (m_bSliding && m_iPierceability<=sf_max_pierceable) {
			if (m_pWorld->RayWorldIntersection(
				rp.Init(pos,slide_normal*(m_dim*-1.1f), m_collTypes, m_iPierceability|geom_colltype_ray<<rwi_colltype_bit, m_collisionClass, hits,1, pIgnoredColliders+1-bHasIgnore,1+bHasIgnore),
				"RayWorldIntersection(PhysParticles)", iCaller)) 
			{
				slide_normal = hits[0].n;	
				pos = hits[0].pt+slide_normal*m_dimLying;
				//if (m_dimLying!=m_dim)
				//	pos0 = m_pos;
				if (((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass>1) {
					sd.ipart = hits[0].ipart;
					if (hits[0].pCollider->GetStatus(&sd))
						vel0 -= (sd.v+=(sd.w^pos-sd.centerOfMass));
				} else sd.v.zero();	
				if (m_flags&particle_no_roll || slide_normal.z<0.5f) { // always slide if the slope is steep enough
					friction = m_pWorld->GetFriction(m_surface_idx, hits[0].surface_idx); 
					if (slide_normal.z<0.5f)
						friction = min(1.0f,friction); // limit sliding friction on slopes
					vn = hits[0].n*vel0; vtang = vel0-hits[0].n*vn; vtang_len = vtang.len(); rvtang_len = vtang_len>1e-4 ? 1.0f/vtang_len:0;
					vel = vel0 = sd.v + hits[0].n*max(0.0f,vn) + 
						vtang*(max(0.0f,vtang_len-max(0.0f,-(vn+(m_gravity*hits[0].n)*time_interval))*friction)*rvtang_len);
					m_wspin.zero(); m_qspin.SetIdentity();
					if (!(m_flags & particle_constant_orientation) && m_normal.len2()>0.0f)
						(qrot = Quat::CreateRotationV0V1(qrot*m_normal,hits[0].n)*qrot).Normalize();
					flags |= particle_constant_orientation;
				} else {
					friction = m_pWorld->m_FrictionTable[m_surface_idx&NSURFACETYPES-1];
					vel0 = vel = sd.v+(vel0-slide_normal*(vel0*slide_normal))*max(0.0f,1.0f-time_interval*friction);
					m_wspin = slide_normal^(vel-sd.v)*m_rdim;
					if (m_rollax.len2()>0.0f && m_wspin.len2()>1e-20f)
						(m_qspin = Quat::CreateRotationV0V1(m_qspin*m_rollax,m_wspin.normalized())*m_qspin).Normalize();
				}
				if (m_flags & particle_single_contact)
					gravity.zero();
				else
					gravity -= slide_normal*(slide_normal*gravity);
				m_bForceAwake = ((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass<=2 || m_timeForceAwake>40.0f ? 2:1;
			} else {
				m_bSliding = 0;
				if (!(m_flags & particle_constant_orientation)) {
					const Vec3 wspin = heading^m_gravity; const float len = wspin.len();
					m_wspin = (len>0.01f) ? wspin*(0.5f*m_rdim*m_gravity.len()/len) : Vec3(0,0,0);
				}
			}
		}
		
		vel += (gravity + heading*m_accThrust + (m_velMedium-vel)*kAirResistance +
			(heading^(heading^m_gravity)).normalize()*(m_kAccLift*vel.len()))*time_interval;
		(heading=vel).normalize();

		if (!(flags & particle_constant_orientation)) {
			if (!(m_flags & particle_no_spin)) {
				if (m_wspin.len2()*sqr(time_interval)<0.1f*0.1f) {
					m_qspin.w	-= (m_wspin*m_qspin.v)*time_interval*0.5f;
					m_qspin.v += ((m_wspin^m_qspin.v)+m_wspin*m_qspin.w)*(time_interval*0.5f);
					// m_qspin += quaternionf(0,m_wspin*0.5f)*m_qspin*time_interval;
				}	else {
					float wlen = m_wspin.len();
					m_qspin = Quat::CreateRotationAA(wlen*time_interval,m_wspin/wlen)*m_qspin;
				}
				m_qspin.Normalize();
			} else
				m_wspin.zero();
			if (!(m_flags & particle_no_path_alignment) && !m_bSliding) {
				Vec3 dirbuf[3]; 
				dirbuf[0] = m_gravity^m_heading; dirbuf[1] = m_heading;
				if (dirbuf[0].len2()<0.01f) dirbuf[0].SetOrthogonal(m_heading);
				dirbuf[0].normalize(); dirbuf[2] = dirbuf[0]^dirbuf[1];
				qrot = quaternionf(GetMtxFromBasisT(dirbuf))*m_qspin;
			} else
				qrot = m_qspin;
		}

		bHit = nhits = 0;
		if (iCaller<MAX_PHYS_THREADS && m_pWorld->m_bWorldStep==2 && m_iPierceability<=sf_max_pierceable) {
			CPhysicalEntity **pentlist;
			pe_status_pos sp; sp.timeBack = 1;//time_interval;
			sp.pGridRefEnt = this;
			Vec3 posFixed;
			int nents = m_pWorld->GetEntitiesAround(pos0-Vec3(m_dim,m_dim,m_dim),pos0+Vec3(m_dim,m_dim,m_dim),pentlist,ent_rigid,0,0,iCaller);

			hits[0].dist = hits[1].dist = 1E10f;
			for(i=0;i<nents;i++) if (pentlist[i]!=m_pColliderToIgnore) {
				pentlist[i]->GetStatus(&sp);
				posFixed = (pentlist[i]->m_qrot*!sp.q)*(pos0-sp.pos)+pentlist[i]->m_pos;
				if ((m_pWorld->RayTraceEntity(pentlist[i],posFixed,pos0-posFixed+(pos0-posFixed).normalized()*(m_dim*0.97f),hits+1) && 
					pentlist[i]->m_parts[hits[1].ipart].flags&geom_colltype_ray && hits[1].dist<hits[0].dist))
				{
					hits[0] = hits[1];
					bHit = 1;
				}
			}
			if (bHit)	{ // ignore collisions with moving bodies if they push us through statics
				heading0 = (hits[0].pt-pos0).normalized();
				bHit ^= m_pWorld->RayWorldIntersection(
					rp.Init(pos0,hits[0].pt-pos0+heading0*m_dim,ent_terrain|ent_static,
						m_iPierceability|geom_colltype_ray<<rwi_colltype_bit|rwi_ignore_back_faces, m_collisionClass,
						hits+2,1, pIgnoredColliders+1-bHasIgnore,1+bHasIgnore),
					"RayWorldIntersection(PhysParticles)", iCaller);
			}
			if (nhits = bHit) {
				heading0=(pos0-posFixed).normalized(); pos0=posFixed; 
			}
		}

		if (!bHit && m_iPierceability<=sf_max_pierceable) {
			heading0 = (pos-pos0).normalized();
			nhits = m_pWorld->RayWorldIntersection(
				rp.Init(pos0,pos-pos0+heading0*m_dim, m_collTypes|ent_water,
					m_iPierceability|(geom_colltype_ray|geom_colltype13)<<rwi_colltype_bit|rwi_colltype_any|
					rwi_force_pierceable_noncoll|rwi_ignore_solid_back_faces|rwi_separate_important_hits, m_collisionClass, hits,8, 
					pIgnoredColliders+1-bHasIgnore,1+bHasIgnore, 0,0,0, portals,CRY_ARRAY_COUNT(portals)-1),
				"RayWorldIntersection(PhysParticles)", iCaller);
			bHit = isneg(-nhits) & (isneg(hits[0].dist+0.5f)^1);
			/*if (bHit && hits[0].n*(pos-pos0)>0)	{
				pos = hits[0].pt + heading0*m_pWorld->m_vars.maxContactGap;
				bHit=0; nhits--;
			}*/
		}

		event.pEntity[0] = this; event.pForeignData[0] = m_pForeignData; event.iForeignData[0] = m_iForeignData;
		for(i=0; i<nhits; i++) {	// register all hits in history 
			j = i+1 & i-(nhits-bHit)>>31;
			event.pt = hits[j].pt;//-heading0*m_dim;	// store not contact, but position of particle center at the time of contact
			event.n = hits[j].n;									// it's better for explosions to be created at some distance from the wall
			event.normImpulse = 0;
			event.partid[1] = hits[j].partid;
			if (hits[j].pCollider && ((CPhysicalPlaceholder*)hits[j].pCollider)->m_iSimClass!=5) {
				CPhysicalEntity *pCollider = (CPhysicalEntity*)hits[j].pCollider;
				event.pEntity[1] = hits[j].pCollider;
				event.pForeignData[1] = pCollider->m_pForeignData; 
				event.iForeignData[1] = pCollider->m_iForeignData;
				RigidBody body(false), *pbody = pCollider->GetRigidBodyTrans(&body,hits[j].ipart,this);
				event.vloc[1] = pbody->v+(pbody->w^event.pt-pbody->pos);
				event.mass[1] = pbody->M;
				event.idCollider = m_pWorld->GetPhysicalEntityId(hits[j].pCollider);
				int ipart = max(0,min(hits[j].ipart, pCollider->m_nParts-1));
				event.partid[1] += hits[j].iNode-event.partid[1] & pCollider->m_nParts-1>>31; // return iNode for partless entitis (ropes, cloth)
				event.iPrim[1] = max(0,hits[j].iPrim);

				if (pCollider->GetType()!=PE_PARTICLE && 
						(pCollider->m_iSimClass>0 || pCollider->m_parts[ipart].flags & geom_monitor_contacts) && 
						!(pCollider->m_parts[ipart].flagsCollider & geom_no_particle_impulse)) 
				{	Vec3 vrel = vel0-event.vloc[1];
					float M = max(pbody->M, pCollider->GetMass(hits[j].ipart)), kmul=1.0f;
					M += (m_mass*100.0f)*isneg(M-1e-10f);
					if (j>0)
						kmul = 0.5f*(1-(1.0f/15)*(int)((m_pWorld->m_SurfaceFlagsTable[min(NSURFACETYPES-1,max(0,(int)hits[j].surface_idx))] & sf_pierceable_mask)-m_iPierceability));
					if ((vrel*hits[j].n)*M*kmul<0 && vel0.len2()>sqr(m_minBounceVel)) 
						if (!(m_flags & particle_no_impulse)) {
							ai.point = hits[j].pt;
							ai.impulse = vrel*(m_mass*M*kmul/(m_mass+M));
							ai.ipart = hits[j].ipart;
							pCollider->Action(&ai,1);
						} else
							event.normImpulse = m_mass*M*kmul/(m_mass+M);
				}
			}	else {
				event.pEntity[1] = hits[j].pCollider ? hits[j].pCollider:&g_StaticPhysicalEntity;
				hits[j].pCollider = 0;
				event.pForeignData[1] = 0; event.iForeignData[1] = -1;
				event.vloc[1].zero();
				event.mass[1] = 0;
				event.idCollider = -1;
			}
			event.vloc[0] = vel0;
			event.mass[0] = m_mass;
			event.partid[0] = m_iPierceability;
			event.idmat[0] = m_surface_idx;
			event.idmat[1] = hits[j].surface_idx;
			event.penetration = -0.01f*isneg(hits[j].idmatOrg);	// penetration<0 and idmatOrg<0 tell that surfacetype was modified via geom_mat_substitutor
			m_pWorld->OnEvent(m_flags,&event);
			if (hits[j].surface_idx==m_pWorld->m_matWater) {
				if (hits[j].pCollider && hits[j].pCollider->GetType()==PE_AREA && ((CPhysArea*)hits[j].pCollider)->m_pWaterMan)
					((CPhysArea*)hits[j].pCollider)->m_pWaterMan->OnWaterHit(hits[j].pt,vel0);
			} else {
				m_bHadCollisions=1; m_bRecentCollisions += (3-m_bRecentCollisions)*isneg(hits[j].n*gravity-0.001f);
			}
		}

		if (bHit && hits[0].pCollider && ((CPhysicalPlaceholder*)hits[0].pCollider)->m_iSimClass!=5) {
			e = max(min((m_pWorld->m_BouncinessTable[m_surface_idx & NSURFACETYPES-1] + 
									 m_pWorld->m_BouncinessTable[hits[0].surface_idx & NSURFACETYPES-1])*0.5f, 1.0f), 0.0f);
			friction = m_pWorld->GetFriction(m_surface_idx, hits[0].surface_idx); 
			float minv = ((CPhysicalEntity*)hits[0].pCollider)->GetMassInv();
			k = m_mass*minv/(1+m_mass*minv);
			/*CPhysicalEntity *pCollider = (CPhysicalEntity*)hits[0].pCollider;
			if ((pCollider->m_iSimClass>0 || pCollider->m_parts[hits[0].ipart].flags & geom_monitor_contacts) && hits[0].pCollider->GetType()!=PE_PARTICLE) {
				RigidBody *pbody = ((CPhysicalEntity*)hits[0].pCollider)->GetRigidBody(hits[0].ipart);
				Vec3 vrel = vel0-pbody->v-(pbody->w^hits[0].pt-pbody->pos);
				float M = ((CPhysicalEntity*)hits[0].pCollider)->GetMass(hits[0].ipart);
				if (vel0.len2()>sqr(m_minBounceVel)) {
					ai.point = hits[0].pt;
					ai.impulse = vrel*(m_mass*M/(m_mass+M)*(1+e));
					ai.ipart = hits[0].ipart;
					hits[0].pCollider->Action(&ai,1);

					arce.pt = hits[0].pt;
					arce.n = hits[0].n;
					arce.v = vel;
					arce.collMass = m_mass;
					arce.pCollider = this;
					arce.partid[0] = hits[0].partid;
					arce.partid[1] = 0;
					arce.idmat[0] = hits[0].surface_idx;
					arce.idmat[1] = m_surface_idx;
					hits[0].pCollider->Action(&arce);
				}
			}*/
			if (sqr(m_dim)<(hits[0].pt-pos0).len2())
				pos = hits[0].pt - heading0*m_dim;
			else 
				pos = pos0;
			if (((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass>1) {
				hits[0].pCollider->GetStatus(&sd);
				vel0 -= (sd.v+=(sd.w^pos-sd.centerOfMass));
				//vel += hits[0].n*(sd.v*hits[0].n);
			} else sd.v.zero();

			vn = hits[0].n*vel0; vtang = vel0-hits[0].n*vn;
			if (vn>-m_minBounceVel || m_dimLying<m_dim*0.3f)
				e = 0;
			vel = vel0-hits[0].n*(vn*(1-k)*(1+e));
			/*if (sqr_signed(vn*(k-1)*friction) < vtang.len2())
				vel += vtang.normalized()*min(0.0f,vn*(1-k))*friction;
			else
				vel -= vtang;*/
			vel -= vtang*(1-k)*(1-e);

			j = isneg(hits[0].n*gravity-0.001f);
			if (vel.len2()<sqr(m_minVel)*j || m_flags & particle_single_contact) {
				vel.zero(); m_wspin.zero(); m_qspin.SetIdentity();
				if (m_dim!=m_dimLying)
					pos = hits[0].pt+hits[0].n*m_dimLying;
				m_bSliding = 1; slide_normal = hits[0].n;
				if (!(flags & particle_constant_orientation) && m_normal.len2()>0.0f)
					qrot = Quat::CreateRotationV0V1(qrot*m_normal,hits[0].n)*qrot;
			} else {
				vel_next = vel + (gravity + heading*m_accThrust - vel*kAirResistance +
					(heading^(heading^m_gravity)).normalize()*(m_kAccLift*vel.len()))*time_interval;
				if ((vel_next*hits[0].n)*j < (m_minVel+0.001f)*j) {
					if (m_dim!=m_dimLying)
						pos = hits[0].pt+hits[0].n*m_dimLying;
					m_bSliding = 1; slide_normal = hits[0].n;
					if (m_flags & particle_no_roll && !(m_flags & particle_constant_orientation) && m_normal.len2()>0.0f)
						qrot = Quat::CreateRotationV0V1(qrot*m_normal,hits[0].n)*qrot;
				}

				if (m_flags & particle_no_roll) m_wspin.zero();
				else if ((m_wspin = (hits[0].n^vtang)*m_rdim).len2() > sqr(20.0f))
					m_wspin.normalize() *= 20.0f;
				hits[0].n.z = 1.0f;
			}
			vel += sd.v;

			m_bForceAwake = hits[0].n.z>0.7f && (((CPhysicalEntity*)hits[0].pCollider)->m_iSimClass<=2 || m_timeForceAwake>40.0f) ? 2:1;
		}
		i = m_bForceAwake & 1;
		m_timeForceAwake = m_timeForceAwake*i + time_interval*i;
		m_sleepTime = 0;

		Vec3 BBox[2];
		int bGridLocked = 0;
		BBox[0] = pos-Vec3(m_dim,m_dim,m_dim);
		BBox[1] = pos+Vec3(m_dim,m_dim,m_dim);
		if (m_flags & particle_traceable)
			bGridLocked = m_pWorld->RepositionEntity(this,1,BBox);
		{ WriteLock lock(m_lockUpdate);
			m_pos=pos; m_qrot=qrot; m_BBox[0]=BBox[0]; m_BBox[1]=BBox[1];
			m_pWorld->UnlockGrid(this,-bGridLocked);
		}
		{ WriteLock lock(m_lockParticle);
			m_vel=vel; m_slide_normal=slide_normal; m_heading=heading;
		}

/*if (CheckPointInside(m_pos) && g_retest) {
m_pos=postest; m_vel=veltest; m_heading=headingtest; m_slide_normal=slidingnormaltest; m_bSliding=bslidingtest;
goto doretest; }*/

		if (iCaller<MAX_PHYS_THREADS) {
			IPhysicalEntity *gridRefNew[2] = { nullptr,nullptr };
			for(i=0; portals[i]; i++)	{
				pe_params_outer_entity poe;
				IPhysicalEntity *insideWhat = portals[i]->GetParams(&poe) && poe.pOuterEntity ? poe.pOuterEntity : portals[i];
				pe_status_contains_point scp;
				scp.pt = m_pWorld->GetGrid(insideWhat)->m_transW.GetInverted() * (m_pWorld->GetGrid(this)->m_transW * m_pos);
				int isExit = 1-isneg(-(((CPhysicalPlaceholder*)portals[i])->m_iForeignFlags & TRIGGER_PORTAL_INV));
				int inside = insideWhat->GetStatus(&scp);
				if (inside ^ isExit)
					gridRefNew[isExit] = ((CPhysicalPlaceholder*)portals[i])->m_pEntBuddy;
			}
			if (!gridRefNew[0])
				gridRefNew[0] = gridRefNew[1];
			if (gridRefNew[0] && m_pWorld->GetGrid(this)!=m_pWorld->GetGrid(gridRefNew[0])) {
				pe_params_pos pp;
				pp.pGridRefEnt = gridRefNew[0];
				SetParams(&pp);
			}

			EventPhysPostStep epps;	InitEvent(&epps,this,iCaller);
			epps.dt=time_interval; epps.pos=m_pos; epps.q=m_qrot; epps.idStep=m_pWorld->m_idStep;
			m_pWorld->OnEvent(m_flags,&epps);
		}

		if ((m_nStepCount|(int)(char)m_areaCheckPeriod>>31)==0) {
			pe_params_buoyancy pb[4];
			Vec3 vmedium[2] = { Vec3(ZERO),Vec3(ZERO) };
			float depth;

			if (nhits = m_pWorld->CheckAreas(this,gravity,pb,4,Vec3(ZERO),iCaller)) {
				if (!is_unused(gravity))
					m_waterGravity=m_gravity = (gravity-m_pWorld->m_vars.gravity).len2() < gravity.len2()*sqr(0.01) ? m_gravity0 : gravity;
				for(i=0,m_depth=depth=0; i<nhits; i++) {
					vmedium[pb[i].iMedium] += pb[i].waterFlow;
					if (pb[i].iMedium==0 && (depth=(m_pos-pb[i].waterPlane.origin)*pb[i].waterPlane.n)<0)
						m_depth = min(m_depth,depth);
				}
				m_velMedium = vmedium[isnonneg(m_depth)];
			}
			m_nStepCount = m_areaCheckPeriod;
		}
		m_nStepCount -= 1+((int)(char)m_nStepCount>>31);

		g_pCurParticle[iCaller] = 0;
	} else
		m_sleepTime += time_interval;
	
	return 1;
}

static geom_contact g_ParticleContact[MAX_PHYS_THREADS+1];

int CParticleEntity::RayTrace(SRayTraceRes& rtr)
{
	CParticleEntity *pCurParticle = g_pCurParticle[get_iCaller()];
	if (pCurParticle && pCurParticle->m_flags & m_flags & particle_no_self_collisions)
		return 0;
	prim_inters inters;
	box abox;
	Vec3 normal = m_normal.len2()>0.0f ? m_qrot*m_normal : m_heading;
	abox.Basis.SetRow(2,normal);
	abox.Basis.SetRow(0,normal.GetOrthogonal().normalized());
	abox.Basis.SetRow(1,abox.Basis.GetRow(2)^abox.Basis.GetRow(0));
	abox.size(m_dim,m_dim,m_dimLying);
	abox.center = m_pos;

	if (box_ray_intersection(&abox,&rtr.pRay->m_ray,&inters)) {
		int caller = get_iCaller();
		assert (0 <= caller && caller < (MAX_PHYS_THREADS+1));

		rtr.pcontacts = &g_ParticleContact[caller];
		rtr.pcontacts->pt = inters.pt[0];
		rtr.pcontacts->t = (inters.pt[0]-rtr.pRay->m_ray.origin)*rtr.pRay->m_dirn;
		rtr.pcontacts->id[0] = m_surface_idx;
		rtr.pcontacts->iNode[0] = 0;
		rtr.pcontacts->n = inters.n;
		return 1;
	}
	return 0;
}

void CParticleEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer,flags);

	if (flags & pe_helper_geometry) {
		box abox;
		Vec3 normal = m_normal.len2()>0.0f ? m_qrot*m_normal : m_heading;
		abox.Basis.SetRow(2,normal);
		abox.Basis.SetRow(0,normal.GetOrthogonal().normalized());
		abox.Basis.SetRow(1,abox.Basis.GetRow(2)^abox.Basis.GetRow(0));
		abox.center = m_pos;
		int i,j;
		Vec3 pt[8];
		for(i=0;i<8;i++)
			pt[i] = Vec3(m_dim*((i<<1&2)-1),m_dim*((i&2)-1),m_dimLying*((i>>1&2)-1))*abox.Basis+abox.center;
		for(i=0;i<8;i++) for(j=0;j<3;j++) if (i&1<<j)
			pRenderer->DrawLine(pt[i],pt[i^1<<j],m_iSimClass);
	}
	if (flags & pe_helper_collisions && m_bSliding)
		pRenderer->DrawLine(m_pos-m_slide_normal*m_dimLying,m_pos+m_slide_normal*0.1f/*(m_dimLying*3)*/,m_iSimClass);
}


void CParticleEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	CPhysicalEntity::GetMemoryStatistics(pSizer);
	if (GetType()==PE_PARTICLE)
		pSizer->AddObject(this, sizeof(CParticleEntity));
}
