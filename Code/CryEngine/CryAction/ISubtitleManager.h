// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

//////////////////////////////////////////////////////////////////////////
struct ISubtitleHandler
{
	virtual ~ISubtitleHandler(){}
	virtual void ShowSubtitle(const CryAudio::SRequestInfo* const pAudioRequestInfo, bool bShow) = 0;
	virtual void ShowSubtitle(const char* subtitleLabel, bool bShow) = 0;
};

//////////////////////////////////////////////////////////////////////////
struct ISubtitleManager
{
	virtual ~ISubtitleManager(){}
	virtual void SetHandler(ISubtitleHandler* pHandler) = 0;

	// enables/disables subtitles manager
	virtual void SetEnabled(bool bEnabled) = 0;

	// automatic mode. Will inform the subtitleHandler about every executed/stopped audio trigger.
	// You can use this mode, if you want to drive your subtitles by started sounds and not manually.
	virtual void SetAutoMode(bool bOn) = 0;

	virtual void ShowSubtitle(const char* subtitleLabel, bool bShow) = 0;
};
