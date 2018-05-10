// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RADIO_H__
#define __RADIO_H__

#pragma once

#include "GameActions.h"
#include <CryInput/IInput.h>

class CGameRules;

class CRadio:public IInputEventListener
{
public:
	CRadio(CGameRules*);
	~CRadio();
	bool OnAction(const ActionId& actionId, int activationMode, float value);

	void Update();

	static const int RADIO_MESSAGE_NUM;

	//from IInputEventListener
	virtual bool	OnInputEvent( const SInputEvent &event );
	void			OnRadioMessage(int id, EntityId fromId);
	void			CancelRadio();
	void			SetTeam(const string& name);
	void			GetMemoryStatistics(ICrySizer * s);
private:
	CGameRules	*m_pGameRules;
	int			m_currentGroup;
	string		m_TeamName;
	float		m_lastMessageTime;
	float		m_menuOpenTime;		// used to close the menu if no input within n seconds.

	bool	m_keyState[10];
	bool	m_keyIgnored[10];
	int		m_requestedGroup;
	bool	m_inputEventConsumedKey;
	bool	m_waitForInputEvents;
	bool		UpdatePendingGroup();

	void		CloseRadioMenu();
};

#endif