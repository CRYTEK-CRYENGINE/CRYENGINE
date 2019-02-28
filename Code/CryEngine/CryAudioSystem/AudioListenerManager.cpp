// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AudioListenerManager.h"
#include "ATLEntities.h"
#include <ATLEntityData.h>
#include <IAudioImpl.h>
#include <algorithm>

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CAudioListenerManager::~CAudioListenerManager()
{
	if (!m_activeListeners.empty())
	{
		for (auto const pListener : m_activeListeners)
		{
			CRY_ASSERT(pListener->m_pImplData == nullptr);
			delete pListener;
		}

		m_activeListeners.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::Init(Impl::IImpl* const pIImpl)
{
	m_pIImpl = pIImpl;

	if (!m_activeListeners.empty())
	{
		for (auto const pListener : m_activeListeners)
		{
#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
			pListener->m_pImplData = m_pIImpl->ConstructListener(pListener->m_name.c_str());
#else
			pListener->m_pImplData = m_pIImpl->ConstructListener();
#endif  // INCLUDE_AUDIO_PRODUCTION_CODE

			pListener->HandleSetTransformation(pListener->Get3DAttributes().transformation);
		}
	}
	else
	{
		// Create a default listener for early functionality.
		CreateListener("DefaultListener");
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::Release()
{
	for (auto const pListener : m_activeListeners)
	{
		m_pIImpl->DestructListener(pListener->m_pImplData);
		pListener->m_pImplData = nullptr;
	}

	m_pIImpl = nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::Update(float const deltaTime)
{
	if (deltaTime > 0.0f)
	{
		for (auto const pListener : m_activeListeners)
		{
			pListener->Update(deltaTime);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CATLListener* CAudioListenerManager::CreateListener(char const* const szName /*= nullptr*/)
{
	if (!m_activeListeners.empty())
	{
		// Currently only one listener supported!
		CATLListener* const pListener = m_activeListeners.front();

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
		// Update name, TODO: needs reconstruction with the middleware in order to update it as well.
		pListener->m_name = szName;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

		return pListener;
	}

	CATLListener* const pListener = new CATLListener(m_pIImpl->ConstructListener(szName));

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
	pListener->m_name = szName;
#endif // INCLUDE_AUDIO_PRODUCTION_CODE

	m_activeListeners.push_back(pListener);
	return pListener;
}

//////////////////////////////////////////////////////////////////////////
void CAudioListenerManager::ReleaseListener(CATLListener* const pListener)
{
	// As we currently support only one listener we will destroy that instance only on engine shutdown!
	/*m_activeListeners.erase
	   (
	   std::find_if(m_activeListeners.begin(), m_activeListeners.end(), [=](CATLListener const* pRegisteredListener)
	   {
	   if (pRegisteredListener == pListener)
	   {
	    m_pIImpl->DestructListener(pListener->m_pImplData);
	    delete pListener;
	    return true;
	   }

	   return false;
	   })
	   );*/
}

//////////////////////////////////////////////////////////////////////////
size_t CAudioListenerManager::GetNumActiveListeners() const
{
	return m_activeListeners.size();
}

//////////////////////////////////////////////////////////////////////////
Impl::SObject3DAttributes const& CAudioListenerManager::GetActiveListenerAttributes() const
{
	for (auto const pListener : m_activeListeners)
	{
		// Only one listener supported currently!
		return pListener->Get3DAttributes();
	}

	return Impl::SObject3DAttributes::GetEmptyObject();
}

#if defined(INCLUDE_AUDIO_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
char const* CAudioListenerManager::GetActiveListenerName() const
{
	for (auto const pListener : m_activeListeners)
	{
		// Only one listener supported currently!
		return pListener->m_name.c_str();
	}

	return nullptr;
}
#endif // INCLUDE_AUDIO_PRODUCTION_CODE
}      // namespace CryAudio
