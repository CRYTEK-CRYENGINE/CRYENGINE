// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Environment.h"
#include "Object.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include "Common/IEnvironmentConnection.h"

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Common/Logger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CEnvironment::~CEnvironment()
{
	CRY_ASSERT_MESSAGE(g_pIImpl != nullptr, "g_pIImpl mustn't be nullptr during %s", __FUNCTION__);

	for (auto const pConnection : m_connections)
	{
		g_pIImpl->DestructEnvironmentConnection(pConnection);
	}
}

///////////////////////////////////////////////////////////////////////////
void CEnvironment::Set(CObject const& object, float const value) const
{
	Impl::IObject* const pIObject = object.GetImplData();

	for (auto const pConnection : m_connections)
	{
		pConnection->Set(pIObject, value);
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Environment "%s" set on object "%s" without connections)", GetName(), object.GetName());
	}

	const_cast<CObject&>(object).StoreEnvironmentValue(m_id, value);
#endif // CRY_AUDIO_USE_DEBUG_CODE
}
} // namespace CryAudio