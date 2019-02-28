// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if 0
/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements a standard helicopter movements

-------------------------------------------------------------------------
History:
	- Created by Stan Fichele

*************************************************************************/
#ifndef __VEHICLEMOVEMENTHELICOPTER_ARCADE_H__
#define __VEHICLEMOVEMENTHELICOPTER_ARCADE_H__

#include "VehicleMovementBase.h"
#include "Network/NetActionSync.h"
#include "Vehicle/VehiclePhysicsHelicopter.h"

class CVehicleMovementHelicopterArcade;
class CVehicleActionLandingGears;


class CNetworkMovementHelicopterArcade
{
public:
	CNetworkMovementHelicopterArcade();
	CNetworkMovementHelicopterArcade(CVehicleMovementHelicopterArcade *pMovement);
	
	typedef CVehicleMovementHelicopterArcade * UpdateObjectSink;
	
	bool operator == (const CNetworkMovementHelicopterArcade &rhs)
	{
		return false;
	};

	bool operator != (const CNetworkMovementHelicopterArcade &rhs)
	{
		return !this->operator==(rhs);
	};

	void UpdateObject( CVehicleMovementHelicopterArcade *pMovement );
	void Serialize(TSerialize ser, EEntityAspects aspects);

	static const NetworkAspectType CONTROLLED_ASPECT = eEA_GameClientServerOnlyA;

public:
	SHelicopterAction action;	// Do we need to store this twice?
};


class CVehicleMovementHelicopterArcade
	: public CVehicleMovementBase
{
public:

	CVehicleMovementHelicopterArcade();
	virtual ~CVehicleMovementHelicopterArcade() {}

	// IVehicleMovement
	virtual bool Init(IVehicle* pVehicle, const CVehicleParams& table);
	virtual void PostInit();
	virtual void Reset();
	virtual void Release();
	virtual void Physicalize();
	virtual void PostPhysicalize();

	virtual EVehicleMovementType GetMovementType() { return eVMT_Air; }
	virtual float GetDamageRatio() { return min(1.0f, max(m_damage, m_steeringDamage/3.0f ) ); }

	virtual bool StartEngine(EntityId driverId);
	virtual void StopEngine();

	virtual void OnEvent(EVehicleMovementEvent event, const SVehicleMovementEventParams& params);
	virtual void OnAction(const TVehicleActionId actionId, int activationMode, float value);

	virtual void ProcessMovement(const float deltaTime);
	virtual void ProcessActions(const float deltaTime);
	virtual void ProcessAI(const float deltaTime);

	virtual void Update(const float deltaTime);

	virtual SVehicleMovementAction GetMovementActions();
	virtual void RequestActions(const SVehicleMovementAction& movementAction);
	virtual bool RequestMovement(CMovementRequest& movementRequest);

	virtual void Serialize(TSerialize ser, EEntityAspects aspects);
	virtual void SetAuthority( bool auth )
	{
		m_netActionSync.CancelReceived();
	};
	virtual void SetChannelId(uint16 id) {};

	virtual SVehicleNetState GetVehicleNetState();
	virtual void SetVehicleNetState(const SVehicleNetState& state);
	// ~IVehicleMovement

	// CVehicleMovementBase
	virtual void OnEngineCompletelyStopped();
	virtual float GetEnginePedal();
	// ~CVehicleMovementBase

	virtual void ResetActions();
	virtual void UpdateDamages(float deltaTime);
	virtual void UpdateEngine(float deltaTime);
	virtual void ProcessActions_AdjustActions(float deltaTime) {}

	virtual void GetMemoryUsage(ICrySizer * pSizer) const; 
	void GetMemoryUsageInternal(ICrySizer * pSizer) const; 	

protected:
	float GetEnginePower();
	float GetDamageMult();

protected:
	// Handling
	SHelicopterAction m_inputAction;
	CVehiclePhysicsHelicopter m_arcade;

	// Animation
	IVehicleAnimation* m_pRotorAnim; 
	IVehiclePart* m_pRotorPart;
	IVehicleHelper* m_pRotorHelper;
	CVehicleActionLandingGears* m_pLandingGears;

	float m_altitudeMax;
	float m_engineWarmupDelay;  // Time taken to get to full power
	float m_enginePower;        // ramps from 0 -> 1.0

	float m_damageActual;
	float m_steeringDamage;
	float m_turbulence;
	bool m_isTouchingGround;
	float m_timeOnTheGround;

	// AI
	CMovementRequest m_aiRequest;

	// Network related
	CNetActionSync<CNetworkMovementHelicopterArcade> m_netActionSync;

	friend class CNetworkMovementHelicopterArcade;
};

#endif
#endif //0
