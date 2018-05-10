// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioObjectManager.h"
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
CObjectManager::~CObjectManager()
{
	if (m_pIImpl != nullptr)
	{
		Release();
	}

	for (auto const pObject : m_constructedObjects)
	{
		delete pObject;
	}

	m_constructedObjects.clear();
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::Init(uint32 const poolSize)
{
	m_constructedObjects.reserve(static_cast<std::size_t>(poolSize));
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::SetImpl(Impl::IImpl* const pIImpl)
{
	m_pIImpl = pIImpl;

	for (auto const pObject : m_constructedObjects)
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
void CObjectManager::Release()
{
	// Don't clear m_constructedObjects here as we need the objects to survive a middleware switch!
	for (auto const pObject : m_constructedObjects)
	{
		m_pIImpl->DestructObject(pObject->GetImplDataPtr());
		pObject->Release();
	}

	m_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::Update(float const deltaTime, Impl::SObject3DAttributes const& listenerAttributes)
{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	CPropagationProcessor::s_totalAsyncPhysRays = 0;
	CPropagationProcessor::s_totalSyncPhysRays = 0;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	if (deltaTime > 0.0f)
	{
		bool const listenerMoved = listenerAttributes.velocity.GetLengthSquared() > FloatEpsilon;

		auto iter = m_constructedObjects.begin();
		auto iterEnd = m_constructedObjects.end();

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
				m_pIImpl->DestructObject(pObject->GetImplDataPtr());
				pObject->SetImplDataPtr(nullptr);
				delete pObject;

				if (iter != (iterEnd - 1))
				{
					(*iter) = m_constructedObjects.back();
				}

				m_constructedObjects.pop_back();
				iter = m_constructedObjects.begin();
				iterEnd = m_constructedObjects.end();
				continue;
			}

			++iter;
		}
	}
	else
	{
		for (auto const pObject : m_constructedObjects)
		{
			if (IsActive(pObject))
			{
				pObject->GetImplDataPtr()->Update();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::RegisterObject(CATLAudioObject* const pObject)
{
	m_constructedObjects.push_back(pObject);
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ReportStartedEvent(CATLEvent* const pEvent)
{
	if (pEvent != nullptr)
	{
		CATLAudioObject* const pAudioObject = pEvent->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Event reported as started has no audio object!");
		pAudioObject->ReportStartedEvent(pEvent);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL pEvent in CObjectManager::ReportStartedEvent");
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ReportFinishedEvent(CATLEvent* const pEvent, bool const bSuccess)
{
	if (pEvent != nullptr)
	{
		CATLAudioObject* const pAudioObject = pEvent->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Event reported as finished has no audio object!");
		pAudioObject->ReportFinishedEvent(pEvent, bSuccess);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL pEvent in CObjectManager::ReportFinishedEvent");
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::GetStartedStandaloneFileRequestData(CATLStandaloneFile* const pStandaloneFile, CAudioRequest& request)
{
	if (pStandaloneFile != nullptr)
	{
		CATLAudioObject* const pAudioObject = pStandaloneFile->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Standalone file request without audio object!");
		pAudioObject->GetStartedStandaloneFileRequestData(pStandaloneFile, request);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL _pStandaloneFile in CObjectManager::GetStartedStandaloneFileRequestData");
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ReportFinishedStandaloneFile(CATLStandaloneFile* const pStandaloneFile)
{
	if (pStandaloneFile != nullptr)
	{
		CATLAudioObject* const pAudioObject = pStandaloneFile->m_pAudioObject;
		CRY_ASSERT_MESSAGE(pAudioObject, "Standalone file reported as finished has no audio object!");
		pAudioObject->ReportFinishedStandaloneFile(pStandaloneFile);
	}
	else
	{
		Cry::Audio::Log(ELogType::Warning, "NULL _pStandaloneFile in CObjectManager::ReportFinishedStandaloneFile");
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::ReleasePendingRays()
{
	for (auto const pObject : m_constructedObjects)
	{
		if (pObject != nullptr)
		{
			pObject->ReleasePendingRays();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::IsActive(CATLAudioObject const* const pAudioObject) const
{
	return HasActiveData(pAudioObject);
}

//////////////////////////////////////////////////////////////////////////
bool CObjectManager::HasActiveData(CATLAudioObject const* const pAudioObject) const
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
size_t CObjectManager::GetNumAudioObjects() const
{
	return m_constructedObjects.size();
}

//////////////////////////////////////////////////////////////////////////
size_t CObjectManager::GetNumActiveAudioObjects() const
{
	size_t numActiveObjects = 0;

	for (auto const pObject : m_constructedObjects)
	{
		if (IsActive(pObject))
		{
			++numActiveObjects;
		}
	}

	return numActiveObjects;
}

//////////////////////////////////////////////////////////////////////////
void CObjectManager::DrawPerObjectDebugInfo(
  IRenderAuxGeom& auxGeom,
  Vec3 const& listenerPos,
  AudioTriggerLookup const& triggers,
  AudioParameterLookup const& parameters,
  AudioSwitchLookup const& switches,
  AudioPreloadRequestLookup const& preloadRequests,
  AudioEnvironmentLookup const& environments) const
{
	for (auto const pObject : m_constructedObjects)
	{
		if (IsActive(pObject))
		{
			pObject->DrawDebugInfo(
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
void CObjectManager::DrawDebugInfo(IRenderAuxGeom& auxGeom, Vec3 const& listenerPosition, float const posX, float posY) const
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

	for (auto const pObject : m_constructedObjects)
	{
		Vec3 const& position = pObject->GetTransformation().GetPosition();
		float const distance = position.GetDistance(listenerPosition);

		if (g_cvars.m_debugDistance <= 0.0f || (g_cvars.m_debugDistance > 0.0f && distance < g_cvars.m_debugDistance))
		{
			char const* const szObjectName = pObject->m_name.c_str();
			CryFixedStringT<MaxControlNameLength> lowerCaseObjectName(szObjectName);
			lowerCaseObjectName.MakeLower();
			bool const hasActiveData = HasActiveData(pObject);
			bool const stringFound = (lowerCaseSearchString.empty() || (lowerCaseSearchString.compareNoCase("0") == 0)) || (lowerCaseObjectName.find(lowerCaseSearchString) != CryFixedStringT<MaxControlNameLength>::npos);
			bool const draw = stringFound && ((g_cvars.m_hideInactiveAudioObjects == 0) || ((g_cvars.m_hideInactiveAudioObjects > 0) && hasActiveData));

			if (draw)
			{
				bool const isVirtual = (pObject->GetFlags() & EObjectFlags::Virtual) != 0;

				auxGeom.Draw2dLabel(posX, posY, 1.25f,
				                    isVirtual ? itemVirtualColor : (hasActiveData ? itemActiveColor : itemInactiveColor),
				                    false,
				                    "%s : %.2f",
				                    szObjectName,
				                    pObject->GetMaxRadius());

				posY += 11.0f;
				++numAudioObjects;
			}
		}
	}

	auxGeom.Draw2dLabel(posX, headerPosY, 1.5f, headerColor, false, "Audio Objects [%" PRISIZE_T "]", numAudioObjects);
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio
