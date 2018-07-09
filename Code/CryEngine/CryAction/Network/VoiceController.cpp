// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VoiceController.h"
#include "GameClientChannel.h"
#include "GameContext.h"

#ifndef OLD_VOICE_SYSTEM_DEPRECATED

CVoiceController::CVoiceController(CGameContext* pGameContext)
{
	m_pNetContext = pGameContext->GetNetContext();
	m_pMicrophone = NULL;

	m_enabled = false;
	m_recording = false;

	if (!pGameContext->HasContextFlag(eGSF_LocalOnly))
	{
		m_pMicrophone = gEnv->pAudioSystem->CreateMicrophone(0, 0, 0, 0);

		if (m_pMicrophone)
			m_recording = m_pMicrophone->Record(0, 16, 8000, 16000);
	}
	else
		m_pMicrophone = 0;
}

CVoiceController::~CVoiceController()
{
	if (m_pMicrophone)
		m_pMicrophone->Stop();
	SAFE_RELEASE(m_pMicrophone);
}

void CVoiceController::Enable(const bool e)
{
	if (e != m_enabled)
	{
		if (m_enabled && m_recording)
			m_DataReader.m_pMicrophone = 0;

		if (e)
			m_DataReader.m_pMicrophone = m_pMicrophone;

		m_enabled = e;
	}
}

bool CVoiceController::Init()
{
	return true;
}

void CVoiceController::PlayerIdSet(EntityId id)
{
	if (m_pNetContext && m_pNetContext->GetVoiceContext())
		m_pNetContext->GetVoiceContext()->SetVoiceDataReader(id, &m_DataReader);
}

#endif
