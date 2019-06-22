// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ParameterState.h"
#include "Common.h"
#include "Object.h"

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CParameterState::~CParameterState()
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->RemoveParameter(m_parameterInfo);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParameterState::Set(IObject* const pIObject)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->SetParameter(m_parameterInfo, m_value);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
	if (m_parameterInfo.IsGlobal())
	{
		Cry::Audio::Log(ELogType::Warning,
		                R"(FMOD - Global parameter "%s" was set to %f on object "%s". Consider setting it globally.)",
		                m_parameterInfo.GetName(),
		                m_value,
		                pObject->GetName());
	}
#endif  // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CParameterState::SetGlobally()
{
	if (m_parameterInfo.IsGlobal())
	{
		g_pStudioSystem->setParameterByID(m_parameterInfo.GetId(), m_value);
	}
	else
	{
		for (auto const pObject : g_constructedObjects)
		{
			pObject->SetParameter(m_parameterInfo, m_value);
		}

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
		Cry::Audio::Log(ELogType::Warning,
		                R"(FMOD - Local parameter "%s" was set globally to %f on all objects. Consider using a global parameter for better performance.)",
		                m_parameterInfo.GetName(),
		                m_value);
#endif    // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio
