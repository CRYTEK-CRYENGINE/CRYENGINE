// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements a tread for tanks

   -------------------------------------------------------------------------
   History:
   - 14:09:2005: Created by Mathieu Pinard

*************************************************************************/
#ifndef __VEHICLEPARTTREAD_H__
#define __VEHICLEPARTTREAD_H__

#include "VehicleSystem/VehiclePartBase.h"

class CVehicle;

class CVehiclePartTread
	: public CVehiclePartBase
{
	IMPLEMENT_VEHICLEOBJECT
public:

	CVehiclePartTread();
	virtual ~CVehiclePartTread() {}

	// IVehiclePart
	virtual bool        Init(IVehicle* pVehicle, const CVehicleParams& table, IVehiclePart* parent, CVehicle::SPartInitInfo& initInfo, int partType) override;
	virtual void        InitGeometry();
	virtual void        Reset() override;

	virtual bool        ChangeState(EVehiclePartState state, int flags = 0) override;

	virtual void        Physicalize() override;

	virtual Matrix34    GetLocalTM(bool relativeToParentPart, bool forced = false) override;
	virtual Matrix34    GetWorldTM() override;
	virtual const AABB& GetLocalBounds() override;

	virtual void        Update(const float frameTime) override;

	virtual void        RegisterSerializer(IGameObjectExtension* gameObjectExt) override {}
	virtual void        GetMemoryUsage(ICrySizer* s) const override
	{
		s->Add(*this);
		s->AddObject(m_wheels);
		CVehiclePartBase::GetMemoryUsageInternal(s);
	}
	// ~IVehiclePart

	virtual void UpdateU();

protected:

	_smart_ptr<ICharacterInstance>     m_pCharInstance;
	int                                m_lastWheelIndex;

	float                              m_uvSpeedMultiplier;

	bool                               m_bForceSetU;
	float                              m_wantedU;
	float                              m_currentU;

	_smart_ptr<IMaterial>              m_pMaterial;
	_smart_ptr<IRenderShaderResources> m_pShaderResources;

	struct SWheelInfo
	{
		int                       slot;
		int                       jointId;
		CVehiclePartSubPartWheel* pWheel;
		void                      GetMemoryUsage(ICrySizer* s) const
		{
			s->AddObject(pWheel);
		}
	};

	IAnimationOperatorQueuePtr m_operatorQueue;

	typedef std::vector<SWheelInfo> TWheelInfoVector;
	TWheelInfoVector m_wheels;
};

#endif
