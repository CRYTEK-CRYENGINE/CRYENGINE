// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   GameStateRecorder.h
//  Version:     v1.00
//  Created:     3/2008 by Luciano Morpurgo.
//  Compilers:   Visual Studio.NET
//  Description: Checks the player and other game specific objects' states and communicate them to the TestManager
//							 Implements IGameStateRecorder
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef __GAMESTATERECORDER_H__
#define __GAMESTATERECORDER_H__

#pragma once


#include "TestSystem/IGameStateRecorder.h"
#include "Actor.h"

#define MAX_FIRE_MODES 10
#define TItemName const char*
/*
typedef std::vector<uint8> TVectorIndex;

struct SAmmoVector
{
	std::vector<uint8> Ammo;
	SAmmoVector(): Ammo(MAX_FIRE_MODES,0)
	{
		Ammo.reserve(MAX_FIRE_MODES);
	}
	inline uint8 operator [](int i) const
	{
		return Ammo[i];
	}
};

typedef std::vector<SAmmoVector> TAmmoArray;
*/

enum 
{
	//eGE_HealthChanged = eGE_Last,
	eGE_ZoomFactor = eGE_Last,
	eGE_AmmoPickedUp,
	eGE_AccessoryPickedUp,

};

///////////////////////////////////////////////////////////////////////
inline bool equal_strings(const char *s1, const char *s2)
{
	return (s1 && s2 && !strcmp(s1,s2));
}

typedef std::map<string,uint16> TAmmoContainer;

typedef std::map<string,uint8> TAccessoryContainer;

struct SItemProperties
{
	TAmmoContainer Ammo;
	uint8 fireMode;
	uint8 count; // used for more instances of same item - would be only for an item without ammo and fire mode

	SItemProperties()
	{
		fireMode=0;
		count = 0;
		Ammo[""] = 1;
		Ammo.clear();
	}
};


typedef std::map<string,SItemProperties> TItemContainer;

struct SActorGameState
{
	float health;
	bool bHealthDifferent;
	TItemName itemSelected;
	TItemContainer Items;
	TAccessoryContainer Accessories;
	TAmmoContainer AmmoMags;

	SActorGameState()
		:
	health(0.0f),
	itemSelected(0),
	bHealthDifferent(false)
	{
	}

	void GetMemoryUsage( ICrySizer *pSizer ) const {/*LATER*/ }
};


class CGameStateRecorder: public IGameStateRecorder, IGameplayListener
{
public:
	
	CGameStateRecorder();
	~CGameStateRecorder();
	virtual void Enable(bool bEnable, bool bRecording);
	void GetMemoryStatistics(ICrySizer * s);
	virtual void RegisterListener(IGameplayListener* pL);
	virtual void UnRegisterListener(IGameplayListener* pL);
	virtual float RenderInfo(float y, bool bRecording);
	inline virtual bool	IsEnabled() {return m_mode!=0;}
	virtual void	Update();
	virtual void	Release();
	virtual void	OnRecordedGameplayEvent(IEntity *pEntity, const GameplayEvent &event, int currentFrame, bool bRecording=false);

	// implements IGameplayListener
	virtual void OnGameplayEvent(IEntity *pEntity, const GameplayEvent &event);
	void	SendGamePlayEvent(IEntity *pEntity, const GameplayEvent &event);
	
private:
/*
	struct RecordGameEventFtor {
		RecordGameEventFtor(CGameStateRecorder* pGSR)
		{
			m_pGameStateRecorder = pGSR;
		}

		void operator()(CActor* pActor, const GameplayEvent& event)
		{
			m_pGameStateRecorder->SendGamePlayEvent(pActor->GetEntity(),event);
		}
		CGameStateRecorder* m_pGameStateRecorder;
	};

	struct PlayGameEventFtor {
		PlayGameEventFtor(CGameStateRecorder* pGSR)
		{
			m_pGameStateRecorder = pGSR;
		}
		void operator()(IEntity *pEntity, const GameplayEvent& event)
		{
			
		}
		CGameStateRecorder* m_pGameStateRecorder;
	};
*/
/*
	template <typename TRec> bool CheckDifference( TRec rec, TRec curr, const char* message,...)
	{
		if( m_bLogWarning && rec != curr)
		{
			char buffer[512];
			va_list args;
			va_start(args, rec);			
			cry_vsprintf(buffer, message, args);
			va_end(args);
			GameWarning(buffer,rec,curr);
		}
	}
*/
	inline bool CheckDifference( float rec, float	curr, const char* message, IEntity* pentity, bool* pbDifferent = 0)
	{
		bool bDiff = !m_bRecording && rec != curr;
		if(bDiff)
		{
			if(!pbDifferent || !*pbDifferent)
				GameWarning(message, m_currentFrame, pentity->GetName(), rec,curr);
		}
		if(pbDifferent)
			*pbDifferent = bDiff;

		return bDiff;
	}

	inline bool CheckDifference( int rec, int	curr, const char* message, IEntity* pentity, bool* pbDifferent = 0)
	{
		bool bDiff = !m_bRecording && rec != curr;
		if(bDiff)
		{
			if(!pbDifferent || !*pbDifferent)
				GameWarning(message, m_currentFrame, pentity->GetName(), rec,curr);
		}
		if(pbDifferent)
			*pbDifferent = bDiff;

		return bDiff;
	}

	template <typename T2> inline bool CheckDifference( float rec, float curr, const char* message, IEntity* pentity, T2 val)
	{
		if( !m_bRecording && rec != curr)
		{
			GameWarning(message, m_currentFrame, pentity->GetName(), val,rec,curr);
			return true;
		}
		return false;
	}

	template <typename T2> inline bool CheckDifference( int rec, int curr, const char* message, IEntity* pentity, T2 val)
	{
	 if( !m_bRecording && rec != curr)
	 {
		 GameWarning(message,m_currentFrame, pentity->GetName(), val,rec,curr);
		 return true;
	 }
	 return false;
	}


	bool CheckDifferenceString(const char* rec, const char* curr, const char* message,IEntity* pEntity)
	{
		const char* recOut = (rec? rec:"(null)");
		const char* currOut = (curr? curr:"(null)");
		if(!m_bRecording  && !equal_strings(recOut,currOut))
		{
			GameWarning(message,m_currentFrame,pEntity->GetName(),rec,curr );
			return true;
		}
		return false;
	}

	void	DumpWholeGameState(const CActor* pActor);

	TItemName	GetItemName(const char* desc, bool bAddIfNotFound = false);
	TItemName	GetItemName(EntityId id, CItem** pItemOut =0);
	CItem* GetItemOfName(CActor* pActor, TItemName itemName);
	CActor* GetActorOfName(const char* name);
	void AddActorToStats(const CActor* pActor);
	void StartSession();
	void AddToStatsActorsOfType(unsigned short int nType);

	/*template <class EventHandlerFunc>*/ void CheckInventory(CActor* pActor, IItem *pItem);//, EventHandlerFunc eventHandler);

	typedef std::vector<IGameplayListener*> TListeners;
	typedef std::vector<int> TGameEventContainer;

	//typedef std::map<EntityId,float> TMapEntityFloat;
	typedef std::map<EntityId,SActorGameState> TGameStates;

	TListeners m_listeners; // GameStateRecorder's listeners

	EGameProfileMode m_mode;
	TGameStates m_GameStates;
	bool m_bRecording;
	bool m_bLogWarning;
	int m_currentFrame;
	TGameEventContainer m_IgnoredEvents;
	
	TGameStates::iterator m_itSingleActorGameState;

	CActor* m_pSingleActor;
	int m_demo_forceGameState;
	ICVar* m_demo_actorInfo;
	ICVar* m_demo_actorFilter;
	bool m_bEnable;
};

#endif
