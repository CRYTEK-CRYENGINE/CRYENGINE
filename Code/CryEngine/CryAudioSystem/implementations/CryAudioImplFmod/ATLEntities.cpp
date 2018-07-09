// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ATLEntities.h"
#include <CryAudio/IAudioSystem.h>
#include <CrySystem/ISystem.h> // needed for gEnv in Release builds
#include <Logger.h>
#include <fmod_common.h>
#include <fmod.hpp>

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK FileCallback(FMOD_CHANNELCONTROL* pChannelControl, FMOD_CHANNELCONTROL_TYPE controlType, FMOD_CHANNELCONTROL_CALLBACK_TYPE callbackType, void* pData1, void* pData2)
{
	if (controlType == FMOD_CHANNELCONTROL_CHANNEL && callbackType == FMOD_CHANNELCONTROL_CALLBACK_END)
	{

		FMOD::Channel* pChannel = (FMOD::Channel*)pChannelControl;
		CStandaloneFile* pStandaloneFile = nullptr;
		FMOD_RESULT const fmodResult = pChannel->getUserData(reinterpret_cast<void**>(&pStandaloneFile));
		ASSERT_FMOD_OK;

		if (pStandaloneFile != nullptr)
		{
			pStandaloneFile->m_pChannel = nullptr;
			pStandaloneFile->ReportFileFinished();
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
FMOD_RESULT F_CALLBACK ProgrammerSoundFileCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE* pEvent, void* pInOutParameters)
{
	if (pEvent != nullptr)
	{
		FMOD::Studio::EventInstance* const pEventInstance = reinterpret_cast<FMOD::Studio::EventInstance*>(pEvent);
		CProgrammerSoundFile* pFile = nullptr;
		FMOD_RESULT fmodResult = pEventInstance->getUserData(reinterpret_cast<void**>(&pFile));
		ASSERT_FMOD_OK;

		if (pFile != nullptr)
		{
			if (type == FMOD_STUDIO_EVENT_CALLBACK_CREATE_PROGRAMMER_SOUND)
			{
				CRY_ASSERT(pInOutParameters);
				FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* const pInOutProperties = reinterpret_cast<FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*>(pInOutParameters);

				// Create the sound
				FMOD::Sound* pSound = nullptr;
				fmodResult = pFile->s_pLowLevelSystem->createSound(pFile->m_fileName, FMOD_CREATESTREAM | FMOD_NONBLOCKING | FMOD_3D, nullptr, &pSound);
				ASSERT_FMOD_OK;

				// Pass the sound to FMOD
				pInOutProperties->sound = reinterpret_cast<FMOD_SOUND*>(pSound);
			}
			else if (type == FMOD_STUDIO_EVENT_CALLBACK_DESTROY_PROGRAMMER_SOUND)
			{
				CRY_ASSERT(pInOutParameters);
				FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES* const pInOutProperties = reinterpret_cast<FMOD_STUDIO_PROGRAMMER_SOUND_PROPERTIES*>(pInOutParameters);

				// Obtain the sound
				FMOD::Sound* pSound = reinterpret_cast<FMOD::Sound*>(pInOutProperties->sound);

				// Release the sound
				fmodResult = pSound->release();
				ASSERT_FMOD_OK;
			}
			else if (type == FMOD_STUDIO_EVENT_CALLBACK_STOPPED)
			{
				pFile->ReportFileFinished();
			}
		}
	}

	return FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
CStandaloneFileBase::CStandaloneFileBase(char const* const szFile, CATLStandaloneFile& atlStandaloneFile)
	: m_atlStandaloneFile(atlStandaloneFile)
	, m_fileName(szFile)
{

}

//////////////////////////////////////////////////////////////////////////
CStandaloneFileBase::~CStandaloneFileBase()
{
	if (m_pObject != nullptr)
	{
		m_pObject->RemoveFile(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CStandaloneFileBase::ReportFileStarted()
{
	gEnv->pAudioSystem->ReportStartedFile(m_atlStandaloneFile, true);
}

//////////////////////////////////////////////////////////////////////////
void CStandaloneFileBase::ReportFileFinished()
{
	gEnv->pAudioSystem->ReportStoppedFile(m_atlStandaloneFile);
}

//////////////////////////////////////////////////////////////////////////
void CStandaloneFile::StartLoading()
{
	FMOD_RESULT fmodResult = s_pLowLevelSystem->createSound(m_fileName, FMOD_CREATESTREAM | FMOD_NONBLOCKING | FMOD_3D, nullptr, &m_pLowLevelSound);

	ASSERT_FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
bool CStandaloneFile::IsReady()
{
	FMOD_OPENSTATE state = FMOD_OPENSTATE_ERROR;
	if (m_pLowLevelSound)
	{
		m_pLowLevelSound->getOpenState(&state, nullptr, nullptr, nullptr);

		if (state == FMOD_OPENSTATE_ERROR)
		{
			Cry::Audio::Log(ELogType::Error, "Failed to load audio file %s", m_fileName.c_str());
		}
	}

	return state != FMOD_OPENSTATE_LOADING;
}

//////////////////////////////////////////////////////////////////////////
void CStandaloneFile::Play(FMOD_3D_ATTRIBUTES const& attributes)
{
	FMOD_RESULT const fmodResult = s_pLowLevelSystem->playSound(m_pLowLevelSound, nullptr, true, &m_pChannel);
	ASSERT_FMOD_OK;

	if (m_pChannel != nullptr)
	{
		m_pChannel->setUserData(this);
		m_pChannel->set3DAttributes(&attributes.position, &attributes.velocity);
		m_pChannel->setCallback(FileCallback);
		m_pChannel->setPaused(false);
	}

	ReportFileStarted();
}

//////////////////////////////////////////////////////////////////////////
void CStandaloneFile::Set3DAttributes(FMOD_3D_ATTRIBUTES const& attributes)
{
	m_pChannel->set3DAttributes(&attributes.position, &attributes.velocity);
}

//////////////////////////////////////////////////////////////////////////
void CStandaloneFile::Stop()
{
	if (m_pChannel != nullptr)
	{
		FMOD_RESULT const fmodResult = m_pChannel->stop();
		ASSERT_FMOD_OK;
	}
}

//////////////////////////////////////////////////////////////////////////
void CProgrammerSoundFile::StartLoading()
{
	FMOD::Studio::EventDescription* pEventDescription = nullptr;
	FMOD_RESULT fmodResult = CObjectBase::s_pSystem->getEventByID(&m_eventGuid, &pEventDescription);
	ASSERT_FMOD_OK;

	fmodResult = pEventDescription->createInstance(&m_pEventInstance);
	ASSERT_FMOD_OK;

	fmodResult = m_pEventInstance->setUserData(this);
	ASSERT_FMOD_OK;

	fmodResult = m_pEventInstance->setCallback(ProgrammerSoundFileCallback);
	ASSERT_FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
bool CProgrammerSoundFile::IsReady()
{
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CProgrammerSoundFile::Play(FMOD_3D_ATTRIBUTES const& attributes)
{
	FMOD_RESULT fmodResult = m_pEventInstance->start();
	ASSERT_FMOD_OK;

	fmodResult = m_pEventInstance->set3DAttributes(&attributes);
	ASSERT_FMOD_OK;

}

//////////////////////////////////////////////////////////////////////////
void CProgrammerSoundFile::Set3DAttributes(FMOD_3D_ATTRIBUTES const& attributes)
{
	FMOD_RESULT fmodResult = m_pEventInstance->set3DAttributes(&attributes);
	ASSERT_FMOD_OK;
}

//////////////////////////////////////////////////////////////////////////
void CProgrammerSoundFile::Stop()
{
	FMOD_RESULT fmodResult = m_pEventInstance->stop(FMOD_STUDIO_STOP_IMMEDIATE);
	ASSERT_FMOD_OK;
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
