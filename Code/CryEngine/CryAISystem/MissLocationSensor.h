// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   MissLocationSensor.h
   $Id$
   Description:

   -------------------------------------------------------------------------
   History:
   - 29 Mar 2010			 : Alex McCarthy: extracted from AIPlayer.h/cpp

 *********************************************************************/
#ifndef __MISS_LOCATION_SENSOR__
#define __MISS_LOCATION_SENSOR__

class CMissLocationSensor
{
	struct MissLocation
	{
		enum EType
		{
			Destroyable,
			Lightweight,
			Rope,
			ManuallyBreakable,
			JointStructure,
			Deformable,
			MatBreakable,
			Unbreakable,
			Undefined,
		};

		MissLocation()
			: position(ZERO)
			, type(Undefined)
			, score(0.0f)
		{
		}

		MissLocation(const Vec3& pos, EType typ)
			: position(pos)
			, type(typ)
			, score(0.0f)
		{
		}

		bool operator<(const MissLocation& rhs) const
		{
			return score < rhs.score;
		};

		Vec3  position;
		float score;
		EType type;
	};

	enum EState
	{
		Starting = 0,
		Collecting,
		Filtering,
		Finishing,
	};

	enum
	{
		MaxCollectedCount  = 384,
		MaxConsiderCount   = 32,
		MaxRopeVertexCount = 4,
		MaxRandomPool      = 6,
	};

public:
	CMissLocationSensor(const CAIActor* owner);
	virtual ~CMissLocationSensor();

	void Reset();
	void Update(float timeLimit);
	void Collect(int objTypes);
	bool Filter(float timeLimit);
	bool GetLocation(CAIObject* target, const Vec3& shootPos, const Vec3& shootDir, float maxAngle, Vec3& pos);

	void DebugDraw();

	void AddDestroyableClass(const char* className);
	void ResetDestroyableClasses();

public:
	void ClearEntities();

	typedef std::vector<IPhysicalEntity*> MissEntities;
	MissEntities m_entities;

	typedef std::vector<MissLocation> MissLocations;
	MissLocations m_locations;
	MissLocations m_working;

	MissLocations m_goodies;

	typedef std::vector<IEntityClass*> DestroyableClasses;
	DestroyableClasses m_destroyableEntityClasses;

	uint32             m_updateCount;
	Vec3               m_lastCollectionLocation;

	EState             m_state;
	const CAIActor*    m_owner;
};

#endif // __MISS_LOCATION_SENSOR__
