// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a vehicle action to deploy a rope

-------------------------------------------------------------------------
History:
- 30:11:2006: Created by Mathieu Pinard
- 22:04:2010: Re-factored and re-worked by Paul Slinger

*************************************************************************/

#ifndef __VEHICLEACTIONDEPLOYROPE_H__
#define __VEHICLEACTIONDEPLOYROPE_H__

struct IRopeRenderNode;

class CVehicleActionDeployRope : public IVehicleSeatAction
{
	IMPLEMENT_VEHICLEOBJECT;

	public:

		CVehicleActionDeployRope();

		virtual ~CVehicleActionDeployRope();

		// IVehicleSeatAction

		virtual bool Init(IVehicle *pVehicle, IVehicleSeat* pSeat, const CVehicleParams &table) override;

		virtual void Reset() override;

		virtual void Release() override;

		virtual void StartUsing(EntityId passengerId) override;
		virtual void ForceUsage() override {}

		virtual void StopUsing() override;

		virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value) override;

		virtual void OnVehicleEvent(EVehicleEvent event, const SVehicleEventParams &params) override;

		virtual void Serialize(TSerialize serialize, EEntityAspects aspects) override;

		virtual void PostSerialize() override;

		virtual void Update(const float deltaTime) override;

		virtual void GetMemoryUsage(ICrySizer* pSizer) const override;

		// ~IVehicleSeatAction

	protected:

		bool DeployRope();

		EntityId CreateRope(IPhysicalEntity *pLinkedEntity, const Vec3 &highPos, const Vec3 &lowPos);

		IRopeRenderNode *GetRopeRenderNode(EntityId ropeId);

		bool AttachActorToRope(EntityId actorId, EntityId ropeId);

		static const float	ms_extraRopeLength;

		IVehicle						*m_pVehicle;

		IVehicleSeat				*m_pSeat;

		IVehicleHelper			*m_pRopeHelper;

		IVehicleAnimation		*m_pDeployAnim;
		
		TVehicleAnimStateId	m_deployAnimOpenedId, m_deployAnimClosedId;

		EntityId						m_actorId, m_upperRopeId, m_lowerRopeId;

		float								m_altitude;
};

#endif //__VEHICLEACTIONDEPLOYROPE_H__