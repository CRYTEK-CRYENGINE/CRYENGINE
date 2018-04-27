// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// The internal representation of a hazardous sphere area within the hazard system.

#pragma once

#ifndef HazardSphere_h
#define HazardSphere_h

#include <CryMath/Cry_Vector3.h>
#include <CryNetwork/ISerialize.h>

#include "Hazard.h"

// Forward declarations:
struct IPhysicalEntity;


namespace HazardSystem
{

// Forward declarations:
class HazardModule;
class HazardSphereID;


// The context information of a hazard entry for sphere shapes.
class HazardSphere
{
public:
	// The center position of the sphere in world-space.
	Vec3                                m_CenterPos;

	// The radius of the sphere (>= 0.0) (in meters).
	float                               m_Radius;


public:
	HazardSphere();
	HazardSphere(const Vec3& centerPos, const float radius);
	~HazardSphere();

	void                                Serialize(TSerialize ser);
};


// Special instance of a hazard entry for sphere shapes.
class HazardDataSphere  : public HazardData
{
public:
	// The context information.
	HazardSphere						m_Context;

	// The squared radius of the sphere (>= 0.0).
	float                               m_RadiusSq;


public:
	HazardDataSphere();	
	HazardDataSphere(const HazardDataSphere& source);

	void                                Serialize(TSerialize ser);

	// Queries:
	HazardSphereID						GetTypeInstanceID() const;
	virtual const Vec3&                 GetNormal() const;
	virtual bool                        IsAgentAwareOfDanger(const Agent& agent, const Vec3& avoidPos) const;

	// Collisions:
	virtual void                        CheckCollision(Agent& agent, HazardCollisionResult* result) const;

#if !defined(_RELEASE)

	// Debug:
	virtual void						DebugRender(IPersistantDebug& debugRenderer) const;

#endif // !defined(_RELEASE)
};


};


#endif // HazardSphere_h
