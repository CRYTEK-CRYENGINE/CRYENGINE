// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a part for vehicles which is the an attachment
   of a parent Animated part

   -------------------------------------------------------------------------
   History:
   - 05:09:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEPARTANIMATEDJOINT_H__
#define __VEHICLEPARTANIMATEDJOINT_H__

#include "VehicleSystem/VehiclePartBase.h"

#define VEH_USE_RPM_JOINT

class CVehicle;

class CVehiclePartAnimatedJoint
	: public CVehiclePartBase
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehiclePartAnimatedJoint();
	virtual ~CVehiclePartAnimatedJoint();

	// IVehiclePart
	virtual bool        Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void        InitGeometry(const CVehicleParams& table);
	virtual void        PostInit() override;
	virtual void        Reset() override;
	virtual bool        ChangeState(EVehiclePartState state, int flags) override;
	virtual void        SetMaterial(IMaterial* pMaterial) override;
	virtual void        OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams& params) override;

	virtual void        OnEvent(const SVehiclePartEvent& event) override;

	virtual Matrix34    GetLocalTM(bool relativeToParentPart, bool forced = false) override;
	virtual Matrix34    GetWorldTM() override;
	virtual const AABB& GetLocalBounds() override;
	virtual Matrix34    GetLocalInitialTM() override { return m_initialTM; }

	virtual void        Physicalize() override;
	virtual void        Update(const float frameTime) override;

	virtual void        InvalidateTM(bool invalidate) override;
	virtual void        SetMoveable(bool allowTranslationMovement = false) override;

	virtual void        GetMemoryUsage(ICrySizer* s) const override;
	// ~IVehiclePart

	virtual void       SetLocalTM(const Matrix34& localTM) override;
	virtual void       ResetLocalTM(bool recursive) override;

	virtual int        GetJointId() { return m_jointId; }

	virtual IStatObj*  GetStatObj() override;
	virtual bool       SetStatObj(IStatObj* pStatObj) override;

	virtual IMaterial* GetMaterial() override;

	virtual Matrix34   GetLocalBaseTM() override { return m_baseTM; }
	virtual void       SetLocalBaseTM(const Matrix34& tm) override;

	virtual void       SerMatrix(TSerialize ser, Matrix34& mat);
	virtual void       Serialize(TSerialize ser, EEntityAspects aspects) override;

	virtual IStatObj*  GetExternalGeometry(bool destroyed) override { return destroyed ? m_pDestroyedGeometry.get() : m_pGeometry.get(); }

protected:
	Matrix34 m_baseTM;
	Matrix34 m_initialTM;
	Matrix34 m_worldTM;
	Matrix34 m_localTM;
	Matrix34 m_nextFrameLocalTM;
	AABB     m_localBounds;

	// if using external geometry
	_smart_ptr<IStatObj>           m_pGeometry;
	_smart_ptr<IStatObj>           m_pDestroyedGeometry;
	_smart_ptr<IMaterial>          m_pMaterial;

	_smart_ptr<ICharacterInstance> m_pCharInstance;
	CVehiclePartAnimated*          m_pAnimatedPart;
	int                            m_jointId;

#ifdef VEH_USE_RPM_JOINT
	float m_dialsRotMax;
	float m_initialRotOfs;
#endif

	bool                       m_localTMInvalid;
	bool                       m_isMoveable;
	bool                       m_isTransMoveable;
	bool                       m_bUsePaintMaterial;

	IAnimationOperatorQueuePtr m_operatorQueue;

	friend class CVehiclePartAnimated;
};

#endif
