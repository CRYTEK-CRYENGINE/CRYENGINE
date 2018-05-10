// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which uses CGA files

   -------------------------------------------------------------------------
   History:
   - 24:08:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEPARTANIMATED_H__
#define __VEHICLEPARTANIMATED_H__

#include "VehiclePartBase.h"

class CScriptBind_VehiclePart;
class CVehicle;
class CVehiclePartAnimatedJoint;

class CVehiclePartAnimated
	: public CVehiclePartBase
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehiclePartAnimated();
	~CVehiclePartAnimated();

	// IVehiclePart
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void Release() override;
	virtual void Reset() override;

	virtual void SetMaterial(IMaterial* pMaterial) override;

	virtual void OnEvent(const SVehiclePartEvent& event) override;

	virtual bool ChangeState(EVehiclePartState state, int flags = 0) override;
	virtual void Physicalize() override;

	virtual void Update(const float frameTime) override;

	virtual void Serialize(TSerialize serialize, EEntityAspects) override;
	virtual void PostSerialize() override;

	virtual void GetMemoryUsage(ICrySizer* s) const override;
	// ~IVehiclePart

	virtual IStatObj* GetSubGeometry(CVehiclePartBase* pPart, EVehiclePartState state, Matrix34& position, bool removeFromParent) override;
	IStatObj*         GetGeometryForState(CVehiclePartAnimatedJoint* pPart, EVehiclePartState state);
	IStatObj*         GetDestroyedGeometry(const char* pJointName, unsigned int index = 0);
	Matrix34          GetDestroyedGeometryTM(const char* pJointName, unsigned int index);

	virtual void      SetDrivingProxy(bool bDrive);

	void              RotationChanged(CVehiclePartAnimatedJoint* pJoint);

	int               AssignAnimationLayer() { return ++m_lastAnimLayerAssigned; }

#if ENABLE_VEHICLE_DEBUG
	void Dump();
#endif

	int  GetNextFreeLayer();
	bool ChangeChildState(CVehiclePartAnimatedJoint* pPart, EVehiclePartState state, int flags);

protected:

	virtual void              InitGeometry();
	void                      FlagSkeleton(ISkeletonPose* pSkeletonPose, IDefaultSkeleton& rIDefaultSkeleton);
	virtual EVehiclePartState GetStateForDamageRatio(float ratio) override;

	typedef std::map<string, /*_smart_ptr<*/ IStatObj*> TStringStatObjMap;
	TStringStatObjMap m_intactStatObjs;

	typedef std::map<string, IVehiclePart*> TStringVehiclePartMap;
	TStringVehiclePartMap          m_jointParts;

	_smart_ptr<ICharacterInstance> m_pCharInstance;
	ICharacterInstance*            m_pCharInstanceDestroyed;

	int                            m_hullMatId[2];

	int                            m_lastAnimLayerAssigned;
	int                            m_iRotChangedFrameId;
	bool                           m_serializeForceRotationUpdate;
	bool                           m_initialiseOnChangeState;
	bool                           m_ignoreDestroyedState;
};

#endif
