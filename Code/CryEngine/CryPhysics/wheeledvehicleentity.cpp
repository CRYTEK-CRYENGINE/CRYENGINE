// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "bvtree.h"
#include "geometry.h"
#include "raybv.h"
#include "raygeom.h"
#include "rigidbody.h"
#include "physicalplaceholder.h"
#include "physicalentity.h"
#include "geoman.h"
#include "physicalworld.h"
#include "rigidentity.h"
#include "wheeledvehicleentity.h"


CWheeledVehicleEntity::CWheeledVehicleEntity(CPhysicalWorld *pworld, IGeneralMemoryHeap* pHeap)
	: CRigidEntity(pworld, pHeap)
	, m_enginePower(10000.0f)
	, m_maxSteer(g_PI / 4)
	, m_engineMaxw(120.0f)
	, m_gearDirSwitchw(1.0f)
	, m_engineStartw(40.0f)
	, m_axleFriction(0.0f)
	, m_brakeTorque(4000.0f)
	, m_clutchSpeed(1.0f)
	, m_minBrakingFriction(0.0f)
	, m_maxBrakingFriction(1.0f)
	, m_kDynFriction(1.0f)
	, m_slipThreshold(0.05f)
	, m_kStabilizer(0.0f)
	, m_enginePedal(0.0f)
	, m_steer(0.0f)
	, m_ackermanOffset(0.0f)
	, m_clutch(0.0f)
	, m_nGears(3)
	, m_iCurGear(1)
	, m_maxGear(127)
	, m_minGear(0)
	, m_bHandBrake(1)
	, m_nHullParts(0)
	, m_bHasContacts(0)
	, m_bKeepTractionWhenTilted(0)
	, m_kSteerToTrack(0.0f)
	, m_EminVehicle(sqr(0.05f))
	, m_maxAllowedStepVehicle(0.02f)
	, m_dampingVehicle(0.01f)
	, m_timeNoContacts(10.0f)
	, m_lockVehicle(0)
	, m_pullTilt(0.0f)
	, m_drivingTorque(0.0f)
	, m_maxTilt(0.866f)
	, m_wheelMassScale(0.0f)
{
	m_engineMinw = m_engineMaxw*0.05f;
	m_wengine = m_engineIdlew = m_engineMaxw*0.1f;
	m_engineShiftUpw = m_engineMaxw*0.5f;
	m_engineShiftDownw = m_engineMaxw*0.2f;	

	m_gears[0] = -1.0f;
	m_gears[1] = 1.0f; // neutral, unused
	m_gears[2] = 1.0f;

	m_EminRigid = m_Emin;
	m_maxAllowedStepRigid = m_maxAllowedStep;


	//CRigidEntity
	m_bCollisionCulling = 1;
	m_nContacts = 0;
	m_bDisablePreCG = 1;

	//CPhysicalEntity
	m_flags = (m_flags|pef_fixed_damping)&~pef_never_break;
	m_collisionClass.type |= collision_class_wheeled;
}


int CWheeledVehicleEntity::SetParams(pe_params *_params, int bThreadSafe)
{
	ChangeRequest<pe_params> req(this,m_pWorld,_params,bThreadSafe);
	if (req.IsQueued())
		return 1+(m_bProcessed>>PENT_QUEUED_BIT);

	int res;
	if (res = CRigidEntity::SetParams(_params,1)) {
		if (_params->type==pe_simulation_params::type_id) {
			pe_simulation_params *params = (pe_simulation_params*)_params;
			if (!is_unused(params->maxTimeStep)) m_maxAllowedStepRigid = params->maxTimeStep;
			if (!is_unused(params->minEnergy)) m_EminRigid = params->minEnergy;
		}	else if (_params->type==pe_params_part::type_id) {
			pe_params_part *params = (pe_params_part*)_params;
			int i;
			if (is_unused(params->ipart))
				for(i=0;i<m_nParts && m_parts[i].id!=params->partid;i++);
			else i = params->ipart;
			if (i>=m_nParts)
				return 0;
			if (i>=m_nHullParts) {
				if (!is_unused(params->pPhysGeom) || !is_unused(params->pPhysGeomProxy)) {
					m_susp[i-m_nHullParts].q0 = m_parts[i].q;
					m_susp[i-m_nHullParts].pos0 = m_parts[i].pos;
					m_susp[i-m_nHullParts].ptc0 = m_parts[i].q*m_parts[i].pPhysGeomProxy->origin + m_parts[i].pos;
				}
				(m_susp[i-m_nHullParts].flags0 &= params->flagsAND) |= params->flagsOR;
				(m_susp[i-m_nHullParts].flagsCollider0 &= params->flagsColliderAND) |= params->flagsColliderOR;
				m_parts[i].flagsCollider = 0;
			}
		}
		return res;
	}
		
	if (_params->type==pe_params_car::type_id) {
		pe_params_car *params = (pe_params_car*)_params;
		WriteLock lock(m_lockVehicle);
		if (!is_unused(params->axleFriction)) m_axleFriction = params->axleFriction;
		if (!is_unused(params->enginePower)) m_enginePower = params->enginePower;
		if (!is_unused(params->engineMaxRPM)) m_engineMaxw = params->engineMaxRPM*(2*g_PI/60.0);
		if (!is_unused(params->maxSteer)) m_maxSteer = params->maxSteer;
		// if (!is_unused(params->iIntegrationType)) m_iIntegrationType = params->iIntegrationType;
		if (!is_unused(params->maxTimeStep)) m_maxAllowedStepVehicle = params->maxTimeStep;
		if (!is_unused(params->minEnergy)) m_EminVehicle = params->minEnergy;
		if (!is_unused(params->damping)) m_dampingVehicle = params->damping;
    if (!is_unused(params->minBrakingFriction)) m_minBrakingFriction = params->minBrakingFriction;
		if (!is_unused(params->maxBrakingFriction)) m_maxBrakingFriction = params->maxBrakingFriction;
		if (!is_unused(params->kStabilizer)) m_kStabilizer = params->kStabilizer;
		if (!is_unused(params->engineMinRPM)) m_engineMinw = params->engineMinRPM*(2*g_PI/60.0);
		if (!is_unused(params->engineIdleRPM))
			m_wengine = m_engineIdlew = params->engineIdleRPM*(2*g_PI/60.0);
		if (!is_unused(params->engineStartRPM)) m_engineStartw = params->engineStartRPM*(2*g_PI/60.0);
		if (!is_unused(params->engineShiftUpRPM)) m_engineShiftUpw = params->engineShiftUpRPM*(2*g_PI/60.0);
		if (!is_unused(params->engineShiftDownRPM)) m_engineShiftDownw = params->engineShiftDownRPM*(2*g_PI/60.0);
		if (!is_unused(params->engineIdleRPM)) m_engineIdlew = params->engineIdleRPM*(2*g_PI/60.0);
		if (!is_unused(params->clutchSpeed)) m_clutchSpeed = params->clutchSpeed;
		if (!is_unused(params->nGears)) m_nGears = params->nGears;
		if (!is_unused(params->gearRatios)) for(int i=0;i<m_nGears;i++) m_gears[i] = params->gearRatios[i];
		if (!is_unused(params->maxGear)) m_maxGear = params->maxGear;
		if (!is_unused(params->minGear)) m_minGear = params->minGear;
		if (!is_unused(params->kDynFriction)) m_kDynFriction = params->kDynFriction;
		if (!is_unused(params->gearDirSwitchRPM)) m_gearDirSwitchw = params->gearDirSwitchRPM*(2*g_PI/60.0);
		if (!is_unused(params->slipThreshold)) m_slipThreshold = params->slipThreshold;
		if (!is_unused(params->brakeTorque)) m_brakeTorque = params->brakeTorque;
		if (!is_unused(params->steerTrackNeutralTurn)) 
			m_kSteerToTrack = params->steerTrackNeutralTurn>0 ? 2.0f/params->steerTrackNeutralTurn : 0;
    if (!is_unused(params->pullTilt)) m_pullTilt = params->pullTilt; 
		if (!is_unused(params->maxTilt)) m_maxTilt = params->maxTilt;
		if (!is_unused(params->bKeepTractionWhenTilted)) m_bKeepTractionWhenTilted = params->bKeepTractionWhenTilted;
		if (!is_unused(params->wheelMassScale)) m_wheelMassScale = params->wheelMassScale;
		return 1;
	}

	if (_params->type==pe_params_wheel::type_id) {
		pe_params_wheel *params = (pe_params_wheel*)_params;
		WriteLock lock(m_lockVehicle);
		int iWheel = max(0,min(m_nParts-m_nHullParts-1,params->iWheel));
		if (!is_unused(params->bDriving)) m_susp[iWheel].bDriving = params->bDriving;
		if (!is_unused(params->iAxle)) m_susp[iWheel].iAxle = params->iAxle;
		if (!is_unused(params->bCanBrake)) m_susp[iWheel].bCanBrake = params->bCanBrake;
		if (!is_unused(params->bCanSteer)) m_susp[iWheel].bCanSteer = params->bCanSteer;
    if (!is_unused(params->bBlocked)) m_susp[iWheel].bBlocked = params->bBlocked;
		if (!is_unused(params->suspLenMax)) {
			m_susp[iWheel].fullen = params->suspLenMax;
			if (m_susp[iWheel].len0>m_susp[iWheel].fullen) m_susp[iWheel].len0=m_susp[iWheel].fullen;
			if (m_susp[iWheel].curlen>m_susp[iWheel].fullen) m_susp[iWheel].curlen=m_susp[iWheel].fullen;
		}
		if (!is_unused(params->suspLenInitial)) m_susp[iWheel].len0 = params->suspLenInitial;
		if (!is_unused(params->minFriction)) m_susp[iWheel].minFriction = params->minFriction;
		if (!is_unused(params->maxFriction)) m_susp[iWheel].maxFriction = params->maxFriction;
		if (!is_unused(params->surface_idx)) m_parts[m_nHullParts+iWheel].surface_idx = params->surface_idx;
		if (!is_unused(params->bRayCast)) m_susp[iWheel].bRayCast = params->bRayCast;
		if (!is_unused(params->kStiffness)) m_susp[iWheel].kStiffness = params->kStiffness;
		if (!is_unused(params->kStiffnessWeight)) m_susp[iWheel].kStiffnessWeight = params->kStiffnessWeight;
		if (!is_unused(params->kDamping)) 
			m_susp[iWheel].kDamping = params->kDamping>0 ? 
				params->kDamping : -params->kDamping*sqrt_tpl(4*m_susp[iWheel].kStiffness*m_susp[iWheel].Mpt);
    if (!is_unused(params->kLatFriction)) m_susp[iWheel].kLatFriction = params->kLatFriction;
		if (!is_unused(params->Tscale)) m_susp[iWheel].Tscale = params->Tscale;
		if (!is_unused(params->w)) m_susp[iWheel].w = params->w;
		return 1;
	}

	return 0;
}


int CWheeledVehicleEntity::GetParams(pe_params *_params) const
{
	if (_params->type==pe_params_car::type_id) {
		pe_params_car *params = (pe_params_car*)_params;
		ReadLock lock(m_lockVehicle);
		params->axleFriction = m_axleFriction;
		params->enginePower = m_enginePower;
		params->engineMaxRPM = float2int(m_engineMaxw*(30.0/g_PI));
		params->maxSteer = m_maxSteer;
		params->iIntegrationType = 1; // m_iIntegrationType;
		params->maxTimeStep = m_maxAllowedStepVehicle;
		params->minEnergy = m_EminVehicle;
		params->damping = m_dampingVehicle;
    params->minBrakingFriction = m_minBrakingFriction;
		params->maxBrakingFriction = m_maxBrakingFriction;
		params->kStabilizer = m_kStabilizer;
		params->engineMinRPM = m_engineMinw*(60.0/(2*g_PI));
		params->engineIdleRPM = m_engineIdlew*(60.0/(2*g_PI));
		params->engineShiftUpRPM = m_engineShiftUpw*(60.0/(2*g_PI));
		params->engineShiftDownRPM = m_engineShiftDownw*(60.0/(2*g_PI));
		params->engineStartRPM = m_engineStartw*(60.0/(2*g_PI));
		params->clutchSpeed = m_clutchSpeed;
		params->nGears = m_nGears;
		if (!is_unused(params->gearRatios))
			for(int i=0;i<m_nGears;i++) params->gearRatios[i] = m_gears[i];
		params->maxGear = m_maxGear;
		params->minGear = m_minGear;
		params->kDynFriction = m_kDynFriction;
		params->gearDirSwitchRPM = m_gearDirSwitchw*(60.0/(2*g_PI));
		params->slipThreshold = m_slipThreshold;
		params->brakeTorque = m_brakeTorque;
		params->steerTrackNeutralTurn = m_kSteerToTrack!=0 ? 2.0f/m_kSteerToTrack : 0;
		params->nWheels = m_nParts-m_nHullParts;
		params->maxTilt = m_maxTilt;
		params->bKeepTractionWhenTilted = m_bKeepTractionWhenTilted;
		params->wheelMassScale = m_wheelMassScale;
		return 1;
	}

	if (_params->type==pe_params_wheel::type_id) {
		pe_params_wheel *params = (pe_params_wheel*)_params;
		ReadLock lock(m_lockVehicle);
		if ((unsigned int)params->iWheel>=(unsigned int)(m_nParts-m_nHullParts))
			return 0;
		int iWheel = params->iWheel;
		params->bDriving = m_susp[iWheel].bDriving;
		params->iAxle = m_susp[iWheel].iAxle;
		params->bCanBrake = m_susp[iWheel].bCanBrake;
		params->bCanSteer = m_susp[iWheel].bCanSteer;
		params->bBlocked = m_susp[iWheel].bBlocked;
		params->suspLenMax = m_susp[iWheel].fullen;
		params->suspLenInitial = m_susp[iWheel].len0;
		params->minFriction = m_susp[iWheel].minFriction;
		params->maxFriction = m_susp[iWheel].maxFriction;
		params->surface_idx = m_parts[m_nHullParts+iWheel].surface_idx;
		params->bRayCast = m_susp[iWheel].bRayCast;
		params->kStiffness = m_susp[iWheel].kStiffness;
		params->kStiffnessWeight = m_susp[iWheel].kStiffnessWeight;
		params->kDamping = m_susp[iWheel].kDamping;
    params->kLatFriction = m_susp[iWheel].kLatFriction;
		params->Tscale = m_susp[iWheel].Tscale;
		params->w = m_susp[iWheel].w;
		return 1;
	}

	int res = CRigidEntity::GetParams(_params);
	if (res && _params->type==pe_params_part::type_id && ((pe_params_part*)_params)->ipart>=m_nHullParts) {
		pe_params_part *params = (pe_params_part*)_params;
		params->flagsOR=params->flagsAND = m_susp[params->ipart-m_nHullParts].flags0;
		params->flagsColliderOR=params->flagsColliderAND = m_susp[params->ipart-m_nHullParts].flagsCollider0;
	}

	return res;
}


int CWheeledVehicleEntity::Action(pe_action *_action, int bThreadSafe)
{
	if (_action->type==pe_action_add_constraint::type_id)
		return CRigidEntity::Action(_action,bThreadSafe);

	ChangeRequest<pe_action> req(this,m_pWorld,_action,bThreadSafe);
	if (req.IsQueued())
		return 1;

	int res;
	if (res = CRigidEntity::Action(_action,1)) {
		/*if (_action->type==pe_action_impulse::type_id && ((pe_action_impulse*)_action)->iSource!=1)
			m_minAwakeTime = max(m_minAwakeTime,3.0f);
		else*/ if (_action->type==pe_action_reset::type_id) {
			for(int i=0;i<m_nParts-m_nHullParts;i++) 
				m_susp[i].w=m_susp[i].wa=m_susp[i].T=0;
			m_enginePedal = 0; m_timeNoContacts = 10.0f;
		}
		return res;
	}

	if (_action->type==pe_action_drive::type_id) {
		pe_action_drive *action = (pe_action_drive*)_action;
		if (!is_unused(action->dpedal)) m_enginePedal = min_safe(1.0f,max_safe(-1.0f,m_enginePedal+action->dpedal));
		if (!is_unused(action->pedal)) m_enginePedal = action->pedal;
		if (!is_unused(action->clutch)) m_clutch = action->clutch;
		if (!is_unused(action->iGear)) m_iCurGear = action->iGear;
		if (!is_unused(action->steer) || !is_unused(action->dsteer)) {
			int i;
			if (!is_unused(action->steer)) m_steer = action->steer;
			if (!is_unused(action->dsteer)) m_steer += action->dsteer;
			if (!is_unused(action->ackermanOffset)) m_ackermanOffset = action->ackermanOffset;
			m_steer = min_safe(m_maxSteer,max_safe(-m_maxSteer,m_steer));
			//if (m_pWorld->m_vars.bMultiplayer)
			//	m_steer = CompressFloat(m_steer,1.0f,12);
			if (m_kSteerToTrack==0) {
				if (m_steer!=0) {	// apply Ackerman steering to front wheels, ackermanOffset = 0.0 -> back wheels fixed, 0.5 -> front+back wheels steer, 1.0 -> front wheels fixed
					WriteLock lock(m_lockVehicle);
					// Choose the wheel that is furthest away from the ackerman line
					// maxY, y-position of most forward wheel, minY, y-position of furthest back wheel
					const int numWheels = m_nParts - m_nHullParts;
					float ackermanOffset, maxX=0.f, maxY=-10.f, minY=+10.f, dir=fsgnf(m_steer);
					for (int j=0;j<numWheels;j++) if (m_susp[j].iAxle>=0 && m_susp[j].bCanSteer) {
						minY = min(minY, m_susp[j].pt.y); maxY = max(maxY, m_susp[j].pt.y); maxX = max(maxX, fabsf(m_susp[j].pt.x));
					}
					ackermanOffset = minY + m_ackermanOffset*(maxY - minY);
					maxY = max(maxY - ackermanOffset, ackermanOffset - minY);
					if (maxY>0.01f) {
						float tanSteer = tanf(fabsf(m_steer));  // NB: turningRadius = maxY / tanf(fabsf(m_steer));
						for (int j=0;j<numWheels;j++) if (m_susp[j].iAxle>=0 && m_susp[j].bCanSteer) {
							float y = m_susp[j].pt.y-ackermanOffset, steer0=m_susp[j].steer;
							m_susp[j].steer = dir*atanf( y*tanSteer / (maxY + tanSteer*(maxX - m_susp[j].pt.x*dir)));
							if (m_wheelMassScale && max(fabs_tpl(steer0),fabs_tpl(m_susp[j].steer))>0.01f) {
								Vec3 axis0(ZERO),axis1(ZERO); 
								sincos_tpl(-steer0, &axis0.y,&axis0.x);
								sincos_tpl(-m_susp[j].steer, &axis1.y,&axis1.x);
								m_body.L -= m_qrot*(axis1-axis0)*(-m_susp[j].w*m_wheelMassScale/m_susp[j].Iinv);
							}
						}
						if (m_wheelMassScale)
							m_body.w = m_body.Iinv*m_body.L;
					}
				} else for(i=0;i<m_nParts-m_nHullParts;i++) m_susp[i].steer=0;

				{ WriteLock lock(m_lockUpdate); 
					for(i=0;i<m_nParts-m_nHullParts;i++) {
						m_susp[i].q=m_parts[i+m_nHullParts].q = Quat::CreateRotationAA(m_susp[i].steer,Vec3(0,0,-1))*m_susp[i].q0;
						Vec3 ptc1 = m_parts[i+m_nHullParts].q*m_parts[i+m_nHullParts].pPhysGeomProxy->origin + m_susp[i].pos0;
						(m_parts[i+m_nHullParts].pos = m_susp[i].pos0+m_susp[i].ptc0-ptc1).z -= m_susp[i].curlen-m_susp[i].len0;
						m_susp[i].pos = m_parts[i+m_nHullParts].pos;
					}
				}
			}
		}
		int bPrevHandBrake = m_bHandBrake;
		if (!is_unused(action->bHandBrake))
			m_bHandBrake = action->bHandBrake;
		//if (m_bHandBrake)
		//	m_enginePedal = 0;
		if ((m_enginePedal!=0 || !m_bHandBrake && bPrevHandBrake) && !m_bAwake) {
			m_bAwake=1;	m_minAwakeTime = max(m_minAwakeTime,1.0f);
			if (m_iSimClass<2) {
				m_iSimClass = 2; m_pWorld->RepositionEntity(this, 2);
			}
		}
		m_timeIdle = 0;
	}

	return 0;
}


int CWheeledVehicleEntity::GetStatus(pe_status* _status) const
{
	int res;
	if (_status->type==pe_status_pos::type_id) {
		pe_status_pos *status = (pe_status_pos*)_status;
		int iwheel,i,iLock=0,flags=status->flags;
		if (status->ipart==-1 && status->partid>=0) {
			for(i=0;i<m_nParts && m_parts[i].id!=status->partid;i++);
			if (i==m_nParts) return 0;
		} else
			i = status->ipart;

		Vec3 prevpos,ptc1; 
		quaternionf prevq;
		iwheel = i-m_nHullParts;
		if ((unsigned int)iwheel<(unsigned int)(m_nParts-m_nHullParts)) {
			// only rotate wheels when status pos is requested; keep them unrotated internally
			SpinLock(&m_lockUpdate,0,iLock=WRITE_LOCK_VAL);
			prevpos = m_parts[i].pos; prevq = m_parts[i].q;
			m_parts[i].q =
				Quat::CreateRotationAA(m_susp[iwheel].steer,Vec3(0,0,-1))*Quat::CreateRotationAA(m_susp[iwheel].rot,Vec3(-1,0,0))*m_susp[iwheel].q0;
			ptc1 = m_parts[i].q*m_parts[i].pPhysGeomProxy->origin + m_susp[iwheel].pos0;
			(m_parts[i].pos = m_susp[iwheel].pos0+m_susp[iwheel].ptc0-ptc1).z -= m_susp[iwheel].curlen-m_susp[iwheel].len0;
			status->flags |= status_thread_safe;
		}

		res = CRigidEntity::GetStatus(_status);

		if ((unsigned int)iwheel<(unsigned int)(m_nParts-m_nHullParts)) {
			m_parts[i].pos = prevpos;	m_parts[i].q = prevq;
		}
		AtomicAdd(&m_lockUpdate,-iLock); status->flags = flags;
		return res;
	}

	if (res = CRigidEntity::GetStatus(_status))
		return res;

	if (_status->type==pe_status_vehicle::type_id) {
		pe_status_vehicle *status = (pe_status_vehicle*)_status;
		ReadLock lock(m_lockVehicle);
		status->steer = m_steer;
		status->pedal = m_enginePedal;
		status->bHandBrake = m_bHandBrake;
		status->footbrake = m_enginePedal*sgn(m_iCurGear-1)<=0 ? fabs_tpl(m_enginePedal) : 0;
		status->vel = m_body.v;
		int i; for(i=status->bWheelContact=0; i<m_nParts-m_nHullParts; i++)
			status->bWheelContact += m_susp[i].bContact;
		status->engineRPM = m_wengine*(60.0/(2*g_PI));
		status->iCurGear = m_iCurGear;
		status->clutch = m_clutch;
    status->drivingTorque = m_drivingTorque;
		for(i=status->nActiveColliders=0; i<m_nColliders; i++) 
			if (m_pColliders[i]->m_iSimClass>0 && m_pColliders[i]->GetType()!=PE_ARTICULATED)
				status->nActiveColliders++;
		return 1;
	}

	if (_status->type==pe_status_wheel::type_id) {
		pe_status_wheel *status = (pe_status_wheel*)_status;
		ReadLock lock(m_lockVehicle);
		if (!is_unused(status->partid)) {
			for(status->iWheel=0; status->iWheel<m_nParts-m_nHullParts && m_parts[status->iWheel+m_nHullParts].id!=status->partid; status->iWheel++);
			if (status->iWheel>=m_nParts-m_nHullParts)
				return 0;
		}
		if ((unsigned int)status->iWheel>=(unsigned int)(m_nParts-m_nHullParts))
			return 0;
		status->w = m_susp[status->iWheel].w;
		if (status->bContact = m_susp[status->iWheel].bContact) {
			status->ptContact = m_susp[status->iWheel].ptcontact;
			status->bSlip = m_susp[status->iWheel].bSlip;
			status->velSlip = m_susp[status->iWheel].vrel;
			Vec3 axis(cos_tpl(m_susp[status->iWheel].steer),-sin_tpl(m_susp[status->iWheel].steer),0), pulldir;
			pulldir = m_qrot*(m_susp[status->iWheel].ncontact^axis).normalized();
			status->velSlip -= pulldir*(m_susp[status->iWheel].w*m_susp[status->iWheel].r);
			status->normContact = m_susp[status->iWheel].ncontact;
			status->velSlip -= status->normContact*(status->velSlip*status->normContact);
		} else {
			status->ptContact.zero(); status->velSlip.zero(); status->normContact.zero(); status->bSlip=0;
		}
		if (!m_bAwake) {
			status->w = 0;
			status->velSlip.zero();
		}
		status->contactSurfaceIdx = m_susp[status->iWheel].surface_idx[1];    
		status->suspLen = m_susp[status->iWheel].curlen;
		status->suspLenFull = m_susp[status->iWheel].fullen;
		status->suspLen0 = m_susp[status->iWheel].len0;
		status->r = m_susp[status->iWheel].r;
    status->pCollider = m_susp[status->iWheel].pent;   
    status->friction = m_susp[status->iWheel].contact.friction;
    status->torque = m_susp[status->iWheel].T;
		status->steer = m_susp[status->iWheel].steer;
		return 1;
	}

	if (_status->type==pe_status_vehicle_abilities::type_id) {
		pe_status_vehicle_abilities *status = (pe_status_vehicle_abilities*)_status;
		ReadLock lock(m_lockVehicle);
		int i,nd;
		if (!is_unused(status->steer)) {
			Vec3 pt[2],cm=(m_body.pos-m_pos)*m_qrot;
			for(i=0;i<m_nParts-m_nHullParts;i++) if ((m_susp[i].pt.x-cm.x)*status->steer>0)
				pt[isneg(cm.y-m_susp[i].pt.y)] = m_susp[i].pt;
			pt[0].x = pt[1].x+(pt[1].y-pt[0].y)/tan_tpl(status->steer)*sgn(status->steer);
			status->rotPivot = m_qrot*pt[0]+m_pos;
		}

		float k,wlim[2],w;
		wlim[0]=m_engineMaxw*0.01f; wlim[1]=m_engineMaxw;
		for(i=nd=0;i<m_nParts-m_nHullParts;i++) nd += m_susp[i].bDriving;
		if (nd>0) {
			k=g_PI/m_engineMaxw; i=0;
			do {
				w = (wlim[0]+wlim[1])*0.5f;
				wlim[isneg((sin_tpl(w*k)*m_enginePower-m_axleFriction*w)*nd - sqr(w*m_susp[0].r)*m_dampingVehicle*m_body.M)] = w;
			} while((wlim[1]-wlim[0])>m_engineMaxw*0.005f && ++i<256);
			status->maxVelocity = w*m_susp[0].r;
		} else
			status->maxVelocity = 0;
		return 1;
	}

	return 0;
}


void CWheeledVehicleEntity::RecalcSuspStiffness()
{
	if (m_body.Minv<=0)
		return;
	Vec3 cm=(m_body.pos-m_pos)*m_qrot, sz=(m_BBox[1]-m_BBox[0]), r;
	const int numWheels = m_nParts - m_nHullParts;
	int i,j, *idx = (int*)alloca(numWheels*sizeof(int)); 
	float scale[2]={0.f,0.f}, force[2]={0.f,0.f}, torque[2]={0.f,0.f}, y, w, denom;
	float e = max(max(sz.x,sz.y),sz.z)*0.02f;
	Matrix33 R,Iinv; 
	WriteLock lock(m_lockVehicle);

	R = Matrix33(!m_qrot*m_body.q);
	Iinv = R*m_body.Ibody_inv*R.T();
	for(i=0;i<numWheels;i++) {
		r = m_susp[i].pt-m_body.pos+m_pos;
		m_susp[i].Mpt = 1.0f/(m_body.Minv+(Iinv*Vec3(r.y,-r.x,0)^r).z);
	}

	for(i=0;i<numWheels;i++) m_susp[i].iBuddy = -1;
	// first, force nearly symmetrical wheels to be symmetrical (if they are on the same axle)
	for(i=0;i<numWheels;i++) {
		Vec3 ptc0 = m_susp[i].ptc0;
		for(j=i+1;j<numWheels;j++) if (m_susp[j].iAxle==max(0,(int)m_susp[i].iAxle)) {
			if (fabs_tpl(m_susp[i].pt.y-m_susp[j].pt.y)<e && fabs_tpl(m_susp[i].pt.x+m_susp[j].pt.x)<e) {
				m_susp[j].pt.Set(-m_susp[i].pt.x, m_susp[i].pt.y, m_susp[i].pt.z);
				m_susp[j].ptc0.Set(-m_susp[i].ptc0.x, m_susp[i].ptc0.y, m_susp[i].ptc0.z);
			}
			m_susp[i].iBuddy = j;	m_susp[j].iBuddy = i;
			break;
		}
		if (m_susp[i].iBuddy<0 && m_susp[i].iAxle>=0 && fabs_tpl(m_susp[i].ptc0.x)<e) {
			m_susp[i].pt.x -= m_susp[i].ptc0.x;
			m_susp[i].ptc0.x = 0;
		}
		m_parts[m_nHullParts+i].pos = (m_susp[i].pos += m_susp[i].ptc0-ptc0);
	}
	if (m_wheelMassScale>0)
		RecomputeMassDistribution();

	// Auto calculate the suspension stiffness by balancing the front-back torque
	for(i=0;i<numWheels;i++) {
		y = m_susp[i].pt.y - cm.y;
		w = max(0.f, m_susp[i].kStiffnessWeight)*isneg(-m_susp[i].iAxle-1);
		idx[i] = (m_susp[i].pt.y - cm.y)>=0.f ? 1:0;
		force[idx[i]] += w;
		torque[idx[i]] += y*w;
	}
	// Solve for (force[0] * scale_back  + force[1] * scale_front == 1.0) && (torque[0] * scale_back + torque[1] * scale_front == 0.0)
	denom = torque[1]*force[0] - torque[0]*force[1];
	if (denom>1e-4f) {
		float mg = -m_body.M*m_gravity.z;
		denom = 1.f/denom;
		scale[0] = +mg*torque[1] * denom;
		scale[1] = -mg*torque[0] * denom;
		for(i=0;i<numWheels;i++)
			m_susp[i].kStiffness = (m_susp[i].kStiffnessWeight>0.f) ? (scale[idx[i]] * m_susp[i].kStiffnessWeight) : (-mg * m_susp[i].kStiffnessWeight/(float)numWheels);
	} else for(i=0;i<numWheels;i++)
		m_susp[i].kStiffness = m_susp[i].Mpt*-m_gravity.z;			

	for(i=0;i<numWheels;i++) if (m_susp[i].fullen>0) {
		m_susp[i].kStiffness /= m_susp[i].fullen-m_susp[i].len0;
		if (m_susp[i].kDamping0<=0 && m_susp[i].kStiffness*m_susp[i].Mpt>=0)
			m_susp[i].kDamping = -m_susp[i].kDamping0*sqrt_tpl(4*m_susp[i].kStiffness*m_susp[i].Mpt);
	}	else 
		m_susp[i].kStiffness = m_susp[i].kDamping = 0;
}


int CWheeledVehicleEntity::AddGeometry(phys_geometry *pgeom, pe_geomparams *_params, int id, int bThreadSafe)
{
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

	int res;
	if (_params->type!=pe_cargeomparams::type_id) {
		pe_cargeomparams params = *(pe_geomparams*)_params;
		return AddGeometry(pgeom,&params,id,1);
	}

	pe_cargeomparams *params = (pe_cargeomparams*)_params;
	float density=params->mass!=0 ? params->mass/pgeom->V : params->density;
	int flags0=params->flags, flagsCollider0=params->flagsCollider;
	if (!is_unused(params->bDriving)) {
		if (density<=0) {
			VALIDATOR_LOG(m_pWorld->m_pLog,"CWheeledVehicleEntity::AddGeometry: trying to add a weightless wheel");
			return -1;
		}
		params->flags |= geom_car_wheel;//geom_colltype_player|geom_colltype_ray;
		params->flagsCollider = 0;
		params->density*=m_wheelMassScale; params->mass*=m_wheelMassScale;
	}

	int nPartsOld=m_nParts,i;
	if ((res = CRigidEntity::AddGeometry(pgeom,_params,id,1))<0)
		return res;
	m_parts[m_nParts-1].flags |= geom_monitor_contacts;
	pgeom = m_parts[m_nParts-1].pPhysGeom;

	if (params->bDriving<0) {
		if (m_nParts > nPartsOld) {
			if (m_nHullParts < m_nParts-1) {
				geom defpart;
				memcpy(&defpart, m_parts+m_nParts-1, sizeof(geom));
				memmove(m_parts+m_nHullParts+1, m_parts+m_nHullParts, sizeof(geom)*(m_nParts-m_nHullParts-1));
				memcpy(m_parts+m_nHullParts, &defpart, sizeof(geom));
				for(i=m_nHullParts+1; i<m_nParts; i++) m_parts[i].pNewCoords = (coord_block_BBox*)&m_susp[i-m_nHullParts-1].pos;
				m_parts[m_nHullParts].pNewCoords = (coord_block_BBox*)&m_parts[m_nHullParts].pos;
			}
			m_nHullParts++;
			RecalcSuspStiffness();
		}
	} else {
		int idx = m_nParts-1-m_nHullParts;
		if (idx>=m_suspAlloc) {
			ReallocateList(m_susp, m_suspAlloc, idx+4&~3, true);
			m_suspAlloc = idx+4 & ~3;
			for(i=m_nHullParts; i<m_nParts; i++) m_parts[i].pNewCoords = (coord_block_BBox*)&m_susp[i-m_nHullParts].pos;
		}
		m_susp[idx].bDriving = params->bDriving;
		m_susp[idx].iAxle = params->iAxle;
		m_susp[idx].bCanBrake = params->bCanBrake;
		m_susp[idx].bCanSteer = params->bCanSteer;
		m_susp[idx].bBlocked = 0;
		m_susp[idx].pt = params->pivot;
		m_susp[idx].fullen = params->lenMax;
		m_susp[idx].curlen=m_susp[idx].len0 = params->lenInitial;
		m_susp[idx].bRayCast = is_unused(params->bRayCast) ? 0 : params->bRayCast;
		m_susp[idx].steer = 0;
		m_susp[idx].T=m_susp[idx].w=m_susp[idx].wa=m_susp[idx].rot=m_susp[idx].prevTdt=m_susp[idx].prevw=0;
		m_susp[idx].contact.friction=0;
		m_susp[idx].q0 = params->q;
		m_susp[idx].pos0 = params->pos;
		m_susp[idx].ptc0 = params->q*pgeom->origin + params->pos;
		m_susp[idx].bSlip = m_susp[idx].bSlipPull = 0;
		m_susp[idx].minFriction = is_unused(params->minFriction) ? 0.0f : params->minFriction;
		m_susp[idx].maxFriction = is_unused(params->maxFriction) ? 1.0f : params->maxFriction;
    m_susp[idx].kLatFriction = is_unused(params->kLatFriction) ? 1.f : params->kLatFriction;
		m_susp[idx].Tscale = 1.0f;
		m_susp[idx].pent = 0;
		m_susp[idx].bContact = 0;
		m_susp[idx].ncontact.Set(0,0,1); m_susp[idx].ptcontact.zero();
		m_susp[idx].flags0 = flags0;
		m_susp[idx].flagsCollider0 = flagsCollider0;
		m_susp[idx].kStiffness = m_susp[idx].kDamping = 0;
		m_susp[idx].kStiffnessWeight = params->kStiffnessWeight;
		m_susp[idx].contact.Pspare = 0;
		m_susp[idx].pCollEvent = 0;
		box bbox; pgeom->pGeom->GetBBox(&bbox);
		m_susp[idx].r = max(max(bbox.size.x,bbox.size.y),bbox.size.z);
		m_susp[idx].width = min(min(bbox.size.x,bbox.size.y),bbox.size.z);
		assert(density);
		m_susp[idx].Iinv = 1.0f/(max(max(pgeom->Ibody.x,pgeom->Ibody.y),pgeom->Ibody.z)*density);
		m_susp[idx].rinv = 1.0f/m_susp[idx].r;

		Vec3 r = m_susp[idx].pt-m_body.pos+m_pos;
		Matrix33 R,Iinv; 
		R = Matrix33(!m_qrot*m_body.q);
		Iinv = R*m_body.Ibody_inv*R.T();
		if (m_body.Minv>0)
			m_susp[idx].Mpt = 1.0f/(m_body.Minv+(Iinv*Vec3(r.y,-r.x,0)^r).z);

		m_susp[idx].kDamping0 = params->kDamping;
		m_susp[idx].bSlip = 0;
		m_parts[idx+m_nHullParts].pNewCoords = (coord_block_BBox*)&m_susp[idx].pos;
		m_susp[idx].pos = m_parts[idx+m_nHullParts].pos;
		m_susp[idx].q = m_parts[idx+m_nHullParts].q;
		m_susp[idx].scale = m_parts[idx+m_nHullParts].scale;
		m_susp[idx].BBox[0] = m_parts[idx+m_nHullParts].BBox[0]; 
		m_susp[idx].BBox[1] = m_parts[idx+m_nHullParts].BBox[1];
    m_susp[idx].vrel.zero();
		if (params->kStiffness==0)
			RecalcSuspStiffness();
		else {
			m_susp[idx].kStiffness = params->kStiffness;
			m_susp[idx].kDamping = params->kDamping;
		}
	}
	m_iVarPart0 = m_nHullParts;

	return res;
}


void CWheeledVehicleEntity::RemoveGeometry(int id, int bThreadSafe)
{
	ChangeRequest<void> req(this,m_pWorld,0,bThreadSafe,0,id);
	if (req.IsQueued())
		return;

	int i; for(i=0;i<m_nParts && m_parts[i].id!=id;i++);
	if (i<m_nParts) {
		if (i<m_nHullParts) m_nHullParts--;
		else {
			memmove(m_susp+i-m_nHullParts, m_susp+i-m_nHullParts+1, sizeof(m_susp[0])*(m_nParts-i-1));
			for(; i<m_nParts-1; i++) m_parts[i+1].pNewCoords = (coord_block_BBox*)&m_susp[i-m_nHullParts].pos;
		}
	}
	m_iVarPart0 = m_nHullParts;

	CRigidEntity::RemoveGeometry(id,1);
}


void CWheeledVehicleEntity::AlertNeighbourhoodND(int mode) 
{
	CRigidEntity::AlertNeighbourhoodND(mode);
	for(int i=0;i<m_nParts-m_nHullParts;i++) {
		m_susp[i].bContact=0; m_susp[i].pent=0;
	}
}


void CWheeledVehicleEntity::ComputeBBox(Vec3 *BBox, int flags) 
{ 
	CPhysicalEntity::ComputeBBox(BBox,flags); 
	// inflate the box a bit, because wheels attempt to maintain a safety gap from the ground
	Vec3 infl; infl.x=infl.y=infl.z = m_pWorld->m_vars.maxContactGap*2;
	BBox[0] -= infl; BBox[1] += infl;
}


void CWheeledVehicleEntity::CheckAdditionalGeometry(float time_interval)
{
	CRY_PROFILE_FUNCTION(PROFILE_PHYSICS );

	int iCaller = get_iCaller_int();
	int i,j,nents,ient,iwheel,ncontacts,icont,bRayCast,bHasContacts,ient1,j1,bHasMatSubst=0;
	int bFakeInnerWheels, iwMiny[2]={-1,-1},iwMaxy[2]={-1,-1};
	float tmax,tcur,newlen,tmaxTilted;
	Vec3 r,ptc1,geompos,axis,axisz,axisy,ptcw,dirmax,unproj,n_accum;// axisx=m_qrot*Vec3(1,0,0);
	Vec3 BBoxWheel[2],sz,center;
	quaternionf qsteer;
	geom_world_data gwd[2];
	intersection_params ip;
	CRayGeom aray;
	CPhysicalEntity **pentlist;
	geom_contact *pcontacts;

	nents = m_pWorld->GetEntitiesAround(m_BBoxNew[0],m_BBoxNew[1], pentlist, 
		ent_terrain|ent_static|ent_sleeping_rigid|ent_rigid|ent_triggers, this, 0,get_iCaller_int());
	for(i=j=0;i<nents;i++) if (pentlist[i]!=this) 
		if (pentlist[i]->GetMassInv()<m_body.Minv*5)
			pentlist[j++] = pentlist[i];
		else if (m_bAwake) {
			box abox;
			Vec3 BBoxEnt[2] = { Vec3(),Vec3() };
			r = m_BBox[1]-m_BBox[0]; r.x = max(max(r.x,r.y),r.z);
			BBoxEnt[0]=Vec3(r.x); BBoxEnt[1]=Vec3(-r.x);
			for(j1=0; j1<pentlist[i]->m_nParts; j1++) {
				pentlist[i]->GetPartTransform(j1, gwd[1].offset,gwd[1].R,gwd[1].scale, this);
				pentlist[i]->m_parts[j1].pPhysGeomProxy->pGeom->GetBBox(&abox);
				//gwd[1].R = Matrix33(!m_qNew*pentlist[i]->m_qrot*pentlist[i]->m_parts[j1].q);
				abox.Basis *= gwd[1].R.T() * Matrix33(m_qNew);
				r = (abox.size*abox.Basis.Fabs())*pentlist[i]->m_parts[j1].scale;
				ptcw = (gwd[1].offset + gwd[1].R*abox.center*gwd[1].scale - m_posNew)*m_qNew;
				BBoxEnt[0] = min(BBoxEnt[0], ptcw-r);
				BBoxEnt[1] = max(BBoxEnt[1], ptcw+r);
			}
			for(iwheel=0;iwheel<m_nParts-m_nHullParts;iwheel++) {
				(ptcw = m_susp[iwheel].ptc0).z += m_susp[iwheel].fullen*0.5f-m_susp[iwheel].len0;
				if (m_susp[iwheel].steer)
					axis.x=fabs_tpl(cos_tpl(m_susp[iwheel].steer)),axis.y=fabs_tpl(sin_tpl(m_susp[iwheel].steer));
				else
					axis.x=1,axis.y=0;
				r.x = m_susp[iwheel].width*axis.x+m_susp[iwheel].r*axis.y;
				r.y = m_susp[iwheel].width*axis.y+m_susp[iwheel].r*axis.x;
				r.z = m_susp[iwheel].r;
				BBoxWheel[0] = ptcw-r; BBoxWheel[1] = ptcw+r;
				if (AABB_overlap(BBoxEnt,BBoxWheel)) {
					pentlist[i]->Awake(1,5); break;
				}
			}
			//pentlist[i]->Awake(1,5);
		}
	//if (pentlist[i]!=this && (pentlist[i]->m_iSimClass<2 || pentlist[i]->GetMassInv()<m_body.Minv*5 || 
	//		pentlist[i]->m_nParts==1 && pentlist[i]->GetMassInv()<m_body.Minv*100))
		//pentlist[j++] = pentlist[i];
		// && (pentlist[i]->m_iGroup==m_iGroup && pentlist[i]->m_bMoved || !pentlist[i]->IsAwake() ||
			//m_pWorld->m_pGroupNums[pentlist[i]->m_iGroup]<m_pWorld->m_pGroupNums[m_iGroup]))
		
	nents = j;
	axisz = m_qNew*Vec3(0,0,-1);
	axisy = m_qNew*Vec3(0,1,0);
	//gwd[0].v = m_qrot*Vec3(0,0,-1);
	//ip.bStopAtFirstTri = true;
	//ip.bNoBorder = true;
	ip.vrel_min = 1e10f;
	ip.maxSurfaceGapAngle = 4.0f/180*g_PI;
	ip.bThreadSafe = 1;
	for(i=0,bHasContacts=m_nContacts;i<m_nParts-m_nHullParts;i++)
		bHasContacts += m_susp[i].bContact;

	bFakeInnerWheels = isneg((m_flags & wwef_fake_inner_wheels)*(0.001f-m_maxTimeIdle));
	if (bFakeInnerWheels) for(iwheel=0;iwheel<m_nParts-m_nHullParts;iwheel++) if (m_susp[iwheel].fullen>0) {
		j = isneg(m_susp[iwheel].pt.x);
		iwMiny[j] += iwheel+1 & iwMiny[j]>>31;
		iwMaxy[j] += iwheel+1 & iwMaxy[j]>>31;
		iwMiny[j] += iwheel-iwMiny[j] & -isneg(m_susp[iwheel].pt.y-m_susp[iwMiny[j]].pt.y);
		iwMaxy[j] += iwheel-iwMaxy[j] & -isneg(m_susp[iwMaxy[j]].pt.y-m_susp[iwheel].pt.y);
	}

	for(i=m_nHullParts;i<m_nParts;i++) {
		iwheel = i-m_nHullParts;
		m_parts[i].flags = m_susp[iwheel].flags0|geom_car_wheel;//geom_colltype_player|geom_colltype_ray;	
		m_parts[i].flagsCollider = 0;
		ip.time_interval = m_susp[iwheel].fullen*2.0f;
		if (m_susp[iwheel].steer)
			qsteer = Quat::CreateRotationAA(m_susp[iwheel].steer,Vec3(0,0,-1));
		else qsteer.SetIdentity();
		axis = (m_qNew*qsteer)*Vec3(sgnnz(m_susp[iwheel].pt.x),0,0); // outward wheel rotation axis in world cs
		m_susp[iwheel].q = qsteer*m_susp[iwheel].q0;
		gwd[0].R = Matrix33(m_qNew*m_susp[iwheel].q);
		ptc1 = m_susp[iwheel].q*m_parts[i].pPhysGeomProxy->origin + m_susp[iwheel].pos0;
		ptcw = m_posNew + m_qNew*ptc1;
		(m_susp[iwheel].pos = m_susp[iwheel].pos0+m_susp[iwheel].ptc0-ptc1).z -= max(0.0f,m_susp[iwheel].fullen-m_susp[iwheel].len0);
		geompos = m_posNew + m_qNew*m_susp[iwheel].pos;

		Matrix33 Rabs = Matrix33(m_qNew*qsteer);
		sz = Rabs.Fabs()*Vec3(m_susp[iwheel].width,m_susp[iwheel].r,m_susp[iwheel].r+m_susp[iwheel].fullen);
		(center = m_susp[iwheel].ptc0).z -= m_susp[iwheel].len0;
		center = m_posNew + m_qNew*center;
		BBoxWheel[0] = center-sz; BBoxWheel[1] = center+sz;

		gwd[0].scale = m_parts[i].scale;
		tmax=tmaxTilted = 0; m_susp[iwheel].ptcontact = m_pos;
		m_susp[iwheel].pbody = 0;
		m_susp[iwheel].bContact = 0;
		unproj.zero(); n_accum.zero();

		j = isneg(m_susp[iwheel].pt.x);
		if (bFakeInnerWheels && (m_susp[iwheel].fullen*(iszero(iwheel-iwMiny[j])|iszero(iwheel-iwMaxy[j])))==0.0f)
			continue;

		gwd[0].offset = geompos; bRayCast = 0;
		if (ip.bSweepTest = m_susp[iwheel].fullen>0) {
			if (!m_susp[iwheel].bRayCast)
				gwd[0].offset -= (gwd[0].v = axisz)*(ip.time_interval = m_susp[iwheel].fullen*2.0f);
			else {
				aray.m_ray.origin = ptcw-axisz*(m_susp[iwheel].len0+m_susp[iwheel].fullen)+axis*m_susp[iwheel].width;
				aray.m_ray.dir = (aray.m_dirn=axisz)*(ip.time_interval+m_susp[iwheel].r);
				bRayCast = 1;	ip.bSweepTest = false;
			}
		} else {
			gwd[0].v = m_body.v + (m_body.w ^ m_qrot*m_susp[iwheel].pos+m_posNew-m_body.pos); 
			gwd[0].w = m_body.w;
			gwd[0].centerOfMass = m_body.pos;
			ip.time_interval = time_interval*1.4f;
		}
		Vec3 partBBox[2];

		for(ient=0;ient<nents;ient++) for(j=0; j<pentlist[ient]->m_nParts; j++) 
		if (pentlist[ient]->m_parts[j].flags & m_susp[iwheel].flagsCollider0 && 
				((pentlist[ient]->m_parts[j].BBox[1]-pentlist[ient]->m_parts[j].BBox[0]).len2()==0 || AABB_overlap(pentlist[ient]->GetPartBBox(j,partBBox,this),BBoxWheel))) 
		{
			pentlist[ient]->GetPartTransform(j, gwd[1].offset,gwd[1].R,gwd[1].scale, this);
			if (!bRayCast)
				ncontacts = m_parts[i].pPhysGeomProxy->pGeom->Intersect(pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom, gwd+0,gwd+1, &ip, pcontacts);
			else {
				ip.bSweepTest = false;
				ncontacts = pentlist[ient]->m_parts[j].pPhysGeomProxy->pGeom->Intersect(&aray, gwd+1,0, &ip, pcontacts);
				for(icont=0; icont<ncontacts; icont++)
					pcontacts[icont].n.Flip();
				ip.bSweepTest = true;
			}

			for(icont=0; icont<ncontacts; icont++) {
				pcontacts[icont].t -= m_susp[iwheel].r*bRayCast;
				if (!ip.bSweepTest) {
					if (pcontacts[icont].vel>0) {
						if (!bHasContacts && sqr(pcontacts[icont].t)>unproj.len2())
							unproj = pcontacts[icont].dir*pcontacts[icont].t;
					} else if (pcontacts[icont].n*pcontacts[icont].dir>-0.8f)
						pcontacts[icont].n = -pcontacts[icont].dir;
				}
				tcur = ip.bSweepTest ? ip.time_interval-pcontacts[icont].t : pcontacts[icont].t*-(pcontacts[icont].dir*axisz);
				n_accum += pcontacts[icont].n*-tcur;
				if (fabs_tpl(pcontacts[icont].n*axisy) < m_maxTilt) {
					if (tcur>tmax/*&& (ip.bSweepTest || pcontacts[icont].vel>0)*/)	{
						tmax = tcur; dirmax = pcontacts[icont].dir; m_susp[iwheel].bContact = 1;
						m_susp[iwheel].pent = pentlist[ient]; m_susp[iwheel].ipart = j;
						m_susp[iwheel].pbody = pentlist[ient]->GetRigidBody(j);
						m_susp[iwheel].surface_idx[0] = GetMatId(pcontacts[icont].id[0],i);
						m_susp[iwheel].surface_idx[1] = pentlist[ient]->GetMatId(pcontacts[icont].id[1],j);
						// project contact point to the outer wheel edge if the wheel has a buddy on the opposite side of the axle, and to the center otherwise
						m_susp[iwheel].ptcontact = pcontacts[icont].pt + axis*(m_susp[iwheel].width*isneg(-m_susp[iwheel].iBuddy-1)-axis*(pcontacts[icont].pt-ptcw)); 
						m_susp[iwheel].ncontact = -pcontacts[icont].n;
						int scMask = (2<<m_susp[iwheel].pent->m_iSimClass)-(m_susp[iwheel].pent->m_id>>31);
						if (bHasMatSubst & scMask) for(ient1=0;ient1<nents;ient1++) for(int j2=0;j2<pentlist[ient1]->GetUsedPartsCount(iCaller);j2++)
							if (pentlist[ient1]->m_parts[j1=pentlist[ient1]->GetUsedPart(iCaller,j2)].flags & geom_mat_substitutor && 
									pentlist[ient1]->m_parts[j1].flagsCollider & scMask &&
									pentlist[ient1]->m_parts[j1].pPhysGeom->pGeom->PointInsideStatus(
										((m_susp[iwheel].ptcontact-pentlist[ient1]->m_pos)*pentlist[ient1]->m_qrot-pentlist[ient1]->m_parts[j1].pos)*pentlist[ient1]->m_parts[j1].q))
							m_susp[iwheel].surface_idx[1] = pentlist[ient1]->GetMatId(pentlist[ient1]->m_parts[j1].pPhysGeom->surface_idx,j1);
					}
					AddCollider(pentlist[ient]);
				} else if (pcontacts[icont].n*axisz > -0.01f && // if contact normal is too steep (but not negative), 
									pentlist[ient]->GetRigidBody(j)->Minv < m_body.Minv*10.f && // ..and colliding body is not loo light (too avoid degeneracy)
									(m_kSteerToTrack==0 || m_susp[iwheel].fullen==0)) // never do it for tank's road wheels
					tmaxTilted = max_safe(tmaxTilted,tcur);
			}
		} else 
			bHasMatSubst |= pentlist[ient]->m_parts[j].flagsCollider & -((int)pentlist[ient]->m_parts[j].flags & geom_mat_substitutor)>>31;

		//endwheel:
		if (tmaxTilted>tmax*1.01f) {
			m_parts[i].flags = m_susp[iwheel].flags0; m_parts[i].flagsCollider = m_susp[iwheel].flagsCollider0;
			m_susp[iwheel].bContact &= m_bKeepTractionWhenTilted; // treat wheel as part of vehicle's rigid geometry						
		}
		m_posNew += unproj;
		m_body.pos += unproj;
		newlen = m_susp[iwheel].fullen;

		if (m_susp[iwheel].bContact) {
			m_susp[iwheel].vrel = m_body.v+(m_body.w^m_susp[iwheel].ptcontact-m_body.pos);
			if (m_susp[iwheel].pbody)
				m_susp[iwheel].vrel -= m_susp[iwheel].pbody->v + (m_susp[iwheel].pbody->w^m_susp[iwheel].ptcontact-m_susp[iwheel].pbody->pos);
			DetachPartContacts(i,0, m_susp[iwheel].pent,1, tmax==0);
			m_susp[iwheel].pent->DetachPartContacts(i,1, this,1);

			if (ip.bSweepTest) {
				if (tmax>0) 
					tmax += m_pWorld->m_vars.maxContactGap*0.5f;
				newlen = max(0.0f,m_susp[iwheel].fullen-tmax);
				if (tmax>m_susp[iwheel].fullen) { // spring is fully compressed, but we still have penetration
					tmax -= m_susp[iwheel].fullen;
					m_susp[iwheel].contact.vreq = axisz*-min(tmax*m_pWorld->m_vars.unprojVelScale*2, m_pWorld->m_vars.maxUnprojVel*0.7f);
				}	else
					m_susp[iwheel].contact.vreq.zero();
			} else {
				m_susp[iwheel].ncontact = n_accum.normalized();
				m_susp[iwheel].contact.vreq = (dirmax-axis*(axis*dirmax))*min(m_pWorld->m_vars.maxUnprojVel*0.7f,
					max(0.0f,(tmax-m_pWorld->m_vars.maxContactGap))*m_pWorld->m_vars.unprojVelScale*2);
			}
		}
		m_susp[iwheel].curlen = min(newlen, m_susp[iwheel].curlen+time_interval*7.0f);
		m_susp[iwheel].pos.z += m_susp[iwheel].fullen-m_susp[iwheel].curlen;
		m_parts[iwheel+m_nHullParts].pos = m_susp[iwheel].pos;
	}

	if (bFakeInnerWheels) {
		assert(iwMaxy[0] > 0);
		PREFAST_ASSUME(iwMaxy[0] > 0);
		float rspan[2] = { 1.0f/max(1e-6f,m_susp[iwMaxy[0]].pt.y-m_susp[iwMiny[0]].pt.y), 1.0f/max(1e-6f,m_susp[iwMaxy[1]].pt.y-m_susp[iwMiny[1]].pt.y) };
		for(iwheel=0; iwheel<m_nParts-m_nHullParts; iwheel++)
			if (j=isneg(m_susp[iwheel].pt.x), m_susp[iwheel].fullen>(iszero(iwheel-iwMiny[j])|iszero(iwheel-iwMaxy[j]))*2*m_susp[iwheel].fullen) {
				float kmin = (m_susp[iwMaxy[j]].pt.y-m_susp[iwheel].pt.y)*rspan[j];
				float kmax = (m_susp[iwheel].pt.y-m_susp[iwMiny[j]].pt.y)*rspan[j];
				i = iwMiny[j]+(iwMaxy[j]-iwMiny[j] & -isneg(kmin-kmax));
				if (!m_susp[i].bContact)
					continue;

				m_susp[iwheel].bContact = 1;
				m_susp[iwheel].pent = m_susp[i].pent; m_susp[iwheel].ipart = m_susp[i].ipart;
				m_susp[iwheel].pbody = m_susp[i].pbody;
				m_susp[iwheel].surface_idx[0] = m_susp[i].surface_idx[0];
				m_susp[iwheel].surface_idx[1] = m_susp[i].surface_idx[1];
				m_susp[iwheel].ncontact = m_susp[i].ncontact;

				DetachPartContacts(i,0, m_susp[iwheel].pent,1, 0);
				m_susp[iwheel].pent->DetachPartContacts(i,1, this,1);
				m_susp[iwheel].curlen = m_susp[iwMiny[j]].curlen*kmin + m_susp[iwMaxy[j]].curlen*kmax;
				Vec3 ptcontact = m_susp[iwheel].pt; 
				ptcontact.z -= m_susp[iwheel].curlen;
				ptcontact.x += m_susp[iwheel].width*(1-j*2);
				m_susp[iwheel].ptcontact = m_posNew + m_qNew*ptcontact;
				m_susp[iwheel].pos.z += m_susp[iwheel].fullen-m_susp[iwheel].curlen;
				m_susp[iwheel].vrel = m_body.v+(m_body.w^m_susp[iwheel].ptcontact-m_body.pos);
				if (m_susp[iwheel].pbody)
					m_susp[iwheel].vrel -= m_susp[iwheel].pbody->v + (m_susp[iwheel].pbody->w^m_susp[iwheel].ptcontact-m_susp[iwheel].pbody->pos);
				m_susp[iwheel].contact.vreq.zero();
				m_parts[iwheel+m_nHullParts].pos = m_susp[iwheel].pos;
			}
	}

	//m_qrot.Normalize();
 	//m_body.pos = m_pos+m_qrot*m_body.offsfb;
 	//m_body.q = m_qrot*!m_body.qfb;
 	//m_body.UpdateState();
}


/*void calc_lateral_limits(float cosa,float sina, float fspring,float mue, quotient &frmin,quotient &frmax)
{
	frmin.set(-mue*cosa-sina,cosa-mue*sina).fixsign();
	frmax = min(quotientf(cosa,sina), quotientf(mue*cosa-sina,cosa+mue*sina));
	frmin.x *= fspring; frmax.x *= fspring;
}*/


float CWheeledVehicleEntity::ComputeDrivingTorque(float time_interval)
{
	if (m_nGears==0) return 0.f;

	float wwheel=0,T=0,enginePower,power,w,wscale[2]={1,1},rTscale;
	int i,iNewGear,nContacts,sg=sgnnz(m_gears[m_iCurGear]);
	if (m_kSteerToTrack!=0 && fabs_tpl(m_steer)>0.01f) {
		wscale[i=isneg(m_steer)] = max(-1.0f,1.0f-fabs_tpl(m_steer*m_kSteerToTrack));
		if (fabs_tpl(wscale[i])<0.05f) wscale[i] = sgnnz(wscale[i])*0.05f;
		wscale[i] = 1/wscale[i];
	}

	for(i=0,rTscale=0;i<m_nParts-m_nHullParts;i++) if (m_susp[i].iAxle>=0 && m_susp[i].bDriving) {
		w = m_susp[i].w*wscale[isneg(m_susp[i].pt.x)]*m_susp[i].Tscale;
		wwheel = max(wwheel,w*m_gears[m_iCurGear]); rTscale += m_susp[i].Tscale*m_susp[i].bDriving;
	}
	rTscale = 1.0f/max(0.001f,rTscale);
	//wwheel *= rTscale; 
	enginePower = m_enginePower*rTscale;

	if (m_iCurGear!=1) {
		if (m_clutch>0)
			m_wengine += (wwheel-m_wengine)*(m_clutchSpeed*2*time_interval);
		if (fabs_tpl(m_wengine)>m_engineMinw) {//m_enginePedal!=0) {
			m_clutch += time_interval*m_clutchSpeed;
			if (m_clutch>1.0f)
				m_clutch=1.0f, m_wengine=wwheel; // ful clutch
		}
	}

	// if the engine goes below min RPM - set neutral gear
	if (m_clutch>0 && m_wengine<m_engineMinw) {
		if (m_enginePedal*(m_iCurGear-1)<=0) 
			m_iCurGear=1, m_clutch=0; // rpm < min, setting neutral gear
		m_wengine = m_engineIdlew; // engine RPM's too low. switching to gear %d. clutch disengaged   
	}
	
	// if pedal is on, but gear is neutral - set gear 0/2 and rev engine m_engineStartw, disengage clutch 
	iNewGear = m_iCurGear;
	for(i=nContacts=0;i<m_nParts-m_nHullParts;i++) nContacts += m_susp[i].bContact;
	if (m_enginePedal!=0 && m_iCurGear==1 && wwheel*-sgnnz(m_enginePedal)<m_gearDirSwitchw) {
		iNewGear = isneg(m_enginePedal)*2^2;
		m_wengine = m_engineStartw;	// setting startRPM
	} else if (m_clutch>0.99f && m_iCurGear>1 && nContacts>1) {
		if (m_wengine>m_engineShiftUpw)
			iNewGear = min(iNewGear+1,m_nGears-1);
		else if (m_wengine<m_engineShiftDownw && (m_enginePedal<=0 || m_iCurGear>2))
			iNewGear = max(iNewGear-1,1);
	}
	iNewGear = max((int)m_minGear,min((int)m_maxGear, (int)iNewGear));
	if (iNewGear!=m_iCurGear)	{
		m_clutch = 0;	// switching to gear iNewGear. clutch disengaged
    if (iNewGear==1)
      m_wengine = m_engineIdlew;
	}
	m_iCurGear = iNewGear;

	if (m_enginePedal*sgn(m_iCurGear-1)>0) {
		if (m_wengine>0.1f) {
			power = sin_tpl(min(m_wengine,m_engineMaxw*1.5f)/m_engineMaxw*g_PI)*enginePower*fabs_tpl(m_enginePedal);
			T = power/m_wengine;
		} else
			T = fabs_tpl(m_enginePedal)*enginePower*(g_PI/m_engineMaxw);
		T *= m_gears[m_iCurGear]*m_clutch;
	}

	return m_drivingTorque = T;
}


/*const int NH=256;
CWheeledVehicleEntity *g_whist[NH],*g_whist2[NH];
CStream g_wsnap[NH];
entity_contact *g_conthist[NH],*g_conthist2[NH];
masktype *g_collhist[NH],*g_collhist2[NH];
int g_iwhist=0,g_checksum[NH],g_ncompare=0,g_bstartcompare=0,g_histinit=0,g_iwhist0=-1;
int g_forcepedal = 0;*/


void CWheeledVehicleEntity::AddAdditionalImpulses(float time_interval)
{
	int i,j,/*idx[NMAXWHEELS],nContacts,*/bAllSlip=1,iDriver[2],bContact[2],iside;//,i1,j1,nfr=0,nfr1,slide[16];
	float fN,friction,kLatFriction,Npull,N,fpull,driving_torque=0,minfric,maxfric,wengine,wground,wground_avg[2],Npull_tot[2];
	int iCaller = get_iCaller_int();
	//quotient frmin[16],frmax[16];
	//real buf[NMAXWHEELS*(NMAXWHEELS+2)];
	Vec3 dP,Pexp,Lexp,ncontact,vdir,pulldir,ptc,axis,axisz,axisx,r,vel_slip;//,frdir[16];
	Matrix33 R = Matrix33(m_qNew), Ctmp;
	pe_action_impulse ai;
	ai.iSource = 3;
	axisz = R*Vec3(0,0,1); axisx = R*Vec3(1,0,0);
//	cur_power = m_enginePedal>0 ? m_enginePower : m_enginePowerBack;
//if (g_forcepedal) m_enginePedal = 1;

	for(i=0;i<m_nParts-m_nHullParts;i++) if (m_susp[i].bContact) {
		AddCollider(m_susp[i].pent); 
		if (!(m_susp[i].pent->m_bProcessed & 1<<iCaller))
			AtomicAdd(&m_susp[i].pent->m_bProcessed, 1<<iCaller);
	}
	for(i=m_nColliders-1;i>=0;i--) 
		if (!(m_pColliders[i]->m_bProcessed & 1<<iCaller) && !m_pColliderContacts[i] && !m_pColliderConstraints[i] && 
				!m_pColliders[i]->HasContactsWith(this)) 
			RemoveCollider(m_pColliders[i]);
	for(i=0;i<m_nParts-m_nHullParts;i++) if (m_susp[i].bContact && m_susp[i].pent->m_bProcessed & 1<<iCaller)
		AtomicAdd(&m_susp[i].pent->m_bProcessed, -(1<<iCaller));

	m_nContacts = 0;
	driving_torque = ComputeDrivingTorque(time_interval);

	// evolve wheels
	for(i=0;i<m_nParts-m_nHullParts;i++) {
		m_susp[i].T = -sgn(m_susp[i].w)*m_axleFriction;
		if (m_enginePedal*sgn(m_iCurGear-1)<=0)    
      m_susp[i].T += m_brakeTorque*m_enginePedal;    
		/*if (m_enginePedal*m_susp[i].bDriving==0) {
			if (m_susp[i].w*m_susp[i].prevw<=0.0f)
				m_susp[i].bBlocked = 1; 
			quotientf T0(-m_susp[i].w*m_susp[i].prevTdt*0.1f,(m_susp[i].w-m_susp[i].prevw)*time_interval);
			if (T0.x*T0.y*m_susp[i].T>0 && fabsf(m_susp[i].T*T0.y)>fabsf(T0.x))
				m_susp[i].T = T0.val();
		}*/
		m_susp[i].prevTdt = m_susp[i].T*time_interval;
		m_susp[i].prevw = m_susp[i].w;
		if (m_susp[i].bDriving)	{      
      float t = driving_torque;
      if (m_kSteerToTrack!=0 && fabs_tpl(m_steer)>0.01f && m_steer*m_susp[i].pt.x>0) {
        if (m_susp[i].w*m_enginePedal>=0) // for 'braking' use max(brakingTorque,drivingTorque) 
          t = sgnnz(driving_torque)*max(m_brakeTorque,abs(driving_torque));
        t *= max(-1.0f,1.0f-fabs_tpl(m_steer*m_kSteerToTrack));
        t *= sgn(m_engineMaxw-m_wengine); // correct sign for negative torque due to rpm>max                 
      }      
      m_susp[i].T += t*m_susp[i].Tscale;
    }
		if ((m_bHandBrake & m_susp[i].bCanBrake)|m_susp[i].bBlocked)
			m_susp[i].w = 0;
		bAllSlip &= m_susp[i].bSlipPull|m_susp[i].bDriving^1;
		m_susp[i].bSlipPull = m_susp[i].bContact^1;
		m_susp[i].rot += m_susp[i].w*time_interval;
		if (m_susp[i].rot>2*g_PI) m_susp[i].rot-=2*g_PI;
		if (m_susp[i].rot<-2*g_PI) m_susp[i].rot+=2*g_PI;
	}
	wengine = (bAllSlip ? m_engineMaxw : min(m_wengine,m_engineMaxw));
	wengine /= max(0.01f,fabs_tpl(m_gears[m_iCurGear]))*sgnnz(m_gears[m_iCurGear]);

	Pexp.zero(); Lexp.zero(); //nContacts=0;
	for(i=0;i<m_nParts-m_nHullParts;i++) {
		m_susp[i].rworld = R*m_susp[i].pt+m_posNew-m_body.pos;
		m_susp[i].vworld = (m_body.v+(m_body.w^m_susp[i].rworld))*axisz;
		if (m_susp[i].bContact && m_susp[i].iAxle>=0) {
			fN = (m_susp[i].fullen-m_susp[i].curlen)*m_susp[i].kStiffness;
			fN -= m_susp[i].vworld*m_susp[i].kDamping;
			if (m_susp[i].iBuddy>=0 && m_kStabilizer>0)
				fN -= (m_susp[i].curlen-m_susp[m_susp[i].iBuddy].curlen)*m_susp[i].kStiffness*m_kStabilizer;
			m_susp[i].PN = max(0.0f,fN*time_interval);
			dP = axisz*m_susp[i].PN; Pexp += dP; Lexp += m_susp[i].rworld^dP;
//			idx[nContacts++] = i;
		}
	}

	/*if (m_iIntegrationType) { // solve for (delta)v = (I-AK)^-1*(A*(Pexplicit - ks*v*dt^2 + Pexternal))
		matrix I_AK(nContacts,nContacts,0,buf+nContacts*2);
		vectorn delta_v_src(nContacts,buf),delta_v_dst(nContacts,buf+nContacts);
		for(i=0;i<nContacts;i++) {
			dP = axisz*(-m_susp[idx[i]].vworld*m_susp[idx[i]].kStiffness*sqr(time_interval));
			if (m_susp[idx[i]].iBuddy && m_kStabilizer>0)
				dP += axisz*((m_susp[m_susp[idx[i]].iBuddy].vworld-m_susp[idx[i]].vworld)*m_susp[idx[i]].kStiffness*m_kStabilizer*sqr(time_interval));
			Pexp += dP; Lexp += m_susp[idx[i]].rworld^dP;
		}

		Pexp += (m_Ffriction+m_body.Fcollision)*time_interval; 
		Pexp += (m_nColliders ? m_gravity : m_gravityFreefall)*(time_interval*m_body.M);
		Lexp += (m_Tfriction+m_body.Tcollision-(m_body.w^m_body.L))*time_interval;
		Pexp = Pexp*m_body.Minv+m_gravity*time_interval; Lexp = m_body.Iinv*Lexp;	// Pexp = body delta v, Lexp = body delta w

		for(i=0;i<nContacts;i++) {
			for(j=0;j<nContacts;j++) { // build I - A*(-ks*dt^2-kd*dt) matrix
				float kStabilizer = m_kStabilizer*(isneg(m_susp[idx[j]].iBuddy)^1);
				I_AK[i][j] = iszero(i-j) + (m_body.Minv - (m_susp[idx[i]].rworld^(m_body.Iinv*(m_susp[idx[j]].rworld^axisz)))*axisz)*
					(m_susp[idx[j]].kStiffness*(1.0f+kStabilizer)*time_interval+m_susp[idx[j]].kDamping)*time_interval;
				if (m_susp[idx[j]].iBuddy>=0 && m_kStabilizer>0)
					I_AK[i][j] -= (m_body.Minv - (m_susp[idx[i]].rworld^(m_body.Iinv*(m_susp[m_susp[idx[j]].iBuddy].rworld^axisz)))*axisz)*
						m_susp[idx[j]].kStiffness*m_kStabilizer*sqr(time_interval);
			}
			delta_v_src[i] = (Pexp+(Lexp^m_susp[idx[i]].rworld))*axisz; // compute delta v along each spring
		}

		I_AK.invert();
		mul_vector_by_matrix(I_AK,delta_v_src.data,delta_v_dst.data);

		// now compute normal impulses that correspond to the computed delta_v
		for(i=0;i<nContacts;i++) {
			m_susp[idx[i]].vworld += delta_v_dst[i];
			fN = (m_susp[idx[i]].fullen-m_susp[idx[i]].curlen-m_susp[idx[i]].vworld*time_interval)*m_susp[i].kStiffness;
			fN -= m_susp[idx[i]].vworld*m_susp[idx[i]].kDamping;
			if (m_susp[i].iBuddy>=0 && m_kStabilizer>0)
				fN -= (m_susp[i].curlen-m_susp[m_susp[i].iBuddy].curlen+(m_susp[idx[i]].vworld-m_susp[m_susp[idx[i]].iBuddy].vworld)*time_interval)*
							m_susp[i].kStiffness*m_kStabilizer;
			m_susp[idx[i]].PN = fN*time_interval;
		}
	}*/

	Pexp.zero(); Lexp.zero();
	wground_avg[0]=wground_avg[1] = 0; Npull_tot[0]=Npull_tot[1] = 0;
	bContact[0]=bContact[1] = 0; iDriver[0]=iDriver[1] = 0;

	for(i=iside=0;i<m_nParts-m_nHullParts;i++) 
	if ((m_kSteerToTrack!=0 && bContact[iside=isneg(m_susp[i].pt.x)] && m_susp[i].bDriving || m_susp[i].bContact) && m_susp[i].iAxle>=0) {
		axis = R*Vec3(cos_tpl(m_susp[i].steer),-sin_tpl(m_susp[i].steer),0);
		iDriver[iside] |= i & -(int)m_susp[i].bDriving;

		if (m_susp[i].bContact) {
			dP = axisz*m_susp[i].PN; Pexp += dP; Lexp += m_susp[i].rworld^dP;
			j = (m_bHandBrake & m_susp[i].bCanBrake)|m_susp[i].bBlocked;
			maxfric = m_maxBrakingFriction*j + (j^1)*10.0f;
      minfric = m_minBrakingFriction*j - (j^1)*10.0f;
			friction = min(m_susp[i].maxFriction, max(m_susp[i].minFriction, 
				m_pWorld->GetFriction(m_susp[i].surface_idx[0], m_susp[i].surface_idx[1])));
			friction = max(minfric, min(maxfric,friction));

			pulldir = (m_susp[i].ncontact^axis).normalize();
			wground = (m_susp[i].vrel*pulldir)*m_susp[i].rinv;
			vel_slip = m_susp[i].vrel-pulldir*(m_susp[i].w*m_susp[i].r);
			if (m_susp[i].bSlip = vel_slip.len2()>sqr(m_slipThreshold))
				friction *= m_kDynFriction;
			Npull = N = (m_susp[i].PN+iszero(m_susp[i].fullen)*m_susp[i].contact.Pspare)*friction;			
      if (m_susp[i].bSlip && m_kSteerToTrack==0)
				Npull *= fabs_tpl(vel_slip.normalized()*pulldir);
			if (m_kSteerToTrack!=0)	if (!m_susp[i].bDriving) {
				bContact[iside]++; wground_avg[iside] += wground; Npull_tot[iside] += Npull;
			} else {
				wground = wground_avg[iside]/max(1,bContact[iside]);
				Npull = max(Npull, Npull_tot[iside]);
				pulldir = R*(m_enginePedal>=0 ? Vec3(0,cos_tpl(m_pullTilt),-sin_tpl(m_pullTilt)) : Vec3(0,1,0));      
			}

			if (m_susp[i].pent->GetRigidBody(m_susp[i].ipart)->Minv<m_body.Minv*10.0f) {
				ai.point = m_susp[i].ptcontact;
				ai.impulse = -dP;
				ai.ipart = m_susp[i].ipart;
				m_susp[i].pent->Action(&ai);
			}
		}	else {
			wground = wground_avg[iside]/max(1,bContact[iside]);
			Npull = Npull_tot[iside];			
      pulldir = R*(m_enginePedal>=0 ? Vec3(0,cos_tpl(m_pullTilt),-sin_tpl(m_pullTilt)) : Vec3(0,1,0));      
		}

		if ((m_bHandBrake & m_susp[i].bCanBrake)|m_susp[i].bBlocked) {
			/*if (m_susp[i].bSlip) { // the wheel slips, use rel. velocity direction as primary friction axis
				vdir = m_susp[i].vrel - m_susp[i].ncontact*(m_susp[i].vrel*m_susp[i].ncontact);
				frdir[nfr] = -vdir.normalized(); frmax[nfr] = N; idx[nfr++] = i;
				frdir[nfr] = frdir[nfr-1]^m_susp[i].ncontact; frmax[nfr] = N*0.3f; idx[nfr++] = i;
			} else {*/
			/*frdir[nfr] = axisz^axis; frdir[nfr] *= -sgnnz(frdir[nfr]*m_susp[i].ncontact);
			sina = pulldir*axisz;
			cosa = sqrt_tpl(max(0.0f,1.0f-sina*sina));
			calc_lateral_limits(cosa,sina, N*=0.85f,friction, frmin[nfr],frmax[nfr]);
			idx[nfr] = i;
			nfr += isneg(m_body.M*1E-6f*frmax[nfr].y-frmax[nfr].x);*/

			if (m_susp[i].curlen>0)	{
				//m_susp[i].contact.C.SetIdentity();
				//m_susp[i].contact.C -= dotproduct_matrix(axisz,axisz, Ctmp);
				m_susp[i].contact.nloc = axisz;
				m_susp[i].contact.flags = contact_wheel|contact_use_C_2dof;
			}	else
				m_susp[i].contact.flags = 0;
			kLatFriction = 1.0f;
		} else {
			ptc = R*(m_susp[i].ptc0-Vec3(0,0,m_susp[i].curlen-m_susp[i].len0))+m_posNew - m_body.pos;
			fpull = m_susp[i].T*m_susp[i].rinv*time_interval;
			if (isneg(fabs_tpl(Npull)-fabs_tpl(fpull)) & m_susp[i].bDriving) {
				m_susp[i].bSlipPull = 1;
				fpull = sgn(fpull)*Npull;
				if (m_gears[m_iCurGear]*m_enginePedal>0) {
					float wengineLoc = wengine*(m_steer*m_susp[i].pt.x>0 ? fabs_tpl(max(-1.0f,1.0f-fabs_tpl(m_steer*m_kSteerToTrack))) : 1.0f);
					m_susp[i].w = m_susp[i].w*(1-time_interval*4)+wengineLoc*time_interval*4;
					fpull *= isneg(wground*wengineLoc-sqr(wengineLoc)*0.9f);
				} else
					m_susp[i].w = wground;
				//if (bAllSlip)
				//	m_susp[i].w += (fpull-sgn(fpull)*Npull)*m_susp[i].r*m_susp[i].Iinv;
				//if (m_susp[i].w*wground<0 || m_susp[i].w*m_susp[i].T<0 || fabs_tpl(wground)>fabs_tpl(m_susp[i].w))
				//	m_susp[i].w = wground;
				//wground = m_engineMaxw*(m_steer*m_susp[i].pt.x>0 ? fabs_tpl(max(-1.0f,1.0f-fabs_tpl(m_steer*m_kSteerToTrack))) : 1.0f);
				//if (fabs_tpl(m_susp[i].w)*m_gears[m_iCurGear] > wground)
        //  m_susp[i].w = sgnnz(m_susp[i].w)*wground/m_gears[m_iCurGear];
			} else
				m_susp[i].w = wground;
			dP = pulldir*fpull; //dP = axisz^axis; dP *= (pulldir*dP)*fpull; 
			Pexp += dP; Lexp += ptc^dP;
			N = sqrt_tpl(max(0.0f,sqr(N)-sqr(fpull)));

			if (m_susp[i].curlen*m_susp[i].fullen>0)	{
				//dotproduct_matrix(axis,axis, m_susp[i].contact.C);
				m_susp[i].contact.nloc = axis;
				m_susp[i].contact.flags = contact_wheel|contact_use_C_1dof;
			} else {
				//m_susp[i].contact.C.SetIdentity();
				//m_susp[i].contact.C -= dotproduct_matrix(pulldir,pulldir, Ctmp);
				m_susp[i].contact.nloc = pulldir;
				m_susp[i].contact.flags = contact_use_C_2dof;
			}
			kLatFriction = m_susp[i].kLatFriction;
		}

		if (m_susp[i].bContact) {
			m_susp[i].contact.flags |= contact_preserve_Pspare;
			m_susp[i].contact.pt[0]=m_susp[i].contact.pt[1] = m_susp[i].ptcontact;
			m_susp[i].contact.n = m_susp[i].ncontact;
			//m_susp[i].contact.K.SetZero();
			//GetContactMatrix(m_susp[i].ptcontact, m_nHullParts+i, m_susp[i].contact.K);
			//m_susp[i].pent->GetContactMatrix(m_susp[i].ptcontact, m_susp[i].ipart, m_susp[i].contact.K);
			m_susp[i].contact.friction = friction*kLatFriction;
			m_susp[i].contact.Pspare = kLatFriction*max(0.0f,(axisz*m_susp[i].ncontact)*N);
			// vreq is filled in CheckAdditionalGeometry
			m_susp[i].contact.pbody[0] = &m_body;
			m_susp[i].contact.pbody[1] = m_susp[i].pbody;
			m_susp[i].contact.pent[0] = this;
			m_susp[i].contact.pent[1] = m_susp[i].pent;
			m_susp[i].contact.ipart[0] = i+m_nHullParts;
			m_susp[i].contact.ipart[1] = m_susp[i].ipart;
		}
		/*frdir[nfr] = axis; frdir[nfr] *= -sgnnz(frdir[nfr]*m_susp[i].ncontact);
		cosa = axisz*m_susp[i].ncontact;
		sina = sqrt_tpl(max(0.0f,1.0f-cosa*cosa));
		calc_lateral_limits(cosa,sina, N,friction, frmin[nfr],frmax[nfr]);
		idx[nfr] = i;
		nfr += isneg(m_body.M*1E-6f*frmax[nfr].y-frmax[nfr].x);
		m_susp[i].bSlip = 0;*/
	} else if (bAllSlip)
		m_susp[i].w += m_susp[i].T*m_susp[i].Iinv*time_interval;

	if (m_kSteerToTrack!=0) for(i=0;i<m_nParts-m_nHullParts;i++) {
		m_susp[i].w = m_susp[iDriver[iside = isneg(m_susp[i].pt.x)]].w;
		m_susp[i].bSlipPull = m_susp[iDriver[iside]].bSlipPull;
	}

	if (m_wheelMassScale) for(i=0;i<m_nParts-m_nHullParts;i++) {
		axis = ortx;
		if (fabs_tpl(m_susp[i].steer)>0.01f)
			sincos_tpl(-m_susp[i].steer, &axis.y,&axis.x);
		Lexp -= (m_qNew*axis-m_body.q*m_body.qfb*axis)*(-m_susp[i].w*m_wheelMassScale/m_susp[i].Iinv);
	}

	m_body.v = (m_body.P+=Pexp)*m_body.Minv; 
	m_body.w = m_body.Iinv*(m_body.L+=Lexp);
	// m_Ffriction.zero(); m_Tfriction.zero();

	/*if (nfr>0) {
		matrix A(nfr,nfr,mtx_PSD | mtx_symmetric,buf);
		vectorn v(nfr,buf+nfr*nfr),p(nfr,buf+nfr*(nfr+1)),v1(nfr,buf+nfr*(nfr+2)),p1(nfr,buf+nfr*(nfr+3)),
			dv(nfr,buf+nfr*(nfr+5)),dp(nfr,buf+nfr*(nfr+6));
		float sign; int d,prev_j,bSliding=0;
		quotient s,s1,sd;	real sval;
		float Ebefore=CalcEnergy(0),Eafter,damping;
		Vec3 Pbefore,Lbefore;
		int iter;

		for(i=0;i<nfr;i++) {
			r = m_susp[idx[i]].ptcontact-m_body.pos; 
			v[i] = (m_body.v + (m_body.w^r) - m_susp[idx[i]].pbody->v - 
				(m_susp[idx[i]].pbody->w^m_susp[idx[i]].ptcontact-m_susp[idx[i]].pbody->pos))*frdir[i];
			v[i] += ((m_body.Fcollision*m_body.Minv+(m_body.Iinv*m_body.Tcollision^r))*frdir[i])*time_interval;
			for(j=i;j<nfr;j++)
				A[i][j] = A[j][i] = frdir[i]*(frdir[j]*m_body.Minv + (m_body.Iinv*(m_susp[idx[j]].ptcontact-m_body.pos^frdir[j])^r));
			v1[i] = -v[i]; slide[i] = 0; 
			if (frmax[i].x*frmin[i].x>0 && frmax[i].y*frmin[i].y>1E-6) {
				p[i] = (frmax[i]+frmin[i]).val()*0.5; bSliding = 1;
			} else p[i] = 0;
		}
		if (bSliding)
			v += A*p;

		// first, try to drive all relative velocities to zero and see if we exceed friction limits
		p1.zero(); A.conjugate_gradient(p1,v1,0,0.0001);
		for(i=bSliding=0;i<nfr;i++) bSliding += isneg(frmax[i]-fabs_tpl(p1[i]));
		if (!bSliding) {
			for(i=0;i<nfr;i++) p[i] = p1[i];
			d = nfr;
		} else // .. invoke LCP solver otherwise
		for(d=iter=0;d<nfr;d++) if (!slide[d]) {
			prev_j=-1; do {
			for(i=nfr1=0;i<d;i++) nfr1 += iszero(slide[i]);
			sign = -sgn(v[d]); dp.zero();
			if (nfr1>0) {
				matrix A1(nfr1,nfr1,mtx_PSD | mtx_symmetric,buf+nfr*(nfr+7));
				for(i=i1=0;i<d;i++) if (!slide[i]) {
					v1[i1] = -sign*A[i][d];
					for(j=j1=0;j<d;j++) if (!slide[j])
						A1[i1][j1++] = A[i][j];
					i1++;
				} p1.zero();
				A1.conjugate_gradient(p1,v1);
				for(i=i1=0;i<d;i++) if (!slide[i])
					dp[i] = p1[i1++];
			}
			dp[d] = sign;
			dv = A*dp;
			s.set(1,1E-20); j=-1;
			if (v[d]*dv[d]<0) 
			{	j=d; sd = s.set(-v[d],dv[d]).fixsign(); }
			for(i=0;i<=d;i++) if (
				slide[i]==0 && (s1.set(frmax[i]-p[i],dp[i]).fixsign()>=0 && s1<s || s1.set(frmin[i]-p[i],dp[i]).fixsign()>=0 && s1<s) ||
				slide[i]==1 && (v[i]!=0 || p[i]*dv[i]<0) && (s1.set(-v[i],dv[i]).fixsign()>=0 && s1<s))
			{ s=s1; j=i; }
			if (j==-1)
				break;
			sval = s.val(); p += dp*sval; v += dv*sval;
			if (j==d) {
				if (s!=sd) 
					slide[d]=1; 
				break;
			}
			v[j]=0;	slide[j] = slide[j]^1;
			if (slide[j] && isneg(s.x-1E-8*s.y) && prev_j==j)
				break;
			prev_j=j;
		} while(++iter<1000); }

		for(i=0;i<d;i++) {
			dP = frdir[i]*p[i]; m_Ffriction += dP; m_Tfriction += m_susp[idx[i]].ptcontact-m_body.pos^dP;
		}
		Eafter = (m_body.P+m_Ffriction).len2()*m_body.Minv + (m_body.Iinv*(m_body.L+m_Tfriction))*(m_body.L+m_Tfriction);
		if (Eafter>Ebefore*1.1f)
			damping = sqrt_tpl(Ebefore/Eafter);
		else 
			damping = 1.0f;

		for(i=0;i<d;i++) {
			m_susp[idx[i]].bSlip = slide[i];
			//dP = frdir[i]*p[i]; m_Ffriction += dP; m_Tfriction += m_susp[idx[i]].ptcontact-m_body.pos^dP;
			if (m_susp[idx[i]].pent->GetRigidBody(m_susp[idx[i]].ipart)->Minv<m_body.Minv*10.0f) {
				ai.point = m_susp[idx[i]].ptcontact;
				ai.impulse = -frdir[i]*(p[i]*damping);
				ai.ipart = m_susp[idx[i]].ipart;
				m_susp[idx[i]].pent->Action(&ai);
			}
		}
		Pbefore = m_body.P; Lbefore = m_body.L;
		(m_body.P+=m_Ffriction)*=damping; (m_body.L+=m_Tfriction)*=damping;
		m_Ffriction = m_body.P-Pbefore; m_Tfriction = m_body.L-Lbefore;

#ifdef _DEBUG
#if CRY_PLATFORM_WINDOWS && CRY_PLATFORM_64BIT
		assert(m_Ffriction.len2()>=0);
#else
		if (!(m_Ffriction.len2()>=0))
			DEBUG_BREAK;
#endif
		m_body.UpdateState();
		for(i=0;i<d;i++) {
			r = m_susp[idx[i]].ptcontact-m_body.pos;
			v[i] = (m_body.v + (m_body.w^r) - m_susp[idx[i]].pbody->v - 
				(m_susp[idx[i]].pbody->w^m_susp[idx[i]].ptcontact-m_susp[idx[i]].pbody->pos))*frdir[i];
			v[i] += ((m_body.Fcollision*m_body.Minv+(m_body.Iinv*m_body.Tcollision^r))*frdir[i])*time_interval;
		}
#endif

		time_interval = 1.0f/time_interval;
		m_Ffriction*=time_interval; m_Tfriction*=time_interval;
	}*/
}

int CWheeledVehicleEntity::RegisterContacts(float time_interval,int nMaxPlaneContacts)
{
	EventPhysCollision epc;
	epc.pEntity[0]=this; epc.pForeignData[0]=m_pForeignData; epc.iForeignData[0]=m_iForeignData;
	epc.mass[0]=m_body.M;	epc.mass[1]=0; epc.vloc[1].zero();
	epc.penetration=epc.radius = 0;

	for(int i=0;i<m_nParts-m_nHullParts;i++) {
		m_susp[i].pCollEvent = 0;
		if (m_susp[i].bContact) {
			if (m_susp[i].iAxle>=0)
				RegisterContact(&m_susp[i].contact);
			if (m_susp[i].pent->m_parts[m_susp[i].ipart].flags & geom_manually_breakable || m_susp[i].curlen<=0) {
				epc.pEntity[1]=m_susp[i].pent; 
				epc.pForeignData[1]=m_susp[i].pent->m_pForeignData; epc.iForeignData[1]=m_susp[i].pent->m_iForeignData;
				epc.pt = m_susp[i].ptcontact;
				epc.n = m_susp[i].ncontact;
				epc.idCollider = m_pWorld->GetPhysicalEntityId(m_susp[i].pent);
				epc.vloc[0] = m_susp[i].ncontact*(-m_susp[i].PN/m_susp[i].Mpt);
				epc.partid[0] = m_parts[m_nHullParts+i].id;
				epc.idmat[0] = m_susp[i].surface_idx[0];
				epc.idmat[1] = m_susp[i].surface_idx[1];
				epc.normImpulse = m_susp[i].PN;
				if (m_susp[i].curlen<=0) {
					epc.vloc[0] += epc.n*(epc.n*(m_body.v+(m_body.w^m_susp[i].ptcontact-m_body.pos)));
					m_pWorld->OnEvent(pef_log_collisions,&epc, &m_susp[i].pCollEvent);
				}	else
					m_pWorld->OnEvent(pef_log_collisions,&epc);
			}
		}
	}

	return CRigidEntity::RegisterContacts(time_interval,nMaxPlaneContacts);
}


void CWheeledVehicleEntity::OnContactResolved(entity_contact *pcontact, int iop, int iGroupId)
{
	if (iop<2 && pcontact->pent[iop^1]!=this)
		m_nContacts++;
}


float CWheeledVehicleEntity::GetMaxTimeStep(float time_interval) 
{
	m_maxAllowedStep = m_bHasContacts ? m_maxAllowedStepRigid : m_maxAllowedStepVehicle;
	m_maxAllowedStep *= 1.0f-0.25f*(-((int)m_flags & pef_invisible)>>31);
	return CRigidEntity::GetMaxTimeStep(time_interval);
}


float CWheeledVehicleEntity::GetDamping(float time_interval)
{
	return m_bHasContacts ? CRigidEntity::GetDamping(time_interval) : max(0.0f,1.0f-m_dampingVehicle*time_interval);
}

float CWheeledVehicleEntity::CalcEnergy(float time_interval)
{
	float E=0;
	for(int i=0;i<m_nParts-m_nHullParts;i++)
		E += sqr(m_susp[i].fullen-m_susp[i].curlen)*m_susp[i].kStiffness;
	return E+CRigidEntity::CalcEnergy(time_interval);
}


int CWheeledVehicleEntity::HasContactsWith(CPhysicalEntity *pent)
{
	int i; 
	for(i=0;i<m_nParts-m_nHullParts && m_susp[i].pent!=pent;i++);
	if (i<m_nParts-m_nHullParts)
		return 1;
	return CRigidEntity::HasContactsWith(pent);
}

int CWheeledVehicleEntity::HasPartContactsWith(CPhysicalEntity *pent, int ipart, int bGreaterOrEqual)
{
	for(int i=0;i<m_nParts-m_nHullParts;i++)
		if (m_susp[i].pent==pent && iszero(ipart-m_susp[i].ipart) | isneg(ipart-m_susp[i].ipart) & -bGreaterOrEqual)
			return 1;
	return CRigidEntity::HasPartContactsWith(pent,ipart,bGreaterOrEqual);
}


int CWheeledVehicleEntity::RemoveCollider(CPhysicalEntity *pCollider, bool bRemoveAlways)
{
	for(int i=0;i<m_nParts-m_nHullParts;i++) if (m_susp[i].pent==pCollider) {
		m_susp[i].pent=0; m_susp[i].bContact=0;
	}
	return CRigidEntity::RemoveCollider(pCollider,bRemoveAlways);
}


int CWheeledVehicleEntity::Update(float time_interval, float damping)
{
	if (m_nContacts)
		m_timeNoContacts = 0;
	else 
		m_timeNoContacts += time_interval;
	int i,bWheelContact=0;
	for(i=0;i<m_nParts-m_nHullParts;i++) 
		bWheelContact |= m_susp[i].bContact;
	m_bHasContacts = isneg(m_timeNoContacts-0.5f);
	m_Emin = (m_bHasContacts|bWheelContact^1) ? m_EminRigid : m_EminVehicle;

	CRigidEntity::Update(time_interval, damping);
	
	/*m_simTime += time_interval;
	if (!m_bAwake && m_simTime<m_minSimTime) {
		m_bAwake = 1;
		if (m_iSimClass<2) {
			m_iSimClass = 2; m_pWorld->RepositionEntity(this,2);	
		}
	}*/
	for(i=0;i<m_nParts-m_nHullParts;i++) {
		m_susp[i].w *= m_bAwake;
		if (m_susp[i].pCollEvent)	{
			m_susp[i].pCollEvent->normImpulse += m_susp[i].contact.Pspare;
			m_susp[i].pCollEvent = 0;
		}
	}

	/*if (m_pWorld->m_vars.bMultiplayer) {
		for(int i=0;i<m_nParts-m_nHullParts;i++)
			m_susp[i].w = CompressFloat(m_susp[i].w,200.0f,14);
		m_enginePedal = CompressFloat(m_enginePedal,1.0f,12);
		m_clutch = CompressFloat(m_clutch,1.0f,12);
		m_wengine = CompressFloat(m_wengine,200.0f,14);
	}*/

	/*if (!g_histinit) {
		g_histinit=1; int i;
		for(i=0;i<NH;i++) { g_whist[i] = new CWheeledVehicleEntity(m_pWorld);	g_whist2[i] = new CWheeledVehicleEntity(m_pWorld); }
		for(i=0;i<NH;i++) { g_conthist[i] = new entity_contact[64];	g_conthist2[i] = new entity_contact[64]; }
		for(i=0;i<NH;i++) { g_collhist[i] = new masktype[64];	g_collhist2[i] = new masktype[64]; }
	}
	if (g_ncompare) {
		if (g_checksum[g_iwhist]!=GetStateChecksum())
		g_iwhist = g_iwhist;
	} else {
		memcpy(g_whist[g_iwhist],this,sizeof(*this));
		memcpy(g_conthist[g_iwhist],m_pContacts,m_nContactsAlloc*sizeof(entity_contact));
		memcpy(g_collhist[g_iwhist],m_pColliderContacts,m_nCollidersAlloc*sizeof(masktype));
		g_wsnap[g_iwhist].Reset();
		GetStateSnapshot(g_wsnap[g_iwhist]);
		g_checksum[g_iwhist] = GetStateChecksum();
	}
	memcpy(g_whist2[g_iwhist],this,sizeof(*this));
	memcpy(g_conthist2[g_iwhist],m_pContacts,m_nContactsAlloc*sizeof(entity_contact));
	memcpy(g_collhist2[g_iwhist],m_pColliderContacts,m_nCollidersAlloc*sizeof(masktype));
	g_iwhist = g_iwhist+1 & NH-1;
	if (g_ncompare && ++g_ncompare>NH-2) g_ncompare=0;
	if (g_bstartcompare) {
		if (g_iwhist0>=0)	g_iwhist=g_iwhist0;
		else g_iwhist0=g_iwhist;
		g_ncompare = 1; g_bstartcompare=0;
		g_wsnap[g_iwhist].Seek(0);
		SetStateFromSnapshot(g_wsnap[g_iwhist]);
		PostSetStateFromSnapshot();
		g_iwhist = g_iwhist+1 & NH-1;
	}*/

	return (m_bAwake^1) | isneg(m_timeStepFull-m_timeStepPerformed-0.001f) | iszero(m_enginePedal+(~m_flags & pef_invisible));
}


int CWheeledVehicleEntity::GetStateSnapshot(CStream &stm, float time_back, int flags)
{
	CRigidEntity::GetStateSnapshot(stm,time_back,flags);

	if (!(flags & ssf_checksum_only)) {
		if (m_pWorld->m_vars.bMultiplayer) {
			for(int i=0;i<m_nParts-m_nHullParts;i++)
				WriteCompressedFloat(stm, m_susp[i].w, 200.0f,14);
			WriteCompressedFloat(stm, m_enginePedal, 1.0f,12);
			WriteCompressedFloat(stm, m_steer, 1.0f,12);
			WriteCompressedFloat(stm, m_clutch, 1.0f,12);
			WriteCompressedFloat(stm, m_wengine, 200.0f,14);
		} else {
			for(int i=0;i<m_nParts-m_nHullParts;i++)
				stm.Write(m_susp[i].w);
			stm.Write(m_enginePedal);
			stm.Write(m_steer);
			stm.Write(m_clutch);
			stm.Write(m_wengine);
		}
		stm.Write(m_bHandBrake!=0);
		if (1/*m_iIntegrationType*/) {
			stm.Write(true);
			//stm.Write(m_Ffriction);
			//stm.Write(m_Tfriction);
		} else stm.Write(false);
		if (m_body.Fcollision.len2()+m_body.Tcollision.len2()>0) {
			stm.Write(true);
			stm.Write(m_body.Fcollision);
			stm.Write(m_body.Tcollision);
		}	else stm.Write(false);
		stm.Write(m_bHasContacts!=0);
		stm.WriteNumberInBits(m_iCurGear,3);
	}

	return 1;
}

int CWheeledVehicleEntity::SetStateFromSnapshot(CStream &stm, int flags)
{
	int res = CRigidEntity::SetStateFromSnapshot(stm,flags);
	if (res && res!=2) {
		bool bnz; int gear;
		if (!(flags & ssf_no_update)) {
			pe_action_drive ad;
			if (m_pWorld->m_vars.bMultiplayer) {
				for(int i=0;i<m_nParts-m_nHullParts;i++)
					ReadCompressedFloat(stm, m_susp[i].w, 200.0f,14);
				ReadCompressedFloat(stm, ad.pedal, 1.0f,12);
				ReadCompressedFloat(stm, ad.steer, 1.0f,12);
				ReadCompressedFloat(stm, m_clutch, 1.0f,12);
				ReadCompressedFloat(stm, m_wengine, 200.0f,14);
			} else {
				for(int i=0;i<m_nParts-m_nHullParts;i++)
					stm.Read(m_susp[i].w);
				stm.Read(ad.pedal);
				stm.Read(ad.steer);
				stm.Read(m_clutch);
				stm.Read(m_wengine);
			}
			stm.Read(bnz); ad.bHandBrake = bnz ? 1:0;
			Action(&ad);

			stm.Read(bnz); if (bnz) {
				//stm.Read(m_Ffriction);
				//stm.Read(m_Tfriction);
			}
			stm.Read(bnz); if (bnz) {
				stm.Read(m_body.Fcollision);
				stm.Read(m_body.Tcollision);
			}	else {
				m_body.Fcollision.zero();
				m_body.Tcollision.zero();
			}
			stm.Read(bnz); m_bHasContacts = bnz ? 1:0;
			stm.ReadNumberInBits(gear,3);
			m_iCurGear = gear;
		} else {
			stm.Seek(stm.GetReadPos()+(m_pWorld->m_vars.bMultiplayer ? 
				(m_nParts-m_nHullParts)*14+12*3+14+1 : (m_nParts-m_nHullParts)*sizeof(float)*8+4*sizeof(float)*8+1));
			stm.Read(bnz); if (bnz)
				stm.Seek(stm.GetReadPos()+2*sizeof(Vec3)*8);
			stm.Read(bnz); if (bnz)
				stm.Seek(stm.GetReadPos()+2*sizeof(Vec3)*8);
			stm.Seek(stm.GetReadPos()+4);
		}
	}

	return res;
}

void SWheeledVehicleEntityNetSerialize::Serialize( TSerialize ser, int nSusp )
{
	if (ser.GetSerializationTarget()!=eST_Network) {
		ser.BeginGroup("wheels");
		for (int i=0; i<nSusp; i++)
			ser.Value(numbered_tag("suspension",i), pSusp[i].curlen);
		ser.EndGroup();
	}

	ser.Value("pedal", pedal, 'pPed');
	ser.Value("steer", steer, 'pStr');
	//ser.Value("clutch", clutch, 'pClt');
	ser.Value("wengine", wengine, 'pWEn');
	ser.Value("handbrake", handBrake, 'bool');
	ser.Value("curGear", curGear, 'pGr');
}

int CWheeledVehicleEntity::GetStateSnapshot(TSerialize ser, float time_back, int flags)
{
	SWheeledVehicleEntityNetSerialize helper = {
		m_susp,
		m_enginePedal,
		m_steer,
		m_clutch,
		m_wengine,
		m_bHandBrake != 0,
		m_iCurGear
	};
	helper.Serialize( ser, m_nParts - m_nHullParts );

	return CRigidEntity::GetStateSnapshot( ser, time_back, flags );
}

int CWheeledVehicleEntity::SetStateFromSnapshot(TSerialize ser, int flags)
{
	static suspension_point suspDummy;

	bool commit = ser.ShouldCommitValues();

	SWheeledVehicleEntityNetSerialize helper;
	helper.pSusp = commit? strided_pointer<suspension_point>(m_susp) : strided_pointer<suspension_point>(&suspDummy,0);
	helper.Serialize( ser, m_nParts - m_nHullParts );

	if (ser.ShouldCommitValues())
	{
		pe_action_drive ad;
		ad.pedal = helper.pedal;
		ad.steer = helper.steer;
		ad.bHandBrake = helper.handBrake;
		m_clutch = 1.f;//helper.clutch;		// No need to serialise clutch, network game seems unaffected
		m_wengine = helper.wengine;
		m_iCurGear = helper.curGear;
#if USE_IMPROVED_RIGID_ENTITY_SYNCHRONISATION
		if (!m_hasAuthority || ser.GetSerializationTarget()!=eST_Network)
#endif
			Action( &ad, 0 );
	}

	return CRigidEntity::SetStateFromSnapshot( ser, flags );
}


void CWheeledVehicleEntity::DrawHelperInformation(IPhysRenderer *pRenderer, int flags)
{
	CRigidEntity::DrawHelperInformation(pRenderer, flags);

	if (flags & pe_helper_collisions) {
		for(int i=0;i<m_nParts-m_nHullParts;i++) if (m_susp[i].bContact && m_susp[i].iAxle>=0)
			pRenderer->DrawLine(m_susp[i].ptcontact, m_susp[i].ptcontact + m_susp[i].ncontact*m_pWorld->m_vars.maxContactGap*30, m_iSimClass);
	}
}


void CWheeledVehicleEntity::GetMemoryStatistics(ICrySizer *pSizer) const
{
	if (GetType()==PE_WHEELEDVEHICLE)
		pSizer->AddObject(this, sizeof(CWheeledVehicleEntity));
	CRigidEntity::GetMemoryStatistics(pSizer);
	pSizer->AddObject(m_susp, (m_nParts-m_nHullParts)*sizeof(m_susp[0]));
}
