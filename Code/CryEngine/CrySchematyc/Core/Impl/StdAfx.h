#pragma once

#define eCryModule eCryM_Legacy

#include <CryCore/Project/CryModuleDefs.h>
#include <CryCore/Platform/platform.h>
#include <CryString/CryString.h>
#include <CrySystem/IConsole.h>
#include <CrySystem/ISystem.h>

#define CRY_STRING

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#ifdef GetObject
#undef GetObject
#endif
