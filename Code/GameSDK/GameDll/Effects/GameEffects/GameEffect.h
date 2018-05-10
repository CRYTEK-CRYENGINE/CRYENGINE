// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _GAME_EFFECT_
#define _GAME_EFFECT_

#pragma once

#include "ItemString.h"
#include "Effects/GameEffectsSystemDefines.h"
#include "IViewSystem.h"

// Forward declares
struct SGameEffectParams;

//==================================================================================================
// Name: Flag macros
// Desc: Flag macros to make code more readable
// Author: James Chilvers
//==================================================================================================
#define SET_FLAG(currentFlags,flag,state) ((state)?(currentFlags|=flag):(currentFlags&=~flag));
#define IS_FLAG_SET(currentFlags,flag) ((currentFlags&flag)?true:false)
//--------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: EGameEffectFlags
// Desc: Game effect flags
// Author: James Chilvers
//==================================================================================================
enum EGameEffectFlags
{
	GAME_EFFECT_INITIALISED											= (1<<0),
	GAME_EFFECT_RELEASED												=	(1<<1),
	GAME_EFFECT_AUTO_RELEASE										= (1<<2),	// Release called when Game Effect System is destroyed
	GAME_EFFECT_AUTO_DELETE											= (1<<3), // Delete is called when Game Effect System is destroyed
	GAME_EFFECT_AUTO_UPDATES_WHEN_ACTIVE				= (1<<4),
	GAME_EFFECT_AUTO_UPDATES_WHEN_NOT_ACTIVE		= (1<<5),
	GAME_EFFECT_REGISTERED											= (1<<6),
	GAME_EFFECT_ACTIVE													= (1<<7),
	GAME_EFFECT_DEBUG_EFFECT										= (1<<8),// Set true for any debug effects to avoid confusion
	GAME_EFFECT_UPDATE_WHEN_PAUSED							= (1<<9),
	GAME_EFFECT_RELEASING									= (1<<10)
};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: IGameEffect
// Desc: Interface for all game effects
// Author: James Chilvers
//==================================================================================================
struct IGameEffect : public IViewSystemListener
{
	friend class CGameEffectsSystem;
public:
	virtual ~IGameEffect() {}

	virtual void	Initialise(const SGameEffectParams* gameEffectParams = NULL) = 0;
	virtual void	Release() = 0;
	virtual void	Update(float frameTime) = 0;

	virtual void	SetActive(bool isActive) = 0;

	virtual void	SetFlag(uint32 flag,bool state) = 0;
	virtual bool	IsFlagSet(uint32 flag) const = 0;
	virtual uint32 GetFlags() const = 0;
	virtual void  SetFlags(uint32 flags) = 0;

	virtual void GetMemoryUsage( ICrySizer *pSizer ) const=0;

	virtual const char* GetName() const=0;
	virtual void ResetRenderParameters() = 0;

private:
	virtual IGameEffect*	Next() const = 0;
	virtual IGameEffect*	Prev() const = 0;
	virtual void					SetNext(IGameEffect* newNext) = 0;
	virtual void					SetPrev(IGameEffect* newPrev) = 0;
};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: CGameEffect
// Desc: Game effect - Ideal for handling a specific visual game feature
// Author: James Chilvers
//==================================================================================================
class CGameEffect : public IGameEffect
{

public:
	CGameEffect();
	virtual ~CGameEffect();

	virtual void	Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	virtual void	Release() override;
	virtual void	Update(float frameTime) override;

	virtual void	SetActive(bool isActive) override;

	void					SetFlag(uint32 flag,bool state) override;
	bool					IsFlagSet(uint32 flag) const override;
	uint32				GetFlags() const override;
	void					SetFlags(uint32 flags) override;

	virtual void GetMemoryUsage( ICrySizer *pSizer ) const override
	{
		pSizer->AddObject(this, sizeof(*this));	
	}

	// IViewSystemListener
	virtual bool OnBeginCutScene(IAnimSequence* pSeq, bool bResetFX) override { return true; }
	virtual bool OnEndCutScene(IAnimSequence* pSeq) override { return true; }
	virtual bool OnCameraChange(const SCameraParams& cameraParams) override;
	// ~IViewSystemListener
	
protected:

	// General helper functions
	void					SpawnParticlesOnSkeleton(IEntity* pEntity, IParticleEmitter* pParticleEmitter, uint32 numParticles,float maxHeightScale = 1.0f) const;
	void					SetMaterialOnEntity(IMaterial* pMaterial,EntityId entityId,PodArray<int>*	pBodyAttachmentIndexArray);
	void					SetMaterialOnEntity(IMaterial* pMaterial,EntityId entityId,PodArray<ItemString>* bodyAttachmentNameArray);

	// General data functions
	static void		ReadAttachmentNames(const IItemParamsNode* paramNode,PodArray<ItemString>* attachmentNameArray);
	void					InitAttachmentIndexArray(PodArray<int> *attachmentIndexArray,PodArray<ItemString>* pAttachmentNameArray,EntityId entityId) const;
	static IMaterial*				LoadMaterial(const char* pMaterialName);
	static IParticleEffect* LoadParticleEffect(const char* pParticleEffectName);

	static bool IsEntity3rdPerson(EntityId entityId);

private:
	IGameEffect*	Next() const override;
	IGameEffect*	Prev() const override;
	void					SetNext(IGameEffect* newNext) override;
	void					SetPrev(IGameEffect* newPrev) override;

	IGameEffect*	m_prev;
	IGameEffect*	m_next;
	uint16				m_flags;

#if DEBUG_GAME_FX_SYSTEM
	CryFixedStringT<32>		m_debugName;
#endif
};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: SGameEffectParams
// Desc: Game effect parameters
// Author: James Chilvers
//==================================================================================================
struct SGameEffectParams
{
	friend class CGameEffect;

// Make constructor private to stop SGameEffectParams ever being created, should always inherit this
// for each effect to avoid casting problems
protected:
	SGameEffectParams()
	{
		autoUpdatesWhenActive = true;
		autoUpdatesWhenNotActive = false;
		autoRelease = false;
		autoDelete = false;
	}

public:
	bool	autoUpdatesWhenActive;
	bool	autoUpdatesWhenNotActive;
	bool	autoRelease;	// Release called when Game Effect System is destroyed
	bool	autoDelete;	// Delete is called when Game Effect System is destroyed
};//------------------------------------------------------------------------------------------------

#endif // _GAME_EFFECT_
