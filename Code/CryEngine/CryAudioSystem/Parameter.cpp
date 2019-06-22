// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Parameter.h"
#include "Common/IImpl.h"
#include "Common/IObject.h"
#include "Common/IParameterConnection.h"

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	#include "Object.h"
	#include "Common/Logger.h"
#endif // CRY_AUDIO_USE_DEBUG_CODE

namespace CryAudio
{
//////////////////////////////////////////////////////////////////////////
CParameter::~CParameter()
{
	CRY_ASSERT_MESSAGE(g_pIImpl != nullptr, "g_pIImpl mustn't be nullptr during %s", __FUNCTION__);

	for (auto const pConnection : m_connections)
	{
		g_pIImpl->DestructParameterConnection(pConnection);
	}
}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
//////////////////////////////////////////////////////////////////////////
void CParameter::Set(CObject const& object, float const value) const
{
	Impl::IObject* const pIObject = object.GetImplData();

	for (auto const pConnection : m_connections)
	{
		pConnection->Set(pIObject, value);
	}

	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Parameter "%s" set on object "%s" without connections)", GetName(), object.GetName());
	}

	const_cast<CObject&>(object).StoreParameterValue(m_id, value);
}
#else
//////////////////////////////////////////////////////////////////////////
void CParameter::Set(Impl::IObject* const pIObject, float const value) const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->Set(pIObject, value);
	}
}
#endif // CRY_AUDIO_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CParameter::Set(float const value) const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->Set(g_pIObject, value);
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Parameter "%s" set without connections)", GetName());
	}

	g_parameters[m_id] = value;
#endif // CRY_AUDIO_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CParameter::SetGlobally(float const value) const
{
	for (auto const pConnection : m_connections)
	{
		pConnection->SetGlobally(value);
	}

#if defined(CRY_AUDIO_USE_DEBUG_CODE)
	if (m_connections.empty())
	{
		Cry::Audio::Log(ELogType::Warning, R"(Parameter "%s" set globally without connections)", GetName());
	}

	g_parametersGlobally[m_id] = value;
#endif // CRY_AUDIO_USE_DEBUG_CODE
}
} // namespace CryAudio
