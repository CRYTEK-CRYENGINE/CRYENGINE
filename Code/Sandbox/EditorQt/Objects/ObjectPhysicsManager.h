// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __ObjectPhysicsManager_h__
#define __ObjectPhysicsManager_h__

#if _MSC_VER > 1000
	#pragma once
#endif

//////////////////////////////////////////////////////////////////////////
class CObjectPhysicsManager
{
public:
	CObjectPhysicsManager();
	~CObjectPhysicsManager();

	void SimulateSelectedObjectsPositions();
	void Update();

	//////////////////////////////////////////////////////////////////////////
	/// Collision Classes
	//////////////////////////////////////////////////////////////////////////
	int  RegisterCollisionClass(const SCollisionClass& collclass);
	int  GetCollisionClassId(const SCollisionClass& collclass);
	void SerializeCollisionClasses(CXmlArchive& xmlAr);

	void PrepareForExport();

	void Command_SimulateObjects();
	void Command_GetPhysicsState();
	void Command_ResetPhysicsState();
	void Command_GenerateJoints();

private:

	void UpdateSimulatingObjects();

	bool                                 m_bSimulatingObjects;
	float                                m_fStartObjectSimulationTime;
	int                                  m_wasSimObjects;
	std::vector<_smart_ptr<CBaseObject>> m_simObjects;
	CWaitProgress*                       m_pProgress;

	typedef std::vector<SCollisionClass> TCollisionClassVector;
	int                   m_collisionClassExportId;
	TCollisionClassVector m_collisionClasses;
};

#endif // __ObjectPhysicsManager_h__

