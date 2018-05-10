// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   SignalTimer.h
   $Id$
   $DateTime$
   Description: Signal entities based on configurable timers
   ---------------------------------------------------------------------
   History:
   - 16:04:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#ifndef _SIGNAL_TIMER_H_
#define _SIGNAL_TIMER_H_

class CPersonalSignalTimer;

class CSignalTimer
{

public:

	/*$1- Singleton Stuff ----------------------------------------------------*/
	static CSignalTimer& ref();
	static bool          Create();
	static void          Shutdown();
	void                 Reset();

	/*$1- IEditorSetGameModeListener -----------------------------------------*/
	void OnEditorSetGameMode(bool bGameMode);
	void OnProxyReset(EntityId IdEntity);

	/*$1- Basics -------------------------------------------------------------*/
	void Init();
	bool Update(float fElapsedTime);

	/*$1- Utils --------------------------------------------------------------*/
	bool EnablePersonalManager(EntityId IdEntity, const char* sSignal);
	bool DisablePersonalSignalTimer(EntityId IdEntity, const char* sSignal);
	bool ResetPersonalTimer(EntityId IdEntity, const char* sSignal);
	bool EnableAllPersonalManagers(EntityId IdEntity);
	bool DisablePersonalSignalTimers(EntityId IdEntity);
	bool ResetPersonalTimers(EntityId IdEntity);
	bool SetTurnRate(EntityId IdEntity, const char* sSignal, float fTime, float fTimeMax = -1.0f);
	void SetDebug(bool bDebug);
	bool GetDebug() const;

protected:

	/*$1- Creation and destruction via singleton -----------------------------*/
	CSignalTimer();
	virtual ~CSignalTimer();

	/*$1- Utils --------------------------------------------------------------*/
	CPersonalSignalTimer* GetPersonalSignalTimer(EntityId IdEntity, const char* sSignal) const;
	CPersonalSignalTimer* CreatePersonalSignalTimer(EntityId IdEntity, const char* sSignal);

private:

	/*$1- Members ------------------------------------------------------------*/
	bool                               m_bInit;
	bool                               m_bDebug;
	static CSignalTimer*               m_pInstance;
	std::vector<CPersonalSignalTimer*> m_vecPersonalSignalTimers;
};
#endif // _SIGNAL_TIMER_H_
