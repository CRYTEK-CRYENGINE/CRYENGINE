// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Implements Multiple Actor Coop Anims.

*************************************************************************/

#pragma once

#ifndef __ACTIONMULTICOOPANIM_H__
#define __ACTIONMULTICOOPANIM_H__

#include "ICryMannequin.h"

class CPlayer;

class CActionMultiCoopAnimation : public TAction<SAnimationContext>, public IEntityEventListener
{
	typedef TAction<SAnimationContext> TBaseAction;
public:

	enum EParamFlags
	{
		ePF_DisablePhysics			= BIT(0),
		ePF_ApplyDeltas					= BIT(1),
		ePF_AnimControlledCam		= BIT(2),
		ePF_HolsterItem					= BIT(3),
		ePF_UnholsterItemOnExit	= BIT(4),
		ePF_RenderNearest				= BIT(5),
		ePF_AnimCtrldMovement		= BIT(6),
	};

	typedef uint8 ParamFlags;

	DEFINE_ACTION( "MultiCoopAnim" )

	//////////////////////////////////////////////////////////////////////////

	struct SParticipant
	{
		friend class CActionMultiCoopAnimation;

		SParticipant( IAnimatedCharacter& animChar, ICharacterInstance* pCharInst, TagID tagId, uint32 contextId, const IAnimationDatabase* pAnimDb, const ParamFlags params );
		virtual ~SParticipant(){}
		virtual void Install();
		virtual void Enslave(SParticipant& slave);
		virtual void Enter();
		virtual void Exit();
		virtual void OnSlaveEnter(SParticipant& slave);
		virtual void OnSlaveExit(SParticipant& slave);
		virtual void OnAddedAsSlave();
		virtual void OnRemovedAsSlave();

		ILINE IAnimatedCharacter& GetAnimatedCharacter ( ) { return m_animChar; }
		ILINE IActionController* GetActionController ( ) { return m_pActionController; }
		ILINE bool HasFlag ( const EParamFlags flag ) const { return (m_paramFlags&flag)!=0; }
		void ApplyDeltaTransform( const QuatT& delta );
		IEntity* GetAndValidateEntity();

	protected:
		void SetRenderNearest(IEntity& entity, const bool bSet);

		const EntityId m_entityId;
		IAnimatedCharacter& m_animChar;
		ICharacterInstance* m_pCharInst;
		const IAnimationDatabase* m_pAnimDb;
		IActionController* m_pActionController;
		SParticipant* m_pMaster;
		const TagID m_tagId;
		const uint32 m_contextId;
		const ParamFlags m_paramFlags;
		uint8 m_entityValid;
	};

	//////////////////////////////////////////////////////////////////////////

	struct SPlayerParticipant : public SParticipant
	{
		friend class CActionMultiCoopAnimation;

		SPlayerParticipant( CPlayer& player, TagID tagId, uint32 contextId, const IAnimationDatabase* pAnimDb, const ParamFlags params );
		virtual void Enter();
		virtual void Exit();
		virtual void OnSlaveEnter(SParticipant& slave);
		virtual void OnSlaveExit(SParticipant& slave);
		virtual void OnAddedAsSlave();
		virtual void OnRemovedAsSlave();
	protected:
		CPlayer& m_player;
	};

	//////////////////////////////////////////////////////////////////////////

	explicit CActionMultiCoopAnimation( const QuatT& animSceneRoot, FragmentID fragId, TagState tagState, CPlayer& player, const ParamFlags masterParams );
	~CActionMultiCoopAnimation();

	// IEntityEventListener
	virtual void OnEntityEvent( IEntity *pEntity, const SEntityEvent& event ) override;
	// ~IEntityEventListener

	void AddSlave( SParticipant* pSlave );
	void UpdateSceneRoot( const QuatT& sceneRoot );

	virtual EStatus Update(float timePassed) override;

protected:
	virtual void Install() override;
	virtual void Fail(EActionFailure actionFailure) override;
	virtual void Enter() override;
	virtual void Exit() override;

private:
	void RegisterForEntityEvents(const EntityId entityId);
	void UnregisterForEntityEvents(const EntityId entityId);

protected:
	QuatT m_animSceneRoot;
	QuatT m_updatedSceneRoot;
	SPlayerParticipant m_master;

	typedef std::list<SParticipant*> TSlaveList;
	TSlaveList m_slaves;

	uint8 m_dirtyUpdatedSceneRoot:1;

private:
	CActionMultiCoopAnimation(); // DO NOT IMPLEMENT!!!
};



#endif// __ACTIONMULTICOOPANIM_H__
