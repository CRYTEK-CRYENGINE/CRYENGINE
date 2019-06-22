// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "singleboxtree.h"
#include "cylindergeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h" 
#include "geoman.h"
#include "physicalworld.h"
#include "rigidentity.h"
#include "articulatedentity.h"
#include "qpOASES.hpp"

typedef Vec3_tpl<qpOASES::real_t> Vec3qp;

int __ae_step=0; // for debugging

CArticulatedEntity::CArticulatedEntity(CPhysicalWorld *pWorld, IGeneralMemoryHeap* pHeap)
	: CRigidEntity(pWorld, pHeap)
	, m_infos(nullptr)
	, m_joints(nullptr)
	, m_nJoints(0)
	, m_nJointsAlloc(0)
	, m_posPivot(ZERO)
	, m_offsPivot(ZERO)
	, m_acc(ZERO)
	, m_wacc(ZERO)
	, m_simTime(0.0f)
	, m_simTimeAux(10.0f)
	, m_scaleBounceResponse(1.0f)
	, m_bGrounded(0)
	, m_nRoots(0)
	, m_bInheritVel(0)
	, m_pHost(nullptr)
	, m_posHostPivot(ZERO) 
	, m_qHostPivot(IDENTITY)
	, m_velHost(ZERO)
	, m_bCheckCollisions(0)
	, m_bFeatherstone(1)
	, m_bExertImpulse(0)
	, m_iSimType(1)
	, m_iSimTypeLyingMode(1)
	, m_iSimTypeCur(1)
	, m_iSimTypeOverride(0)
	, m_bIaReady(0)
	, m_bPartPosForced(0)
	, m_bFastLimbs(0)
	, m_maxPenetrationCur(0.0f)
	, m_bUsingUnproj(0)
	, m_bUpdateBodies(1)
	, m_nDynContacts(0)
	, m_bInGroup(0)
	, m_bIgnoreCommands(0)
	, m_Ejoints(0.0f)
	, m_nCollLyingMode(5)
	, m_gravityLyingMode(m_gravity)
	, m_dampingLyingMode(0.2f)
	, m_EminLyingMode (sqr(0.12f))
	, m_nBodyContacts(0)
	, m_rhistTime(0.0f)
	, m_lockJoints(0)
	, m_pCollEntList(nullptr)
	, m_nCollEnts(0)
	, m_bContactsAssigned(0)
{	
	//CPhysicalPlaceholder
	m_iSimClass = 4;

	//CPhysicalEntity
	m_flags &= ~pef_never_break;
	m_collisionClass.type |= collision_class_articulated;

	//CRigidEntity
	m_bAwake = 0;
	m_Emin = sqr(0.025f);
	m_damping = 0.1f;
	m_bCanSweep = 0;
	m_iVarPart0 = 0;
	
	//CArticulatedEntity
	static_assert(CRY_ARRAY_COUNT(m_Ya_vec) == 2, "Invalid array size!");
	m_Ya_vec[0].Set(0,0,0);
	m_Ya_vec[1].Set(0,0,0);

	static_assert(CRY_ARRAY_COUNT(m_posHist) == 2, "Invalid array size!");
	static_assert(CRY_ARRAY_COUNT(m_qHist) == 2, "Invalid array size!");
	m_posHist[0].zero(); m_qHist[0].SetIdentity();
	m_posHist[1].zero(); m_qHist[1].SetIdentity();
}

CArticulatedEntity::~CArticulatedEntity()
{
	if (m_joints) delete[] m_joints;
	if (m_infos) delete[] m_infos;
	if (m_pSrc && m_pWorld) m_pWorld->DestroyPhysicalEntity(m_pSrc,4);
}

void CArticulatedEntity::AlertNeighbourhoodND(int mode)
{
	if (m_pHost) {
		m_pHost->RemoveCollider(this); --m_pHost->m_nSyncColliders;
		m_pHost->Release();
	}
	m_pHost = 0;
	CRigidEntity::AlertNeighbourhoodND(mode);
}

void CArticulatedEntity::AllocFSData()
{
	if (m_nJoints && !m_joints[0].fs) for(int i=0;i<m_nJoints;i++) {
		m_joints[i].fs = (featherstone_data*)_align16(m_joints[i].fsbuf = new char[sizeof(featherstone_data)+16]);
		memset(m_joints[i].fs, 0, sizeof(featherstone_data));
		IdentityBasis(alias_cast<Vec3*>(m_joints[i].fs->qinv));
	}
}

int CArticulatedEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams* _params,int id, int bThreadSafe)
{
	if (pgeom && fabs_tpl(_params->mass)+fabs_tpl(_params->density)!=0 && (pgeom->V<=0 || pgeom->Ibody.x<0 || pgeom->Ibody.y<0 || pgeom->Ibody.z<0))	{
		char errmsg[256];
		cry_sprintf(errmsg, "CArticulatedEntity::AddGeometry: (%s at %.1f,%.1f,%.1f) Trying to add bad geometry",
			m_pWorld->m_pRenderer ? m_pWorld->m_pRenderer->GetForeignName(m_pForeignData,m_iForeignData,m_iForeignFlags):"", m_pos.x,m_pos.y,m_pos.z);
		VALIDATOR_LOG(m_pWorld->m_pLog,errmsg);
		return -1;
	}

	if (!pgeom)
		return 0;
	ChangeRequest<pe_geomparams> req(this,m_pWorld,_params,bThreadSafe,pgeom,id);
	if (req.IsQueued()) {
		WriteLock lock(m_lockPartIdx);
		if (id<0)
			*((int*)req.GetQueuedStruct()-1) = id = m_iLastIdx++;
		else
			m_iLastIdx = max(m_iLastIdx,id+1);
		return id;
	}

	if (_params->type!=pe_articgeomparams::type_id) {
		pe_articgeomparams params = *(pe_geomparams*)_params;
		return AddGeometry(pgeom,&params,id,bThreadSafe);
	}
	pe_articgeomparams *params = (pe_articgeomparams*)_params;

	int res,i,j,nPartsAlloc=m_nPartsAlloc,nParts=m_nParts,idx;
	if ((res = CPhysicalEntity::AddGeometry(pgeom,params,id,1))<0 || m_nParts==nParts)
		return res;

	WriteLock lock(m_lockJoints);
	idx = m_nParts-1; pgeom = m_parts[idx].pPhysGeom;
	float V=pgeom->V*cube(params->scale), M=params->mass ? params->mass : params->density*V;
	quaternionf qrot = m_bGrounded && m_bAwake && m_nJoints>0 ? m_joints[0].quat0 : m_qrot;
	Vec3 bodypos = m_pos+qrot*(params->pos+params->q*pgeom->origin*params->scale); 
	quaternionf bodyq = qrot*params->q*pgeom->q;
	//if (M<=0) M=0.1f;
	m_parts[idx].mass = M;
	m_parts[idx].flags |= geom_monitor_contacts;
	if (nPartsAlloc < m_nPartsAlloc) {
		ae_part_info *pInfos = m_infos;
		memcpy(m_infos = new ae_part_info[m_nPartsAlloc], pInfos, sizeof(ae_part_info)*nPartsAlloc);
		for(i=0;i<m_nParts;i++) {
			m_infos[i].pos=m_parts[i].pos; m_infos[i].q=m_parts[i].q; m_infos[i].scale=m_parts[i].scale;
			m_infos[i].BBox[0]=m_parts[i].BBox[0]; m_infos[i].BBox[1]=m_parts[i].BBox[1];
			m_parts[i].pNewCoords = (coord_block_BBox*)&m_infos[i].pos;
			m_infos[i].posHist[0]=m_infos[i].posHist[1]=m_parts[i].pos; m_infos[i].qHist[0]=m_infos[i].qHist[1]=m_parts[i].q;
		}
		delete[] pInfos;
	}
	MEMSTAT_USAGE(m_infos, sizeof(ae_part_info) * m_nParts);

	for(i=0;i<m_nJoints && m_joints[i].idbody!=params->idbody;i++);
	if (i==m_nJoints) {
		if (m_nJoints==m_nJointsAlloc) 
			ReallocateList(m_joints, m_nJoints,m_nJointsAlloc+=4);
		m_joints[m_nJoints].iStartPart = idx;
		m_joints[m_nJoints].nParts = 0;
		if (m_joints[0].fs) {
			m_joints[m_nJoints].fs = (featherstone_data*)_align16(m_joints[i].fsbuf = new char[sizeof(featherstone_data)+16]);
			union {Vec3* pv; float *pf;} u; u.pf = m_joints[m_nJoints].fs->qinv;
			memset(u.pv, 0, sizeof(featherstone_data));
			IdentityBasis(u.pv);
		}
		m_joints[m_nJoints++].idbody = params->idbody;
	}
	if (m_joints[i].iStartPart+m_joints[i].nParts<idx) {
		geom tmppart = m_parts[idx];
		for(; idx>m_joints[i].iStartPart+m_joints[i].nParts; idx--) {
			m_parts[idx] = m_parts[idx-1]; m_infos[idx] = m_infos[idx-1];
			m_parts[idx].pNewCoords = (coord_block_BBox*)&m_infos[idx];
		}
		m_parts[idx] = tmppart;
		m_parts[idx].pNewCoords = (coord_block_BBox*)&m_infos[idx];
		for(j=0;j<m_nJoints;j++) if (m_joints[j].iStartPart>=idx)
			m_joints[j].iStartPart++;
		for(j=0;j<NMASKBITS && getmask(j)<=m_constraintMask;j++) if (m_constraintMask & getmask(j)) 
			for(int iop=0;iop<2;iop++) if (m_pConstraints[j].pent[iop]==this && m_pConstraints[j].ipart[iop]>=idx)
				m_pConstraints[j].ipart[iop] = m_pConstraints[j].ipart[iop]+1;
	}
	m_joints[i].nParts++;

	if (M>0) {
		Vec3 pivot = m_joints[i].body.pos+m_joints[i].quat*m_joints[i].pivot[1];
		if (m_body.M==0)
			m_body.Create(bodypos,pgeom->Ibody*sqr(params->scale)*cube(params->scale),bodyq, V,M, qrot,m_pos);
		else
			m_body.Add(bodypos,pgeom->Ibody*sqr(params->scale)*cube(params->scale),bodyq,V,M);	

		if (m_joints[i].body.M==0)
			m_joints[i].body.Create(bodypos,pgeom->Ibody*sqr(params->scale)*cube(params->scale),bodyq, V,M, qrot*params->q,bodypos);
		else
			m_joints[i].body.Add(bodypos,pgeom->Ibody*sqr(params->scale)*cube(params->scale),bodyq,V,M);	
		if (m_joints[i].nParts>1) {
			for(j=m_joints[i].iStartPart; j<m_joints[i].iStartPart+m_joints[i].nParts-1; j++)
				m_infos[j].pos0 = (m_pos+m_qrot*m_parts[j].pos-m_joints[i].body.pos)*m_joints[i].quat;
			m_joints[i].pivot[1] = (pivot-m_joints[i].body.pos)*m_joints[i].quat;
		}	else
			m_joints[i].quat = m_joints[i].body.q*m_joints[i].body.qfb;
		m_joints[i].I = Matrix33(m_joints[i].body.q)*m_joints[i].body.Ibody*Matrix33(!m_joints[i].body.q);
	}	else if (m_joints[i].nParts==1) {
		m_joints[i].body.pos = bodypos;
		m_joints[i].quat = qrot*m_parts[idx].q;
	}
	m_infos[idx].iJoint = i;
	m_infos[idx].idbody = params->idbody;
	m_infos[idx].q0 = !m_joints[i].quat*(qrot*m_parts[idx].q);
	m_infos[idx].pos0 = (m_pos+qrot*m_parts[idx].pos-m_joints[i].body.pos)*m_joints[i].quat;
	m_infos[idx].pos=m_parts[idx].pos; m_infos[idx].q=m_parts[idx].q; m_infos[idx].scale=m_parts[idx].scale;
	m_infos[idx].BBox[0]=m_parts[idx].BBox[0]; m_infos[idx].BBox[1]=m_parts[idx].BBox[1];
	m_infos[idx].posHist[1].zero(); m_infos[idx].qHist[1].SetIdentity();
	m_parts[idx].pNewCoords = (coord_block_BBox*)&m_infos[idx].pos;
	
	return res;
}

void CArticulatedEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this,m_pWorld,0,bThreadSafe,0,id);
	if (req.IsQueued())
		return;
	int i,j;

	{ 
		WriteLock lockJoints(m_lockJoints);
		for(i=0;i<m_nParts && m_parts[i].id!=id;i++);
		if (i==m_nParts) return;	
		phys_geometry *pgeom = m_parts[i].pPhysGeom;

		if (m_parts[i].mass>0) {
			quaternionf qrot = m_bGrounded && m_bAwake ? m_joints[0].quat0 : m_qrot;
			Vec3 bodypos = m_pos + qrot*(m_parts[i].pos+m_parts[i].q*pgeom->origin); 
			quaternionf bodyq = qrot*m_parts[i].q*pgeom->q;
			m_body.Add(bodypos,-pgeom->Ibody*cube(m_parts[i].scale)*sqr(m_parts[i].scale),bodyq,-pgeom->V*cube(m_parts[i].scale),-m_parts[i].mass);
			int iJoint = m_infos[i].iJoint;
			Vec3 pivot = m_joints[iJoint].body.pos+m_joints[iJoint].quat*m_joints[iJoint].pivot[1];
			m_joints[iJoint].body.zero();
			for(j=m_joints[iJoint].iStartPart; j<m_joints[iJoint].iStartPart+m_joints[iJoint].nParts; j++) if (j!=i) {
				bodypos = m_pos + qrot*(m_parts[j].pos+m_parts[j].q*m_parts[j].pPhysGeom->origin); 
				bodyq = qrot*m_parts[j].q*m_parts[j].pPhysGeom->q;
				pgeom = m_parts[j].pPhysGeom;
				if (m_joints[iJoint].body.M==0)
					m_joints[iJoint].body.Create(bodypos,pgeom->Ibody*sqr(m_parts[j].scale)*cube(m_parts[j].scale),bodyq, pgeom->V*cube(m_parts[j].scale),m_parts[j].mass, qrot*m_parts[j].q,bodypos);
				else
					m_joints[iJoint].body.Add(bodypos,pgeom->Ibody*sqr(m_parts[j].scale)*cube(m_parts[j].scale),bodyq,pgeom->V*cube(m_parts[j].scale),m_parts[j].mass);	
			}
			if (m_body.M<1E-8) m_body.M = 0;
			if (m_joints[iJoint].body.M<1E-8) m_joints[iJoint].body.M = 0;
			for(j=m_joints[iJoint].iStartPart; j<m_joints[iJoint].iStartPart+m_joints[iJoint].nParts; j++)
				m_infos[j].pos0 = (m_pos+m_qrot*m_parts[j].pos-m_joints[iJoint].body.pos)*m_joints[iJoint].quat;
			m_joints[iJoint].pivot[1] = (pivot-m_joints[iJoint].body.pos)*m_joints[iJoint].quat;
			m_joints[iJoint].I = Matrix33(m_joints[iJoint].body.q)*m_joints[iJoint].body.Ibody*Matrix33(!m_joints[iJoint].body.q);
		}
		if (--m_joints[j=m_infos[i].iJoint].nParts<=0 && !m_joints[j].nChildren) {
			for(int ij=0;ij<m_nJoints;ij++) 
				m_joints[ij].iParent -= isneg(j-m_joints[ij].iParent);
			if (m_joints[j].iParent>=0)
				m_joints[m_joints[j].iParent].nChildren--, m_joints[m_joints[j].iParent].nChildrenTree--;
			memmove(m_joints+j, m_joints+j+1, (--m_nJoints-j)*sizeof(ae_joint));
			for(int ipart=0;ipart<m_nParts;ipart++)
				m_infos[ipart].iJoint -= isneg(j-m_infos[ipart].iJoint);
		}
		for(j=0;j<m_nJoints;j++) if (m_joints[j].iStartPart>i)
			m_joints[j].iStartPart--;
		memmove(m_infos+i, m_infos+i+1, (m_nParts-i-1)*sizeof(m_infos[0]));
	}

	CPhysicalEntity::RemoveGeometry(id,1);

	for(i=0;i<m_nParts;i++)
		m_parts[i].pNewCoords = (coord_block_BBox*)&m_infos[i].pos;
}

void CArticulatedEntity::RecomputeMassDistribution(int ipart,int bMassChanged)
{
	int i,iStart,iEnd;
	float V,M0=m_body.M;
	Vec3 bodypos;
	quaternionf bodyq;
	Matrix33 R;

	WriteLock lock(m_lockJoints);

	if (ipart>=0) {
		i = m_infos[ipart].iJoint;
		if (!bMassChanged) {
			if (m_joints[i].iStartPart!=i)
				return;
			m_joints[i].quat = m_parts[ipart].q*!m_infos[ipart].q0;
			m_joints[i].body.q = m_qrot*m_joints[i].quat*!m_joints[i].body.qfb;
			m_joints[i].body.pos = m_qrot*(m_parts[ipart].pos-m_joints[i].quat*m_infos[ipart].pos0)+m_pos;
			R = Matrix33(m_joints[i].body.q);
			m_joints[i].I = R*m_joints[i].body.Ibody*R.T();
			return;
		}
		m_joints[i].body.zero();
		iStart = m_joints[i].iStartPart; iEnd = m_joints[i].iStartPart+m_joints[i].nParts-1;
	}	else {
		m_body.zero();
		iStart = 0; iEnd = m_nParts-1;
		for(i=0;i<m_nJoints;i++) {
			m_joints[i].prev_v = m_joints[i].body.v;
			m_joints[i].prev_w = m_joints[i].body.w;
			m_joints[i].body.zero();
		}
	}

	for(i=iStart; i<=iEnd; i++) {
		V = m_parts[i].pPhysGeom->V*cube(m_parts[i].scale);
		bodypos = m_pos + m_qrot*(m_parts[i].pos+m_parts[i].q*m_parts[i].pPhysGeom->origin); 
		bodyq = m_qrot*m_parts[i].q*m_parts[i].pPhysGeom->q;

		if (m_parts[i].mass>0) {
			if (ipart<0) {
				if (m_body.M==0)
					m_body.Create(bodypos,m_parts[i].pPhysGeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale),bodyq, V,m_parts[i].mass, m_qrot,m_pos);
				else
					m_body.Add(bodypos,m_parts[i].pPhysGeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale),bodyq, V,m_parts[i].mass);
			}

			if (m_joints[m_infos[i].iJoint].body.M==0)
				m_joints[m_infos[i].iJoint].body.Create(bodypos,m_parts[i].pPhysGeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale),bodyq, 
					V,m_parts[i].mass, m_qrot*m_parts[i].q,bodypos);
			else
				m_joints[m_infos[i].iJoint].body.Add(bodypos,m_parts[i].pPhysGeom->Ibody*sqr(m_parts[i].scale)*cube(m_parts[i].scale),bodyq, 
					V,m_parts[i].mass);	
		}
	}

	if (ipart<0) {
		if (M0>0)
			M0 = m_body.M/M0;
		for(i=0;i<m_nJoints;i++) {
			m_joints[i].body.P = (m_joints[i].body.v=m_joints[i].prev_v)*m_joints[i].body.M; 
			m_joints[i].body.L = m_joints[i].body.q*(m_joints[i].body.Ibody*(!m_joints[i].body.q*(m_joints[i].body.w=m_joints[i].prev_w)));
			m_joints[i].Pext*=M0; m_joints[i].Lext*=M0;
			m_joints[i].Pimpact*=M0; m_joints[i].Limpact*=M0;
		}
	}
}

void CArticulatedEntity::RerootJoint(int idx, int iParent, int flagsParent,const Ang3 &qextParent)
{
	if (idx<0) return;
	pe_params_joint pj;
	int idx1=m_infos[m_pSrc->m_joints[idx].iStartPart].iJoint, iParent1=iParent>=0 ? m_infos[m_pSrc->m_joints[iParent].iStartPart].iJoint:iParent;
	int flags0 = m_joints[idx1].flags, idx0=idx, swap=0;
	Ang3 qext0 = m_joints[idx1].q+m_joints[idx1].qext;
	pj.op[0] = m_pSrc->m_joints[max(0,iParent)].idbody | iParent>>31;
	pj.op[1] = m_pSrc->m_joints[idx].idbody;
	pj.qext = Ang3(ZERO);
	m_joints[idx1].body.q.Normalize(); m_joints[idx1].quat.Normalize();

	if (iParent<0) {
		pj.flags = angle0_locked*7;;
		pj.q0 = m_pSrc->m_joints[idx].quat;
		pj.pivot = m_joints[idx1].body.pos;
		pj.limits[0]=Vec3(-1e+10f); pj.limits[1]=Vec3(1e10f);
	} else if (iParent==m_pSrc->m_joints[idx].iParent) {
		pj.flags = m_pSrc->m_joints[idx].flags;
		pj.q0 = m_pSrc->m_joints[idx].quat0;
		pj.qext = m_pSrc->m_joints[idx].qext;
		pj.pivot = m_joints[idx1].body.pos+m_joints[idx1].quat*m_pSrc->m_joints[idx].pivot[1];
		for(int iop=0;iop<2;iop++) pj.limits[iop]=m_pSrc->m_joints[idx].limits[iop];
	}	else {
		idx0 = iParent; swap = 1;
		pj.pivot = m_joints[iParent1].body.pos+m_joints[iParent1].quat*m_pSrc->m_joints[iParent].pivot[1];
		pj.q0 = !m_pSrc->m_joints[iParent].quat0;

		pj.flags = m_pSrc->m_joints[iParent].flags & ~((angle0_locked|angle0_gimbal_locked|angle0_limit_reached)*7);
		int flagsLimit = (m_joints[idx1].iLevel ? flagsParent : m_joints[idx1].flags) & angle0_limit_reached*7;
		int flagsLock = m_pSrc->m_joints[iParent].flags & angle0_locked*7;
		Vec3i axmap,axsg; for(int i=0;i<3;i++) { Vec3 ort1=pj.q0*ort[i]; axsg[i]=-sgnnz(ort1[axmap[i]=idxmax3(ort1.abs())]); }
		for(int i=0;i<3;i++) pj.flags |= (flagsLock>>i & angle0_locked)<<axmap[i] | (flagsLimit>>i & angle0_limit_reached)<<axmap[i];

		/*pj.limits[0].zero(); pj.limits[1].zero();
		for(int i=0;i<3;i++) {
			Vec3 ang[2] = { Vec3(ZERO),Vec3(ZERO) }; 
			for(int j=0;j<2;j++) if (fabs_tpl(m_pSrc->m_joints[iParent].limits[j][i]) < 1e9f) 
				ang[j] = (Vec3)Ang3::GetAnglesXYZ(Quat::CreateRotationAA(-m_pSrc->m_joints[iParent].limits[j][i],ort[i])*pj.q0);
			for(int j=0;j<3;j++) { float a0=ang[0][j],a1=ang[1][j]; ang[0][j]=min(a0,a1); ang[1][j]=max(a0,a1); }
			pj.limits[0]+=ang[0]; pj.limits[1]+=ang[1];
		}
		for(int i=0;i<3;i++) for(int j=0;j<2;j++) if (fabs_tpl(m_pSrc->m_joints[iParent].limits[j][i]) > 1e9f)
			pj.limits[j^isneg(axsg[i])][axmap[i]] = 1e10f*(j*2-1)*sgnnz(axsg[i]);*/
		for(int i=0;i<3;i++) for(int j=0;j<2;j++)
			pj.limits[j^isneg(axsg[i])][axmap[i]] = m_pSrc->m_joints[iParent].limits[j][i]*axsg[i];
	}
	pj.pivot = (pj.pivot-m_pos)*m_qrot;
	SetParams(&pj);
	idx1 = m_infos[m_pSrc->m_joints[idx].iStartPart].iJoint;
	if (iParent>=0) for(int iop=0;iop<2;iop++)
		m_joints[idx1].pivot[iop] = m_pSrc->m_joints[idx0].pivot[iop^swap];
	SyncJointWithBody(idx1,2);
	m_joints[idx1].bQuat0Changed = 1;

	int i,j,ichild;
	for(i=m_pSrc->m_joints[idx].nChildren-1; i>=0; i--) {
		for(ichild=idx+1,j=0; j<i; ichild+=m_pSrc->m_joints[ichild].nChildrenTree+1,j++);
	//for(i=0,ichild=idx+1; i<m_pSrc->m_joints[idx].nChildren; ichild+=m_pSrc->m_joints[ichild].nChildrenTree+1,i++)
		RerootJoint(ichild==iParent ? m_pSrc->m_joints[idx].iParent:ichild, idx, flags0,qext0);
	}
	if (iParent<0)
		RerootJoint(m_pSrc->m_joints[idx].iParent, idx, flags0,qext0);
}

bool CArticulatedEntity::Reroot(int inewRoot)
{
	if (!m_pSrc) {
		if (!inewRoot)
			return false;
		(m_pSrc = (CArticulatedEntity*)m_pWorld->ClonePhysicalEntity(this,false))->AddRef();
	}
	inewRoot = j2src(inewRoot);
	int part0 = m_joints[0].iStartPart;
	if (m_pSrc->m_joints[inewRoot].idbody != m_joints[0].idbody) {
		for(int i=0;i<m_nJoints;i++) {
			int iParent=m_joints[i].iParent, iParent0=m_pSrc->m_joints[j2src(i)].iParent;
			m_joints[i].iLevel = (iParent>>31|iParent0>>31) || m_joints[iParent].idbody!=m_pSrc->m_joints[iParent0].idbody;
			m_joints[i].iParent=-2; m_joints[i].nChildren=m_joints[i].nChildrenTree=0; 
		}
		int inewRoot1 = m_infos[m_pSrc->m_joints[inewRoot].iStartPart].iJoint;
		m_offsPivot = (m_posPivot = m_joints[inewRoot1].body.pos)-m_pos;
		if (m_bGrounded) {
			m_body.v = m_joints[inewRoot1].body.v; m_body.w = m_joints[inewRoot1].body.w; 
		}
		m_bGrounded = 1; 
		RerootJoint(inewRoot,-1,0,Ang3(ZERO));
		for(int i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i))
			for(int j=0;j<2;j++) if (m_pConstraints[i].pent[j]==this)
				m_pConstraints[i].pbody[j] = GetRigidBody(m_pConstraints[i].ipart[j]);
		for(entity_contact *pcont=m_pContactStart; pcont!=CONTACT_END(m_pContactStart); pcont=pcont->next)
			for(int j=0;j<2;j++) if (pcont->pent[j]==this) 
				pcont->pbody[j] = GetRigidBody(pcont->ipart[j]);
		AllocFSData();
		float Iabuf[39]; matrixf Ia(6,6,0,_align16(Iabuf)); 
		CalcBodyIa(0,Ia,0);
		PropagateImpulses(-m_body.v,0,1,-m_body.w);
		m_body.v.zero(); m_body.w.zero();
		for(int i=0;i<m_nJoints;i++)
			SyncBodyWithJoint(i,2);
		int ioldRoot = m_infos[part0].iJoint;
		if (m_bGrounded && m_joints[ioldRoot].body.v*m_gravity>0.01f) {
			Matrix33 K(ZERO);
			CalcBodiesIinv(0);
			m_joints[ioldRoot].body.GetContactMatrix(Vec3(ZERO),K);
			m_joints[ioldRoot].Pext = K.GetInverted()*-m_joints[ioldRoot].body.v;
			CollectPendingImpulses(0,part0,0);
			PropagateImpulses(Vec3(ZERO));
			for(int i=0;i<m_nJoints;i++)
				SyncBodyWithJoint(i,2);
		}
		if (m_saveVel) for(int i=0;i<m_nJoints;i++)
			m_joints[i].body.v = m_pSrc->m_joints[j2src(i)].body.v;
		m_iSimType=m_iSimTypeLyingMode = 0; 
		return true;
	}
	return false;
}

static inline Vec3 mirror(const Vec3& v, const Vec3& n) { return v-n*((n*v)*2); }
static inline Quat mirror_flipy(const Quat& src, const Vec3& n) {
	Matrix33 srcM; 
	for(int iax=0;iax<3;iax++) srcM.SetColumn(iax, mirror(const_cast<Quat&>(src).GetColumn(iax)*(1-(iax&1)*2),n));
	return Quat(srcM);
}

void CArticulatedEntity::MirrorInto(int jntSrc,int jntDst, CArticulatedEntity *pdst)
{
	if (!m_pSrc)
		(m_pSrc = (CArticulatedEntity*)m_pWorld->ClonePhysicalEntity(this,false))->AddRef();
	ae_joint *jnt = m_pSrc->m_joints;
	jnt[0].quat0.SetIdentity();
	for(int i=0;i<m_nJoints;i++) {
		jnt[i].q.SetZero();
		jnt[i].bHasExtContacts = -1-i; // mirror index
		m_pSrc->SyncBodyWithJoint(i,1);
	}
	Vec3 dir(ZERO);	dir[idxmax3((jnt[j2src(jntSrc)].body.pos-jnt[j2src(jntDst)].body.pos).abs())] = 1;
	for(int i=0,j;i<m_nJoints;i++) if (jnt[i].bHasExtContacts<0) {
		for(j=(i ? i+1:m_nJoints);j<m_nJoints;j++) if (jnt[jnt[i].iParent].bHasExtContacts==jnt[j].iParent) {
			float score = 1;
			if (jnt[jnt[i].iParent].nChildren!=1 || jnt[jnt[j].iParent].nChildren!=1) {
				int tcode = m_parts[jnt[i].iStartPart].pPhysGeom->pGeom->GetType()*16 + m_parts[jnt[j].iStartPart].pPhysGeom->pGeom->GetType();
				if (tcode==GEOM_CYLINDER*17	|| tcode==GEOM_CAPSULE*17) {
					cylinder cyl[2];
					for(int iop=0,ij=i;iop<2;iop++,ij+=j-i)	{
						cyl[iop] = *(cylinder*)m_parts[jnt[ij].iStartPart].pPhysGeom->pGeom->GetData();
						cyl[iop].axis = m_pSrc->m_infos[jnt[ij].iStartPart].q * cyl[iop].axis;
					}
					score = fabs(mirror(cyl[0].axis,dir)*cyl[1].axis) * min(cyl[0].hh,cyl[1].hh)*min(cyl[0].r,cyl[1].r) / (max(cyl[0].hh,cyl[1].hh)*max(cyl[0].r,cyl[1].r));
				} else if ((tcode>>4)==(tcode&15)) {
					box bbox[2];
					for(int iop=0,ij=i;iop<2;iop++,ij+=j-i) {
						m_parts[jnt[ij].iStartPart].pPhysGeom->pGeom->GetBBox(bbox+iop);
						bbox[iop].Basis *= Matrix33(!m_pSrc->m_infos[jnt[ij].iStartPart].q);
					}
					for(int iax=0;iax<3;iax++) bbox[0].Basis.SetRow(iax,mirror(bbox[0].Basis.GetRow(iax),dir));
					Vec3 sz1 = (bbox[0].Basis*(bbox[1].size*bbox[1].Basis)).abs();
					for(int iop=0;iop<3;iop++) score *= min(bbox[0].size[iop],sz1[iop]) / max(bbox[0].size[iop],sz1[iop]);
				} else
					score = 0;
			}
			if (score>0.75f)
				break;
		}
		if (j==m_nJoints)
			j = i;
		for(int iop=0,ij=i,ji=j;iop<1+(i!=j);iop++,swap(ij,ji)) {
			// mqi = qj * mj
			// qj' = mqi'	* (mj.inv	= flipy * qmj.inv)
			jnt[ji].prev_qrot = !mirror_flipy(jnt[ij].quat,dir)*jnt[ji].quat; // store as qmj.inv
			jnt[ji].bHasExtContacts = ij;
		}
	}
	for(int i=0;i<m_nJoints;i++) {
		const ae_joint &jsrc = m_joints[m_infos[jnt[jnt[i].bHasExtContacts].iStartPart].iJoint];
		ae_joint &jdst = pdst->m_joints[pdst->m_infos[jnt[i].iStartPart].iJoint];
		jdst.body.q = mirror_flipy(jsrc.quat,dir)*jnt[i].prev_qrot * !jdst.body.qfb;
		for(int ivel=0;ivel<4;ivel++) (&jdst.body.P)[ivel] = mirror((&jsrc.body.P)[ivel],dir);
		pdst->SyncJointWithBody(i,1);	pdst->SyncBodyWithJoint(i,1);
	}
	pdst->UpdatePosition(0);
}


int CArticulatedEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1+(m_bProcessed>>PENT_QUEUED_BIT);

	Vec3 prevpos = m_pos, prevBBox[2] = { m_BBox[0],m_BBox[1] };
	quaternionf prevq = m_qrot;

	if (_params->type==pe_params_articulated_body::type_id) {
		if (((pe_params_articulated_body*)_params)->bGrounded==100) {
			m_bIgnoreCommands=0; return 1;
		}
	}
	if (m_bIgnoreCommands)
		return 1;

	if (CRigidEntity::SetParams(_params,1)) {
		if (_params->type==pe_params_pos::type_id) {
			WriteLock lock(m_lockUpdate);
			if ((prevq.v-m_qrot.v).len2()>0) {
				m_offsPivot = (m_qrot*!prevq)*m_offsPivot;
				m_posPivot = m_pos + m_offsPivot;
				for(int i=0;i<m_nJoints;i++) if (m_joints[i].iParent<0)	{
					if (m_joints[i].flags & joint_rotate_pivot)
						m_joints[i].pivot[0] = (m_qrot*!prevq)*m_joints[i].pivot[0];
				}
			}
			if (m_iSimClass > 2)
				m_bPartPosForced |= 2;
			else for(int i=0;i<m_nJoints;i++) {
				int j = m_joints[i].iStartPart;
				m_joints[i].quat = m_qrot*m_parts[j].q*!m_infos[j].q0;
				m_joints[i].body.pos = m_qrot*m_parts[j].pos-m_joints[i].quat*m_infos[j].pos0+m_pos;
				m_joints[i].body.q = m_joints[i].quat*!m_joints[i].body.qfb;
				SyncJointWithBody(i);
			}
			m_posPivot = m_pos + m_offsPivot;
		}
		if (_params->type==pe_params_part::type_id)	{
			pe_params_part *params = (pe_params_part*)_params;
			if (!is_unused(params->pos) && !(m_nJoints==0 || m_joints[0].flags & joint_rotate_pivot))
				m_bPartPosForced |= 1;
			if (!m_pHost && params->bRecalcBBox && m_body.Minv==0 && m_nParts) {
				CPhysicalEntity **pentlist;
				Vec3 gap(m_pWorld->m_vars.maxContactGapPlayer);
				int nEnts=m_pWorld->GetEntitiesAround(min(m_BBox[0],prevBBox[0])-gap,max(m_BBox[1],prevBBox[1])+gap,pentlist,m_collTypes&(ent_sleeping_rigid|ent_living|ent_independent)|ent_triggers,this);
				for(--nEnts;nEnts>=0;nEnts--) if (pentlist[nEnts]!=this)
					pentlist[nEnts]->Awake();
			}
			int i=params->ipart;
			if (is_unused(params->ipart))
				for(i=0; i<m_nParts && m_parts[i].id!=params->partid; i++);
			if (i<m_nParts) {
				int iJoint=m_infos[i].iJoint, i0=m_joints[iJoint].iStartPart;
				if (!is_unused(params->pos) && !is_unused(params->invTimeStep) && m_pHost && i==i0) { // Calculate the joint's v,w based on position+orientation changes
					prevpos = m_joints[iJoint].body.pos;
					prevq   = m_joints[iJoint].body.q;
					m_joints[iJoint].quat = m_parts[i].q*!m_infos[i].q0;
					m_joints[iJoint].body.q = m_pHost->m_qrot*m_joints[iJoint].quat*!m_joints[iJoint].body.qfb;
					m_joints[iJoint].body.pos = m_pHost->m_qrot*(m_parts[i].pos-m_joints[iJoint].quat*m_infos[i].pos0)+m_pos;
					m_joints[iJoint].prev_v=m_joints[iJoint].body.v = (m_joints[iJoint].body.pos-prevpos)*params->invTimeStep;
					Quat qdiff = !prevq*m_joints[iJoint].body.q;
					m_joints[iJoint].prev_w=m_joints[iJoint].body.w = qdiff.v*(qdiff.w*2*params->invTimeStep);
				}
				if (i0<i) {
					quaternionf qrot = m_bGrounded && m_bAwake ? m_joints[0].quat0 : m_qrot;
					m_infos[i].q0 = !m_joints[iJoint].quat*(qrot*m_parts[i].q);
					m_infos[i].pos0 = (m_pos+qrot*m_parts[i].pos-m_joints[iJoint].body.pos)*m_joints[iJoint].quat;
 					//m_infos[i].q0 = !m_parts[i0].q*m_parts[i].q;
					//Vec3 posJoint = m_parts[i0].pos-m_parts[i0].q*m_infos[i0].pos0;
					//m_infos[i].pos0 = !m_parts[i0].q*(m_parts[i].pos-posJoint);
				}
			}
			
		}
		return 1;
	}

	if (_params->type==pe_params_joint::type_id) {
		pe_params_joint *params = (pe_params_joint*)_params;
		m_bPartPosForced &= ~1;
		int i,j,op[2]={-1,-1},nChanges=0,bAnimated=m_bFeatherstone|iszero(m_body.M);
		for(i=0;i<m_nJoints;i++) {
			if (m_joints[i].idbody==params->op[0]) op[0]=i;
			if (m_joints[i].idbody==params->op[1]) op[1]=i;
		}

		if (op[1]>=0 && is_unused(params->op[0])) op[0] = m_joints[op[1]].iParent;
		if (op[0]<0 && params->op[0]>=0 || op[1]<0) 
			return 0;

		quaternionf qparent, qchild = m_joints[op[1]].body.q*m_joints[op[1]].body.qfb;
		Vec3 posparent, poschild = m_joints[op[1]].body.pos;
		if (op[0]>=0) {
			qparent = m_joints[op[0]].body.q*m_joints[op[0]].body.qfb;
			posparent = m_joints[op[0]].body.pos;
		} else {
			qparent.SetIdentity(); 
			posparent = m_posPivot;
		}

		if (!is_unused(params->flags)) {
			m_joints[op[1]].flags = params->flags | m_joints[op[1]].flags & params->flags & (angle0_limit_reached|angle0_gimbal_locked)*7;
			for(i=0;i<3;i++) if (params->flags & angle0_locked<<i)
				m_joints[op[1]].dq[i] = 0;
		}
		if (!is_unused(params->pivot)) {
			Vec3 pivot = params->pivot;
			if (params->flagsPivot & 8)
				pivot = m_parts[m_joints[op[0]].iStartPart].q*pivot + m_parts[m_joints[op[0]].iStartPart].pos;
			pivot = m_qrot*pivot + m_pos;
			if (params->flagsPivot & 1) {
				if (params->op[0]!=-1 || params->flagsPivot & 4)
					m_joints[op[1]].pivot[0] = (pivot-posparent)*qparent;
				else m_joints[op[1]].pivot[0].Set(0,0,0);
			}								
			if (params->flagsPivot & 2)	{
				if (m_bGrounded || params->op[0]!=-1 || params->flagsPivot & 4)
					m_joints[op[1]].pivot[1] = (pivot-poschild)*qchild;
				else m_joints[op[1]].pivot[1].Set(0,0,0);
			}
			if (params->flagsPivot & 4)
				m_joints[op[1]].flags |= joint_rotate_pivot;
		}
		
		if (!is_unused(params->nSelfCollidingParts)) {
			for(i=0,m_joints[op[1]].selfCollMask=0;i<m_nParts;i++) for(j=0;j<params->nSelfCollidingParts;j++) if (m_parts[i].id==params->pSelfCollidingParts[j])
				m_joints[op[1]].selfCollMask |= getmask(i);
		}

		float rdt = !is_unused(params->ranimationTimeStep) ? params->ranimationTimeStep : 
							 (!is_unused(params->animationTimeStep) ? 1.0f/params->animationTimeStep : 0);
		for(i=0;i<3;i++) {
			if (!is_unused(params->limits[0][i])) m_joints[op[1]].limits[0][i] = params->limits[0][i];
			if (!is_unused(params->limits[1][i])) m_joints[op[1]].limits[1][i] = params->limits[1][i];
			if (!is_unused(params->bounciness[i])) m_joints[op[1]].bounciness[i] = params->bounciness[i];
			if (!is_unused(params->ks[i])) m_joints[op[1]].ks[i] = params->ks[i];
			if (!is_unused(params->kd[i])) 
				m_joints[op[1]].kd[i] = ((m_joints[op[1]].flags & angle0_auto_kd<<i) ? 2.0f*sqrt_tpl(max(1E-10f,m_joints[op[1]].ks[i])) : 1.0f)*params->kd[i];
			if (!is_unused(params->qdashpot[i])) m_joints[op[1]].qdashpot[i] = params->qdashpot[i];
			if (!is_unused(params->kdashpot[i])) m_joints[op[1]].kdashpot[i] = params->kdashpot[i];
			if (!is_unused(params->q[i])) m_joints[op[1]].prev_q[i] = m_joints[op[1]].q[i] = params->q[i];
			if (!is_unused(params->qtarget[i])) m_joints[op[1]].q0[i] = params->qtarget[i];
			if (!is_unused(params->qext[i]) && m_joints[op[1]].iParent==op[0]) {
				//m_joints[op[1]].dqext[i] = m_joints[op[1]].iParent>=0 ? (params->qext[i]-m_joints[op[1]].qext[i])*rdt;
				float delta = params->qext[i]-m_joints[op[1]].qext[i];
				int sgnDelta = sgnnz(delta); delta = fabs_tpl(delta);
				delta -= isneg(fabs_tpl(delta-gf_PI2)-delta)*gf_PI2; // try to detect pi <-> -pi and pi/2 <-> -pi/2 euler flips
				delta -= isneg(fabs_tpl(delta-gf_PI)-delta)*gf_PI;
				m_joints[op[1]].dqext[i] = sgnDelta*(delta*rdt);
				m_joints[op[1]].qext[i] = params->qext[i]; nChanges++; 
				if (!(m_joints[op[1]].flags & angle0_locked<<i) && 
						isneg(m_joints[op[1]].limits[0][i]-m_joints[op[1]].qext[i]) + isneg(m_joints[op[1]].qext[i]-m_joints[op[1]].limits[1][i]) + 
						isneg(m_joints[op[1]].limits[1][i]-m_joints[op[1]].limits[0][i]) < 2) 
				{	// qext violates limits; adjust the limits
					float diff[2];
					diff[0] = m_joints[op[1]].limits[0][i]-m_joints[op[1]].qext[i];
					diff[1] = m_joints[op[1]].limits[1][i]-m_joints[op[1]].qext[i];
					diff[0] -= (int)(diff[0]/(2*g_PI))*2*g_PI;
					diff[1] -= (int)(diff[1]/(2*g_PI))*2*g_PI;
					m_joints[op[1]].limits[isneg(fabs_tpl(diff[1])-fabs_tpl(diff[0]))][i] = m_joints[op[1]].qext[i];
				}
			}
		}
		if (!is_unused(params->pMtx0) && params->pMtx0) 
			params->q0 = quaternionf(*params->pMtx0/params->pMtx0->GetRow(0).len());

		if (!is_unused(params->q0)) { 
			m_joints[op[1]].quat0 = params->q0; m_joints[op[1]].bQuat0Changed = 0; nChanges++; 
		}
		ENTITY_VALIDATE("CArticulatedEntity:SetParams(params_joint)",params);

		if (m_joints[op[1]].iParent!=op[0]) {
			m_joints[op[1]].q = Ang3::GetAnglesXYZ(!(qparent*m_joints[op[1]].quat0)*qchild);
			if (is_unused(params->qext[0]))
				m_joints[op[1]].qext = m_joints[op[1]].q;
			m_joints[op[1]].prev_q = (m_joints[op[1]].q -= m_joints[op[1]].qext);

			if (op[1]!=op[0]+1) {
				int nChildren;
				for(nChildren=m_joints[op[1]].nChildren+1,i=1; i<nChildren; nChildren+=m_joints[op[1]+i++].nChildren);
				ae_joint jbuf,*pjbuf=nChildren>1 ? new ae_joint[nChildren] : &jbuf;
				memcpy(pjbuf, m_joints+op[1], nChildren*sizeof(ae_joint));
				if (op[1]<op[0]) {
					for(i=1; i<nChildren; i++) pjbuf[i].iParent += op[0]-op[1]+1-nChildren;
					for(i=op[1]+nChildren; i<m_nJoints; i++) if (m_joints[i].iParent>op[1] && m_joints[i].iParent<=op[0]) 
						m_joints[i].iParent -= nChildren;
					memmove(m_joints+op[1], m_joints+op[1]+nChildren, (op[0]-op[1]-nChildren+1)*sizeof(ae_joint)); 
					op[0] -= nChildren;
				} else {
					for(i=1;i<nChildren;i++) pjbuf[i].iParent -= op[1]-op[0]-1;
					for(i=op[0]+1; i<op[1]; i++) if (m_joints[i].iParent>op[0]) m_joints[i].iParent += nChildren;
					memmove(m_joints+op[0]+1+nChildren, m_joints+op[0]+1, (op[1]-op[0]-1)*sizeof(ae_joint));
				}
				memcpy(m_joints+op[0]+1, pjbuf, nChildren*sizeof(ae_joint));
				for(i=0;i<nChildren;i++) pjbuf[i].fsbuf=0;
				if (pjbuf!=&jbuf) delete[] pjbuf;
				JointListUpdated();
			}
			op[1] = op[0]+1;
			m_joints[op[1]].iParent = op[0];
			m_joints[op[1]].q0 = m_joints[op[1]].qext;
			if (op[0]>=0) {
				m_joints[op[0]].nChildren++;
				m_joints[op[0]+1].iLevel = m_joints[op[0]].iLevel+1;
			} else m_joints[op[0]+1].iLevel = 0;
			for(i=op[0]; i>=0; i=m_joints[i].iParent) 
				m_joints[i].nChildrenTree += m_joints[op[0]+1].nChildrenTree+1;
			if (params->op[0]==-1 && !m_bGrounded && !(params->flagsPivot & 4)) {
				m_posPivot = m_joints[op[1]].body.pos;
				m_offsPivot = m_posPivot-m_pos;
			}
			nChanges++;
		}	else {
			for(i=0;i<3;i++) if (!is_unused(params->q[i])) {
				m_joints[op[1]].q[i] = params->q[i]; nChanges++;
			}
			if (nChanges) {
				m_joints[op[1]].q0 = m_joints[op[1]].q;
				if (!params->bNoUpdate)	{
					for(i=op[1];i<=op[1]+m_joints[op[1]].nChildrenTree;i++) SyncBodyWithJoint(i);
					UpdatePosition(0);
				}
			}
		}
		m_bUpdateBodies = bAnimated;
		for(m_nRoots=i=0;i<m_nJoints;i++)
			m_nRoots += max(0,-m_joints[i].iParent);
		return 1;
	}

	if (_params->type==pe_params_articulated_body::type_id) {
		pe_params_articulated_body *params = (pe_params_articulated_body*)_params;
		if (!is_unused(params->pivot)) {
			Vec3 offs = m_posPivot;
			m_posPivot = m_pos+(m_offsPivot=params->pivot); offs = m_posPivot-offs;
			for(int i=0;i<m_nJoints;i++) m_joints[i].body.pos += offs;
		}
		if (!is_unused(params->bGrounded)) {
			m_bGrounded = params->bGrounded;
			if (m_nJoints>0 && m_joints[0].iParent==-1) {
				quaternionf qroot = m_joints[0].body.q*m_joints[0].body.qfb;
				if (m_bGrounded) {
					if (!is_unused(params->pivot)) {
						m_joints[0].pivot[1] = params->pivot-m_joints[0].body.pos;
						m_offsPivot -= m_joints[0].pivot[1]; m_posPivot += m_joints[0].pivot[1];
						m_joints[0].pivot[1] = m_joints[0].pivot[1]*qroot;
					}
				} else {
					m_joints[0].pivot[1] = qroot*m_joints[0].pivot[1];
					m_offsPivot -= m_joints[0].pivot[1]; m_posPivot -= m_joints[0].pivot[1];
					m_joints[0].pivot[1].Set(0,0,0);
					m_joints[0].flags &= ~all_angles_locked;
					m_joints[0].limits[0].Set(-1E15,-1E15,-1E15);
					m_joints[0].limits[1].Set(1E15,1E15,1E15);
					m_joints[0].ks.Set(0,0,0); m_joints[0].kd.Set(0,0,0);
					m_acc.Set(0,0,0); m_body.w.Set(0,0,0); m_wacc.Set(0,0,0);
				}
			}
			m_bIaReady = 0; m_simTime = 0; m_bAwake = 1;
		}
		if (!is_unused(params->v) && (params->v-m_body.v).len2()>0) { m_body.v = params->v; m_simTime = 0; m_bAwake = 1; }
		if (!is_unused(params->a) && (params->a-m_acc).len2()>0) { m_acc = params->a; m_simTime = 0; m_bAwake = 1; }
		if (!is_unused(params->w) && (params->w-m_body.w).len2()>0) { 
			if (m_bGrounded)
				m_body.w = params->w; 
			else if (m_bIaReady) {
				Matrix33 basis_inv = GetMtxFromBasis(m_joints[0].rotaxes);
				basis_inv.Invert(); m_joints[0].dq = basis_inv*m_body.w;
			}
			m_simTime = 0; m_bAwake = 1;
		}
		if (!is_unused(params->wa) && (params->wa-m_wacc).len2()>0) { m_wacc = params->wa; m_simTime = 0; }
		if (!is_unused(params->scaleBounceResponse)) m_scaleBounceResponse = params->scaleBounceResponse;
		if (params->bApply_dqext) {
			for(int i=0;i<m_nJoints;i++)
				m_joints[i].dq += m_joints[i].dqext;
		}
		if (!is_unused(params->bAwake))
			if (!(m_bAwake = params->bAwake))
				m_simTime = 10.0f;
		int bRecalcPos = 0;
		if (!is_unused(params->pHost)) { 
			if (m_pHost) {
				m_pHost->RemoveCollider(this); --m_pHost->m_nSyncColliders;
				m_pHost->Release(); m_pHost = 0;
			}
			if (params->pHost) {
				(m_pHost = ((CPhysicalPlaceholder*)params->pHost)->GetEntity())->AddRef(); 
				m_pHost->AddCollider(this); ++m_pHost->m_nSyncColliders;
				bRecalcPos = 1; 
			}
		}
		if (!is_unused(params->bInheritVel)) { m_bInheritVel = params->bInheritVel; bRecalcPos = 1; }
		if (!is_unused(params->posHostPivot)) { m_posHostPivot = params->posHostPivot; bRecalcPos = 1; }
		if (!is_unused(params->qHostPivot)) { m_qHostPivot = params->qHostPivot; bRecalcPos = 1; }
		if (bRecalcPos && (!m_bAwake || m_body.M<=0 || m_flags & aef_recorded_physics))
			SyncWithHost(params->bRecalcJoints,0);
		if (!is_unused(params->bCheckCollisions)) m_bCheckCollisions = params->bCheckCollisions;
		if (!is_unused(params->bCollisionResp)) m_bFeatherstone = !params->bCollisionResp;
		if (!is_unused(params->bFeatherstone)) if (m_bFeatherstone = params->bFeatherstone)
			m_iSimType = m_iSimTypeLyingMode = 0;

		if (!is_unused(params->nCollLyingMode)) m_nCollLyingMode = params->nCollLyingMode;
		if (!is_unused(params->gravityLyingMode)) m_gravityLyingMode = params->gravityLyingMode;
		if (!is_unused(params->dampingLyingMode)) m_dampingLyingMode = params->dampingLyingMode;
		if (!is_unused(params->minEnergyLyingMode)) m_EminLyingMode = params->minEnergyLyingMode;
		if (!is_unused(params->iSimType)) m_iSimType = params->iSimType;
		if (!is_unused(params->iSimTypeLyingMode)) m_iSimTypeLyingMode = params->iSimTypeLyingMode;

		if (!is_unused(params->nJointsAlloc) && params->nJointsAlloc>m_nJointsAlloc) {
			WriteLock lock(m_lockUpdate);
			ReallocateList(m_joints, m_nJoints, m_nJointsAlloc=params->nJointsAlloc);
			if (params->nJointsAlloc>m_nPartsAlloc) {
				// Reallocate parts
				geom* pNewParts = m_pHeap
					? (geom*)m_pHeap->Malloc(sizeof(geom) * params->nJointsAlloc, "Parts")
					: m_pWorld->AllocEntityParts(params->nJointsAlloc);
				memcpy(pNewParts, m_parts, sizeof(geom)*min(m_nParts, params->nJointsAlloc));
				if (m_pHeap)
					m_pHeap->Free(m_parts);
				else
					m_pWorld->FreeEntityParts(m_parts, m_nPartsAlloc);
				m_nPartsAlloc=params->nJointsAlloc;
				m_parts = pNewParts;
				if (m_nPartsAlloc!=1) { MEMSTAT_USAGE(m_parts, sizeof(geom) * m_nParts); }
				// Reallocate infos
				ReallocateList(m_infos, m_nParts, m_nPartsAlloc, true);
				for(int i=m_nParts;i<m_nPartsAlloc;i++)
					m_infos[i].qHist[0].SetIdentity(), m_infos[i].qHist[1].SetIdentity();
				for(int i=0;i<m_nParts;i++)
					m_parts[i].pNewCoords = (coord_block_BBox*)&m_infos[i].pos;
			}
		}

		return 1;
	}

	return 0;
}


int CArticulatedEntity::GetParams(pe_params *_params) const
{
	int res = CRigidEntity::GetParams(_params);
	if (res)
		return res;

	if (_params->type==pe_params_joint::type_id) {
		pe_params_joint *params = (pe_params_joint*)_params;
		int i; 
		if (params->op[1]<0)
			params->op[1] = m_joints[i=0].idbody;
		else {
			for(i=0;i<m_nJoints && m_joints[i].idbody!=params->op[1];i++);
			if (i>=m_nJoints)
				return 0;
		}
		params->flags = m_joints[i].flags;
		params->pivot = (m_joints[i].body.pos + (m_joints[i].body.q*m_joints[i].body.qfb)*m_joints[i].pivot[1] - m_pos)*m_qrot;
		params->q0 = m_joints[i].quat0;
		params->qtarget = m_joints[i].q0;
		if (params->pMtx0) *params->pMtx0 = Matrix33(m_joints[i].quat0);
		params->limits[0] = m_joints[i].limits[0];
		params->limits[1] = m_joints[i].limits[1];
		params->bounciness = m_joints[i].bounciness;
		params->ks = m_joints[i].ks;
		params->kd = m_joints[i].kd;
		params->q = m_joints[i].q;
		params->qext = m_joints[i].qext;
		params->op[0] = m_joints[i].iParent>=0 ? m_joints[m_joints[i].iParent].idbody : -1;
		return 1;
	}

	if (_params->type==pe_params_articulated_body::type_id) {
		pe_params_articulated_body *params = (pe_params_articulated_body*)_params;
		params->bGrounded = m_bGrounded;
		params->pivot = m_offsPivot;
		params->a = m_acc;
		params->wa = m_wacc;
		params->w = m_body.w;
		params->v = m_body.v;
		params->scaleBounceResponse = m_scaleBounceResponse;
		params->pHost = m_pHost;
		params->bInheritVel = m_bInheritVel;
		params->posHostPivot = m_posHostPivot;
		params->qHostPivot = m_qHostPivot;
		params->bCheckCollisions = m_bCheckCollisions;
		params->bCollisionResp = !m_bFeatherstone;
		params->bFeatherstone = m_bFeatherstone;
		params->nCollLyingMode = m_nCollLyingMode;
		params->gravityLyingMode = m_gravityLyingMode;
		params->dampingLyingMode = m_dampingLyingMode;
		params->minEnergyLyingMode = m_EminLyingMode;
		params->iSimType = m_iSimType;
		params->iSimTypeLyingMode = m_iSimTypeLyingMode;
		params->nRoots = m_nRoots;
		return 1;
	}

	return 0;
}


int CArticulatedEntity::GetStatus(pe_status* _status) const
{
	if (_status->type==pe_status_joint::type_id) {
		pe_status_joint *status = (pe_status_joint*)_status;
		int i; 
		if (!is_unused(status->idChildBody)) {
			for(i=0;i<m_nJoints && m_joints[i].idbody!=status->idChildBody;i++);
			if (i>=m_nJoints) return 0;
			status->partid = m_parts[m_joints[i].iStartPart].id;
		}	else if (!is_unused(status->partid)) {
			for(i=0;i<m_nParts && m_parts[i].id!=status->partid;i++);
			if (i>=m_nParts) return 0;
			status->idChildBody = m_joints[i = m_infos[i].iJoint].idbody;
		} else
			return 0;
		ReadLock lock(m_lockJoints);
		status->flags = m_joints[i].flags;
		status->q = m_joints[i].prev_q;
		status->qext = m_joints[i].qext;
		status->dq = Ang3(m_joints[i].prev_dq);
		status->quat0 = m_joints[i].quat0;
		return 1;
	}

	if (_status->type==pe_status_dynamics::type_id) {
		int i; pe_status_dynamics *status = (pe_status_dynamics*)_status;
		ReadLock lock(m_lockJoints);
		if (!is_unused(status->partid))
			for(i=0; i<m_nParts && m_parts[i].id!=status->partid; i++);
		else i = status->ipart;

		if ((unsigned int)i>=(unsigned int)m_nParts) {
			status->mass = 0;
			if (m_body.M>0) {
				for(i=0,status->centerOfMass.zero(); i<m_nJoints; i++) {
					status->centerOfMass += m_joints[i].body.pos*m_joints[i].body.M;
					status->mass += m_joints[i].body.M;
				}
				if (status->mass) status->centerOfMass *= __fres(status->mass);
			}	else
				status->centerOfMass = m_pos; 
			status->v = m_body.v;
			status->w = m_body.w;
			status->a = m_gravity; status->wa.zero();
			status->submergedFraction = m_submergedFraction;
			status->nContacts = m_nBodyContacts;
			for(i=0,status->energy=0; i<m_nJoints; i++)
				status->energy += m_joints[i].body.M*m_joints[i].body.v.len2() + m_joints[i].body.L*m_joints[i].body.w;
			status->energy *= 0.5f;
			return i;
		}

		RigidBody *pbody = &m_joints[i=m_infos[i].iJoint].body;
		status->v = m_joints[i].prev_v;
		status->w = m_joints[i].prev_w;
		status->a = m_gravity;
		status->wa = pbody->Iinv*(-(status->w^m_joints[i].prev_qrot*(pbody->Ibody*(m_joints[i].prev_qrot*status->w))));
		status->centerOfMass = pbody->pos;
		status->submergedFraction = m_submergedFraction;
		status->mass = pbody->M;
		status->energy = (pbody->M*pbody->v.len2()+pbody->L*pbody->w)*0.5f;
		status->nContacts = m_joints[i].bHasExtContacts;
		return 1;
	}

	int res = CRigidEntity::GetStatus(_status);
	if (res) {
		if (_status->type==pe_status_awake::type_id && m_body.M==0)
			return 0;
		if (_status->type==pe_status_caps::type_id) {
			pe_status_caps *status = (pe_status_caps*)_status;
			status->bCanAlterOrientation = 1;
		}
	}
	return res;
}


int CArticulatedEntity::Action(pe_action *_action, int bThreadSafe)
{
	if (_action->type==pe_action_add_constraint::type_id)
		return CRigidEntity::Action(_action,bThreadSafe);

	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	if (_action->type==pe_action_batch_parts_update::type_id) {
		pe_action_batch_parts_update *action = (pe_action_batch_parts_update*)_action;
		int i,j,nParts;
		if (action->pValidator && !action->pValidator->Lock())
			return 0;
		ReapplyParts:
		for(i=j=0;i<action->numParts;i++) {
			for(;j<m_nParts && m_parts[j].id!=action->pIds[i];j++);
			if (j<m_nParts) {
				if (action->posParts) m_infos[j].pos = action->qOffs*action->posParts[i]+action->posOffs;
				if (action->qParts) m_infos[j].q = action->qOffs*action->qParts[i];
				int idx = m_infos[j].iJoint;
				m_joints[idx].quat = m_qrot*m_infos[j].q*!m_infos[j].q0;
				m_joints[idx].body.q = m_joints[idx].quat*!m_joints[idx].body.qfb;
				m_joints[idx].body.pos = m_qrot*m_infos[j].pos-m_joints[idx].quat*m_infos[j].pos0+m_posNew;
			} else {
				for(i=nParts=0;i<m_nParts;i++) for(j=nParts;j<action->numParts;j++) if (action->pIds[j]==m_parts[i].id) {
					action->pIds[j]=action->pIds[nParts]; action->pIds[nParts++]=m_parts[i].id;
					break;
				}
				if (action->pnumParts)
					*action->pnumParts = nParts;
				action->numParts = nParts;
				goto ReapplyParts;
			}
		}
		if (action->pValidator)
			action->pValidator->Unlock();
		ComputeBBox(m_BBoxNew);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));

		if (!(m_nJoints==0 || m_joints[0].flags & joint_rotate_pivot))
			m_bPartPosForced |= 1;
		if (!m_pHost && m_body.Minv==0 && m_nParts && m_iSimClass<4) {
			CPhysicalEntity **pentlist;
			int nEnts=m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1],pentlist,m_collTypes&(ent_sleeping_rigid|ent_living|ent_independent)|ent_triggers,this);
			for(--nEnts;nEnts>=0;nEnts--) if (pentlist[nEnts]!=this)
				pentlist[nEnts]->Awake();
		}
		return 1;
	}

	if (_action->type==pe_action_impulse::type_id) {
		pe_action_impulse *action = (pe_action_impulse*)_action;
		int i,j;
		if (!m_bFeatherstone && action->impulse.len2()<sqr(m_body.M*0.04f) && action->iSource==0 && !(m_flags & aef_recorded_physics) 
				|| action->iSource==4 || m_nJoints<=0)
			return 1;
		if (is_unused(action->ipart))
			for(i=0; i<m_nParts && m_parts[i].id!=action->partid; i++);
		else i = action->ipart;
		ENTITY_VALIDATE("CArticulatedEntity:Action(action_impulse)",action);

		box bbox; Vec3 posloc;
		if ((unsigned int)i>=(unsigned int)m_nParts) {
			if (is_unused(action->point)) 
				i = m_joints[0].iStartPart;
			else {
				for(i=0;i<m_nParts;i++) {
					m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
					posloc = (m_qrot*m_parts[i].q)*bbox.center + m_qrot*m_parts[i].pos + m_pos;
					posloc = bbox.Basis*((action->point-posloc)*(m_qrot*m_parts[i].q));
					for(j=0;j<3 && posloc[j]<bbox.size[j];j++);
					if (j==3) break;
				}
				if ((unsigned int)i>=(unsigned int)m_nParts) return 0;
			}
		}

		if ((unsigned int)i>=(unsigned int)m_nParts || (unsigned int)(j=m_infos[i].iJoint)>=(unsigned int)m_nJoints || (!action->iSource && m_joints[j].flags & joint_ignore_impulses))
			return 0;
		if (action->iSource==0) {
			if (action->impulse.len2()<sqr(m_joints[j].body.M*0.04f))
				return 1;
			/*if (m_flags & (pef_monitor_impulses | pef_log_impulses)) {
				EventPhysImpulse event;
				event.pEntity=this; event.pForeignData=m_pForeignData; event.iForeignData=m_iForeignData;
				event.ai = *action;
				if (!m_pWorld->OnEvent(m_flags,&event))
					return 1;
			}*/
			m_timeIdle = 0;
		}

		Vec3 P(ZERO),L(ZERO);
		if (!is_unused(action->impulse))
			P += action->impulse;
		if (!is_unused(action->angImpulse)) 
			L += action->angImpulse;
		if (!is_unused(action->point)) {
			// make sure that the point is not too far from the part's bounding box
			m_parts[i].pPhysGeomProxy->pGeom->GetBBox(&bbox);
			posloc = (m_qrot*m_parts[i].q)*bbox.center + m_qrot*m_parts[i].pos + m_pos;
			posloc = bbox.Basis*((action->point-posloc)*(m_qrot*m_parts[i].q));
			if (fabs_tpl(posloc.x)<bbox.size.x*3 && fabs_tpl(posloc.y)<bbox.size.y*3 && fabs_tpl(posloc.z)<bbox.size.z*3) {
				// recompute body's com, since sometimes only parts are in sync with animation, not joint bodies
				Vec3 bodypos = m_parts[i].pos+m_pos-(m_parts[i].q*!m_infos[i].q0)*m_infos[i].pos0;
				L += action->point-bodypos ^ action->impulse;
			}
		}
		if (P.len2()>sqr(m_pWorld->m_vars.maxVel*m_body.M))
			P.normalize() *= m_pWorld->m_vars.maxVel*m_body.M;
		Matrix33 R = (Matrix33)(m_parts[i].q*!m_infos[i].q0*!m_joints[j].body.qfb);
		Matrix33 Iinv = R*m_joints[j].body.Ibody_inv*R.T();
		float w = (Iinv*L).len2();
		if (w > sqr(1000.0f))
			L *= 1000.0f/sqrt_tpl(w);
		if (!(m_flags & aef_recorded_physics))
			m_joints[j].Pimpact += P, m_joints[j].Limpact += L;
		else {
			m_joints[j].prev_v=m_joints[j].body.v = (m_joints[j].body.P+=P)*m_joints[j].body.Minv;
			m_joints[j].prev_w=m_joints[j].body.w = Iinv*(m_joints[j].body.L+=L);
		}

		if (action->iSource!=1) {
			m_bAwake = 1; m_simTime = 0;
			if (m_iSimClass==1) {
				m_iSimClass = 2;	m_pWorld->RepositionEntity(this, 2);
			}
		}

		CPhysicalEntity::Action(_action,1);

		return 1;
	}

	if (_action->type==pe_action_reset::type_id) {
		/*{ WriteLock lock(m_lockJoints);
			for(int idx=0;idx<m_nJoints;idx++) {
				m_joints[idx].q=Ang3(ZERO);
				if (m_joints[idx].bQuat0Changed)
					m_joints[idx].quat0.SetIdentity();
				m_joints[idx].dq.Set(0,0,0);
				m_joints[idx].body.v.Set(0,0,0); m_joints[idx].body.w.Set(0,0,0);
				m_joints[idx].body.P.Set(0,0,0); m_joints[idx].body.L.Set(0,0,0);
				m_joints[idx].prev_q.Set(0,0,0); m_joints[idx].prev_dq.zero();
				m_joints[idx].prev_v.zero(); m_joints[idx].prev_w.zero();
				m_joints[idx].Fcollision.zero(); m_joints[idx].Tcollision.zero();
				SyncBodyWithJoint(idx);
			}
		}*/
		if (m_iGThunk0) {
			ComputeBBox(m_BBoxNew);
			UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
		}
		m_body.v.Set(0,0,0); m_body.w.Set(0,0,0);
		m_body.P.Set(0,0,0); m_body.L.Set(0,0,0);
		return CRigidEntity::Action(_action,1);
	}

	if (_action->type==pe_action_set_velocity::type_id) {
		pe_action_set_velocity *action = (pe_action_set_velocity*)_action;
		if (m_nJoints<=0)
			return 0;

		if (!is_unused(action->ipart) || !is_unused(action->partid)) {
			int i;
			if (!is_unused(action->ipart))
				i = action->ipart;
			else for(i=0;i<m_nParts && m_parts[i].id!=action->partid;i++);
			if ((unsigned int)i>=(unsigned int)m_nParts)
				return 0;
			i = m_infos[i].iJoint;
			if (!is_unused(action->v)) {
				m_joints[i].body.P = (m_joints[i].body.v=action->v)*m_joints[i].body.M;
				if (i==0)
					m_body.v = m_joints[i].body.v;
				m_saveVel = true;
			}
			if (!is_unused(action->w))
				m_joints[i].body.L = m_joints[i].body.q*(m_joints[i].body.Ibody*(!m_joints[i].body.q*(m_joints[i].body.w=action->w)));
			if (m_joints[i].body.v.len2() > sqr(m_pWorld->m_vars.maxVel)) {
				m_joints[i].body.v.normalize() *= m_pWorld->m_vars.maxVel;
				m_joints[i].body.P = m_joints[i].body.v*m_body.M;
			}
			if (m_joints[i].body.w.len2() > sqr(m_maxw)) {
				m_joints[i].body.w.normalize() *= m_maxw;
				m_joints[i].body.L = m_joints[i].body.q*(m_joints[i].body.Ibody*(!m_joints[i].body.q*m_joints[i].body.w));
			}
			if ((m_joints[i].bAwake = m_joints[i].body.v.len2()+m_joints[i].body.w.len2()>0) && !m_bAwake) {
				if (m_body.Minv==0)
					m_body.v.x = 1E-10f;
				Awake();
			}
		}	else if (!is_unused(action->v)) {
			Vec3 v = action->v, w = !is_unused(action->w) ? action->w : Vec3(0);
			for(int i=0;i<m_nJoints;i++) {
				m_joints[i].body.P = (m_joints[i].body.v = v+(w^m_joints[i].body.pos-m_joints[0].body.pos))*m_joints[i].body.M;
				m_joints[i].body.L = m_joints[i].body.q*(m_joints[i].body.Ibody*(!m_joints[i].body.q*(m_joints[i].body.w = w)));
				m_joints[i].dq.zero();
			}
			m_body.P = (m_body.v=v)*m_body.M;
		}
		return 1;
	}

	if (_action->type==pe_action_resolve_constraints::type_id) {
		pe_action_resolve_constraints *action = (pe_action_resolve_constraints*)_action;
		// solve constraints immediately using kinetic energy minimizing IK
		// arguments: angles (velocities) at each joint (including limit-reached) + global v
		// constraints: vreq at each constraint	and contact
		static int szBuf = 0;
		static qpOASES::real_t *qbuf = nullptr;
		int i,bBounced,iCaller=get_iCaller(), bFeatherstone=m_bFeatherstone, flags=m_flags;
		unsigned int awake=m_bAwake;
		iCaller &= iCaller-MAX_PHYS_THREADS >> 31;
		float dt=0, invHardness=1/action->stepHardness, e=0.005f;
		m_bFloating = 1; // disable individual joint sleeping
		m_bFeatherstone = 1; // don't register joint links as constraints
		m_iSimTypeCur = 0;
		m_flags &= ~(pef_monitor_collisions | pef_log_collisions);
		int narg = (1-m_bGrounded)*3, nconstr, ncoll=0;
		for(i=0; i<m_nJoints; i++) {
			int f = (int)m_joints[i].flags; 
			narg += iszero(f&angle0_locked) + iszero(f&angle0_locked*2) + iszero(f&angle0_locked*4);
			m_joints[i].dq.zero();
			m_joints[i].Pext = m_joints[i].body.v;
		}
		if (!m_bGrounded) {
			m_joints[0].flags &= ~(angle0_locked*7);
			m_joints[0].limits[0] = -(m_joints[0].limits[1]=Vec3(1e10f));
		}
		m_joints[0].body.v.zero(); m_joints[0].body.w.zero();	m_body.v.zero();
		masktype constrMask = MaskIgnoredColliders(iCaller);
		for(i=0,m_constrInfoFlags=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i))
			m_constrInfoFlags |= m_pConstraintInfos[i].flags;

		for (int iter=0; ; iter++) {
			ComputeBBox(m_BBoxNew);
			m_nCollEnts = m_pWorld->GetEntitiesAround(m_BBoxNew[0],m_BBoxNew[1], m_pCollEntList, 
				m_collTypes&(ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid) | ent_sort_by_mass|ent_triggers, this, 0,iCaller);
			for(int j=i=0;j<m_nCollEnts;j++) if (!IgnoreCollision(m_pCollEntList[j]->m_collisionClass,m_collisionClass)) 
				m_pCollEntList[i++] = m_pCollEntList[j];
			m_nCollEnts = i;

			float bounceVel = m_pWorld->m_vars.minBounceSpeed; m_pWorld->m_vars.minBounceSpeed = 1e10f;
		retry_step:
			for(i=0; i<m_nJoints; i=StepJoint(i,dt,bBounced,0,iCaller)); 
			m_pWorld->m_vars.minBounceSpeed = bounceVel;
			if (iter>action->maxIters)
				break;
			VerifyExistingContacts(0);
			InitContactSolver(0);
			RegisterContacts(0,3);
			int ncont;
			entity_contact **pcont = GetContacts(ncont, iCaller);
			for(i=0;i<ncont;i++) if ((pcont[i]->flags & (contact_constraint_3dof|contact_angular))==(contact_constraint_3dof|contact_angular) && pcont[i]->pent[1]->m_id<0)
				MARK_UNUSED m_joints[m_infos[pcont[i]->ipart[0]].iJoint].ddq;	// filter away collision contacts on joints that are firmly constrained to the world
			for(int j=i=0;j<ncont;j++) 
				if (!(is_unused(m_joints[m_infos[pcont[j]->ipart[0]].iJoint].ddq) && !(pcont[j]->flags & (contact_constraint|contact_angular))) && 
						!(pcont[j]->bConstraint && m_pConstraintInfos[pcont[j]->iConstraint-1].flags & constraint_instant))
					pcont[i++] = pcont[j];
			ncont = i;
			float dtback = 0;
			for(i=nconstr=ncoll=0;i<ncont;i++) if (!(pcont[i]->flags & (contact_angular|contact_constraint))) {
				nconstr += pcont[i]->bProcessed=1; ncoll++;
				if (action->stopOnContact) {
					float vn = pcont[i]->n*(pcont[i]->pbody[0]->v+(pcont[i]->pbody[0]->w ^ pcont[i]->pt[0]-pcont[i]->pbody[0]->pos));
					dtback = max(dtback,min(dt,pcont[i]->penetration/-vn));
				}
			} else switch (pcont[i]->flags & contact_constraint) {
				case contact_constraint_3dof: nconstr += pcont[i]->bProcessed=3; break;
				case contact_constraint_2dof: nconstr += pcont[i]->bProcessed=1; break;
				case contact_constraint_1dof: nconstr += pcont[i]->bProcessed=2; break;
			}
			if (dtback>0) {
				if (!m_bGrounded)
					m_joints[0].body.pos = (m_posPivot -= m_body.v*dt);
				for(i=0;i<m_nJoints;i++) m_joints[i].q=m_joints[i].prev_q;
				dt -= dtback;
				iter = action->maxIters+1;
				goto retry_step;
			}
			if (iter>=action->maxIters || min(nconstr, narg)==0)
				break;
			float maxDrift=0;
			for(int j=(i=-1)+1;j<ncont;j++) if (!(pcont[j]->flags & contact_angular) && pcont[j]->vreq.len2()>maxDrift)
				maxDrift = pcont[i=j]->vreq.len2();
			if (i<0 || !pcont[i]->bConstraint)
				break;
			float rdt=m_pConstraintInfos[pcont[i]->iConstraint-1].hardness; dt=1/rdt;
			if (maxDrift*sqr(dt) > sqr(action->lastDist)) {
				dt *= action->stepHardness;	rdt *= invHardness;	// time step to resolve the maximum constraint error * stepHardness
			}	else
				iter = action->maxIters-1; // when close enough, assume stepHardness=1, only make one step

			if (szBuf < (i=sqr(narg)+narg*nconstr+narg*3+nconstr*2))
				ReallocateList(qbuf, 0,szBuf=i);

			// H: energy minimization goal, argT * H * arg -> min
			// A: arg->constraints matrix; constraints = A * arg
			// lbA, ubA: lower and upper bounds for constraints
			// lb, ub: lower and upper bounds for the arguments
			qpOASES::real_t *H=qbuf, *A=H+sqr(narg), *arg=A+nconstr*narg, *lb=arg+narg, *ub=lb+narg, *lbA=ub+narg, *ubA=lbA+nconstr;
			memset(H,0,sqr(narg)*sizeof(H[0])); memset(A,0,narg*nconstr*sizeof(A[0])); memset(arg,0,narg*sizeof(arg[0]));

			for(int ic=i=0;ic<ncont;ic++) if (!(pcont[ic]->flags & (contact_angular|contact_constraint)))	{
				lbA[i] = pcont[ic]->vreq*pcont[ic]->n; ubA[i] = 100; i++; // collision contact; enforce non-penetration along n
			}	else switch (pcont[ic]->flags & contact_constraint) {
				case contact_constraint_3dof: {	// full constaint; limit all 3 directions
					Vec3qp vreq=pcont[ic]->vreq, vleeway=pcont[ic]->vreq.abs()*0.05f+Vec3(e);
					*(Vec3qp*)(lbA+i)=vreq-vleeway; *(Vec3qp*)(ubA+i)=vreq+vleeway; i+=3; 
				} break; 
				case contact_constraint_2dof: lbA[i]=(ubA[i]=pcont[ic]->vreq*pcont[ic]->n+e)-e*2; break; // 2 dof constraint; lock rotation around n
				case contact_constraint_1dof:	{	// 1 dof constraint; choose 2 axes orthogonal to n and limit rotation around them
					Vec3 ax=pcont[ic]->n.GetOrthogonal().GetNormalized();
					for(int iax=0; iax<2; iax++,i++,ax=pcont[ic]->n^ax)
						lbA[i]=(ubA[i] = pcont[ic]->vreq*ax+e)-e*2;
				} break;
			}
			for(i=0;i<(1-m_bGrounded)*3;i++) { // if not grounded, the first 3 arguments are the pivot's linear velocity 
				for(int ic=0,ici=0; ic<ncont; ici+=pcont[ic++]->bProcessed)	{
					if (!(pcont[ic]->flags & (contact_angular|contact_constraint)))
						A[ici*narg+i] = pcont[ic]->n[i];
					else if (!(pcont[ic]->flags & contact_angular)) switch (pcont[ic]->flags & contact_constraint) {
						case contact_constraint_3dof:	A[(ici+i)*narg+i]=1; break;
						case contact_constraint_2dof:	A[ici*narg+i] = pcont[ic]->n[i]; break;
						case contact_constraint_1dof: {
							Vec3 ax=pcont[ic]->n.GetOrthogonal().GetNormalized();
							for(int iax=0; iax<2; iax++,ax=pcont[ic]->n^ax)
								A[(ici+iax)*narg+i] = ax[i];
						} break;
					}
				}
				lb[i] = -(ub[i] = 100);
				H[(narg+1)*i] = m_body.M;
			}
			int ndriftable = i;
				
			for(int j=0;j<m_nJoints;j++) {
				Vec3 pivot = m_joints[j].iParent<0 ? m_posPivot : m_joints[m_joints[j].iParent].body.pos + m_joints[m_joints[j].iParent].quat*m_joints[j].pivot[0];
				m_joints[j].dq_req = pivot;	// store for reusage in H calculation
				for(int ang=0;ang<3;ang++) if (!(m_joints[j].flags & angle0_locked<<ang)) {
					Vec3 w = m_joints[j].rotaxes[ang];
					((Vec3i&)m_joints[j].ddq)[ang] = i;	
					for(int ic=0,ici=0; ic<ncont; ici+=pcont[ic++]->bProcessed) {
						int j1 = m_infos[pcont[ic]->ipart[0]].iJoint;
						if (!inrange(j1,j-1,j+m_joints[j].nChildrenTree+1))
							continue;
						Vec3 v = pcont[ic]->flags & contact_angular ? w : w ^ m_joints[j1].body.pos-pivot;
						if (!(pcont[ic]->flags & (contact_angular|contact_constraint)))
							A[ici*narg+i] = v*pcont[ic]->n;
						else switch (pcont[ic]->flags & contact_constraint) {
							case contact_constraint_3dof:	for(int ix=0;ix<3;ix++) A[(ici+ix)*narg+i]=v[ix]; break;
							case contact_constraint_2dof:	A[ici*narg+i] = v*pcont[ic]->n; break;
							case contact_constraint_1dof: {
								Vec3 ax=pcont[ic]->n.GetOrthogonal().GetNormalized();
								for(int iax=0; iax<2; iax++,ax=pcont[ic]->n^ax)
									A[(ici+iax)*narg+i] = v*ax;
							} break;
						}
					}
					float q = m_joints[j].q[ang]+m_joints[j].qext[ang], lmin=m_joints[j].limits[0][ang], lmax=m_joints[j].limits[1][ang];
					lb[i] = (lmin-q)*rdt; ub[i] = (lmax-q)*rdt;
					if (action->mode) {
						float E=0; for(int c=0;c<=m_joints[j].nChildrenTree;c++)
							E += w*(m_joints[j+c].I*w) + (w^m_joints[j+c].body.pos-pivot).len2()*m_joints[j+c].body.M;
						H[(narg+1)*i] = E;	
					} else {
						Vec3 v = w ^ m_joints[j].body.pos-pivot;
						for(int iax=2-m_bGrounded*2;iax>=0;iax--)
							H[iax*narg+i]=H[i*narg+iax] = v[iax]*m_joints[j].body.M;
					}
					i++;
				}
			}
			// in non stiff mode, for each body:
			// w = sum (rotaxis) (for all joints down the path to the root)
			// v = sum (rotaxis ^ this_body_pos - rotaxis_pivot) + v_root
			// dE = M*v^2 + w*(I*w)
			// iterate from the current axis down to update Hij components in the expansion of v^2 and wIw
			#define ChainToRoot(idx) \
				for(int j##idx=j;j##idx>=0;j##idx=m_joints[j##idx].iParent) for(int ang##idx=0;ang##idx<3;ang##idx++) if (!(m_joints[j##idx].flags & angle0_locked<<ang##idx)) { \
					int i##idx = ((Vec3i&)m_joints[j##idx].ddq)[ang##idx]; \
					Vec3 w##idx = m_joints[j##idx].rotaxes[ang##idx], v##idx = w##idx^m_joints[j].body.pos-m_joints[j##idx].dq_req; 
			if (!action->mode) for(int j=0;j<m_nJoints;j++) ChainToRoot(0) ChainToRoot(1)
				H[i0*narg+i1] += (v0*v1)*m_joints[j].body.M + w0*(m_joints[j].I*w1);
			} }

			qpOASES::QProblem qp(narg, nconstr, qpOASES::HST_POSDEF);
			float edrift=e;	int ndrifts=4;
			for(; ndrifts>=0 && qp.init(H,arg,A,lb,ub,lbA,ubA,i=action->maxItersInner)!=qpOASES::SUCCESSFUL_RETURN; ndrifts--,edrift*=2)	{
				for(int j=0;j<ndriftable;j++) lb[j]-=edrift, ub[j]+=edrift;
			}
			if (ndrifts<0)
				break;
			qp.getPrimalSolution(arg);

			if (!m_bGrounded)
				m_body.v = Vec3(arg[0],arg[1],arg[2]);
			i = (1-m_bGrounded)*3;
			for(int j=0;j<m_nJoints;j++) for(int ang=0;ang<3;ang++) if (!(m_joints[j].flags & angle0_locked<<ang))
				m_joints[j].dq[ang] = arg[i++];
			SyncBodyWithJoint(0,2);
		}
		for(i=0; i<m_nJoints; i++) {
			m_joints[i].dq.zero(); m_joints[i].ddq.zero();
			MARK_UNUSED m_joints[i].dq_req.x,m_joints[i].dq_req.y,m_joints[i].dq_req.z;
			SyncBodyWithJoint(i,2);
			m_joints[i].body.P = (m_joints[i].body.v=m_joints[i].Pext)*m_joints[i].body.M;
			m_joints[i].Pext.zero();
		}
		m_body.v.zero();
		m_bFeatherstone=bFeatherstone; m_flags=flags; m_bAwake=awake;

		UnmaskIgnoredColliders(constrMask,iCaller);
		ComputeBBox(m_BBoxNew);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
		PostStepNotify(0,0,0,iCaller);

		if (ncoll) {
			EventPhysSimFinished epsf; InitEventBase(&epsf,this);
			epsf.frameData = nullptr;	epsf.time = 0;
			epsf.numColl = ncoll;
			m_pWorld->OnEvent(0, &epsf);
		}
		return 1;
	}

	return CRigidEntity::Action(_action,1);
}


int CArticulatedEntity::GetPotentialColliders(CPhysicalEntity **&pentlist, float dt)
{
	pentlist = m_pCollEntList; return m_nCollEnts;
}

int CArticulatedEntity::CheckSelfCollision(int ipart0,int ipart1)
{
	return m_joints[m_infos[ipart0].iJoint].selfCollMask & getmask(ipart1);
}

RigidBody *CArticulatedEntity::GetRigidBody(int ipart,int bModify)
{
	return (unsigned int)ipart<(unsigned int)m_nParts ? &m_joints[m_infos[ipart].iJoint].body : &m_body;
}
RigidBody *CArticulatedEntity::GetRigidBodyData(RigidBody *pbody, int ipart) 
{ 
	RigidBody *pbodyInt = GetRigidBody(ipart);
	*pbody = *pbodyInt;
	if (m_iSimClass!=4 || (unsigned int)ipart>=(unsigned int)m_nParts)
		pbody->v += m_velHost;
	else {
		Vec3 pos0=m_posHist[0]+m_qHist[0]*m_infos[ipart].posHist[0], pos1=m_posHist[1]+m_qHist[1]*m_infos[ipart].posHist[1];
		quaternionf q0=m_qHist[0]*m_infos[ipart].qHist[0], q1=m_qHist[1]*m_infos[ipart].qHist[1], dq=q1*!q0;
		pbody->pos = pos1;
		pbody->v = (pos1-pos0)*m_rhistTime;
		pbody->w = dq.v*(dq.w*2*m_rhistTime);
		if (pbody->v.len2()>sqr(m_pWorld->m_vars.maxVelBones)) {
			float scale = m_pWorld->m_vars.maxVelBones*isqrt_fast_tpl(pbody->v.len2());
			pbody->v*=scale; pbody->w*=scale;
		}
	}
	return pbody;
}
Quat qlerp(const Quat &qlast,const Quat &q0,const Quat q1,float k) { return Quat::CreateNlerp(q1,q0,k)*!q1*qlast; }
void CArticulatedEntity::GetLocTransformLerped(int ipart, Vec3 &offs, quaternionf &q, float &scale, float k, const CPhysicalPlaceholder *trg) const 
{
	if ((unsigned int)ipart<(unsigned int)m_nParts) {
		Vec3 pos=m_pos-(m_posHist[1]-m_posHist[0])*k, posPart=m_parts[ipart].pos-(m_infos[ipart].posHist[1]-m_infos[ipart].posHist[0])*k;
		quaternionf qrot=qlerp(m_qrot,m_qHist[0],m_qHist[1],k), qpart=qlerp(m_infos[ipart].q,m_infos[ipart].qHist[0],m_infos[ipart].qHist[1],k);
		q = qrot*qpart;
		offs = qrot*posPart + pos;
		scale = m_parts[ipart].scale;
	} else {
		q.SetIdentity(); offs.zero(); scale=1.0f;
	}
	m_pWorld->TransformToGrid(this,trg, offs,q);
}

void CArticulatedEntity::UpdatePosition(int bGridLocked)
{
	int i;
	WriteLock lock(m_lockUpdate); 

	m_pos = m_pNewCoords->pos; m_qrot = m_pNewCoords->q;
	for(i=0; i<m_nParts; i++) {
		m_parts[i].pos = m_parts[i].pNewCoords->pos; m_parts[i].q = m_parts[i].pNewCoords->q;
		m_parts[i].BBox[0] = m_parts[i].pNewCoords->BBox[0]; m_parts[i].BBox[1] = m_parts[i].pNewCoords->BBox[1];
	}
	m_BBox[0]=m_BBoxNew[0]; m_BBox[1]=m_BBoxNew[1];
	m_pWorld->UnlockGrid(this,-bGridLocked);
}

void CArticulatedEntity::UpdateJointDyn()
{
	WriteLock lock(m_lockJoints); 
	for(int i=0;i<m_nJoints; i++) {
		m_joints[i].prev_q = m_joints[i].q;
		m_joints[i].prev_dq = m_joints[i].dq;
		m_joints[i].prev_pos = m_joints[i].body.pos;
		m_joints[i].prev_qrot = m_joints[i].body.q;
		m_joints[i].prev_v = m_joints[i].body.v;
		m_joints[i].prev_w = m_joints[i].body.w;
	}
}


void CArticulatedEntity::OnHostSync(CPhysicalEntity *pHost)
{
	if (m_pHost==pHost && m_pSyncCoords!=(coord_block*)&m_pos) {
		m_pSyncCoords->pos = pHost->m_pSyncCoords->q*m_posHostPivot + pHost->m_pSyncCoords->pos - m_offsPivot;
		m_pSyncCoords->q = pHost->m_pSyncCoords->q*m_qHostPivot;
	}
}

int CArticulatedEntity::SyncWithHost(int bRecalcJoints, float time_interval)
{
	if (m_pHost) {
		if (m_pHost->m_iSimClass==7) {
			m_pHost->RemoveCollider(this); --m_pHost->m_nSyncColliders;
			m_pHost->Release(); m_pHost=0; return 0;
		}
		int i,ipart;
		pe_status_pos sp;
		m_pHost->GetStatus(&sp);
		m_posPivot = sp.q*m_posHostPivot + sp.pos;
		pe_params_pos pp;
		pp.q = m_pHost->m_qrot*m_qHostPivot;
		pp.pos = m_posPivot - pp.q*m_offsPivot;
		pp.pGridRefEnt = m_pHost;
		pp.bRecalcBounds = 0;
		if (!m_bAwake || m_body.M<=0 || m_flags & aef_recorded_physics) {
			SetParams(&pp,1);
			m_bUpdateBodies = bRecalcJoints = 0;
		} else if (m_nJoints)	{
			m_joints[0].quat0 = pp.q;
			m_posPivot = pp.pos;
		}
		m_flags = m_flags & ~pef_invisible | m_pHost->m_flags & pef_invisible;
		if (time_interval>0) {
			pe_status_dynamics sd;
			m_pHost->GetStatus(&sd);
			m_velHost = sd.v;
			if (m_bInheritVel) {
				float rdt = 1.0f/time_interval;
				m_acc = (sd.v-m_body.v)*rdt;
				m_wacc = (sd.w-m_body.w)*rdt;
				m_body.v = sd.v;
				m_body.w = sd.w;
			}
		}
		if (bRecalcJoints)
			for(i=0;i<m_nJoints;i++) SyncBodyWithJoint(i);
		else for(i=0;i<m_nJoints;i++) {
			ipart = m_joints[i].iStartPart;
			m_joints[i].quat = m_parts[ipart].q*!m_infos[ipart].q0;
			m_joints[i].body.q = m_joints[i].quat*!m_joints[i].body.qfb;
			m_joints[i].body.pos = (m_parts[ipart].pos-m_joints[i].quat*m_infos[ipart].pos0)+m_pos;
		}
		ComputeBBox(m_BBoxNew);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
		return 1; 
	}
	return 0; 
}

float CArticulatedEntity::GetDamping(float time_interval)
{
	float damping = CRigidEntity::GetDamping(time_interval);
	if (m_nBodyContacts>=m_nCollLyingMode)
		damping = min(1.0f-m_dampingLyingMode*time_interval, damping);
	return damping;
}


int CArticulatedEntity::IsAwake(int ipart) const
{
	return (unsigned int)ipart<(unsigned int)m_nParts ? m_joints[m_infos[ipart].iJoint].bAwake : m_bAwake;
}


int CArticulatedEntity::GetUnprojAxis(int idx, Vec3 &axis)
{
	axis.Set(0,0,0);
	if (!(m_joints[idx].flags & (angle0_limit_reached|angle0_locked|angle0_gimbal_locked)*6)) // y and z axes are not locked
		return 1;
	else {
		int i; for(i=2; i>=0 && m_joints[idx].flags & (angle0_limit_reached|angle0_locked|angle0_gimbal_locked)<<i; i--);
		if (i>=0)	{
			axis = m_joints[idx].rotaxes[i]; // free axis from list: z y x
			return 1;
		}
	}
	return 0;
}

float CArticulatedEntity::GetMaxTimeStep(float time_interval)
{
	return CRigidEntity::GetMaxTimeStep(time_interval);
}

int CArticulatedEntity::Step(float time_interval)
{
	int i,j,bboxUpdated,bBounced=0;
	Vec3 gravity;
	int bWasAwake = m_bAwake;
	//float maxPenetrationPrev = m_maxPenetrationCur;
	box bbox;
	pe_params_buoyancy pb[4];
	if (m_nBodyContacts>=m_nCollLyingMode) {
		gravity = m_gravityLyingMode;
		m_iSimTypeCur = m_iSimTypeLyingMode;
	} else {
		gravity = m_nBodyContacts ? m_gravity : m_gravityFreefall;
		m_iSimTypeCur = m_iSimType | m_bFloating & 1-m_bFeatherstone;
	}

	for(i=m_nColliders-1;i>=0;i--)
		if (m_pColliders[i]->m_iSimClass==7)
			RemoveColliderMono(m_pColliders[i]);
	if (m_pHost && m_pHost->m_iSimClass==7) {
		m_pForeignData = 0;
		m_pHost->Release(), m_pHost=0;
	}
	if (m_bUpdateBodies & m_iSimTypeCur)
		for(i=0;i<m_nJoints;i++)
			SyncBodyWithJoint(i);
	m_bUpdateBodies = 0;
	m_iLastLog = max(1,m_iLastLog);

	if (m_timeStepPerformed > m_timeStepFull-0.001f || m_flags & pef_disabled)
		return UpdateHistory(1);
	m_timeStepPerformed += time_interval;
	m_lastTimeStep = time_interval;
	m_dampingEx = 0;
	m_bContactsAssigned = 0;

	bboxUpdated = SyncWithHost(0,time_interval);

	bool bNoSim;
	if ((bNoSim = m_body.M==0 || m_flags & aef_recorded_physics) && m_iSimClass<=2) {
		if (!m_pHost) {
			float E;
			for(E=0.0f,i=0;i<m_nJoints;i++) 
				E += m_joints[i].body.v.len2()+m_joints[i].body.w.len2();
			if (m_bAwake = E>0.0f) for(i=0;i<m_nJoints;i++) {
				m_joints[i].body.Step(time_interval);
				SyncJointWithBody(i,3);
			}
			if (m_pStructure && m_pStructure->autoDetachmentDist>0) for(i=m_pStructure->nJoints-1;i>=0;i--)
				if (m_pStructure->pJoints[i].ipart[0]>=0 && 
						(m_joints[m_infos[m_pStructure->pJoints[i].ipart[0]].iJoint].body.pos-m_pos).len2()>sqr(m_pStructure->autoDetachmentDist)) {
					if (i!=m_pStructure->nJoints-1)
						m_pStructure->pJoints[i] = m_pStructure->pJoints[m_pStructure->nJoints-1];
					--m_pStructure->nJoints;
					m_pWorld->MarkEntityAsDeforming(this);
				}
		}

		if ((m_nParts>1) || (m_flags & aef_recorded_physics))
		{
			m_nCollEnts = m_pWorld->GetEntitiesAround(m_BBox[0],m_BBox[1], m_pCollEntList, 
				m_collTypes&(ent_sleeping_rigid|ent_rigid|ent_living|ent_independent)|ent_triggers, this);
		}

		for(i=0;i<m_nCollEnts;i++)
			m_pCollEntList[i]->Awake();
		return UpdateHistory(1);
	}
	if (!m_bAwake && !m_bCheckCollisions || m_nRoots>1 || (!m_bCheckCollisions && !m_bGrounded) || bNoSim) {
		if (m_iSimClass==4)
			UpdateConstraints(time_interval);
		return UpdateHistory(1);
	}

	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );
	PHYS_ENTITY_PROFILER

	int iCaller = get_iCaller_int();
	if (!bboxUpdated) ComputeBBox(m_BBoxNew);
	if (m_bCheckCollisions) {
		Vec3 sz = m_BBoxNew[1]-m_BBoxNew[0];
		float szmax = max(max(sz.x,sz.y),sz.z)*0.3f;
		if (m_body.v.len2()*sqr(time_interval) > szmax)
			szmax = m_body.v.len()*time_interval;
		sz.Set(szmax,szmax,szmax);
		masktype constrMask = MaskIgnoredColliders(iCaller,1);
		m_nCollEnts = m_pWorld->GetEntitiesAround(m_BBox[0]-sz,m_BBox[1]+sz, m_pCollEntList, m_collTypes|ent_sort_by_mass|ent_triggers, this, 0,iCaller);

		for(i=0;i<m_nCollEnts;i++) if (m_pCollEntList[i]->m_iSimClass>2 && !IgnoreCollision(m_pCollEntList[i]->m_collisionClass,m_collisionClass))
			if (m_pCollEntList[i]->GetType()!=PE_ARTICULATED) {
				m_pCollEntList[i]->Awake();
				if (!m_pWorld->m_vars.bMultiplayer || m_pCollEntList[i]->m_iSimClass!=3)
					m_pWorld->ScheduleForStep(m_pCollEntList[i],time_interval),m_nNonRigidNeighbours++;
			} else if (m_iSimClass==2 && m_body.v.len2()>sqr(5.0f))
				FakeRayCollision(m_pCollEntList[i], time_interval);

		if (m_iSimClass<=2) {
			for(i=j=0;i<m_nCollEnts;i++) if (!IgnoreCollision(m_pCollEntList[i]->m_collisionClass,m_collisionClass)) 
			if (m_pCollEntList[i]==this || m_pCollEntList[i]->m_iSimClass<=2 && (m_pCollEntList[i]->m_iGroup==m_iGroup && m_pCollEntList[i]->m_bMoved || 
					!m_pCollEntList[i]->IsAwake() || m_pCollEntList[i]->m_iGroup<0 || m_pWorld->m_pGroupNums[m_pCollEntList[i]->m_iGroup]<m_pWorld->m_pGroupNums[m_iGroup]))
				m_pCollEntList[j++] = m_pCollEntList[i];
			m_nCollEnts = j;
		} else
			m_nCollEnts = 0;
		UnmaskIgnoredColliders(constrMask, iCaller);
	} else 
		m_nCollEnts = 0;

	for(i=m_bFastLimbs=0; i<m_nJoints; i++) 
		m_bFastLimbs |= isneg(sqr(0.9f)-m_joints[i].body.w.len2()*sqr(time_interval));
	/*{
		m_parts[m_joints[i].iStartPart].pPhysGeomProxy->pGeom->GetBBox(&bbox);
		r = sqr(max(max(bbox.size.x,bbox.size.y),bbox.size.z)*m_parts[m_joints[i].iStartPart].scale);
		m_bFastLimbs |= isneg(r-(m_joints[i].body.v.len2()+m_joints[i].body.w.len2()*r)*sqr(time_interval));
	}*/
	m_iSimTypeCur &= m_bFastLimbs^1;
	m_iSimTypeCur |= m_iSimTypeOverride;
	m_iSimTypeCur &= isneg(m_iSimClass-3);

	m_qNew.SetIdentity();
	if (m_bPartPosForced) 
		for(i=0;i<m_nJoints;i++) {
			j = m_joints[i].iStartPart;
			m_joints[i].quat = m_qrot*m_parts[j].q*!m_infos[j].q0;
			m_joints[i].body.pos = m_qrot*m_parts[j].pos-m_joints[i].quat*m_infos[j].pos0+m_pos;
			m_joints[i].body.q = m_joints[i].quat*!m_joints[i].body.qfb;
			SyncJointWithBody(i);
			m_joints[i].qext += m_joints[i].q;
			m_joints[i].q.Set(0,0,0);
			for (j=0;j<3;j++) if (!(m_joints[i].flags & angle0_locked<<j) && 
					isneg(m_joints[i].limits[0][j]-m_joints[i].qext[j]) + isneg(m_joints[i].qext[j]-m_joints[i].limits[1][j]) + 
					isneg(m_joints[i].limits[1][j]-m_joints[i].limits[0][j]) < 2) 
			{	// qext violates limits; adjust the limits
				float diff[2];
				diff[0] = m_joints[i].limits[0][j]-m_joints[i].qext[j];
				diff[1] = m_joints[i].limits[1][j]-m_joints[i].qext[j]; 
				diff[0] -= (int)(diff[0]/(2*g_PI))*2*g_PI;
				diff[1] -= (int)(diff[1]/(2*g_PI))*2*g_PI;
				m_joints[i].limits[isneg(fabs_tpl(diff[1])-fabs_tpl(diff[0]))][j] = m_joints[i].qext[j];
			}
		}
	m_bPartPosForced = 0;
	m_prev_pos = m_pos;
	m_prev_vel = m_body.v;
	if (!m_bGrounded && m_iSimTypeCur)
		m_posPivot = m_joints[0].body.pos;
	// m_posPivot += m_body.v*time_interval;
	m_body.pos = m_posPivot;
	m_posNew = m_posPivot - m_offsPivot;
	m_body.offsfb = (m_body.pos-m_posNew)*m_qrot;
	m_bAwake = isneg(m_simTime-2.5f) | (iszero(m_nBodyContacts) & (m_bGrounded^1) & (m_bFloating^1));
	m_simTime += time_interval;
	m_simTimeAux += time_interval;
	m_maxPenetrationCur = 0;
	if (!m_bGrounded && !m_iSimTypeCur) {
		m_joints[0].flags &= ~(angle0_locked*7);
		m_joints[0].limits[0] = -(m_joints[0].limits[1]=Vec3(1e10f));
		m_joints[0].quat0 = (m_joints[0].quat0*Quat::CreateRotationXYZ(m_joints[0].q+m_joints[0].qext)).GetNormalized();
		m_joints[0].q = m_joints[0].qext = Ang3(ZERO);
	}
	if (!m_iSimTypeCur) for(i=0,m_constrInfoFlags=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) if (m_constraintMask & getmask(i))
		m_constrInfoFlags |= m_pConstraintInfos[i].flags;
	for(i=0; i<m_nJoints; i=StepJoint(i, time_interval, bBounced, iszero(m_nBodyContacts) & isneg(sqr(m_pWorld->m_vars.maxContactGap)-m_body.v.len2()*sqr(time_interval)), get_iCaller_int()));
	m_bAwake = isneg(-(int)m_bAwake);
	m_nBodyContacts = m_nDynContacts = 0;
	for(i=0;i<m_nJoints;i++) m_joints[i].bHasExtContacts = 0;

	if (!m_bFeatherstone)	{
		VerifyExistingContacts(m_pWorld->m_vars.maxContactGap);
		for(i=0;i<m_nJoints;i++) 
			m_joints[i].body.P = (m_joints[i].body.v+=gravity*time_interval)*m_joints[i].body.M;
		/*m_joints[i].body.w -= m_joints[i].dw_body;
			m_joints[i].body.v -= m_joints[i].dv_body-gravity*time_interval;
			m_joints[i].body.P = m_joints[i].body.v*m_joints[i].body.M;
			m_joints[i].body.L = m_joints[i].I*m_joints[i].body.w;
		}*/

		ComputeBBox(m_BBoxNew);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
		gravity = m_gravity;
		i = PostStepNotify(time_interval,pb,CRY_ARRAY_COUNT(pb),iCaller);
		ApplyBuoyancy(time_interval,m_gravityFreefall,pb,i);
		if ((m_gravity-gravity).len2()>0)
			m_gravityLyingMode = m_gravity;
		for(i=0;i<m_nJoints;i++) if (m_joints[i].Pimpact.len2()+m_joints[i].Limpact.len2()>0) {
			m_joints[i].body.P += m_joints[i].Pimpact; 
			m_joints[i].body.L += m_joints[i].Limpact; 
			m_joints[i].body.v = m_joints[i].body.P*m_joints[i].body.Minv;
			m_joints[i].body.w = m_joints[i].body.Iinv*m_joints[i].body.L;
			m_joints[i].Pimpact.zero(); m_joints[i].Limpact.zero();
		}
		m_bAwake = bWasAwake;
		m_bSteppedBack = 0;

		// do not request step back if we were in deep penetration state initially
		i = 1;//m_iSimTypeCur | isneg(m_maxPenetrationCur-0.07f) | isneg(0.07f-maxPenetrationPrev);
		if (!i)
			m_simTimeAux = 0;
		return UpdateHistory(i | isneg(3-(int)m_nStepBackCount));
	}

	if (m_iSimClass>2 && (m_simTime>10 || m_flags & pef_invisible || m_nJoints>1 && m_joints[1].dq.len2()>sqr(1000.0f))) {
		for(i=0;i<m_nJoints;i++) {
			m_joints[i].q=Ang3(ZERO); 
			m_joints[i].dq.zero();
		}
		m_bAwake = 0;
	}

	if (!m_joints[0].fs)
		AllocFSData();

	Matrix33 M0host; M0host.SetZero();
	if (m_pHost && m_bExertImpulse) {
		float Ihost_inv[6][6]; // TODO: support full spatial matrices if the host provides them
		memset(Ihost_inv, 0, sizeof(float)*36);
		m_pHost->GetSpatialContactMatrix(m_posPivot, -1, Ihost_inv);
		M0host = GetMtxStrided<float,6,1>(&Ihost_inv[3][0]);
	}

	StepFeatherstone(time_interval, bBounced, M0host);

	ComputeBBox(m_BBoxNew);
	UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
	UpdateJointDyn();
	gravity = m_gravity;
	PostStepNotify(time_interval,pb,CRY_ARRAY_COUNT(pb),iCaller);
	if ((m_gravity-gravity).len2()>0)
		m_gravityLyingMode = m_gravity;

	int bDone = UpdateHistory(isneg(m_timeStepFull-m_timeStepPerformed-0.001f));
	return bDone | isneg(m_pWorld->m_bWorldStep-2);
}


void CArticulatedEntity::UpdateJointRotationAxes(int idx)
{
	quaternionf q_parent = m_joints[idx].iParent>=0 ? m_joints[m_joints[idx].iParent].quat : m_qNew;
	SetBasisTFromMtx(m_joints[idx].rotaxes, Matrix33(q_parent*m_joints[idx].quat0));

	float cosa,sina;
	sincos_tpl(m_joints[idx].q.y+m_joints[idx].qext.y, &sina,&cosa);
	m_joints[idx].rotaxes[0] = m_joints[idx].rotaxes[0].GetRotated(m_joints[idx].rotaxes[1], cosa,sina); // rot x around y - pitch
	sincos_tpl(m_joints[idx].q.z+m_joints[idx].qext.z, &sina,&cosa);
	m_joints[idx].rotaxes[0] = m_joints[idx].rotaxes[0].GetRotated(m_joints[idx].rotaxes[2], cosa,sina); // rot x around z - yaw
	m_joints[idx].rotaxes[1] = m_joints[idx].rotaxes[1].GetRotated(m_joints[idx].rotaxes[2], cosa,sina); // rot y around z
}

void CArticulatedEntity::CheckForGimbalLock(int idx)
{
	int i;
	m_joints[idx].flags &= ~(angle0_gimbal_locked*7);
	if (!(m_joints[idx].flags & angle0_locked*5) && fabs(m_joints[idx].rotaxes[0]*m_joints[idx].rotaxes[2])>0.995f) {
		i = 2-iszero((int)m_joints[idx].flags & angle0_limit_reached<<2)*2; // gimbal lock the angle that already reached its limit, if this is the case
		m_joints[idx].flags |= angle0_gimbal_locked<<i;
		m_joints[idx].dq[i^2] += m_joints[idx].dq[i]*(m_joints[idx].rotaxes[0]*m_joints[idx].rotaxes[2]);
		m_joints[idx].dq[i] = 0;
	}
}

void CArticulatedEntity::SyncBodyWithJoint(int idx, int flags)
{
	Vec3 pos_parent,pivot,wpivot;
	quaternionf q_parent,q;
	Matrix33 R;
	int i;
	if (m_joints[idx].iParent>=0) {
		q_parent = m_joints[m_joints[idx].iParent].quat;
		pos_parent = m_joints[m_joints[idx].iParent].body.pos;
		pivot = pos_parent + q_parent*m_joints[idx].pivot[0];
	} else {
		q_parent = m_qNew; pos_parent = pivot = m_posPivot;
	}

	if (flags & 1) { // sync geometry
		UpdateJointRotationAxes(idx);

	//	m_joints[idx].quat = q_parent*m_joints[idx].quat0*quaternionf(m_joints[idx].q+m_joints[idx].qext);
		m_joints[idx].quat = q_parent*m_joints[idx].quat0*Quat::CreateRotationXYZ(m_joints[idx].q+m_joints[idx].qext);
		m_joints[idx].body.q = m_joints[idx].quat*!m_joints[idx].body.qfb;
		m_joints[idx].body.pos = pos_parent + q_parent*m_joints[idx].pivot[0] - m_joints[idx].quat*m_joints[idx].pivot[1];
		R = Matrix33(m_joints[idx].body.q);
		m_joints[idx].body.Iinv = R*m_joints[idx].body.Ibody_inv*R.T();
		m_joints[idx].I = R*m_joints[idx].body.Ibody*R.T();

		for(i=m_joints[idx].iStartPart; i<m_joints[idx].iStartPart+m_joints[idx].nParts; i++) {
			m_infos[i].q = m_joints[idx].quat*m_infos[i].q0;
			m_infos[i].pos = m_joints[idx].quat*m_infos[i].pos0+m_joints[idx].body.pos-m_posNew;
		}
	}

	if (flags & 2) { // sync velocities
		if (m_joints[idx].iParent>=0) {
			m_joints[idx].body.w = m_joints[m_joints[idx].iParent].body.w;
			m_joints[idx].body.v = m_joints[m_joints[idx].iParent].body.v + 
				(m_joints[m_joints[idx].iParent].body.w^m_joints[idx].body.pos-m_joints[m_joints[idx].iParent].body.pos);
		} else {
			m_joints[idx].body.v = m_body.v;
			if (m_bGrounded) {
				m_joints[idx].body.w = m_body.w;
				m_joints[idx].body.v += m_body.w^m_joints[idx].body.pos-m_posPivot;
			}	else m_joints[idx].body.w.Set(0,0,0);
		}

		for(i=0,wpivot.Set(0,0,0); i<3; i++)
			wpivot += m_joints[idx].rotaxes[i]*(m_joints[idx].dq[i]+m_joints[idx].dqext[i]*m_bFeatherstone);
		m_joints[idx].body.v += wpivot^m_joints[idx].body.pos-pivot;
		m_joints[idx].body.w += wpivot;
		m_joints[idx].body.P = m_joints[idx].body.v*m_joints[idx].body.M;
		m_joints[idx].body.L = m_joints[idx].I*m_joints[idx].body.w;
	}
}

void CArticulatedEntity::SyncJointWithBody(int idx, int flags)
{
	if (flags & 1) { // sync angles
		quaternionf qparent = m_joints[idx].iParent>=0 ? m_joints[m_joints[idx].iParent].quat : m_qNew;
		m_joints[idx].quat = m_joints[idx].body.q*m_joints[idx].body.qfb;
		if (idx==0 && !m_bGrounded) {
			m_joints[idx].quat0 = m_joints[idx].quat;
			m_joints[idx].bQuat0Changed = 1;
			m_joints[idx].q = -m_joints[idx].qext;
			SetBasisTFromMtx(m_joints[idx].rotaxes, Matrix33(m_joints[idx].quat));
		} else {
			m_joints[idx].q = Ang3::GetAnglesXYZ(!(qparent*m_joints[idx].quat0)*m_joints[idx].quat);
			m_joints[idx].q -= m_joints[idx].qext;
			UpdateJointRotationAxes(idx);
			CheckForGimbalLock(idx);
		}
	}

	if (flags & 2) { // sync velocities
		Vec3 wrel = m_joints[idx].iParent>=0 ? m_joints[m_joints[idx].iParent].body.w : m_body.w;
		wrel = m_joints[idx].body.w - wrel;
		if (!(m_joints[idx].flags & (angle0_locked|angle0_gimbal_locked)*7)) {
			Matrix33 Basis_inv = GetMtxFromBasis(m_joints[idx].rotaxes);
			Basis_inv.Invert(); m_joints[idx].dq = wrel*Basis_inv;
		} else {
			int i,j,iAxes[3];
			for(i=j=0;i<3;i++) if (!(m_joints[idx].flags & (angle0_locked|angle0_gimbal_locked)<<i))
				iAxes[j++] = i;
			m_joints[idx].dq.Set(0,0,0);
			if (j==1)
				m_joints[idx].dq[iAxes[0]] = m_joints[idx].rotaxes[iAxes[0]]*wrel;
			else if (j==2) {
				if (!(m_joints[idx].flags & (angle0_locked|angle0_gimbal_locked)<<1)) {
					// only x and z axes can be nonorthogonal, so just project wrel on active axes in other cases
					m_joints[idx].dq[iAxes[0]] = m_joints[idx].rotaxes[iAxes[0]]*wrel;
					m_joints[idx].dq[iAxes[1]] = m_joints[idx].rotaxes[iAxes[1]]*wrel;
				} else {
					Vec3 axisz,axisy;
					axisz = (m_joints[idx].rotaxes[iAxes[0]]^m_joints[idx].rotaxes[iAxes[1]]).normalized();
					axisy = axisz ^ m_joints[idx].rotaxes[iAxes[0]];
					m_joints[idx].dq[iAxes[1]] = (axisy*wrel)/(axisy*m_joints[idx].rotaxes[iAxes[1]]);
					m_joints[idx].dq[iAxes[0]] = wrel*m_joints[idx].rotaxes[iAxes[0]] - 
						(m_joints[idx].rotaxes[iAxes[0]]*m_joints[idx].rotaxes[iAxes[1]])*m_joints[idx].dq[iAxes[1]];
				}
			}
		}
	}
}


float CArticulatedEntity::CalcEnergy(float time_interval)
{
  float maxVel = m_pWorld->m_vars.maxVel; 
	float E=m_Ejoints,vmax=0,Emax=m_body.M*sqr(maxVel)*2; 
	int i; Vec3 v;
	entity_contact *pContact;

	for(i=0; i<m_nJoints; i++) m_joints[i].ddq.zero();
	if (time_interval>0) for(pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next) {
		v = m_joints[m_infos[pContact->ipart[0]].iJoint].ddq;
		m_joints[m_infos[pContact->ipart[0]].iJoint].ddq += 
			pContact->n*max(0.0f,max(0.0f,pContact->vreq*pContact->n-max(0.0f,pContact->vrel))-v*pContact->n);
		if (pContact->pbody[1]->Minv==0 && pContact->pent[1]->m_iSimClass>1) {
			RigidBody *pbody = pContact->pbody[1];
			vmax = max(vmax,(pbody->v + (pbody->w^pbody->pos-pContact->pt[1])).len2());
			Emax = m_body.M*sqr(10.0f)*2; // since we'll allow some extra energy, make sure we restrict the upper limit
		}
	}

	for(i=0; i<m_nJoints; i++)
		E += m_joints[i].body.M*(m_joints[i].body.v+m_joints[i].ddq).len2() + m_joints[i].body.L*m_joints[i].body.w +
		     m_joints[i].Pimpact.len2()*m_joints[i].body.Minv + m_joints[i].Limpact*(m_joints[i].body.Iinv*m_joints[i].Limpact);

	if (time_interval>0) {
		v.Set(0,0,0);
		for(i=0; i<NMASKBITS && getmask(i)<=m_constraintMask; i++) 
		if (m_constraintMask & getmask(i) && (m_pConstraints[i].flags & contact_constraint_3dof || m_pConstraintInfos[i].flags & constraint_rope) && !(m_pConstraintInfos[i].flags & constraint_inactive)) {
			v += m_pConstraints[i].n*max(0.0f,max(0.0f,m_pConstraints[i].vreq*m_pConstraints[i].n-max(0.0f,m_pConstraints[i].vrel))-
				m_pConstraints[i].n*v);
			if (m_pConstraints[i].pbody[1]->Minv==0 && m_pConstraints[i].pent[1]->m_iSimClass>1) {
				RigidBody *pbody = m_pConstraints[i].pbody[1];
				vmax = max(vmax,(pbody->v + (pbody->w^pbody->pos-m_pConstraints[i].pt[1])).len2());
			}
		}
		E += m_body.M*(v.len2()+vmax);
		return min_safe(E,Emax);
	}

	return E;
}

int CArticulatedEntity::CalcBodyZa(int idx, float time_interval, vectornf &Za_change)
{
	int i,j,curidx,nextidx,idir;
	float kd,tlim,qdashpot,dq_dashpot,k,qbuf[3]; 
	vectornf Q(3,m_joints[idx].fs->Q),Qtmp(3,qbuf);
	Vec3 d,r,pos_parent,Qscale;
	quaternionf q_parent;
	Vec3 Za_vec[2]; vectornf Za(6,Za_vec[0]);

	if (m_joints[idx].iParent>=0) {
		pos_parent = m_joints[m_joints[idx].iParent].body.pos;
		q_parent = m_joints[m_joints[idx].iParent].quat;
	} else {
		pos_parent = m_posPivot; q_parent = m_qNew;
	}
	d = m_joints[idx].body.pos - (pos_parent + q_parent*m_joints[idx].pivot[0]);
	r = m_joints[idx].body.pos - pos_parent;

	if (m_joints[idx].flags & joint_no_gravity)
		Za_vec[0].zero();
	else
		Za_vec[0] = m_gravity*-m_joints[idx].body.M*time_interval;
	Za_vec[1] = (m_joints[idx].body.w^m_joints[idx].I*m_joints[idx].body.w)*time_interval;
	Za_vec[0] += m_joints[idx].dv_body*m_joints[idx].body.M;
	Za_vec[1] += m_joints[idx].I*m_joints[idx].dw_body;
	m_joints[idx].fs->Q.zero();
	m_joints[idx].ddq.zero();

	if (!(m_joints[idx].flags & joint_isolated_accelerations)) for(j=0;j<m_joints[idx].nPotentialAngles;j++) 
		Qscale[j] = 1.0f/m_joints[idx].fs->qinv[j*(m_joints[idx].nPotentialAngles+1)];
	else Qscale.Set(1.0f,1.0f,1.0f);

	// calculate torque vector and spatial joint axes
	for(j=0;j<m_joints[idx].nPotentialAngles;j++) {
		i = m_joints[idx].fs->axidx2qidx[j];
		kd = fabsf(m_joints[idx].dq[i])>15.0f ? 0 : m_joints[idx].kd[i];
		k = time_interval*Qscale[j];
		Q[j] = (m_joints[idx].ks[i]*m_joints[idx].q[i] + kd*m_joints[idx].dq[i])*-k;
		// if the joint is approaching limit (but haven't breached it yet) and is in dashpot area, damp the velocity
		if (fabsf(m_joints[idx].dq[i])>0.5f) {
			idir = isneg(m_joints[idx].dq[i])^1;
			tlim = m_joints[idx].q[i]-m_joints[idx].limits[idir][i];
			if (tlim<-g_PI) tlim += 2*g_PI;
			if (tlim>g_PI) tlim -= 2*g_PI;
			tlim *= idir*2-1;	qdashpot = m_joints[idx].qdashpot[i];
			dq_dashpot = min(fabsf(m_joints[idx].dq[i])*Qscale[j], (qdashpot-tlim)*m_joints[idx].kdashpot[i]*isneg(fabsf(tlim-2*qdashpot)-qdashpot)*k);
			Q[j] += dq_dashpot*(1-idir*2);
		}
	}
	if (!(m_joints[idx].flags & (angle0_locked|angle0_gimbal_locked)*5)) { // take into accout that x and z axes influence one another
		Q[m_joints[idx].fs->qidx2axidx[0]] += Q[m_joints[idx].fs->qidx2axidx[2]]*(m_joints[idx].rotaxes[0]*m_joints[idx].rotaxes[2]);
		Q[m_joints[idx].fs->qidx2axidx[2]] += Q[m_joints[idx].fs->qidx2axidx[0]]*(m_joints[idx].rotaxes[0]*m_joints[idx].rotaxes[2]);
	}
	if (m_joints[idx].flags & joint_isolated_accelerations)
		m_joints[idx].ddq = m_joints[idx].fs->Q;

	Vec3 Za_child_vec[2]; vectornf Za_child(6,Za_child_vec[0]);
	// iterate all children, accumulate Za
	for(i=0,curidx=idx+1; i<m_joints[idx].nChildren; i++) {
		nextidx = CalcBodyZa(curidx, time_interval, Za_child);
		Za += Za_child;	Za_vec[1] += Za_child_vec[0] ^ m_joints[idx].body.pos-m_joints[curidx].body.pos;
		curidx = nextidx;
	}

	// calculate Za changes for the parent
	Za_change = Za;
	vectornf ddq(m_joints[idx].nActiveAngles, m_joints[idx].ddq);
	if (m_joints[idx].nActiveAngles>0) {
		matrixf sT(Q.len = m_joints[idx].nActiveAngles,6,0, m_joints[idx].fs->s_vec[0][0]);
		matrixf qinv_down(m_joints[idx].nActiveAngles,m_joints[idx].nActiveAngles,0, m_joints[idx].fs->qinv_down);
		matrixf Ia_s(6,m_joints[idx].nActiveAngles,0, m_joints[idx].fs->Ia_s);
		if (!(m_joints[idx].flags & joint_isolated_accelerations))
			ddq = qinv_down*((Qtmp=Q)-=sT*Za);
		Za_change += Ia_s*ddq;
	}

	if (m_joints[idx].nActiveAngles<m_joints[idx].nPotentialAngles && !(m_joints[idx].flags & joint_isolated_accelerations)) {
		matrixf sT(Q.len = m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->s_vec[0][0]);
		matrixf qinv(m_joints[idx].nPotentialAngles,m_joints[idx].nPotentialAngles,0, m_joints[idx].fs->qinv);
		matrixf qinv_sT(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT[0]);
		ddq = qinv*((Qtmp=Q)-=sT*Za);
	}

	return curidx;
}


int CArticulatedEntity::CalcBodyIa(int idx, matrixf& Ia_change, int bIncludeLimits)
{
	int i,j,curidx,nextidx;
	Vec3 d,r,pos_parent;
	quaternionf q_parent;

	if (m_joints[idx].iParent>=0) {
		pos_parent = m_joints[m_joints[idx].iParent].body.pos;
		q_parent = m_joints[m_joints[idx].iParent].quat;
	} else {
		pos_parent = m_posPivot; q_parent = m_qNew;
	}
	d = m_joints[idx].body.pos - (pos_parent + q_parent*m_joints[idx].pivot[0]);
	r = m_joints[idx].body.pos - pos_parent;

	// fill initial Ia
	matrixf Ia(6,6,0,m_joints[idx].fs->Ia[0]); Ia.zero();
	m_joints[idx].fs->Ia[0][3]=m_joints[idx].fs->Ia[1][4]=m_joints[idx].fs->Ia[2][5] = m_joints[idx].body.M;
	SetMtxStrided<float,6,1>(m_joints[idx].I, &m_joints[idx].fs->Ia[3][0]);

	// sort axes so that active angles' axes come first
	for (i=j=0;i<3;i++) if (!(m_joints[idx].flags & (angle0_locked|angle0_limit_reached|angle0_gimbal_locked)<<i)) {
		m_joints[idx].fs->qidx2axidx[i] = j; m_joints[idx].fs->axidx2qidx[j++] = i;
	} m_joints[idx].nActiveAngles = j;
	for (i=0;i<3;i++) if ((m_joints[idx].flags & (angle0_locked|angle0_limit_reached)<<i & -bIncludeLimits) == angle0_limit_reached<<i) {
		m_joints[idx].fs->qidx2axidx[i] = j; m_joints[idx].fs->axidx2qidx[j++] = i;
	} m_joints[idx].nPotentialAngles = j;
	m_joints[idx].nActiveAngles += m_joints[idx].nPotentialAngles-m_joints[idx].nActiveAngles & m_iSimClass-3>>31;

	for(j=0;j<m_joints[idx].nPotentialAngles;j++) {
		i = m_joints[idx].fs->axidx2qidx[j];
		m_joints[idx].fs->s_vec[j][0] = m_joints[idx].rotaxes[i]^d; 
		m_joints[idx].fs->s_vec[j][1] = m_joints[idx].rotaxes[i];
	}
	
	float buf[39]; matrixf Ia_child(6,6,0,_align16(buf));
	// iterate all children, accumulate Ia
	for(i=0,curidx=idx+1; i<m_joints[idx].nChildren; i++) {
		nextidx = CalcBodyIa(curidx, Ia_child);
		r = m_joints[idx].body.pos-m_joints[curidx].body.pos;
		LeftOffsetSpatialMatrix(Ia_child, -r);
		RightOffsetSpatialMatrix(Ia_child, r);
		Ia += Ia_child;
		curidx = nextidx;
	}

	// calculate Ia changes for the parent
	Ia_change = Ia;
	matrixf sT(m_joints[idx].nActiveAngles,6,0, m_joints[idx].fs->s_vec[0][0]);
	matrixf s(6,m_joints[idx].nActiveAngles,0, m_joints[idx].fs->s);
	matrixf Ia_s_qinv_sT(6,6,0, m_joints[idx].fs->Ia_s_qinv_sT[0]);
	matrixf qinv(m_joints[idx].nActiveAngles,m_joints[idx].nActiveAngles,0, m_joints[idx].fs->qinv);
	if (m_joints[idx].nActiveAngles>0) {
		SpatialTranspose(sT, s);
		matrixf Ia_s(6,m_joints[idx].nActiveAngles,0, m_joints[idx].fs->Ia_s);
		matrixf qinv_down(m_joints[idx].nActiveAngles,m_joints[idx].nActiveAngles,0, m_joints[idx].fs->qinv_down);
		matrixf Ia_s_qinv(6,m_joints[idx].nActiveAngles,0, buf);
		Ia_s = Ia*s; qinv = (qinv_down = sT*Ia_s).invert();
		Ia_s_qinv_sT = (Ia_s_qinv = Ia_s*qinv)*sT;
		Ia_change -= Ia_s_qinv_sT*Ia;
		Ia_s_qinv_sT *= -1.0f;
	} else Ia_s_qinv_sT.zero();
	for(i=0;i<6;i++) m_joints[idx].fs->Ia_s_qinv_sT[i][i] += 1;
	r = m_joints[idx].body.pos - pos_parent;
	LeftOffsetSpatialMatrix(Ia_s_qinv_sT, r);

	if (m_joints[idx].nPotentialAngles>0) {
		// precalculate matrices for velocity propagation up - use limited angles also 
		matrixf qinv_sT(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT[0]);
		matrixf qinv_sT_Ia(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT_Ia[0]);
		matrixf sT_Ia(m_joints[idx].nPotentialAngles,6,0, buf);
		if (m_joints[idx].nActiveAngles < m_joints[idx].nPotentialAngles) {
			sT.nRows = m_joints[idx].nPotentialAngles; SpatialTranspose(sT, s);
			(qinv = (sT_Ia = sT*Ia)*s).invert();
		}
		(qinv_sT = qinv*sT)*=-1.0f; qinv_sT_Ia = qinv_sT*Ia;
		//matrixf(6,6,0,m_joints[idx].s_qinv_sT_Ia[0]) = s*qinv_sT_Ia;
	}

	return curidx;
}


int CArticulatedEntity::CollectPendingImpulses(int idx,int &bNotZero,int bBounce)
{
	int i,j,curidx,newidx,bChildNotZero;
	bNotZero = 0;
	bBounce &= 2-m_iSimClass>>31;

	for(i=0;i<(3&-bBounce);i++) if (!is_unused(m_joints[idx].dq_req[i])) {
		// note that articulation matrices from the previous frame are used, while rotaxes are already new; this introduces slight inconsistency
		//Matrix33 K; GetJointTorqueResponseMatrix(idx, K);
		//float resp = m_joints[idx].rotaxes[i]*K*m_joints[idx].rotaxes[i];
    float buf[39],buf1[39]; matrixf Mresp(6,6,0,_align16(buf)),MrespLoc(6,6,0,_align16(buf1));
		MrespLoc = matrixf(6,6,0,m_joints[idx].fs->Ia_s_qinv_sT[0]);
		for(j=0;j<6;j++) MrespLoc[j][j] -= 1.0f;
		Mresp = matrixf(6,6,0,m_joints[m_joints[idx].iParent].fs->Iinv[0])*MrespLoc;
		LeftOffsetSpatialMatrix(Mresp,m_joints[m_joints[idx].iParent].body.pos-m_joints[idx].body.pos);
		j = m_joints[idx].fs->qidx2axidx[i];
		Vec3 axis = m_joints[idx].rotaxes[i], dw = GetMtxStrided<float,6,1>(&Mresp[0][3])*axis, 
				 dv = GetMtxStrided<float,6,1>(&Mresp[3][3])*axis;
		float resp = -(Vec3(m_joints[idx].fs->qinv_sT[j]+3)*axis);
		resp -= Vec3(m_joints[idx].fs->qinv_sT_Ia[j])*dw + Vec3(m_joints[idx].fs->qinv_sT_Ia[j]+3)*dv;

		if (resp>0.1f*m_body.Minv) {
			resp = (m_joints[idx].dq_req[i]-m_joints[idx].dq[i])/resp*m_scaleBounceResponse;
			m_joints[idx].fs->Ya_vec[1] -= m_joints[idx].rotaxes[i]*resp;
			if (m_joints[idx].iParent>=0)
				m_joints[m_joints[idx].iParent].fs->Ya_vec[1] += m_joints[idx].rotaxes[i]*resp;
			bNotZero++;
		}
	}

	if (bNotZero += isneg(1E-10f-m_joints[idx].Pext.len2()-m_joints[idx].Lext.len2())) {
		m_joints[idx].fs->Ya_vec[0] -= m_joints[idx].Pext; m_joints[idx].Pext.zero();
		m_joints[idx].fs->Ya_vec[1] -= m_joints[idx].Lext; m_joints[idx].Lext.zero();
	}

	for(i=0,curidx=idx+1; i<m_joints[idx].nChildren; i++) {
		newidx = CollectPendingImpulses(curidx, bChildNotZero, bBounce);
		if (bChildNotZero) {
			matrixf Ia_s_qinv_sT(6,6,0,m_joints[curidx].fs->Ia_s_qinv_sT[0]);
			vectornf Ya(6,m_joints[idx].fs->Ya_vec[0]);
			vectornf Ya_child(6,m_joints[curidx].fs->Ya_vec[0]);
			Ya += Ia_s_qinv_sT*Ya_child;
			bNotZero += bChildNotZero;
		}
		curidx = newidx;
	}
	if (idx==0)
		vectornf(6,(float*)(m_Ya_vec[0])) = matrixf(6,6,0,m_joints[0].fs->Ia_s_qinv_sT[0])*vectornf(6,m_joints[0].fs->Ya_vec[0]);
	return curidx;
}


void CArticulatedEntity::PropagateImpulses(const Vec3 &dv, int bLockLimits, int bApplyVel, const Vec3 &dw)
{
	int idx,i,j,bHitsLimit;
	Vec3 pos_parent;

	for(idx=0; idx<m_nJoints; idx++) {
		vectornf dv_spat(6, m_joints[idx].fs->dv_vec[0]);
		if (m_joints[idx].iParent>=0) {
			m_joints[idx].fs->dv_vec[0] = m_joints[m_joints[idx].iParent].fs->dv_vec[0];
			m_joints[idx].fs->dv_vec[1] = m_joints[m_joints[idx].iParent].fs->dv_vec[1];
			pos_parent = m_joints[m_joints[idx].iParent].body.pos;
		} else {
			m_joints[idx].fs->dv_vec[0] = dw; m_joints[idx].fs->dv_vec[1] = dv; pos_parent = m_posPivot;
		}
		m_joints[idx].fs->dv_vec[1] += m_joints[idx].fs->dv_vec[0] ^ m_joints[idx].body.pos-pos_parent;

		if (m_joints[idx].nPotentialAngles>0) {
			vectornf ddq(m_joints[idx].nPotentialAngles, m_joints[idx].ddq);
			matrixf qinv_sT_Ia(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT_Ia[0]);
			matrixf qinv_sT(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT[0]);
			vectornf Ya(6,m_joints[idx].fs->Ya_vec[0]);
			matrixf s(6,m_joints[idx].nPotentialAngles,0, m_joints[idx].fs->s);
			ddq = qinv_sT_Ia*dv_spat;
			ddq += qinv_sT*Ya;

			for(i=0; i<(m_joints[idx].nPotentialAngles&-bApplyVel); i++) {
				j = m_joints[idx].fs->axidx2qidx[i];
				if (!is_unused(m_joints[idx].dq_req[j])) {
					m_joints[idx].ddq[i] = m_joints[idx].dq_req[j]-m_joints[idx].dq[j];
					MARK_UNUSED m_joints[idx].dq_req[j];
				} else if (m_joints[idx].flags & angle0_limit_reached<<j & 2-m_iSimClass>>31) {
					bHitsLimit = isneg(-m_joints[idx].ddq[i]*m_joints[idx].dq_limit[j]);
					if (bHitsLimit || bLockLimits) 
						m_joints[idx].ddq[i] = 0;	// do not accelerate angle that reached its limit
					if (!bHitsLimit && bLockLimits)
						m_joints[idx].flags &= ~(angle0_limit_reached<<j);
				}
				m_joints[idx].dq[j] += m_joints[idx].ddq[i];
			}
			
			dv_spat += s*ddq;
		}
		m_joints[idx].fs->Ya_vec[0].zero(); m_joints[idx].fs->Ya_vec[1].zero();
	}
}


void CArticulatedEntity::CalcVelocityChanges(float time_interval, const Vec3 &dv,const Vec3 &dw)
{
	int idx,i,j;
	Vec3 pos_parent;

	for(idx=0; idx<m_nJoints; idx++) {
		vectornf dv_spat(6, m_joints[idx].fs->dv_vec[0]);
		if (m_joints[idx].iParent>=0) {
			m_joints[idx].fs->dv_vec[0] = m_joints[m_joints[idx].iParent].fs->dv_vec[0];
			m_joints[idx].fs->dv_vec[1] = m_joints[m_joints[idx].iParent].fs->dv_vec[1];
			pos_parent = m_joints[m_joints[idx].iParent].body.pos;
		} else {
			m_joints[idx].fs->dv_vec[0] = dw; m_joints[idx].fs->dv_vec[1] = dv; pos_parent = m_posPivot;
		}
		m_joints[idx].fs->dv_vec[1] += m_joints[idx].fs->dv_vec[0] ^ m_joints[idx].body.pos-pos_parent;

		if (m_joints[idx].nPotentialAngles>0) {
			vectornf ddq(m_joints[idx].nPotentialAngles, m_joints[idx].ddq);
			matrixf qinv_sT(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT[0]);
			vectornf Ya(6,m_joints[idx].fs->Ya_vec[0]);
			matrixf qinv_sT_Ia(m_joints[idx].nPotentialAngles,6,0, m_joints[idx].fs->qinv_sT_Ia[0]);
			matrixf s(6,m_joints[idx].nPotentialAngles,0, m_joints[idx].fs->s);

			ddq += qinv_sT_Ia*dv_spat;
			ddq += qinv_sT*Ya;

			for(i=0; i<m_joints[idx].nPotentialAngles; i++) {
				j = m_joints[idx].fs->axidx2qidx[i]; 
				if (m_joints[idx].flags & angle0_limit_reached<<j & 2-m_iSimClass>>31 && m_joints[idx].ddq[i]*m_joints[idx].dq_limit[j]>0)
					m_joints[idx].ddq[i] = 0;	// do not accelerate angle that reached its limit
				m_joints[idx].dq[j] += m_joints[idx].ddq[i];
			}
			
			dv_spat += s*ddq;
		}
		m_joints[idx].fs->Ya_vec[0].zero(); m_joints[idx].fs->Ya_vec[1].zero();
	}
}

void CArticulatedEntity::CalcBodiesIinv(int bLockLimits)
{
	int idx,i,nAngles;
	float Iinv_buf[39],aux_buf[39],Mright_buf[39],s_buf[34],qinv_sT_buf[18];
	Vec3 r,pos_parent,axisParent,pivotParent;
	matrixf Iinv_parent(6,6,0,_align16(Iinv_buf)),Iaux(6,6,0,_align16(aux_buf));
	memset(Iinv_buf,0,sizeof(Iinv_buf));
	SetMtxStrided<float,6,1>(m_M0inv*-1.0f, Iinv_parent.data+18);

	for(idx=0; idx<m_nJoints; idx++) {
		if (m_joints[idx].iParent>=0) {
			Iinv_parent.data = m_joints[m_joints[idx].iParent].fs->Iinv[0];
			pos_parent = m_joints[m_joints[idx].iParent].body.pos;
			axisParent = m_joints[m_joints[idx].iParent].fs->axisSingular;
			pivotParent = m_joints[m_joints[idx].iParent].fs->pivot;
		} else {
			Iinv_parent.data = _align16(Iinv_buf);
			pivotParent = pos_parent = m_posPivot;
			axisParent = Vec3(m_bGrounded);
		}
		matrixf Iinv(6,6,0,m_joints[idx].fs->Iinv[0]);
		matrixf s_qinv_sT_Ia(6,6,0, m_joints[idx].fs->s_qinv_sT_Ia[0]);
		r = m_joints[idx].body.pos-pos_parent;
		nAngles = bLockLimits ? m_joints[idx].nActiveAngles : m_joints[idx].nPotentialAngles;

		if (nAngles>0) {
			matrixf Mright(6,6,0,_align16(Mright_buf)),s(6,nAngles,0,m_joints[idx].fs->s);
			matrixf Ia(6,6,0, m_joints[idx].fs->Ia[0]);
			matrixf s_qinv_sT(6,6,0,m_joints[idx].fs->s_qinv_sT[0]);
			matrixf qinv_sT(nAngles,6,0,m_joints[idx].fs->qinv_sT[0]);

			if (nAngles!=m_joints[idx].nPotentialAngles) {
				matrixf sT(6,nAngles,0, m_joints[idx].fs->s_vec[0][0]);
				s.data = s_buf;
				SpatialTranspose(sT,s);
				qinv_sT.data = qinv_sT_buf;
				qinv_sT = matrixf(nAngles,nAngles,0,m_joints[idx].fs->qinv_down)*sT;
			}

			s_qinv_sT = s*qinv_sT; // already with '-' sign
			Iinv = s_qinv_sT*Ia; 
			for(i=0;i<6;i++) Iinv.data[i*7] += 1.0f;
			RightOffsetSpatialMatrix(Iinv, -r);
			s_qinv_sT_Ia = Iinv;

			//Mright = Iinv_parent;
			//LeftOffsetSpatialMatrix(Mright, -r);
			//RightOffsetSpatialMatrix(Mright, r);
			Iinv = (Iaux=Iinv)*Iinv_parent; //Mright;

			if (nAngles == m_joints[idx].nActiveAngles)
				Iinv = (Iaux=Iinv)*matrixf(6,6,0,m_joints[idx].fs->Ia_s_qinv_sT[0]);
			else {
				Mright = Ia*s_qinv_sT;
				for(i=0;i<6;i++) Mright.data[i*7] += 1.0f;
				LeftOffsetSpatialMatrix(Mright, r);
				Iinv = (Iaux=Iinv)*Mright;
			}
			Iinv += s_qinv_sT;
			Vec3 axis0 = nAngles==1 ? m_joints[idx].rotaxes[m_joints[idx].fs->axidx2qidx[0]] : Vec3(0);
			m_joints[idx].fs->axisSingular = fabs(axis0*axisParent)+axisParent.len2()>1.98f ? axis0 : Vec3(0);
			m_joints[idx].fs->pivot = m_joints[idx].quat*m_joints[idx].pivot[1];
		} else {
			Iinv = Iinv_parent;
			LeftOffsetSpatialMatrix(Iinv, -r);
			RightOffsetSpatialMatrix(Iinv, r);
			s_qinv_sT_Ia.identity();
			RightOffsetSpatialMatrix(s_qinv_sT_Ia, -r);
			m_joints[idx].fs->axisSingular = axisParent;
			m_joints[idx].fs->pivot = pivotParent;
		}
	}
}

int CArticulatedEntity::StepJoint(int idx, float time_interval,int &bBounced,int bFlying, int iCaller)
{
	int i,j,idxpivot,ncont,itmax,curidx,sgq,bSelfColl,bFreeFall=isneg(-m_nBodyContacts)^1;
	float qlim[2],dq,curq,e,diff[2],tdiff, minEnergy = m_nBodyContacts>=m_nCollLyingMode ? m_EminLyingMode : m_Emin, maxVel=m_pWorld->m_vars.maxVel;
	Ang3 q;
	Vec3 n,ptsrc,ptdst,axis,axis0,pivot,prevpt;
	quaternionf qrot;
	Matrix33 R;
	geom_contact *pcontacts;
	geom_world_data gwd;
	intersection_params ip;
	box bbox;
	minEnergy *= 0.1f;

	{ WriteLock lock(m_lockJoints); 
		m_joints[idx].prev_q = m_joints[idx].q;
		m_joints[idx].prev_dq = m_joints[idx].dq;
		m_joints[idx].prev_pos = m_joints[idx].body.pos;
		m_joints[idx].prev_qrot = m_joints[idx].body.q;
		if (m_joints[idx].body.v.len2() > sqr(maxVel))
			m_joints[idx].body.P = (m_joints[idx].body.v.normalize()*=maxVel)*m_joints[idx].body.M;
		m_joints[idx].prev_v = m_joints[idx].body.v;
		m_joints[idx].prev_w = m_joints[idx].body.w;
	}
	m_joints[idx].vSleep.zero();
	m_joints[idx].wSleep.zero();

	if (m_iSimTypeCur || !m_bGrounded && !idx) {
		e = (m_joints[idx].body.v.len2() + (m_joints[idx].body.L*m_joints[idx].body.w)*m_joints[idx].body.Minv)*0.5f + m_joints[idx].body.Eunproj;
		m_bAwake += (m_joints[idx].bAwake = isneg(minEnergy-e));
		m_joints[idx].bAwake |= m_bFloating|m_bUsingUnproj|bFreeFall;
		if (!m_joints[idx].bAwake) {
			for(i=m_joints[idx].iParent; i>=0; i=m_joints[i].iParent)
				m_joints[idx].bAwake |= m_joints[i].bAwake;
		}

		if (m_joints[idx].bAwake) {
			m_joints[idx].body.Step(time_interval);
			if ((m_bGrounded|idx)==0)
				m_posNew = (m_posPivot = m_joints[0].body.pos)-m_offsPivot;
			SyncJointWithBody(idx,3);
		}
		R = Matrix33(m_joints[idx].body.q);
		m_joints[idx].I = R*m_joints[idx].body.Ibody*R.T();
		m_joints[idx].dv_body.zero(); m_joints[idx].dw_body.zero();

		m_joints[idx].flags &= ~(angle0_limit_reached*7|joint_dashpot_reached);
		q = m_joints[idx].q + m_joints[idx].qext;
		for(i=0;i<3;i++) if (!(m_joints[idx].flags & (angle0_locked|angle0_gimbal_locked)<<i)) {
			qlim[0] = m_joints[idx].limits[0][i]; qlim[1] = m_joints[idx].limits[1][i];
			if (isneg(qlim[0]-q[i]) + isneg(q[i]-qlim[1]) + isneg(qlim[1]-qlim[0]) < 2) {
				m_joints[idx].flags |= angle0_limit_reached<<i;
				diff[0] = q[i]-qlim[0]; tdiff = diff[0]-sgn(diff[0])*(2*g_PI);
				if (fabsf(tdiff)<fabsf(diff[0])) 
					diff[0] = tdiff;
				diff[1] = q[i]-qlim[1]; tdiff = diff[1]-sgn(diff[1])*(2*g_PI);
				if (fabsf(tdiff)<fabsf(diff[1]))
					diff[1] = tdiff;
				m_joints[idx].dq_limit[i] = diff[isneg(fabsf(diff[1])-fabsf(diff[0]))];
			}	else 
				m_joints[idx].flags |= joint_dashpot_reached & -isneg(min(fabs_tpl(qlim[0]-q[i]),fabs_tpl(qlim[1]-q[i]))-m_joints[idx].qdashpot[i]);
		}

		for(i=m_joints[idx].iStartPart; i<m_joints[idx].iStartPart+m_joints[idx].nParts; i++) {
			(m_infos[i].q = m_joints[idx].quat*m_infos[i].q0).Normalize();
			m_infos[i].pos = m_joints[idx].quat*m_infos[i].pos0+m_joints[idx].body.pos-m_posNew;
		}
	}	
	else 
	{
		for(i=0;i<3;i++) if (!(m_joints[idx].flags & (angle0_locked|angle0_gimbal_locked)<<i)) {
			if (fabsf(m_joints[idx].dq[i])<1E-5f && fabsf(m_joints[idx].q[i])<0.01f) 
				m_joints[idx].dq[i] = 0;
			qlim[0] = m_joints[idx].limits[0][i]; qlim[1] = m_joints[idx].limits[1][i];
			//qlim[1] += 2*g_PI*isneg(qlim[1]-qlim[0]);
			if (m_joints[idx].limits[0][i] > m_joints[idx].limits[1][i]) {
				sgq = sgn(m_joints[idx].q[i]+m_joints[idx].qext[i]);
				qlim[sgq+1>>1] += sgq*2*g_PI;
			}
			m_joints[idx].flags &= ~joint_dashpot_reached;
			dq = m_joints[idx].dq[i]*time_interval;	sgq = sgn(dq);
			curq = m_joints[idx].q[i]+m_joints[idx].qext[i];
			if ((curq + dq - qlim[sgq+1>>1])*sgq > 0)	{	// we'll breach the limit that lies ahead along movement (ignore the other one)
				dq = qlim[sgq+1>>1]-curq-sgq*0.01f;
				//dq = sgnnz(dq)*min(fabsf(dq),0.1f+m_bFeatherstone); // limit angle snapping in full simulation mode
				m_joints[idx].dq_limit[i] = m_joints[idx].dq[i];
				m_joints[idx].flags |= angle0_limit_reached<<i;
				if (m_iSimClass > 2) {
					m_joints[idx].dq_req[i] = -m_joints[idx].dq[i]*m_joints[idx].bounciness[i];
					bBounced++;
				}
			} else if (sgq!=0) {
				m_joints[idx].flags &= ~(angle0_limit_reached<<i);
				// if angle passes through 0 during this step (but isn't too close to 0), make it snap to 0 and also clamp dq if it's small enough
				if (m_joints[idx].ks[i]!=0 && max(fabs_tpl(dq*0.1f)-fabs_tpl(m_joints[idx].q[i]), (m_joints[idx].q[i]+dq)*m_joints[idx].q[i])<0) {
					dq = -m_joints[idx].q[i]; 
					if (fabs_tpl(m_joints[idx].dq[i])<0.1f)
						m_joints[idx].dq[i] = 0;
				}
				m_joints[idx].flags |= joint_dashpot_reached & -isneg(fabs_tpl(curq-qlim[sgq+1>>1])-m_joints[idx].qdashpot[i]);
			}
			m_joints[idx].q[i] += min(fabsf(dq),1.2f)*sgn(dq);
			//while(m_joints[idx].q[i]>g_PI) m_joints[idx].q[i]-=2*g_PI;
			//while(m_joints[idx].q[i]<-g_PI) m_joints[idx].q[i]+=2*g_PI;
		}	else if (m_joints[idx].flags & angle0_locked<<i) {
			m_joints[idx].dq[i] = 0;
			m_joints[idx].q[i] += (m_joints[idx].q0[i]-m_joints[idx].q[i])*isneg(-m_joints[idx].ks[i])*time_interval*10*(1-isneg(m_joints[idx].iParent));
		}

		m_joints[idx].body.Step(time_interval);
		Vec3 v0=m_joints[idx].body.v, w0=m_joints[idx].body.w;
		SyncBodyWithJoint(idx, 1+m_bFeatherstone*2);
		m_joints[idx].dv_body = m_joints[idx].body.v-v0;
		m_joints[idx].dw_body = m_joints[idx].body.w-w0;
		m_joints[idx].ddq.Set(0,0,0);

		if (m_constrInfoFlags & constraint_instant) for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) 
			if (m_constraintMask & getmask(i) && m_pConstraintInfos[i].flags & constraint_instant &&
				  (m_pConstraints[i].flags & (contact_constraint_1dof|contact_angular))==(contact_constraint_1dof|contact_angular) &&
				  m_infos[m_pConstraints[i].ipart[0]].iJoint==idx && FrameOwner(m_pConstraints[i]) && m_joints[idx].iParent>=0) 
			{	// for plane-align (2 dof locked) angular constraints support instant positional enforcement
				QuatT frames[2]; GetContactFrames(i, frames);
				Vec3 vcur=frames[0].q*m_pConstraintInfos[i].qframe_rel[0]*ortx, vdst=frames[1].q*m_pConstraintInfos[i].qframe_rel[1]*m_pConstraints[i].nloc;
				Vec3 proj; for(j=0;j<3;j++) proj[j] = fabs(m_joints[idx].rotaxes[j]*(vcur^vdst));
				int idxsort[3]; idxsort[1] = 3-(idxsort[0]=idxmax3(proj))-(idxsort[2]=idxmin3(proj)); 
				for(int j1=0;j1<3;j1++) if (!(m_joints[idx].flags & angle0_locked<<(j=idxsort[j1]))) {
					Vec3 ax=m_joints[idx].rotaxes[j], ny=ax^vcur, nx=ny^ax;
					dq = atan2(vdst*ny, vdst*nx);
					Vec3 vcur0=vcur.GetRotated(ax,dq), vcur1=vcur.GetRotated(ax,gf_PI+dq);
					sgq = 1^isneg(dq += gf_PI*isneg(vcur0*vdst-vcur1*vdst));
					curq = m_joints[idx].q[j]+m_joints[idx].qext[j] + dq;
					curq = minmax(m_joints[idx].limits[sgq][j], curq, 1-sgq);
					vcur = vcur.GetRotated(ax, curq-m_joints[idx].q[j]-m_joints[idx].qext[j]);
					m_joints[idx].q[j] = curq-m_joints[idx].qext[j];
					SyncBodyWithJoint(idx,1);
				}
			}

		e = (m_joints[idx].body.v.len2() + (m_joints[idx].body.L*m_joints[idx].body.w)*m_joints[idx].body.Minv)*0.5f;
		m_bAwake += (m_joints[idx].bAwake = isneg(minEnergy-e));
		m_joints[idx].bAwake |= m_bFloating;

		if (!m_joints[idx].bAwake && !m_bFeatherstone) {
			for(i=m_joints[idx].iParent; i>=0; i=m_joints[i].iParent)
				m_joints[idx].bAwake |= m_joints[i].bAwake;
			if (!m_joints[idx].bAwake) {
				m_joints[idx].q = m_joints[idx].prev_q;
				m_joints[idx].dq.Set(0,0,0);
				SyncBodyWithJoint(idx);
				m_joints[idx].body.P.Set(0,0,0); m_joints[idx].body.L.Set(0,0,0);
				m_joints[idx].body.v.Set(0,0,0); m_joints[idx].body.w.Set(0,0,0);
				if (idx==0) {
					m_posNew = m_prev_pos; m_posPivot = m_posNew+m_offsPivot;
					m_body.v.zero();
				}
			}
		}

		CheckForGimbalLock(idx);
	}

	pivot = m_joints[idx].iParent<0 ? m_posPivot : 
		m_joints[m_joints[idx].iParent].body.pos + m_joints[m_joints[idx].iParent].quat*m_joints[idx].pivot[0];

	if (m_bCheckCollisions) {// && m_joints[idx].bAwake) {
		// check for new contacts; unproject if necessary; register new contacts
		Vec3 sweep(0);
		if (/*m_iSimTypeCur==0 &&*/ bFlying) {
			ip.iUnprojectionMode = 0; ip.time_interval = time_interval*1.6f;
			gwd.v = m_body.v; gwd.w.zero();	sweep = m_body.v*time_interval;
			n = m_BBox[1]-m_BBox[0];
			ip.maxUnproj = max(max(n.x,n.y),n.z);
			ip.vrel_min = ip.maxUnproj/ip.time_interval*0.1f;
		} else /*if (m_iSimTypeCur==0 && m_joints[idx].nChildren==0 && (m_joints[idx].body.w.len2()>sqr(6) || m_joints[idx].body.v.len2()>sqr(4)) &&
			m_joints[idx].body.Minv>m_body.Minv*10.0f) 
		{
			ip.iUnprojectionMode = 1; ip.centerOfRotation = pivot;
			ip.time_interval = 0.6f;
			if (!GetUnprojAxis(idx,ip.axisOfRotation)) {
				ip.iUnprojectionMode = 0; ip.vrel_min = 1E10;
			}
		}	else*/ {
			ip.iUnprojectionMode = 0; ip.vrel_min = 1E10; ip.bNoAreaContacts = true;
			for(int ipart=m_joints[idx].iStartPart; ipart<m_joints[idx].iStartPart+m_joints[idx].nParts; ipart++)
				if (m_parts[ipart].pPhysGeomProxy->pGeom->GetType()==GEOM_BOX) {
					ip.bNoAreaContacts = false; break;
				}
		}
		idxpivot = idx;	itmax = -1; 
		ip.ptOutsidePivot[0] = m_joints[idx].iParent>=0 ? pivot : m_joints[idx].body.pos;
		ip.bNoIntersection = 1;
		ip.maxSurfaceGapAngle = DEG2RAD(4.0f);
		ncont = CheckForNewContacts(&gwd,&ip, itmax, sweep, m_joints[idx].iStartPart,m_joints[idx].nParts, nullptr, iCaller);
		pcontacts = ip.pGlobalContacts;
		/*if (ncont>0 && itmax<0 && ip.iUnprojectionMode==1) {
			// find parent that has good free axes
			axis0 = (pcontacts[0].center-ip.centerOfRotation^pcontacts[0].dir).normalized();
			for(i=3,idxpivot=m_joints[idxpivot].iParent; i>0 && idxpivot>0; i--,idxpivot=m_joints[idxpivot].iParent) {
				for(i=0,axis=axis0;i<3;i++) if (m_joints[idxpivot].flags & (angle0_limit_reached|angle0_locked|angle0_gimbal_locked)<<i)
					axis -= m_joints[idxpivot].rotaxes[i]*(axis*m_joints[idx].rotaxes[i]);
				if (axis.len2()>0.5f*0.5f) {
					ip.centerOfRotation = m_joints[idxpivot].body.pos + m_joints[idxpivot].quat*m_joints[idxpivot].pivot[1];
					ip.axisOfRotation = GetUnprojAxis(idxpivot);
					break;
				}
			}
			if (i*idxpivot>0)
				ncont = CheckForNewContacts(&gwd,&ip, itmax, m_joints[idx].iStartPart,m_joints[idx].nParts);
		}*/
		if (itmax>=0) {
			if (pcontacts[itmax].iUnprojMode) {
				qrot.SetRotationAA(pcontacts[itmax].t, pcontacts[itmax].dir);
				for(i=idxpivot;i<=idx;i++) { // note: update of parent will possibly invalidate its registered contacts, is it crucial?
					m_joints[i].body.q = qrot*m_joints[i].body.q; 
					m_joints[i].body.pos = ip.centerOfRotation + qrot*(m_joints[i].body.pos-ip.centerOfRotation);
					SyncJointWithBody(i,3);
				}
				if (m_bFeatherstone) {
					for(i=0;i<3;i++) 
					if (!(m_joints[idxpivot].flags & (angle0_locked|angle0_gimbal_locked|angle0_limit_reached)<<i) && 
								m_joints[idxpivot].rotaxes[i]*pcontacts[itmax].dir*sgnnz(m_joints[idxpivot].dq[i])<-0.5f)
						m_joints[idxpivot].dq[i] = 0;
				}
			} else {
				axis = pcontacts[itmax].dir*pcontacts[itmax].t;
				for(i=0;i<m_nJoints;i++) m_joints[i].body.pos += axis;
				m_posPivot += axis; m_posNew += axis;	m_pos += axis;
			}
		}

		{	// register new contacts
			for(j=m_joints[idx].iStartPart+m_joints[idx].nParts-1; j>=m_joints[idx].iStartPart; j--) {
				m_parts[j].pPhysGeomProxy->pGeom->GetBBox(&bbox);
				bbox.Basis *= Matrix33(!(m_qNew*m_infos[j].q));
				Vec3 sz = (bbox.size*bbox.Basis.Fabs())*m_parts[j].scale;
				m_infos[j].BBox[0]=m_infos[j].BBox[1] = m_posNew + m_qNew*(m_infos[j].pos + m_infos[j].q*bbox.center*m_parts[j].scale);
				m_infos[j].BBox[0] -= sz; m_infos[j].BBox[1] += sz;
			}	j++;
			if (m_nParts>3)
				m_parts[j].minContactDist = max(max(bbox.size.x,bbox.size.y),bbox.size.z)*m_parts[j].scale*0.2f;

			for(i=0;i<ncont;i++) {
				if (!(bSelfColl = g_CurColliders[i]==this))
					m_maxPenetrationCur = max(m_maxPenetrationCur,(float)pcontacts[i].t);
				if ((pcontacts[i].iNode[0]&pcontacts[i].iNode[1])>=0) { 
					// at least one geometry is a mesh; use intersection points rather than areas
					e = sqr(m_parts[m_joints[idx].iStartPart].minContactDist)*(1.0f+0.25f*bSelfColl);
					j = 0;
					if (idx>0)
						for(;j<pcontacts[i].nborderpt && (pcontacts[i].ptborder[j]-pcontacts[i].ptborder[0]).len2()<e; j++);
					if (j==pcontacts[i].nborderpt/*idx>0*/) // register border center only for penetration contacts in marginal joints
						RegisterContactPoint(i, pcontacts[i].center, pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
							pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, pcontacts[i].t, iCaller);
					else {
						for(j=0,prevpt.zero(); j<pcontacts[i].nborderpt; j++)	if ((pcontacts[i].ptborder[j]-prevpt).len2()>e) {
							RegisterContactPoint(i, pcontacts[i].ptborder[j], pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
								pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|(contact_last&j-1>>31), pcontacts[i].t, iCaller);
							prevpt = pcontacts[i].ptborder[j];
						}
					}
				} else if (sqr_signed((pcontacts[i].pt-ip.ptOutsidePivot[0])*pcontacts[i].n) > (pcontacts[i].pt-ip.ptOutsidePivot[0]).len2()*-0.25f) // 120 degrees culling
					if (pcontacts[i].parea) { 
						for(j=0;j<pcontacts[i].parea->npt;j++)
							RegisterContactPoint(i, pcontacts[i].parea->pt[j], pcontacts, pcontacts[i].parea->piPrim[0][j], pcontacts[i].parea->piFeature[0][j], 
								pcontacts[i].parea->piPrim[1][j],pcontacts[i].parea->piFeature[1][j], contact_new|contact_area|contact_inexact|(contact_last&j-1>>31), pcontacts[i].t,iCaller);
					} else
						RegisterContactPoint(i, pcontacts[i].pt, pcontacts, pcontacts[i].iPrim[0],pcontacts[i].iFeature[0], 
							pcontacts[i].iPrim[1],pcontacts[i].iFeature[1], contact_new|contact_last, pcontacts[i].t,iCaller);
			}
		}
		CleanupAfterContactsCheck(iCaller);
	}

	// iterate children
	for(i=0,curidx=idx+1; i<m_joints[idx].nChildren; i++)
		curidx = StepJoint(curidx, time_interval,bBounced,bFlying, iCaller);
	return curidx;
}

void CArticulatedEntity::StepFeatherstone(float time_interval, int bBounced, Matrix33 &M0host, float *Zabuf)
{
	int i;
	Vec3 Y_vec[2]; vectornf Y(6,Y_vec[0]);
	float Iabuf[39]; matrixf Ia(6,6,0,_align16(Iabuf)); 
	Vec3 Za_vec[2]; vectornf Za(6,Za_vec[0]);
	Vec3 dv,dw;

	if (m_bCheckCollisions)
		VerifyExistingContacts(m_pWorld->m_vars.maxContactGap);

	if (bBounced && m_bIaReady) {	// apply bounce impulses at joints that reached their limits
		bBounced=0; CollectPendingImpulses(0,bBounced);
		if (bBounced) {
			PropagateImpulses(dv = m_M0inv*-m_Ya_vec[0]);
			if (!m_bGrounded)
				m_body.v += dv;
		}
	}

	CalcBodyIa(0, Ia);
	m_M0inv = GetMtxStrided<float,6,1>(Ia.data+3);
	if (m_bGrounded) {
		if (M0host.IsZero())
			m_M0inv.SetZero();
		else {
			//(m_M0inv += M0host.invert()).invert();
			M0host.Invert(); (m_M0inv+=M0host).Invert();
		}
	} else 
		m_M0inv.Invert();

	m_bIaReady = 1;
	CalcBodiesIinv(!m_bFeatherstone);

	for(i=0;i<m_nJoints;i++) { // apply external impulses
		m_joints[i].Pext += m_joints[i].Pimpact;	m_joints[i].Lext += m_joints[i].Limpact;
		m_joints[i].Pimpact.zero();	m_joints[i].Limpact.zero();
	}
	CollectPendingImpulses(0,bBounced);
	Za.zero(); CalcBodyZa(0, time_interval,Za);

	dv = m_M0inv*(-Za_vec[0]-m_Ya_vec[0]); dw.zero(); 
	if (m_bGrounded) {
		dv += m_acc*time_interval; dw += m_wacc*time_interval;
	}
	CalcVelocityChanges(time_interval, dv,dw);
	if (!m_bGrounded)	{
		m_body.v += dv; 
		m_body.w.zero();
	}

	for(i=0; i<m_nJoints; i++)
		SyncBodyWithJoint(i,3);	// synchronize body geometry as well as dynamics to accomodate potential changes in joint limits

	if (m_bGrounded && m_pHost && m_bExertImpulse) {
		pe_action_impulse ai;
		ai.impulse = -Za_vec[0]-m_Ya_vec[0];
		ai.point = m_posPivot;
		m_pHost->Action(&ai,1);
		pe_player_dynamics pd;
		pd.timeImpulseRecover = 4;
		m_pHost->SetParams(&pd);
	}

	int nPhysColl = 0;
	for(i=0;i<m_nColliders;i++)
		nPhysColl += m_pColliders[i]->m_iSimClass<3 && m_pColliders[i]->GetMassInv();
	if (m_nColliders && !nPhysColl && m_iSimClass>2) {
		float Ebefore=CalcEnergy(time_interval),Eafter=0,damping=GetDamping(time_interval);
		for(i=0;i<m_nColliders;i++)	if (m_pColliders[i]!=this) {
			damping = min(damping, m_pColliders[i]->GetDamping(time_interval));
			Ebefore += m_pColliders[i]->CalcEnergy(time_interval);
		}
		InitContactSolver(time_interval);
		RegisterContacts(time_interval, m_pWorld->m_vars.nMaxPlaneContacts);
		for(i=0;i<m_nColliders;i++)	if (m_pColliders[i]!=this)
			m_pColliders[i]->RegisterContacts(time_interval, m_pWorld->m_vars.nMaxPlaneContacts);
		entity_contact **pContacts; int nContacts=0;
		InvokeContactSolver(time_interval,&m_pWorld->m_vars,Ebefore, pContacts,nContacts);
		for(i=0; i<nContacts; i++) for(int j=0;j<2;j++) 
			if (pContacts[i]->pent[j]->m_parts[pContacts[i]->ipart[j]].flags & geom_monitor_contacts)
				pContacts[i]->pent[j]->OnContactResolved(pContacts[i],j,-1);
		Eafter = CalcEnergy(0.0f);
		for(i=0;i<m_nColliders;i++)	if (m_pColliders[i]!=this)
			Eafter += m_pColliders[i]->CalcEnergy(0.0f);
		if (Eafter>Ebefore)
			damping = sqrt_tpl(Ebefore/Eafter);
		Update(time_interval,damping);
		for(i=0;i<m_nColliders;i++)	if (m_pColliders[i]!=this)
			m_pColliders[i]->Update(time_interval,damping);
	}
	if (Zabuf)
		memcpy(Zabuf, Za_vec, sizeof(Za_vec));
}

void CArticulatedEntity::StepBack(float time_interval)
{
	if (m_simTime>0) {
		Matrix33 R;
		m_posNew = m_prev_pos;
		m_posPivot = m_posNew+m_offsPivot;
		m_body.P = (m_body.v=m_prev_vel)*m_body.M;

		for(int i=0;i<m_nJoints;i++) {
			m_joints[i].q = m_joints[i].prev_q;
			m_joints[i].dq = m_joints[i].prev_dq;
			m_joints[i].body.pos = m_joints[i].prev_pos;
			m_joints[i].body.q = m_joints[i].prev_qrot;
			m_joints[i].body.v = m_joints[i].prev_v;
			m_joints[i].body.w = m_joints[i].prev_w;

			m_joints[i].quat = m_joints[i].body.q*m_joints[i].body.qfb;
			R = Matrix33(m_joints[i].body.q);
			m_joints[i].I = R*m_joints[i].body.Ibody*R.T();
			m_joints[i].body.Iinv = R*m_joints[i].body.Ibody_inv*R.T();
			m_joints[i].body.P = m_joints[i].body.v*m_joints[i].body.M; 
			m_joints[i].body.L = m_joints[i].I*m_joints[i].body.w;

			UpdateJointRotationAxes(i);
			for(int j=m_joints[i].iStartPart; j<m_joints[i].iStartPart+m_joints[i].nParts; j++) {
				m_infos[j].q = m_joints[i].quat*m_infos[j].q0;
				m_infos[j].pos = m_joints[i].quat*m_infos[j].pos0+m_joints[i].body.pos-m_posNew;
			}
			//SyncBodyWithJoint(i);
		}
		m_bSteppedBack = 1;

		//UpdateContactsAfterStepBack(time_interval);
		ComputeBBox(m_BBoxNew);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
	}
}


void CArticulatedEntity::JointListUpdated()
{
	int i,j;
	for(i=0;i<m_nParts;i++) {
		for(j=0;j<m_nJoints && m_joints[j].idbody!=m_infos[i].idbody;j++);
		m_infos[i].iJoint = j;
	}
	for(i=0;i<m_nJoints;i++)
		m_joints[i].iLevel = m_joints[i].iParent>=0 ? m_joints[m_joints[i].iParent].iLevel+1 : 0;
}


void CArticulatedEntity::BreakableConstraintsUpdated()
{
	CRigidEntity::BreakableConstraintsUpdated();
	for(int i=0;i<m_nParts;i++)
		m_parts[i].flags |= geom_monitor_contacts;
}

void CArticulatedEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	if (!pRenderer) return;
	int idsel = -1;
	if (flags & 1<<28) {
		idsel = flags>>16 & 0xFFF;
		flags &= 0xF000FFFF;
	}

	CRigidEntity::DrawHelperInformation(pRenderer, flags);

	if(flags&0x10){
		for(int i=0;i<m_nJoints;i++) if ((m_joints[i].idbody | idsel>>31)==idsel) {
			quaternionf q_parent;
			if (m_joints[i].iParent>=0) q_parent = m_joints[m_joints[i].iParent].quat;
			else q_parent = m_qNew;
			quaternionf j_q = q_parent*m_joints[i].quat0;
			int ipart = m_joints[i].iStartPart;
			box bbox; m_parts[ipart].pPhysGeom->pGeom->GetBBox(&bbox);
			Vec3 j_pos = m_pos + m_qrot*(m_parts[ipart].pos-m_joints[i].quat*m_infos[ipart].pos0) + m_joints[i].quat*m_joints[i].pivot[1];
			Vec3 axes[3] = {j_q.GetColumn0(), j_q.GetColumn1(),j_q.GetColumn2()};
			pRenderer->DrawFrame(j_pos, axes, max(max(bbox.size.x,bbox.size.y),bbox.size.z)*m_parts[ipart].scale, m_joints[i].limits, m_joints[i].flags ^ all_angles_locked);
		}
	}
}

int CArticulatedEntity::GetStateSnapshot(CStream &stm,float time_back,int flags)
{
	stm.WriteNumberInBits(SNAPSHOT_VERSION,4);
	stm.Write((unsigned char)m_nJoints);
	stm.Write(m_pos);
	stm.Write(m_body.v);
	stm.Write(m_bAwake!=0);
	int i = m_iSimClass-1;
	stm.WriteNumberInBits(i,2);

#ifdef DEBUG_BONES_SYNC
	stm.WriteBits((uint8*)&m_qrot,sizeof(m_qrot)*8);
	for(i=0;i<m_nParts;i++) {
		stm.Write(m_parts[i].pos);
		stm.WriteBits((uint8*)&m_parts[i].q,sizeof(m_parts[i].q)*8);
	}
	return 1;
#endif

	if (m_bPartPosForced) for(i=0;i<m_nJoints;i++) {
		int j = m_joints[i].iStartPart;
		m_joints[i].quat = m_qrot*m_parts[j].q*!m_infos[j].q0;
		m_joints[i].body.pos = m_parts[j].pos-m_joints[i].quat*m_infos[j].pos0+m_pos;
		m_joints[i].body.q = m_joints[i].quat*!m_joints[i].body.qfb;
	}

	for(i=0;i<m_nJoints;i++) {
		stm.Write(m_joints[i].bAwake!=0);
		stm.Write((Vec3)m_joints[i].q);
		stm.Write(m_joints[i].dq);

		stm.Write((Vec3)m_joints[i].qext);
		stm.Write(m_joints[i].body.pos);
		stm.WriteBits((uint8*)&m_joints[i].body.q,sizeof(m_joints[i].body.q)*8);
		if (m_joints[i].bQuat0Changed) {
			stm.Write(true);
			stm.WriteBits((uint8*)&m_joints[i].quat0, sizeof(m_joints[i].quat0)*8);
		} else stm.Write(false);
		if (m_joints[i].body.P.len2()+m_joints[i].body.L.len2()>0) {
			stm.Write(true); 
			stm.Write(m_joints[i].body.P);
			stm.Write(m_joints[i].body.L);
		}	else stm.Write(false);
	}

	WriteContacts(stm,flags);

	return 1;
}

int CArticulatedEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	int i=0,j,ver=0; 
	bool bnz;
	Matrix33 R;
	unsigned char nJoints;

	stm.ReadNumberInBits(ver,4);
	if (ver!=SNAPSHOT_VERSION)
		return 0;
	stm.Read(nJoints);
	if (nJoints!=m_nJoints)
		return 0;

	if (!(flags & ssf_no_update)) {
		stm.Read(m_posNew);
		stm.Read(m_body.v);
		stm.Read(bnz); m_bAwake = bnz ? 1:0;
		stm.ReadNumberInBits(i, 2); m_iSimClass=i+1;

#ifdef DEBUG_BONES_SYNC
stm.ReadBits((uint8*)&m_qrot,sizeof(m_qrot)*8);
for(i=0;i<m_nParts;i++) {
	stm.Read(m_parts[i].pos);
	stm.ReadBits((uint8*)&m_parts[i].q,sizeof(m_parts[i].q)*8);
}
#else
		for(i=0;i<m_nJoints;i++) {
			stm.Read(bnz);
			m_joints[i].bAwake = bnz ? 1:0;
			stm.Read(m_joints[i].q);
			stm.Read(m_joints[i].dq);
			//SyncBodyWithJoint(i);

			stm.Read(m_joints[i].qext);
			stm.Read(m_joints[i].body.pos);
			stm.ReadBits((uint8*)&m_joints[i].body.q,sizeof(m_joints[i].body.q)*8);
			stm.Read(bnz); if (bnz)	{
				stm.ReadBits((uint8*)&m_joints[i].quat0,sizeof(m_joints[i].quat0)*8);
				m_joints[i].bQuat0Changed = 1;
			} else m_joints[i].bQuat0Changed = 0;
			stm.Read(bnz); if (bnz)	{
				stm.Read(m_joints[i].body.P);
				stm.Read(m_joints[i].body.L);
			}	else {
				m_joints[i].body.P.zero();
				m_joints[i].body.L.zero();
			}
			m_body.UpdateState();

			UpdateJointRotationAxes(i);
			m_joints[i].quat = m_joints[i].body.q*m_joints[i].body.qfb;
			//m_joints[i].body.q.getmatrix(R); //Q2M_IVO
			R = Matrix33(m_joints[i].body.q);
			m_joints[i].I = R*m_joints[i].body.Ibody*R.T();

			for(j=m_joints[i].iStartPart; j<m_joints[i].iStartPart+m_joints[i].nParts; j++) {
				m_infos[j].q = m_joints[i].quat*m_infos[j].q0;
				m_infos[j].pos = m_joints[i].quat*m_infos[j].pos0+m_joints[i].body.pos-m_posNew;
			}
		}
#endif

		m_bUpdateBodies = 0;
		ComputeBBox(m_BBoxNew);
		UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
	} else {
		masktype dummy;
		stm.Seek(stm.GetReadPos()+2*sizeof(Vec3)*8+3);
#ifdef DEBUG_BONES_SYNC
stm.Seek(stm.GetReadPos()+sizeof(quaternionf)*8+m_nParts*(sizeof(quaternionf)+sizeof(Vec3))*8);
#else
		for(i=0;i<m_nJoints;i++) {
			stm.Seek(stm.GetReadPos()+1+4*sizeof(Vec3)*8+sizeof(quaternionf)*8);
			stm.Read(bnz); if (bnz) 
				stm.Seek(stm.GetReadPos()+sizeof(quaternionf)*8);
			stm.Read(bnz); if (bnz) 
				stm.Seek(stm.GetReadPos()+2*sizeof(Vec3)*8);
			ReadPacked(stm,dummy);
		}
#endif
	}

#ifndef DEBUG_BONES_SYNC
	ReadContacts(stm,flags);
#endif

	return 1;
}


int CArticulatedEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	if (ser.GetSerializationTarget() == eST_Network)
	{	
		bool awake = m_bAwake!=0;

		ser.Value("awake", awake, 'bool');
		ser.Value("pos", m_pos, 'wrld');
		ser.Value("vel", m_body.v, 'pAVl');

		//int nJoints=m_nJoints;
		//ser.Value("numjoints", nJoints, 'ui8');

		// for network, we serialize the first bone only (root) as serializing the whole body would be expensive
		//for(int i=0;i<nJoints;i++)
		int i=0;
		// JAT - BeginOptionalGroup() will return true in the VM phase of serialisation 
		// JAT - ALL possible network serialisations must be run to build up the correct packet format
		// JAT - must call EndGroup() during the VM phase of serialisation
		if(ser.BeginOptionalGroup("joint", m_nJoints != 0))
		{
			//ser.BeginGroup("joint");
			if (m_nJoints != 0)
			{
				ser.Value("q", m_joints[i].q, 'pHAn');
				ser.Value("dq", m_joints[i].dq, 'pRAV');
				ser.Value("qext", m_joints[i].qext, 'pHAn');
				bool quatchanged=m_joints[i].bQuat0Changed!=0;
				ser.Value("q0changed", quatchanged, 'bool');
				ser.Value("quat0", m_joints[i].quat0, 'ori1');
			}
			else
			{
				Ang3 dummyQ(ZERO);
				Vec3 dummyDQ(ZERO);
				Ang3 dummyQEXT(ZERO);
				bool dummyQUATCHANGED(false);
				quaternionf dummyQUAT0(IDENTITY);
				ser.Value("q", dummyQ, 'pHAn');
				ser.Value("dq", dummyDQ, 'pRAV');
				ser.Value("qext", dummyQEXT, 'pHAn');
				ser.Value("q0changed", dummyQUATCHANGED, 'bool');
				ser.Value("quat0", dummyQUAT0, 'ori1');
			}
			//ser.EndGroup();
			ser.EndGroup();
		}
		//ser.Value("offsPivot", m_offsPivot, 'wrl2');
	}	else {
		bool awake = (m_bAwake != 0);
		ser.Value("awake", awake, 'bool');
		ser.Value("pos", m_pos, 'wrld'); 
		ser.Value("vel", m_body.v, 'pAVl');
		ser.Value("numjoints", m_nJoints);
		ser.Value("savevel", m_saveVel);
		for(int i=0;i<m_nJoints;i++) {
			ser.BeginGroup("joint");
			
			ser.Value("q", m_joints[i].q);
			if (m_bAwake)
				ser.Value("dq", m_joints[i].dq);
			ser.Value("qext", m_joints[i].qext);
			ser.Value("q0changed", m_joints[i].bQuat0Changed);
			if (m_joints[i].bQuat0Changed)
				ser.Value("quat0", m_joints[i].quat0);
			if (m_saveVel)
				ser.Value("v", m_joints[i].body.v);

			ser.EndGroup();
		}
		ser.Value("offsPivot", m_offsPivot);
		if (flags & 16)
			WriteContacts(ser);
		WriteConstraints(ser, constraint_rope, 1);
	}
		
 	return 1;
}

int CArticulatedEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	if (ser.GetSerializationTarget() == eST_Network)
	{
		pe_action_awake aa;
		{ WriteLock locku(m_lockUpdate);

		bool awake=false;
		ser.Value("awake", awake, 'bool');
		ser.Value("pos", m_pos, 'wrld');
		ser.Value("vel", m_body.v, 'pAVl');

		//int nJoints;
		//ser.Value("numjoints", nJoints, 'ui8');

		aa.bAwake=awake?1:0;
		//m_iSimClass = 1+m_bAwake;

		//for(int i=0;i<nJoints;i++)
		int i=0;
		if(ser.BeginOptionalGroup("joint", m_nJoints != 0))
		{
			//ser.BeginGroup("joint");
			ae_joint dummy,*joints = m_nJoints>0 ? m_joints:&dummy;

			ser.Value("q", joints[i].q, 'pHAn');
			ser.Value("dq", joints[i].dq, 'pRAV');
			ser.Value("qext", joints[i].qext, 'pHAn');

			bool quatchanged;
			ser.Value("q0changed", quatchanged, 'bool');
			ser.Value("quat0", joints[i].quat0, 'ori1');

			joints[i].prev_q = joints[i].q;
			joints[i].bQuat0Changed=quatchanged?1:0;
			if (!m_bAwake)
				joints[i].dq.zero();

			//ser.EndGroup();
			ser.EndGroup();
		}
		//ser.Value("offsPivot", m_offsPivot, 'wrl2');

		m_posPivot = m_pos+m_offsPivot;
		m_posNew = m_pos;

		if (m_pWorld)
		{
			for(i=0;i<m_nJoints;i++)
				SyncBodyWithJoint(i);

			ComputeBBox(m_BBox);
			m_pos = m_pNewCoords->pos; m_qrot = m_pNewCoords->q;
			for(i=0; i<m_nParts; i++) {
				m_parts[i].pos = m_parts[i].pNewCoords->pos; m_parts[i].q = m_parts[i].pNewCoords->q;
				m_parts[i].BBox[0] = m_parts[i].pNewCoords->BBox[0]; m_parts[i].BBox[1] = m_parts[i].pNewCoords->BBox[1];
			}
			m_BBox[0]=m_BBoxNew[0]; m_BBox[1]=m_BBoxNew[1];
			//UpdatePosition(m_pWorld->RepositionEntity(this,3,m_BBoxNew));	// cannot risk concurrent mt calls of that

			m_iLastLog = m_pWorld->m_iLastLogPump;
			m_nEvents = 0;
		}
		} // m_lockUpdate

		if (m_pWorld) {
			Action(&aa,0); // will get queued if necessary
			if (m_bProcessed>=PENT_QUEUED) {
				pe_params_articulated_body pab;
				pab.bGrounded = 100; m_bIgnoreCommands = 1;
				SetParams(&pab,0);
			}
		}
	}
	else {
		bool awake = false;
		ser.Value("awake", awake, 'bool');
		m_bAwake = awake? 1 : 0;
		ser.Value("pos", m_pos, 'wrld'); 
		//if (m_bAwake)
			ser.Value("vel", m_body.v, 'pAVl');
		//else m_body.v.zero();

		int nJoints;
		ser.Value("numjoints", nJoints);
		if (nJoints!=m_nJoints)
			return 0;
		if ((unsigned int)m_iSimClass<7u)
			m_iSimClass = 1+m_bAwake;
		ser.Value("savevel", m_saveVel);

		for(int i=0;i<m_nJoints;i++) {
			ser.BeginGroup( "joint" );

			ser.Value("q", m_joints[i].q);
			if (m_bAwake)
				ser.Value("dq", m_joints[i].dq);
			else m_joints[i].dq.zero();
			ser.Value("qext", m_joints[i].qext);
			ser.Value("q0changed", m_joints[i].bQuat0Changed);
			if (m_joints[i].bQuat0Changed)
				ser.Value("quat0", m_joints[i].quat0);
			if (m_saveVel)
				ser.Value("v", m_joints[i].prev_v);
			m_joints[i].prev_q = m_joints[i].q;

			ser.EndGroup();
		}
		ser.Value("offsPivot", m_offsPivot);
		m_posPivot = m_pos+m_offsPivot;
		m_posNew = m_pos;
		for(int i=0;i<m_nJoints;i++)
			SyncBodyWithJoint(i);
		if (m_saveVel) for(int i=0;i<m_nJoints;i++)
			m_joints[i].body.P = (m_joints[i].body.v=m_joints[i].prev_v)*m_joints[i].body.M;
		ComputeBBox(m_BBoxNew);
		if (flags & 16)
			ReadContacts(ser);
		if ((unsigned int)m_iSimClass>=7u) 
			return 1;
		UpdatePosition(m_pWorld->RepositionEntity(this,3,m_BBoxNew));
		ReadConstraints(ser);

		m_iLastLog = m_pWorld->m_iLastLogPump;
		m_nEvents = 0;

		if (m_bProcessed>=PENT_QUEUED) {
			pe_params_articulated_body pab;
			pab.bGrounded = 100; m_bIgnoreCommands = 1;
			SetParams(&pab,0);
		}
	}

	return 1;
}


int CollectPendingImpulses(strided_pointer<ArticulatedBody> joints, int idx)
{
	int i,curidx,newidx;
	for(i=0,curidx=idx+1; i<joints[idx].nChildren; i++) {
		newidx = CollectPendingImpulses(joints, curidx);
		if (joints[curidx].fs->Ya_vec[0].len2()+joints[curidx].fs->Ya_vec[1].len2()) {
			matrixf Ia_s_qinv_sT(6,6,0,joints[curidx].fs->Ia_s_qinv_sT[0]);
			vectornf Ya(6,joints[idx].fs->Ya_vec[0]);
			vectornf Ya_child(6,joints[curidx].fs->Ya_vec[0]);
			Ya += Ia_s_qinv_sT*Ya_child;
		}
		curidx = newidx;
	}
	return curidx;
}

void PropagateImpulses(strided_pointer<ArticulatedBody> joints, const Vec3 &dv)
{
	for(int idx=0,iparent; idx<=joints[0].nChildrenTree; idx++) {
		vectornf dv_spat(6, joints[idx].fs->dv_vec[0]);
		if ((iparent=joints[idx].iParent)>=0) {
			joints[idx].fs->dv_vec[0] = joints[iparent].fs->dv_vec[0];
			joints[idx].fs->dv_vec[1] = joints[iparent].fs->dv_vec[1] + (joints[idx].fs->dv_vec[0] ^ joints[idx].body.pos-joints[iparent].body.pos);
		} else {
			joints[idx].fs->dv_vec[0].zero(); joints[idx].fs->dv_vec[1] = dv;
		}
		if (joints[idx].nPotentialAngles>0) {
			float ddqBuf[3];
			vectornf ddq(joints[idx].nPotentialAngles, ddqBuf);
			matrixf qinv_sT_Ia(joints[idx].nPotentialAngles,6,0, joints[idx].fs->qinv_sT_Ia[0]);
			matrixf qinv_sT(joints[idx].nPotentialAngles,6,0, joints[idx].fs->qinv_sT[0]);
			vectornf Ya(6,joints[idx].fs->Ya_vec[0]);
			matrixf s(6,joints[idx].nPotentialAngles,0, joints[idx].fs->s);
			ddq = qinv_sT_Ia*dv_spat;
			ddq += qinv_sT*Ya;
			dv_spat += s*ddq;
		}
		joints[idx].fs->Ya_vec[0].zero(); joints[idx].fs->Ya_vec[1].zero();
	}
}

void ArticulatedBody::GetContactMatrix(const Vec3& r, Matrix33 &K)
{
	Matrix33 rx; crossproduct_matrix(r,rx);
	Matrix33 Pw=GetMtxStrided<float,6,1>(fs->Iinv[0]), Lw=GetMtxStrided<float,6,1>(fs->Iinv[0]+3),
					 Pv=GetMtxStrided<float,6,1>(fs->Iinv[3]), Lv=GetMtxStrided<float,6,1>(fs->Iinv[3]+3);
	K -= Pv+Lv*rx-rx*(Pw+Lw*rx);
	Matrix33 mtxsing; 
	Vec3 dirsing = r-fs->pivot;
	if (dirsing*(K*dirsing) < dirsing.len2()*body.Minv*0.001f) { // check if the direction towards pivot is singular for movement
		dirsing.NormalizeFast();
		K += dotproduct_matrix(dirsing,dirsing*body.Minv*0.01f,mtxsing);
	}
	if (fs->axisSingular*(K*fs->axisSingular) < body.Minv*0.001f)	// check if the principal rotation axis is singular for movement
		K += dotproduct_matrix(fs->axisSingular,fs->axisSingular*body.Minv*0.01f,mtxsing);
}

void ArticulatedBody::GetContactMatrixRot(Matrix33 &K, ArticulatedBody *buddy)
{
	Matrix33 L2w = GetMtxStrided<float,6,1>(fs->Iinv[0]+3);
	K -= L2w;
	strided_pointer<ArticulatedBody> joints0 = strided_pointer<ArticulatedBody>(this,fs->jointSize), 
																	 joints1 = strided_pointer<ArticulatedBody>(buddy,fs->jointSize);
	if (&joints0[fs->iparent]==buddy) { // buddy is our parent
		Matrix33 P2w_parent = GetMtxStrided<float,6,1>(buddy->fs->Iinv[0]), L2w_parent = GetMtxStrided<float,6,1>(buddy->fs->Iinv[0]+3);
		Matrix33 L2P_parent = GetMtxStrided<float,6,1>(fs->Ia_s_qinv_sT[0]+3), L2L_parent = GetMtxStrided<float,6,1>(fs->Ia_s_qinv_sT[3]+3);
		K += P2w_parent*L2P_parent + L2w_parent*L2L_parent;
	}	else if (buddy && &joints1[buddy->fs->iparent]==this) { // buddy is our child
		Matrix33 L2v = GetMtxStrided<float,6,1>(fs->Iinv[3]+3);
		Matrix33 w2w_child = GetMtxStrided<float,6,1>(buddy->fs->s_qinv_sT_Ia[0]), v2w_child = GetMtxStrided<float,6,1>(buddy->fs->s_qinv_sT_Ia[0]+3);
		K += w2w_child*L2w + v2w_child*L2v;
	}
}

void ArticulatedBody::ApplyImpulse(const Vec3& dP, const Vec3& dL, body_helper *bodies, int iCaller)
{
	Vec3 dPspat[2] = { dP, dL };
	Pext += dP; Lext += dL;

	strided_pointer<ArticulatedBody> joints = strided_pointer<ArticulatedBody>(this,fs->jointSize);
	if (fs->useTree<0) {
		for(; joints[0].fs->iparent; joints=joints+joints[0].fs->iparent);
		int useTree = 0;
		for(int i=0; i<=joints[0].nChildrenTree; i++) if (joints[i].body.bProcessed[iCaller])
			for(int j=i; j>0; j=joints[j].iParent) {
				useTree = max(useTree, joints[j].fs->useTree);
				joints[j].fs->useTree = 1;
			}
		if (!useTree)
			for(int i=0; i<=joints[0].nChildrenTree; joints[i++].fs->useTree>>=31);
	}

	if (!fs->useTree) {
		vectornf(6,&body.w.x) -= matrixf(6,6,0,fs->Iinv[0])*vectornf(6,&dPspat[0].x);
		int ibody = body.bProcessed[iCaller]-1;
		bodies[ibody].v=body.v; bodies[ibody].w=body.w;	bodies[ibody].L+=dL;
	}	else {
		for(; joints[0].fs->iparent; joints=joints+joints[0].fs->iparent);
		fs->Ya_vec[0]=-dP; fs->Ya_vec[1]=-dL;
		CollectPendingImpulses(joints,0);
		Vec3 Ya_vec[2]; 
		vectornf(6,&Ya_vec[0].x) = matrixf(6,6,0,joints[0].fs->Ia_s_qinv_sT[0])*vectornf(6,&joints[0].fs->Ya_vec[0].x);
		PropagateImpulses(joints, *joints[0].fs->pM0inv*-Ya_vec[0]);
		for(int i=0,j;i<=joints[0].nChildrenTree;i++) if (j=joints[i].body.bProcessed[iCaller]) {
			bodies[j-1].v = (joints[i].body.v += joints[i].fs->dv_vec[1]);
			bodies[j-1].w = (joints[i].body.w += joints[i].fs->dv_vec[0]);
		}
		bodies[body.bProcessed[iCaller]-1].L += dL;
	}
}

void CArticulatedEntity::OnContactResolved(entity_contact *pcontact, int iop, int iGroupId)
{
	if (iop<2 && pcontact->pent[iop^1]!=this) {
		m_nBodyContacts++;
		m_nDynContacts -= -pcontact->pent[iop^1]->m_iSimClass >> 31;
		m_joints[m_infos[pcontact->ipart[iop]].iJoint].bHasExtContacts++;
	}
	CPhysicalEntity::OnContactResolved(pcontact,iop,iGroupId);
	// force simtype 1 in case of character-vehicle interaction
	//m_iSimTypeOverride |= inrange(pcontact->pbody[iop^1]->Minv,1E-8f,m_body.Minv*0.1f);
}


void CArticulatedEntity::GetJointTorqueResponseMatrix(int idx, Matrix33 &K)
{
	K = GetMtxStrided<float,6,1>(&m_joints[idx].fs->Iinv[0][3])*-1.0f;	// Lw*-1
	if (m_joints[idx].iParent>=0) {
		Matrix33 ww_up = GetMtxStrided<float,6,1>(&m_joints[idx].fs->s_qinv_sT_Ia[0][0]),
							 vw_up = GetMtxStrided<float,6,1>(&m_joints[idx].fs->s_qinv_sT_Ia[0][3]),
							 LL_down = GetMtxStrided<float,6,1>(&m_joints[idx].fs->Ia_s_qinv_sT[3][3]),
							 LP_down = GetMtxStrided<float,6,1>(&m_joints[idx].fs->Ia_s_qinv_sT[0][3]),
							 Lv_parent = GetMtxStrided<float,6,1>(&m_joints[m_joints[idx].iParent].fs->Iinv[3][3]), 
							 Lw_parent = GetMtxStrided<float,6,1>(&m_joints[m_joints[idx].iParent].fs->Iinv[0][3]),
							 Pw_parent = GetMtxStrided<float,6,1>(&m_joints[m_joints[idx].iParent].fs->Iinv[0][0]);
		//Matrix33 rmtx;
		//(m_joints[idx].body.pos-m_joints[m_joints[idx].iParent].body.pos).crossproduct_matrix(rmtx);
		K += vw_up*Lv_parent;//-rmtx*Lw_parent);
		K += ww_up*Lw_parent;
		K += Pw_parent*LP_down;
		K += Lw_parent*LL_down;
		K -= Lw_parent;
	}
}


entity_contact *CArticulatedEntity::CreateConstraintContact(int idx)
{
	entity_contact *pcontact = (entity_contact*)AllocSolverTmpBuf(sizeof(entity_contact));
	if (!pcontact) 
		return 0;
	pcontact->flags = 0;
	pcontact->pent[0] = this;
	pcontact->pbody[0] = &m_joints[idx].body;
	pcontact->ipart[0] = m_joints[idx].iStartPart;
	if (m_joints[idx].iParent>=0) {
		pcontact->pent[1] = this;
		pcontact->pbody[1] = &m_joints[m_joints[idx].iParent].body;
		pcontact->ipart[1] = m_joints[m_joints[idx].iParent].iStartPart;
	} else {
		pcontact->pent[1] = &g_StaticPhysicalEntity;
		pcontact->pbody[1] = &g_StaticRigidBody;
		pcontact->ipart[1] = 0;
	}
	pcontact->friction = 0;
	pcontact->vreq.zero();
	pcontact->pt[0] = pcontact->pt[1] = m_joints[idx].body.pos + m_joints[idx].quat*m_joints[idx].pivot[1];
	//pcontact->K.SetZero();

	return pcontact;
}


void CArticulatedEntity::AssignContactsToJoints()
{
	if (!m_bContactsAssigned) {
		for(int i=0;i<m_nJoints;i++) m_joints[i].pContact = 0;
		for(entity_contact *pContact=m_pContactStart; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next) if (!(pContact->flags & contact_2b_verified)) {
			int j = m_infos[pContact->ipart[0]].iJoint;
			pContact->nextAux = m_joints[j].pContact;	m_joints[j].pContact = pContact;
		}
		m_bContactsAssigned = 1;
	}
}

int CArticulatedEntity::RegisterContacts(float time_interval,int nMaxPlaneContacts)
{
	m_Ejoints = 0;
	int flags = 0;
	if (m_bFeatherstone && m_joints[0].fs) {
		DisablePreCG();
		int useTree = m_joints[0].nPotentialAngles==0 && m_M0inv.IsZero() ? -1:1;
		for(int i=0;i<m_nJoints;i++) {
			m_joints[i].fs->iparent = m_joints[i].iParent-i;
			m_joints[i].fs->useTree = useTree;
			m_joints[i].fs->jointSize = sizeof(ae_joint);
			m_joints[i].fs->pM0inv = &m_M0inv;
			matrixf s(6,m_joints[i].nActiveAngles,0, m_joints[i].fs->s);
			matrixf Ia_s(6,m_joints[i].nActiveAngles,0, m_joints[i].fs->Ia_s);
			vectornf ddq(m_joints[i].nActiveAngles, m_joints[i].ddq);
			Vec3 Pspat[2],vspat[2];
			vectornf(6,&Pspat[0].x) = Ia_s*ddq;
			vectornf(6,&vspat[0].x) = s*ddq;
			m_Ejoints += Pspat[0]*vspat[1]+Pspat[1]*vspat[0];
		}
		flags = rb_articulated;
		m_joints[0].fs->iparent = 0;
	}

	int idx,i,j,iAxes[3];
	entity_contact *pContact;
	Vec3 dw,dL;
	float kd,ks;
__ae_step++;
	if (m_flags & pef_log_collisions && m_nMaxEvents>0 && m_pWorld->m_iLastLogPump!=m_iLastLogColl) {
		m_nEvents=0; m_vcollMin=1E10f; m_iLastLogColl=m_pWorld->m_iLastLogPump;	
	}

	UpdateConstraints(time_interval);
	m_bContactsAssigned = 0;
	AssignContactsToJoints();

	for(idx=0;idx<m_nJoints;idx++) {
		(m_joints[idx].body.flags &= ~rb_articulated) |= flags;
		if (m_joints[idx].pContact)
			ArchiveContact(m_joints[idx].pContact);
		for(pContact=m_joints[idx].pContact; pContact; pContact=pContact->nextAux)
			RegisterContact(pContact);

		for(i=0;i<NMASKBITS && getmask(i)<=m_constraintMask;i++) 
		if (m_constraintMask&getmask(i) && m_pConstraintInfos[i].bActive && 
				(unsigned int)(m_pConstraints[i].ipart[0]-m_joints[idx].iStartPart) < (unsigned int)m_joints[idx].nParts)
			RegisterContact(m_pConstraints+i);

		if (!m_bFeatherstone && (m_bGrounded || m_joints[idx].iParent>=0)) {
			Vec3 pivot[2],axisDrift(ZERO),axisTens(ZERO);
			if (!(pContact = CreateConstraintContact(idx)))
				break;
			pContact->flags = contact_constraint_3dof;
			pContact->n.Set(0,0,-1);
			//GetContactMatrix(pContact->pt[0], pContact->ipart[0], pContact->K);
			if (m_joints[idx].iParent>=0) {
				//GetContactMatrix(pContact->pt[0], pContact->ipart[1], pContact->K);
				pivot[0] = m_joints[m_joints[idx].iParent].body.pos + m_joints[m_joints[idx].iParent].quat*m_joints[idx].pivot[0];
			} else 
				pivot[0] = m_posPivot;
			pivot[1] = m_joints[idx].body.pos + m_joints[idx].quat*m_joints[idx].pivot[1];
			if (m_iSimTypeCur && (pivot[0]-pivot[1]).len2()>sqr(0.003f)) {
				pContact->vreq = (pivot[0]-pivot[1])*10.0f;
				pContact->n = pContact->vreq.normalized();
				if (pContact->vreq.len2()>sqr(10.0f))
					pContact->vreq = pContact->n*10.0f;
				pContact->pt[1] = pivot[0];
			}
			RegisterContact(pContact);
			m_Ejoints += (pContact->vreq.len2()+(pContact->pbody[0]->v-pContact->pbody[1]->v).GetLengthFast()*pContact->vreq.GetLengthFast()*2)*(pContact->pbody[0]->M+pContact->pbody[1]->M);

			for(i=j=0,ks=kd=0;i<3;i++) if (!(m_joints[idx].flags & angle0_locked<<i)) {
				iAxes[j++] = i;
				axisTens += m_joints[idx].rotaxes[i]*(min(1.0f,max(-1.0f,m_joints[idx].q0[i]-m_joints[idx].q[i]-m_joints[idx].qext[i])));
				kd = max(kd, m_joints[idx].kd[i]);
				kd = max(kd, m_joints[idx].kdashpot[i]*(1^iszero((int)m_joints[idx].flags & joint_dashpot_reached))); 
				ks = max(ks, m_joints[idx].ks[i]);
			} else
				axisDrift += m_joints[idx].rotaxes[i]*min(1.0f,m_joints[idx].q0[i]-m_joints[idx].q[i]-m_joints[idx].qext[i]);
			if (!(pContact = CreateConstraintContact(idx)))
				break;
			Matrix33 K = m_joints[idx].body.Iinv;
			if (m_joints[idx].iParent>=0)
				K += m_joints[m_joints[idx].iParent].body.Iinv;
			dw = pContact->pbody[0]->w-pContact->pbody[1]->w;
			if (j<3) {
				if (j==2) { // 2 free axes, 1 axis locked
					pContact->flags = contact_constraint_2dof | contact_angular;
					pContact->n = (m_joints[idx].rotaxes[iAxes[0]]^m_joints[idx].rotaxes[iAxes[1]]).normalized();
					pContact->vreq = pContact->n*min(5.0f,(axisDrift*pContact->n)*6.0f);
					dw -= pContact->n*(pContact->n*dw);
				} else if (j==1) { // 1 free axis, 2 axes locked
					pContact->flags = contact_constraint_1dof | contact_angular;
					pContact->n = m_joints[idx].rotaxes[iAxes[0]];
					pContact->vreq = (axisDrift-pContact->n*(pContact->n*axisDrift))*6.0f;
					if (pContact->vreq.len2()>sqr(5.0f))
						pContact->vreq.normalize() *= 5.0f;
					dw = pContact->n*(pContact->n*dw);
				}	else {
					pContact->flags |= contact_constraint_3dof | contact_angular;
					Quat qparent = m_joints[idx].iParent>=0 ? m_joints[m_joints[idx].iParent].quat : m_qNew;
					Quat qdst = qparent*m_joints[idx].quat0*Quat(m_joints[idx].q0), qdiff = qdst*!m_joints[idx].quat;
					pContact->n = (pContact->vreq = qdiff.v*sgnnz(qdiff.w)*20.0f).normalized(); dw.zero(); 
					if ((ks=max(max(m_joints[idx].ks.x,m_joints[idx].ks.y),m_joints[idx].ks.z))>0) {
						pContact->Pspare = (K.GetInverted()*pContact->n).len()*ks*time_interval;
						pContact->flags |= contact_preserve_Pspare; ks=0;
					}
				}
				RegisterContact(pContact);
				pContact->vreq *= m_iSimTypeCur;
				m_Ejoints += ((m_joints[idx].I+m_joints[m_joints[idx].iParent].I)*pContact->vreq).GetLengthFast() * 
					(pContact->vreq.GetLengthFast() + (pContact->pbody[0]->w-pContact->pbody[1]->w).GetLengthFast()*2);
			}	else 
				ks = 0;
			dL = K.GetInverted()*(dw*-min(1.0f,time_interval*kd)+axisTens*(time_interval*ks));
			pContact->pbody[0]->L += dL; pContact->pbody[1]->L -= dL; 
			pContact->pbody[0]->w = pContact->pbody[0]->Iinv*pContact->pbody[0]->L;
			pContact->pbody[1]->w = pContact->pbody[1]->Iinv*pContact->pbody[1]->L;
		}

		if ((m_iSimClass<=2 || !m_bFeatherstone) && (m_bGrounded || m_joints[idx].iParent>=0)) {
			for(i=0;i<3;i++) if (m_joints[idx].flags & angle0_limit_reached<<i) {
				if (!(pContact = CreateConstraintContact(idx)))
					break;
				pContact->flags = contact_angular;
				pContact->n = m_joints[idx].rotaxes[i]*-sgnnz(m_joints[idx].dq_limit[i]);
				if (m_iSimTypeCur)
					pContact->vreq = pContact->n*min(5.0f,fabsf(m_joints[idx].dq_limit[i])*4.0f);
				//pContact->vsep = -0.01f;
				RegisterContact(pContact);
			}
		}
	}

	return 0;
}

float __maxdiff = 0;

int CArticulatedEntity::Update(float time_interval, float damping)
{
	if (m_bFeatherstone && m_nJoints>0) {
		int active = 1;
		if (m_joints[0].fs) {
			for(int i=0;i<m_nJoints;i++) { m_joints[i].Pext*=m_scaleBounceResponse; m_joints[i].Lext*=m_scaleBounceResponse; }
			CollectPendingImpulses(0,active);
			float dampingF = m_iSimClass>2 ? damping : 1.0f;
			if (active) {
				Vec3 dv; PropagateImpulses(dv = m_M0inv*-m_Ya_vec[0]);
				if (!m_bGrounded)
					(m_body.v += dv)*=dampingF;
				int doSnap = 2-m_iSimClass>>31;
				for(int i=0;i<m_nJoints;i++) {
					m_joints[i].dq *= dampingF;
					for(int j=0;j<(m_joints[i].nActiveAngles&doSnap);j++) 
						if (max(m_joints[i].prev_dq[j]*m_joints[i].dq[j], fabs_tpl(m_joints[i].dq[j])*0.2f-fabs_tpl(m_joints[i].prev_dq[j]))<0)
							m_joints[i].dq[j] = 0;
					SyncBodyWithJoint(i,2);
				}
			}
		}
		if (active) {
			m_timeIdle=0; m_bAwake=1;	m_simTime=0;
		}
		if (m_iSimClass>2)
		 return 1;
	}

	int i,j,nCollJoints=0,bPosChanged=0, bFloating=m_bFloating|iszero(m_gravity.len2()); 
	int iCaller = get_iCaller();
	entity_contact *pContact;
	float dt,e,minEnergy = m_nBodyContacts>=m_nCollLyingMode ? m_EminLyingMode : m_Emin;
	m_bAwake = (iszero(m_nBodyContacts) & (bFloating^1)) | isneg(m_simTimeAux-0.5f) | isneg(-m_minAwakeTime);
	m_bUsingUnproj = 0;
	m_nStepBackCount = (m_nStepBackCount&-(int)m_bSteppedBack)+m_bSteppedBack;

	for(pContact=m_pContactStart,i=0; pContact!=CONTACT_END(m_pContactStart); pContact=pContact->next) {
		INT_PTR pSafePtr = (INT_PTR)pContact->pent[1];
		// only access m_bAwake if iSimClass is 2, otherwise access data that's safely within CPhysicalEntity bounds
		pSafePtr -= (INT_PTR)&((CRigidEntity*)0)->m_iVarPart0 & ~-iszero(pContact->pent[1]->m_iSimClass-2);
		i |= isneg(sqr_signed(pContact->n*m_gravity)+m_gravity.len2()*0.5f) & iszero((int)pContact->pent[1]->m_iSimClass*2+(int)((CRigidEntity*)pSafePtr)->m_bAwake-5);
	}
	minEnergy *= 1-i*0.9f;

	if (m_flags & pef_log_collisions && m_nMaxEvents>0) {
		if (m_pWorld->m_iLastLogPump!=m_iLastLogColl) {
			m_nEvents=0; m_vcollMin=1E10f; m_iLastLogColl=m_pWorld->m_iLastLogPump;	
		}
		for(i=0;i<(int)m_nEvents;i++) if (m_pEventsColl[i]->pEntContact) {
			m_pEventsColl[i]->normImpulse += ((entity_contact*)m_pEventsColl[i]->pEntContact)->Pspare;
			m_pEventsColl[i]->pEntContact = 0;
		}
	}

	if (m_body.M>0) {
		for(i=0;i<m_nJoints;i++) {
			m_joints[i].dq *= damping;
			m_joints[i].body.v*=damping; m_joints[i].body.w*=damping;
			m_joints[i].body.P*=damping; m_joints[i].body.L*=damping;

			e = (m_joints[i].body.v.len2() + (m_joints[i].body.L*m_joints[i].body.w)*m_joints[i].body.Minv)*0.5f + m_joints[i].body.Eunproj;
			m_bAwake |= (m_joints[i].bAwake = isneg(minEnergy-e));
			m_bUsingUnproj |= isneg(1E-8f-m_joints[i].body.Eunproj);
			m_joints[i].bAwake |= bFloating|m_bUsingUnproj;
			for(j=m_joints[i].iParent; j>=0; j=m_joints[j].iParent)
				m_joints[i].bAwake |= m_joints[j].bAwake;
			nCollJoints += m_joints[i].bHasExtContacts;

			if (m_iSimTypeCur && m_joints[i].body.Eunproj>0) {
				SyncJointWithBody(i,3);
				for(j=m_joints[i].iStartPart; j<m_joints[i].iStartPart+m_joints[i].nParts; j++) {
					(m_infos[j].q = m_joints[i].quat*m_infos[j].q0).Normalize();
					m_infos[j].pos = m_joints[i].quat*m_infos[j].pos0+m_joints[i].body.pos-m_pos;
				}
				bPosChanged = 1;
			}
		}
		if (bPosChanged) {
			ComputeBBox(m_BBoxNew);
			UpdatePosition(m_pWorld->RepositionEntity(this,1,m_BBoxNew));
		}
		m_nSleepFrames = (m_nSleepFrames&~-(int)m_bAwake) + (m_bAwake^1);
		m_bAwake |= isneg((int)m_nSleepFrames-4) & isneg(nCollJoints-3);
		m_timeIdle *= isneg(-m_nColliders-m_nPrevColliders-m_submergedFraction) & m_pWorld->m_threadData[iCaller].bGroupInvisible;
		i = isneg(0.0001f-m_maxTimeIdle); // forceful deactivation is turned on
		dt = time_interval+(m_timeStepFull-time_interval)*m_pWorld->m_threadData[iCaller].bGroupInvisible;
		m_bAwake &= i^1 | isneg((m_timeIdle+=dt*i)-m_maxTimeIdle);
		if (!m_bGrounded)
			m_body.v = m_joints[0].body.v;
		m_body.P = m_body.v*m_body.M;
		m_body.w.zero();
		m_bInGroup = isneg(-m_nDynContacts);

		if (!m_bFeatherstone) for(i=0; i<m_nJoints; i++) {
			SyncJointWithBody(i,2);
			if (m_iSimTypeCur==0 && (m_bGrounded || i))
				SyncBodyWithJoint(i,2);
		}
		if (!m_bGrounded && m_nJoints>0 && max(m_joints[0].dq.len2(),m_joints[0].body.w.len2())>sqr(m_maxw)) {
			m_joints[0].body.w.normalize() *= m_maxw;
			m_joints[0].dq.normalize() *= m_maxw;
		}

		if (m_iSimClass-1!=m_bAwake) {
			m_iSimTypeOverride &= m_bAwake;
			m_iSimClass = 1+m_bAwake;
			m_pWorld->RepositionEntity(this,2);
		}

		if (!m_bAwake) {
			for(i=0;i<m_nJoints;i++) {
				m_joints[i].vSleep=m_joints[i].body.v; m_joints[i].wSleep=m_joints[i].body.w;
				m_joints[i].dq.zero(); m_joints[i].body.P.zero(); m_joints[i].body.L.zero();
				m_joints[i].body.v.zero(); m_joints[i].body.w.zero();
			}
			m_body.P.zero(); m_body.L.zero(); m_body.v.zero(); m_body.w.zero();
		}
		UpdateJointDyn();
	}
	m_minAwakeTime = max(m_minAwakeTime,0.0f)-time_interval;

	return (m_bAwake^1) | isneg(m_timeStepFull-m_timeStepPerformed-0.001f) | m_pWorld->m_threadData[iCaller].bGroupInvisible;
}


void CArticulatedEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_ARTICULATED)
		pSizer->AddObject(this, sizeof(CArticulatedEntity));
	CRigidEntity::GetMemoryStatistics(pSizer);
	pSizer->AddObject(m_joints, m_nJointsAlloc*sizeof(m_joints[0]));
	for(int i=0; i<m_nJoints; i++) if (m_joints[i].fsbuf)
		pSizer->AddObject(m_joints[i].fsbuf, sizeof(featherstone_data)+16);
	pSizer->AddObject(m_infos, m_nPartsAlloc*sizeof(m_infos[0]));
}
