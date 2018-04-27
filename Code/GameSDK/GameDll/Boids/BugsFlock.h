// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   bugsflock.h
//  Version:     v1.00
//  Created:     11/4/2003 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#ifndef __bugsflock_h__
#define __bugsflock_h__
#pragma once

#include "Flock.h"

/*! Single Bug.
*/
class CBoidBug : public CBoidObject
{
public:
	CBoidBug( SBoidContext &bc );
	void Update( float dt,SBoidContext &bc );
	void Render( SRendParams &rp,CCamera &cam,SBoidContext &bc );
private:
	void UpdateBugsBehavior( float dt,SBoidContext &bc );
	void UpdateDragonflyBehavior( float dt,SBoidContext &bc );
	void UpdateFrogsBehavior( float dt,SBoidContext &bc );
	//void CalcRandomTarget( const Vec3 &origin,SBoidContext &bc );
	friend class CBugsFlock;
	int m_objectId;

	//Vec3 m_targetPos;
	// Flags.
	unsigned m_onGround : 1;	//! True if landed on ground.
	//unsigned m_landing : 1;		//! True if bird wants to land.
	//unsigned m_takingoff : 1;	//! True if bird is just take-off from land.
};

/*!	Bugs Flock, is a specialized flock type for all kind of small bugs and flies around player.
*/
class CBugsFlock : public CFlock
{
public:
	CBugsFlock( IEntity *pEntity );
	~CBugsFlock();

	virtual void CreateBoids( SBoidsCreateContext &ctx );
protected:
	friend class CBoidBug;
};

#endif // __bugsflock_h__
