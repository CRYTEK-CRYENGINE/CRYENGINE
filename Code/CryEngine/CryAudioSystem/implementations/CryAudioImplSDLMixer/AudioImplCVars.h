// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CCVars final
{
public:

	CCVars() = default;
	CCVars(CCVars const&) = delete;
	CCVars(CCVars&&) = delete;
	CCVars& operator=(CCVars const&) = delete;
	CCVars& operator=(CCVars&&) = delete;

	void    RegisterVariables();
	void    UnregisterVariables();
};

extern CCVars g_cvars;
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
