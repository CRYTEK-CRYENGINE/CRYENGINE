// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "overlapchecks.h"
#include "raybv.h"
#include "raygeom.h"
#include "singleboxtree.h"
#include "boxgeom.h"
#include "cylindergeom.h"
#include "spheregeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "ropeentity.h"
#include "trimesh.h"
#include "intersectionchecks.h"
#define m_bAwake m_bPermanent

CRopeEntity::CRopeEntity(CPhysicalWorld *pWorld, IGeneralMemoryHeap* pHeap)
	: CPhysicalEntity(pWorld, pHeap)
	, m_gravity(ZERO)
	, m_gravity0(ZERO)
	, m_damping(0.2f)
	, m_maxAllowedStep(0.05f)
	, m_Emin(sqr(0.04f))
	, m_timeStepPerformed(0.0f)
	, m_timeStepFull(0.0f)
	, m_nSlowFrames(0)
	, m_lastTimeStep(0.0f)
	, m_nSleepingNeighboursFrames(0)
	, m_bHasContacts(0)
	, m_bContactsRegistered(0)
	, m_lockVtx(0)
	, m_lockAwake(0)
	, m_length(0.0f)
	, m_nSegs(-1)
	, m_segs(nullptr)
	, m_timeLastActive(0.0f)
	, m_bTargetPoseActive(0)
	, m_stiffnessAnim(70.0f)
	, m_lastposHost(ZERO)
	, m_lastqHost(IDENTITY)
	, m_mass(1.0f)
	, m_collDist(0.01f)
	, m_surface_idx(0)
	, m_friction(0.2f)
	, m_stiffness(10.0f)
	, m_dampingAnim(0.0f)
	, m_stiffnessDecayAnim(0.75f)
	, m_wind(ZERO)
	, m_wind0(ZERO)
	, m_wind1(ZERO)
	, m_airResistance(0.0f)
	, m_windVariance(0.0f)
	, m_windTimer(0.0f)
	, m_waterResistance(5.0f)
	, m_rdensity(1.0f / 500)
	, m_jointLimit(0.0f)
	, m_jointLimitDecay(0)
	, m_szSensor(0.05f)
	, m_maxForce(0.0f)
	, m_flagsCollider(geom_colltype0)
	, m_collTypes(0)
	, m_penaltyScale(5.0f)
	, m_maxIters(650)
	, m_attachmentZone(0.0f)
	, m_minSegLen(0.0f)
	, m_unprojLimit(0.5f)
	, m_noCollDist(0.5f)
	, m_idConstraint(0)
	, m_iConstraintClient(0)
	, m_dir0dst(ZERO)
	, m_vtx(nullptr)
	, m_vtx1(nullptr)
	, m_vtxSolver(nullptr)
	, m_nVtx(0)
	, m_nVtxAlloc(0)
	, m_nVtx0(0)
	, m_nFragments(0)
	, m_nMaxSubVtx(3)
	, m_idx(nullptr)
	, m_bStrained(0)
	, m_frictionPull(0.0f)
	, m_energy(0.0f)
	, m_pContact(nullptr)
	, m_attach(nullptr)
	, m_nAttach(0)
#ifdef DEBUG_ROPES
	, m_segs(&m_nSegs, 1), m_vtx(&m_nVtxAlloc), m_vtx1(&m_nVtxAlloc), m_idx(&m_nMaxSubVtx, 2), m_vtxSolver(&m_nVtxAlloc)
#endif
{
	m_pTiedTo[0] = m_pTiedTo[1] = nullptr;
	m_ptTiedLoc[0].zero(); m_ptTiedLoc[1].zero();
	m_iTiedPart[0] = m_iTiedPart[1] = 0;

	m_posBody[0][0] = m_posBody[0][1] = m_posBody[1][0] = m_posBody[1][1] = Vec3(0, 0, 0);
	m_qBody[0][0] = m_qBody[0][1] = m_qBody[1][0] = m_qBody[1][1] = quaternionf(IDENTITY);

	if (pWorld) {
		m_gravity0 = m_gravity = pWorld->m_vars.gravity;
		m_stiffness = m_pWorld->m_vars.unprojVelScale;
		m_penaltyScale = m_pWorld->m_vars.unprojVelScale * 2;
	}

	MARK_UNUSED m_collBBox[0];

	//CPhysicalPlaceholder
	m_iSimClass = 4;
	m_ig[0].x = m_ig[1].x = m_ig[0].y = m_ig[1].y = NO_GRID_REG;

	//CPhysicalEntity
	m_flags = 0;
	m_defpart.id = 0;
	m_collisionClass.type |= collision_class_rope;
	m_bAwake = 0;
}

CRopeEntity::~CRopeEntity()
{
	if (m_segs)	delete[] m_segs; // cppcheck-suppress autovarInvalidDeallocation
	if (m_vtx) delete[] m_vtx;
	if (m_vtx1) delete[] m_vtx1;
	if (m_vtxSolver) delete[] m_vtxSolver;
	if (m_idx) delete[] m_idx;
	delete[] m_attach;
	m_segs = 0; m_nSegs = -1;
	for(int i=0;i<2;i++) if (m_pTiedTo[i] && !m_pWorld->m_bMassDestruction)  m_pTiedTo[i]->Release();
}

void CRopeEntity::AlertNeighbourhoodND(int mode)
{
	if (!mode) {
		pe_params_rope pr;
		pr.pEntTiedTo[0] = pr.pEntTiedTo[1] = 0;
		SetParams(&pr);
	} else if (mode==2) for (int i=0;i<2;i++) if (m_pTiedTo[i] && m_pTiedTo[i]->GetType()==PE_ROPE)
		m_pTiedTo[i]->AddCollider(this);
	int i;
	if (m_nSegs>0) for(i=0;i<=m_nSegs;i++) if (m_segs[i].pContactEnt) {
		if (!(m_flags & rope_subdivide_segs))
			m_segs[i].pContactEnt->Release(); 
		m_segs[i].pContactEnt=0;
	}
	for(i=0;i<m_nColliders;i++)
		m_pColliders[i]->Release();
	m_nColliders = 0;
}

int CRopeEntity::Awake(int bAwake,int iSource)
{ 
	m_bAwake=bAwake; m_nSlowFrames=0; 
	m_pWorld->RepositionEntity(this,2);
	int i;
	if (m_nSegs>0) {
		WriteLock lock(m_lockAwake);
		for(i=0;i<=m_nSegs;i++)	if (m_segs[i].pContactEnt && (m_segs[i].pContactEnt->m_iDeletionTime>0 || 
			m_segs[i].iContactPart>=m_segs[i].pContactEnt->m_nParts))
		{
			if (!(m_flags & rope_subdivide_segs))
				m_segs[i].pContactEnt->Release();
			m_segs[i].pContactEnt = 0;
			MARK_UNUSED m_segs[i].ncontact;
		}
		if (m_vtx && m_bStrained) for(i=0;i<m_nVtx;i++) if (m_vtx[i].pContactEnt && (m_vtx[i].pContactEnt->m_iDeletionTime>0 || 
			m_vtx[i].iContactPart>=m_vtx[i].pContactEnt->m_nParts))
			m_vtx[i].pContactEnt=0, MARK_UNUSED m_vtx[i].ncontact;
		for(i=0;i<2;i++) if (m_pTiedTo[i] && m_pTiedTo[i]->m_iDeletionTime>0)	{
			if (m_pTiedTo[i]->GetType()==PE_ROPE)
				m_pTiedTo[i]->RemoveCollider(this);
			m_pTiedTo[i]->Release(); m_pTiedTo[i]=0;
		}
		for(i=0;i<m_nColliders;i++) if (m_pColliders[i]->GetType()==PE_ROPE)
			m_pColliders[i]->Awake();
		if (!(m_flags & pef_step_requested))
			m_timeIdle = 0;
	}
	return 1; 
}

void CRopeEntity::RecalcBBox()
{
	if (m_flags & pef_traceable) {
		m_BBox[0]=m_BBox[1] = m_nSegs>0 ? m_segs[0].pt:m_pos;
		for(int i=1;i<=m_nSegs;i++) {
			m_BBox[0] = min(m_BBox[0],m_segs[i].pt); 
			m_BBox[1] = max(m_BBox[1],m_segs[i].pt); 
		}
		m_BBox[0]-=Vec3(m_collDist*2); m_BBox[1]+=Vec3(m_collDist*2);
		m_pWorld->RepositionEntity(this,1|8,m_BBox);
	}
}


int CRopeEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1;

	int res;
	unsigned int flags0 = m_flags;
	Vec3 prevpos = m_pos;
	quaternionf prevq = m_qrot;
	if (res = CPhysicalEntity::SetParams(_params,1)) {
		if (_params->type==pe_params_pos::type_id) {
			WriteLock lock(m_lockUpdate);
			Matrix33 R = Matrix33(m_qrot);
			if ((prevpos-m_pos).len2()>sqr(0.0001f) || (prevq.v-m_qrot.v).len2()>sqr(0.0001f))
				m_nVtx = 0;
			for(int i=0;i<=m_nSegs;i++) {
				m_segs[i].pt = m_segs[i].pt0 = R*(m_segs[i].pt-prevpos)+m_pos;
				m_segs[i].vel = R*m_segs[i].vel;
				m_segs[i].dir = R*m_segs[i].dir;
			}
			m_qrot.SetIdentity();
		}
		if (m_nSegs>0 && !(m_flags & (rope_collides|rope_collides_with_terrain)) && (flags0 & (rope_collides|rope_collides_with_terrain)))
			for(int i=0;i<=m_nSegs;i++) m_segs[i].pContactEnt = 0;

		RecalcBBox();

		if ((m_flags^flags0) & rope_collides)
			(m_collTypes &= ~(ent_static|ent_sleeping_rigid|ent_rigid|ent_living)) |= (ent_static|ent_sleeping_rigid|ent_rigid|ent_living) & -((int)m_flags & rope_collides)>>31;
		if ((m_flags^flags0) & rope_collides_with_terrain)
			m_collTypes ^= ent_terrain;
		return res;
	}

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		if (!is_unused(params->gravity)) m_gravity = m_gravity0 = params->gravity;
		if (!is_unused(params->damping)) m_damping = params->damping;
		if (!is_unused(params->maxTimeStep)) m_maxAllowedStep = params->maxTimeStep;
		if (!is_unused(params->minEnergy)) m_Emin = params->minEnergy;
		return 1;
	}

	if (_params->type==pe_params_rope::type_id) {
		pe_params_rope *params = (pe_params_rope*)_params;
		WriteLock lock(m_lockUpdate);
		int i;
		float diff=0;
		if (!is_unused(params->length)) m_length = params->length;
		else if (!is_unused(params->nSegments) && !is_unused(params->pPoints) && params->pPoints)
			for(i=0,m_length=0;i<params->nSegments;i++) m_length += (params->pPoints[i+1]-params->pPoints[i]).len();
		if (!is_unused(params->mass)) m_mass = params->mass;
		if (!is_unused(params->collDist)) { m_collDist = params->collDist; RecalcBBox(); }
		if (!is_unused(params->surface_idx)) m_surface_idx = params->surface_idx;
		if (!is_unused(params->friction)) m_friction = params->friction;
		if (!is_unused(params->stiffnessAnim)) 
			diff=fabs_tpl(m_stiffnessAnim-params->stiffnessAnim), m_stiffnessAnim=params->stiffnessAnim;
		if (!is_unused(params->stiffnessDecayAnim)) m_stiffnessDecayAnim = params->stiffnessDecayAnim;
		if (!is_unused(params->dampingAnim)) m_dampingAnim = params->dampingAnim;
		if (!is_unused(params->bTargetPoseActive)) 
			diff+=fabs_tpl(m_bTargetPoseActive-params->bTargetPoseActive), m_bTargetPoseActive = params->bTargetPoseActive;
		if (!is_unused(params->wind)) 
			diff+=(m_wind0-params->wind).len2(), m_wind0=m_wind1=m_wind = params->wind;
		if (!is_unused(params->windVariance)) m_windVariance = params->windVariance;
		if (!is_unused(params->airResistance)) m_airResistance = params->airResistance;
		if (!is_unused(params->waterResistance)) m_waterResistance = params->waterResistance;
		if (!is_unused(params->density)) m_rdensity = 1.0f/params->density;
		if (!is_unused(params->jointLimit)) m_jointLimit = params->jointLimit; 
		if (!is_unused(params->jointLimitDecay)) m_jointLimitDecay = params->jointLimitDecay; 
		if (!is_unused(params->sensorRadius)) m_szSensor = params->sensorRadius;
		if (!is_unused(params->maxForce)) m_maxForce = params->maxForce;
		if (!is_unused(params->flagsCollider)) m_flagsCollider = params->flagsCollider;
		if (!is_unused(params->collTypes)) m_collTypes = params->collTypes;
		if (!is_unused(params->nMaxSubVtx)) m_nMaxSubVtx = params->nMaxSubVtx;
		if (!is_unused(params->frictionPull)) m_frictionPull = params->frictionPull;
		if (!is_unused(params->stiffness)) m_stiffness = params->stiffness;
		if (!is_unused(params->penaltyScale)) m_penaltyScale = params->penaltyScale;
		if (!is_unused(params->maxIters)) m_maxIters = params->maxIters;
		if (!is_unused(params->attachmentZone)) m_attachmentZone = params->attachmentZone;
		if (!is_unused(params->minSegLen)) m_minSegLen = params->minSegLen;
		if (!is_unused(params->unprojLimit)) m_unprojLimit = params->unprojLimit;
		if (!is_unused(params->noCollDist)) m_noCollDist = params->noCollDist;
		if (!is_unused(params->collisionBBox[0]))
			if ((params->collisionBBox[1]-params->collisionBBox[0]).len2()>0) {
				m_collBBox[0]=params->collisionBBox[0]; m_collBBox[1]=params->collisionBBox[1];
			} else
				MARK_UNUSED m_collBBox[0];
		if (!is_unused(params->hingeAxis)) m_dir0dst = params->hingeAxis;
		if (!is_unused(params->nSegments)) {
			diff++;
			Vec3 pt[2]={ m_pos, m_pos-Vec3(0,0,m_length) };
			for(i=0;i<2;i++) if (m_pTiedTo[i]) {
				Vec3 pos; quaternionf q; float scale;
				m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos,q,scale, this);
				pt[i] = pos + q*m_ptTiedLoc[i];
			}
			if (m_nSegs!=params->nSegments) {
				if (m_segs) delete[] m_segs;
				m_segs = new rope_segment[params->nSegments+1];
				memset(m_segs, 0, (params->nSegments+1)*sizeof(rope_segment));
			}
			float rnSegs = 1.0f/params->nSegments;
			if (params->nSegments!=m_nSegs || m_pTiedTo[0] && m_pTiedTo[1]) {
				m_nSegs = params->nSegments;
				for(i=0;i<=params->nSegments;i++)
					m_segs[i].pt=m_segs[i].pt0 = pt[0]+(pt[1]-pt[0])*(i*rnSegs);
				if (m_flags & rope_subdivide_segs) {
					// cppcheck-suppress autovarInvalidDeallocation
					if (m_vtx) delete[] m_vtx;
					if (m_vtx1) delete[] m_vtx1;
					m_nVtxAlloc=(params->nSegments+1)*2;
					memset(m_vtx = new rope_vtx[m_nVtxAlloc], 0, m_nVtxAlloc*sizeof(rope_vtx));
					memset(m_vtx1 = new rope_vtx[m_nVtxAlloc], 0, m_nVtxAlloc*sizeof(rope_vtx));
					for(i=0;i<=params->nSegments;i++)	{
						m_vtx[i].pt=m_vtx[i].pt0 = m_segs[i].pt;
						MARK_UNUSED m_vtx[i].ncontact;
						m_segs[i].iVtx0 = i;
					}
					m_nVtx=m_nVtx0 = params->nSegments+1;
				}
			}
		}
		if (!is_unused(params->pPoints) && params->pPoints) {
			for(i=0;i<=m_nSegs;i++) m_segs[i].pt = m_segs[i].pt0 = params->pPoints[i];
			m_nVtx=m_nVtx0=0; RecalcBBox();	++diff;
		}
		if (!is_unused(params->pVelocities) && params->pVelocities)	{
			for(i=0;i<=m_nSegs;i++) m_segs[i].vel = params->pVelocities[i];
			++diff;
		}

		if (m_idConstraint && m_pTiedTo[m_iConstraintClient]) {
			pe_action_update_constraint arc;
			arc.idConstraint=m_idConstraint; arc.bRemove=1;
			m_pTiedTo[m_iConstraintClient]->Action(&arc);
			m_idConstraint = 0;
		}
		//for(i=0;i<2;i++) if (m_pTiedTo[i] && m_pTiedTo[i]->m_iSimClass==7)
		//	m_pTiedTo[i] = 0;

		if (m_segs) for(i=0;i<2;i++) if (!is_unused(params->pEntTiedTo[i])) {
			if (m_pTiedTo[i]) m_pTiedTo[i]->Release();
			CPhysicalEntity *pPrevTied = m_pTiedTo[i];
			m_pTiedTo[i] = params->pEntTiedTo[i]==WORLD_ENTITY ? &g_StaticPhysicalEntity : 
										 (params->pEntTiedTo[i] ? ((CPhysicalPlaceholder*)params->pEntTiedTo[i])->GetEntity() : 0);
			if (m_pTiedTo[i]!=pPrevTied && pPrevTied)	{
				pPrevTied->RemoveCollider(this);
				if (!(m_flags & pef_disabled)) {
					if ((unsigned int)pPrevTied->m_iSimClass<7u)
						pPrevTied->Awake();
					if (m_pTiedTo[i^1] && (unsigned int)m_pTiedTo[i^1]->m_iSimClass<7u)
						m_pTiedTo[i^1]->Awake();
					diff++;
				}
			}
			//if (m_pTiedTo[i] && m_pTiedTo[i]->m_iDeletionTime>0)
			//	m_pTiedTo[i] = 0;
			if (m_pTiedTo[i]) {
				if (!is_unused(params->idPartTiedTo[i])) if (m_pTiedTo[i]->GetType()!=PE_ROPE) {
					for(m_iTiedPart[i]=0; m_iTiedPart[i]<m_pTiedTo[i]->m_nParts && 
							params->idPartTiedTo[i]!=m_pTiedTo[i]->m_parts[m_iTiedPart[i]].id; m_iTiedPart[i]++);
					if (m_iTiedPart[i]>=m_pTiedTo[i]->m_nParts)
						m_iTiedPart[i] = 0;
				}	else
					m_iTiedPart[i] = params->idPartTiedTo[i];
				Vec3 pos; quaternionf q; float scale;
				m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos,q,scale, this);
				if (!is_unused(params->ptTiedTo[i]))
					m_ptTiedLoc[i] = params->bLocalPtTied ? params->ptTiedTo[i] : (params->ptTiedTo[i]-pos)*q;
				else if (m_nSegs>0 && (!is_unused(params->pEntTiedTo[i]) || !is_unused(params->idPartTiedTo[i])))
					m_ptTiedLoc[i] = (m_segs[m_nSegs*i].pt-pos)*q;
				m_pTiedTo[i]->AddRef();
				if (m_pTiedTo[i]->GetType()==PE_ROPE)
					m_pTiedTo[i]->AddCollider(this);
				if (m_nSegs>0 && m_segs[i*m_nSegs].pContactEnt==m_pTiedTo[i]) {
					m_pTiedTo[i]->Release();
					m_segs[i*m_nSegs].pContactEnt = 0;
				}
				if (i==0)	{
					m_lastposHost=pos; m_lastqHost=q;
				}
			}
		}
		if (diff>1E-6f) {
			m_bAwake = 1;
			m_pWorld->RepositionEntity(this,2);
			m_nSlowFrames = 0;
		}

		/*if (m_pTiedTo[0] && (m_pTiedTo[0]==this || m_pTiedTo[0]->m_iSimClass==3 || m_pTiedTo[0]->GetType()==PE_ROPE || m_pTiedTo[0]->GetType()==PE_SOFT) ||
				m_pTiedTo[1] && (m_pTiedTo[1]==this || m_pTiedTo[1]->m_iSimClass==3 || m_pTiedTo[1]->GetType()==PE_ROPE || m_pTiedTo[1]->GetType()==PE_SOFT))
		{	// rope cannot be attached to such objects
			if (m_pTiedTo[0]) m_pTiedTo[0]->Release();
			if (m_pTiedTo[1]) m_pTiedTo[1]->Release();
			m_pTiedTo[0] = 0; m_pTiedTo[1] = 0;
			return 0;
		}*/

		if (m_pTiedTo[0] && m_pTiedTo[1] && (m_pTiedTo[0]!=m_pTiedTo[1] || m_pTiedTo[0]->m_iSimClass<3)) {
			pe_action_add_constraint aac;
			Vec3 pos; quaternionf q; float scale;
			m_iConstraintClient = isneg(m_pTiedTo[0]->GetMassInv()-m_pTiedTo[1]->GetMassInv());
			for(i=0;i<2;i++) {
				m_pTiedTo[m_iConstraintClient^i]->GetLocTransform(m_iTiedPart[m_iConstraintClient^i], pos,q,scale, this);
				aac.pt[i] = pos + q*m_ptTiedLoc[m_iConstraintClient^i];
				aac.partid[i] = m_pTiedTo[m_iConstraintClient^i]->m_parts[m_iTiedPart[m_iConstraintClient^i]].id;
			}
			aac.pBuddy = m_pTiedTo[m_iConstraintClient^1];
			aac.pConstraintEntity = this;
			aac.maxPullForce = m_maxForce;
			aac.damping = m_frictionPull;
			m_idConstraint = m_pTiedTo[m_iConstraintClient]->Action(&aac);
		}

		return 1;
	}

	return 0;
}

int CRopeEntity::GetParams(pe_params *_params) const
{
	int res;
	if (res = CPhysicalEntity::GetParams(_params))
		return res;

	if (_params->type==pe_simulation_params::type_id) {
		pe_simulation_params *params = (pe_simulation_params*)_params;
		params->gravity = m_gravity;
		params->damping = m_damping;
		params->maxTimeStep = m_maxAllowedStep;
		params->minEnergy = m_Emin;
		return 1;
	}

	if (_params->type==pe_params_rope::type_id) {
		pe_params_rope *params = (pe_params_rope*)_params;
		ReadLock lock(m_lockUpdate);
		params->length = m_length;
		params->mass = m_mass;
		params->nSegments = m_nSegs;
		params->collDist = m_collDist;
		params->surface_idx = m_surface_idx;
		params->friction = m_friction;
		params->stiffnessAnim = m_stiffnessAnim;
		params->stiffnessDecayAnim = m_stiffnessDecayAnim;
		params->dampingAnim = m_dampingAnim;
		params->bTargetPoseActive = m_bTargetPoseActive;
		params->wind = m_wind;
		params->windVariance = m_windVariance;
		params->airResistance = m_airResistance;
		params->waterResistance = m_waterResistance;
		params->density = 1.0f/m_rdensity;
		params->jointLimit = m_jointLimit; 
		params->jointLimitDecay = m_jointLimitDecay; 
		params->sensorRadius = m_szSensor;
		params->maxForce = m_maxForce;
		params->flagsCollider = m_flagsCollider;
		params->collTypes = m_collTypes;
		params->nMaxSubVtx = m_nMaxSubVtx;
		params->frictionPull = m_frictionPull;
		params->stiffness = m_stiffness;
		params->penaltyScale = m_penaltyScale;
		params->maxIters = m_maxIters;
		params->attachmentZone = m_attachmentZone;
		params->minSegLen = m_minSegLen;
		params->unprojLimit = m_unprojLimit;
		params->noCollDist = m_noCollDist;
		params->pPoints = strided_pointer<Vec3>(&m_segs[0].pt, sizeof(m_segs[0]));
		params->pVelocities = strided_pointer<Vec3>(&m_segs[0].vel, sizeof(m_segs[0]));
		for(int i=0;i<2;i++) if (params->pEntTiedTo[i] = m_pTiedTo[i]) {
			if (m_pTiedTo[i]==&g_StaticPhysicalEntity)
				params->pEntTiedTo[i] = WORLD_ENTITY;
			params->idPartTiedTo[i] = m_pTiedTo[i]->m_parts[m_iTiedPart[i]].id;
			Vec3 pos; quaternionf q; float scale;
			m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos,q,scale, this);
			params->ptTiedTo[i] = pos + q*m_ptTiedLoc[i];
		}
		if (is_unused(m_collBBox[0]))
			params->collisionBBox[0]=params->collisionBBox[1].zero();
		else {
			params->collisionBBox[0] = m_collBBox[0];
			params->collisionBBox[1] = m_collBBox[1];
		}
		params->hingeAxis = m_dir0dst;
		return 1;
	}

	return 0;
}


int CRopeEntity::GetStatus(pe_status *_status) const
{
	PrefetchLine(&m_nSegs, 0);

	if (_status->type==pe_status_rope::type_id) {
		PrefetchLine(&m_lastqHost, 0);
		pe_status_rope *status = (pe_status_rope*)_status;			
		QuatT transG = is_unused(status->pGridRefEnt) ? QuatT(IDENTITY) : m_pWorld->GetGrid(status->pGridRefEnt)->m_transW.GetInverted() * m_pWorld->GetGrid(this)->m_transW;
		{ ReadLock lock(m_lockUpdate);
			PrefetchLine(m_segs, 0);
			if (m_nSegs<0)
				return 0;
			status->nSegments = m_nSegs;
			int i;
			int *nCollCount[2] = { &(status->nCollStat=0), &(status->nCollDyn=0) };
			PREFAST_ASSUME(nCollCount[0] && nCollCount[1]);

			status->bTargetPoseActive = m_bTargetPoseActive;
			status->stiffnessAnim = m_stiffnessAnim;
			status->timeLastActive = m_timeLastActive;
			status->posHost = transG*m_lastposHost;
			status->qHost = transG.q*m_lastqHost;

			//All access to m_segs moved to the end so there's a chance of the cache line being retrieved
			union {CPhysicalEntity ** pe; IPhysicalEntity** pi;} u; u.pe = &m_segs[0].pContactEnt;
			status->pContactEnts = strided_pointer<IPhysicalEntity*>(u.pi, sizeof(m_segs[0]));
			volatile CPhysicalEntity *pContactEnt;
			for(i=0;i<m_nSegs;i++) if (pContactEnt = *(volatile CPhysicalEntity**)&m_segs[i].pContactEnt)
				(*nCollCount[-pContactEnt->m_iSimClass>>31 & 1])++;
			for(i=0;i<m_nAttach;i++)
				(*nCollCount[-m_attach[i].pent->m_iSimClass>>31 & 1])++;
			if (!is_unused(status->pPoints) && status->pPoints)
				for(i=0;i<=m_nSegs;i++) status->pPoints[i] = transG*m_segs[i].pt0;
			if (!is_unused(status->pVelocities) && status->pVelocities)
				for(i=0;i<=m_nSegs;i++) status->pVelocities[i] = transG.q*m_segs[i].vel;
			if (!is_unused(status->pContactNorms) && status->pContactNorms)
				for(i=0;i<m_nSegs;i++) if (m_segs[i].pContactEnt)
					status->pContactNorms[i] = transG*m_segs[i].ncontact;
		}

		if (m_flags & rope_subdivide_segs) {
			ReadLockCond lockVtx(m_lockVtx, isneg(status->lock)^1);
			lockVtx.SetActive(status->lock-1>>31&1);
			status->nVtx = m_nVtx0;
			if (status->pVtx) for(int i=0;i<m_nVtx0;i++)
				status->pVtx[i] = transG*m_vtx[i].pt0;
			status->bStrained = m_bStrained;
		} else {
			status->nVtx = 0;
			status->bStrained = m_nSegs ? isneg(sqr(m_length)-(m_segs[0].pt0-m_segs[m_nSegs].pt0).len2()) : 0;
		}

		return 1;
	}


	int res;	
	if (res = CPhysicalEntity::GetStatus(_status)) {
		if (_status->type==pe_status_caps::type_id) {
			pe_status_caps *status = (pe_status_caps*)_status;
			status->bCanAlterOrientation = 1;
		}
		return res;
	}	

	return 0;
}


void CRopeEntity::EnforceConstraints(float seglen, const quaternionf& qtv,const Vec3& offstv,float scaletv, int bTargetPoseActive, float dt)
{
	int i,iDir,iStart,iEnd,bHasContacts;

	if (!m_nAttach && (!m_pTiedTo[0] || !m_pTiedTo[1])) {
		float a,len,angleSrc,angleDst;
		Vec3 dir0src,dir1src,dir0dst,dir1dst,axis0src,axis1src,axis1dst;

		iDir = m_pTiedTo[1] && !m_pTiedTo[0] ? -1:1;
		iStart = m_nSegs & iDir>>31;
		iEnd = m_nSegs & -iDir>>31;

		if (m_flags & rope_subdivide_segs && m_bHasContacts) {
			int j,jEnd = m_nVtx-1 & -iDir>>31; 
			for(i=0,len=0,seglen*=1.05f;i<m_nVtx-1;i++) len += (m_vtx[i+1].pt-m_vtx[i].pt)*m_vtx[i].dir;
			if (len > seglen*m_nSegs)	{
				for(i=iStart,j=m_nVtx-1 & iDir>>31; i!=iEnd; i+=iDir) {
					Vec3 pt0;
					for(len=seglen,pt0=m_segs[i].pt; j!=jEnd && len>(a=(m_vtx[j+iDir].pt-pt0).GetLengthFast()); len-=a,pt0=m_vtx[j+=iDir].pt);
					if (j!=jEnd) {
						m_segs[i+iDir].pt = pt0+(m_vtx[j+iDir].pt-pt0).GetNormalizedFast()*len;
						if ((m_segs[i+iDir].pt-m_segs[i+iDir].pt0).len2() > sqr(seglen*0.5f))
							m_segs[i+iDir].pt0 = m_segs[i+iDir].pt+(m_segs[i+iDir].pt0-m_segs[i+iDir].pt).GetNormalizedFast()*(seglen*0.5f);
						int i0 = i+min(iDir,0);
						m_segs[i0].dir = (m_segs[i0+1].pt-m_segs[i0].pt).GetNormalizedFast();
					}
				}
				m_nVtx = 0;
			}
			return;
		}

		if (bTargetPoseActive==0) for(i=iStart;i!=iEnd;i+=iDir)
			m_segs[i+iDir].pt = m_segs[i].pt+(m_segs[i+(iDir>>31)].dir=(m_segs[i+iDir].pt-m_segs[i].pt).normalized())*seglen;
		else if (m_jointLimit<=0) {
			for(i=iStart,m_length=0;i!=iEnd;i+=iDir) {
				len = (m_segs[i+iDir].ptdst-m_segs[i].ptdst).len()*scaletv;	m_length += len;
				m_segs[i+iDir].pt = m_segs[i].pt+(m_segs[i+(iDir>>31)].dir=(m_segs[i+iDir].pt-m_segs[i].pt).normalized())*len;
			}
		} else {
			dir0dst = qtv*(m_segs[iStart+iDir].ptdst-m_segs[iStart].ptdst);
			m_length=len = (m_segs[iStart+iDir].ptdst-m_segs[iStart].ptdst).len();
			dir0src = (m_segs[iStart+iDir].pt-m_segs[iStart].pt).normalized();
			axis0src = dir0src^dir0dst;
			angleSrc = atan2_tpl(a=axis0src.len(), dir0src*dir0dst);
			if (angleSrc>m_jointLimit && a>1e-20f)
				dir0src = dir0src.GetRotated(axis0src/a, angleSrc-m_jointLimit);
			Vec3 n; bool bPlane;
			if (bPlane = m_dir0dst.len2()>0) {
				n = qtv*m_dir0dst;
				(dir0src -= n*(n*dir0src)).normalize();
			}
			m_segs[iStart+iDir].pt = m_segs[iStart].pt+(m_segs[iStart+(iDir>>31)].dir=dir0src)*(len*scaletv);
			float decay = m_jointLimitDecay/max(1,m_nSegs);

			for(i=iStart+iDir;i!=iEnd;i+=iDir) {
				len = (m_segs[i+iDir].ptdst-m_segs[i].ptdst).len();	m_length += len;
				m_segs[i+(iDir>>31)].dir = (m_segs[i+iDir].pt-m_segs[i].pt).normalized();
				dir0src = m_segs[i].pt-m_segs[i-iDir].pt; dir0dst = m_segs[i].ptdst-m_segs[i-iDir].ptdst;
				dir1src = m_segs[i+iDir].pt-m_segs[i].pt; dir1dst = m_segs[i+iDir].ptdst-m_segs[i].ptdst; 
				axis1src = dir0src^dir1src; axis1dst = dir0dst^dir1dst;
				angleSrc = atan2_tpl(a=axis1src.len(), dir0src*dir1src);
				angleDst = atan2_tpl(  axis1dst.len(), dir0dst*dir1dst);
				float limit = m_jointLimit*(1.0f - decay*fabs_tpl(i-iStart));
				if (fabs_tpl(angleSrc-angleDst)>limit && a>1e-20f && (!m_segs[i+(iDir>>31)].pContactEnt || bPlane))
					m_segs[i+(iDir>>31)].dir = m_segs[i+(iDir>>31)].dir.GetRotated(axis1src/a, angleDst-angleSrc-limit*sgnnz(angleDst-angleSrc));
				if (bPlane)
					(m_segs[i+(iDir>>31)].dir -= n*(n*m_segs[i+(iDir>>31)].dir)).normalize();
				m_segs[i+iDir].pt = m_segs[i].pt+m_segs[i+(iDir>>31)].dir*(len*scaletv);
			}
		}
	} else if (m_nAttach || m_nSegs && (!(m_flags & rope_subdivide_segs) || m_maxIters<sqr(m_nSegs)*10 && !m_bStrained && m_length>0)) {
		Vec3 dir,ptend[2] = { m_segs[0].pt,m_segs[m_nSegs].pt };
		float diff,len2,seglen2=sqr(seglen),rseglen=1.0f/max(1e-10f,seglen),rseglen2=sqr(rseglen),k;
		m_segs[0].kdP=m_segs[m_nSegs].kdP = 0;
		if (m_flags & rope_subdivide_segs && m_vtx) {
			float lenFree=0,lenFull=0;
			for(i=1;i<m_nSegs;i++) m_segs[i].kdP = iszero(m_segs[i].iVtx0+1-m_segs[i+1].iVtx0) & (int)is_unused(m_vtx[m_segs[i].iVtx0].ncontact);
			for(i=0;i<m_nVtx-1;i++) lenFull+=(m_vtx[i+1].pt-m_vtx[i].pt).len();
			for(i=bHasContacts=0;i<m_nSegs;i++) if (m_segs[i].kdP+m_segs[i+1].kdP>0) lenFree+=(m_segs[i+1].pt-m_segs[i].pt).len(),bHasContacts++;
			if (bHasContacts<2 || lenFull<=m_length)
				return;
			rseglen2 = sqr(rseglen = 1/(seglen=(lenFree+m_length-lenFull)/bHasContacts));
		} else 
			for(i=1;i<m_nSegs;i++) m_segs[i].kdP=1;
		for(i=0;i<m_nAttach;m_segs[m_attach[i++].ivtx].kdP=0);
		for(i=0;i<2;i++) m_segs[m_nSegs*i].kdP=!m_pTiedTo[i];

		bHasContacts = max(100,float2int((dt>0 ? max(0.2f,dt/m_maxAllowedStep):1.0f)*min(sqr(m_nSegs),m_maxIters>>2)));
		int iDir0 = (iDir = m_pTiedTo[1] && !m_pTiedTo[0] ? 1:-1);
		do {
			iDir = -iDir;
			iStart = m_nSegs & iDir>>31;
			iEnd = m_nSegs & -iDir>>31;

			for(i=iStart,bTargetPoseActive=0; i!=iEnd; i+=iDir)	{
				dir = m_segs[i+iDir].pt-m_segs[i].pt; len2 = dir.len2(); 
				if ((1-inrange(len2, seglen2*1.02f,seglen2*0.98f))*m_segs[i+iDir].kdP) {
					diff = sgnnz(len2-seglen2)*0.01f;
					if (inrange(len2, seglen2*1.1f,seglen2*0.8f)) { // use 3 terms of 1/sqrt(x) Taylor series expansion
						k = (len2-seglen2*(1+diff*2))*rseglen2*(1-diff*2); 
						k = 1.0f+(-0.5f+(0.375f-0.3125f*k)*k)*k;
					} else
						k = seglen*(1+diff)/sqrt_tpl(len2);
					m_segs[i+iDir].pt = m_segs[i].pt + dir*k;
					bHasContacts--;
					bTargetPoseActive = 1;
				}	else k = 1.0f;
				m_segs[i+(iDir>>31)].dir = dir*(k*rseglen*iDir);
			}
		} while((m_pTiedTo[iDir+1>>1] || m_nAttach) && bTargetPoseActive && (iDir!=iDir0 || bHasContacts>0));

		if (m_flags & rope_subdivide_segs && m_vtx)
			for(i=0;i<m_nSegs;i++) m_vtx[m_segs[i].iVtx0].pt = m_segs[i].pt;
		for(i=0;i<m_nSegs;i++) m_segs[i].dir = (m_segs[i+1].pt-m_segs[i].pt).normalized();
	}
}

int CRopeEntity::Action(pe_action *_action, int bThreadSafe)
{
	if (_action->type==pe_action_notify::type_id) {
		pe_action_notify *action = (pe_action_notify*)_action;
		if (action->iCode==pe_action_notify::ParentChange) {
			int i; Vec3 pos; quaternionf q; float scale;
			if ((!m_pTiedTo[0] || !m_pTiedTo[1]) && m_nSegs>0 && m_segs) { // if the rope is tied with at most one end, do exact length enforcement
				for(i=0;i<2;i++) if (m_pTiedTo[i]) {
					m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos,q,scale, this);
					m_segs[m_nSegs*i].pt=m_segs[m_nSegs*i].pt0 = pos + q*m_ptTiedLoc[i];
				}
				q.SetIdentity(); pos.zero(); scale=1.0f;
				/*if (m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1)) {
					i = (m_flags & rope_target_vtx_rel1)!=0;
					if (m_pTiedTo[i]) {
						m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos,q,scale);
						if (m_bTargetPoseActive==2)
							m_ptTiedLoc[i] = m_segs[i*m_nSegs].ptdst;
					}
				}*/
				EnforceConstraints(m_length/m_nSegs, q,pos,scale, m_bTargetPoseActive);
				for(i=0;i<=m_nSegs;i++)
					m_segs[i].pt0 = m_segs[i].pt;
				if (!(m_flags & pef_disabled))	{
					m_bAwake=1; m_nSlowFrames=0;
					m_pWorld->RepositionEntity(this,2);
				}
				/*iDir = m_pTiedTo[1] && !m_pTiedTo[0] ? -1:1;
				iStart = m_nSegs & iDir>>31;
				iEnd = m_nSegs & -iDir>>31;
				if (m_bTargetPoseActive==0) for(i=iStart,seglen=m_length/m_nSegs;i!=iEnd;i+=iDir)
					m_segs[i+iDir].pt0=m_segs[i+iDir].pt = m_segs[i].pt+(m_segs[i+(iDir>>31)].dir=(m_segs[i+iDir].pt-m_segs[i].pt).normalized())*seglen;
				else for(i=iStart;i!=iEnd;i+=iDir)
					m_segs[i+iDir].pt0=m_segs[i+iDir].pt = m_segs[i].pt+(m_segs[i+(iDir>>31)].dir=(m_segs[i+iDir].pt-m_segs[i].pt).normalized())*
					(m_segs[i+iDir].ptdst-m_segs[i].ptdst).len();*/
			}
		}
		return 1;
	}

	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	int i,j;
	float t=0;
	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		if (!is_unused(action->ipart)) i = action->ipart;
		else if (!is_unused(action->partid)) i = action->partid;
		else if (!is_unused(action->point)) {
			for(j=1,i=0;j<=m_nSegs;j++) if ((m_segs[j].pt-action->point).len2()<(m_segs[i].pt-action->point).len2())
				i=j;
			if ((m_segs[i].pt-action->point).len2()>sqr(m_collDist*3) && 
					(i==0 || (m_segs[i].pt-m_segs[i-1].pt^action->point-m_segs[i-1].pt).len2() > sqr(m_collDist)*(m_segs[i].pt-m_segs[i-1].pt).len2()))
				return 0;
			if (i<m_nSegs)
				t = (action->point-m_segs[i].pt)*(m_segs[i+1].pt-m_segs[i].pt).normalized();
		} else
			i = m_nSegs>>1;
		if ((unsigned int)i>(unsigned int)m_nSegs)
			return 0;

		t = max(0.0f,min(1.0f,t));
		Vec3 impulse = action->impulse*((m_nSegs+1)/m_mass);
		if (impulse.len2() > sqr(m_pWorld->m_vars.maxVel))
			impulse.normalize() *= m_pWorld->m_vars.maxVel;
		m_segs[i].vel_ext += impulse*(1.0f-t);
		if (i<m_nSegs)
			m_segs[i+1].vel_ext += impulse*t;

		m_bAwake = 1; m_nSlowFrames = 0;
		m_pWorld->RepositionEntity(this,2);
		m_timeIdle *= isneg(-action->iSource);
		for(i=0;i<m_nColliders;i++)
			m_pColliders[i]->Awake();
		return 1;
	}

	if (_action->type==pe_action_target_vtx::type_id) {
		pe_action_target_vtx *action = (pe_action_target_vtx*)_action;
		WriteLock lock(m_lockUpdate);
		if (!is_unused(action->points) && action->nPoints==m_nSegs+1) {
			int bFar = (m_segs[0].pt-action->points[0]).len2()>sqr(m_length*5);
			for(i=0;i<=m_nSegs;i++)	{
				m_segs[i].ptdst = action->points[i];
				if (!(m_flags & (rope_target_vtx_rel0|rope_target_vtx_rel1)) && (m_flags & pef_disabled || bFar)) 
					m_segs[i].pt=m_segs[i].pt0 = action->points[i];
			}
			if (m_bTargetPoseActive==1)	for(i=0;i<2;i++) if (m_pTiedTo[i]) {
				Vec3 pos; quaternionf q(IDENTITY); float scale;
				if (m_flags & rope_target_vtx_rel0<<i)
					m_ptTiedLoc[i] = action->points[m_nSegs*i];
				else if (!(m_flags & (rope_target_vtx_rel0|rope_target_vtx_rel1))) {
					m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], pos,q,scale, this);
					m_ptTiedLoc[i] = (action->points[m_nSegs*i]-pos)*q;
				}
				//if (i==0 && m_iTiedPart[0]<m_pTiedTo[0]->m_nParts)
				//	m_dir0dst = (action->points[1]-action->points[0])*q;
			}
		}	else if (m_flags & (rope_target_vtx_rel0|rope_target_vtx_rel1)) {
			int iParent = (m_flags & rope_target_vtx_rel1)!=0;
			Vec3 offsParent;
			quaternionf qParent;
			float scaleParent;
			if (m_pTiedTo[iParent]) {
				m_pTiedTo[iParent]->GetLocTransform(m_iTiedPart[iParent], offsParent,qParent,scaleParent, this);
				scaleParent = 1.0f/scaleParent;
				for(i=0;i<=m_nSegs;i++)
					m_segs[i].ptdst = (m_segs[i].pt-offsParent)*qParent*scaleParent;
			}
		}	else for(i=0;i<=m_nSegs;i++) 
			m_segs[i].ptdst = m_segs[i].pt;
		return 1;
	}

	if (_action->type==pe_action_attach_points::type_id) {
		pe_action_attach_points *action = (pe_action_attach_points*)_action;
		if (!action->nPoints) {
			delete[] m_attach; m_attach=0; m_nAttach=0;
		}	else {
			ReallocateList(m_attach,m_nAttach,m_nAttach+action->nPoints);
			CPhysicalEntity *pent = (CPhysicalEntity*)action->pEntity;
			int ipart = 0; 
			if (!is_unused(action->partid)) {
				for(;ipart<pent->m_nParts && pent->m_parts[ipart].id!=action->partid;ipart++);
				if (ipart==pent->m_nParts)
					return 0;
			}
			for(i=0;i<action->nPoints;i++) if ((unsigned int)action->piVtx[i]>m_nSegs)
				return 0;
			Vec3 pos; Quat q; float scale;
			pent->GetLocTransform(ipart, pos,q,scale, this);
			for(i=0;i<action->nPoints;i++) {
				rope_vtx_attach &att = m_attach[m_nAttach+i];
				(att.pent = pent)->AddRef();
				att.ipart = ipart;
				att.ivtx = action->piVtx[i];
				if (action->bLocal && !is_unused(action->points))
					att.ptloc = action->points[i];
				else {
					Vec3 pt = is_unused(action->points) ? m_segs[att.ivtx].pt : action->points[i];
					att.ptloc = (pt-pos)*q;
				}
			}
			m_nAttach += action->nPoints;
		}
		return 1;
	}

	if (_action->type==pe_action_reset::type_id) {
		if (!(m_flags & rope_subdivide_segs)) {
			for(i=0;i<=m_nSegs;i++) {
				m_segs[i].vel.zero();
				if (m_segs[i].pContactEnt) {
					m_segs[i].pContactEnt->Release();
					m_segs[i].pContactEnt = 0;
				}
			}
		}	else {
			for(i=0;i<2;i++) if (m_pTiedTo[i])
				m_pTiedTo[i]->RemoveCollider(this);
			for(i=1;i<m_nVtx-1;i++) if (m_vtx[i].pContactEnt)	{
				m_vtx[i].pContactEnt->RemoveCollider(this);
				m_vtx[i].pContactEnt = 0;
			}
			for(i=0;i<m_nColliders;i++)
				m_pColliders[i]->Release();
			for(i=0;i<m_nSegs;i++)
				m_segs[i].pContactEnt=0, m_segs[i].vel.zero();
			if (m_nSegs>0)
				m_segs[m_nSegs].vel.zero();
			m_nColliders = 0;
		}
		m_bStrained = 0;
		return 1;
	}

	if (_action->type==pe_action_slice::type_id) {
		pe_action_slice *action = (pe_action_slice*)_action;
		if (action->npt==1 && action->pt) {
			float dist = action->pt[0].x + m_length*isneg(action->pt[0].x), seglen;
			for(i=0; i<m_nSegs && dist>(seglen=(m_segs[i+1].pt-m_segs[i].pt).len())*0.5f; i++,dist-=seglen);
		} else if (action->npt==3) {
			triangle tri;
			for(i=0;i<3;i++) tri.pt[i]=action->pt[i];
			tri.n = (tri.pt[1]-tri.pt[0]^tri.pt[2]-tri.pt[0]).normalized();
			prim_inters inters;	bool bHit=false;
			for(i=0;i<m_nSegs;i++) {
				ray aray; aray.origin=m_segs[i].pt; aray.dir=m_segs[i+1].pt-m_segs[i].pt;
				if (ray_tri_intersection(&aray,&tri,&inters)) {
					i += isneg(aray.dir.len2()*0.5f-sqr_signed((inters.pt[0]-aray.origin)*aray.dir));
					bHit=true; break;
				}
			}
			if (!bHit)
				return 1;
		} else
			return 0;
		pe_params_pos pp; GetParams(&pp);
		CRopeEntity *pentDst = (CRopeEntity*)m_pWorld->CreatePhysicalEntity(PE_ROPE,&pp);
		pe_params_flags pf; pf.flags = m_flags;
		pentDst->SetParams(&pf);
		pe_simulation_params sp; GetParams(&sp);
		pentDst->SetParams(&sp);
		pe_params_rope pr; GetParams(&pr);
		MARK_UNUSED pr.nSegments;
		pentDst->SetParams(&pr);
		new(&pr) pe_params_rope;

		if (i==0)
			pr.pEntTiedTo[0] = 0;
		else {
			if (i<m_nSegs) {
				float seglen = m_length/m_nSegs;
				if (pentDst->m_nSegs < m_nSegs-i) {
					if (pentDst->m_segs) delete[] pentDst->m_segs;
					pentDst->m_segs = new rope_segment[m_nSegs-i+1];
				}
				pentDst->m_length = seglen*(pentDst->m_nSegs = m_nSegs-i);
				memcpy(pentDst->m_segs, m_segs+i, (m_nSegs-i+1)*sizeof(rope_segment));
				if (m_nVtx) {
					int nVtx = m_nVtx-m_segs[i].iVtx0+1;
					if (pentDst->m_nVtx < nVtx) {
						if (pentDst->m_vtx) delete[] pentDst->m_vtx;
						if (pentDst->m_vtx1) delete[] pentDst->m_vtx1;
						pentDst->m_vtx = new rope_vtx[nVtx];
						pentDst->m_vtx1 = new rope_vtx[nVtx];
					}
					pentDst->m_nVtx = nVtx;
					memcpy(pentDst->m_vtx, m_vtx+m_segs[i].iVtx0, nVtx*sizeof(rope_vtx));
					m_nVtx = m_segs[i].iVtx0;	
				}
				m_length = seglen*(m_nSegs = i);
				if (pentDst->m_pTiedTo[1] = m_pTiedTo[1])
					m_pTiedTo[1]->AddRef();
				pentDst->m_iTiedPart[1] = m_iTiedPart[1];
				pentDst->m_ptTiedLoc[1] = m_ptTiedLoc[1];

				EventPhysCreateEntityPart epcep;
				epcep.pEntity=this; epcep.pForeignData=m_pForeignData; epcep.iForeignData=m_iForeignData;
				epcep.pEntNew = pentDst;
				m_pWorld->OnEvent(2,&epcep);
				EventPhysUpdateMesh epum;
				epum.pEntity=this; epum.pForeignData=m_pForeignData; epum.iForeignData=m_iForeignData;
				m_pWorld->OnEvent(2,&epum);
			}
			pr.pEntTiedTo[1] = 0;
		}
		SetParams(&pr);

		EventPhysJointBroken epjb;
		epjb.idJoint=0; epjb.bJoint=0; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
		epjb.pEntity[0]=epjb.pEntity[1]=this; epjb.pForeignData[0]=epjb.pForeignData[1]=m_pForeignData; 
		epjb.iForeignData[0]=epjb.iForeignData[1]=m_iForeignData;
		epjb.pt = m_segs[m_nSegs].pt;	epjb.n = m_segs[m_nSegs-1].dir;
		epjb.partid[0] = 1;	epjb.partid[1] = -1;
		epjb.partmat[0]=epjb.partmat[1] = -1;
		m_pWorld->OnEvent(2,&epjb);

		return 1;
	}

	return CPhysicalEntity::Action(_action,1);
}


RigidBody *CRopeEntity::GetRigidBodyData(RigidBody *pbody, int ipart)
{ 
	pbody->pos = (m_segs[ipart].pt+m_segs[ipart+1].pt)*0.5f;
	Vec3 dir = m_segs[ipart+1].pt-m_segs[ipart].pt;
	pbody->q = Quat::CreateRotationV0V1(Vec3(0,0,-1),dir.normalized());
	pbody->v = (m_segs[ipart].vel+m_segs[ipart+1].vel)*0.5f;
	pbody->w = (dir ^ m_segs[ipart+1].vel-m_segs[ipart].vel)/dir.len2();
	pbody->M = m_mass/m_nSegs;
	pbody->Minv = 1.0f/pbody->M;
	return pbody;
}

void CRopeEntity::GetLocTransform(int ipart, Vec3 &offs, quaternionf &q, float &scale, const CPhysicalPlaceholder *trg)	const
{
	scale = 1.0f;
	/*if (m_bTargetPoseActive) {
		q = Quat::CreateRotationV0V1((m_segs[ipart+1].pt-m_segs[ipart].pt).normalized(), (m_segs[ipart+1].ptdst-m_segs[ipart].ptdst).normalized());
		offs = m_segs[ipart].pt-m_segs[ipart].ptdst;
	}	else*/ {
		q = Quat::CreateRotationV0V1(Vec3(0,0,-1), (m_segs[ipart+1].pt-m_segs[ipart].pt).normalized());
		offs = m_segs[ipart].pt;
	}
	m_pWorld->TransformToGrid(this,trg, offs,q);

	/*if (m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1)) {
		int i = (m_flags & rope_target_vtx_rel1)!=0;
		Vec3 offsParent;
		quaternionf qParent;
		float scaleParent;
		if (m_pTiedTo[i]) {
			m_pTiedTo[i]->GetLocTransform(m_iTiedPart[i], offsParent,qParent,scaleParent);
			q = qParent*q;
			offs = qParent*offs*scaleParent + offsParent;
			scale *= scaleParent;
		}
	}*/
}


/*void CRopeEntity::MeshVtxUpdated()
{
	int i;
	box *pbox = &((CSingleBoxTree*)m_pMesh->m_pTree)->m_Box;
	Vec3 axisx,axisy,BBox[2]={Vec3(0),Vec3(0)};

	for(i=0;i<2;i++) m_pMesh->m_pNormals[i+2] = -(m_pMesh->m_pNormals[i] = 
		(m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+1]]-m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3]] ^
		 m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3+2]]-m_pMesh->m_pVertices[m_pMesh->m_pIndices[i*3]]).normalized());

	axisx = m_pMesh->m_pVertices[1].normalized();
	axisy = m_pMesh->m_pVertices[2]+m_pMesh->m_pVertices[3]-m_pMesh->m_pVertices[1];
	(axisy -= axisx*(axisy*axisx)).normalize();
	pbox->Basis.SetFromVectors(axisx,axisy,axisx^axisy);
	for(i=1;i<4;i++) {
		BBox[0] = min(BBox[0],m_pMesh->m_pVertices[i]);
		BBox[1] = max(BBox[1],m_pMesh->m_pVertices[i]);
	}
	pbox->center = (BBox[1]+BBox[0])*0.5f;
	pbox->size = (BBox[1]-BBox[0])*0.5f;
	float e = max(max(pbox->size.x,pbox->size.y),pbox->size.z)*0.01f;
	for(i=0;i<3;i++) pbox->size[i] = max(pbox->size[i],e);
}*/

void CRopeEntity::AllocSubVtx()
{
	if (!m_vtx)	{
		m_vtx = new rope_vtx[m_nVtxAlloc=(m_nSegs+1)*2];
		m_vtx1 = new rope_vtx[m_nVtxAlloc];
	}
	if (m_nVtx==m_nVtxAlloc) {
		rope_vtx *pVtx = m_vtx;
		NO_BUFFER_OVERRUN	// cppcheck-suppress autovarInvalidDeallocation
		memcpy(m_vtx=new rope_vtx[m_nVtxAlloc+=16], pVtx, m_nVtx*sizeof(rope_vtx));	delete[] pVtx;
		pVtx = m_vtx1;
		memcpy(m_vtx1=new rope_vtx[m_nVtxAlloc], pVtx, m_nVtx*sizeof(rope_vtx)); delete[] pVtx;
		if (m_vtxSolver) {
			rope_solver_vtx *pVtxSolver = m_vtxSolver;
			memcpy(m_vtxSolver=new rope_solver_vtx[m_nVtxAlloc], pVtxSolver, m_nVtx*sizeof(rope_solver_vtx));
			delete[] pVtxSolver;
		}
	}
}

void CRopeEntity::FillVtxContactData(rope_vtx *pvtx,int iseg, SRopeCheckPart &cp, geom_contact *pcontact)
{
	pvtx->pt = pcontact->pt-pcontact->dir*pcontact->t;
	if (iseg>=0) {
		float t = (pvtx->pt-m_segs[iseg].pt).len(); 
		t = t/(t+(pvtx->pt-m_segs[iseg+1].pt).len());
		pvtx->vel = m_segs[iseg].vel*(1-t)+m_segs[iseg+1].vel*t;
	}
	pvtx->ncontact = pcontact->n.normalized();
	pvtx->pt += pvtx->ncontact*m_collDist;
	RigidBody body(false), *pbody = cp.pent->GetRigidBodyTrans(&body,cp.ipart,this);
	pvtx->vcontact = pbody->v+(pbody->w^pvtx->pt-pbody->pos);
	pvtx->pContactEnt = cp.pent;
	pvtx->iContactPart = cp.ipart;
}


void CRopeEntity::StartStep(float time_interval)
{
	m_timeStepPerformed = 0;
	m_timeStepFull = time_interval;
	m_posBody[0][0]=m_posBody[0][1]; m_qBody[0][0]=m_qBody[0][1];
	m_posBody[1][0]=m_posBody[1][1]; m_qBody[1][0]=m_qBody[1][1];
}


float CRopeEntity::GetMaxTimeStep(float time_interval)
{
	if (m_timeStepPerformed > m_timeStepFull-0.001f)
		return time_interval;
	return min(min(m_timeStepFull-m_timeStepPerformed,m_maxAllowedStep),time_interval);
}


void CRopeEntity::StepSubdivided(float time_interval, SRopeCheckPart *checkParts,int nCheckParts, float seglen)
{
	int i,j,iStart,iEnd,i1,j1,iter,iMoveEnd[2],ncont,bIncomplete[2],bStrainRechecked=0,bRopeChanged;
	Vec3 dv,center,dir,n;
	float tmax[2],a,b,len2,rsep=m_length>0 ? 1.3f:1.01f;
	RigidBody body(false),*pbody;
	Vec3 dirUnproj[2] = { Vec3(ZERO),Vec3(ZERO) };
	rope_vtx vtxBest;
	intersection_params ip;
	geom_world_data gwd,gwd1;
	CRayGeom aray;
	geom_contact *pcontact;
	WriteLock lockVtx(m_lockVtx);

	if (m_idConstraint && m_pTiedTo[m_iConstraintClient]) {
		pe_action_update_constraint arc;
		arc.idConstraint=m_idConstraint; arc.bRemove=1;
		m_pTiedTo[m_iConstraintClient]->Action(&arc);
		m_idConstraint = 0;
	}

	for(i=0;i<2;i++) if (m_pTiedTo[i])
		m_pTiedTo[i]->RemoveCollider(this);
	if (m_bStrained) for(i=1;i<m_nVtx-1;i++) if (m_vtx[i].pContactEnt)
		m_vtx[i].pContactEnt->RemoveCollider(this);
	for(i=0;i<m_nColliders;i++)
		m_pColliders[i]->Release();
	m_nColliders = 0;

	if (!m_idx)
		m_idx = new int[m_nMaxSubVtx+2];

	RecheckAfterStrain:
	ip.bNoAreaContacts = true;
	ip.bStopAtFirstTri = false;
	ip.iUnprojectionMode = -1;
	ip.vrel_min = 1e-6f;
	aray.m_iCollPriority = 0;
	if (m_pTiedTo[0]) {
		iStart=1; MARK_UNUSED m_segs[0].ncontact; m_segs[0].pContactEnt=0;
	}	else iStart=0;
	if (m_pTiedTo[1]) {
		iEnd = m_nSegs-1; MARK_UNUSED m_segs[m_nSegs].ncontact; m_segs[m_nSegs].pContactEnt=0;
	} else iEnd=m_nSegs;
	bRopeChanged = isneg(sqr(m_collDist*0.1f)-max((m_segs[0].pt-m_segs[0].pt0).len2(), (m_segs[m_nSegs].pt-m_segs[m_nSegs].pt0).len2()));

	for(i=iStart; i<=iEnd; i++) {
		if (!m_segs[i].pContactEnt && !m_segs[min(m_nSegs,i+1)].pContactEnt && m_segs[min(m_nSegs,i+1)].iVtx0-m_segs[i].iVtx0>1)
			++bRopeChanged;
		if (m_segs[i].pContactEnt && !is_unused(m_segs[i].ncontact)) {
			++bRopeChanged;
			pbody = m_segs[i].pContactEnt->GetRigidBodyTrans(&body,m_segs[i].iContactPart,this);
			dv = pbody->v + (pbody->w^m_segs[i].pt-pbody->pos);
			if ((m_segs[i].vel-dv)*m_segs[i].ncontact<m_pWorld->m_vars.minSeparationSpeed) {
				aray.m_ray.origin.zero(); gwd1.offset = m_segs[i].pt;
				aray.m_ray.dir = (aray.m_dirn=-m_segs[i].ncontact)*m_collDist*1.5f;
				gwd.offset = m_segs[i].pContactEnt->m_pos + m_segs[i].pContactEnt->m_qrot*m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].pos;
				gwd.R = Matrix33(m_segs[i].pContactEnt->m_qrot*m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].q);
				gwd.scale = m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].scale;
				if (!((CGeometry*)m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].pPhysGeomProxy->pGeom)->IsAPrimitive() &&
					  m_segs[i].pContactEnt->m_parts[m_segs[i].iContactPart].pPhysGeomProxy->pGeom->Intersect(&aray,&gwd,&gwd1,&ip,pcontact)) {
					m_segs[i].pt = pcontact->pt+(m_segs[i].ncontact = pcontact->n)*m_collDist;
					m_segs[i].vcontact = dv;
					bRopeChanged -= isneg((m_segs[i].pt-m_segs[i].pt0).len2()-sqr(m_collDist*0.1f));
				}	else { MARK_UNUSED m_segs[i].ncontact; m_segs[i].pContactEnt=0; }
			}	else { MARK_UNUSED m_segs[i].ncontact; m_segs[i].pContactEnt=0; }
		}	else { MARK_UNUSED m_segs[i].ncontact; m_segs[i].pContactEnt=0; }

		// cppcheck-suppress nullPointer
		for(j=0;j<nCheckParts;j++) if (!checkParts[j].pGeom->IsAPrimitive()) {//checkParts[j].pent->m_iSimClass!=3) {
			center = (checkParts[j].pent->m_qrot*!checkParts[j].q0)*(m_segs[i].pt0-checkParts[j].pos0) + checkParts[j].pent->m_pos;
			aray.m_ray.origin = (center-checkParts[j].offset)*checkParts[j].R;
			aray.m_ray.dir = (m_segs[i].pt-checkParts[j].offset)*checkParts[j].R-aray.m_ray.origin;
			if (aray.m_ray.dir.len2()==0)//<sqr(0.001f))
				continue;
			aray.m_ray.dir += (aray.m_dirn=aray.m_ray.dir.normalized())*m_collDist;
			if (box_ray_overlap_check(&checkParts[j].bbox,&aray.m_ray)) {
				gwd.offset.zero(); gwd.R.SetIdentity(); gwd.scale=checkParts[j].scale;
				if (ncont = checkParts[j].pGeom->Intersect(&aray,&gwd,0,&ip,pcontact)) {
					for(i1=ncont-1; i1>=0 && pcontact[i1].n*aray.m_dirn>0; i1--);
					if (i1>=0) {
						float gap = min(m_collDist/max(0.3f,-(pcontact[i1].n*aray.m_dirn)), aray.m_ray.dir*aray.m_dirn);
						m_segs[i].pt = checkParts[j].R*(pcontact[i1].pt-aray.m_dirn*gap)+checkParts[j].offset;
						m_segs[i].ncontact = checkParts[j].R*pcontact[i1].n;
						m_segs[i].pContactEnt = checkParts[j].pent;
						m_segs[i].iContactPart = checkParts[j].ipart;
						pbody = checkParts[j].pent->GetRigidBodyTrans(&body,checkParts[j].ipart,this);
						m_segs[i].vcontact = pbody->v + (pbody->w ^ m_segs[i].pt-pbody->pos);
						++bRopeChanged;
					}
				}
			}
		} else {
			contact acontact;
			if (checkParts[j].pGeom->UnprojectSphere(((m_segs[i].pt-checkParts[j].offset)*checkParts[j].R)*checkParts[j].rscale, 
				m_collDist*checkParts[j].rscale, m_collDist*rsep*checkParts[j].rscale, &acontact)) 
			{
				m_segs[i].pt = checkParts[j].R*(acontact.pt*checkParts[j].scale+acontact.n*m_collDist)+checkParts[j].offset;
				m_segs[i].ncontact = checkParts[j].R*acontact.n;
				m_segs[i].pContactEnt = checkParts[j].pent;
				m_segs[i].iContactPart = checkParts[j].ipart;
				pbody = checkParts[j].pent->GetRigidBodyTrans(&body,checkParts[j].ipart,this);
				m_segs[i].vcontact = pbody->v + (pbody->w ^ m_segs[i].pt-pbody->pos);
				++bRopeChanged;
			}
		}
	}
	//if (m_bStrained && !bRopeChanged && m_penaltyScale==0.0f)
	//	goto stepdone;
	ip.iUnprojectionMode = 0;
	ip.vrel_min = 0;
	aray.m_iCollPriority = 10;
	if (m_length==0 && m_nSegs)	{
		for(i=0,seglen=0; i<m_nSegs; i++) seglen += (m_segs[i+1].pt-m_segs[i].pt).GetLengthFast();
		seglen /= i;
	}

	for(i=m_nVtx=0; i<m_nSegs; i++) {
		AllocSubVtx();
		m_vtx[m_segs[i].iVtx0 = m_nVtx++].pt = m_segs[i].pt;
		m_vtx[m_segs[i].iVtx0].vel = m_segs[i].vel;
		m_vtx[m_segs[i].iVtx0].ncontact = m_segs[i].ncontact;
		m_vtx[m_segs[i].iVtx0].vcontact = m_segs[i].vcontact;
		m_vtx[m_segs[i].iVtx0].pContactEnt = m_segs[i].pContactEnt;
		m_vtx[m_segs[i].iVtx0].iContactPart = m_segs[i].iContactPart;
		if (!m_nMaxSubVtx)
			continue;
		AllocSubVtx(); m_nVtx++;
		AllocSubVtx(); m_nVtx++;

		for(j=0;j<nCheckParts;j++) {
			aray.m_ray.origin = (m_segs[i].pt-checkParts[j].offset)*checkParts[j].R;
			aray.m_ray.dir = (m_segs[i+1].pt-checkParts[j].offset)*checkParts[j].R-aray.m_ray.origin;
			checkParts[j].bProcess = box_ray_overlap_check(&checkParts[j].bbox,&aray.m_ray);
		}

		aray.m_ray.origin.zero();	gwd1.offset = m_segs[i].pt;
		m_segs[i].dir=aray.m_dirn = (aray.m_ray.dir = m_segs[i+1].pt-m_segs[i].pt).normalized();
		if (i==0 && m_pTiedTo[0])
			aray.m_ray.origin += (aray.m_ray.dir*=0.5f);
		else if (i==m_nSegs-1 && m_pTiedTo[1])
			aray.m_ray.dir *= 0.5f;
		//if (m_segs[i].ptdst.len2()>0)
			gwd1.v = -m_segs[i].ptdst;
		/*else {
			gwd1.v = m_segs[max(0,i-1)].pt-m_segs[i].pt; gwd1.v -= aray.m_dirn*(aray.m_dirn*gwd1.v);
			dir = m_segs[min(m_nSegs,i+2)].pt-m_segs[i+1].pt; dir -= aray.m_dirn*(aray.m_dirn*dir);
			gwd1.v += dir;
			if (gwd1.v.len2()>sqr(seglen*0.1f))
				gwd1.v.normalize();
			else 
				gwd1.v.zero();
		}*/
		ip.time_interval=ip.maxUnproj = seglen*2;
		iMoveEnd[0]=iMoveEnd[1] = -1;

		for(i1=0,tmax[0]=tmax[1]=0,bIncomplete[0]=bIncomplete[1]=1; i1<2; i1++)	{
			for(j=0,dir.zero(); j<nCheckParts; j++) if (checkParts[j].bProcess) {
				gwd.offset = checkParts[j].offset; gwd.R = checkParts[j].R; gwd.scale = checkParts[j].scale;
				if (ncont=checkParts[j].pGeom->Intersect(&aray,&gwd,&gwd1,&ip,pcontact)) {
					if (pcontact->iUnprojMode==0 && pcontact->t>tmax[i1]+(real)0.00011 && pcontact->t<seglen*2.3f) {
						bIncomplete[i1] = pcontact->n*pcontact->dir>0;
						if (bIncomplete[i1] && i1==1 && tmax[0]==0)
							continue;
						FillVtxContactData(m_vtx+m_nVtx-2+i1,i,checkParts[j],pcontact);
						tmax[i1] = pcontact->t; dirUnproj[i1] = pcontact->dir;
						if (!(pcontact->iFeature[1] & 0x20))
							iMoveEnd[i1] = min(1,pcontact->iFeature[1] & 0x1F);
					}
				}
			}
			if (gwd1.v.len2()==0)
				gwd1.v = dirUnproj[i1];
			gwd1.v.Flip();
		}
		if (tmax[0]+tmax[1]==0) {
			m_nVtx-=2; continue;
		}
		if (tmax[1]>0 && (tmax[1]<tmax[0]*0.5f && bIncomplete[0]==bIncomplete[1] || bIncomplete[0] && !bIncomplete[1]))	{
			m_vtx[m_nVtx-2]=m_vtx[m_nVtx-1]; iMoveEnd[0]=iMoveEnd[1]; 
			tmax[0]=tmax[1]; dirUnproj[0]=dirUnproj[1];
		}
		if (iMoveEnd[0]>=0)	{
			m_segs[i+iMoveEnd[0]].pt -= dirUnproj[0]*tmax[0];
			m_segs[i].dir = (m_segs[i+1].pt-m_segs[i].pt).normalized();
			m_vtx[m_nVtx-3].pt = m_segs[i].pt;
		}
		m_vtx[m_nVtx-1].pt = m_segs[i+1].pt;
		dir = dirUnproj[0];

		for(iter=0; m_nVtx-m_segs[i].iVtx0-2<m_nMaxSubVtx && iter<3; iter++) {
			for(i1=0; i1<m_nVtx-m_segs[i].iVtx0-1; i1++) m_idx[i1] = m_segs[i].iVtx0+i1;
			for(i1=0; i1<m_nVtx-m_segs[i].iVtx0-2; i1++) for(j1=m_nVtx-m_segs[i].iVtx0-2; j1>i1; j1--) 
				if ((m_vtx[m_idx[j1]+1].pt-m_vtx[m_idx[j1]].pt).len2() > (m_vtx[m_idx[j1-1]+1].pt-m_vtx[m_idx[j1-1]].pt).len2())
					j=m_idx[j1-1], m_idx[j1-1]=m_idx[j1], m_idx[j1]=j;
			for(i1=0; i1<m_nVtx-m_segs[i].iVtx0-1; i1++) {
				// unproject the next longest subseg until some unprojection is found
				aray.m_dirn = (aray.m_ray.dir = m_vtx[m_idx[i1]+1].pt-m_vtx[m_idx[i1]].pt).normalized();
				gwd1.offset = m_vtx[m_idx[i1]].pt;
				gwd1.v = ((aray.m_dirn^dir)^aray.m_dirn).normalized();
				for(j=0,tmax[0]=0; j<nCheckParts; j++) if (checkParts[j].bProcess) {
					gwd.offset = checkParts[j].offset; gwd.R = checkParts[j].R; gwd.scale = checkParts[j].scale;
					if (checkParts[j].pGeom->Intersect(&aray,&gwd,&gwd1,&ip,pcontact)) {
						float rng[2] = { seglen*(1.5f*isneg(i-iStart)-1), seglen*(2-1.5f*isneg(iEnd-1-i)) };
						if (pcontact->t>tmax[0] && inrange(m_segs[i].dir*(pcontact->pt-m_segs[i].pt),rng[0],rng[1])) 
							FillVtxContactData(&vtxBest,i,checkParts[j],pcontact), tmax[0]=pcontact->t;
					}
				}
				if (tmax[0]>0) {
					// if a new point is close to an existing one, replace the old one (but if the old one is an end, skip the new)
					j1 = m_idx[i1]+1;
					if ((m_vtx[m_idx[i1]].pt-vtxBest.pt).len2()<sqr(seglen*0.05f)) {
						if (m_idx[i1]==m_segs[i].iVtx0)
							continue;
						j1 = m_idx[i1];
					} else if ((m_vtx[m_idx[i1]+1].pt-vtxBest.pt).len2()>sqr(seglen*0.05f)) {
						AllocSubVtx(); m_nVtx++; iter=0;
						memmove(m_vtx+m_idx[i1]+2, m_vtx+m_idx[i1]+1, (m_nVtx-m_idx[i1]-2)*sizeof(rope_vtx));
					}	else if (m_idx[i1]==m_nVtx-2)
						continue;
					m_vtx[j1] = vtxBest;
					break;
				}
			}
			if (i1==m_nVtx-m_segs[i].iVtx0-1)
				break;
		}
		m_nVtx--;
	}
	m_vtx[m_segs[i].iVtx0 = m_nVtx++].pt = m_segs[i].pt;
	m_vtx[m_segs[i].iVtx0].vel = m_segs[i].vel;
	m_vtx[m_segs[i].iVtx0].ncontact = m_segs[i].ncontact;
	m_vtx[m_segs[i].iVtx0].vcontact = m_segs[i].vcontact;
	m_vtx[m_segs[i].iVtx0].pContactEnt = m_segs[i].pContactEnt;
	m_vtx[m_segs[i].iVtx0].iContactPart = m_segs[i].iContactPart;
	for(i=0;i<m_nVtx-1;i++)
		m_vtx[i].dir = (m_vtx[i+1].pt-m_vtx[i].pt).normalized();
	m_vtx[i].dir = m_vtx[max(0,i-1)].dir;
	for(i=0;i<m_nVtx;i++)
		m_bHasContacts |= !is_unused(m_vtx[i].ncontact);
	for(i=0;i<2;i++) if (m_pTiedTo[i]) {
		m_vtx[i*(m_nVtx-1)].pContactEnt = m_pTiedTo[i];
		m_vtx[i*(m_nVtx-1)].iContactPart = m_iTiedPart[i];
		m_vtx[i*(m_nVtx-1)].ncontact = m_vtx[i*(m_nVtx-1)].dir*(1-i*2);
	}

	if (!m_pTiedTo[0] || !m_pTiedTo[1])
		m_bStrained = 0;
	else {
		for(i=0,len2=0,m_bStrained=1; i<m_nVtx-1; ) {
			for(j=i++,b=0; b+=(m_vtx[i-1].pt-m_vtx[i].pt).len(), 
										 i<m_nVtx-1 && (is_unused(m_vtx[i].ncontact) || 
										 sqr_signed(m_vtx[i].ncontact*(m_vtx[i-1].dir-m_vtx[i].dir))<(m_vtx[i-1].dir-m_vtx[i].dir).len2()*0.01f); i++);
			len2 += a=(m_vtx[i].pt-m_vtx[j].pt).len();
			//m_bStrained &= isneg(min(1e-6f-m_length, b-a*1.05f));
			dir = m_vtx[i-1].dir-m_vtx[i].dir;
			if (dir.len2()>sqr(0.3f)) {
				Vec3 axis=m_vtx[i-1].dir^m_vtx[i].dir; n=m_vtx[i].ncontact;
				if ((a=axis.len2())>sqr(0.2f))
					n -= axis*(n*axis);
				else a=1.0f;
				if (i<m_nVtx-1 && sqr_signed(n*dir)*(1+sqr(m_friction))<dir.len2()*n.len2()*sqr(a))
					m_bStrained = 0;
			}
		}
		m_bStrained &= isneg(m_length*1.01f-len2);
		if (m_bStrained && m_length>0) {
			for(i=1;i<m_nVtx-2;i++) if (!is_unused(m_vtx[i].ncontact) && (!is_unused(m_vtx[i+1].ncontact) || !is_unused(m_vtx[i-1].ncontact)) && m_vtx[i].ncontact*(m_vtx[i-1].dir-m_vtx[i].dir)<0.01f) {
				MARK_UNUSED m_vtx[i].ncontact; m_vtx[i].pContactEnt=0;	
			}
			for(i=0,len2=0; i<m_nVtx-1;) {
				for(iStart=i++; i<m_nVtx-1 && is_unused(m_vtx[i].ncontact); i++);
				len2 += (m_vtx[i].pt-m_vtx[iStart].pt).len();
			}
			for(a=0,i=j=ncont=0,seglen=len2/m_nSegs,iEnd=m_nVtx-1,m_nVtx=0; i<iEnd; ncont++) {
				for(iStart=i++; is_unused(m_vtx[i].ncontact) && i<iEnd; i++);
				dir = m_vtx[i].pt-m_vtx[iStart].pt; 
				if ((b=dir.len())>0.0001f)
					dir /= b;
				else
					dir = (m_vtx[min(m_nSegs-1,i+1)].pt-m_vtx[min(0,i-1)].pt).normalized();
				if (m_nVtx>0)
					m_vtx1[m_nVtx-1].dir = dir;
				for(; a<b+seglen*0.01f && j<=m_nSegs; m_nVtx++,a+=seglen) {
					AllocSubVtx();
					m_vtx1[m_nVtx].pt = m_vtx[iStart].pt+(m_vtx1[m_nVtx].dir=dir)*a; 
					MARK_UNUSED m_vtx1[m_nVtx].ncontact; m_vtx1[m_nVtx].pContactEnt=0;
					m_vtx1[m_nVtx].vel.zero();
					m_segs[j++].iVtx0 = m_nVtx;
				}
				m_nVtx -= isneg(seglen*0.98f-(a-=b));
				AllocSubVtx();
				m_vtx1[m_nVtx++] = m_vtx[i];
			}
			for(;j<=m_nSegs;j++)
				m_segs[j].iVtx0 = m_nVtx-1;
			rope_vtx *pvtx=m_vtx; m_vtx=m_vtx1; m_vtx1=pvtx;
			for(i=1;i<m_nSegs;i++) {
				m_segs[i].ncontact = m_vtx[m_segs[i].iVtx0].ncontact;
				m_segs[i].pContactEnt = m_vtx[m_segs[i].iVtx0].pContactEnt;
				m_segs[i].iContactPart = m_vtx[m_segs[i].iVtx0].iContactPart;
			}
			if (ncont<m_nFragments && !bStrainRechecked)	{
				m_nFragments=ncont; bStrainRechecked=1; 
				for(i=0;i<=m_nSegs;i++)
					m_segs[i].pt0=m_segs[i].pt, m_segs[i].pt=m_vtx[m_segs[i].iVtx0].pt;
				goto RecheckAfterStrain;
			}
			m_nFragments = ncont;
		}
	}
	//stepdone:
	if (m_minSegLen>0) for(i=1;i<m_nSegs;i++) 
		if (m_segs[i+1].iVtx0==m_segs[i].iVtx0+1 && (m_segs[i+1].pt-m_segs[i].pt).len2()<sqr(m_minSegLen)) {
			for(j=i+1;j<=m_nSegs;j++) --m_segs[j].iVtx0;
			memmove(m_segs+i, m_segs+i+1, (m_nSegs-i)*sizeof(m_segs[0]));
			memmove(m_vtx+m_segs[i].iVtx0, m_vtx+m_segs[i].iVtx0+1, (m_nVtx-1-m_segs[i].iVtx0)*sizeof(m_vtx[0]));
			--m_nVtx; --m_nSegs;
		}
	if (m_bStrained) {
		m_pTiedTo[0]->AddCollider(this); AddCollider(m_pTiedTo[0]);
		m_pTiedTo[1]->AddCollider(this); AddCollider(m_pTiedTo[1]);
		for(i=1;i<m_nVtx-1;i++) if (m_vtx[i].pContactEnt)
			m_vtx[i].pContactEnt->AddCollider(this), AddCollider(m_vtx[i].pContactEnt);
		for(i=j=iter=0;i<m_nColliders;i++)
			j+=iszero(m_pColliders[i]->m_iSimClass-1), iter+=iszero(m_pColliders[i]->m_iSimClass-2);
		if (j>0 && iter==0)	{
			if (++m_nSleepingNeighboursFrames==3 && m_nSlowFrames==0)	
				for(i=0;i<m_nColliders;i++) if (m_pColliders[i]->m_iSimClass==1)
					m_pColliders[i]->Awake();
			} else
				m_nSleepingNeighboursFrames = 0;
		if (m_pWorld->m_bWorldStep!=2) {
			int nIndep=0,nRigid=0;
			for(i=0;i<m_nColliders;i++) {
				nIndep += (j = isneg(2-m_pColliders[i]->m_iSimClass));
				nRigid += (isneg(-m_pColliders[i]->m_iSimClass)-j)*isneg(1e-10f-m_pColliders[i]->GetMassInv());
			}
			if (nIndep) {
				m_pTiedTo[0]->Awake(); m_pTiedTo[1]->Awake();
				if (!nRigid) {
					InitContactSolver(time_interval);
					RegisterContacts(time_interval,1);
					float Ebefore=m_energy, Eafter=0.0f, damping=1.0f;
					for(i=0;i<m_nColliders;i++) if (m_pColliders[i]->m_iSimClass>2)
						Ebefore += m_pColliders[i]->CalcEnergy(0.0f);
					for(i=m_nColliders-1;i>=0;i--) if (m_pColliders[i]->m_iSimClass>2)
						m_pColliders[i]->RegisterContacts(time_interval,4);
					entity_contact **pContacts; int nContacts=0;
					InvokeContactSolver(time_interval,&m_pWorld->m_vars,Ebefore, pContacts,nContacts);
					for(i=0;i<m_nColliders;i++) if (m_pColliders[i]->m_iSimClass>2)
						Eafter += m_pColliders[i]->CalcEnergy(0.0f);
					if (Eafter>Ebefore)
						damping = sqrt_tpl(Ebefore/Eafter);
					for(entity_contact *pContact=m_pContact; pContact; pContact=pContact->next)	for(j=0;j<2;j++) 
						if (pContact->pent[j]->m_parts[pContact->ipart[j]].flags & geom_monitor_contacts)
							pContact->pent[j]->OnContactResolved(pContact,j,-1);
					for(i=m_nColliders-1;i>=0;i--) if (m_pColliders[i]->m_iSimClass>2)
						m_pColliders[i]->Update(time_interval,damping);
				}
			}
			Update(time_interval,1.0f);
		}
	} else for(i=0;i<=m_nSegs;i++) if (m_segs[i].pContactEnt)
		m_segs[i].pContactEnt->m_flags |= pef_always_notify_on_deletion;

	for(i=0;i<m_nSegs;i++)
		m_segs[i].dir = (m_segs[i+1].pt-m_segs[i].pt).normalized();
	for(i=0;i<m_nVtx;i++)
		m_vtx[i].pt0 = m_vtx[i].pt;
	m_nVtx0 = m_nVtx;
}


inline bool skip_vtx(rope_vtx *pvtx) {
	return is_unused(pvtx[0].ncontact) || !(pvtx[0].pt-pvtx[-1].pt).len2() || ((pvtx[-1].dir-pvtx[0].dir)*pvtx[0].ncontact)*pvtx[0].dP<=0;
}

void CRopeEntity::ZeroLengthStraighten(float time_interval)
{
	int i,j,iseg,jseg,imin=-1,imax=-1;//,nVtx,nSegs,nExtraSegs,nExtraSegsCur;
	float len,seglen,rdt=1.0f/time_interval;//,rseglen,curlen;
	if (!(m_flags & rope_subdivide_segs))
		return;
	if (!m_bContactsRegistered)
		for(i=0;i<m_nVtx;i++) m_vtx[i].dP=1.0f;
	m_bContactsRegistered = 0;
	
	for(i=0,len=0; i<m_nVtx-1; len+=(m_vtx[j].pt-m_vtx[i].pt).len(),i=j)
		for(j=i+1; j<m_nVtx-1 && skip_vtx(m_vtx+j); j++);
	if (!len)
		return;
	float seglenMin=len, seglenMax=0;

	for(i=iseg=jseg=0; i<m_nVtx-1; i=j) {
		for(j=i+1; j<m_nVtx-1 && skip_vtx(m_vtx+j); j++);
		for(; m_segs[iseg].iVtx0<i; iseg++);
		for(; jseg<m_nSegs && m_segs[jseg+1].iVtx0<=j; jseg++);
		Vec3 seg=m_vtx[j].pt-m_vtx[i].pt, dir=seg.normalized();
		float end0=(m_segs[iseg].pt-m_vtx[i].pt).len(), end1=(m_segs[jseg].pt-m_vtx[j].pt).len();
		seglen = (dir*seg-end0-end1)/max(1,m_segs[jseg].iVtx0-m_segs[iseg].iVtx0);
		end0 += (seglen-end0)*iszero(m_segs[iseg].iVtx0-i);
		int i0=max(i+1,m_segs[iseg].iVtx0), i1=min(j-1,m_segs[jseg].iVtx0), imid=-1;
		for(int ivtx=i0; ivtx<=i1; ivtx++,end0+=seglen) {
			m_vtx[ivtx].vel = (m_vtx[i].pt+dir*end0 - m_vtx[ivtx].pt)*rdt;
			m_vtx[ivtx].vcontact.zero();
			for(; m_segs[iseg].iVtx0<ivtx; iseg++);
			imid += iseg+1 & -(iszero(m_segs[iseg].iVtx0-ivtx) & iszero(m_segs[iseg+1].iVtx0-ivtx-1) & isneg(ivtx-i1) & isneg(i0-ivtx)) & imid>>31;
		}
		if (imid>=0) {
			if (m_segs[imid].iVtx0<i1-1 && seglen<seglenMin)
				imin=imid, seglenMin=seglen;
			if (seglen>seglenMax)
				imax=imid, seglenMax=seglen;
		}
	}
	for(i=1;i<m_nSegs;i++)
		m_segs[i].vel = m_vtx[m_segs[i].iVtx0].vel;

	if ((imin|imax)>=0 && imin!=imax && max(imin,imax)<m_nSegs && (seglenMin*m_nSegs<len*0.7f || seglenMax*m_nSegs>len*1.3f)) {
		WriteLock lock0(m_lockVtx),lock1(m_lockUpdate);
		i=m_segs[imin].iVtx0; memmove(m_vtx+i, m_vtx+i+1, (m_nVtx-1-i)*sizeof(m_vtx[0]));
		memmove(m_segs+imin, m_segs+imin+1, (m_nSegs-1-imin)*sizeof(m_segs[0]));
		for(j=imin;j<m_nSegs;j++) m_segs[j].iVtx0--;
		imax -= isneg(imin-imax);
		i=m_segs[imax].iVtx0; memmove(m_vtx+i+2, m_vtx+i+1, (m_nVtx-2-i)*sizeof(m_vtx[0]));
		memmove(m_segs+imax+2, m_segs+imax+1, (m_nSegs-2-imax)*sizeof(m_segs[0]));
		for(j=imax+2;j<=m_nSegs;j++) m_segs[j].iVtx0++;
		m_segs[imax+1].pt=m_vtx[i+1].pt = (m_vtx[i].pt+m_vtx[i+1].pt+(m_vtx[i].vel+m_vtx[i+2].vel)*time_interval)*0.5f;
		m_vtx[i+1].vel.zero(); m_vtx[i+1].vcontact.zero(); m_segs[imax+1].vel.zero(); m_segs[imax+1].vcontact.zero();
		MARK_UNUSED m_vtx[i+1].ncontact; m_segs[imax+1].pContactEnt=0;
	}

	/*if (j>i+1)
		curlen = 1.0f/(float)(j-i);
	for(int iStart=i++; i<j; i++)	{
		m_vtx[i].vel = (m_vtx[iStart].pt+(m_vtx[j].pt-m_vtx[iStart].pt)*(i-iStart)*curlen - m_vtx[i].pt)*rdt;
		m_vtx[i].vcontact.zero();
	}*/
	/*rseglen = 1/(seglen=len/m_nSegs);
	for(i=iseg=nExtraSegs=0; i<m_nVtx-1; i=j,iseg=jseg) {
		for(j=i+1; j<m_nVtx-1 && skip_vtx(m_vtx+j); j++);
		for(; m_segs[iseg].iVtx0<i; iseg++,nExtraSegs++);
		for(jseg=iseg+1; jseg<m_nSegs && m_segs[jseg+1].iVtx0<=j; jseg++);
		int nsegs = float2int(((m_vtx[j].pt-m_vtx[i].pt).len()-(m_segs[iseg].pt-m_vtx[i].pt).len()-(m_segs[jseg].pt-m_vtx[j].pt).len())*rseglen);
		nExtraSegs += max(0, (nsegs|jseg-iseg>>31)-iszero(m_segs[iseg].iVtx0-i));
	}
	nExtraSegs -= m_nSegs;

	for(i=iseg=jseg=nVtx=nSegs=0; i<m_nVtx-1; i=j,iseg=jseg,len-=curlen,nExtraSegs-=nExtraSegsCur) {
		for(j=i+1; j<m_nVtx-1 && skip_vtx(m_vtx+j); j++);
		for(; m_segs[iseg].iVtx0<i; iseg++);
		for(; jseg<m_nSegs && m_segs[jseg+1].iVtx0<=j; jseg++);
		Vec3 seg=m_vtx[j].pt-m_vtx[i].pt, dir=seg.normalized();	curlen = dir*seg;
		float end0=(m_segs[iseg].pt-m_vtx[i].pt).len(), end1=(m_segs[jseg].pt-m_vtx[j].pt).len();
		int nsegs = float2int((curlen-end0-end1)*rseglen)-(nExtraSegsCur = len>0 ? float2int(nExtraSegs*curlen/len) : 0) | jseg-iseg>>31;
		seglen = (curlen-end0-end1)/max(1,nsegs);
		AllocSubVtx(); (m_vtx1[nVtx++] = m_vtx[i]).dir = dir;
		int j1 = iszero(m_segs[iseg].iVtx0-i); end0 += (seglen-end0)*j1;
		for(; j1<=nsegs; j1++,nVtx++,end0+=seglen) {
			AllocSubVtx(); memset(m_vtx1+nVtx,0,sizeof(rope_vtx));
			m_vtx1[nVtx].pt = m_vtx[i].pt+(m_vtx1[nVtx].dir=dir)*end0;
			MARK_UNUSED m_vtx1[nVtx].ncontact; m_vtx1[nVtx].vel.zero();
			m_vtx1[nSegs++].iContactPart |= nVtx<<16;
		}
	}
	AllocSubVtx(); m_vtx1[nVtx++]=m_vtx[m_nVtx-1];
	
	WriteLock lock(m_lockVtx);
	m_segs[nSegs].iVtx0 = nVtx;
	rope_vtx *pvtx=m_vtx; m_vtx=m_vtx1; m_vtx1=pvtx; m_nVtx=nVtx;
	for(i=1;i<m_nSegs;i++) {
		m_segs[i].iVtx0=j = m_vtx[i].iContactPart>>16; m_vtx[i].iContactPart &= 0xffff;
		if (!is_unused(m_vtx[j].ncontact)) {
			m_segs[i].pContactEnt = m_vtx[j].pContactEnt; m_segs[i].ncontact = m_vtx[j].ncontact;
			m_segs[i].pt = m_vtx[j].pt; m_segs[i].vel = m_vtx[j].vel;
		} else {
			m_segs[i].pContactEnt = 0;
			m_segs[i].vel = (m_vtx[j].pt-m_segs[i].pt)*rdt;
		}
	}*/
}

float CRopeEntity::Solver(float time_interval, float seglen)
{
	int i,j,iStart,iter;
	float E,Ebefore,k;

	if (m_length==0.0f && !(m_flags & rope_subdivide_segs)) {
		for(i=0;i<=m_nSegs;i++) m_segs[i].vel.zero();
		return 0.0f;
	}

	for(i=0,Ebefore=0;i<=m_nSegs;i++)
		Ebefore += m_segs[i].vel.len2();

	if (m_bHasContacts+m_nAttach || m_flags & rope_subdivide_segs) {
		CRY_PROFILE_REGION(PROFILE_PHYSICS, "Rope solver MC");
		int bBounced; iter=m_maxIters;
		float vrel,vreq,dPtang;
		Vec3 dp;

		if (!(m_flags & rope_subdivide_segs)) {
			for(i=0;i<=m_nSegs;i++) 
				m_segs[i].kdP=1, m_segs[i].dP=0;
			for(i=0;i<2;i++) m_segs[i*m_nSegs].kdP = !m_pTiedTo[i];
			for(i=0;i<m_nAttach;i++) m_segs[m_attach[i].ivtx].kdP = 0;

			do { // solve for velocities using relaxation solver with friction
				for(i=bBounced=0;i<m_nSegs;i++,iter--) {
					float denom = 1.5f-(m_segs[i].kdP+m_segs[i+1].kdP)*0.5f;
					if (fabsf(vrel=(m_segs[i+1].vel-m_segs[i].vel)*m_segs[i].dir) > seglen*0.005f) {
						m_segs[i].vel += m_segs[i].dir*(vrel*m_segs[i].kdP*denom);
						m_segs[i+1].vel -= m_segs[i].dir*(vrel*m_segs[i+1].kdP*denom);
						bBounced++;
					}
					if (m_segs[i].pContactEnt) {
						dp = m_segs[i].vel*(1.0f-m_segs[i].tcontact)+m_segs[i+1].vel*m_segs[i].tcontact-m_segs[i].vcontact;
						if ((vrel=dp*m_segs[i].ncontact) < (vreq=m_segs[i].vreq)-seglen*0.005f) {
							if (m_friction>0.01f) {
								m_segs[i].dP += (vreq-vrel)*m_friction;
								m_segs[i].dP -= dPtang=sqrt_tpl(max(0.0001f,dp.len2()-sqr(vrel)));
								if (max(m_segs[i].dP, 1e-10f-fabs_tpl(vrel)) < 0) {	// friction cannot stop sliding
									dp += (dp-m_segs[i].ncontact*vrel)*(m_segs[i].dP/dPtang); // remove part of dp that friction cannot stop
									//dp *= (vrel-vreq)/(m_segs[i].ncontact*dp); // apply impulse along dp so that it stops normal component
									m_segs[i].dP = 0;
								}
								dp -= m_segs[i].ncontact*vreq;
							} else
								dp = m_segs[i].ncontact*(vrel-vreq);
							m_segs[i].vel -= dp*(1.0f-m_segs[i].tcontact)*m_segs[i].kdP;
							m_segs[i+1].vel -= dp*m_segs[i].tcontact*m_segs[i+1].kdP;
							bBounced += 4;
						}
					}
				}
				for(i=0;i<m_nAttach;i++)
					m_segs[m_attach[i].ivtx].vel = m_attach[i].v;
			} while (bBounced && (iter-=bBounced)>0);
		}
		else if (!m_bStrained || m_length==0) {
			float e=m_pWorld->m_vars.accuracyMC;
			for(i=0;i<m_nVtx;m_vtx[i++].kdP=1); 
			for(i=0;i<2;i++) m_vtx[i*(m_nVtx-1)].kdP = !m_pTiedTo[i];
			for(i=0;i<m_nAttach;i++) m_vtx[m_segs[m_attach[i].ivtx].iVtx0].kdP = 0;
			float length = m_length;
			if (m_length==0) for(i=0,length=0;i<m_nVtx-1;i++)
				length += (m_vtx[i+1].pt-m_vtx[i].pt).GetLengthFast();
			float ks = m_stiffness/length;
			for(i=0;i<m_nSegs;i++) {
				for(j=m_segs[i].iVtx0,k=0;j<m_segs[i+1].iVtx0;j++) 
					k += m_vtx[j].dir*(m_vtx[j+1].pt-m_vtx[j].pt);
				m_segs[i].vreq = ks*(m_length-min(length*1.5f,k*m_nSegs));
			}
			for(i=0;i<m_nSegs;i++) 
				Ebefore += sqr(m_segs[i].vreq*((m_segs[i+1].pt-m_segs[i].pt)*m_segs[i].dir))+m_segs[i].vel.len()*fabs_tpl(m_segs[i].vreq)*2;
			for(i=0;i<m_nVtx;++i) 
				m_vtx[i].dP=0;
			do {
				for(i=bBounced=iStart=0; i<m_nVtx; i++,iter--) {
					if (i<m_nVtx-1) {
						vreq = ((m_vtx[i+1].pt-m_vtx[i].pt)*m_vtx[i].dir)*m_segs[iStart].vreq;
						iStart += isneg(m_segs[iStart+1].iVtx0-2-i);
						if (fabs_tpl(vrel=(m_vtx[i+1].vel-m_vtx[i].vel)*m_vtx[i].dir-vreq) > e) {
							float denom = 1.5f-(m_vtx[i].kdP+m_vtx[i+1].kdP)*0.5f;
							m_vtx[i].vel   += m_vtx[i].dir*(vrel*m_vtx[i].kdP*denom);
							m_vtx[i+1].vel -= m_vtx[i].dir*(vrel*m_vtx[i+1].kdP*denom);
							bBounced++;
						}
					}
					if (!is_unused(m_vtx[i].ncontact)) {
						dp = m_vtx[i].vel-m_vtx[i].vcontact;
						if ((vrel=dp*m_vtx[i].ncontact) < -e) {
							if (m_friction>0.01f) {
								m_vtx[i].dP -= dPtang=sqrt_tpl(max(0.0f,dp.len2()-sqr(vrel)));
								if (m_vtx[i].dP-vrel*m_friction < 0) {	// friction cannot stop sliding
									dp += (dp-m_vtx[i].ncontact*vrel)*((m_vtx[i].dP-vrel*m_friction)/dPtang); // remove part of dp that friction cannot stop
									//dp *= vrel/(m_vtx[i].ncontact*dp); // apply impulse along dp so that it stops normal component
									m_vtx[i].dP = 0;
								} else 
									m_vtx[i].dP -= vrel*m_friction;
							} else
								dp = m_vtx[i].ncontact*vrel;
							m_vtx[i].vel -= dp;
							bBounced += 4;
						}
					}
				}
			} while (bBounced && (iter-=bBounced)>0);

			if (m_vtx) for(i=0;i<=m_nSegs;i++)
				m_segs[i].vel = m_vtx[m_segs[i].iVtx0].vel;
		}
	}	else {
		CRY_PROFILE_REGION(PROFILE_PHYSICS, "Rope solver CG");
		m_segs[0].vcontact.x = 0;
		m_segs[0].vcontact.y = m_segs[1].dir*m_segs[0].dir;
		m_segs[0].vcontact.z = (m_segs[0].vel-m_segs[1].vel)*m_segs[0].dir;
		if (m_pTiedTo[0])
			m_segs[0].vcontact *= 2.0f;
		for(i=1;i<m_nSegs;i++) {
			m_segs[i].vcontact.x = m_segs[i-1].dir*m_segs[i].dir;	// a
			m_segs[i].vcontact.y = m_segs[i+1].dir*m_segs[i].dir;	// c
			m_segs[i].vcontact.z = (m_segs[i].vel-m_segs[i+1].vel)*m_segs[i].dir;	// d; b=-2 for all i
		}
		m_segs[m_nSegs-1].vcontact.y = 0;
		if (m_pTiedTo[1])
			m_segs[m_nSegs-1].vcontact *= 2.0f;
		for(i=1,m_segs[0].vcontact*=-0.5f; i<m_nSegs; i++) {
			k = -2.0f-m_segs[i].vcontact.x*m_segs[i-1].vcontact.y;
			k = fabs_tpl(k)>1e-10f ? 1.0f/k : 0.0f;
			m_segs[i].vcontact.y *= k;
			(m_segs[i].vcontact.z -= m_segs[i-1].vcontact.z*m_segs[i].vcontact.x) *= k;
		}
		m_segs[m_nSegs-1].dP = m_segs[m_nSegs-1].vcontact.z;
		for(i=m_nSegs-2;i>=0;i--)
			m_segs[i].dP = m_segs[i].vcontact.z-m_segs[i].vcontact.y*m_segs[i+1].dP;

		if (!m_pTiedTo[0]) m_segs[0].vel += m_segs[0].dir*m_segs[0].dP;
		if (!m_pTiedTo[1]) m_segs[m_nSegs].vel -= m_segs[m_nSegs-1].dir*m_segs[m_nSegs-1].dP;
		m_segs[1].vel -= m_segs[0].dir*m_segs[0].dP;
		m_segs[m_nSegs-1].vel += m_segs[m_nSegs-1].dir*m_segs[m_nSegs-1].dP;
		for(i=1;i<m_nSegs-1;i++) {
			m_segs[i].vel += m_segs[i].dir*m_segs[i].dP;
			m_segs[i+1].vel -= m_segs[i].dir*m_segs[i].dP;
		}
	}

	if (m_length>0 || m_nVtx==0) {
		for(i=0,E=0;i<=m_nSegs;i++) E += m_segs[i].vel.len2();
		if (E>Ebefore && E>m_Emin) {
			k = sqrt_tpl(Ebefore/E);
			for(i=0;i<=m_nSegs;i++) m_segs[i].vel*=k;
			E = Ebefore;
		}
	} else {
		for(i=0,E=0;i<=m_nSegs;i++) E += (m_segs[i].pt-m_segs[i].pt0).len2();
		E /= sqr(time_interval);
	}
	return E;
}


void CRopeEntity::ApplyStiffness(float time_interval, int bTargetPoseActive, const quaternionf &qtv,const Vec3 &offstv,float scaletv)
{
	int i;
	Vec3 dv,dw,dir0dst,dir1dst,dir0src,dir1src,axis0src,axis1src,axis0dst,axis1dst,vrel;
	float a,b,angleSrc,angleDst,rnSegs=1.0f/m_nSegs;

	if (bTargetPoseActive==1) {
		for(i=0; i<m_nSegs; i++) {
			dv = (qtv*m_segs[i+1].ptdst*scaletv+offstv-m_segs[i+1].pt)*m_stiffnessAnim*(1-m_stiffnessDecayAnim*(i+1)*rnSegs);
			a = max(0.0f, 1-m_dampingAnim*time_interval);
			m_segs[i+1].vel = m_segs[i+1].vel*a + dv*(1-a);
		}
	}	else if (bTargetPoseActive==2) {
		// dir0src, dir0dst - point to attached object's center
		// axis0src=axis0dst = some axis perpendicular to dir0src
		if (m_stiffnessAnim>0) {
			if (m_pTiedTo[0])	{
				dir0dst = (m_segs[0].pt-offstv).normalized();
				dir1dst = qtv*(m_segs[1].ptdst-m_segs[0].ptdst);
				if (sqr(dir0dst*dir1dst)>dir1dst.len2()*sqr(0.985f))
					dir0dst.zero()[idxmin3(dir1dst.abs())] = 1;
				dir0src = dir0dst;
				axis0src=axis0dst = qtv*(!qtv*dir0src).GetOrthogonal().normalized();
			} else {
				dir0src.Set(0,0,1); dir0dst = dir0src;
				axis0src.Set(0,1,0); axis0dst = axis0src;
			}

			for(i=0; i<m_nSegs; i++,dir0src=dir1src,dir0dst=dir1dst,axis0src=axis1src,axis0dst=axis1dst) {
				dir1src = (m_segs[i+1].pt-m_segs[i].pt).normalized(); 
				dir1dst = qtv*(m_segs[i+1].ptdst-m_segs[i].ptdst).normalized();
				axis1src = dir0src^dir1src; axis1dst = dir0dst^dir1dst;
				angleSrc = atan2_tpl(a=axis1src.len(), dir0src*dir1src);
				angleDst = atan2_tpl(b=axis1dst.len(), dir0dst*dir1dst);
				// rotate around axis1src to bring angleSrc to angleDst
				axis1src = a > m_length*0.001f ? axis1src/a : axis0src; 
				axis1dst = b > m_length*0.001f ? axis1dst/b : axis0dst;
				if (fabs_tpl(angleDst-angleSrc)>g_PI)
					angleDst -= 2*sgnnz(angleDst-angleSrc)*g_PI;
				dw = axis1src*(angleDst-angleSrc);
				// rotate around dir0src to match (axis1src wrt axis0src) to (axis1dst wrt axis0dst)
				angleSrc = atan2_tpl((axis1src^axis0src)*dir0src, axis0src*axis1src-(axis0src*dir0src)*(axis1src*dir0src));
				angleDst = atan2_tpl((axis1dst^axis0dst)*dir0dst, axis0dst*axis1dst-(axis0dst*dir0dst)*(axis1dst*dir0dst));
				if (fabs_tpl(angleDst-angleSrc)>g_PI)
					angleDst -= 2*sgnnz(angleDst-angleSrc)*g_PI;
				dw -= dir0src*(angleDst-angleSrc);
				// update m_segs[i+1].vel
				dv = dw*(m_stiffnessAnim*(1-m_stiffnessDecayAnim*(i+1)*rnSegs)*time_interval) ^ m_segs[i+1].pt-m_segs[i].pt;
				vrel = m_segs[i+1].vel-m_segs[i].vel;
				m_segs[i+1].vel = m_segs[i].vel + vrel*max(0.0f,1.0f-m_dampingAnim*time_interval) + dv;
			}
		} else for(i=0;i<=m_nSegs;i++)
			m_segs[i].pt = qtv*m_segs[i].ptdst*scaletv+offstv;
	}
}


void CRopeEntity::CheckCollisions(int iDir, SRopeCheckPart *checkParts,int nCheckParts, float seglen,float rseglen, const Vec3& hingeAxis)
{
	int i,j,iseg,iStart,iEnd,iend,hinge=hingeAxis.len2()>0;
	Vec3 n,rotax,ptres[2];
	float tfullCheck=0.1f+hinge*100.0f;
	float t,angle,dist2,diff,cost,sint,collDistLine=m_collDist*0.1f;
	float unprojLimit = m_unprojLimit+isneg(0.0001f-m_jointLimit)*100.0f;
	bool bUseLineUnproj = !m_pOuterEntity || m_collDist<seglen*0.166f || m_collTypes & (ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid);
	geom_world_data gwd;
	geom_contact *pcontact;
	contact cnt;
	CRayGeom aray; aray.m_iCollPriority = 10;
	CPhysicalEntity *pent;
	intersection_params ip;
	ip.bStopAtFirstTri = true;
	ip.iUnprojectionMode = 1;

	iDir = 1-iDir*2;
	iStart = m_nSegs & iDir>>31;
	iEnd = m_nSegs & -iDir>>31;
	for(i=iStart;i!=iEnd;i+=iDir)	{
		iseg = i+(iDir>>31);
		m_segs[iseg].vreq = 0; 
		if (pent=m_segs[iseg].pContactEnt) {
			gwd.R = checkParts[m_segs[iseg].iCheckPart].R;
			gwd.offset = checkParts[m_segs[iseg].iCheckPart].offset;
			gwd.scale = checkParts[m_segs[iseg].iCheckPart].scale;
			t = iszero(i-iStart)*m_noCollDist; // don't check the first half of the first segment
			if (!m_segs[iseg].bRecheckContact &&
					checkParts[m_segs[iseg].iCheckPart].pGeom->FindClosestPoint(&gwd, m_segs[iseg].iPrim,m_segs[iseg].iFeature, 
						m_segs[i+iDir].pt,m_segs[i].pt+(m_segs[i+iDir].pt-m_segs[i].pt)*t, ptres)>=0 && 
					(dist2=(n=ptres[1]-ptres[0]).len2())<sqr(collDistLine*2) && // drop contact when gap becomes too big
					sqr_signed(m_segs[iseg].vreq=n*m_segs[iseg].ncontact)>n.len2()*sqr(0.75f)) // drop contact if normal changes abruptly
			{
				n.normalize();
				m_segs[iseg].tcontact = (ptres[1]-m_segs[iseg].pt).len()*rseglen;
				m_segs[iseg].tcontact = min(1.0f,max(0.0f,m_segs[iseg].tcontact));
				if (dist2<sqr(collDistLine)) { // reinforce the distance
					t = (iDir>>31&1) + m_segs[iseg].tcontact*iDir; // 1.0-t for iDir==-1
					j = isneg(t-0.1f); t += j; j = -j & iDir;
					if ((unsigned int)i-j<=(unsigned int)m_nSegs) {
						rotax = (m_segs[i+iDir-j].pt-m_segs[i-j].pt^n).normalized();
						angle = (collDistLine-sqrt_tpl(dist2))/(max(t,0.25f)*seglen);
						m_segs[i+iDir-j].pt = m_segs[i+iDir-j].pt.GetRotated(m_segs[i-j].pt, rotax, cos_tpl(angle),sin_tpl(angle));
						m_segs[i+iDir-j].bRecalcDir = m_segs[max(0,i+iDir-j-1)].bRecalcDir = 1;
					}
					m_segs[iseg].vreq = 0;	 
				} else
					m_segs[iseg].vreq = (collDistLine-m_segs[iseg].vreq)*10.0f;
				m_segs[iseg].ncontact = n;
				m_segs[iseg].vcontact = checkParts[m_segs[iseg].iCheckPart].v+(checkParts[m_segs[iseg].iCheckPart].w^ptres[0]);
				m_bHasContacts = 1;
			}	else
				m_segs[iseg].pContactEnt=0, m_segs[iseg].ncontact.zero(), m_segs[iseg].vreq=0;
		}	else
			m_segs[iseg].ncontact.zero();

		if (!m_segs[iseg].pContactEnt || m_segs[iseg].tcontact<tfullCheck && m_nSegs<=8) for(j=0,angle=0;j<nCheckParts;j++) {
			aray.m_ray.origin = (m_segs[i].pt-checkParts[j].offset)*checkParts[j].R;
			aray.m_dirn = aray.m_ray.dir = (m_segs[i+iDir].pt-checkParts[j].offset)*checkParts[j].R-aray.m_ray.origin;
			if (box_ray_overlap_check(&checkParts[j].bbox,&aray.m_ray)) {
				gwd.offset.zero(); gwd.R.SetIdentity(); gwd.scale=checkParts[j].scale;
				ip.centerOfRotation = aray.m_ray.origin; ip.axisOfRotation = hingeAxis*checkParts[j].R;
				bool bNoLineUnproj = !bUseLineUnproj;
				if (hinge) {
					Vec3 dirSeg=aray.m_ray.dir, org0=aray.m_ray.origin;
					aray.m_ray.origin += aray.m_ray.dir; 
					(aray.m_dirn = ip.axisOfRotation^dirSeg).normalize();
					aray.m_dirn *= sgnnz(aray.m_dirn*(aray.m_ray.origin-checkParts[j].bbox.center));
					aray.m_ray.dir = aray.m_dirn*(m_length*0.5f);
					aray.m_ray.origin -= aray.m_dirn*(m_collDist*1.3f);
					aray.m_iCollPriority = -1;
					if (checkParts[j].pGeom->Intersect(&aray,&gwd,0,0,pcontact) && pcontact->n*aray.m_dirn>0) {
						cnt.n = aray.m_dirn;
						cnt.pt = pcontact->pt*checkParts[j].rscale;
						bNoLineUnproj=true; goto unproj_end_point;
					}
					aray.m_ray.origin=org0; aray.m_dirn=aray.m_ray.dir=dirSeg; aray.m_iCollPriority=10;
				}
				if (1-hinge & checkParts[j].bVtxUnproj && checkParts[j].pGeom->UnprojectSphere((aray.m_ray.origin+aray.m_ray.dir)*checkParts[j].rscale,m_collDist,m_collDist,&cnt)) {
					unproj_end_point:
					m_segs[iseg].pContactEnt = checkParts[j].pent;
					m_segs[iseg].iContactPart = checkParts[j].ipart;
					m_segs[iseg].iCheckPart = j;
					m_segs[iseg].tcontact = iDir+1>>1;
					(m_segs[iseg].ncontact*=angle) += (n=checkParts[j].R*cnt.n); angle=1.0f;
					Vec3 ptCnt = checkParts[j].R*cnt.pt*checkParts[j].scale + checkParts[j].offset;
					if ((t=(m_segs[i+iDir].pt-ptCnt)*n) < m_collDist)	{
						if (t<0)
							m_segs[i+iDir].pt = ptCnt;
						m_segs[iseg].vreq = max(m_segs[iseg].vreq, (m_collDist-max(t,0.0f))*10.0f);
					}
					m_segs[iseg].iPrim = cnt.iFeature[0];
					m_segs[iseg].iFeature = 0;
					m_segs[iseg].vcontact = checkParts[j].v+(checkParts[j].w^m_segs[i+iDir].pt);
					m_segs[iseg].bRecheckContact = 1;
					m_segs[i+iDir].bRecalcDir = m_segs[max(0,i+iDir-1)].bRecalcDir = 1;
					m_bHasContacts = 1;
					aray.m_dirn = aray.m_ray.dir = (m_segs[i+iDir].pt-checkParts[j].offset)*checkParts[j].R-aray.m_ray.origin;
				}
				rotax = aray.m_ray.dir*(iszero(i-iStart)*m_noCollDist);
				aray.m_ray.origin += rotax; aray.m_ray.dir -= rotax;
				if (!bNoLineUnproj && checkParts[j].pGeom->Intersect(&aray,&gwd,0,&ip,pcontact)) {
					aray.m_ray.origin -= rotax;
					if (pcontact->iUnprojMode==1) {// && (pcontact->pt-aray.m_ray.origin)*pcontact->n<0) {
						diff = m_segs[iseg].tcontact = (pcontact->pt-aray.m_ray.origin).len();
						m_segs[iseg].tcontact = m_segs[iseg].tcontact*iDir-seglen*(iDir>>31); // flip tcontact when iDir is -1
						iend = -iseg>>31 & 1;
						m_segs[iseg].pContactEnt = checkParts[j].pent;
						m_segs[iseg].iContactPart = checkParts[j].ipart;
						m_segs[iseg].iCheckPart = j;
						rotax = checkParts[j].R*-pcontact->dir;
						//pcontact->t += min((float)pcontact->t, m_collDist/diff);
						if (pcontact->t>(real)unprojLimit) {
							pcontact->t=(real)unprojLimit; m_segs[iseg].bRecheckContact=1;
						} else
							m_segs[iseg].bRecheckContact=0;
						cost = cos_tpl(pcontact->t); sint = sin_tpl(pcontact->t);
						m_segs[iseg].ncontact = checkParts[j].R*(pcontact->n.normalized().GetRotated(pcontact->dir,cost,-sint));
						m_segs[iseg].tcontact	*= rseglen;
						m_segs[iseg].tcontact = min(1.0f,max(0.0f,m_segs[iseg].tcontact));
						m_segs[i+iDir].pt = m_segs[i+iDir].pt.GetRotated(m_segs[i].pt, rotax, cost,sint);
						m_segs[i+iDir].pt += m_segs[iseg].ncontact*collDistLine;
						//m_segs[iseg].bRecheckContact = isnonneg((m_segs[i+iDir].pt-m_segs[i].pt)*m_segs[iseg].ncontact);
						m_segs[iseg].iPrim = pcontact->iPrim[0];
						m_segs[iseg].iFeature = pcontact->iFeature[0];
						m_segs[iseg].vcontact = checkParts[j].v+(checkParts[j].w^m_segs[i+iDir].pt);
						m_segs[i+iDir].bRecalcDir = m_segs[max(0,i+iDir-1)].bRecalcDir = 1;
						m_bHasContacts = 1;
					}
				}
			}
		}
		if (m_segs[iseg].pContactEnt && fabs_tpl(m_segs[iseg].ncontact.len2()-1.0f)>0.01f)
			m_segs[iseg].ncontact.normalize();

		if (pent!=m_segs[iseg].pContactEnt) {
			if (pent) pent->Release();
			if (m_segs[iseg].pContactEnt) m_segs[iseg].pContactEnt->AddRef();
		}
	}

	for(i=0;i<m_nSegs;i++) if (m_segs[i].bRecalcDir)
		m_segs[i].dir = (m_segs[i+1].pt-m_segs[i].pt).normalized();
}

int CRopeEntity::Step(float time_interval)
{
	if (m_nSegs<=0 || !m_bAwake)
		return 1;
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_ENTITY_PROFILER
	
	int iCaller = get_iCaller_int();
	float seglen=m_length/m_nSegs,seglen2=sqr(seglen), rseglen=m_nSegs/max(1e-6f,m_length),rseglen2=sqr(rseglen),scale; 
	int i,j,k,iDir,iEnd,iter,bTargetPoseActive=m_bTargetPoseActive,bGridLocked=0,bHasContacts=0,nCheckParts=0;
	int collTypes = m_collTypes;
	Vec3 pos,gravity,dir,ptend[2],sz,BBox[2],ptnew,dv,dw,vrel,dir0,offstv(ZERO),collBBox[2];
	float diff,a,b,E,rnSegs=1.0f/m_nSegs,rcollDist=0,scaletv=1.0f,ktimeBack,
				damping=max(0.0f,1.0f-(m_damping-m_dampingAnim*(m_bTargetPoseActive-1>>31))*time_interval);
	quaternionf dq,qtv(1,0,0,0),q;
	RigidBody *pbody,rbody;
	pe_params_buoyancy pb[4];
	SRopeCheckPart checkParts[6+eBigEndian*26];
	EventPhysPostStep event;

	if (m_collTypes & ent_all)
		for(i=0;i<=m_nSegs;i++)	if (m_segs[i].pContactEnt && (m_segs[i].pContactEnt->m_iSimClass==7 ||
			m_segs[i].iContactPart>=m_segs[i].pContactEnt->m_nParts)) 
		{
			if (!(m_flags & rope_subdivide_segs)) 
				m_segs[i].pContactEnt->Release();
			m_segs[i].pContactEnt=0; MARK_UNUSED m_segs[i].ncontact;
		}
	for(i=j=0;i<2;i++) 
		if (m_pTiedTo[i] && (m_pTiedTo[i]->m_iDeletionTime>0 || 
				(unsigned int)m_pTiedTo[i]->m_iSimClass-1u<2u && m_iTiedPart[i]>=m_pTiedTo[i]->m_nParts))
		j |= 1<<i;
	if (j) {
		pe_params_rope pr;
		if (j&1) pr.pEntTiedTo[0]=0;
		if (j&2) pr.pEntTiedTo[1]=0;
		SetParams(&pr);
	}

	if (m_flags & pef_disabled)	{
		if (m_bTargetPoseActive==2 && m_pTiedTo[0])	{
			m_pTiedTo[0]->GetLocTransform(m_iTiedPart[0], dir,q,scale, this);
			m_segs[0].pt = dir + q*m_ptTiedLoc[0];
			m_BBox[0] = min(m_BBox[0],m_segs[0].pt);
			m_BBox[1] = max(m_BBox[1],m_segs[0].pt);
		}
		return 1;
	}
	if (m_pWorld->m_bWorldStep!=2) {
		if (m_timeStepPerformed>m_timeStepFull-0.001f || m_timeStepPerformed>0 && (m_flags & pef_invisible))
			return 1;
	}	else if (time_interval>0)
		time_interval = max(0.001f, min(m_timeStepFull-m_timeStepPerformed, time_interval));
	else
		return 1;

	m_timeStepPerformed += time_interval;
	m_lastTimeStep = time_interval;
	m_timeLastActive = m_pWorld->m_timePhysics;
	ktimeBack = (m_timeStepFull-m_timeStepPerformed)/m_timeStepFull*(iszero(m_pWorld->m_bWorldStep-2)^1);

	iEnd = 0;
	if (!m_pOuterEntity && (iEnd=m_pWorld->CheckAreas(this,dir,pb,4)) && !is_unused(dir))
		m_gravity = (dir-m_pWorld->m_vars.gravity).len2() < dir.len2()*sqr(0.01) ? m_gravity0 : dir;
	gravity = m_gravity;

	event.dt=time_interval; event.pos=m_pos; event.q=m_qrot; event.idStep=m_pWorld->m_idStep;
	for(i=0,dir=m_wind;i<iEnd;i++)
		dir += pb[i].waterFlow*pb[i].iMedium;
	if (m_flags & rope_no_stiffness_when_colliding) {
		iter = m_pTiedTo[0] ? m_pTiedTo[0]->m_id : (m_pTiedTo[1] ? m_pTiedTo[1]->m_id : -10);
		for(i=bHasContacts=0;i<=m_nSegs;i++) if (m_segs[i].pContactEnt)
			bHasContacts |= m_segs[i].pContactEnt->m_id-iter>>31 | iter-m_segs[i].pContactEnt->m_id>>31;
		if (bHasContacts)
			bTargetPoseActive = 0;
	}
	if (m_flags & (rope_target_vtx_rel0 | rope_target_vtx_rel1)) {
		i = (m_flags & rope_target_vtx_rel1)!=0;
		if (m_pTiedTo[i]) 
			m_pTiedTo[i]->GetLocTransformLerped(m_iTiedPart[i], offstv,qtv,scaletv,ktimeBack, this);
	}

	if ((m_windTimer+=time_interval*4)>1.0f) {
		m_windTimer = 0; m_wind0 = m_wind1;
		a = m_windVariance*(fabs_tpl(dir.x)+fabs_tpl(dir.y)+fabs_tpl(dir.z));
		m_wind1 = dir+Vec3(cry_random(0.0f, a)-a*0.5f,cry_random(0.0f, a)-a*0.5f,cry_random(0.0f, a)-a*0.5f);
	}
	dw = m_wind0*m_windTimer+m_wind1*(1.0f-m_windTimer);
	if (bTargetPoseActive && m_segs[0].pContactEnt && m_segs[0].tcontact<0.2f) {
		//gravity.zero(); dw.zero(); 
		bTargetPoseActive=0;
		//for(i=0;i<=m_nSegs;i++) m_segs[i].vel.zero();
	}

	if (m_flags & rope_findiff_attached_vel)
		a = 1.0f/m_timeStepFull;
	for(i=1;i>=0;i--) if (m_pTiedTo[i]) {
		m_pTiedTo[i]->GetLocTransformLerped(m_iTiedPart[i], pos,q,scale,ktimeBack, this);
		m_posBody[i][1] = pos; m_qBody[i][1] = q;
		m_segs[m_nSegs*i].pt = ptend[i] = pos + q*m_ptTiedLoc[i];
		if (m_nVtx)
			m_vtx[(m_nVtx-1)*i].pt = ptend[i];
		pbody = m_pTiedTo[i]->GetRigidBodyTrans(&rbody,m_iTiedPart[i],this,2);
		if (!(m_flags & rope_findiff_attached_vel)) {
			dv = pbody->v; dw = pbody->w;
		}	else {
			dv = (m_posBody[i][1]-m_posBody[i][0])*a;
			dq = m_qBody[i][1]*!m_qBody[i][0];
			dw = dq.v*(dq.w*2*a);
			if (dv.len2()>sqr(m_pWorld->m_vars.maxVel*0.3f)) {
				dv.zero(); dw.zero();
			}
		}
		m_segs[m_nSegs*i].vel = dv + (dw^ptend[i]-pbody->pos);
		m_timeIdle *= m_pTiedTo[i]->IsAwake()^1;
	}

	if (m_length==0 && m_nVtx>0)
		ZeroLengthStraighten(time_interval),k=0;
	else k=1;

	for(i=(m_pTiedTo[0]!=0),j=m_pTiedTo[1]!=0;i<=m_nSegs-j;i++)	{
		m_segs[i].pt0 = m_segs[i].pt;
		ptnew = m_segs[i].pt + m_segs[i].vel*time_interval;
		Vec3 ptdst = qtv*m_segs[i].ptdst*scaletv+offstv;
		if (bTargetPoseActive==1 && (m_stiffnessAnim==0 || (m_segs[i].pt-ptdst)*(ptnew-ptdst)<0 && 
				(ptnew-ptdst).len2()<sqr(seglen*0.08f)))
			ptnew = ptdst;
		m_segs[i].pt = ptnew;
		m_segs[i].vel += gravity*(time_interval*k);
		m_segs[i].vel += (dw-m_segs[i].vel)*(min(1.0f,m_airResistance*time_interval)*k);
		for(iter=0;iter<iEnd;iter++) 
			if ((diff=((m_segs[i].pt-pb[iter].waterPlane.origin)*pb[iter].waterPlane.n)*(pb[iter].iMedium-1>>31))*k*m_collDist>0) {
				if (rcollDist==0)
					rcollDist = 1.0f/m_collDist;
				m_segs[i].vel += (pb[iter].waterFlow-m_segs[i].vel)*min(1.0f,m_waterResistance*time_interval)-
													gravity*(pb[iter].waterDensity*m_rdensity*min(1.0f,diff*rcollDist)*time_interval);
			}
	}

	if (m_flags & rope_subdivide_segs && m_nVtx>0) {
		for(i=0;i<m_nVtx;i++)	
			m_vtx[i].pt += m_vtx[i].vel*time_interval;
		for(i=0;i<m_nSegs;i++) if (m_segs[i+1].iVtx0>m_segs[i].iVtx0+1) {
			dir0 = (m_vtx[m_segs[i+1].iVtx0].pt-m_vtx[m_segs[i].iVtx0].pt).normalized();
			for(m_segs[i].ptdst.zero(),j=m_segs[i].iVtx0,b=0; j<m_segs[i+1].iVtx0; j++) {
				dir = m_vtx[j].pt-m_vtx[m_segs[i].iVtx0].pt;
				if ((a=(dir^dir0).len2())>b)
					m_segs[i].ptdst = dir-dir0*(dir*dir0), b=a;
			}
			m_segs[i].ptdst.normalize();
		}	else
			m_segs[i].ptdst.zero();
		if (m_bStrained && m_length>0) for(i=0;i<=m_nSegs;i++)
			m_segs[i].pt = m_vtx[m_segs[i].iVtx0].pt;
	}

	for(i=0;i<m_nAttach;i++) {
		m_attach[i].pent->GetLocTransformLerped(m_attach[i].ipart, pos,q,scale,ktimeBack, this);
		m_attach[i].pt = m_segs[j=m_attach[i].ivtx].pt = pos+q*m_attach[i].ptloc;
		pbody = m_attach[i].pent->GetRigidBodyTrans(&rbody,m_attach[i].ipart,this,2);
		m_segs[j].vel = m_attach[i].v = pbody->v+(pbody->w ^ m_attach[i].pt-pbody->pos);
		if (m_nVtx)	{
			m_vtx[m_segs[j].iVtx0].pt = m_attach[i].pt;
			m_vtx[m_segs[j].iVtx0].vel = m_attach[i].v;
		}
	}

	if (!(m_flags & rope_subdivide_segs) && m_pTiedTo[0] && m_pTiedTo[1] && (ptend[1]-ptend[0]).len2()>sqr(m_length)) {
		float newlen=(ptend[1]-ptend[0]).len(), newseglen=newlen/m_nSegs; rnSegs=1.0f/(m_nSegs+1);
		dir = (ptend[1]-ptend[0]).normalized();
		for(i=1;i<m_nSegs;i++) {
			m_segs[i].dir = dir;
			m_segs[i].pt = ptend[0] + m_segs[i].dir*(newseglen*i);
			m_segs[i].vel = m_segs[0].vel*((m_nSegs+1-i)*rnSegs)+m_segs[m_nSegs].vel*(i*rnSegs);
		}
		m_segs[0].dir = m_segs[m_nSegs].dir = dir;
		pe_status_awake isawake;
		m_bAwake = m_pTiedTo[0]->GetStatus(&isawake) | m_pTiedTo[1]->GetStatus(&isawake) | isneg(-bTargetPoseActive);
		m_bAwake; m_pWorld->RepositionEntity(this,2);

		pe_status_constraint sc; sc.id=m_idConstraint;
		if (m_idConstraint && (!m_pTiedTo[m_iConstraintClient]->GetStatus(&sc) || sc.flags & constraint_inactive)) {
			EventPhysJointBroken epjb;
			epjb.idJoint=0; epjb.bJoint=0; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
			epjb.pEntity[0]=epjb.pEntity[1]=this; epjb.pForeignData[0]=epjb.pForeignData[1]=m_pForeignData; 
			epjb.iForeignData[0]=epjb.iForeignData[1]=m_iForeignData;
			epjb.pt = m_segs[m_nSegs].pt;	epjb.n = m_segs[m_nSegs-1].dir;
			epjb.partid[0] = 1;
			epjb.partid[1] = m_pTiedTo[1]->m_parts[m_iTiedPart[1]].id;
			epjb.partmat[0]=epjb.partmat[1] = -1;
			m_pWorld->OnEvent(2,&epjb);

			pe_params_rope pr; pr.pEntTiedTo[1]=0;
			SetParams(&pr);
		}

		BBox[0] = min(m_segs[0].pt,m_segs[m_nSegs].pt); BBox[1] = max(m_segs[0].pt,m_segs[m_nSegs].pt);
		a = max(max(BBox[1].x-BBox[0].x,BBox[1].y-BBox[0].y),BBox[1].z-BBox[0].z)*0.01f+m_collDist*2;
		BBox[0] -= Vec3(a,a,a); BBox[1] += Vec3(a,a,a);
		if (m_flags & pef_traceable)
			bGridLocked = m_pWorld->RepositionEntity(this,3,BBox);
		{ WriteLock lock(m_lockUpdate);
			m_pos = m_segs[0].pt; m_BBox[0] = BBox[0]; m_BBox[1] = BBox[1];
			for(i=0;i<=m_nSegs;i++)
				m_segs[i].pt0 = m_segs[i].pt;
			m_pWorld->UnlockGrid(this,-bGridLocked);
		}
		InitEvent(&event,this,iCaller);
		event.pos = m_pos;
		m_pWorld->OnEvent(m_flags,&event);
		return 1;
	}

	if (collTypes & ent_terrain && m_pTiedTo[0] && m_pTiedTo[1] && m_pTiedTo[0]->m_iSimClass+m_pTiedTo[1]->m_iSimClass==0 &&
			sqr(m_length*0.95f) < (m_segs[0].pt-m_segs[m_nSegs].pt).len2() && m_penaltyScale>0)
		collTypes &= ~ent_terrain;

	EnforceConstraints(seglen, qtv,offstv,scaletv, m_bTargetPoseActive, time_interval);
	if (bTargetPoseActive)
		ApplyStiffness(time_interval, bTargetPoseActive, qtv,offstv,scaletv);

	BBox[0] = BBox[1] = m_segs[0].pt;
	for(i=0;i<=m_nSegs;i++) {
		(m_segs[i].vel += m_segs[i].vel_ext) *= damping;
		m_segs[i].vel_ext.zero();
		m_segs[i].bRecalcDir = 0;
		BBox[0] = min(BBox[0], m_segs[i].pt);
		BBox[1] = max(BBox[1], m_segs[i].pt);
	}
	a = max(max(BBox[1].x-BBox[0].x,BBox[1].y-BBox[0].y),BBox[1].z-BBox[0].z)*0.01f+m_collDist*2;
	BBox[0] -= Vec3(a,a,a); BBox[1] += Vec3(a,a,a);
	collBBox[0]=BBox[0]; collBBox[1]=BBox[1];

	if (collTypes & ent_all | m_flags & rope_collides_with_attachment) {
		CRY_PROFILE_REGION(PROFILE_PHYSICS, "Rope collision");
		int objtypes;
		CPhysicalEntity **pentlist,*pentbuf[2];
		int iseg,nEnts,iend,ippbv=0,nPrecompPartBVs=0;
		box boxrope,boxpart;
		Vec3 center,ptres[2],n;
		Matrix33 mtxWorld2Rope(IDENTITY);
		geom_world_data gwd;
		COverlapChecker Overlapper;
		pe_status_pos sp; sp.timeBack = 1;
		m_bHasContacts = 0;

		Overlapper.Init();
		if (!is_unused(m_collBBox[0])) {
			if (!m_pOuterEntity)
				collBBox[0]=m_collBBox[0], collBBox[1]=m_collBBox[1];
			else {
				m_pOuterEntity->GetLocTransform(0,pos,q,scale,this);
				Matrix33 mtx = Matrix33(m_pOuterEntity->m_qrot);
				center = mtx*((m_collBBox[0]+m_collBBox[1])*0.5f*scale)+m_pOuterEntity->m_pos;
				sz = mtx.Fabs()*((m_collBBox[1]-m_collBBox[0])*0.5f*scale);
				collBBox[0] = center-sz; collBBox[1] = center+sz;
			}
		}
		center = (BBox[0]+BBox[1])*0.5f;
		if (m_nSegs<6) {
			Vec3 axisx,axisy,axisz=(m_segs[m_nSegs].pt-m_segs[0].pt).normalized();
			if (m_nSegs<2) 
				axisx = axisz.GetOrthogonal();
			else for(i=1,axisx.zero(); i<m_nSegs; i++)
				axisx += m_segs[i].pt-m_segs[0].pt-axisz*(axisz*(m_segs[i].pt-m_segs[0].pt));
			axisy = axisz^axisx.normalize();
			mtxWorld2Rope = Matrix33(axisx,axisy,axisz).T();
			for(i=0,boxrope.size=Vec3(m_pWorld->m_vars.maxContactGap); i<=m_nSegs; i++)
				boxrope.size = max(boxrope.size, (mtxWorld2Rope*(m_segs[i].pt-center)).abs());
		}	else
			boxrope.size = (BBox[1]-BBox[0])*0.5f;
		boxrope.size += Vec3(m_collDist);
		boxrope.bOriented = 2;

		objtypes = collTypes | ent_sort_by_mass|ent_ignore_noncolliding|ent_triggers;
		nEnts = m_pWorld->GetEntitiesAround(collBBox[0],collBBox[1], pentlist, objtypes, this, 0,iCaller);
		if (m_flags & rope_collides_with_attachment) {
			if (!pentlist)
				pentlist = pentbuf;
			for(i=j=0;i<nEnts;i++) 
				j += (pentlist[i]==m_pTiedTo[0]) | (pentlist[i]==m_pTiedTo[1])*2;
			if (m_pTiedTo[0] && !(j&1))
				pentlist[nEnts++] = m_pTiedTo[0];
			if (m_pTiedTo[1] && !(j&2))
				pentlist[nEnts++] = m_pTiedTo[1];
		}

		{
		WriteLock lockEL(g_lockProcessedAux);
		sphere sphrope; 
		sphrope.center = center; 
		sphrope.r = ((BBox[1]-BBox[0])*0.5f).GetLengthFast();
		assert(pentlist || nEnts == 0);
		const int nMaxTris = m_pWorld->m_vars.maxRopeColliderSize;
		SPrecompPartBV*& partBVList=m_pWorld->m_threadData[iCaller].pTmpPartBVList;
		if (!partBVList || m_pOuterEntity!=m_pWorld->m_threadData[iCaller].pTmpPartBVListOwner || !m_pOuterEntity || is_unused(m_collBBox[0])) {
			if (!partBVList) partBVList=new SPrecompPartBV[m_pWorld->m_threadData[iCaller].szTmpPartBVList=64]; 
			for(i=0,ippbv=0;i<nEnts;i++) 
			if (pentlist[i]!=this && !IgnoreCollision(m_collisionClass, pentlist[i]->m_collisionClass) && !(m_flags & rope_ignore_attachments && (
					pentlist[i]==m_pTiedTo[0] || m_pTiedTo[0] && m_pTiedTo[0]->m_pForeignData && m_pTiedTo[0]->m_pForeignData==pentlist[i]->m_pForeignData ||
					pentlist[i]==m_pTiedTo[1] || m_pTiedTo[1] && m_pTiedTo[1]->m_pForeignData && m_pTiedTo[1]->m_pForeignData==pentlist[i]->m_pForeignData)))
			for(iter=0; iter<pentlist[i]->GetUsedPartsCount(iCaller); iter++) 
			if (pentlist[i]->m_parts[j=pentlist[i]->GetUsedPart(iCaller,iter)].flags & m_flagsCollider &&
					!(pentlist[i]->m_parts[j].flags & geom_no_coll_response) &&
					(!nMaxTris || pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetPrimitiveCount()<=nMaxTris)) {
				if (ippbv==m_pWorld->m_threadData[iCaller].szTmpPartBVList) {
					ReallocateList(partBVList, m_pWorld->m_threadData[iCaller].szTmpPartBVList, m_pWorld->m_threadData[iCaller].szTmpPartBVList+64); 
					m_pWorld->m_threadData[iCaller].szTmpPartBVList+=64; 
				}
				partBVList[ippbv].iparttype=pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetType(); 
				pentlist[i]->GetLocTransformLerped(j,partBVList[ippbv].pospart,partBVList[ippbv].qpart,scale,ktimeBack,this);
				if (partBVList[ippbv].iparttype!=GEOM_HEIGHTFIELD) {
					pentlist[i]->m_parts[j].pPhysGeomProxy->pGeom->GetBBox(&partBVList[ippbv].partbox);
					partBVList[ippbv].partbox.size *= pentlist[i]->m_parts[j].scale; 
					partBVList[ippbv].partbox.center *= pentlist[i]->m_parts[j].scale;
					partBVList[ippbv].partsph.center=partBVList[ippbv].pospart+partBVList[ippbv].qpart*partBVList[ippbv].partbox.center;
					partBVList[ippbv].partsph.r=partBVList[ippbv].partbox.size.GetLengthFast(); 
				}	else {
					partBVList[ippbv].partbox.Basis.SetIdentity(); partBVList[ippbv].partbox.center.zero(); partBVList[ippbv].partbox.size=Vec3(1e6f);
					partBVList[ippbv].partsph = sphrope;
				}
				partBVList[ippbv].scale=pentlist[i]->m_parts[j].scale; 
				partBVList[ippbv].pent=pentlist[i]; partBVList[ippbv].ppart=&pentlist[i]->m_parts[j]; 
				partBVList[ippbv].ient=i; partBVList[ippbv++].ipart=j; 
		  }
			m_pWorld->m_threadData[iCaller].nTmpPartBVs=ippbv;
			m_pWorld->m_threadData[iCaller].pTmpPartBVListOwner=m_pOuterEntity;
		} 
		
		CPhysicalEntity* pent=NULL;
		geom* ppart; 
		for (k=0,nPrecompPartBVs=m_pWorld->m_threadData[iCaller].nTmpPartBVs; k<nPrecompPartBVs; ++k) { 
			if (!sphere_sphere_overlap_check(&sphrope, &partBVList[k].partsph, &Overlapper)) 
				continue; 
			PrefetchLine(&partBVList[k+1],0);
			i=partBVList[k].ient; j=partBVList[k].ipart; 
			if (pent != partBVList[k].pent) partBVList[k].pent->m_bProcessed_aux=nCheckParts<<24; 
			pent=partBVList[k].pent; assert(pent);
			ppart=partBVList[k].ppart; 
			checkParts[nCheckParts].offset=partBVList[k].pospart; 
			boxrope.Basis = mtxWorld2Rope*Matrix33(partBVList[k].qpart);
			boxrope.center = (center-partBVList[k].pospart)*partBVList[k].qpart;
			if (box_box_overlap_check(&boxrope,&partBVList[k].partbox,&Overlapper)) {
				if (partBVList[k].iparttype!=GEOM_HEIGHTFIELD) { 
					checkParts[nCheckParts].bbox.Basis = partBVList[k].partbox.Basis; 
					checkParts[nCheckParts].bbox.bOriented = partBVList[k].partbox.bOriented; 
					checkParts[nCheckParts].bbox.center = partBVList[k].partbox.center;
					checkParts[nCheckParts].bbox.size = partBVList[k].partbox.size+Vec3(m_collDist);
				} else
					checkParts[nCheckParts].bbox = boxrope; 
				checkParts[nCheckParts].pent = pent;
				checkParts[nCheckParts].ipart = j;
				checkParts[nCheckParts].R = Matrix33(partBVList[k].qpart);
				checkParts[nCheckParts].scale = partBVList[k].scale;
				if (((CGeometry*)ppart->pPhysGeomProxy->pGeom)->IsAPrimitive() && pent!=m_pTiedTo[0] && pent!=m_pTiedTo[1]) {
					n = (m_segs[iseg=(m_pTiedTo[1]==0 ? 0:m_nSegs)].pt-checkParts[nCheckParts].offset)*checkParts[nCheckParts].R;
					gwd.scale = checkParts[nCheckParts].scale;
					if (ppart->pPhysGeomProxy->pGeom->FindClosestPoint(&gwd,iend,iend,n,n,ptres)>=0 && 
						((a=(ptres[1]-ptres[0])*(ptres[0]-checkParts[nCheckParts].bbox.center))<0 || 
							(ptres[0]-ptres[1]).len2()<sqr(m_collDist*1.2f))) 
					{
						n = ptres[0]+(ptres[1]-ptres[0]).normalized()*(m_collDist*1.2f*sgnnz(a));
						m_segs[iseg].pt = checkParts[nCheckParts].R*n+checkParts[nCheckParts].offset;
						//continue;
					}
				}

				assert( pent != NULL );

				ppart->pPhysGeomProxy->pGeom->PrepareForRayTest(
					partBVList[k].scale==1.0f ? seglen:seglen/partBVList[k].scale);
				checkParts[nCheckParts].rscale = checkParts[nCheckParts].scale==1.0f ? 1.0f:1.0f/checkParts[nCheckParts].scale;
				if (m_flags & rope_subdivide_segs) {
					pent->GetStatus(&sp);
					checkParts[nCheckParts].pos0 = sp.pos;
					checkParts[nCheckParts].q0 = sp.q;	
				} else {
					checkParts[nCheckParts].pos0 = pentlist[i]->m_pos;
					checkParts[nCheckParts].q0 = pentlist[i]->m_qrot;
				}
				checkParts[nCheckParts].pGeom = (CGeometry*)ppart->pPhysGeomProxy->pGeom;
				checkParts[nCheckParts].bProcess = 1;
				if (pent->m_nParts<=24)
					pent->m_bProcessed_aux |= 1u<<j;
				else
					pent->m_parts[j].flags |= geom_removed;
				AtomicAdd(&pent->m_bProcessed, ~pent->m_bProcessed&1<<iCaller);
				pbody = pent->GetRigidBodyTrans(&rbody,j,this,2);
				checkParts[nCheckParts].v = pbody->v-(pbody->w^pbody->pos);
				checkParts[nCheckParts].w = pbody->w;
				checkParts[nCheckParts].bVtxUnproj = checkParts[nCheckParts].pGeom->IsAPrimitive();
				if (++nCheckParts==CRY_ARRAY_COUNT(checkParts))
					goto enoughgeoms;
			}
			boxrope.bOriented++;
		} enoughgeoms:
		for(i=0; i<=m_nSegs; i++) if (m_segs[i].pContactEnt)
			if (!(m_segs[i].pContactEnt->m_bProcessed & 1<<iCaller) ||
					 (j = m_segs[i].pContactEnt->GetCheckPart(m_segs[i].iContactPart) & 0xFFFF)>=nCheckParts)
			{
				if (!(m_flags & rope_subdivide_segs))
					m_segs[i].pContactEnt->Release(); 
				m_segs[i].pContactEnt = 0;
			}	else
				m_segs[i].iCheckPart = j;
		for(i=0; i<nCheckParts; i++) if (checkParts[i].pent->m_bProcessed & 1<<iCaller) {
			AtomicAdd(&checkParts[i].pent->m_bProcessed, -(1<<iCaller));
			if (checkParts[i].pent->m_nParts>24) for(j=0;j<checkParts[i].pent->m_nParts;j++)
				checkParts[i].pent->m_parts[j].flags &= ~geom_removed;
		}	
		}	// g_lockProcessedAux

    if (m_pTiedTo[0] && m_pTiedTo[1]) 
			iDir = isneg(m_pTiedTo[1]->GetRigidBody(m_iTiedPart[1])->Minv-m_pTiedTo[0]->GetRigidBody(m_iTiedPart[0])->Minv);
    else 
			iDir = iszero((intptr_t)m_pTiedTo[0]);
		if (!(m_flags & rope_subdivide_segs)) {
			CheckCollisions(iDir, checkParts,nCheckParts, seglen,rseglen, qtv*m_dir0dst);
		} else 
			StepSubdivided(time_interval,checkParts,nCheckParts, seglen);
	}	else {
		for(i=0;i<=m_nSegs;i++)
			m_segs[i].pContactEnt = 0;
		if (m_flags & rope_subdivide_segs)
			StepSubdivided(time_interval,0,0, seglen);
	}

	if (!(m_flags & rope_no_solver) && m_length>0)
		E = Solver(time_interval, seglen);
	else
		for(i=0,E=0;i<=m_nSegs;i++) E += m_segs[i].vel.len2();

	int bAwake;
	i = -isneg(E-m_Emin*(m_nSegs+1));
	int nSlowFrames = m_nSlowFrames;
	nSlowFrames = (nSlowFrames&i)-i;
	i = isneg(0.0001f-m_maxTimeIdle); // forceful deactivation is turned on
	bAwake = (isneg(nSlowFrames-4)|(bTargetPoseActive&1)) & (i^1 | isneg((m_timeIdle+=time_interval*i)-m_maxTimeIdle));
	if (m_pTiedTo[0] && m_pTiedTo[0]->GetType()==PE_ROPE)
		bAwake |= m_pTiedTo[0]->IsAwake();
	if (!bAwake)
		for(i=iszero((intptr_t)m_pTiedTo[0])^1; i<m_nSegs+iszero((intptr_t)m_pTiedTo[1]); i++)
			m_segs[i].vel.zero();
	m_bAwake = bAwake;
	m_nSlowFrames = nSlowFrames;

	m_pos = m_segs[0].pt;
	if (m_flags & pef_traceable)
		bGridLocked = m_pWorld->RepositionEntity(this,3,BBox);
	{ WriteLock lock(m_lockUpdate);
		m_BBox[0] = BBox[0]; m_BBox[1] = BBox[1]; 
		for(i=0;i<=m_nSegs;i++)
			m_segs[i].pt0 = m_segs[i].pt;
		if (m_pTiedTo[0])
			m_pTiedTo[0]->GetLocTransformLerped(m_iTiedPart[0],m_lastposHost,m_lastqHost,scaletv,ktimeBack,this);
		else {
			m_lastposHost=m_pos; m_lastqHost.SetIdentity();
		}
		m_pWorld->UnlockGrid(this,-bGridLocked);
	}
	InitEvent(&event,this,iCaller);
	event.pos = m_pos;
	m_pWorld->OnEvent(m_flags,&event);

	return isneg(m_timeStepFull-m_timeStepPerformed-0.001f);
}

int CRopeEntity::RegisterContacts(float time_interval,int nMaxPlaneContacts)
{
	m_energy = 0;
	if (!m_bStrained || !m_pTiedTo[0] || !m_pTiedTo[1] || m_penaltyScale==0)
		return 1;
	int i,j,iStart,iEnd,jStart,sg,nUniqueBodies,ivtx0,ivtx1,icnt0,icnt1;
	float friction,cos2a,cosa,sina,cosb,sinb,kP,len,rlen,sag,maxM;
	Vec3 n,dir0,dir1,vdir0,vdir1;
	RigidBody *pbody[3];
	entity_contact *pContact;
	DisablePreCG();

	if (!m_vtxSolver)
		m_vtxSolver = new rope_solver_vtx[m_nVtxAlloc];
	cosb = 1/sqrt_tpl(1+sqr(m_frictionPull));
	for(i=0,len=0;i<m_nVtx-1;i++)
		len += (m_vtx[i+1].pt-m_vtx[i].pt)*m_vtx[i].dir;
	rlen = 1.0f/max(1e-10f,len); sag = m_length>0 ? m_length*1.005f-len : -1.0f;
	for(ivtx0=0,len=0; ivtx0<m_nVtx-1 && len<m_attachmentZone; len+=(m_vtx[ivtx0+1].pt-m_vtx[ivtx0].pt).len(),ivtx0++);	ivtx0--;
	for(ivtx1=m_nVtx-1,len=0; ivtx1>0 && len<m_attachmentZone; len+=(m_vtx[ivtx1-1].pt-m_vtx[ivtx1].pt).len(),ivtx1--); ivtx1++;

	for(iStart=j=0; iStart<m_nVtx-1; iStart=iEnd) {
		pContact = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact));
		pContact->nextAux = (entity_contact*)this;

		for(i=iStart+1,kP=1,len=0,sg=0,icnt0=-1,icnt1=iStart; i<m_nVtx-1; i++) {
			len += m_vtx[i-1].dir*(m_vtx[i].pt-m_vtx[i-1].pt);
			if (!is_unused(m_vtx[i].ncontact))
				if ((cos2a=-(m_vtx[i].dir*m_vtx[i-1].dir)) > -0.9998f && 
						sqr_signed(m_vtx[i].ncontact*(m_vtx[i-1].dir-m_vtx[i].dir))>(m_vtx[i-1].dir-m_vtx[i].dir).len2()*0.01f &&
						inrange(i,ivtx0,ivtx1)) 
				{
					pbody[2] = m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart,1);
					pbody[2]->Fcollision.zero(); pbody[2]->Tcollision.zero();
					m_vtx[i].dP = 1;
					if ((1-cos2a) < sqr(m_frictionPull)*(1+cos2a))
						break;
					sg -= sgn((m_vtx[i-1].dir+m_vtx[i].dir)*(m_vtx[i].vel-m_vtx[i].vcontact));
					m_vtx[icnt1].iContactPart |= i<<16;
					icnt0 += i-icnt0 & icnt0>>31; icnt1 = i;
				} else 
					m_vtx[i].dP = 0;
		}
		m_vtx[icnt1].iContactPart |= i<<16;
		// TODO: switch to an infinite friction mdoe if we have too many high-friction contacts 
		// with a non-static geom (or maybe not just non-static)
		friction = m_frictionPull*sgn(sg);
		if (icnt0>=0) {
			dir0 = (m_vtx[icnt0].pt-m_vtx[iStart].pt).normalized();
			dir1 = (m_vtx[i].pt-m_vtx[icnt1].pt).normalized();
		}	else
			dir0=dir1 = (m_vtx[i].pt-m_vtx[iStart].pt).normalized();
		len += dir1*(m_vtx[i].pt-m_vtx[i-1].pt);
		iEnd = i;
		pContact->pbody[0]=pbody[0] = iStart==0 ? 
			(pContact->pent[0]=m_pTiedTo[0])->GetRigidBody(pContact->ipart[0]=m_iTiedPart[0],1) : 
			(pContact->pent[0]=m_vtx[iStart].pContactEnt)->GetRigidBody(pContact->ipart[0]=m_vtx[iStart].iContactPart,1);
		pContact->pbody[1]=pbody[1] = iEnd==m_nVtx-1 ? 
			(pContact->pent[1]=m_pTiedTo[1])->GetRigidBody(pContact->ipart[1]=m_iTiedPart[1],1) : 
			(pContact->pent[1]=m_vtx[iEnd].pContactEnt)->GetRigidBody(pContact->ipart[1]=m_vtx[iEnd].iContactPart,1);
		for(i=0;i<2;i++)
			pbody[i]->Fcollision.zero(), pbody[i]->Tcollision.zero();
		pContact->pt[0] = m_vtx[iStart].pt;
		pContact->pt[1] = m_vtx[iEnd].pt;
		//pContact->K.SetZero();
		sinb = friction*cosb;
		QuatT Body2Rope = GridTrans(m_pTiedTo[0], this);
		pbody[0]->Fcollision += dir0;
		pbody[0]->Tcollision += m_vtx[iStart].pt-Body2Rope*pbody[0]->pos ^ dir0;
		maxM = max(pbody[0]->M, pbody[1]->M);
		nUniqueBodies = (pbody[0]!=pbody[1])+1;

		for(i=iStart+1,kP=1,jStart=j,icnt1=iStart; i<iEnd; i++) 
			if (!is_unused(m_vtx[i].ncontact) && m_vtx[i].dP>0) {
				vdir0 = (m_vtx[i].pt-m_vtx[icnt1].pt).normalized();
				vdir1 = (m_vtx[m_vtx[i].iContactPart>>16].pt-m_vtx[i].pt).normalized();
				m_vtx[i].iContactPart &= 0xFFFF; icnt1 = i;
				if ((cos2a = -(vdir1*vdir0))>0.9995f)	{
					m_vtx[i].dP=0; continue;
				}
				cosa = sqrt_tpl(max(0.0f,(1+cos2a)*0.5f));
				sina = sqrt_tpl(max(0.0f,(1-cos2a)*0.5f));
				m_vtxSolver[j].ivtx = i;
				m_vtxSolver[j].v = vdir1-vdir0;
				n = cosa>1e-4f ? m_vtxSolver[j].v/-(2*cosa) : vdir1.GetOrthogonal().normalized();
				n += (vdir0+vdir1)*(friction/(2*sina));
				kP /= sina*cosb+cosa*sinb;
				m_vtxSolver[j].P = n*(kP*-2*sina*cosa*cosb);
				m_vtxSolver[j].pbody=pbody[2] = m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart,1);
				m_vtxSolver[j].pent = m_vtx[i].pContactEnt;
				Body2Rope = GridTrans(m_vtx[i].pContactEnt, this);
				maxM = max(maxM, pbody[2]->M);
				pbody[2]->Fcollision += m_vtxSolver[j].P;
				pbody[2]->Tcollision += (m_vtxSolver[j].r = m_vtx[i].pt-Body2Rope*pbody[2]->pos)^m_vtxSolver[j].P;
				kP *= sina*cosb-cosa*sinb;
				nUniqueBodies += pbody[2]!=pbody[0] && pbody[2]!=pbody[1];
				j++;
			}
		m_vtx[iStart].iContactPart &= 0xFFFF;
		if (nUniqueBodies<=1 || maxM<=0.0f)
			continue;
		pContact->nloc.x = kP;
		Body2Rope = GridTrans(m_pTiedTo[1], this);
		pbody[1]->Fcollision -= dir1*kP;
		pbody[1]->Tcollision -= m_vtx[iEnd-1].pt-Body2Rope*pbody[1]->pos ^ dir1;
		kP = -dir1*(pbody[1]->Fcollision*pbody[1]->Minv + (Body2Rope.q*(pbody[1]->Iinv*(pbody[1]->Tcollision*Body2Rope.q)) ^ m_vtx[iEnd].pt-Body2Rope*pbody[1]->pos));
		Body2Rope = GridTrans(m_pTiedTo[0], this);
		kP += dir0*(pbody[0]->Fcollision*pbody[0]->Minv + (Body2Rope.q*(pbody[0]->Iinv*(pbody[0]->Tcollision*Body2Rope.q)) ^ m_vtx[iStart].pt-Body2Rope*pbody[0]->pos));
		for(i=iStart+1,j=jStart; i<iEnd; i++) 
			if (!is_unused(m_vtx[i].ncontact) && m_vtx[i].dP>0 && (pbody[2]=m_vtx[i].pContactEnt->GetRigidBody(m_vtx[i].iContactPart,1)))	{
				Body2Rope = GridTrans(m_vtx[i].pContactEnt,this);
				kP += m_vtxSolver[j++].v*(pbody[2]->Fcollision*pbody[2]->Minv + 
							(Body2Rope.q*(pbody[2]->Iinv*(pbody[2]->Tcollision*Body2Rope.q)) ^ m_vtx[i].pt-Body2Rope*pbody[2]->pos));
			}
		pContact->nloc.y = 1/kP;
		pContact->nloc.z = m_maxForce*max(0.01f,time_interval)*1.1f;

		pContact->flags = contact_rope | (m_flags & rope_no_tears && m_maxForce>0 ? contact_rope_stretchy : 0);
		pContact->n = dir0;
		pContact->vreq = -dir1;
		pContact->friction = len*rlen*max(0.0f,min(m_pWorld->m_vars.maxUnprojVelRope, -sag*m_penaltyScale));	
		pContact->Pspare = 0;
		pContact->pBounceCount = (int*)(m_vtxSolver+jStart);
		pContact->iCount = j-jStart;
		
		::RegisterContact(pContact);
		m_energy += maxM*sqr(pContact->friction);
		pContact->next=m_pContact; m_pContact=pContact;
	}
	m_bContactsRegistered = 1;
	m_vtx[0].dP = 0;

	return 1;
}


int CRopeEntity::Update(float time_interval, float damping)
{
	if (m_bStrained && m_pTiedTo[0] && m_pTiedTo[1]) {
		int i,j,i0;
		float vrope[2],len,t;
		Vec3 norm,v,tang,dir;
		RigidBody body(false),*pbody;

		/*m_pTiedTo[0]->RemoveCollider(this);
		m_pTiedTo[1]->RemoveCollider(this);
		for(i=1;i<m_nVtx-1;i++) if (m_vtx[i].pContactEnt)
			m_vtx[i].pContactEnt->RemoveCollider(this);
		for(i=0;i<m_nColliders;i++)
			m_pColliders[i]->Release();
		m_nColliders = 0;*/

		{//if (m_length>0) {
			for(i=0;i<2;i++) {
				pbody = m_pTiedTo[i]->GetRigidBodyTrans(&body,m_iTiedPart[i],this);
				vrope[i] = m_vtx[i*(m_nVtx-2)].dir*(m_vtx[i*(m_nVtx-1)].vel = pbody->v + (pbody->w ^ m_vtx[i*(m_nVtx-1)].pt-pbody->pos));
			}
			for(i=0; i<m_nVtx-1; ) {
				for(j=i+1; j<m_nVtx-1 && is_unused(m_vtx[j].ncontact); j++);
				len = (m_vtx[j].pt-m_vtx[0].pt).len2(); t = (m_vtx[j].pt-m_vtx[m_nVtx-1].pt).len2(); t=t/(len+t);
				len = (dir=m_vtx[j].pt-m_vtx[i].pt).len2(); dir *= len>1e-8f ? 1/len : 0.0f;
				if (j<m_nVtx-1) {
					pbody = m_vtx[j].pContactEnt->GetRigidBodyTrans(&body,m_vtx[j].iContactPart,this);
					v = pbody->v + (pbody->w^m_vtx[j].pt-pbody->pos);
					norm = (m_vtx[j-1].dir-m_vtx[j].dir).normalized();
					tang = (m_vtx[j-1].dir+m_vtx[j].dir).normalized();
					m_vtx[j].vel = norm*(v*norm) + tang*(vrope[0]*(1.0f-t)+vrope[1]*t);	
				}
				for(i0=i++; i<j; i++) {
					t = dir*(m_vtx[i].pt-m_vtx[i0].pt);
					m_vtx[i].vel = m_vtx[i0].vel*(1-t) + m_vtx[j].vel*t;
				}
			}
			for(i=0;i<=m_nSegs;i++)
				m_segs[i].vel = m_vtx[m_segs[i].iVtx0].vel;
		}

		for(len=0; m_pContact; m_pContact=m_pContact->next)	{
			len = max(len,m_pContact->Pspare);
			for(i=0;i<m_pContact->iCount;i++)	{
				j = ((rope_solver_vtx*)m_pContact->pBounceCount)[i].ivtx;
				if (m_vtx[j].pContactEnt && m_vtx[j].pContactEnt->m_parts[m_vtx[j].iContactPart].flags & geom_monitor_contacts)
					m_vtx[j].pContactEnt->OnContactResolved(m_pContact,i+2,-1);
			}
		}
		if (m_maxForce>0 && len>m_maxForce*max(0.01f,time_interval) && !(m_flags & rope_no_tears)) {
			EventPhysJointBroken epjb;
			epjb.idJoint=0; epjb.bJoint=0; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
			epjb.pEntity[0]=epjb.pEntity[1]=this; epjb.pForeignData[0]=epjb.pForeignData[1]=m_pForeignData; 
			epjb.iForeignData[0]=epjb.iForeignData[1]=m_iForeignData;
			epjb.pt = m_segs[m_nSegs].pt;	epjb.n = m_segs[m_nSegs-1].dir;
			epjb.partid[0] = 1;
			epjb.partid[1] = m_pTiedTo[1]->m_parts[m_iTiedPart[1]].id;
			epjb.partmat[0]=epjb.partmat[1] = -1;
			m_pWorld->OnEvent(2,&epjb);

			pe_params_rope pr;
			pr.pEntTiedTo[1] = 0;
			SetParams(&pr);
		}
	}
	m_pContact = 0;
	return 1;
}

static geom_contact g_RopeContact[MAX_PHYS_THREADS+1];

int CRopeEntity::RayTrace(SRayTraceRes& rtr)
{
	ReadLock lock0(m_lockVtx),lock(m_lockUpdate);
	Vec3 dp,dir,l,pt;
	strided_pointer<Vec3> vtx;
	float t,t1,llen2,raylen=rtr.pRay->m_ray.dir*rtr.pRay->m_dirn; 
	int i,nSegs;
	if (m_nVtx==0) {
		vtx = strided_pointer<Vec3>(&m_segs[0].pt0, sizeof(rope_segment));
		nSegs = m_nSegs;
	}	else {
		vtx = strided_pointer<Vec3>(&m_vtx[0].pt, sizeof(rope_vtx));
		nSegs = m_nVtx-1;
	}

	for(i=0;i<nSegs;i++) {
		dp = rtr.pRay->m_ray.origin-vtx[i];
		dir = vtx[i+1]-vtx[i];
		if ((dp^rtr.pRay->m_dirn).len2()<sqr(m_collDist) && inrange((vtx[i]-rtr.pRay->m_ray.origin)*rtr.pRay->m_dirn, 0.0f,raylen)) {
			pt = vtx[i]; break;
		}
		l = dir^rtr.pRay->m_dirn; llen2 = l.len2();
		t = (dp^rtr.pRay->m_dirn)*l; t1 = (dp^dir)*l;
		if (sqr(dp*l)<sqr(m_collDist)*llen2 && inrange(t, 0.0f,llen2) && inrange(t1, 0.0f,llen2*raylen)) {
			pt = vtx[i]+dir*(t/llen2); break;
		}
	}

	if (i<m_nSegs) {
		rtr.pcontacts = &g_RopeContact[get_iCaller()];
		rtr.pcontacts->n = (dir^(dir^rtr.pRay->m_dirn)).normalized();
		rtr.pcontacts->pt = pt + rtr.pcontacts->n*m_collDist;
		rtr.pcontacts->t = (pt-rtr.pRay->m_ray.origin)*rtr.pRay->m_dirn;
		rtr.pcontacts->id[0] = m_surface_idx;
		rtr.pcontacts->iNode[0] = i;
		return 1;
	}

	return 0;
}

void CRopeEntity::ApplyVolumetricPressure(const Vec3 &epicenter, float kr, float rmin)
{
	if (m_nSegs>0 && m_maxForce>0 && m_pTiedTo[0] && m_pTiedTo[1]) {
		int i;
		float dP;
		Vec3 r;
		int bThreadSafe = -(get_iCaller()-MAX_PHYS_THREADS>>31);
		for(i=0,dP=0; i<m_nSegs; i++) {
			r = (m_segs[i].pt+m_segs[i+1].pt)*0.5f-epicenter;
			dP += (m_segs[i].dir^r).len()/(r.len()*max(r.len2(),rmin*rmin));
		}
		if (dP*kr*m_length*m_collDist*2 > m_maxForce*0.01f) {
			EventPhysJointBroken epjb;
			epjb.idJoint=0; epjb.bJoint=0; MARK_UNUSED epjb.pNewEntity[0],epjb.pNewEntity[1];
			epjb.pEntity[0]=epjb.pEntity[1]=this; epjb.pForeignData[0]=epjb.pForeignData[1]=m_pForeignData; 
			epjb.iForeignData[0]=epjb.iForeignData[1]=m_iForeignData;
			epjb.pt = m_segs[m_nSegs].pt;	epjb.n = m_segs[m_nSegs-1].dir;
			epjb.partid[0] = isneg((m_segs[m_nSegs].pt-epicenter).len2()-(m_segs[0].pt-epicenter).len2());
			epjb.partid[1] = m_pTiedTo[epjb.partid[0]]->m_parts[m_iTiedPart[epjb.partid[0]]].id;
			epjb.partmat[0]=epjb.partmat[1] = -1;
			m_pWorld->OnEvent(2,&epjb);

			pe_params_rope pr;
			pr.pEntTiedTo[epjb.partid[0]] = 0;
			SetParams(&pr, bThreadSafe);
		}
		pe_action_awake pa;
		Action(&pa, bThreadSafe);
	}
}


int CRopeEntity::GetStateSnapshot(CStream &stm,float time_back,int flags)
{
	ReadLock lock(m_lockUpdate);
	stm.WriteNumberInBits(SNAPSHOT_VERSION,4);
	WritePacked(stm,m_nSegs);

	if (m_nSegs>0) for(int i=0; i<=m_nSegs; i++) {
		stm.Write(m_segs[i].pt);
		if (m_segs[i].vel.len2()>0) {
			stm.Write(true);
			stm.Write(m_segs[i].vel);
		} else stm.Write(false);
	}
	stm.Write(m_bAwake!=0);

	return 1;
}


int CRopeEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	int i,ver=0; 
	bool bnz;

	stm.ReadNumberInBits(ver,4);
	if (ver!=SNAPSHOT_VERSION)
		return 0;
	ReadPacked(stm,i);
	if (i!=m_nSegs)
		return 0;
	WriteLock lock(m_lockUpdate);

	if (!(flags & ssf_no_update)) {
		if (m_nSegs>0) for(i=0; i<=m_nSegs; i++) {
			stm.Read(m_segs[i].pt);
			stm.Read(bnz); if (bnz) 
				stm.Read(m_segs[i].vel);
			else
				m_segs[i].vel.zero();
			if (m_segs[i].pContactEnt) {
				m_segs[i].pContactEnt->Release();
				m_segs[i].pContactEnt = 0;
			}
		}
		stm.Read(bnz);
		m_bAwake = bnz ? 1:0;

		for(i=0;i<m_nSegs;i++)
			m_segs[i].dir = (m_segs[i+1].pt-m_segs[i].pt).normalized();
		m_pos = m_segs[0].pt;
		if (m_flags & pef_traceable)
			m_pWorld->RepositionEntity(this,3|8);
	} else {
		for(i=0;i<=m_nSegs;i++) {
			stm.Seek(stm.GetReadPos()+sizeof(Vec3)*8);
			stm.Read(bnz); if (bnz)
				stm.Seek(stm.GetReadPos()+sizeof(Vec3)*8);
		}
		stm.Read(bnz);
	}

	return 1;
}


void CRopeEntity::OnNeighbourSplit(CPhysicalEntity *pentOrig, CPhysicalEntity *pentNew)
{
	int i,ipart;
	CPhysicalEntity *pent;
	if (m_szSensor<=0.0f) 
		return;
	for(i=0;i<2;i++) if (m_pTiedTo[i]==pentOrig) {
		pe_params_rope pr;
		pent = pentNew ? pentNew:pentOrig;
		ipart = pent->TouchesSphere(m_segs[i*m_nSegs].pt,m_szSensor);
		if (ipart>=0) {
			pr.pEntTiedTo[i] = pent;
			pr.idPartTiedTo[i] = pent->m_parts[ipart].id;
		} else if (!pentNew)
			pr.pEntTiedTo[i] = 0;
		else 
			continue;
		SetParams(&pr);
		Awake();
	}
}


int CRopeEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	int i;
	bool bAwake;
	ser.Value("nsegs", m_nSegs);
	ser.Value("length", m_length);

	if (m_nSegs<=0) {
		ser.Value("strained", bAwake = true);
		ser.Value("pos_start", m_pos);
		ser.Value("pos_end", m_pos);
	} else if (!m_bAwake && (m_segs[m_nSegs].pt-m_segs[0].pt).len2() > sqr(m_length)) {
		ser.Value("strained", bAwake = true);
		ser.Value("pos_start", m_segs[0].pt);
		ser.Value("pos_end", m_segs[m_nSegs].pt);
	} else {
		ser.Value("strained", bAwake = false);
		ser.Value("awake", bAwake = m_bAwake!=0);

		ser.BeginGroup("Segs");
		for(i=0;i<=m_nSegs;i++) {
			ser.Value(numbered_tag("pos",i), m_segs[i].pt);
			if (m_bAwake)
				ser.Value(numbered_tag("vel",i), m_segs[i].vel);
		}
		ser.EndGroup();

		ser.Value("nvtx", m_nVtx);
		if (m_nVtx > 0)
		{
			ser.BeginGroup("Verts");
			for(i=0;i<m_nVtx;i++) {
				ser.Value(numbered_tag("pos",i), m_vtx[i].pt);
				if (m_bAwake)
					ser.Value(numbered_tag("vel",i), m_vtx[i].vel);
			}
			ser.EndGroup();
		}

		ser.Value("pt_tied0", m_ptTiedLoc[0]);
		ser.Value("pt_tied1", m_ptTiedLoc[1]);
	}

	if (!m_bTargetPoseActive && (m_pTiedTo[0] && m_pTiedTo[1] || 
			m_pTiedTo[0] && m_pTiedTo[0]->m_iSimClass<3 && (!m_pForeignData || m_pForeignData!=m_pTiedTo[0]->m_pForeignData) || 
			m_pTiedTo[1] && m_pTiedTo[1]->m_iSimClass<3 && (!m_pForeignData || m_pForeignData!=m_pTiedTo[1]->m_pForeignData)))
	{
		ser.Value("saveties", bAwake=true);
		for(i=0;i<2;i++) {
			ser.BeginGroup("link");
			if (m_pTiedTo[i]) {
				ser.Value("tied", bAwake=true);
				m_pWorld->SavePhysicalEntityPtr(ser, m_pTiedTo[i]);
				ser.Value("ipart", m_iTiedPart[i]);
			}	else
				ser.Value("tied", bAwake=false);
			ser.EndGroup();
		}
	} else
		ser.Value("saveties", bAwake=false);

	return 1;
}

int CRopeEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	int i;
	bool bAwake;
	int nSegs;

	ser.Value("nsegs", nSegs);
	if ((unsigned int)nSegs>300u)
		return 0;
	ser.Value("length", m_length);

	if (m_nSegs!=nSegs) {
		if (m_segs) delete[] m_segs;
		memset(m_segs = new rope_segment[nSegs+1], 0, (nSegs+1)*sizeof(rope_segment));
		m_nSegs = nSegs;
	}

	// cppcheck-suppress autovarInvalidDeallocation
	if (m_vtx) delete[] m_vtx; m_vtx=0;
	if (m_vtx1) delete[] m_vtx1; m_vtx1=0;
	if (m_vtxSolver) delete[] m_vtxSolver; m_vtxSolver=0;

	if (m_flags & rope_subdivide_segs) {
		m_nVtxAlloc=(m_nSegs+1)*2;
		memset(m_vtx =new rope_vtx[m_nVtxAlloc], 0, m_nVtxAlloc*sizeof(rope_vtx));
		memset(m_vtx1=new rope_vtx[m_nVtxAlloc], 0, m_nVtxAlloc*sizeof(rope_vtx));
		for(i=0;i<=m_nSegs;i++)
			m_vtx[i].pt=m_segs[i].pt, MARK_UNUSED m_vtx[i].ncontact;
		m_nVtx = m_nVtx0 = m_nSegs+1;
	}

	ser.Value("strained", bAwake);
	if (bAwake) {
		ser.Value("pos_start", m_segs[0].pt);
		ser.Value("pos_end", m_segs[m_nSegs].pt);
		Vec3 step = (m_segs[m_nSegs].pt-m_segs[0].pt)/m_nSegs;
		for(i=1;i<m_nSegs;i++)
			m_segs[i].pt0=m_segs[i].pt=m_segs[0].pt+step*i, m_segs[i].vel.zero();
		m_segs[0].vel.zero(); m_segs[m_nSegs].vel.zero();
		m_nVtx=m_nVtx0 = 0;
	} else {
		ser.Value("awake", bAwake);
		m_bAwake = bAwake;

		ser.BeginGroup("Segs");
		for(i=0;i<=m_nSegs;i++) {
			ser.Value(numbered_tag("pos",i), m_segs[i].pt);
			m_segs[i].pt0 = m_segs[i].pt;
			if (m_bAwake)
				ser.Value(numbered_tag("vel",i), m_segs[i].vel);
			else m_segs[i].vel.zero();
			if (m_segs[i].pContactEnt) {
				if (!(m_flags & rope_subdivide_segs))
					m_segs[i].pContactEnt->Release();
				m_segs[i].pContactEnt = 0;
			}
			MARK_UNUSED m_segs[i].ncontact;
		}
		ser.EndGroup();

		ser.Value("nvtx", m_nVtx);
		m_nVtx0 = m_nVtx;
		if (m_nVtx>0) {
			if (m_nVtx > m_nVtxAlloc) {	// cppcheck-suppress autovarInvalidDeallocation
				delete[] m_vtx; delete[] m_vtx1; m_nVtxAlloc = m_nVtx;
				memset(m_vtx =new rope_vtx[m_nVtxAlloc], 0, m_nVtxAlloc*sizeof(rope_vtx));
				memset(m_vtx1=new rope_vtx[m_nVtxAlloc], 0, m_nVtxAlloc*sizeof(rope_vtx));
			}
			m_vtxSolver = new rope_solver_vtx[m_nVtxAlloc];

			ser.BeginGroup("Verts");
			for(i=0;i<m_nVtx;i++) {
				ser.Value(numbered_tag("pos",i), m_vtx[i].pt);
				m_vtx[i].pt0 = m_vtx[i].pt;
				if (m_bAwake)
					ser.Value(numbered_tag("vel",i), m_vtx[i].vel);
				MARK_UNUSED m_vtx[i].ncontact;
			}
			ser.EndGroup();
		}

		ser.Value("pt_tied0", m_ptTiedLoc[0]);
		ser.Value("pt_tied1", m_ptTiedLoc[1]);
	}

	ser.Value("saveties", bAwake);
	if (bAwake) {
		pe_params_rope pr;
		for(i=0;i<2;i++) 
		{
			ser.BeginGroup("link");
			ser.Value("tied", bAwake);
			if (bAwake) {
				pr.pEntTiedTo[i] = m_pWorld->LoadPhysicalEntityPtr(ser);
				ser.Value("ipart", pr.idPartTiedTo[i]);
				if (pr.pEntTiedTo[i] && pr.pEntTiedTo[i]->GetType()!=PE_ROPE)
					pr.idPartTiedTo[i] = ((CPhysicalEntity*)pr.pEntTiedTo[i])->m_parts[pr.idPartTiedTo[i]].id;
			} else
				pr.pEntTiedTo[i] = 0;
			ser.EndGroup();
		}
		SetParams(&pr);
	}
	float scale;
	if (m_pTiedTo[0])
		m_pTiedTo[0]->GetLocTransform(m_iTiedPart[0],m_lastposHost,m_lastqHost,scale,this);

	m_BBox[0]=m_BBox[1] = m_segs[m_nSegs].pt;
	for(i=0;i<m_nSegs;i++) {
		m_segs[i].dir = (m_segs[i+1].pt-m_segs[i].pt).normalized();
		m_BBox[0] = min(m_BBox[0], m_segs[i].pt);
		m_BBox[1] = max(m_BBox[1], m_segs[i].pt);
	}
	m_pos = m_segs[0].pt;
	if (m_flags & pef_traceable)
		m_pWorld->RepositionEntity(this,3|8);

	return 1;
}


void CRopeEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	CPhysicalEntity::DrawHelperInformation(pRenderer,flags);

	int i;
	flags |= 16;
	if (flags & pe_helper_geometry) {
		ReadLock lock(m_lockUpdate);
		int nSegs;
		strided_pointer<Vec3> vtx;
		if (m_flags & rope_subdivide_segs && m_nVtx>0) { 
			vtx = strided_pointer<Vec3>(&m_vtx[0].pt0,sizeof(rope_vtx));
			nSegs = m_nVtx0-1;
		} else { 
			vtx = strided_pointer<Vec3>(&m_segs[0].pt0,sizeof(rope_segment));
			nSegs = m_nSegs;
		}
		if (flags & 16) {
			cylinder cyl; cyl.r=m_collDist;
			CCylinderGeom cylGeom;
			for(i=0;i<nSegs;i++) {
				cyl.center = (vtx[i]+vtx[i+1])*0.5f;
				cyl.axis = vtx[i+1]-vtx[i];
				cyl.hh=cyl.axis.len(); cyl.axis/=max(1e-10f,cyl.hh); cyl.hh*=0.5f;
				float gap = max(cyl.hh*0.02f,m_pWorld->m_vars.maxContactGap*0.5f);
				int iend = i-1>>31 | (i+1-m_nSegs>>31)+1;
				cyl.center += cyl.axis*gap*iend*0.5f;
				cyl.hh -= gap*(1-(iend&1)*0.5f);
				cylGeom.CreateCylinder(&cyl);
				pRenderer->DrawGeometry(&cylGeom,0,m_iSimClass);
			}
		} else for(i=0;i<nSegs;i++)
			pRenderer->DrawLine(vtx[i],vtx[i+1],m_iSimClass);
		if (m_flags & rope_subdivide_segs && flags & 16) for(i=0;i<m_nSegs;i++) //if (m_segs[i+1].iVtx0>m_segs[i].iVtx0+1)
			pRenderer->DrawLine(m_segs[i].pt,m_segs[i+1].pt,m_iSimClass);
	}
	if (flags & pe_helper_collisions) if (!(m_flags & rope_subdivide_segs)) {
		for(i=0;i<m_nSegs;i++) if (m_segs[i].pContactEnt) {
			Vec3 pt = m_segs[i].pt+(m_segs[i+1].pt-m_segs[i].pt)*m_segs[i].tcontact;
			pRenderer->DrawLine(pt,pt+m_segs[i].ncontact*m_pWorld->m_vars.maxContactGap*30,m_iSimClass);
		}
	} else {
		for(i=0;i<m_nVtx;i++) if (!is_unused(m_vtx[i].ncontact))
			pRenderer->DrawLine(m_vtx[i].pt,m_vtx[i].pt+m_vtx[i].ncontact*m_pWorld->m_vars.maxContactGap*30,m_iSimClass);
		/*if (m_pMesh) {
			geom_world_data gwd;
			gwd.offset = m_lastMeshOffs;
			m_pMesh->DrawWireframe(pRenderer,&gwd,0,m_iSimClass|256);
		}*/
	}
}


void CRopeEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_ROPE)
		pSizer->AddObject(this, sizeof(CRopeEntity));
	CPhysicalEntity::GetMemoryStatistics(pSizer);
	pSizer->AddObject(m_segs, (m_nSegs+1)*sizeof(m_segs[0]));
	pSizer->AddObject(m_vtx, m_nVtxAlloc*sizeof(m_vtx[0]));
	pSizer->AddObject(m_vtx1, m_nVtxAlloc*sizeof(m_vtx1[0]));
	if (m_vtxSolver)
		pSizer->AddObject(m_vtxSolver, m_nVtxAlloc*sizeof(m_vtxSolver[0]));
	if (m_idx)
		pSizer->AddObject(m_idx, (m_nMaxSubVtx+2)*sizeof(m_idx[0]));
	pSizer->AddObject(m_attach, m_nAttach*sizeof(m_attach[0]));
}

int CRopeEntity::GetVertices(strided_pointer<Vec3>& vtx) const
{
	if (m_flags & rope_subdivide_segs && m_nVtx > 0) {
		vtx = strided_pointer<Vec3>(&m_vtx[0].pt0, sizeof(rope_vtx));
		return m_nVtx0;
	} else { 
		vtx = strided_pointer<Vec3>(&m_segs[0].pt0, sizeof(rope_segment));
		return m_nSegs+1;
	}
}

float CRopeEntity::GetExtent(EGeomForm eForm)	const
{
	strided_pointer<Vec3> vtx;

	if (eForm == GeomForm_Vertices)
		return GetVertices(vtx);

	CGeomExtent& ext = m_Extents.Make(GeomForm_Edges);
	if (!ext) {
		WriteLock lock(m_lockUpdate);
		int nSegs = GetVertices(vtx)-1;
		ext.ReserveParts(nSegs);
		for (int i=0;i<nSegs;i++)
			ext.AddPart((vtx[i+1]-vtx[i]).len());
	}
	return ext.TotalExtent();
}

void CRopeEntity::GetRandomPoints(Array<PosNorm> points, CRndGen& seed, EGeomForm eForm) const
{

	CGeomExtent const& ext = m_Extents[GeomForm_Edges];
	if (eForm != GeomForm_Vertices && !ext.NumParts())
		return points.fill(ZERO);

	strided_pointer<Vec3> vtx;
	int nVerts = GetVertices(vtx);

	for (auto& ran : points) {
		Vec3 dir;
		if (eForm == GeomForm_Vertices) {
			int i = seed.GetRandom(0, nVerts - 1);
			ran.vPos = vtx[i];
			dir = (vtx[min(i+1,nVerts-1)] - vtx[max(i-1,0)]).normalized();
		}
		else {
			int i = ext.RandomPart(seed);
			ran.vPos = vtx[i]+(vtx[i+1]-vtx[i])*seed.GetRandom(0.0f, 1.0f);
			dir = (vtx[i+1]-vtx[i]).normalized();
		}

		Vec3 axisx = dir.GetOrthogonal().normalized(), axisy = dir^axisx;
		float angle = seed.GetRandom(0.0f, 2.0f*gf_PI);
		ran.vNorm = axisx*cos_tpl(angle)+axisy*sin_tpl(angle);
	}
}

#undef m_bAwake
#undef CMemStream
