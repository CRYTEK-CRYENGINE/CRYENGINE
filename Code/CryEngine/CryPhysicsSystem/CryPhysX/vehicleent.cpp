// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "geometries.h"
#include "entities.h"
#include "world.h"

using namespace primitives;
using namespace cpx;
using namespace Helper;


int PhysXVehicle::AddGeometry(phys_geometry* pgeom, pe_geomparams* _params, int id, int bThreadSafe)
{
	int res = PhysXEnt::AddGeometry(pgeom,_params,id,bThreadSafe);

	pe_cargeomparams *params = (pe_cargeomparams*)_params;
	if (_params->type==pe_cargeomparams::type_id && !is_unused(params->bDriving) && params->bDriving>=0) {
		Wheel wheel;
		wheel.idx = m_wheels.size();
		wheel.partid = res;
		wheel.driving = !!params->bDriving;
		wheel.canBrake = !!params->bCanBrake;
		wheel.canSteer = !!params->bCanSteer;
		wheel.pivot = params->pivot;
		wheel.suspLenMax = params->lenMax;
		wheel.suspLenRest = params->lenInitial;
		box bbox; pgeom->pGeom->GetBBox(&bbox);
		QuatT trans; Diag33 scale(1);
		ExtractTransform(params,trans,scale);
		wheel.r = max(max(bbox.size.x,bbox.size.y),bbox.size.z);
		wheel.width = min(min(bbox.size.x,bbox.size.y),bbox.size.z)*2;
		wheel.center = trans*bbox.center;
		wheel.kd = params->kDamping;
		float fric[2] = { 1,1 };
		if (!is_unused(params->minFriction)) fric[0]=fric[1] = params->minFriction;
		if (!is_unused(params->maxFriction)) fric[1] = params->maxFriction;
		wheel.friction = (fric[0]+fric[1])*0.5f;
		wheel.mass = params->mass>0 ? params->mass : params->density*pgeom->V*(scale*Vec3(1)).GetVolume();
		if (wheel.driving)
			m_wheels.insert(std::find_if(m_wheels.begin(),m_wheels.end(), [](auto wheel)->bool { return !wheel.driving; }), wheel);
		else
			m_wheels.push_back(wheel);
	} else if (res>=0) {
		PxFilterData fd = m_parts.back().shape->getSimulationFilterData();
		fd.word3 |= 16; // enable CCD
		m_parts.back().shape->setSimulationFilterData(fd);
	}
	return res;
}

int PhysXVehicle::SetParams(pe_params* _params, int bThreadSafe)
{
	if (_params->type==pe_params_wheel::type_id) {
		pe_params_wheel *params = (pe_params_wheel*)_params;
		return 1;
	}	else
		SetupPxVehicle();

	if (_params->type==pe_params_car::type_id) {
		pe_params_car *params = (pe_params_car*)_params;
		if (!is_unused(params->enginePower)) m_enginePower = params->enginePower;
		if (!is_unused(params->engineMaxRPM)) m_engineMaxRPM = params->engineMaxRPM;
		if (!is_unused(params->maxSteer)) m_maxSteer = params->maxSteer;
		if (!is_unused(params->gearRatios))	{
			m_gears.resize(params->nGears);
			memcpy(&m_gears[0], params->gearRatios, params->nGears);
		}
		return 1;
	}
	
	return PhysXEnt::SetParams(_params,bThreadSafe);
}

int PhysXVehicle::Action(pe_action* _action, int bThreadSafe)
{
	if (_action->type==pe_action_drive::type_id) {
		pe_action_drive *action = (pe_action_drive*)_action;
		if (SetupPxVehicle()) {
			#define SET_INPUT(name,nameOffs,val) m_vehicle->mDriveDynData.setAnalogInput(PxVehicleDrive4WControl::Enum(PxVehicleDrive4WControl::eANALOG_INPUT_##name + nameOffs), val)
			#define GET_INPUT(name,nameOffs) m_vehicle->mDriveDynData.getAnalogInput(PxVehicleDrive4WControl::Enum(PxVehicleDrive4WControl::eANALOG_INPUT_##name + nameOffs))
			#define CHANGE_INPUT(name,nameOffs,val) SET_INPUT(name,nameOffs, val+GET_INPUT(name,nameOffs))
			if (!is_unused(action->bHandBrake)) SET_INPUT(HANDBRAKE,0, action->bHandBrake);
			if (!is_unused(action->pedal)) {
				SET_INPUT(ACCEL,0, fabs(action->pedal));
				if (action->pedal<0)
					m_vehicle->mDriveDynData.setCurrentGear(0);
				else if (action->pedal>0 && m_vehicle->mDriveDynData.getCurrentGear()==0)
					m_vehicle->mDriveDynData.setCurrentGear(1);
			}
			if (!is_unused(action->dpedal))	CHANGE_INPUT(ACCEL,0, action->dpedal);
			if (!is_unused(action->steer)) {
				SET_INPUT(STEER_LEFT,isneg(action->steer),fabs(action->steer));
				SET_INPUT(STEER_RIGHT,-isneg(action->steer),0);
			}
			if (!is_unused(action->dsteer) && action->dsteer) {
				CHANGE_INPUT(STEER_LEFT,isneg(action->dsteer),fabs(action->dsteer));
				SET_INPUT(STEER_RIGHT,-isneg(action->dsteer),0);
			}
		}
		return 1;
	}
	if (_action->type==pe_action_reset::type_id)
		SetupPxVehicle();
	return PhysXEnt::Action(_action,bThreadSafe);
}

int PhysXVehicle::GetStatus(pe_status* _status) const 
{
	if (_status->type==pe_status_vehicle::type_id && m_vehicle) {
		pe_status_vehicle *status = (pe_status_vehicle*)_status;
		status->bHandBrake = GET_INPUT(HANDBRAKE,0);
		status->pedal = GET_INPUT(ACCEL,0);
		status->steer = GET_INPUT(STEER_LEFT,0)-GET_INPUT(STEER_RIGHT,0);
		status->engineRPM = m_vehicle->mDriveDynData.getEngineRotationSpeed()*(30/gf_PI);
		status->iCurGear = m_vehicle->mDriveDynData.getCurrentGear();
		for(int i=status->bWheelContact=0; i<m_wheelsQuery.size(); status->bWheelContact|=(int)m_wheelsQuery[i++].isInAir);
		status->vel = V(m_actor->isRigidDynamic()->getLinearVelocity());
		status->clutch = 1;
		return 1;
	}

	if (_status->type==pe_status_wheel::type_id) {
		pe_status_wheel *status = (pe_status_wheel*)_status;
		int i,idx=status->iWheel;
		if (!is_unused(status->partid))
			for(idx=m_wheels.size()-1; idx>=0 && m_wheels[idx].partid!=status->partid; idx--);
		for(i=m_wheelsQuery.size()-1; i>=0 && m_wheels[i].idx!=idx; i--);
		if (i<0)
			return 0;
		if (status->bContact = !m_wheelsQuery[i].isInAir) {
			status->ptContact = V(m_wheelsQuery[i].tireContactPoint);
			status->normContact = V(m_wheelsQuery[i].tireContactNormal);
			status->pCollider = m_wheelsQuery[i].tireContactActor ? Ent(m_wheelsQuery[i].tireContactActor) : nullptr;
			status->contactSurfaceIdx = MatId(m_wheelsQuery[i].tireSurfaceMaterial);
		}
		status->friction = m_wheels[idx].friction;
		status->r = m_wheels[idx].r;
		status->suspLen0 = m_wheels[idx].suspLenRest;
		status->suspLenFull = m_wheels[idx].suspLenMax;
		status->suspLen = status->suspLen0-m_wheelsQuery[i].suspJounce;
		return 1;
	}
	return PhysXEnt::GetStatus(_status);
}

PxQueryHitType::Enum WheelRaycastPreFilter(PxFilterData fdWheel, PxFilterData fdObj, const void* constantBlock, PxU32 constantBlockSize, PxHitFlags& queryFlags)
{
	return fdObj.word2==fdWheel.word2 || !(fdObj.word0 & fdWheel.word1) ? PxQueryHitType::eNONE : PxQueryHitType::eBLOCK;
}

bool PhysXVehicle::SetupPxVehicle()
{
	if (!m_vehicle && m_wheels.size() < 4) // at the moment, only allow 4 or more-wheeled vehicles
		return false;
	if (m_vehicle)
		return true;

	int nwheels=m_wheels.size(), driveMask=0;
	m_vehicle = PxVehicleDrive4W::allocate(nwheels);
	PxVehicleWheelsSimData *wsd = PxVehicleWheelsSimData::allocate(nwheels);
	PxVehicleDriveSimData4W dsd;
	pe_params_car pc;
	PxRigidDynamic *pRD = m_actor->isRigidDynamic();
	QuatT ent2body = T(pRD->getCMassLocalPose()).GetInverted();
	PxVec3 *ptsusp = (PxVec3*)alloca(nwheels*sizeof(PxVec3));
	float *wmass = (float*)alloca(nwheels*sizeof(float));
	Vec3 wpos[4];
	for(int i=0; i<nwheels; i++) 
		ptsusp[i] = V(m_wheels[i].pivot);

	PxVehicleComputeSprungMasses(m_wheels.size(), ptsusp, pRD->getCMassLocalPose().p, pRD->getMass(), 2, wmass);
	m_fric = PxVehicleDrivableSurfaceToTireFrictionPairs::allocate(nwheels,1);
	PxVehicleDrivableSurfaceType surf; surf.mType=0;
	m_fric->setup(nwheels,1, (const PxMaterial**)&g_pPhysWorld->m_mats[0], &surf);

	int nShapes = pRD->getNbShapes();
	PxShape **shapes = (PxShape**)alloca(nShapes*sizeof(void*));
	pRD->getShapes(shapes,nShapes);

	for(int i=0; i<nwheels; i++) {
		PxVehicleWheelData wd;
		wd.mRadius = m_wheels[i].r;
		wd.mWidth = m_wheels[i].width;
		wd.mMass = m_wheels[i].mass;
		wd.mMOI = g_PI*(wd.mWidth*0.5f)*sqr(sqr(wd.mRadius))*m_wheels[i].mass;
		wd.mMaxSteer = m_wheels[i].canSteer ? m_maxSteer : 0.0f;
		wd.mMaxHandBrakeTorque = m_wheels[i].canBrake ? 4000.0f : 0.0f;
		wsd->setWheelData(i,wd);
		int ipos = isneg(m_wheels[i].pivot.x) | 1-sgnnz(m_wheels[i].pivot.y);
		wpos[ipos] = m_wheels[i].pivot;
		driveMask |= m_wheels[i].driving << ipos;

		PxVehicleTireData td;
		td.mType = i;
		wsd->setTireData(i, td);
		m_fric->setTypePairFriction(0,i,m_wheels[i].friction);

		PxVehicleSuspensionData sd;
		sd.mMaxCompression = m_wheels[i].suspLenRest;
		sd.mMaxDroop = m_wheels[i].suspLenMax-m_wheels[i].suspLenRest;
		sd.mSprungMass = wmass[i];
		sd.mSpringStrength = wmass[i]*g_pPhysWorld->m_vars.gravity.len() / max(0.001f,sd.mMaxDroop);
		sd.mSpringDamperRate = m_wheels[i].kd<0 ? 2*sqrt(sd.mSpringStrength*sd.mSprungMass)*-m_wheels[i].kd : m_wheels[i].kd;
 		wsd->setSuspensionData(i, sd);
		wsd->setSuspTravelDirection(i,PxVec3(0,0,-1));

		wsd->setWheelCentreOffset(i, V(ent2body*m_wheels[i].center));
		wsd->setSuspForceAppPointOffset(i, V(ent2body*m_wheels[i].pivot));
		wsd->setTireForceAppPointOffset(i, V(ent2body*(m_wheels[i].center-Vec3(0,0,m_wheels[i].r))));
		int ipart=idxPart(m_wheels[i].partid), ishape;
		for(ishape=nShapes-1; ishape>0 && shapes[ishape]!=m_parts[ipart].shape; ishape--);
		wsd->setWheelShapeMapping(i, ishape);
		PxFilterData fd = m_parts[ipart].shape->getSimulationFilterData();
		wsd->setSceneQueryFilterData(i, fd);
		m_parts[ipart].shape->setSimulationFilterData(PxFilterData(0,0,fd.word2,fd.word3));
	}

	PxVehicleDifferential4WData diff;
	diff.mType = driveMask==15 ? PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD : 
		((driveMask & 3)==3 ? PxVehicleDifferential4WData::eDIFF_TYPE_LS_FRONTWD : PxVehicleDifferential4WData::eDIFF_TYPE_LS_REARWD);
	dsd.setDiffData(diff);
	PxVehicleEngineData engine;
	engine.mPeakTorque = m_enginePower;
	engine.mMaxOmega = m_engineMaxRPM*(gf_PI/30);
	dsd.setEngineData(engine);
	PxVehicleGearsData gears;
	for(int i=0;i<m_gears.size();i++)
		gears.setGearRatio(PxVehicleGearsData::Enum(i), m_gears[i]);
	gears.mSwitchTime = 0.5f;
	dsd.setGearsData(gears);
	PxVehicleAckermannGeometryData ack;
	ack.mAccuracy = 1;
	ack.mFrontWidth = wpos[0].x-wpos[1].x;
	ack.mRearWidth = wpos[2].x-wpos[3].x;
	ack.mAxleSeparation = wpos[0].y-wpos[2].y;
	dsd.setAckermannGeometryData(ack);
	
	m_vehicle->setup(g_cryPhysX.Physics(), pRD, *wsd, dsd, max(nwheels-4,0));
	wsd->free();
	m_vehicle->mDriveDynData.setUseAutoGears(true);
	PxBatchQueryDesc sqDesc(nwheels,0,0);
	m_rayHits.resize(nwheels);
	m_wheelsQuery.resize(nwheels);
	sqDesc.queryMemory.userRaycastResultBuffer = &m_rayHits[0];
	sqDesc.preFilterShader = WheelRaycastPreFilter;
	m_rayCaster = g_cryPhysX.Scene()->createBatchQuery(sqDesc);

	return true;
}

void PhysXVehicle::PostStep(float dt, int immediate)
{
	if (m_vehicle && immediate) {
		PxVehicleWheelQueryResult query;
		query.wheelQueryResults = m_wheelsQuery.data();
		query.nbWheelQueryResults = m_wheelsQuery.size();
		PxVehicleSuspensionRaycasts(m_rayCaster,1,(PxVehicleWheels**)&m_vehicle, m_rayHits.size(),&m_rayHits[0]);
		PxVehicleUpdates(dt, V(g_pPhysWorld->m_vars.gravity), *m_fric, 1, (PxVehicleWheels**)&m_vehicle, &query);
	}
	PhysXEnt::PostStep(dt,immediate);
}