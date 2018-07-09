// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioObjectManager.h"
#include "AudioEventManager.h"
#include "AudioStandaloneFileManager.h"
#include "AudioListenerManager.h"
#include "ATLAudioObject.h"
#include "AudioCVars.h"
#include "IAudioImpl.h"
#include "SharedAudioData.h"
#include "Common/Logger.h"

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	#include <CryRenderer/IRenderAuxGeom.h>
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CAudioObjectManager::CAudioObjectManager(
  CAudioEventManager& audioEventMgr,
  CAudioStandaloneFileManager& audioStandaloneFileMgr,
  CAudioListenerManager const& listenerManager)
	: m_pIImpl(nullptr)
	, m_audioEventMgr(audioEventMgr)
	, m_audioStandaloneFileMgr(audioStandaloneFileMgr)
	, m_listenerManager(listenerManager)
{}

//////////////////////////////////////////////////////////////////////////
CAudioObjectManager::~CAudioObjectManager()
{
	if (m_pIImpl != nullptr)
	{
		Release();
	}

	for (auto const pObject : m_constructedAudioObjects)
	{
		delete pObject;
	}

	m_constructedAudioObjects.clear();
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::Init(Impl::IImpl* const pIImpl)
{
	m_pIImpl = pIImpl;

	for (auto const pObject : m_constructedAudioObjects)
	{
		CRY_ASSERT(pObject->GetImplDataPtr() == nullptr);
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		pObject->SetImplDataPtr(m_pIImpl->ConstructObject(pObject->m_name.c_str()));
#else
		pObject->SetImplDataPtr(m_pIImpl->ConstructObject());
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::Release()
{
	for (auto const pAudioObject : m_constructedAudioObjects)
	{
		m_pIImpl->DestructObject(pAudioObject->GetImplDataPtr());
		pAudioObject->Release();
	}

	m_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::Update(float const deltaTime, Impl::SObject3DAttributes const& listenerAttributes)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CPropagationProcessor::s_totalAsyncPhysRays = 0;
	CPropagationProcessor::s_totalSyncPhysRays = 0;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (deltaTime > 0.0f)
	{
		bool const listenerMoved = listenerAttributes.velocity.GetLengthSquared() > FloatEpsilon;

		auto iter = m_constructedAudioObjects.begin();
		auto iterEnd = m_constructedAudioObjects.end();

		while (iter != iterEnd)
		{
			CATLAudioObject* const pObject = *iter;

			CObjectTransformation const& transformation = pObject->GetTransformation();

			float const distance = transformation.GetPosition().GetDistance(listenerAttributes.transformation.GetPosition());
			float const radius = pObject->GetMaxRadius();

			if (radius <= 0.0f || distance < radius)
			{
				if ((pObject->GetFlags() & EObjectFlags::Virtual) != 0)
				{
					pObject->RemoveFlag(EObjectFlags::Virtual);
				}
			}
			else
			{
				if ((pObject->GetFlags() & EObjectFlags::Virtual) == 0)
				{
					pObject->SetFlag(EObjectFlags::Virtual);
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
					pObject->ResetObstructionRays();
#endif      // INCLUDE_AUDIO_PRODUCTION_CODE
				}
			}

			if (IsActive(pObject))
			{
				pObject->Update(deltaTime, distance, listenerAttributes.transformation.GetPosition(), listenerAttributes.velocity, listenerMoved);
			}
			else if (pObject->CanBeReleased())
			{
				iter = m_constructedAudioObjects.erase(iter);
				iterEnd = m_constructedAudioObjects.end();
				m_pIImpl->DestructObject(pObject->GetImplDataPtr());
				pObject->SetImplDataPtr(nullptr);

				delete pObject;
				continue;
			}

			++iter;
		}
	}
	else
	{
		for (auto const pObject : m_constructedAudioObjects)
		{
			if (IsActive(pObject))
			{
				pObject->GetImplDataPtr()->Update();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::RegisterObject(CATLAudioObject* const pObject)
{
	m_constructedAudioObjects.push_back(pObject);
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReportStartedEvent(CATLEvent* const pEvent)
{
	if (pEvent != nullptr)
	{
		CATLAudioObject* const pAudioObject = pEvent->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Event reported as started has no audio object!");
		pAudioObject->ReportStartedEvent(pEvent);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL pEvent in CAudioObjectManager::ReportStartedEvent");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess)
{
	if (pEvent != nullptr)
	{
		CATLAudioObject* const pAudioObject = pEvent->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Event reported as finished has no audio object!");
		pAudioObject->ReportFinishedEvent(pEvent, bSuccess);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL pEvent in CAudioObjectManager::ReportFinishedEvent");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::GetStartedStandaloneFileRequestData(CATLStandaloneFile* const pStandaloneFile, CAudioRequest& request)
{
	if (pStandaloneFile != nullptr)
	{
		CATLAudioObject* const pAudioObject = pStandaloneFile->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Standalone file request without audio object!");
		pAudioObject->GetStartedStandaloneFileRequestData(pStandaloneFile, request);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL _pStandaloneFile in CAudioObjectManager::GetStartedStandaloneFileRequestData");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReportFinishedStandaloneFile(CATLStandaloneFile* const pStandaloneFile)
{
	if (pStandaloneFile != nullptr)
	{
		CATLAudioObject* const pAudioObject = pStandaloneFile->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Standalone file reported as finished has no audio object!");
		pAudioObject->ReportFinishedStandaloneFile(pStandaloneFile);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL _pStandaloneFile in CAudioObjectManager::ReportFinishedStandaloneFile");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::ReleasePendingRays()
{
	for (auto const pObject : m_constructedAudioObjects)
	{
		if (pObject != nullptr)
		{
			pObject->ReleasePendingRays();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::IsActive(CATLAudioObject const* const pAudioObject) const
{
	return HasActiveData(pAudioObject);
}

//////////////////////////////////////////////////////////////////////////
bool CAudioObjectManager::HasActiveData(CATLAudioObject const* const pAudioObject) const
{
	for (auto const pEvent : pAudioObject->GetActiveEvents())
	{
		if (pEvent->IsPlaying())
		{
			return true;
		}
	}

	for (auto const& standaloneFilePair : pAudioObject->GetActiveStandaloneFiles())
	{
		CATLStandaloneFile const* const pStandaloneFile = standaloneFilePair.first;

		if (pStandaloneFile->IsPlaying())
		{
			return true;
		}
	}

	return false;
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
size_t CAudioObjectManager::GetNumAudioObjects() const
{
	return m_constructedAudioObjects.size();
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioObjectManager::GetNumActiveAudioObjects() const
{
	size_t numActiveAudioObjects = 0;
	for (auto pAudioObject : m_constructedAudioObjects)
	{
		if (IsActive(pAudioObject))
		{
			++numActiveAudioObjects;
		}
	}

	return numActiveAudioObjects;
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::DrawPerObjectDebugInfo(
  IRenderAuxGeom& auxGeom,
  Vec3 const& listenerPos,
  AudioTriggerLookup const& triggers,
  AudioParameterLookup const& parameters,
  AudioSwitchLookup const& switches,
  AudioPreloadRequestLookup const& preloadRequests,
  AudioEnvironmentLookup const& environments) const
{
	for (auto pAudioObject : m_constructedAudioObjects)
	{
		if (IsActive(pAudioObject))
		{
			pAudioObject->DrawDebugInfo(
			  auxGeom,
			  listenerPos,
			  triggers,
			  parameters,
			  switches,
			  preloadRequests,
			  environments);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioObjectManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const
{
	static float const headerColor[4] = { 1.0f, 0.5f, 0.0f, 0.7f };
	static float const itemActiveColor[4] = { 0.1f, 0.7f, 0.1f, 0.9f };
	static float const itemInactiveColor[4] = { 0.8f, 0.8f, 0.8f, 0.9f };
	static float const itemVirtualColor[4] = { 0.1f, 0.8f, 0.8f, 0.9f };

	size_t numAudioObjects = 0;
	float const headerPosY = posY;
	CryFixedStringT<MaxControlNameLength> lowerCaseSearchString(g_cvars.m_pDebugFilter->GetString());
	lowerCaseSearchString.MakeLower();

	posY += 16.0f;

	for (auto const pAudioObject : m_constructedAudioObjects)
	{
		Vec3 const& position = pAudioObject->GetTransformation().GetPosition();
		float const distance = position.GetDistance(listenerPosition);

		if (g_cvars.m_debugDistance <= 0.0f || (g_cvars.m_debugDistance > 0.0f && distance < g_cvars.m_debugDistance))
		{
			char const* const szObjectName = pAudioObject->m_name.c_str();
			CryFixedStringT<MaxControlNameLength> lowerCaseObjectName(szObjectName);
			lowerCaseObjectName.MakeLower();
			bool const hasActiveData = HasActiveData(pAudioObject);
			bool const stringFound = (lowerCaseSearchString.empty() || (lowerCaseSearchString.compareNoCase("0") == 0)) || (lowerCaseObjectName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos);
			bool const draw = stringFound && ((g_cvars.m_hideInactiveAudioObjects == 0) || ((g_cvars.m_hideInactiveAudioObjects > 0) && hasActiveData));

			if (draw)
			{
				bool const isVirtual = (pAudioObject->GetFlags() & EObjectFlags::Virtual) != 0;

				auxGeom.Draw2dLabel(posX, posY, 1.25f,
				                    isVirtual ? itemVirtualColor : (hasActiveData ? itemActiveColor : itemInactiveColor),
				                    false,
				                    "%s : %.2f",
				                    szObjectName,
				                    pAudioObject->GetMaxRadius());

				posY += 11.0f;
				++numAudioObjects;
			}
		}
	}

	auxGeom.Draw2dLabel(posX, headerPosY, 1.5f, headerColor, false, "Audio Objects [%" PRISIZE_T "]", numAudioObjects);
}

#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio
