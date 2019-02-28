// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   birdsflock.cpp
//  Version:     v1.00
//  Created:     72010 by Luciano Morpurgo (refactored from flock.cpp).
//  Compilers:   Visual C++ 7.0
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "BirdsFlock.h"
#include "BoidBird.h"

#include <CryAISystem/IAIObject.h>
#include <IPerceptionManager.h>

#define START_LANDING_TIME 3


CBirdsFlock::CBirdsFlock( IEntity *pEntity ) : CFlock( pEntity,EFLOCK_BIRDS ), m_bAttractOutput(false)
{ 
	m_boidEntityName = "BirdBoid";  
	m_landCollisionCallback = functor(*this,&CBirdsFlock::LandCollisionCallback);

};

void CBirdsFlock::CreateBoids( SBoidsCreateContext &ctx )
{
	CFlock::CreateBoids(ctx);
	m_boidDefaultAnimName =  m_bc.GetAnimationName(Bird::ANIM_FLY);

	m_bc.bAutoTakeOff = false;

	for (uint32 i = 0; i < m_RequestedBoidsCount; i++)
	{
		CBoidObject *boid = new CBoidBird( m_bc );
		if (m_bc.bSpawnFromPoint)
		{
			CBoidBird *pBoid = (CBoidBird *)boid;
			float radius	= 0.5; // Spawn not far from origin
			float z = .25f*Boid::Frand() + .25f;	// z-heading = 0.0 - 0.5
			pBoid->m_pos	= m_origin + Vec3(radius*Boid::Frand(),radius*Boid::Frand(), radius*Boid::Frand());
			pBoid->m_heading		= ( Vec3(Boid::Frand(),Boid::Frand(),z) ).GetNormalized(); 
			pBoid->m_scale			= m_bc.boidScale + Boid::Frand()*m_bc.boidRandomScale;
			boid->m_speed				= m_bc.MinSpeed + (Boid::Frand()+1)/2.0f*(m_bc.MaxSpeed - m_bc.MinSpeed);
			pBoid->m_dead				= 0;
			pBoid->m_currentAccel(0,0,0);
			pBoid->SetSpawnFromPt(true);
			pBoid->m_fNoCenterAttract = m_bc.factorAttractToOrigin;
			pBoid->m_fNoKeepHeight		= m_bc.factorKeepHeight;
			pBoid->SetAttracted(false);
		}
		else
		{
			float radius = m_bc.fSpawnRadius;
			boid->m_pos = m_origin + Vec3(radius*Boid::Frand(),radius*Boid::Frand(),Boid::Frand()*radius);
			boid->m_heading = ( Vec3(Boid::Frand(),Boid::Frand(),0) ).GetNormalized();
			boid->m_scale = m_bc.boidScale + Boid::Frand()*m_bc.boidRandomScale;
			boid->m_speed = m_bc.MinSpeed + (Boid::Frand()+1)/2.0f*(m_bc.MaxSpeed - m_bc.MinSpeed);
		}

		AddBoid(boid);
	}
	m_lastUpdatePosTimePassed = 1;
	m_avgBoidPos = GetPos();
	//
	m_hasLanded = false;
	TakeOff();
	
	m_terrainPoints = 0;
}


void CBirdsFlock::Reset()
{
	CFlock::Reset();
	m_LandingPoints.clear();
	m_landCollisionInfo.Reset();
	m_terrainPoints = 0;
	m_hasLanded = false;

}

void CBirdsFlock::Update( CCamera *pCamera )
{
	
	UpdateLandingPoints();

	m_isPlayerNearOrigin = GetEntity()!=NULL && IsPlayerInProximity(GetEntity()->GetPos());

	CFlock::Update(pCamera);
	
	float dt = gEnv->pTimer->GetFrameTime();

	// set the ai object in the middle of the boids
	UpdateAvgBoidPos(dt);

	if(m_status == Bird::FLYING && !m_bc.noLanding && 
		(gEnv->pTimer->GetFrameStartTime()- m_flightStartTime).GetSeconds() > m_flightDuration)
	{
		if(GetEntity() && !m_isPlayerNearOrigin)
			Land();
	}
	
	if (m_status == Bird::LANDING)
	{
		float timePassed = (gEnv->pTimer->GetFrameStartTime()- m_flightStartTime).GetSeconds(); 
		if(timePassed < START_LANDING_TIME+0.5f)
		{
			int n = m_boids.size();
			int numBoidsLandingNow = int(float(n*timePassed)/START_LANDING_TIME);
			if(numBoidsLandingNow >n)
				numBoidsLandingNow = n;
			for(int i = 0; i<numBoidsLandingNow ;++i)
			{
				CBoidBird* bird = static_cast<CBoidBird*>(m_boids[i]);
				if(bird && !bird->IsDead() && !bird->IsLanding())
					bird->Land();
			}
		}

	}

	
	if(m_status == Bird::LANDING || m_status == Bird::ON_GROUND)
	{
		if(IsPlayerInProximity(GetAvgBoidPos()))
			TakeOff();
	}

}


void CBirdsFlock::SetAttractionPoint(const Vec3 &point)
{
	m_bc.attractionPoint	= point;
	SetAttractOutput(false);

	for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
	{
		((CBoidBird *)*it)->SetAttracted();
	}
}

void CBirdsFlock::SetEnabled( bool bEnabled )
{
	if (!m_bc.bSpawnFromPoint)
		CFlock::SetEnabled(bEnabled);
	else
	{
		if (m_bEnabled != bEnabled)
		{
			SetAttractOutput(false);
			m_bEnabled	= bEnabled;
			for (Boids::iterator it = m_boids.begin(); it != m_boids.end(); ++it)
			{
				CBoidBird *pBoid = (CBoidBird *)*it;
				float radius	= 0.5; // Spawn not far from origin
				float z = .25f*Boid::Frand() + .25f;	// z-heading = 0.0 - 0.5
				pBoid->m_pos	= m_origin + Vec3(radius*Boid::Frand(),radius*Boid::Frand(), radius*Boid::Frand());
				
				pBoid->m_heading		= ( Vec3(Boid::Frand(),Boid::Frand(),z) ).GetNormalized(); 
				pBoid->m_scale			= m_bc.boidScale + Boid::Frand()*m_bc.boidRandomScale;
				pBoid->m_speed				= m_bc.MinSpeed + (Boid::Frand()+1)/2.0f*(m_bc.MaxSpeed - m_bc.MinSpeed);
				pBoid->m_dead				= 0;
				pBoid->m_currentAccel(0,0,0);
				pBoid->SetSpawnFromPt(true);
				pBoid->m_fNoCenterAttract = m_bc.factorAttractToOrigin;
				pBoid->m_fNoKeepHeight		= m_bc.factorKeepHeight;
				pBoid->SetAttracted(false);
			}

			if (!bEnabled && m_bEntityCreated)
				DeleteEntities(false);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CBirdsFlock::OnStimulusReceived(const SAIStimulusParams& params)
{
	CFlock::OnStimulusReceived(params);
	if(params.type == AISTIM_SOUND)
	{
		if(params.threat > 0 && Distance::Point_PointSq(params.position, m_bc.flockPos) < sqr(params.radius * params.threat))
			TakeOff();
	}
}

//////////////////////////////////////////////////////////////////////////
Vec3& CBirdsFlock::FindLandSpot()
{
	return m_defaultLandSpot;
}


//////////////////////////////////////////////////////////////////////////

void CBirdsFlock::Land()
{
/*
	for (Boids::iterator it = m_boids.begin(),itEnd = m_boids.end(); it != itEnd; ++it)
	{
		CBoidBird* pBoid = static_cast<CBoidBird*>(*it);
		if(pBoid && !pBoid->IsDead())
			pBoid->Land();
	}*/

	m_status = Bird::LANDING;
	m_flightStartTime = gEnv->pTimer->GetFrameStartTime();
}

//////////////////////////////////////////////////////////////////////////

void CBirdsFlock::TakeOff()
{
	for (Boids::iterator it = m_boids.begin(),itEnd = m_boids.end(); it != itEnd; ++it)
	{
		CBoidBird* pBoid = static_cast<CBoidBird*>(*it);
		if(pBoid && !pBoid->IsDead())
			pBoid->TakeOff(m_bc);
	}

	m_flightDuration = GetFlightDuration();
	m_flightStartTime = gEnv->pTimer->GetFrameStartTime();
	m_status = Bird::FLYING;
	m_birdsOnGround = 0;
}

//////////////////////////////////////////////////////////////////////////

float CBirdsFlock::GetFlightDuration()
{
	if(!m_hasLanded && !m_bc.noLanding && m_bc.bStartOnGround)
		return 2.f;
	return m_bc.flightTime * (1.f + Boid::Frand()*0.2f);
}

//////////////////////////////////////////////////////////////////////////

int CBirdsFlock::GetNumAliveBirds()
{
	int numAliveBoids = 0;
	for (Boids::iterator it = m_boids.begin(),itEnd = m_boids.end(); it != itEnd; ++it)
	{
		CBoidBird* pBoid = static_cast<CBoidBird*>(*it);
		if(pBoid && !pBoid->IsDead())
			++numAliveBoids;
	}
	return numAliveBoids;

}

//////////////////////////////////////////////////////////////////////////

void CBirdsFlock::NotifyBirdLanded()
{
	++m_birdsOnGround;
	if(m_status != Bird::ON_GROUND) // just optimization
	{
		if(m_birdsOnGround > 2 * GetNumAliveBirds() / 3)
			m_status = Bird::ON_GROUND;
	}
	m_hasLanded = true;
}

//////////////////////////////////////////////////////////////////////////

void CBirdsFlock::UpdateLandingPoints()
{
	int numPoints = m_boids.size()*3 /2;
	if(m_LandingPoints.size() < (size_t) numPoints )
	{
		if(!m_landCollisionInfo.IsRequestingRayCast())
		{
			float maxradius = m_bc.fSpawnRadius *0.6f;
			Vec3 origin(m_bc.flockPos);
			float angle = Boid::Frand()*gf_PI;
			float radius = (Boid::Frand()+1)/2 * maxradius;

			origin += Vec3(cos_tpl(angle)*radius, sin_tpl(angle)*radius, 5.f);

			Vec3 vDir = Vec3(0,0,-10);
			m_landCollisionInfo.QueueRaycast(m_pEntity->GetId(),origin,vDir,&m_landCollisionCallback);
		}
	}
}

////////////////////////////////////////////////

void CBirdsFlock::LandCollisionCallback(const QueuedRayID& rayID, const RayCastResult& result)
{
	if (result.hitCount)
	{
		Vec3 hitpt = result.hits[0].pt;

		hitpt.z += m_bc.groundOffset;

		for(TLandingPoints::iterator it = m_LandingPoints.begin(), itEnd = m_LandingPoints.end(); it != itEnd; ++it)
		{
			if(Distance::Point_PointSq(*it,hitpt)< 0.04f)
				return;
		}

		SLandPoint lpt(hitpt);
//  			if(hit.bTerrain >0)
// 				++m_terrainPoints;
		
		m_LandingPoints.push_back(lpt);
/*
		// prevent birds to walk if there are too many non-terrain objects in the area
		if(m_LandingPoints.size() == numPoints)
		{
			if(m_terrainPoints < 2 * numPoints / 3)
				m_bc.walkSpeed = 0;
		}*/

	}
}

/////////////////////////////////////////////////////////////////////////

Vec3 CBirdsFlock::GetLandingPoint(Vec3& fromPos)
{
	float minDist2 = 100000000.f;
	TLandingPoints::iterator itEnd = m_LandingPoints.end(), itFound = m_LandingPoints.end();
	for(TLandingPoints::iterator it = m_LandingPoints.begin(); it != itEnd; ++it)
	{
		if(!it->bTaken)
		{
			Vec3 landPoint = *it;
			float dist2 = Distance::Point_PointSq(landPoint,fromPos);
			if(dist2< minDist2)
			{
				minDist2 = dist2;
				itFound = it;
			}		
		}
	}	
	if(itFound!=itEnd )
	{
		itFound->bTaken = true;
		return *itFound;
	}
	return ZERO;
}

/////////////////////////////////////////////////////////////////////////////////////

void CBirdsFlock::LeaveLandingPoint(Vec3& point)
{
	TLandingPoints::iterator itEnd = m_LandingPoints.end();
	for(TLandingPoints::iterator it = m_LandingPoints.begin(); it != itEnd; ++it)
		if(point == Vec3(*it))
		{
			it->bTaken = false;
			return;
		}
}

/////////////////////////////////////////////////////////////////////////////////////

bool CBirdsFlock::IsPlayerInProximity(const Vec3& pos) const
{
	const IActor* pClientPlayer = g_pGame->GetIGameFramework()->GetClientActor();
	if(pClientPlayer != NULL && pClientPlayer->GetEntity())
	{
		if(Distance::Point_PointSq(pos,pClientPlayer->GetEntity()->GetPos()) <
			m_bc.fSpawnRadius * m_bc.fSpawnRadius )
			return true;
	}
	return false;
}
