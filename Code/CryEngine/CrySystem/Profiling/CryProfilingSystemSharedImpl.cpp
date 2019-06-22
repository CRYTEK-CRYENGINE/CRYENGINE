// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryProfilingSystemSharedImpl.h"
#include <CryCore/CryCrc32.h>

CCryProfilingSystemImpl::CCryProfilingSystemImpl()
{}

bool CCryProfilingSystemImpl::OnInputEvent(const SInputEvent& event)
{
	if (event.deviceType == eIDT_Keyboard && event.keyId == eKI_ScrollLock)
	{
		if (event.state == eIS_Pressed)
		{
			PauseRecording(!IsPaused());
		}
	}
	return false;
}

void CCryProfilingSystemImpl::OnSliceAndSleep()
{
	EndFrame();
	StartFrame();
}

const char* CCryProfilingSystemImpl::GetModuleName(EProfiledSubsystem subSystem) const
{
	switch (subSystem)
	{
	case PROFILE_RENDERER:
		return "Renderer";
	case PROFILE_3DENGINE:
		return "3DEngine";
	case PROFILE_PARTICLE:
		return "Particles";
	case PROFILE_AI:
		return "AI";
	case PROFILE_ANIMATION:
		return "Animation";
	case PROFILE_MOVIE:
		return "Movie";
	case PROFILE_ENTITY:
		return "Entities";
	case PROFILE_FONT:
		return "Font";
	case PROFILE_NETWORK:
		return "Network";
	case PROFILE_PHYSICS:
		return "Physics";
	case PROFILE_SCRIPT:
		return "Script";
	case PROFILE_SCRIPT_CFUNC:
		return "Script_CFunc";
	case PROFILE_AUDIO:
		return "Audio";
	case PROFILE_EDITOR:
		return "Editor";
	case PROFILE_SYSTEM:
		return "System";
	case PROFILE_ACTION:
		return "Action";
	case PROFILE_GAME:
		return "Game";
	case PROFILE_INPUT:
		return "Input";
	case PROFILE_LOADING_ONLY:
		return "Loading";
	default:
		assert(false);
		return "<Unknown Id>";
	}
}

const char* CCryProfilingSystemImpl::GetModuleName(const SProfilingSection* pSection) const
{
	return GetModuleName(pSection->pDescription->subsystem);
}

uint32 CCryProfilingSystemImpl::GenerateColorBasedOnName(const char* name)
{
	uint32 hash = CCrc32::Compute(name);
	hash |= 0xFF000000; // force alpha to opaque
	return hash;
}
