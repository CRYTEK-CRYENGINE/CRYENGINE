// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
*************************************************************************/

#ifndef MOCKENTITYSYSTEM_H
#define MOCKENTITYSYSTEM_H


#include <EngineFacade/EngineFacade.h>


class CMockEnginePhysicalEntity: public EngineFacade::CNullEnginePhysicalEntity
{
public:
	CMockEnginePhysicalEntity();

	virtual void SetParamsResult(int result);
	virtual int SetParams(pe_params* params);

	virtual void SetReturnParams(pe_player_dynamics params);
	virtual int GetParams(pe_params* params);

	virtual void Action(pe_action* action);
	int GetActionCount();

private:
	int setParamsResult;
	pe_player_dynamics setReturnParams;
	int m_actionCounter;
};


class CMockEngineEntity: public EngineFacade::CNullEngineEntity
{
public:
	CMockEngineEntity();

	virtual EngineFacade::IEnginePhysicalEntity& GetPhysicalEntity();

	virtual const Matrix34& GetSlotWorldTM( int nSlot ) const;
	virtual const Matrix34& GetWorldTM() const;
	virtual Ang3 GetWorldAngles() const;
	virtual	Vec3 GetWorldPos() const;
	virtual void SetWorldPos(Vec3 worldPos);

	virtual Quat GetWorldRotation() const;
	virtual void SetWorldRotation(Quat rotation);

private:
	CMockEnginePhysicalEntity m_mockedPhysicalEntity;

	Vec3 m_worldPos;
	Quat m_worldRotation;
	Ang3 m_worldAngles;
	Matrix34 m_WorldMatrix;
};


class CMockEntitySystem: public EngineFacade::CEngineEntitySystem
{
public:
	CMockEntitySystem();

	virtual EngineFacade::IEngineEntity::Ptr GetEntityByID(EntityId id);

    void Use(EngineFacade::IEngineEntity::Ptr entity);

private:
	EngineFacade::IEngineEntity::Ptr mockedEntity;
};


#endif 
