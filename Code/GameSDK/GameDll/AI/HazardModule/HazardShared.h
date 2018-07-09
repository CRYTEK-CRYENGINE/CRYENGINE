// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Various class and types that are shared between the various hazard module classes.

#pragma once

#ifndef HazardShared_h
#define HazardShared_h


namespace HazardSystem
{


// Forward declarations:
class HazardModule;


// A unique ID with which we can identify any hazard that has been
// registered to the hazard module.
typedef uint32 HazardID;
static const HazardID gUndefinedHazardInstanceID = (HazardID)0;


// A unique hazard ID for projectiles (implemented as structure for additional
// type safety and to allow for compiler optimizations)!
class HazardProjectileID
{
public:
	HazardProjectileID() : m_ID(gUndefinedHazardInstanceID) {}
	explicit HazardProjectileID(HazardID hazardID) : m_ID(hazardID) {}	

	// Life-time:
	void                                Serialize(TSerialize ser);

	// Queries:
	HazardID                            GetHazardID() const { return m_ID; }	
	bool                                IsDefined() const { return (m_ID != gUndefinedHazardInstanceID); }
	
	// Manipulations:
	void								Undefine() { m_ID = gUndefinedHazardInstanceID; }


private:
	// The unique hazard ID or gUndefinedHazardInstanceID if undefined.
	HazardID                            m_ID;
};



// A unique hazard ID for spheres (implemented as structure for additional
// type safety and to allow for compiler optimization)!
class HazardSphereID
{
public:
	HazardSphereID() : m_ID(gUndefinedHazardInstanceID) {}
	explicit HazardSphereID(HazardID hazardID) : m_ID(hazardID) {}

	// Life-time:
	void                                Serialize(TSerialize ser);

	// Queries:
	HazardID                            GetHazardID() const { return m_ID; }	
	bool                                IsDefined() const { return (m_ID != gUndefinedHazardInstanceID); }

	// Manipulations:
	void								Undefine() { m_ID = gUndefinedHazardInstanceID; }


private:
	// The unique hazard ID or gUndefinedHazardInstanceID if undefined.
	HazardID                            m_ID;
};


};


#endif // HazardShared_h
