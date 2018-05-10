// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   flock.h
//  Version:     v1.00
//  Created:     5/4/2002 by Timur.
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __flock_h__
#define __flock_h__

#if _MSC_VER > 1000
#pragma once
#endif

#include <CryScriptSystem/IScriptSystem.h>
#include "BoidObject.h"

#define MAX_ATTRACT_DISTANCE 20
#define MIN_ATTRACT_DISTANCE 5
#define MAX_FLIGHT_HEIGHT 40
#define MIN_FLIGHT_HEIGHT 5

enum EFlockType
{
	EFLOCK_BIRDS,
	EFLOCK_FISH,
	EFLOCK_BUGS,
	EFLOCK_CHICKENS,
	EFLOCK_FROGS,
	EFLOCK_TURTLES,
};

struct SAIStimulusParams;

/*!	This is flock creation context passed to flock Init method.
*/
struct SBoidsCreateContext
{
	int boidsCount;						//! Number of boids in flock.
	std::vector<string> models;	//! Geometry models (Static or character) to be used for flock.
	string characterModel;	//! Character model.
	string animation;	//! Looped character animation.
};




//! Structure passed to CFlock::RayTest method, filled with intersection parameters.
struct SFlockHit {
	//! Hit object.
	CBoidObject *object;
	//! Distance from ray origin to the hit distance.
	float dist;
};


//////////////////////////////////////////////////////////////////////////

/*!
 *	Define flock of boids, where every boid share common properties and recognize each other.
 */
class CFlock
{
public:
	CFlock( IEntity *pEntity,EFlockType flockType );
	virtual ~CFlock();

	//! Initialize boids in flock.
	virtual void CreateBoids( SBoidsCreateContext &ctx );

	// Called when entering/leaving game mode.
	virtual void Reset();

	//! Create boids in flock.
	//! Must be overriden in derived specialized flocks.
	virtual bool CreateEntities();

	virtual CBoidObject* CreateBoid() { return 0; };

	void DeleteEntities( bool bForceDeleteAll );

	int GetId() const { return m_id; };
	EFlockType GetType() const { return m_type; };

	void SetPos( const Vec3& pos );
	Vec3 GetPos() const { return m_origin; };

	void AddBoid( CBoidObject *boid );
	int GetBoidsCount() { return m_boids.size(); }
	CBoidObject* GetBoid( int index ) { return m_boids[index]; }

	float GetMaxVisibilityDistance() const { return m_bc.maxVisibleDistance; };

	//! Retrieve general boids settings in this flock.
	void GetBoidSettings( SBoidContext &bc ) { bc = m_bc; };
	//! Set general boids settings in this flock.
	void SetBoidSettings( SBoidContext &bc );

	bool IsFollowPlayer() const { return m_bc.followPlayer; };

	void ClearBoids();

	//! Check ray to flock intersection.
	bool RayTest( Vec3 &raySrc,Vec3 &rayTrg,SFlockHit &hit );

	const char* GetModelName() const { return m_model; };

	//! Static function that initialize defaults of boids info.
	static void GetDefaultBoidsContext( SBoidContext &bc );

	//! Enable/Disable Flock to be updated and rendered.
	virtual void SetEnabled( bool bEnabled );
	//! True if this flock is enabled, and must be updated and rendered.
	bool IsEnabled() const { return m_bEnabled; }

	//! Set how much percent of flock is visible.
	//! value 0 - 100.
	void SetPercentEnabled( int percent );

	//! See if this flock must be active now.
	bool IsFlockActive();

	//! flock's container should not be saved
	bool IsSaveable() { return(false); }

	//! Get entity owning this flock.
	IEntity* GetEntity() const { return m_pEntity; }

	void UpdateBoidsViewDistRatio();

	//////////////////////////////////////////////////////////////////////////
	// IEntityContainer implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual void Update( CCamera *pCamera );
	void Render( const SRendParams &EntDrawParams );
	//////////////////////////////////////////////////////////////////////////


	void OnBoidHit( EntityId nBoidId,SmartScriptTable &hit );
	void RegisterAIEventListener( bool bEnable );

	virtual void OnStimulusReceived(const SAIStimulusParams& params);

	virtual void GetMemoryUsage(ICrySizer *pSizer )const;
	
	inline const Vec3& GetAvgBoidPos() const
	{return m_avgBoidPos;}

protected:
	void UpdateAvgBoidPos(float dt);
	virtual void UpdateBoidCollisions();

public:
	static int m_e_flocks;
	static int m_e_flocks_hunt; // Hunting mode...
	bool m_bAnyKilled;

	//! All boid parameters.
	SBoidContext m_bc;

protected:
	typedef std::vector<CBoidObject*> Boids;

	struct SBoidCollisionTime
	{
		SBoidCollisionTime( const CTimeValue& time, CBoidObject* pBoid )
			: m_time(time)
			, m_pBoid(pBoid)
		{
		}

		CTimeValue m_time;
		CBoidObject* m_pBoid;
	};

	typedef std::vector<SBoidCollisionTime> TTimeBoidMap;

	struct FSortBoidByTime
	{
		bool operator()( const SBoidCollisionTime& lhs, const SBoidCollisionTime& rhs ) const
		{
			if( lhs.m_time.GetValue() < rhs.m_time.GetValue() )
			{
				return true;
			}

			return false;
		}
	};

	Boids m_boids;
	Vec3 m_origin;

	// Bonding box.
	AABB m_bounds;

	//! Uniq id of this flock, assigned by flock manager at creation.
	int m_id;

	uint32 m_RequestedBoidsCount;
	//! Name of this flock.
	EFlockType m_type;
	
	string m_model;
	string m_modelCgf;
	string m_boidEntityName;
	string m_boidDefaultAnimName;
	int m_nBoidEntityFlagsAdd;

	int m_updateFrameID;
	int m_percentEnabled;

	int m_nViewDistRatio;

	float m_fCenterFloatingTime;

	// Host entity.
	IEntity *m_pEntity;
	// precache
	bool m_bEnabled;
	bool m_bEntityCreated;
	bool m_bAIEventListenerRegistered;

	SBoidsCreateContext m_createContext;

	Vec3 m_avgBoidPos;
	float m_lastUpdatePosTimePassed;

	TTimeBoidMap m_BoidCollisionMap;
};


#endif // __flock_h__
