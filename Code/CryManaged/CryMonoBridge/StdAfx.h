// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#if !defined(PCH_INCLUDE_GUARD_THINGIE)
#define PCH_INCLUDE_GUARD_THINGIE


//#define _CRTDBG_MAP_ALLOC

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <CryCore/Project/CryModuleDefs.h>
#define eCryModule eCryM_MonoBridge

// Insert your headers here
#include <CryCore/Platform/platform.h>

#include <memory>
#include <vector>
#include <map>
#include <queue>
#include <string>
#include <exception>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <CryCore/Project/ProjectDefines.h>
#include <CryMath/Cry_Math.h>
#include <CryMath/Cry_Camera.h>
#include <CrySystem/ISystem.h>
#include <CryNetwork/INetwork.h>
#include <CryInput/IInput.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryNetwork/NetHelpers.h>
#include <CrySystem/File/ICryPak.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/ILog.h>
#include <CryNetwork/IRemoteControl.h>
#include <CryNetwork/ISimpleHttpServer.h>
#include <CryGame/IGameFramework.h>
#include <CryFlowGraph/IFlowSystem.h>

#include <CryAudio/IAudioSystem.h>

namespace MonoInternals
{
#include <mono/jit/jit.h>
#include <mono/metadata/mono-gc.h>
#include <mono/metadata/assembly.h>
#include <mono/utils/mono-logger.h>
#include <mono/metadata/mono-debug.h>
#include <mono/metadata/debug-helpers.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/object.h>
#include <mono/metadata/class.h>
#include <mono/metadata/exception.h>
#include <mono/metadata/reflection.h>
}

#define HAVE_MONO_API

#pragma warning(disable: 4018)	// conditional expression is constant
#pragma warning(disable: 4018)	// conditional expression is constant
#pragma warning(disable: 4503)  // decorated name length exceeded, name was truncated

#endif // PCH_INCLUDE_GUARD_THINGIE