// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   ---------------------------------------------------------------------
   File name:   PersonalSignalTimer.h
   $Id$
   $DateTime$
   Description: Manager per-actor signal timers
   ---------------------------------------------------------------------
   History:
   - 07:05:2007 : Created by Ricardo Pillosu

 *********************************************************************/
#ifndef _PERSONAL_SIGNAL_TIMER_H_
#define _PERSONAL_SIGNAL_TIMER_H_

#include "AIProxy.h"

class CSignalTimer;

class CPersonalSignalTimer : public IAIProxyListener
{

public:
	// Base ----------------------------------------------------------
	CPersonalSignalTimer(CSignalTimer* pParent);
	virtual ~CPersonalSignalTimer();
	bool Init(EntityId Id, const char* sSignal);
	bool Update(float fElapsedTime, uint32 uDebugOrder = 0);
	void ForceReset(bool bAlsoEnable = true);
	void OnProxyReset();

	// Utils ---------------------------------------------------------
	void          SetEnabled(bool bEnabled);
	void          SetRate(float fNewRateMin, float fNewRateMax);
	EntityId      GetEntityId() const;
	const string& GetSignalString() const;

private:
	void           Reset(bool bAlsoEnable = true);
	void           SendSignal();
	IEntity*       GetEntity();
	IEntity const* GetEntity() const;
	void           DebugDraw(uint32 uOrder) const;

	// IAIProxyListener
	void         SetListener(bool bAdd);
	virtual void OnAIProxyEnabled(bool bEnabled);
	// ~IAIProxyListener

private:

	bool          m_bInit;
	CSignalTimer* m_pParent;
	EntityId      m_EntityId;
	string        m_sSignal;
	float         m_fRateMin;
	float         m_fRateMax;
	float         m_fTimer;
	float         m_fTimerSinceLastReset;
	int           m_iSignalsSinceLastReset;
	bool          m_bEnabled;
	IFFont*       m_pDefaultFont;
};
#endif // _PERSONAL_SIGNAL_TIMER_H_
