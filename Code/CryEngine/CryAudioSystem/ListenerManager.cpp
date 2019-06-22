// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ListenerManager.h"
#include "Common.h"
#include "Listener.h"
#include <IImpl.h>

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Common/Logger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CListenerManager::~CListenerManager()
{
	CRY_ASSERT_MESSAGE(m_constructedListeners.empty(), "There are still listeners during %s", __FUNCTION__);
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::Initialize()
{
	m_constructedListeners.push_back(&g_defaultListener);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	m_constructedListeners.push_back(&g_previewListener);
#endif  // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::Terminate()
{
	for (auto const pListener : m_constructedListeners)
	{
		CRY_ASSERT_MESSAGE(pListener->GetImplData() == nullptr, "A listener cannot have valid impl data during %s", __FUNCTION__);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
		ListenerId const id = pListener->GetId();

		if ((id != DefaultListenerId) && (id != g_previewListenerId))
#else
		if (pListener->GetId() != DefaultListenerId)
#endif  // CRY_AUDIO_USE_DEBUG_CODE
		{
			delete pListener;
		}
	}

	m_constructedListeners.clear();
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::ReleaseImplData()
{
	for (auto const pListener : m_constructedListeners)
	{
		g_pIImpl->DestructListener(pListener->GetImplData());
		pListener->SetImplData(nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::Update(float const deltaTime)
{
	if (deltaTime > 0.0f)
	{
		for (auto const pListener : m_constructedListeners)
		{
			pListener->Update(deltaTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CListener* CListenerManager::CreateListener(CTransformation const& transformation, char const* const szName, bool const isUserCreated)
{
	CryFixedStringT<MaxObjectNameLength> name = szName;
	GetUniqueListenerName(szName, name);

	MEMSTAT_CONTEXT(EMemStatContextType::AudioSystem, "CryAudio::CListener");

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	auto const pListener = new CListener(g_pIImpl->ConstructListener(transformation, name.c_str()), StringToId(name.c_str()), isUserCreated, name.c_str());
#else
	auto const pListener = new CListener(g_pIImpl->ConstructListener(transformation, name.c_str()), StringToId(name.c_str()), isUserCreated);
#endif // CRY_AUDIO_USE_DEBUG_CODE

	m_constructedListeners.push_back(pListener);
	return pListener;
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::ReleaseListener(CListener* const pListener)
{
	m_constructedListeners.erase(
		std::find_if(m_constructedListeners.begin(), m_constructedListeners.end(), [=](CListener const* pRegisteredListener)
		{
			if (pRegisteredListener == pListener)
			{
			  g_pIImpl->DestructListener(pListener->GetImplData());

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
			  CRY_ASSERT_MESSAGE((pListener->GetId() != DefaultListenerId) && (pListener->GetId() != g_previewListenerId),
			                     "Listener \"%s\" passed wrongly in %s", pListener->GetName(), __FUNCTION__);
#endif  // CRY_AUDIO_USE_DEBUG_CODE

			  delete pListener;

			  return true;
			}

			return false;
		})
		);
}

//////////////////////////////////////////////////////////////////////////
CListener* CListenerManager::GetListener(ListenerId const id) const
{
	CListener* pListenerToFind = &g_defaultListener;

	if (id != DefaultListenerId)
	{
		for (auto const pListener : m_constructedListeners)
		{
			if (pListener->GetId() == id)
			{
				pListenerToFind = pListener;
				break;
			}
		}
	}

	return pListenerToFind;
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::GetUniqueListenerName(char const* const szName, CryFixedStringT<MaxObjectNameLength>& newName)
{
	GenerateUniqueListenerName(newName);

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	ListenerId const id = StringToId(newName.c_str());

	if (StringToId(szName) != id)
	{
		Cry::Audio::Log(ELogType::Error, R"(A listener with the name "%s" already exists. Setting new name to "%s" (Id: %u))", szName, newName.c_str(), id);
	}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CListenerManager::GenerateUniqueListenerName(CryFixedStringT<MaxObjectNameLength>& name)
{
	bool nameChanged = false;
	ListenerId const id = StringToId(name.c_str());

	for (auto const pListenerSearched : m_constructedListeners)
	{
		if (pListenerSearched->GetId() == id)
		{
			name += "_1";
			nameChanged = true;
			break;
		}
	}

	if (nameChanged)
	{
		GenerateUniqueListenerName(name);
	}
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CListenerManager::ReconstructImplData()
{
	for (auto const pListener : m_constructedListeners)
	{
		ListenerId const id = pListener->GetId();

		if ((id != DefaultListenerId) && (id != g_previewListenerId))
		{
			if (pListener->GetImplData() != nullptr)
			{
				g_pIImpl->DestructListener(pListener->GetImplData());
			}

			pListener->SetImplData(g_pIImpl->ConstructListener(pListener->GetDebugTransformation(), pListener->GetName()));
		}
	}
}
#endif // CRY_AUDIO_USE_DEBUG_CODE
}      // namespace CryAudio
