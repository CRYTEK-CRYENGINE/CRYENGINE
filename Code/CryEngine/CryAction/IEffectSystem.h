// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Manage the activation and lifetime of multiple effects.

   -------------------------------------------------------------------------
   History:
   - 17:01:2006:		Created by Marco Koegler

*************************************************************************/

#ifndef __IEFFECTSYSTEM_H__
#define __IEFFECTSYSTEM_H__
#pragma once

#include <CryGame/IGameFramework.h>

typedef int32 EffectId;

/*
   IEffects at the beginning are inactive. They can be activated through the
   IEffectSystem. At this point the effect starts to receive OnActivate() calls. It
   can transition to the next state by responding with a return value of true. As long
   as false is returned OnActivate will continue being called. Once true is returned,
   the effect will transition to the active state and OnUpdate() will get called on it

   Since activating, updating and deactivating effects are processed in that order, it
   is possible for an effect to go through this lifecycle within a single frame!!!
 */

enum EEffectState
{
	eES_Deactivated,
	eES_Activating,
	eES_Updating,
	eES_Deactivating,
};

struct IEffect
{
	virtual ~IEffect(){}

	// processing
	virtual bool Activating(float delta) = 0;
	virtual bool Update(float delta) = 0;
	virtual bool Deactivating(float delta) = 0;

	// events
	virtual bool OnActivate() = 0;
	virtual bool OnDeactivate() = 0;

	// state management and information
	virtual void         SetState(EEffectState state) = 0;
	virtual EEffectState GetState() = 0;

	virtual void         GetMemoryUsage(ICrySizer* s) const = 0;
};

struct IGroundEffect
{
	enum EGroundEffectFlags
	{
		eGEF_AlignToGround = (1 << 0),
		eGEF_AlignToOcean  = (1 << 1), // takes higher precedence than AlignToGround
		eGEF_PrimeEffect   = (1 << 2),
		eGEF_StickOnGround = (1 << 3),
	};

	virtual ~IGroundEffect(){}

	// set maximum height
	virtual void SetHeight(float height) = 0;

	// use if effect should be scaled dependending on height ratio.
	// scale(zero height) = 1,1
	// scale(max height) = sizeScale,countScale
	// use e.g. sizeScale=0,countScale=0 to fade it out completely at max height
	virtual void SetHeightScale(float sizeScale, float countScale) = 0;

	// set additional scale parameters, if user control is needed
	// these are always multiplied by height scale parameters
	virtual void SetBaseScale(float sizeScale, float countScale, float speedScale = 1.f) = 0;

	// set interpolation speed for size scale
	// speed 0 means no interpolation
	virtual void SetInterpolation(float speed) = 0;

	virtual void SetFlags(int flags) = 0;
	virtual int  GetFlags() const = 0;

	virtual bool SetParticleEffect(const char* name) = 0;

	// if an interaction name is set, the particle effect is looked up in MaterialEffects.
	// if not, one can still call SetParticleEffect by herself
	virtual void SetInteraction(const char* name) = 0;

	// must be called by user
	virtual void Update() = 0;

	//stop == true will keep the effect on hold until a stop == false is set.
	virtual void Stop(bool stop) = 0;
};

struct IEffectSystem
{
	virtual ~IEffectSystem(){}

	virtual bool           Init() = 0;
	virtual void           Update(float delta) = 0;
	virtual void           Shutdown() = 0;
	virtual void           GetMemoryStatistics(ICrySizer* s) = 0;

	virtual EffectId       GetEffectId(const char* name) = 0;

	virtual void           Activate(const EffectId& eid) = 0;
	virtual bool           BindEffect(const char* name, IEffect* pEffect) = 0;
	virtual IGroundEffect* CreateGroundEffect(IEntity* pEntity) = 0;

	DECLARE_GAMEOBJECT_FACTORY(IEffect);
};

#endif //__IEFFECTSYSTEM_H__
