// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "geoman.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "physicalworld.h"


IPhysicalWorld *CPhysicalPlaceholder::GetWorld() const
{
	CPhysicalWorld *pWorld = NULL;
	if (g_nPhysWorlds==1)
		pWorld = g_pPhysWorlds[0];
	else
		for(int i=0;i<g_nPhysWorlds && !(pWorld=g_pPhysWorlds[i])->IsPlaceholder(this);i++);
	return pWorld;
}


CPhysicalEntity *CPhysicalPlaceholder::GetEntity()
{
	CPhysicalEntity *pEntBuddy;
	if (!m_pEntBuddy) {
		CPhysicalWorld *pWorld = NULL;
		if (g_nPhysWorlds==1)
			pWorld = g_pPhysWorlds[0];
		else 
			for(int i=0;i<g_nPhysWorlds && !(pWorld=g_pPhysWorlds[i])->IsPlaceholder(this);i++);
		PREFAST_ASSUME(pWorld);
		if (pWorld->m_pPhysicsStreamer) {
			pWorld->m_pPhysicsStreamer->CreatePhysicalEntity(m_pForeignData,m_iForeignData,m_iForeignFlags);
			pEntBuddy = m_pEntBuddy ? (CPhysicalEntity*)m_pEntBuddy : &g_StaticPhysicalEntity;
		} else
			return 0;
	}	else
		pEntBuddy = (CPhysicalEntity*)m_pEntBuddy;
	pEntBuddy->m_timeIdle = 0;
	return pEntBuddy;
}


pe_type CPhysicalPlaceholder::GetType() const
{
	switch (m_iSimClass) {
		case 0: return PE_STATIC;
		case 1:case 2: return PE_RIGID;
		case 3: return PE_LIVING;
		case 4: return PE_PARTICLE;
		default: return PE_NONE;
	}
}

inline int IsAreaTrigger(const CPhysicalPlaceholder *ppc) { return iszero(ppc->m_iSimClass-6) & -(ppc->m_iForeignFlags & ent_areas)>>31; }

int CPhysicalPlaceholder::SetParams(pe_params *_params, int bThreadSafe)
{
	if (_params->type==pe_params_bbox::type_id) {
		pe_params_bbox *params = (pe_params_bbox*)_params;
		if (!is_unused(params->BBox[0])) m_BBox[0] = params->BBox[0];
		if (!is_unused(params->BBox[1])) m_BBox[1] = params->BBox[1];
		if (m_pEntBuddy) {
			CPhysicalEntity *pent = (CPhysicalEntity*)m_pEntBuddy;
			if (pent->m_flags & (pef_monitor_state_changes | pef_log_state_changes)) {
				EventPhysStateChange event;
				event.pEntity=pent; event.pForeignData=pent->m_pForeignData; event.iForeignData=pent->m_iForeignData;
				event.BBoxNew[0]=params->BBox[0]; event.BBoxNew[1]=params->BBox[1];
				event.BBoxOld[0]=pent->m_BBox[0]; event.BBoxOld[1]=pent->m_BBox[1];
				event.iSimClass[0] = pent->m_iSimClass; event.iSimClass[1] = pent->m_iSimClass;
				event.timeIdle = pent->m_timeIdle;

				pent->m_pWorld->OnEvent(pent->m_flags, &event);
			}

			if (m_pEntBuddy->m_pEntBuddy==this) {
				m_pEntBuddy->m_BBox[0] = m_BBox[0];
				m_pEntBuddy->m_BBox[1] = m_BBox[1];
			}
		}

		CPhysicalWorld *pWorld = (CPhysicalWorld*)GetWorld();
		pWorld->RepositionEntity(this,1|8);
		return 1;
	}

	if (_params->type==pe_params_pos::type_id) {
		pe_params_pos *params = (pe_params_pos*)_params;
		if (!is_unused(params->pos) | !is_unused(params->q) | !is_unused(params->scale) | 
				(intptr_t)params->pMtx3x3 | (intptr_t)params->pMtx3x4)
			return GetEntity()->SetParams(params);
		if (!is_unused(params->iSimClass)) {
			int wasAreaTrigger = IsAreaTrigger(this);
			m_iSimClass = params->iSimClass;
			if (IsAreaTrigger(this) ^ wasAreaTrigger)
				((CPhysicalWorld*)GetWorld())->m_numAreaTriggers += 1-wasAreaTrigger*2;
		}
		return 1;
	}

	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		int wasAreaTrigger = IsAreaTrigger(this);
		if (!is_unused(params->pForeignData)) m_pForeignData = 0;
		if (!is_unused(params->iForeignData)) m_iForeignData = params->iForeignData;
		if (!is_unused(params->pForeignData)) m_pForeignData = params->pForeignData;
		if (!is_unused(params->iForeignFlags)) m_iForeignFlags = params->iForeignFlags;
		m_iForeignFlags = (m_iForeignFlags & params->iForeignFlagsAND) | params->iForeignFlagsOR;
		if (IsAreaTrigger(this) ^ wasAreaTrigger)
			((CPhysicalWorld*)GetWorld())->m_numAreaTriggers += 1-wasAreaTrigger*2;
		if (m_pEntBuddy) {
			m_pEntBuddy->m_pForeignData = m_pForeignData;
			m_pEntBuddy->m_iForeignData = m_iForeignData;
			m_pEntBuddy->m_iForeignFlags = m_iForeignFlags;
		}
		return 1;
	}

	if (m_pEntBuddy)
		return m_pEntBuddy->SetParams(_params);
	return 0;//GetEntity()->SetParams(_params);
}

int CPhysicalPlaceholder::GetParams(pe_params *_params) const
{
	if (_params->type==pe_params_bbox::type_id) {
		pe_params_bbox *params = (pe_params_bbox*)_params;
		params->BBox[0] = m_BBox[0];
		params->BBox[1] = m_BBox[1];
		return 1;
	}

	if (_params->type==pe_params_foreign_data::type_id) {
		pe_params_foreign_data *params = (pe_params_foreign_data*)_params;
		params->iForeignData = m_iForeignData;
		params->pForeignData = m_pForeignData;
		params->iForeignFlags = m_iForeignFlags;
		return 1;
	}

	return ((CPhysicalEntity*)this)->GetEntity()->GetParams(_params);
}

int CPhysicalPlaceholder::GetStatus(pe_status* _status) const
{ 
	if (_status->type==pe_status_placeholder::type_id) {
		((pe_status_placeholder*)_status)->pFullEntity = m_pEntBuddy;
		return 1;
	}

	if (_status->type==pe_status_awake::type_id)
		return 0;

	return ((CPhysicalEntity*)this)->GetEntity()->GetStatus(_status); 
}
int CPhysicalPlaceholder::Action(pe_action* action, int bThreadSafe) { 
	if (action->type==pe_action_awake::type_id && ((pe_action_awake*)action)->bAwake==0 && !m_pEntBuddy) {
		if (m_iSimClass==2)
			m_iSimClass = 1;
		return 1;
	}
	if (action->type==pe_action_remove_all_parts::type_id && !m_pEntBuddy)
		return 1;
	if (action->type==pe_action_reset::type_id) {
		if (!m_pEntBuddy)
			return 1;
		((CPhysicalEntity*)m_pEntBuddy)->m_timeIdle = ((CPhysicalEntity*)m_pEntBuddy)->m_maxTimeIdle+1.0f;
		return m_pEntBuddy->Action(action);
	}
	return GetEntity()->Action(action); 
}

int CPhysicalPlaceholder::AddGeometry(phys_geometry *pgeom, pe_geomparams* params,int id, int bThreadSafe) { 
	return GetEntity()->AddGeometry(pgeom,params,id,bThreadSafe); 
}
void CPhysicalPlaceholder::RemoveGeometry(int id, int bThreadSafe) { 
	return GetEntity()->RemoveGeometry(id,bThreadSafe); 
}
int CPhysicalPlaceholder::GetStateSnapshot(class CStream &stm, float time_back, int flags) { 
	return GetEntity()->GetStateSnapshot(stm,time_back,flags); 
}
int CPhysicalPlaceholder::GetStateSnapshot(TSerialize ser, float time_back, int flags) {
	return GetEntity()->GetStateSnapshot(ser,time_back,flags);
}
int CPhysicalPlaceholder::SetStateFromSnapshot(class CStream &stm, int flags) { 
	return GetEntity()->SetStateFromSnapshot(stm,flags); 
}
int CPhysicalPlaceholder::SetStateFromSnapshot(TSerialize ser, int flags) {
	return GetEntity()->SetStateFromSnapshot(ser,flags);
}
int CPhysicalPlaceholder::SetStateFromTypedSnapshot(TSerialize ser, int type, int flags) {
	return GetEntity()->SetStateFromTypedSnapshot(ser,type,flags);
}
int CPhysicalPlaceholder::PostSetStateFromSnapshot() { 
	return GetEntity()->PostSetStateFromSnapshot(); 
}
int CPhysicalPlaceholder::GetStateSnapshotTxt(char *txtbuf,int szbuf, float time_back) {
	return GetEntity()->GetStateSnapshotTxt(txtbuf,szbuf,time_back);
}
void CPhysicalPlaceholder::SetStateFromSnapshotTxt(const char *txtbuf,int szbuf) {
	GetEntity()->SetStateFromSnapshotTxt(txtbuf,szbuf);
}
unsigned int CPhysicalPlaceholder::GetStateChecksum() {
	return GetEntity()->GetStateChecksum();
}
void CPhysicalPlaceholder::SetNetworkAuthority(int authoritive, int paused) {
	return GetEntity()->SetNetworkAuthority(authoritive, paused);
}

int CPhysicalPlaceholder::Step(float time_interval) { 
	return GetEntity()->Step(time_interval); 
}
void CPhysicalPlaceholder::StartStep(float time_interval) { 
	GetEntity()->StartStep(time_interval); 
}
void CPhysicalPlaceholder::StepBack(float time_interval) { 
	return GetEntity()->StepBack(time_interval); 
}
