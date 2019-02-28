// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SubtitleManager.h"
#include "ISubtitleManager.h"
#include "DialogSystem/DialogActorContext.h"

CSubtitleManager* CSubtitleManager::s_Instance = nullptr;

//////////////////////////////////////////////////////////////////////////
CSubtitleManager::CSubtitleManager()
{
	m_bEnabled = false;
	m_bAutoMode = true;
	m_pHandler = nullptr;

	s_Instance = this;
}

//////////////////////////////////////////////////////////////////////////
CSubtitleManager::~CSubtitleManager()
{
	SetEnabled(false);
	s_Instance = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CSubtitleManager::SetEnabled(bool bEnabled)
{
	if (bEnabled != m_bEnabled)
	{
		m_bEnabled = bEnabled;

		if (m_bAutoMode)
		{
			if (bEnabled)
			{
				gEnv->pAudioSystem->AddRequestListener(&CSubtitleManager::OnAudioTriggerStarted, nullptr, CryAudio::ESystemEvents::TriggerExecuted);
				gEnv->pAudioSystem->AddRequestListener(&CSubtitleManager::OnAudioTriggerFinished, nullptr, CryAudio::ESystemEvents::TriggerFinished);
			}
			else
			{
				gEnv->pAudioSystem->RemoveRequestListener(&CSubtitleManager::OnAudioTriggerStarted, nullptr);
				gEnv->pAudioSystem->RemoveRequestListener(&CSubtitleManager::OnAudioTriggerFinished, nullptr);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSubtitleManager::SetAutoMode(bool bOn)
{
	if (bOn != m_bAutoMode)
	{
		if (m_bEnabled)
		{
			SetEnabled(false);  //force refresh for add/remove as audio listener
			m_bAutoMode = bOn;
			SetEnabled(true);
		}
		else
		{
			m_bAutoMode = bOn;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSubtitleManager::ShowSubtitle(const char* subtitleLabel, bool bShow)
{
	if (m_bEnabled && m_pHandler)
	{
		m_pHandler->ShowSubtitle(subtitleLabel, bShow);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSubtitleManager::ShowSubtitle(const CryAudio::SRequestInfo* const pAudioRequestInfo, bool bShow)
{
	if (m_bEnabled && m_pHandler)
	{
		m_pHandler->ShowSubtitle(pAudioRequestInfo, bShow);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSubtitleManager::OnAudioTriggerStarted(const CryAudio::SRequestInfo* const pAudioRequestInfo)
{
	CSubtitleManager::s_Instance->ShowSubtitle(pAudioRequestInfo, true);
}

//////////////////////////////////////////////////////////////////////////
void CSubtitleManager::OnAudioTriggerFinished(const CryAudio::SRequestInfo* const pAudioRequestInfo)
{
	CSubtitleManager::s_Instance->ShowSubtitle(pAudioRequestInfo, false);
}
