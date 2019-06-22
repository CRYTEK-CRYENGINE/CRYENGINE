// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ISubtitleManager.h"

class CSubtitleManager : public ISubtitleManager
{
public:
	CSubtitleManager();
	virtual ~CSubtitleManager();

	// ISubtitleManager
	virtual void SetHandler(ISubtitleHandler* pHandler) { m_pHandler = pHandler; }
	virtual void SetEnabled(bool bEnabled);
	virtual void SetAutoMode(bool bOn);
	virtual void ShowSubtitle(const char* subtitleLabel, bool bShow);
	// ~ISubtitleManager

	static void OnAudioTriggerStarted(const CryAudio::SRequestInfo* const pAudioRequestInfo);
	static void OnAudioTriggerFinished(const CryAudio::SRequestInfo* const pAudioRequestInfo);

protected:
	void ShowSubtitle(const CryAudio::SRequestInfo* const pAudioRequestInfo, bool bShow);

	ISubtitleHandler*        m_pHandler;
	bool                     m_bEnabled;
	bool                     m_bAutoMode;

	static CSubtitleManager* s_Instance;
};
